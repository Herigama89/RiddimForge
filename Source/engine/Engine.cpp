#include "Engine.h"

namespace rf
{
namespace
{
struct Div { const char* name; float beats; };
const Div kDivs[] = {
    { "1/16", 0.25f }, { "1/8 T", 1.0f / 3.0f }, { "1/8", 0.5f }, { "1/8 D", 0.75f },
    { "1/4 T", 2.0f / 3.0f }, { "1/4", 1.0f }, { "1/4 D", 1.5f }, { "1/2", 2.0f },
};
} // namespace

int numDelayDivisions() { return (int) (sizeof (kDivs) / sizeof (kDivs[0])); }
const char* delayDivisionName (int i) { return kDivs[std::clamp (i, 0, numDelayDivisions() - 1)].name; }
float delayDivisionBeats (int i) { return kDivs[std::clamp (i, 0, numDelayDivisions() - 1)].beats; }

//==============================================================================
Engine::Engine()
{
    skank = makeSkankGuitar();
    piano = makePiano();
    organ = makeOrgan();
    bass  = makeBass();
    lead  = makeLead();
    pad   = makePad();
    for (auto& m : uiMeter) m.store (0.0f);
    for (auto& k : kits) k.store (nullptr);
    for (auto& ps : partSets) for (auto& p : ps) p.store (nullptr);
    for (int p = 0; p < kNumParts; ++p)
    {
        if (auto* syn = synthForPart (p))
        {
            sampled[p] = std::make_unique<SampledInstrument> (p == PartBass || p == PartLead);
            sampled[p]->setFallback (syn);
        }
        active[p] = synthForPart (p);
    }

    // sensible default mix
    auto set = [this] (int p, float lvl, float pan, float dly, float rev)
    { params.part[p].level = lvl; params.part[p].pan = pan; params.part[p].delay = dly; params.part[p].reverb = rev; };
    set (PartDrums, 0.85f, 0.0f, 0.0f, 0.10f);
    set (PartPerc,  0.6f,  0.0f, 0.12f, 0.18f);
    set (PartSkank, 0.6f, 0.25f, 0.18f, 0.25f);
    set (PartPiano, 0.38f, -0.25f, 0.08f, 0.2f);
    set (PartOrgan, 0.36f, -0.1f, 0.0f, 0.15f);
    set (PartBass,  0.58f, 0.0f, 0.0f, 0.0f);
    set (PartLead,  0.6f,  0.1f, 0.3f, 0.3f);
    set (PartPad,   0.45f, 0.0f, 0.0f, 0.35f);
}

void Engine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = std::max (32, maxBlockSize);
    const float fs = (float) sr;
    drums.prepare (fs);
    sampler.prepare (sampleRate, maxBlock);
    for (auto* i : { skank.get(), piano.get(), organ.get(), bass.get(), lead.get(), pad.get() }) i->prepare (fs);
    for (auto& s : sampled) if (s) s->prepare (fs);
    for (int p = 0; p < kNumParts; ++p)
    {
        partL[p].assign ((size_t) maxBlock, 0.0f);
        partR[p].assign ((size_t) maxBlock, 0.0f);
        gainSm[p].reset (params.part[p].mute ? 0.0f : params.part[p].level);
    }
    for (auto* b : { &delL, &delR, &revL, &revR }) b->assign ((size_t) maxBlock, 0.0f);
    masterSm.reset (dbToGain (params.masterDb));
    delay.prepare (fs);
    reverb.prepare (fs);
    dubFilter.prepare (fs);
    reset();
}

void Engine::reset()
{
    drums.allOff();
    sampler.allOff();
    for (auto* i : { skank.get(), piano.get(), organ.get(), bass.get(), lead.get(), pad.get() }) i->allOff();
    for (auto& s : sampled) if (s) s->allOff();
    chordHeld.fill (false);
    haveMidiChord = false;
    freeRunning = false;
    seqWasRunning = false;
    delay.clear();
}

InstrumentBase* Engine::synthForPart (int part)
{
    switch (part)
    {
        case PartSkank: return skank.get();
        case PartPiano: return piano.get();
        case PartOrgan: return organ.get();
        case PartBass:  return bass.get();
        case PartLead:  return lead.get();
        case PartPad:   return pad.get();
        default:        return nullptr;
    }
}

int Engine::synthTypeCount (int part)
{
    switch (part)
    {
        case PartSkank: return 1;
        case PartPiano: return 3;
        case PartOrgan: return 4;
        case PartBass:  return 4;
        case PartLead:  return 5;
        default:        return 1000;
    }
}

InstrumentBase* Engine::instrumentForTarget (int target)
{
    switch (target)
    {
        case TargetSkank: return active[PartSkank];
        case TargetPiano: return active[PartPiano];
        case TargetOrgan: return active[PartOrgan];
        case TargetBass:  return active[PartBass];
        case TargetLead:  return active[PartLead];
        case TargetPad:   return active[PartPad];
        default:          return nullptr;
    }
}

InstrumentBase* Engine::instrumentForPart (int part)
{
    return part >= 0 && part < kNumParts ? active[part] : nullptr;
}

void Engine::triggerDrum (int note, float vel, int delaySamples)
{
    if (params.drumKit > 0 && sampler.trigger (note, vel, delaySamples)) return;
    drums.trigger (note, vel, delaySamples, params.drums);
}

void Engine::applyParams()
{
    const int k = params.drumKit;
    sampler.setKit (k > 0 && k <= kMaxKits ? kits[(size_t) (k - 1)].load() : nullptr);
    for (int b = 0; b < kNumMicBuses; ++b) sampler.mic[b] = params.mic[(size_t) b];
    for (int s = 0; s < kMaxDrumSlots; ++s) sampler.slot[s] = params.slot[(size_t) s];
    auto synthPreset = [] (int type, int part) { return type < synthTypeCount (part) ? type : 0; };
    skank->ctx.tone = params.skankTone; skank->ctx.extra = params.skankMute;
    piano->ctx.preset = synthPreset (params.pianoType, PartPiano); piano->ctx.tone = params.pianoTone;
    organ->ctx.preset = synthPreset (params.organType, PartOrgan); organ->ctx.tone = params.organTone; organ->ctx.extra = (float) params.organRotary * 0.5f;
    bass->ctx.preset = synthPreset (params.bassType, PartBass);    bass->ctx.tone = params.bassTone;
    lead->ctx.preset = synthPreset (params.leadType, PartLead);    lead->ctx.tone = params.leadTone;
    pad->ctx.preset = params.padType;     pad->ctx.tone = params.padTone;

    // choose synth or recorded instrument per part
    const int typeFor[kNumParts] = { 0, 0, params.skankType, params.pianoType, params.organType, params.bassType, params.leadType, 0 };
    for (int p = PartSkank; p < PartPad; ++p)
    {
        InstrumentBase* want = synthForPart (p);
        const int idx = typeFor[p] - synthTypeCount (p);
        if (idx >= 0 && idx < kMaxSetsPerPart && sampled[p] != nullptr)
            if (const auto* set = partSets[(size_t) p][(size_t) idx].load())
            {
                sampled[p]->setSet (set);
                sampled[p]->ctx.bend = active[p] != nullptr ? active[p]->ctx.bend : 0.0f;
                want = sampled[p].get();
            }
        if (want != active[p])
        {
            if (active[p] != nullptr) active[p]->allOff();
            active[p] = want;
        }
    }
}

//==============================================================================
// Channel map: 1 = selectable target, 2 skank, 3 piano, 4 organ, 5 bass,
// 6 lead, 7 pad, 10 drums & percussion, 16 = chord input for the riddim
static int targetForChannel (int ch0, int ch1Target)
{
    switch (ch0)
    {
        case 1: return TargetSkank;
        case 2: return TargetPiano;
        case 3: return TargetOrgan;
        case 4: return TargetBass;
        case 5: return TargetLead;
        case 6: return TargetPad;
        case 9: return TargetDrums;
        default: return ch1Target;
    }
}

void Engine::playNote (int target, int note, float vel)
{
    if (target == TargetDrums) { triggerDrum (note, vel, 0); return; }
    if (auto* i = instrumentForTarget (target)) i->noteOn (note, vel, -1, 0);
}

void Engine::stopNote (int target, int note)
{
    if (auto* i = instrumentForTarget (target)) i->noteOff (note);
}

void Engine::handleMidi (const MidiEvent& e)
{
    const int type = e.status & 0xF0;
    const int ch = e.status & 0x0F;

    if (ch == 15) // chord input channel
    {
        if (type == 0x90 && e.d2 > 0) chordHeld[e.d1 & 127] = true;
        else if (type == 0x80 || (type == 0x90 && e.d2 == 0)) chordHeld[e.d1 & 127] = false;
        Chord c;
        if (detectChord (chordHeld, c)) { midiChord = c; haveMidiChord = true; }
        return;
    }

    const int target = targetForChannel (ch, params.ch1Target);
    auto* inst = instrumentForTarget (target);

    if (type == 0x90 && e.d2 > 0)                    playNote (target, e.d1, (float) e.d2 / 127.0f);
    else if (type == 0x80 || (type == 0x90 && e.d2 == 0)) stopNote (target, e.d1);
    else if (type == 0xB0)
    {
        if (e.d1 == 64 && inst) inst->setPedal (e.d2 >= 64);
        else if (e.d1 == 1 && inst) inst->ctx.mod = (float) e.d2 / 127.0f;
        else if (e.d1 == 120 || e.d1 == 123)
        {
            if (inst) inst->allOff();
            if (target == TargetDrums) { drums.allOff(); sampler.allOff(); }
        }
    }
    else if (type == 0xE0 && inst)
    {
        const int v = ((int) e.d2 << 7) | (int) e.d1;
        inst->ctx.bend = (float) (v - 8192) / 8192.0f * 2.0f;
    }
}

//==============================================================================
void Engine::runSequencer (int n, const Transport& t)
{
    bool run = false;
    double ppq0 = 0.0, bpm = t.bpm > 0 ? t.bpm : 120.0;

    // Host Sync: follows the DAW's transport, or the PLAY button when the DAW is stopped.
    // Free Run : only the PLAY button, at the plugin's own tempo.
    const bool hostRunning = params.runMode == 0 && t.hostPlaying && t.hasPpq;
    if (params.riddimOn && hostRunning)
    {
        freeRunning = false;
        run = true; ppq0 = t.ppq;
    }
    else if (params.riddimOn && params.preview)
    {
        if (! freeRunning) { freePpq = 0.0; freeRunning = true; }
        run = true; ppq0 = freePpq;
        bpm = params.runMode == 1 || t.bpm <= 0 ? clampf (params.freeBpm, 40.0f, 220.0f) : t.bpm;
    }
    else freeRunning = false;

    lastBpm = bpm;
    uiRunning.store (run);

    if (! run)
    {
        if (seqWasRunning)
            for (int p = PartSkank; p < kNumParts; ++p)
                if (auto* i = instrumentForPart (p)) i->allOff();
        seqWasRunning = false;
        uiStep.store (-1);
        uiAbsStep.store (-1);
        return;
    }
    seqWasRunning = true;

    const double spq = sr * 60.0 / bpm;              // samples per quarter note
    const double ppq1 = ppq0 + (double) n / spq;
    if (freeRunning) freePpq = ppq1;

    auto& rs = params.riddim;
    const int64_t kStart = (int64_t) std::ceil (ppq0 * 4.0 - 1e-7);
    const int64_t kEnd   = (int64_t) std::ceil (ppq1 * 4.0 - 1e-7);

    for (int64_t k = std::max<int64_t> (0, kStart); k < kEnd; ++k)
    {
        const int64_t bar = k / 16;
        const bool midiCh = rs.chordFromMidi && haveMidiChord;
        Chord chord = midiCh ? midiChord : progressionChord (rs, bar);
        Chord next = midiCh ? midiChord : progressionChord (rs, bar + 1);
        uiChordRoot.store (chord.root);
        uiChordQuality.store (chord.quality);
        uiStep.store ((int) (k % 16));
        uiAbsStep.store ((long long) k);

        seqEvents.clear();
        generateStep (k, rs, chord, next, seqEvents);
        const double nominal = (double) k * 0.25;
        for (const auto& e : seqEvents)
        {
            const double tp = nominal + (double) e.offsetSteps * 0.25;
            const int dly = std::max (0, (int) std::lround ((tp - ppq0) * spq + (double) e.offsetSec * sr));
            if (e.part == PartDrums || e.part == PartPerc)
                triggerDrum (e.note, e.vel, dly);
            else if (auto* i = instrumentForPart (e.part))
                i->noteOn (e.note, e.vel, std::max (1, (int) (e.gateSteps * 0.25 * spq)), dly);
        }
    }
}

void Engine::renderSegment (int start, int len)
{
    if (len <= 0) return;
    sampler.renderVoices (start, len);
    drums.render (partL[PartDrums].data() + start, partR[PartDrums].data() + start,
                  partL[PartPerc].data() + start, partR[PartPerc].data() + start, len);
    for (int p = PartSkank; p < kNumParts; ++p)
        if (auto* i = instrumentForPart (p))
            if (i->activeVoices() > 0)
                i->render (partL[p].data() + start, partR[p].data() + start, len);
}

void Engine::mix (float* outL, float* outR, int n, double bpm)
{
    std::fill (delL.begin(), delL.begin() + n, 0.0f);
    std::fill (delR.begin(), delR.begin() + n, 0.0f);
    std::fill (revL.begin(), revL.begin() + n, 0.0f);
    std::fill (revR.begin(), revR.begin() + n, 0.0f);
    std::fill (outL, outL + n, 0.0f);
    std::fill (outR, outR + n, 0.0f);

    bool anySolo = false;
    for (const auto& pp : params.part) anySolo |= pp.solo;
    for (int p = 0; p < kNumParts; ++p)
    {
        const auto& pp = params.part[p];
        const bool audible = ! pp.mute && (! anySolo || pp.solo);
        gainSm[p].setTarget (audible ? pp.level : 0.0f, n);
        float gl, gr;
        panGains (pp.pan, gl, gr);
        const float* l = partL[p].data();
        const float* r = partR[p].data();
        float peak = 0;
        for (int i = 0; i < n; ++i)
        {
            const float g = gainSm[p].next();
            const float a = l[i] * g * gl, b = r[i] * g * gr;
            outL[i] += a; outR[i] += b;
            delL[(size_t) i] += a * pp.delay; delR[(size_t) i] += b * pp.delay;
            revL[(size_t) i] += a * pp.reverb; revR[(size_t) i] += b * pp.reverb;
            peak = std::max (peak, std::max (std::fabs (a), std::fabs (b)));
        }
        float old = uiMeter[p].load();
        uiMeter[p].store (std::max (peak, old * 0.85f));
    }

    delay.setTimeSeconds (delayDivisionBeats (params.delayDiv) * (float) (60.0 / bpm));
    delay.setFeedback (params.delayFeedback);
    delay.setTone (params.delayTone);
    reverb.set (params.reverbSize, params.reverbDamp);
    masterSm.setTarget (dbToGain (params.masterDb), n);

    for (int i = 0; i < n; ++i)
    {
        float dl, dr, rl, rr;
        delay.process (delL[(size_t) i], delR[(size_t) i], dl, dr);
        reverb.process (revL[(size_t) i] + dl * 0.25f, revR[(size_t) i] + dr * 0.25f, rl, rr);
        outL[i] += dl + rl;
        outR[i] += dr + rr;
    }

    dubFilter.process (outL, outR, n, params.dubFilter);

    for (int i = 0; i < n; ++i)
    {
        const float g = masterSm.next();
        outL[i] = softClip (outL[i] * g);
        outR[i] = softClip (outR[i] * g);
    }
}

void Engine::processChunk (float* outL, float* outR, int n, const MidiEvent* events, int numEvents, const Transport& t)
{
    for (int p = 0; p < kNumParts; ++p)
    {
        std::fill (partL[p].begin(), partL[p].begin() + n, 0.0f);
        std::fill (partR[p].begin(), partR[p].begin() + n, 0.0f);
    }
    applyParams();
    sampler.clearBuses (n);
    runSequencer (n, t);

    int pos = 0;
    for (int e = 0; e < numEvents; ++e)
    {
        const int s = std::clamp (events[e].sample, pos, n);
        renderSegment (pos, s - pos);
        handleMidi (events[e]);
        pos = s;
    }
    renderSegment (pos, n - pos);
    sampler.mixBuses (partL[PartDrums].data(), partR[PartDrums].data(), n);

    mix (outL, outR, n, lastBpm);
}

void Engine::process (float* outL, float* outR, int numSamples, const MidiEvent* events, int numEvents, const Transport& transport)
{
    int done = 0, ev = 0;
    Transport t = transport;
    while (done < numSamples)
    {
        const int n = std::min (maxBlock, numSamples - done);
        int first = ev;
        while (ev < numEvents && events[ev].sample < done + n) ++ev;

        // events for this chunk, rebased to chunk start
        MidiEvent local[256];
        int cnt = 0;
        for (int i = first; i < ev && cnt < 256; ++i) { local[cnt] = events[i]; local[cnt].sample -= done; ++cnt; }

        processChunk (outL + done, outR + done, n, local, cnt, t);

        if (t.hasPpq && t.bpm > 0) t.ppq += (double) n / (sr * 60.0 / t.bpm);
        done += n;
    }
}

} // namespace rf
