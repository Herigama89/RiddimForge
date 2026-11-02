// RiddimForge - top-level engine: MIDI routing, riddim sequencer, mixer and FX
#pragma once
#include "Drums.h"
#include "DrumSampler.h"
#include "Effects.h"
#include "Instruments.h"
#include "Riddim.h"
#include "SampledInstrument.h"
#include <atomic>

namespace rf
{
// What MIDI channel 1 (and any unassigned channel) plays
enum PlayTarget { TargetDrums, TargetSkank, TargetPiano, TargetOrgan, TargetBass, TargetLead, TargetPad, kNumTargets };

struct PartParams
{
    float level = 0.8f;   // linear gain
    float pan = 0.0f;     // -1..1
    float delay = 0.0f;   // send 0..1
    float reverb = 0.0f;  // send 0..1
    bool mute = false;
    bool solo = false;
};

struct EngineParams
{
    PartParams part[kNumParts];

    bool riddimOn = true;
    bool preview = false;     // the plugin's own PLAY button
    int runMode = 0;          // 0 = host sync (DAW play or PLAY button), 1 = internal tempo (PLAY button)
    float freeBpm = 75.0f;
    RiddimSettings riddim;

    int ch1Target = TargetDrums;

    DrumTweaks drums;
    int drumKit = 1;                                  // 0 = synth kit, 1.. = sampled kits
    std::array<MicParams, kNumMicBuses> mic {};
    std::array<SlotParams, kMaxDrumSlots> slot {};
    int skankType = 1;  float skankTone = 0.6f, skankMute = 0.55f;   // type >= synth count = recorded set
    int pianoType = 0;  float pianoTone = 0.5f;
    int organType = 0;  float organTone = 0.5f; int organRotary = 1;
    int bassType = 0;   float bassTone = 0.4f;
    int leadType = 0;   float leadTone = 0.5f;
    int padType = 0;    float padTone = 0.5f;

    int delayDiv = 3;         // see delayDivisionBeats()
    float delayFeedback = 0.5f, delayTone = 0.45f;
    float reverbSize = 0.6f, reverbDamp = 0.5f;
    float dubFilter = 0.0f;   // -1 LP .. 0 off .. +1 HP
    float masterDb = -2.0f;
};

int numDelayDivisions();
const char* delayDivisionName (int i);
float delayDivisionBeats (int i);

struct MidiEvent
{
    int sample = 0;
    uint8_t status = 0, d1 = 0, d2 = 0;
};

struct Transport
{
    bool hostPlaying = false;
    bool hasPpq = false;
    double ppq = 0.0;
    double bpm = 120.0;
};

class Engine
{
public:
    Engine();
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    // events must be sorted by sample position
    void process (float* outL, float* outR, int numSamples, const MidiEvent* events, int numEvents, const Transport& transport);

    EngineParams params;

    // sampled kits are owned by the caller and must outlive the engine (safe to call from any thread)
    static constexpr int kMaxKits = 8;
    void setKits (const std::vector<const Kit*>& k)
    {
        for (int i = 0; i < kMaxKits; ++i) kits[(size_t) i].store (i < (int) k.size() ? k[(size_t) i] : nullptr);
    }
    std::atomic<long long> uiAbsStep { -1 };

    // recorded instrument sets per part (owned by the caller, safe from any thread)
    static constexpr int kMaxSetsPerPart = 8;
    void setPartSets (int part, const std::vector<const InstrumentSet*>& sets)
    {
        for (int i = 0; i < kMaxSetsPerPart; ++i)
            partSets[(size_t) part][(size_t) i].store (i < (int) sets.size() ? sets[(size_t) i] : nullptr);
    }
    static int synthTypeCount (int part);
    DrumSampler& getSampler() { return sampler; }

    // read-only info for the UI
    std::atomic<int> uiStep { -1 };
    std::atomic<int> uiChordRoot { 9 }, uiChordQuality { QMin };
    std::atomic<float> uiMeter[kNumParts];
    std::atomic<bool> uiRunning { false };

private:
    void processChunk (float* outL, float* outR, int n, const MidiEvent* events, int numEvents, const Transport& t);
    void applyParams();
    void handleMidi (const MidiEvent& e);
    void playNote (int target, int note, float vel);
    void stopNote (int target, int note);
    void runSequencer (int n, const Transport& t);
    void renderSegment (int start, int len);
    void mix (float* outL, float* outR, int n, double bpm);
    void triggerDrum (int note, float vel, int delaySamples);
    InstrumentBase* instrumentForTarget (int target);
    InstrumentBase* instrumentForPart (int part);

    double sr = 44100.0;
    int maxBlock = 512;

    DrumKit drums;
    DrumSampler sampler;
    std::array<std::atomic<const Kit*>, kMaxKits> kits {};
    std::unique_ptr<InstrumentBase> skank, piano, organ, bass, lead, pad;
    std::unique_ptr<SampledInstrument> sampled[kNumParts];
    InstrumentBase* active[kNumParts] {};
    std::array<std::array<std::atomic<const InstrumentSet*>, kMaxSetsPerPart>, kNumParts> partSets {};
    InstrumentBase* synthForPart (int part);

    std::vector<float> partL[kNumParts], partR[kNumParts];
    std::vector<float> delL, delR, revL, revR;
    Smoothed gainSm[kNumParts];
    Smoothed masterSm;

    DubDelay delay;
    Reverb reverb;
    DubFilter dubFilter;

    std::array<bool, 128> chordHeld {};
    Chord midiChord;
    bool haveMidiChord = false;

    double freePpq = 0.0;
    bool freeRunning = false, seqWasRunning = false;
    double lastBpm = 120.0;
    std::vector<SeqEvent> seqEvents;
};

} // namespace rf
