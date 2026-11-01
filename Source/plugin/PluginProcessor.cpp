#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

namespace
{
struct PartDefault { float db, pan, dly, rev; bool seq; };
const PartDefault kPartDefaults[rf::kNumParts] = {
    { -1.4f,  0.0f,  0.0f,  0.10f, true },  // Drums
    { -4.4f,  0.0f,  0.12f, 0.18f, true },  // Perc
    { -4.4f,  0.25f, 0.18f, 0.25f, true },  // Skank
    { -8.4f, -0.25f, 0.08f, 0.20f, true },  // Piano
    { -8.9f, -0.1f,  0.0f,  0.15f, true },  // Organ
    { -4.7f,  0.0f,  0.0f,  0.0f,  true },  // Bass
    { -4.4f,  0.1f,  0.30f, 0.30f, true },  // Lead (plays only when generated or played)
    { -6.9f,  0.0f,  0.0f,  0.35f, false }, // Pad
};

juce::StringArray styleNames()
{
    juce::StringArray s;
    for (int i = 0; i < rf::numStyles(); ++i) s.add (rf::styleName (i));
    return s;
}
juce::StringArray progressionNames()
{
    juce::StringArray s;
    for (int i = 0; i < rf::numProgressions(); ++i) s.add (rf::progressionName (i));
    return s;
}
juce::StringArray delayNames()
{
    juce::StringArray s;
    for (int i = 0; i < rf::numDelayDivisions(); ++i) s.add (rf::delayDivisionName (i));
    return s;
}

float dbToLevel (float db) { return db <= -47.5f ? 0.0f : juce::Decibels::decibelsToGain (db); }

// default mic pans (drummer's perspective)
float defaultMicPan (int b)
{
    switch (b)
    {
        case rf::BusHiHat: return 0.35f;
        case rf::BusTomHi: return 0.25f;
        case rf::BusTomMid: return 0.0f;
        case rf::BusTomFloor: return -0.35f;
        case rf::BusRotos: return 0.3f;
        case rf::BusTimbales: return 0.45f;
        case rf::BusPerc: return -0.4f;
        default: return 0.0f;
    }
}

// recorded instruments per part, in menu order (keys from instruments.json)
struct RecordedList { int part; std::vector<const char*> keys; std::vector<const char*> names; };
const std::vector<RecordedList>& recorded()
{
    static const std::vector<RecordedList> r = {
        { rf::PartSkank, { "skanky", "chop", "clean" }, { "Skanky Guitar (recorded)", "Chop Guitar (recorded)", "Clean Guitar (recorded)" } },
        { rf::PartPiano, { "piano1", "piano2" }, { "Session Piano (recorded)", "Acoustic Piano (recorded)" } },
        { rf::PartOrgan, { "bubble", "percorg", "organ4" }, { "Bubble Organ (recorded)", "Perc Organ (recorded)", "Organ 4 (recorded)" } },
        { rf::PartBass,  { "bass1", "bass2" }, { "Session Bass (recorded)", "5-String Fingered (recorded)" } },
        { rf::PartLead,  { "leadgtr", "pluckjc", "pluckk" }, { "Lead Guitar (recorded)", "Pluck Guitar JC (recorded)", "Pluck Guitar Kahn (recorded)" } },
    };
    return r;
}
juce::StringArray withRecorded (juce::StringArray synth, int part)
{
    for (auto& r : recorded()) if (r.part == part) for (auto* n : r.names) synth.add (n);
    return synth;
}

const char* genIds[] = { "genBars", "genFillEvery", "genFillInt", "style", "progression" };
} // namespace

//==============================================================================
APVTS::ParameterLayout RiddimForgeProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout layout;
    int v = 1;
    auto pid = [&v] (const String& id) { return ParameterID { id, v }; };

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x * 100.0f)) + "%"; });
    auto semi = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return (x > 0 ? "+" : "") + String (x, 1) + " st"; }).withLabel ("st");
    auto mult = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (x, 2) + "x"; });
    auto dbAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return x <= -47.5f ? String ("-inf dB") : String (x, 1) + " dB"; });
    auto panAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) {
        if (std::abs (x) < 0.01f) return String ("C");
        return String (roundToInt (std::abs (x) * 100.0f)) + (x < 0 ? "L" : "R"); });

    // ---- riddim -----------------------------------------------------------
    layout.add (std::make_unique<AudioParameterBool> (pid (ids::riddimOn), "Riddim On", true));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::runMode), "Riddim Clock", StringArray { "Host Sync", "Free Run" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::freeBpm), "Free Run BPM", NormalisableRange<float> (40.0f, 200.0f, 0.1f), 75.0f,
                                                       AudioParameterFloatAttributes().withLabel ("BPM")));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::style), "Style", styleNames(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::key), "Key",
        StringArray { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" }, 9));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::progression), "Progression", progressionNames(), 5));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::barsPerChord), "Bars per Chord", StringArray { "1 bar", "2 bars", "4 bars" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::chordSource), "Chord Source", StringArray { "Progression", "MIDI Ch 16" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::swing), "Swing", NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::humanize), "Humanize", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::fills), "Fills", StringArray { "Off", "Every 4 bars", "Every 8 bars" }, 1));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::ch1Target), "MIDI Ch 1 Plays",
        StringArray { "Drums & Perc", "Skank Guitar", "Piano", "Organ", "Bass", "Lead", "Pad" }, 0));

    // ---- mixer ------------------------------------------------------------
    for (int p = 0; p < rf::kNumParts; ++p)
    {
        const String n = rf::partName (p);
        const auto& d = kPartDefaults[p];
        layout.add (std::make_unique<AudioParameterFloat> (pid (ids::lvl (p)), n + " Level", NormalisableRange<float> (-48.0f, 6.0f, 0.1f, 2.2f), d.db, dbAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid (ids::pan (p)), n + " Pan", NormalisableRange<float> (-1.0f, 1.0f), d.pan, panAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid (ids::dly (p)), n + " Delay Send", NormalisableRange<float> (0.0f, 1.0f), d.dly, pct));
        layout.add (std::make_unique<AudioParameterFloat> (pid (ids::rev (p)), n + " Reverb Send", NormalisableRange<float> (0.0f, 1.0f), d.rev, pct));
        layout.add (std::make_unique<AudioParameterBool> (pid (ids::mute (p)), n + " Mute", false));
        layout.add (std::make_unique<AudioParameterBool> (pid (ids::seq (p)), n + " In Riddim", d.seq));
    }

    // ---- drums ------------------------------------------------------------
    NormalisableRange<float> tuneR (-12.0f, 12.0f, 0.1f);
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::kickTune), "Kick Tune", tuneR, 0.0f, semi));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::snareTune), "Snare Tune", tuneR, 0.0f, semi));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::tomTune), "Tom Tune", tuneR, 0.0f, semi));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::percTune), "Perc Tune", tuneR, 0.0f, semi));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::hatTone), "Hat Tone", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::kitDecay), "Kit Decay", NormalisableRange<float> (0.4f, 2.5f, 0.01f, 0.6f), 1.0f, mult));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::percDecay), "Perc Decay", NormalisableRange<float> (0.4f, 2.5f, 0.01f, 0.6f), 1.0f, mult));

    // ---- instruments ------------------------------------------------------
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::skankType), "Skank Type",
        withRecorded ({ "Synth Skank" }, rf::PartSkank), 1));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::skankTone), "Skank Tone", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::skankMute), "Skank Palm Mute", NormalisableRange<float> (0.0f, 1.0f), 0.55f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::pianoType), "Piano Type", withRecorded ({ "Grand", "Electric", "Honky Tonk" }, rf::PartPiano), 3));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::pianoTone), "Piano Tone", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::organType), "Organ Drawbars", withRecorded ({ "Bubble", "Jazz 888", "Full", "Flute" }, rf::PartOrgan), 4));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::organTone), "Organ Click", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::organRotary), "Organ Rotary", StringArray { "Off", "Slow", "Fast" }, 1));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::bassType), "Bass Type", withRecorded ({ "Roots Sub", "Dub", "Digital 80s", "Finger" }, rf::PartBass), 4));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::bassTone), "Bass Tone", NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::leadType), "Lead Type", withRecorded ({ "Melodica", "Square Lead", "Synth Horns", "Bubble Synth", "Whistle" }, rf::PartLead), 0));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::leadTone), "Lead Tone", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::padType), "Pad Type", StringArray { "Strings", "Warm Pad", "Choir", "Glass" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::padTone), "Pad Tone", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));

    // ---- fx ---------------------------------------------------------------
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::delayDiv), "Delay Time", delayNames(), 3));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::delayFb), "Delay Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::delayTone), "Delay Tone", NormalisableRange<float> (0.0f, 1.0f), 0.45f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::revSize), "Reverb Size", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::revDamp), "Reverb Damp", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::dubFilter), "Dub Filter", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) {
            if (std::abs (x) < 0.02f) return String ("Off");
            return String (x < 0 ? "LP " : "HP ") + String (roundToInt (std::abs (x) * 100.0f)) + "%"; })));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::master), "Master", NormalisableRange<float> (-24.0f, 6.0f, 0.1f), -2.0f, dbAttr));

    // ---- drums tab (sampled kits) ------------------------------------------
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::drumKit), "Drum Kit", StringArray { "Synth Kit", "Studio Kit", "Kit 2" }, 1));
    auto micDb = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return x <= -59.5f ? String ("-inf") : String (x, 1) + " dB"; });
    for (int b = 0; b < rf::kNumMicBuses; ++b)
    {
        const String n = String ("Mic ") + rf::micBusName (b);
        layout.add (std::make_unique<AudioParameterFloat> (pid (ids::micLvl (b)), n + " Level", NormalisableRange<float> (-60.0f, 12.0f, 0.1f, 2.5f),
                                                           b == rf::BusRoom ? -4.0f : 0.0f, micDb));
        layout.add (std::make_unique<AudioParameterFloat> (pid (ids::micPan (b)), n + " Pan", NormalisableRange<float> (-1.0f, 1.0f), defaultMicPan (b), panAttr));
        layout.add (std::make_unique<AudioParameterBool> (pid (ids::micMute (b)), n + " Mute", false));
        layout.add (std::make_unique<AudioParameterBool> (pid (ids::micSolo (b)), n + " Solo", false));
    }
    for (int s = 0; s < rf::numDrumSlots(); ++s)
    {
        const String n = rf::drumSlotName (s);
        layout.add (std::make_unique<AudioParameterFloat> (pid (ids::slTune (s)), n + " Tune", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, semi));
        layout.add (std::make_unique<AudioParameterFloat> (pid (ids::slLvl (s)), n + " Level", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, dbAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid (ids::slDec (s)), n + " Decay", NormalisableRange<float> (0.05f, 1.0f), 1.0f, pct));
    }

    // ---- generator tab -----------------------------------------------------
    layout.add (std::make_unique<AudioParameterBool> (pid (ids::genOn), "Play Generated Parts", true));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::genBars), "Generator Bars", StringArray { "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" }, 2));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::genFillEvery), "Generator Fills", StringArray { "No fills", "Every 2 bars", "Every 4 bars", "Phrase end" }, 2));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::genFillInt), "Fill Intensity", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    for (int p = 0; p < rf::kNumGenParts; ++p)
    {
        StringArray types;
        for (int t = 0; t < rf::numGenTypes (p); ++t) types.add (rf::genTypeName (p, t));
        const String n = String ("Gen ") + rf::genPartName (p);
        layout.add (std::make_unique<AudioParameterChoice> (pid (ids::genType (p)), n + " Type", types, 0));
        layout.add (std::make_unique<AudioParameterFloat> (pid (ids::genCx (p)), n + " Complexity", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
        layout.add (std::make_unique<AudioParameterBool> (pid (ids::genUse (p)), n + " Play", true));
    }

    return layout;
}

//==============================================================================
RiddimForgeProcessor::RiddimForgeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RiddimForgeState", createLayout())
{
    events.reserve (2048);

    // decode the embedded kits in the background so DAW scans stay fast
    loader = std::thread ([this]
    {
        kits = kitloader::loadEmbeddedKits();
        instSets = kitloader::loadEmbeddedInstruments();
        for (auto& r : recorded())
        {
            std::vector<const rf::InstrumentSet*> sets;
            for (auto* key : r.keys)
            {
                const rf::InstrumentSet* found = nullptr;
                for (auto& s : instSets) if (s->key == key) found = s.get();
                sets.push_back (found);
            }
            engine.setPartSets (r.part, sets);
        }
        std::vector<const rf::Kit*> ptrs;
        for (auto& k : kits) ptrs.push_back (k.get());
        engine.setKits (ptrs);
        kitsLoaded.store (true);
        juce::MessageManager::callAsync ([safe = juce::WeakReference<RiddimForgeProcessor> (this)] { if (safe != nullptr) safe->sendChangeMessage(); });
    });

    for (auto* id : genIds) apvts.addParameterListener (id, this);
    for (int p = 0; p < rf::kNumGenParts; ++p) { apvts.addParameterListener (ids::genType (p), this); apvts.addParameterListener (ids::genCx (p), this); }
}

RiddimForgeProcessor::~RiddimForgeProcessor()
{
    cancelPendingUpdate();
    for (auto* id : genIds) apvts.removeParameterListener (id, this);
    for (int p = 0; p < rf::kNumGenParts; ++p) { apvts.removeParameterListener (ids::genType (p), this); apvts.removeParameterListener (ids::genCx (p), this); }
    if (loader.joinable()) loader.join();
}

bool RiddimForgeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void RiddimForgeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    pullParams();
    engine.prepare (sampleRate, std::max (samplesPerBlock, 64));
    monoTmp.assign ((size_t) std::max (samplesPerBlock, 64), 0.0f);
    keyboardState.reset();
}

float RiddimForgeProcessor::getF (const juce::String& id) const
{
    if (auto* p = apvts.getRawParameterValue (id)) return p->load();
    jassertfalse;
    return 0.0f;
}
int RiddimForgeProcessor::getI (const juce::String& id) const { return juce::roundToInt (getF (id)); }
bool RiddimForgeProcessor::getB (const juce::String& id) const { return getF (id) > 0.5f; }

rf::RiddimSettings RiddimForgeProcessor::readRiddimSettings() const
{
    rf::RiddimSettings s;
    s.style = getI (ids::style);
    s.key = getI (ids::key);
    s.progression = getI (ids::progression);
    static const int bpc[3] = { 1, 2, 4 };
    s.barsPerChord = bpc[juce::jlimit (0, 2, getI (ids::barsPerChord))];
    s.chordFromMidi = getI (ids::chordSource) == 1;
    s.swing = getF (ids::swing);
    s.humanize = getF (ids::humanize);
    s.fills = getI (ids::fills);
    for (int p = 0; p < rf::kNumParts; ++p) s.partOn[(size_t) p] = getB (ids::seq (p));
    s.phrase = phrase.load();
    const bool on = getB (ids::genOn);
    for (int p = 0; p < rf::kNumGenParts; ++p) s.genUse[(size_t) p] = on && getB (ids::genUse (p));
    return s;
}

void RiddimForgeProcessor::pullParams()
{
    auto& P = engine.params;
    P.riddimOn = getB (ids::riddimOn);
    P.runMode = getI (ids::runMode);
    P.freeBpm = getF (ids::freeBpm);
    P.riddim = readRiddimSettings();
    P.ch1Target = getI (ids::ch1Target);

    for (int p = 0; p < rf::kNumParts; ++p)
    {
        auto& pp = P.part[p];
        pp.level = dbToLevel (getF (ids::lvl (p)));
        pp.pan = getF (ids::pan (p));
        pp.delay = getF (ids::dly (p));
        pp.reverb = getF (ids::rev (p));
        pp.mute = getB (ids::mute (p));
    }

    P.drums.kickTune = getF (ids::kickTune);
    P.drums.snareTune = getF (ids::snareTune);
    P.drums.tomTune = getF (ids::tomTune);
    P.drums.percTune = getF (ids::percTune);
    P.drums.hatTone = getF (ids::hatTone);
    P.drums.kitDecay = getF (ids::kitDecay);
    P.drums.percDecay = getF (ids::percDecay);

    P.drumKit = kitsLoaded.load() ? getI (ids::drumKit) : 0;
    for (int b = 0; b < rf::kNumMicBuses; ++b)
    {
        auto& m = P.mic[(size_t) b];
        const float db = getF (ids::micLvl (b));
        m.gain = db <= -59.5f ? 0.0f : juce::Decibels::decibelsToGain (db);
        m.pan = getF (ids::micPan (b));
        m.mute = getB (ids::micMute (b));
        m.solo = getB (ids::micSolo (b));
    }
    for (int sl = 0; sl < rf::numDrumSlots(); ++sl)
    {
        auto& sp = P.slot[(size_t) sl];
        sp.tune = getF (ids::slTune (sl));
        sp.gainDb = getF (ids::slLvl (sl));
        sp.decay = getF (ids::slDec (sl));
    }

    P.skankType = getI (ids::skankType);
    P.skankTone = getF (ids::skankTone);  P.skankMute = getF (ids::skankMute);
    P.pianoType = getI (ids::pianoType);  P.pianoTone = getF (ids::pianoTone);
    P.organType = getI (ids::organType);  P.organTone = getF (ids::organTone); P.organRotary = getI (ids::organRotary);
    P.bassType = getI (ids::bassType);    P.bassTone = getF (ids::bassTone);
    P.leadType = getI (ids::leadType);    P.leadTone = getF (ids::leadTone);
    P.padType = getI (ids::padType);      P.padTone = getF (ids::padTone);

    P.delayDiv = getI (ids::delayDiv);
    P.delayFeedback = getF (ids::delayFb);
    P.delayTone = getF (ids::delayTone);
    P.reverbSize = getF (ids::revSize);
    P.reverbDamp = getF (ids::revDamp);
    P.dubFilter = getF (ids::dubFilter);
    P.masterDb = getF (ids::master);
}

void RiddimForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    // on-screen keyboard and drum pads
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    pullParams();

    events.clear();
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.getRawDataSize() < 1 || msg.isSysEx()) continue;
        const auto* d = msg.getRawData();
        rf::MidiEvent e;
        e.sample = juce::jlimit (0, juce::jmax (0, n - 1), meta.samplePosition);
        e.status = d[0];
        e.d1 = msg.getRawDataSize() > 1 ? d[1] : 0;
        e.d2 = msg.getRawDataSize() > 2 ? d[2] : 0;
        if (events.size() < events.capacity()) events.push_back (e);
    }
    midi.clear();

    rf::Transport t;
    t.bpm = engine.params.freeBpm;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            t.hostPlaying = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) { t.ppq = *ppq; t.hasPpq = true; }
        }
    }
    lastBpm.store (engine.params.runMode == 1 ? (double) engine.params.freeBpm : t.bpm);

    buffer.clear();
    float* L = buffer.getWritePointer (0);
    if (buffer.getNumChannels() >= 2)
    {
        engine.process (L, buffer.getWritePointer (1), n, events.data(), (int) events.size(), t);
    }
    else
    {
        if ((int) monoTmp.size() < n) monoTmp.resize ((size_t) n);
        engine.process (L, monoTmp.data(), n, events.data(), (int) events.size(), t);
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + monoTmp[(size_t) i]);
    }
}

//==============================================================================
juce::String RiddimForgeProcessor::riddimFileName (const juce::String& suffix) const
{
    auto s = readRiddimSettings();
    juce::String name = juce::String ("RiddimForge ") + rf::styleName (s.style) + " " + rf::noteName (s.key)
                        + " " + rf::progressionName (s.progression) + (suffix.isNotEmpty() ? " " + suffix : juce::String());
    return juce::File::createLegalFileName (name.replace ("(", "").replace (")", "").replace ("&", "and")) + ".mid";
}

juce::MidiFile RiddimForgeProcessor::createRiddimMidi (int bars, unsigned partMask) const
{
    const auto s = readRiddimSettings();
    const double bpm = lastBpm.load() > 0 ? lastBpm.load() : 75.0;
    const int tpq = 960;
    const double tps = tpq / 4.0; // ticks per 16th

    static const int channelFor[rf::kNumParts] = { 10, 10, 2, 3, 4, 5, 6, 7 };
    static const char* trackNames[] = { "RF Drums+Perc", "RF Skank", "RF Piano", "RF Organ", "RF Bass", "RF Lead", "RF Pad" };
    auto trackFor = [] (int part) { return part <= rf::PartPerc ? 0 : part - 1; };

    std::vector<juce::MidiMessageSequence> tracks (7);
    for (int t = 0; t < 7; ++t)
        tracks[(size_t) t].addEvent (juce::MidiMessage::textMetaEvent (3, trackNames[t]), 0);

    std::vector<rf::SeqEvent> ev;
    for (int64_t k = 0; k < (int64_t) bars * 16; ++k)
    {
        ev.clear();
        rf::generateStep (k, s, rf::progressionChord (s, k / 16), rf::progressionChord (s, k / 16 + 1), ev);
        for (const auto& e : ev)
        {
            if ((partMask & (1u << (unsigned) e.part)) == 0) continue;
            const double start = (double) k * tps + e.offsetSteps * tps + e.offsetSec * (bpm / 60.0) * tpq;
            const bool drum = e.part <= rf::PartPerc;
            const double len = drum ? tps * 0.5 : std::max (10.0, e.gateSteps * tps);
            const int ch = channelFor[e.part];
            const auto vel = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (e.vel * 127.0f));
            auto& tr = tracks[(size_t) trackFor (e.part)];
            tr.addEvent (juce::MidiMessage::noteOn (ch, e.note, vel), start);
            tr.addEvent (juce::MidiMessage::noteOff (ch, e.note), start + len);
        }
    }

    juce::MidiFile file;
    file.setTicksPerQuarterNote (tpq);
    juce::MidiMessageSequence conductor;
    conductor.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / bpm)), 0);
    conductor.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    file.addTrack (conductor);
    for (auto& t : tracks)
    {
        if (t.getNumEvents() <= 1) continue;
        t.sort();
        t.updateMatchedPairs();
        file.addTrack (t);
    }
    return file;
}

//==============================================================================
juce::StringArray RiddimForgeProcessor::kitNames() const
{
    juce::StringArray n { "Synth Kit" };
    if (kitsLoaded.load()) for (auto& k : kits) n.add (k->name);
    return n;
}

const rf::Kit* RiddimForgeProcessor::currentKit() const
{
    const int k = getI (ids::drumKit);
    if (! kitsLoaded.load() || k <= 0 || k > (int) kits.size()) return nullptr;
    return kits[(size_t) (k - 1)].get();
}

juce::String RiddimForgeProcessor::loadUserSample (int slot, const juce::File& file)
{
    if (slot < 0 || slot >= rf::numDrumSlots()) return "Invalid drum slot";
    juce::String err;
    auto s = kitloader::loadUserSample (file, err);
    if (s == nullptr) return err;
    engine.getSampler().setUserSample (slot, s.get());
    userStore.push_back (std::move (s));
    userPaths[(size_t) slot] = file.getFullPathName();
    apvts.state.setProperty (juce::String ("user_") + rf::drumSlotKey (slot), file.getFullPathName(), nullptr);
    sendChangeMessage();
    return {};
}

void RiddimForgeProcessor::clearUserSample (int slot)
{
    if (slot < 0 || slot >= rf::numDrumSlots()) return;
    engine.getSampler().setUserSample (slot, nullptr);
    userPaths[(size_t) slot] = {};
    apvts.state.removeProperty (juce::String ("user_") + rf::drumSlotKey (slot), nullptr);
    sendChangeMessage();
}

juce::String RiddimForgeProcessor::userSampleName (int slot) const
{
    if (slot < 0 || slot >= rf::kMaxDrumSlots || userPaths[(size_t) slot].isEmpty()) return {};
    return juce::File (userPaths[(size_t) slot]).getFileName();
}

//==============================================================================
int RiddimForgeProcessor::generatorBars() const
{
    static const int b[] = { 1, 2, 4, 8, 16 };
    return b[juce::jlimit (0, 4, getI (ids::genBars))];
}

unsigned RiddimForgeProcessor::partMaskForGen (int gp)
{
    if (gp == rf::GPDrums) return (1u << rf::PartDrums) | (1u << rf::PartPerc);
    return 1u << (unsigned) rf::genPartToRiddimPart (gp);
}

rf::GenSettings RiddimForgeProcessor::readGenSettings() const
{
    rf::GenSettings g;
    g.bars = generatorBars();
    g.riddimStyle = getI (ids::style);
    rf::RiddimSettings rs = readRiddimSettings();
    g.minor = rf::progressionChord (rs, 0).quality == rf::QMin || rf::progressionChord (rs, 0).quality == rf::QMin7;
    g.fillEvery = getI (ids::genFillEvery);
    g.fillIntensity = getF (ids::genFillInt);
    for (int p = 0; p < rf::kNumGenParts; ++p)
    {
        g.type[(size_t) p] = getI (ids::genType (p));
        g.complexity[(size_t) p] = getF (ids::genCx (p));
        g.seed[(size_t) p] = seeds[(size_t) p];
    }
    return g;
}

void RiddimForgeProcessor::publishPhrase (std::unique_ptr<rf::Phrase> p)
{
    phrase.store (p.get());
    phraseStore.push_back (std::move (p));
    // keep a few old versions alive: the audio thread may still be reading the previous one
    if (phraseStore.size() > 12) phraseStore.erase (phraseStore.begin(), phraseStore.begin() + (long) (phraseStore.size() - 12));
    sendChangeMessage();
}

void RiddimForgeProcessor::generate (int gp, bool newSeed)
{
    juce::Random r;
    auto p = std::make_unique<rf::Phrase>();
    if (auto* cur = phrase.load()) *p = *cur;
    auto g = readGenSettings();
    p->bars = g.bars;
    for (int part = 0; part < rf::kNumGenParts; ++part)
    {
        const bool target = gp == -1 || gp == part;   // -2 = rebuild live parts only
        if (target && newSeed) seeds[(size_t) part] = (uint32_t) r.nextInt (0x7fffffff) + 1u;
        g.seed[(size_t) part] = seeds[(size_t) part];
        if (target) generated[(size_t) part] = true;
        if (generated[(size_t) part]) rf::generatePart (part, g, *p);   // bar count may have changed: regenerate all live parts
        else { p->has[(size_t) part] = false; p->events[(size_t) part].clear(); }
        apvts.state.setProperty ("seed" + juce::String (part), (int) seeds[(size_t) part], nullptr);
        apvts.state.setProperty ("gen" + juce::String (part), generated[(size_t) part], nullptr);
    }
    p->index();
    publishPhrase (std::move (p));
}

void RiddimForgeProcessor::clearGenerated (int gp)
{
    for (int part = 0; part < rf::kNumGenParts; ++part)
        if (gp < 0 || gp == part) generated[(size_t) part] = false;
    bool any = false;
    for (bool b : generated) any |= b;
    for (int part = 0; part < rf::kNumGenParts; ++part)
        apvts.state.setProperty ("gen" + juce::String (part), generated[(size_t) part], nullptr);
    if (! any) { phrase.store (nullptr); sendChangeMessage(); return; }
    generate (-2, false); // -2: no target, just rebuild the live parts
}

void RiddimForgeProcessor::parameterChanged (const juce::String&, float) { triggerAsyncUpdate(); }

void RiddimForgeProcessor::handleAsyncUpdate()
{
    bool any = false;
    for (bool b : generated) any |= b;
    if (any) generate (-2, false);
}

void RiddimForgeProcessor::restoreExtraState()
{
    auto& st = apvts.state;
    bool any = false;
    for (int p = 0; p < rf::kNumGenParts; ++p)
    {
        if (st.hasProperty ("seed" + juce::String (p))) seeds[(size_t) p] = (uint32_t) (int) st.getProperty ("seed" + juce::String (p));
        generated[(size_t) p] = (bool) st.getProperty ("gen" + juce::String (p), false);
        any |= generated[(size_t) p];
    }
    if (any) generate (-2, false); else phrase.store (nullptr);

    for (int s = 0; s < rf::numDrumSlots(); ++s)
    {
        const auto path = st.getProperty (juce::String ("user_") + rf::drumSlotKey (s)).toString();
        if (path.isNotEmpty() && juce::File (path).existsAsFile()) loadUserSample (s, juce::File (path));
        else engine.getSampler().setUserSample (s, nullptr);
    }
}

//==============================================================================
void RiddimForgeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RiddimForgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            restoreExtraState();
        }
}

juce::AudioProcessorEditor* RiddimForgeProcessor::createEditor()
{
    return new RiddimForgeEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RiddimForgeProcessor();
}
