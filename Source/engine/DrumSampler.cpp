#include "DrumSampler.h"

namespace rf
{
const char* micBusName (int b)
{
    static const char* n[kNumMicBuses] = { "Kick In", "Kick Out", "Snare Top", "Snare Bottom", "Hi-Hat", "Tom Hi", "Tom Mid",
                                           "Tom Floor", "Rototoms", "Timbales", "Percussion", "Cymbals", "Overheads", "Room" };
    return b >= 0 && b < kNumMicBuses ? n[b] : "";
}

namespace
{
struct SlotDef { const char* key; const char* name; std::vector<int> notes; int bus; };
const std::vector<SlotDef>& slots()
{
    static const std::vector<SlotDef> s = {
        { "kick", "Kick", { 36, 35 }, BusKickIn },
        { "snare", "Snare", { 38 }, BusSnareTop },
        { "snare_fill", "Snare Fill", { 40 }, BusSnareTop },
        { "snare_ghost", "Snare Ghost", {}, BusSnareTop },
        { "cross", "Cross Stick", { 37 }, BusSnareTop },
        { "hat_closed", "Hi-Hat Closed", { 42, 44 }, BusHiHat },
        { "hat_open", "Hi-Hat Open", { 46 }, BusHiHat },
        { "tom_hi", "Tom Hi", { 48, 50 }, BusTomHi },
        { "tom_mid", "Tom Mid", { 45, 47 }, BusTomMid },
        { "tom_floor", "Tom Floor", { 41, 43 }, BusTomFloor },
        { "roto_hi", "Rototom Hi", { 93 }, BusRotos },
        { "roto_lo", "Rototom Lo", { 94 }, BusRotos },
        { "timbale_hi", "Timbale Hi", { 65, 79 }, BusTimbales },
        { "timbale_lo", "Timbale Lo", { 66 }, BusTimbales },
        { "tamb", "Tambourine", { 54 }, BusPerc },
        { "shaker", "Shaker", { 82, 70 }, BusPerc },
        { "block", "Jam Block", { 76, 77, 75 }, BusPerc },
        { "crash", "Crash", { 49, 57 }, BusCymbals },
        { "crash_choke", "Crash Choke", { 55 }, BusCymbals },
    };
    return s;
}
struct NoteTweak { float tune = 0, gainDb = 0, decay = 1; };
const NoteTweak& noteTweak (int note)
{
    static NoteTweak t[128];
    static bool init = false;
    if (! init)
    {
        t[35] = { -3.0f, 0.0f, 1.0f };   // kick boom: pitched down
        t[44] = { 0.0f, -6.0f, 0.6f };   // pedal hat: softer, shorter
        t[43] = { 2.0f, 0.0f, 1.0f };
        t[45] = { -1.5f, 0.0f, 1.0f };
        t[50] = { 2.0f, 0.0f, 1.0f };
        t[57] = { 1.5f, 0.0f, 1.0f };
        t[79] = { 1.0f, 0.0f, 1.0f };
        t[70] = { 2.0f, -3.0f, 1.0f };
        t[77] = { -2.0f, 0.0f, 1.0f };
        t[75] = { 3.0f, 0.0f, 1.0f };
        init = true;
    }
    return t[std::clamp (note, 0, 127)];
}
} // namespace

int numDrumSlots() { return (int) slots().size(); }
const char* drumSlotKey (int s) { return s >= 0 && s < numDrumSlots() ? slots()[(size_t) s].key : ""; }
const char* drumSlotName (int s) { return s >= 0 && s < numDrumSlots() ? slots()[(size_t) s].name : ""; }
int drumSlotDefaultBus (int s) { return s >= 0 && s < numDrumSlots() ? slots()[(size_t) s].bus : BusPerc; }
const std::vector<int>& drumSlotNotes (int s) { static std::vector<int> none; return s >= 0 && s < numDrumSlots() ? slots()[(size_t) s].notes : none; }
int drumSlotIndex (const std::string& key)
{
    for (int i = 0; i < numDrumSlots(); ++i) if (key == slots()[(size_t) i].key) return i;
    return -1;
}
int drumSlotForNote (int note)
{
    for (int i = 0; i < numDrumSlots(); ++i)
        for (int n : slots()[(size_t) i].notes) if (n == note) return i;
    return -1;
}

void Kit::buildIndex()
{
    for (auto& v : noteToArtic) v = -1;
    for (size_t i = 0; i < artics.size(); ++i)
    {
        auto& a = artics[i];
        for (int n : a.notes) if (n >= 0 && n < 128) noteToArtic[n] = (int) i;
        a.minDb = 1e9f; a.maxDb = -1e9f;
        for (auto& s : a.samples) { a.minDb = std::min (a.minDb, s.levelDb); a.maxDb = std::max (a.maxDb, s.levelDb); }
        if (a.samples.empty()) a.minDb = a.maxDb = 0;
    }
}

int Kit::findArtic (const std::string& key) const
{
    for (size_t i = 0; i < artics.size(); ++i) if (artics[i].key == key) return (int) i;
    return -1;
}

//==============================================================================
DrumSampler::DrumSampler()
{
    for (auto& u : user) u.store (nullptr);
    for (auto& m : busMeter) m.store (0.0f);
    mic[BusRoom].gain = dbToGain (-4.0f);
    rng.seed (12345);
}

void DrumSampler::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    for (int b = 0; b < kNumMicBuses; ++b)
    {
        busL[b].assign ((size_t) maxBlock, 0.0f);
        busR[b].assign ((size_t) maxBlock, 0.0f);
        busGain[b].reset (mic[b].mute ? 0.0f : mic[b].gain);
    }
    allOff();
    for (auto& v : voices) v.active = false;
}

void DrumSampler::allOff()
{
    for (auto& v : voices)
        if (v.active) { v.choking = true; v.chokeK = (float) t60Coef (0.02f, (float) sr); }
}

DrumSampler::Voice* DrumSampler::allocVoice()
{
    for (auto& v : voices) if (! v.active) return &v;
    Voice* oldest = &voices[0];
    for (auto& v : voices) if (v.age < oldest->age) oldest = &v;
    return oldest;
}

const KitSample* DrumSampler::pick (const KitArtic& a, float vel, int articIndex, float& gainOut)
{
    if (a.samples.empty()) return nullptr;
    const float range = std::max (14.0f, a.maxDb - a.minDb + 8.0f);
    const float target = a.maxDb - (1.0f - clampf (vel, 0.0f, 1.0f)) * range;

    // the three samples nearest to the target level, avoid repeating the last one
    int best[3] = { -1, -1, -1 };
    float bestD[3] = { 1e9f, 1e9f, 1e9f };
    for (int i = 0; i < (int) a.samples.size(); ++i)
    {
        float d = std::fabs (a.samples[(size_t) i].levelDb - target);
        for (int k = 0; k < 3; ++k)
            if (d < bestD[k])
            {
                for (int m = 2; m > k; --m) { best[m] = best[m - 1]; bestD[m] = bestD[m - 1]; }
                best[k] = i; bestD[k] = d; break;
            }
    }
    int n = 0;
    for (int k = 0; k < 3; ++k) if (best[k] >= 0 && bestD[k] < bestD[0] + 4.0f) ++n;
    int choice = best[0];
    const int last = lastPick[articIndex & 63];
    if (n > 1)
    {
        for (int tries = 0; tries < 4; ++tries)
        {
            choice = best[(int) (rng.uni() * (float) n) % n];
            if (choice != last) break;
        }
    }
    lastPick[articIndex & 63] = choice;
    const auto& s = a.samples[(size_t) choice];
    gainOut = dbToGain (clampf (target - s.levelDb, -36.0f, 6.0f));
    return &s;
}

bool DrumSampler::trigger (int note, float vel, int delaySamples)
{
    if (note < 0 || note > 127) return false;
    const Kit* k = kit.load();
    const int noteSlot = drumSlotForNote (note);
    const KitSample* us = noteSlot >= 0 ? user[noteSlot].load() : nullptr;

    const KitSample* s = nullptr;
    const KitArtic* art = nullptr;
    int artIdx = -1, paramSlot = noteSlot, choke = 0;
    float gain = 1.0f;

    if (us != nullptr)
    {
        s = us;
        gain = std::pow (clampf (vel, 0.0f, 1.0f), 1.6f) * 1.3f;
        if (k) { int ai = k->noteToArtic[note]; if (ai >= 0) choke = k->artics[(size_t) ai].choke; }
        if (noteSlot == drumSlotIndex ("hat_closed") || noteSlot == drumSlotIndex ("hat_open")) choke = 1;
    }
    else
    {
        if (k == nullptr) return false;
        artIdx = k->noteToArtic[note];
        if (artIdx < 0) return false;
        art = &k->artics[(size_t) artIdx];
        paramSlot = drumSlotIndex (art->key);
        choke = art->choke;
        // soft snare hits use the ghost-stroke samples when the kit has them
        if (art->key == "snare" && vel < 0.32f)
        {
            int g = k->findArtic ("snare_ghost");
            if (g >= 0 && ! k->artics[(size_t) g].samples.empty())
            {
                art = &k->artics[(size_t) g]; artIdx = g;
                vel = std::min (1.0f, vel / 0.32f);
            }
        }
        s = pick (*art, vel, artIdx, gain);
        if (s == nullptr) return false;
    }

    const NoteTweak& nt = noteTweak (note);
    const SlotParams sp = paramSlot >= 0 ? slot[paramSlot] : SlotParams();

    if (choke != 0)
        for (auto& v : voices)
            if (v.active && v.choke == choke && ! v.choking) { v.choking = true; v.chokeK = t60Coef (0.035f, (float) sr); }

    Voice* v = allocVoice();
    v->active = true; v->choking = false; v->chokeGain = 1.0f;
    v->s = s; v->pos = 0; v->delay = std::max (0, delaySamples);
    v->rate = (s->sampleRate / sr) * std::pow (2.0, (double) (sp.tune + nt.tune) / 12.0);
    v->gain = gain * dbToGain (sp.gainDb + nt.gainDb);
    v->choke = choke; v->note = note; v->age = ++counter;

    const float dec = clampf (sp.decay * nt.decay, 0.05f, 1.0f);
    v->fadeStart = dec >= 0.999f ? 1e12 : (double) s->length * dec * 0.6;
    v->fadeLen = std::max (64.0, (double) s->length * dec * 0.4);

    v->nch = std::min ((int) s->ch.size(), 8);
    for (int c = 0; c < v->nch; ++c)
    {
        if (art != nullptr && c < (int) art->routeBus.size())
        {
            v->bus[c] = art->routeBus[(size_t) c];
            v->mode[c] = art->routeMode[(size_t) c];
        }
        else
        {
            v->bus[c] = drumSlotDefaultBus (noteSlot);
            v->mode[c] = v->nch == 1 ? RouteMono : (c == 0 ? RouteL : RouteR);
        }
    }
    return true;
}

void DrumSampler::clearBuses (int n)
{
    for (int b = 0; b < kNumMicBuses; ++b)
    {
        std::fill (busL[b].begin(), busL[b].begin() + n, 0.0f);
        std::fill (busR[b].begin(), busR[b].begin() + n, 0.0f);
    }
}

void DrumSampler::renderVoices (int start, int n)
{
    for (auto& v : voices)
    {
        if (! v.active) continue;
        const KitSample& s = *v.s;
        for (int i = start; i < start + n; ++i)
        {
            if (v.delay > 0) { --v.delay; continue; }
            const int ip = (int) v.pos;
            if (ip >= s.length - 1) { v.active = false; break; }
            const float fr = (float) (v.pos - ip);
            float g = v.gain;
            if (v.pos > v.fadeStart)
            {
                const double f = 1.0 - (v.pos - v.fadeStart) / v.fadeLen;
                if (f <= 0.0) { v.active = false; break; }
                g *= (float) (f * f);
            }
            if (v.choking)
            {
                v.chokeGain *= v.chokeK;
                if (v.chokeGain < 0.0005f) { v.active = false; break; }
                g *= v.chokeGain;
            }
            const int i0 = std::max (0, ip - 1), i2 = std::min (s.length - 1, ip + 1), i3 = std::min (s.length - 1, ip + 2);
            for (int c = 0; c < v.nch; ++c)
            {
                const float* d = s.ch[(size_t) c].data();
                // 4-point Hermite interpolation
                const float xm1 = d[i0], x0 = d[ip], x1 = d[i2], x2 = d[i3];
                const float c1 = 0.5f * (x1 - xm1);
                const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
                const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
                const float y = (((c3 * fr + c2) * fr + c1) * fr + x0) * g;
                const int b = v.bus[c];
                switch (v.mode[c])
                {
                    case RouteL: busL[b][(size_t) i] += y; break;
                    case RouteR: busR[b][(size_t) i] += y; break;
                    default:     busL[b][(size_t) i] += y; busR[b][(size_t) i] += y; break;
                }
            }
            v.pos += v.rate;
        }
    }
}

void DrumSampler::mixBuses (float* outL, float* outR, int n)
{
    bool anySolo = false;
    for (auto& m : mic) anySolo |= m.solo;
    const Kit* k = kit.load();
    const float kitGain = k != nullptr ? dbToGain (k->outputGainDb) : 1.0f;
    for (int b = 0; b < kNumMicBuses; ++b)
    {
        const auto& m = mic[b];
        const bool audible = ! m.mute && (! anySolo || m.solo);
        busGain[b].setTarget (audible ? m.gain * kitGain : 0.0f, n);
        // mono sources are panned, stereo sources are balanced
        float pl, pr;
        panGains (m.pan, pl, pr);
        const float balL = m.pan > 0 ? 1.0f - m.pan : 1.0f;
        const float balR = m.pan < 0 ? 1.0f + m.pan : 1.0f;
        const bool isStereoBus = b == BusOverheads || b == BusRoom || b == BusCymbals;
        float peak = 0;
        for (int i = 0; i < n; ++i)
        {
            const float g = busGain[b].next();
            float l = busL[b][(size_t) i], r = busR[b][(size_t) i];
            if (isStereoBus) { l *= balL; r *= balR; }
            else             { l *= pl;   r *= pr; }   // close mics: constant-power pan
            l *= g; r *= g;
            outL[i] += l; outR[i] += r;
            peak = std::max (peak, std::max (std::fabs (l), std::fabs (r)));
        }
        busMeter[b].store (std::max (peak, busMeter[b].load() * 0.85f));
    }
}

} // namespace rf
