#include "SampledInstrument.h"
#include "Riddim.h"

namespace rf
{
void InstrumentSet::finalize()
{
    minDb = 1e9f; maxDb = -1e9f;
    for (auto& s : samples)
        if (s.quality != SQMute) { minDb = std::min (minDb, s.levelDb); maxDb = std::max (maxDb, s.levelDb); }
    if (minDb > maxDb) minDb = maxDb = 0.0f;
}

SampledInstrument::SampledInstrument (bool monophonic) : mono (monophonic)
{
    pending.reserve (64);
    for (auto& v : voices) v.chordNotes.reserve (8);
    rng.seed (777);
}

void SampledInstrument::prepare (float sampleRate)
{
    sr = sampleRate;
    ctx.sr = sampleRate;
    now = 0;
    fbL.assign (2048, 0.0f);
    fbR.assign (2048, 0.0f);
    pending.clear();
    heldNotes.fill (false);
    for (auto& v : voices) v.active = false;
}

int SampledInstrument::activeVoices() const
{
    int c = (int) pending.size();
    for (auto& v : voices) if (v.active) ++c;
    if (fallback != nullptr) c += fallback->activeVoices();
    return c;
}

void SampledInstrument::noteOn (int note, float velocity, int gateSamples, int delaySamples)
{
    if (set.load() == nullptr) { if (fallback) fallback->noteOn (note, velocity, gateSamples, delaySamples); return; }
    if (pending.size() < 64)
        pending.push_back ({ note, clampf (velocity, 0.0f, 1.0f), gateSamples, now + std::max (0, delaySamples) });
}

void SampledInstrument::releaseVoice (Voice& v, float seconds)
{
    if (! v.active || v.releasing) return;
    v.releasing = true;
    v.relK = t60Coef (seconds, sr);
}

void SampledInstrument::noteOff (int note)
{
    if (fallback) fallback->noteOff (note);
    if (note >= 0 && note < 128) heldNotes[(size_t) note] = false;
    // a note released before it started: drop it
    pending.erase (std::remove_if (pending.begin(), pending.end(), [&] (const Pending& p) { return p.note == note && p.gate < 0 && p.start > now; }), pending.end());
    const auto* s = set.load();
    for (auto& v : voices)
    {
        if (! v.active || v.gate >= 0 || ! v.held) continue;
        bool match = false;
        if (s != nullptr && s->chords)
        {
            bool anyHeld = false;
            for (int n : v.chordNotes) { if (n == note) match = true; else if (n >= 0 && n < 128 && heldNotes[(size_t) n]) anyHeld = true; }
            if (match && anyHeld) continue;  // other chord notes still held
        }
        else match = v.noteTag == note;
        if (! match) continue;
        v.held = false;
        if (pedal) { v.sustained = true; continue; }
        releaseVoice (v, s != nullptr && s->part == "bass" ? 0.05f : 0.09f);
    }
}

void SampledInstrument::setPedal (bool down)
{
    pedal = down;
    if (fallback) fallback->setPedal (down);
    if (! down)
        for (auto& v : voices)
            if (v.active && v.sustained) { v.sustained = false; releaseVoice (v, 0.09f); }
}

void SampledInstrument::allOff()
{
    pending.clear();
    heldNotes.fill (false);
    pedal = false;
    for (auto& v : voices) releaseVoice (v, 0.03f);
    if (fallback) fallback->allOff();
}

SampledInstrument::Voice* SampledInstrument::allocVoice()
{
    for (auto& v : voices) if (! v.active) return &v;
    Voice* best = &voices[0];
    for (auto& v : voices) if (v.age < best->age) best = &v;
    return best;
}

const InstSample* SampledInstrument::pickNote (int note, float vel, float& shift, float& gain)
{
    const auto* s = set.load();
    if (s == nullptr || s->samples.empty()) return nullptr;
    // nearest recorded pitch (slight preference for re-pitching upwards less)
    int bestMidi = -1; float bestCost = 1e9f;
    for (auto& smp : s->samples)
    {
        const float d = (float) (note - smp.midi);
        const float cost = std::fabs (d) + (d > 0 ? 0.25f : 0.0f);
        if (cost < bestCost) { bestCost = cost; bestMidi = smp.midi; }
    }
    // velocity -> target level, round robin among that pitch
    const float range = std::max (12.0f, s->maxDb - s->minDb + 6.0f);
    const float target = s->maxDb - (1.0f - vel) * range;
    std::vector<int> cand;
    for (int i = 0; i < (int) s->samples.size(); ++i) if (s->samples[(size_t) i].midi == bestMidi) cand.push_back (i);
    int choice = cand[(size_t) (rng.uni() * (float) cand.size()) % cand.size()];
    if (cand.size() > 1 && choice == lastIdx) choice = cand[(size_t) ((std::find (cand.begin(), cand.end(), choice) - cand.begin() + 1) % (long) cand.size())];
    lastIdx = choice;
    const auto& smp = s->samples[(size_t) choice];
    shift = (float) note - ((float) smp.midi + smp.cents * 0.01f);
    gain = dbToGain (clampf (target - smp.levelDb, -30.0f, 6.0f));
    return &smp;
}

static int wrapShift (int d) { d = ((d % 12) + 12) % 12; return d > 5 ? d - 12 : d; }

const InstSample* SampledInstrument::pickChord (int root, int quality, float vel, float& shift, float& gain)
{
    const auto* s = set.load();
    if (s == nullptr || s->samples.empty()) return nullptr;
    const float range = std::max (12.0f, s->maxDb - s->minDb + 6.0f);
    const float target = s->maxDb - (1.0f - vel) * range;

    // soft strokes on guitars: muted scratch
    if (vel < 0.3f)
    {
        std::vector<int> mutes;
        for (int i = 0; i < (int) s->samples.size(); ++i) if (s->samples[(size_t) i].quality == SQMute) mutes.push_back (i);
        if (! mutes.empty())
        {
            const auto& m = s->samples[(size_t) mutes[(size_t) (rng.uni() * (float) mutes.size()) % mutes.size()]];
            shift = 0; gain = dbToGain (clampf (target - m.levelDb + 6.0f, -30.0f, 6.0f));
            return &m;
        }
    }

    float bestCost = 1e9f; int bestRoot = -1, bestQ = -1; int bestShift = 0;
    auto consider = [&] (int wantRoot, int q, float penalty)
    {
        for (auto& smp : s->samples)
        {
            if (smp.quality == SQMute) continue;
            float qc;
            if (smp.quality == q) qc = 0.0f;
            else if (smp.quality == SQNeutral) qc = 0.7f;
            else continue;
            const int sh = wrapShift (wantRoot - smp.root);
            const float cost = (float) std::abs (sh) + qc + penalty;
            if (cost < bestCost) { bestCost = cost; bestRoot = smp.root; bestQ = smp.quality; bestShift = sh; }
        }
    };
    consider (root, quality, 0.0f);
    // relative fallback: minor -> relative major chop (gives m7), major -> chop of the minor a third up (gives maj7)
    if (quality == SQMin) consider ((root + 3) % 12, SQMaj, 1.5f);
    else consider ((root + 4) % 12, SQMin, 1.5f);
    if (bestRoot < 0) return nullptr;

    std::vector<int> cand;
    for (int i = 0; i < (int) s->samples.size(); ++i)
    {
        const auto& smp = s->samples[(size_t) i];
        if (smp.root == bestRoot && smp.quality == bestQ) cand.push_back (i);
    }
    // nearest levels first, then random among the closest few
    std::sort (cand.begin(), cand.end(), [&] (int a, int b) {
        return std::fabs (s->samples[(size_t) a].levelDb - target) < std::fabs (s->samples[(size_t) b].levelDb - target); });
    const int n = std::min<int> (3, (int) cand.size());
    int choice = cand[(size_t) ((int) (rng.uni() * (float) n) % n)];
    if (n > 1 && choice == lastIdx) choice = cand[(size_t) (((int) (std::find (cand.begin(), cand.begin() + n, choice) - cand.begin()) + 1) % n)];
    lastIdx = choice;
    const auto& smp = s->samples[(size_t) choice];
    shift = (float) bestShift;
    gain = dbToGain (clampf (target - smp.levelDb, -30.0f, 6.0f));
    return &smp;
}

void SampledInstrument::triggerNote (const Pending& p, long long when)
{
    float shift = 0, gain = 1;
    const InstSample* smp = pickNote (p.note, p.vel, shift, gain);
    if (smp == nullptr) return;
    if (mono)
        for (auto& v : voices) releaseVoice (v, 0.012f);   // one string at a time
    Voice* v = allocVoice();
    v->active = true; v->releasing = false; v->held = true; v->sustained = false;
    v->s = smp; v->pos = 0; v->baseShift = shift;
    v->rate = smp->sampleRate / (double) sr * std::pow (2.0, (double) shift / 12.0);
    v->gain = gain * (0.35f + 0.65f * p.vel) * dbToGain (set.load()->trimDb);
    v->env = 1.0f; v->start = when; v->gate = p.gate; v->noteTag = p.note;
    v->chordNotes.clear();
    v->age = ++counter;
}

void SampledInstrument::triggerChord (const std::vector<Pending>& grp, long long when)
{
    if (grp.empty()) return;
    if (grp.size() == 1)
    {
        // a single key on a chord instrument: the recordings are chords, so use the synth for single notes
        if (fallback) fallback->noteOn (grp[0].note, grp[0].vel, grp[0].gate, (int) (when - now));
        return;
    }
    std::array<bool, 128> h {};
    float vel = 0; int gate = 0;
    for (auto& p : grp) { if (p.note >= 0 && p.note < 128) h[(size_t) p.note] = true; vel = std::max (vel, p.vel); gate = (p.gate < 0 || gate < 0) ? -1 : std::max (gate, p.gate); }
    Chord c;
    if (! detectChord (h, c)) return;
    const int q = (c.quality == QMin || c.quality == QMin7 || c.quality == QDim) ? SQMin : SQMaj;
    float shift = 0, gain = 1;
    const InstSample* smp = pickChord (c.root, q, vel, shift, gain);
    if (smp == nullptr) return;
    // a new chop cuts the previous one (a guitarist re-strikes; an organist re-voices)
    for (auto& v : voices) releaseVoice (v, 0.015f);
    Voice* v = allocVoice();
    v->active = true; v->releasing = false; v->held = true; v->sustained = false;
    v->s = smp; v->pos = 0; v->baseShift = shift;
    v->rate = smp->sampleRate / (double) sr * std::pow (2.0, (double) shift / 12.0);
    v->gain = gain * (0.4f + 0.6f * vel) * dbToGain (set.load()->trimDb);
    v->env = 1.0f; v->start = when; v->gate = gate; v->noteTag = -1;
    v->chordNotes.clear();
    for (auto& p : grp) v->chordNotes.push_back (p.note);
    v->age = ++counter;
}

void SampledInstrument::startGroup (long long t)
{
    const auto* s = set.load();
    if (s == nullptr) { pending.clear(); return; }
    if (! s->chords)
    {
        for (size_t i = 0; i < pending.size();)
        {
            if (pending[i].start <= t)
            {
                if (pending[i].gate < 0 && pending[i].note >= 0 && pending[i].note < 128) heldNotes[(size_t) pending[i].note] = true;
                triggerNote (pending[i], t);
                pending.erase (pending.begin() + (long) i);
            }
            else ++i;
        }
        return;
    }
    // chord sets: everything starting within 25 ms is one chord (strums, slightly spread keys)
    const long long window = (long long) (0.025f * sr);
    std::vector<Pending> grp;
    for (size_t i = 0; i < pending.size();)
    {
        if (pending[i].start <= t + window)
        {
            grp.push_back (pending[i]);
            if (pending[i].gate < 0 && pending[i].note >= 0 && pending[i].note < 128) heldNotes[(size_t) pending[i].note] = true;
            pending.erase (pending.begin() + (long) i);
        }
        else ++i;
    }
    // keys played live a little late join the chord that just started
    if (grp.size() == 1)
        for (auto& v : voices)
            if (v.active && ! v.releasing && v.held && v.gate < 0 && t - v.start < (long long) (0.06f * sr) && ! v.chordNotes.empty())
            {
                for (int n : v.chordNotes) grp.push_back ({ n, grp[0].vel, -1, t });
                break;
            }
    triggerChord (grp, t);
}

void SampledInstrument::render (float* L, float* R, int n)
{
    const float bendRatio = std::pow (2.0f, ctx.bend / 12.0f);
    for (int i = 0; i < n; ++i)
    {
        const long long t = now + i;
        if (! pending.empty())
        {
            long long first = pending[0].start;
            for (auto& p : pending) first = std::min (first, p.start);
            if (first <= t) startGroup (t);
        }
        for (auto& v : voices)
        {
            if (! v.active) continue;
            if (v.gate >= 0 && ! v.releasing && t - v.start >= v.gate)
                releaseVoice (v, (set.load() != nullptr && set.load()->part == "bass") ? 0.05f : 0.06f);
            const InstSample& s = *v.s;
            const int ip = (int) v.pos;
            if (ip >= s.length - 2) { v.active = false; continue; }
            const float fr = (float) (v.pos - ip);
            if (v.releasing) { v.env *= v.relK; if (v.env < 0.0002f) { v.active = false; continue; } }
            const float g = v.gain * v.env;
            const int i0 = std::max (0, ip - 1), i2 = ip + 1, i3 = std::min (s.length - 1, ip + 2);
            float out[2] = { 0, 0 };
            const int nch = std::min<int> (2, (int) s.ch.size());
            for (int c = 0; c < nch; ++c)
            {
                const float* d = s.ch[(size_t) c].data();
                const float xm1 = d[i0], x0 = d[ip], x1 = d[i2], x2 = d[i3];
                const float c1 = 0.5f * (x1 - xm1);
                const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
                const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
                out[c] = (((c3 * fr + c2) * fr + c1) * fr + x0) * g;
            }
            if (nch == 1) out[1] = out[0];
            L[i] += out[0];
            R[i] += out[1];
            v.pos += v.rate * (set.load() != nullptr && ! set.load()->chords ? bendRatio : 1.0f);
        }
    }
    now += n;
    if (fallback != nullptr && fallback->activeVoices() > 0)
    {
        for (int done = 0; done < n;)
        {
            const int m = std::min (n - done, (int) fbL.size());
            std::fill (fbL.begin(), fbL.begin() + m, 0.0f);
            std::fill (fbR.begin(), fbR.begin() + m, 0.0f);
            fallback->render (fbL.data(), fbR.data(), m);
            for (int i = 0; i < m; ++i) { L[done + i] += fbL[(size_t) i]; R[done + i] += fbR[(size_t) i]; }
            done += m;
        }
    }
}

} // namespace rf
