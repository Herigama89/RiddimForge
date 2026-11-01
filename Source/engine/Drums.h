// RiddimForge - synthesized drum kit, timbales, percussion, nyabinghi and dub FX
#pragma once
#include "Dsp.h"

namespace rf
{
enum DrumType
{
    Kick, KickBoom, CrossStick, Snare, Clap, Rimshot, Tom,
    HatClosed, HatPedal, HatOpen, Crash, China, Splash, Ride, RideBell,
    Tambourine, Cowbell, BongoHi, BongoLo, CongaMute, CongaOpen, CongaLow,
    TimbaleHi, TimbaleLo, TimbaleRim, AgogoHi, AgogoLo, Cabasa, Maracas, Shaker,
    GuiroShort, GuiroLong, Claves, WoodHi, WoodLo, TriangleMute, TriangleOpen,
    NyaBass, NyaFunde, NyaRepeater, NyaRepeaterSlap,
    Syndrum, DubSiren, AirHorn, Laser,
    kNumDrumTypes
};

struct DrumMapEntry
{
    int type = -1;
    float tune = 1.0f;
    float pan = 0.0f;
    bool percBus = false;
    const char* name = "";
};

// MIDI note -> sound. Unmapped notes have type == -1.
const DrumMapEntry& drumMap (int note);

struct DrumTweaks
{
    float kickTune = 0, snareTune = 0, tomTune = 0, percTune = 0; // semitones
    float hatTone = 0.5f;   // 0 dark .. 1 bright
    float kitDecay = 1.0f;  // multiplier
    float percDecay = 1.0f; // multiplier
};

struct DrumRecipe
{
    // tonal body
    int nPartials = 0;
    float freq[3] {}, amp[3] {}, t60[3] {};
    float pitchDrop = 0, pitchT60 = 0.05f;
    // noise
    int noiseMode = 0; // 0 none, 1 band, 2 high, 3 low
    float noiseAmp = 0, noiseT60 = 0.1f, noiseAttack = 0, noiseFc = 1000, noiseQ = 0.7f;
    // bursts (clap, guiro)
    int bursts = 0; float burstSpacing = 0.01f, burstT60 = 0.01f, burstJitter = 0.2f;
    // metallic (hats, cymbals, cowbell)
    int metalCount = 0; float metalFreq[6] {}; float metalAmp = 0, metalT60 = 0.1f, metalBpFc = 8000, metalHpFc = 6000;
    // click transient
    float clickAmp = 0, clickT60 = 0.004f, clickFreq = 2500;
    // special synth FX: 1 siren, 2 air horn, 3 laser
    int special = 0;
    float length = 2.0f, gain = 1.0f, drive = 0.0f, pan = 0.0f;
    int choke = 0;
    bool percBus = false;
};

class DrumVoice
{
public:
    void prepare (float sampleRate) { sr = sampleRate; }
    void start (const DrumRecipe& recipe, float velocity, int delaySamples, uint32_t age, int note);
    // returns false when finished
    bool render (float* kitL, float* kitR, float* percL, float* percR, int n);
    void choke() { choking = true; }

    bool active = false;
    uint32_t age = 0;
    int note = -1;
    int chokeGroup = 0;

private:
    void begin();
    float special (float t);

    float sr = 44100;
    DrumRecipe r;
    float vel = 1;
    int delay = 0, t = 0, maxLen = 0;
    bool started = false, choking = false;
    float chokeGain = 1, chokeK = 0.99f;
    float gl = 1, gr = 1;

    float ph[3] {}, env[3] {}, envK[3] {}, pEnv = 0, pK = 1;
    float nEnv = 0, nK = 1, nAtkInc = 1, nAtk = 1; SVF nf;
    float mPh[6] {}, mInc[6] {}, mEnv = 0, mK = 1; SVF mbp, mhp;
    float cEnv = 0, cK = 1, cPh = 0;
    int burstLeft = 0, burstTimer = 0, burstInterval = 0; float burstK = 1, tailK = 1;
    Osc so1, so2, so3; SVF sf; float lfoPh = 0;
    Rng rng;
};

class DrumKit
{
public:
    static constexpr int kVoices = 40;
    void prepare (float sampleRate);
    void trigger (int note, float velocity, int delaySamples, const DrumTweaks& tw);
    void render (float* kitL, float* kitR, float* percL, float* percR, int n);
    void allOff();

private:
    DrumVoice voices[kVoices];
    uint32_t counter = 0;
    float sr = 44100;
};

// Build the synthesis recipe for one hit (exposed for tests)
void buildDrumRecipe (int type, float keyTune, const DrumTweaks& tw, DrumRecipe& r);

} // namespace rf
