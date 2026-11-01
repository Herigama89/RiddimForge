// RiddimForge - multi-mic drum sampler (Handy Drums style mic mixer)
#pragma once
#include "Dsp.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace rf
{
constexpr int kNumMicBuses = 14;
enum MicBus { BusKickIn, BusKickOut, BusSnareTop, BusSnareBottom, BusHiHat, BusTomHi, BusTomMid, BusTomFloor,
              BusRotos, BusTimbales, BusPerc, BusCymbals, BusOverheads, BusRoom };
const char* micBusName (int bus);

enum RouteMode { RouteMono = 0, RouteL = 1, RouteR = 2 };

struct KitSample
{
    std::vector<std::vector<float>> ch;   // channel data
    int length = 0;
    float levelDb = 0.0f;
    double sampleRate = 48000.0;
};

struct KitArtic
{
    std::string key, name;
    std::vector<int> notes;
    int choke = 0;
    std::vector<int> routeBus, routeMode;  // one entry per sample channel
    std::vector<KitSample> samples;
    float minDb = 0, maxDb = 0;
};

struct Kit
{
    std::string name, description;
    float outputGainDb = 0.0f;
    std::vector<KitArtic> artics;
    int noteToArtic[128];
    void buildIndex();
    int findArtic (const std::string& key) const;
};

// Articulation "slots" shared by every kit (for per-drum controls and user samples)
int numDrumSlots();
const char* drumSlotKey (int slot);
const char* drumSlotName (int slot);
int drumSlotIndex (const std::string& key);
int drumSlotForNote (int note);           // -1 if no slot uses this note
int drumSlotDefaultBus (int slot);
const std::vector<int>& drumSlotNotes (int slot);
constexpr int kMaxDrumSlots = 24;

struct MicParams { float gain = 1.0f, pan = 0.0f; bool mute = false, solo = false; };
struct SlotParams { float tune = 0.0f, gainDb = 0.0f, decay = 1.0f; };

class DrumSampler
{
public:
    static constexpr int kVoices = 64;

    DrumSampler();
    void prepare (double sampleRate, int maxBlock);
    void setKit (const Kit* k) { kit.store (k); }
    const Kit* getKit() const { return kit.load(); }
    // user replacement for a slot (nullptr = use kit). Sample objects must outlive playback.
    void setUserSample (int idx, const KitSample* s) { if (idx >= 0 && idx < kMaxDrumSlots) user[idx].store (s); }
    const KitSample* getUserSample (int idx) const { return (idx >= 0 && idx < kMaxDrumSlots) ? user[idx].load() : nullptr; }

    // returns false if neither the kit nor a user sample covers this note
    bool trigger (int note, float velocity, int delaySamples);
    void renderVoices (int start, int n);          // voices -> mic buses
    void mixBuses (float* outL, float* outR, int n); // mic buses -> stereo (whole block)
    void clearBuses (int n);
    void allOff();

    MicParams mic[kNumMicBuses];
    SlotParams slot[kMaxDrumSlots];
    std::atomic<float> busMeter[kNumMicBuses];

private:
    struct Voice
    {
        bool active = false, choking = false;
        const KitSample* s = nullptr;
        int nch = 0, bus[8] {}, mode[8] {};
        double pos = 0, rate = 1;
        int delay = 0;
        float gain = 1, chokeGain = 1, chokeK = 0.99f;
        double fadeStart = 1e12, fadeLen = 1;
        int choke = 0, note = -1;
        uint32_t age = 0;
    };
    Voice* allocVoice();
    const KitSample* pick (const KitArtic& a, float vel, int articIndex, float& gainOut);

    std::atomic<const Kit*> kit { nullptr };
    std::atomic<const KitSample*> user[kMaxDrumSlots];
    Voice voices[kVoices];
    std::vector<float> busL[kNumMicBuses], busR[kNumMicBuses];
    Smoothed busGain[kNumMicBuses];
    double sr = 44100.0;
    uint32_t counter = 0;
    int lastPick[64] {};
    Rng rng;
};

} // namespace rf
