// RiddimForge - sampled instruments cut from the session stems
//   note sets  : keymapped single notes (bass, lead guitar, plucks)
//   chord sets : chord chops re-pitched to the requested chord (skank, organ, piano)
#pragma once
#include "Instruments.h"
#include <atomic>
#include <string>
#include <vector>

namespace rf
{
enum SampleQuality { SQMaj = 0, SQMin = 1, SQNeutral = 2, SQMute = 3 };

struct InstSample
{
    std::vector<std::vector<float>> ch;
    int length = 0;
    double sampleRate = 48000.0;
    int midi = -1;          // note sets
    float cents = 0.0f;     // tuning of the recorded note
    int root = -1;          // chord sets: pitch class
    int quality = SQMaj;
    float levelDb = 0.0f;
};

struct InstrumentSet
{
    std::string key, name, part;
    bool chords = false;
    float trimDb = 0.0f;   // loudness match against the synth sound of the same part
    std::vector<InstSample> samples;
    float minDb = 0.0f, maxDb = 0.0f;
    void finalize();
};

class SampledInstrument : public InstrumentBase
{
public:
    static constexpr int kVoices = 24;
    explicit SampledInstrument (bool monophonic);

    void setSet (const InstrumentSet* s) { set.store (s); }
    const InstrumentSet* getSet() const { return set.load(); }
    void setFallback (InstrumentBase* f) { fallback = f; }

    void prepare (float sampleRate) override;
    void noteOn (int note, float velocity, int gateSamples = -1, int delaySamples = 0) override;
    void noteOff (int note) override;
    void setPedal (bool down) override;
    void allOff() override;
    void render (float* L, float* R, int n) override;
    int activeVoices() const override;

private:
    struct Pending { int note; float vel; int gate; long long start; };
    struct Voice
    {
        bool active = false, releasing = false;
        const InstSample* s = nullptr;
        double pos = 0, rate = 1;
        float gain = 1, env = 1, relK = 0.999f;
        long long start = 0;
        int gate = -1;               // samples after start, -1 = until noteOff
        int noteTag = -1;            // MIDI note (note sets) or chord trigger id
        std::vector<int> chordNotes; // notes that make up this chord trigger
        bool held = true, sustained = false;
        uint32_t age = 0;
        float baseShift = 0;         // semitones (for pitch bend)
    };

    void startGroup (long long when);
    void triggerChord (const std::vector<Pending>& grp, long long when);
    void triggerNote (const Pending& p, long long when);
    Voice* allocVoice();
    void releaseVoice (Voice& v, float seconds);
    const InstSample* pickChord (int root, int quality, float vel, float& shift, float& gain);
    const InstSample* pickNote (int note, float vel, float& shift, float& gain);

    std::atomic<const InstrumentSet*> set { nullptr };
    InstrumentBase* fallback = nullptr;
    const bool mono;
    float sr = 44100.0f;
    long long now = 0;
    std::vector<Pending> pending;
    std::vector<float> fbL, fbR;   // fallback synth renders here so its own filters don't touch the samples
    std::array<bool, 128> heldNotes {};
    Voice voices[kVoices];
    uint32_t counter = 0;
    int lastIdx = -1;
    bool pedal = false;
    Rng rng;
};

} // namespace rf
