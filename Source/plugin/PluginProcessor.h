#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "KitLoader.h"
#include <thread>

namespace ids
{
// global
inline constexpr const char* riddimOn   = "riddimOn";
inline constexpr const char* preview    = "preview";
inline constexpr const char* runMode    = "runMode";
inline constexpr const char* freeBpm    = "freeBpm";
inline constexpr const char* style      = "style";
inline constexpr const char* key        = "key";
inline constexpr const char* progression = "progression";
inline constexpr const char* barsPerChord = "barsPerChord";
inline constexpr const char* chordSource = "chordSource";
inline constexpr const char* swing      = "swing";
inline constexpr const char* humanize   = "humanize";
inline constexpr const char* fills      = "fills";
inline constexpr const char* ch1Target  = "ch1Target";
// drums
inline constexpr const char* kickTune   = "kickTune";
inline constexpr const char* snareTune  = "snareTune";
inline constexpr const char* tomTune    = "tomTune";
inline constexpr const char* percTune   = "percTune";
inline constexpr const char* hatTone    = "hatTone";
inline constexpr const char* kitDecay   = "kitDecay";
inline constexpr const char* percDecay  = "percDecay";
// instruments
inline constexpr const char* skankType  = "skankType";
inline constexpr const char* skankTone  = "skankTone";
inline constexpr const char* skankMute  = "skankMute";
inline constexpr const char* pianoType  = "pianoType";
inline constexpr const char* pianoTone  = "pianoTone";
inline constexpr const char* organType  = "organType";
inline constexpr const char* organTone  = "organTone";
inline constexpr const char* organRotary = "organRotary";
inline constexpr const char* bassType   = "bassType";
inline constexpr const char* bassTone   = "bassTone";
inline constexpr const char* leadType   = "leadType";
inline constexpr const char* leadTone   = "leadTone";
inline constexpr const char* padType    = "padType";
inline constexpr const char* padTone    = "padTone";
// fx
inline constexpr const char* delayDiv   = "delayDiv";
inline constexpr const char* delayFb    = "delayFb";
inline constexpr const char* delayTone  = "delayTone";
inline constexpr const char* revSize    = "revSize";
inline constexpr const char* revDamp    = "revDamp";
inline constexpr const char* dubFilter  = "dubFilter";
inline constexpr const char* master     = "master";

// drums tab
inline constexpr const char* drumKit    = "drumKit";
inline juce::String micLvl (int b)  { return "micLvl" + juce::String (b); }
inline juce::String micPan (int b)  { return "micPan" + juce::String (b); }
inline juce::String micMute (int b) { return "micMute" + juce::String (b); }
inline juce::String micSolo (int b) { return "micSolo" + juce::String (b); }
inline juce::String slTune (int s)  { return juce::String ("slTune_") + rf::drumSlotKey (s); }
inline juce::String slLvl (int s)   { return juce::String ("slLvl_") + rf::drumSlotKey (s); }
inline juce::String slDec (int s)   { return juce::String ("slDec_") + rf::drumSlotKey (s); }
// generator tab
inline constexpr const char* genOn       = "genOn";
inline constexpr const char* genBars     = "genBars";
inline constexpr const char* genFillEvery = "genFillEvery";
inline constexpr const char* genFillInt  = "genFillInt";
inline juce::String genType (int p) { return "genType" + juce::String (p); }
inline juce::String genCx (int p)   { return "genCx" + juce::String (p); }
inline juce::String genUse (int p)  { return "genUse" + juce::String (p); }

// per part: "<prefix><partIndex>"
inline juce::String lvl (int p)  { return "lvl" + juce::String (p); }
inline juce::String pan (int p)  { return "pan" + juce::String (p); }
inline juce::String dly (int p)  { return "dly" + juce::String (p); }
inline juce::String rev (int p)  { return "rev" + juce::String (p); }
inline juce::String mute (int p) { return "mute" + juce::String (p); }
inline juce::String seq (int p)  { return "seq" + juce::String (p); }
inline juce::String solo (int p) { return "solo" + juce::String (p); }
} // namespace ids

class RiddimForgeProcessor : public juce::AudioProcessor,
                             public juce::ChangeBroadcaster,
                             private juce::AsyncUpdater,
                             private juce::AudioProcessorValueTreeState::Listener
{
public:
    RiddimForgeProcessor();
    ~RiddimForgeProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RiddimForge"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Builds a multi-track MIDI file of the current riddim (partMask: bit per rf::PartId)
    // forceOn: export the masked parts even if they are switched off in the mix (generator drag)
    juce::MidiFile createRiddimMidi (int bars, unsigned partMask = 0xffu, bool forceOn = false) const;
    unsigned generatedPartsMask() const;
    juce::String riddimFileName (const juce::String& suffix = {}) const;

    // ---- sampled kits ------------------------------------------------------
    bool kitsReady() const { return kitsLoaded.load(); }
    juce::StringArray kitNames() const;
    const rf::Kit* currentKit() const;
    // user sample replacement (message thread)
    juce::String loadUserSample (int slot, const juce::File& file);
    void clearUserSample (int slot);
    juce::String userSampleName (int slot) const;

    // ---- generator ---------------------------------------------------------
    void generate (int genPart, bool newSeed);   // genPart = -1 for all parts
    void clearGenerated (int genPart);
    bool isGenerated (int genPart) const { return generated[(size_t) genPart]; }
    const rf::Phrase* getPhrase() const { return phrase.load(); }
    int generatorBars() const;
    static unsigned partMaskForGen (int genPart);

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    rf::Engine engine;
    std::atomic<double> lastBpm { 75.0 };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void pullParams();
    void handleAsyncUpdate() override;
    void parameterChanged (const juce::String& id, float newValue) override;
    rf::GenSettings readGenSettings() const;
    void publishPhrase (std::unique_ptr<rf::Phrase> p);
    void restoreExtraState();
    rf::RiddimSettings readRiddimSettings() const;
    float getF (const juce::String& id) const;
    int getI (const juce::String& id) const;
    bool getB (const juce::String& id) const;

    std::vector<rf::MidiEvent> events;

    std::vector<std::unique_ptr<rf::Kit>> kits;
    std::vector<std::unique_ptr<rf::InstrumentSet>> instSets;
    std::atomic<bool> kitsLoaded { false };
    std::thread loader;

    std::vector<std::unique_ptr<rf::KitSample>> userStore;   // kept alive for the plugin's lifetime
    std::array<juce::String, rf::kMaxDrumSlots> userPaths;

    std::atomic<const rf::Phrase*> phrase { nullptr };
    std::vector<std::unique_ptr<rf::Phrase>> phraseStore;
    std::array<uint32_t, rf::kNumGenParts> seeds { 101, 202, 303, 404, 505, 606 };
    std::array<bool, rf::kNumGenParts> generated {};
    std::vector<float> monoTmp;

    JUCE_DECLARE_WEAK_REFERENCEABLE (RiddimForgeProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiddimForgeProcessor)
};
