#include "KitLoader.h"
#include "RFKits.h"

namespace kitloader
{
static const char* findResource (const juce::String& originalName, int& size)
{
    for (int i = 0; i < RFKits::namedResourceListSize; ++i)
        if (originalName == RFKits::originalFilenames[i])
            return RFKits::getNamedResource (RFKits::namedResourceList[i], size);
    size = 0;
    return nullptr;
}

static bool readAll (juce::AudioFormatReader& reader, rf::KitSample& s)
{
    const int len = (int) juce::jmin<juce::int64> (reader.lengthInSamples, 48000 * 30);
    const int ch = (int) reader.numChannels;
    if (len <= 0 || ch <= 0) return false;
    juce::AudioBuffer<float> buf (ch, len);
    if (! reader.read (&buf, 0, len, 0, true, true)) return false;
    s.ch.assign ((size_t) ch, {});
    float peak = 0;
    for (int c = 0; c < ch; ++c)
    {
        s.ch[(size_t) c].assign (buf.getReadPointer (c), buf.getReadPointer (c) + len);
        s.ch[(size_t) c].push_back (0.0f); // guard sample for interpolation
        peak = juce::jmax (peak, buf.getMagnitude (c, 0, len));
    }
    s.length = len;
    s.sampleRate = reader.sampleRate > 0 ? reader.sampleRate : 48000.0;
    s.levelDb = juce::Decibels::gainToDecibels (peak, -100.0f);
    return true;
}

std::vector<std::unique_ptr<rf::Kit>> loadEmbeddedKits()
{
    std::vector<std::unique_ptr<rf::Kit>> kits;
    int size = 0;
    const char* json = findResource ("kits.json", size);
    if (json == nullptr) return kits;

    const auto root = juce::JSON::parse (juce::String::fromUTF8 (json, size));
    const auto* kitArr = root["kits"].getArray();
    if (kitArr == nullptr) return kits;

    juce::FlacAudioFormat flac;
    for (const auto& kv : *kitArr)
    {
        auto kit = std::make_unique<rf::Kit>();
        kit->name = kv["name"].toString().toStdString();
        kit->description = kv["description"].toString().toStdString();
        kit->outputGainDb = (float) (double) kv.getProperty ("outputGainDb", 0.0);
        if (const auto* arts = kv["articulations"].getArray())
        {
            for (const auto& av : *arts)
            {
                rf::KitArtic a;
                a.key = av["key"].toString().toStdString();
                a.name = av["name"].toString().toStdString();
                a.choke = (int) av["choke"];
                if (const auto* notes = av["notes"].getArray()) for (const auto& n : *notes) a.notes.push_back ((int) n);
                if (const auto* routes = av["routing"].getArray())
                    for (const auto& r : *routes)
                    {
                        a.routeBus.push_back ((int) r[0]);
                        const auto m = r[1].toString();
                        a.routeMode.push_back (m == "L" ? rf::RouteL : (m == "R" ? rf::RouteR : rf::RouteMono));
                    }
                if (const auto* samples = av["samples"].getArray())
                    for (const auto& sv : *samples)
                    {
                        int fsz = 0;
                        const char* data = findResource (sv["file"].toString(), fsz);
                        if (data == nullptr) continue;
                        std::unique_ptr<juce::AudioFormatReader> reader (
                            flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) fsz, false), true));
                        if (reader == nullptr) continue;
                        rf::KitSample s;
                        if (! readAll (*reader, s)) continue;
                        s.levelDb = (float) (double) sv["level"];   // level measured on the source mic
                        a.samples.push_back (std::move (s));
                    }
                kit->artics.push_back (std::move (a));
            }
        }
        kit->buildIndex();
        kits.push_back (std::move (kit));
    }
    return kits;
}

std::vector<std::unique_ptr<rf::InstrumentSet>> loadEmbeddedInstruments()
{
    std::vector<std::unique_ptr<rf::InstrumentSet>> sets;
    int size = 0;
    const char* json = findResource ("instruments.json", size);
    if (json == nullptr) return sets;
    const auto root = juce::JSON::parse (juce::String::fromUTF8 (json, size));
    const auto* arr = root["instruments"].getArray();
    if (arr == nullptr) return sets;
    juce::FlacAudioFormat flac;
    for (const auto& iv : *arr)
    {
        auto set = std::make_unique<rf::InstrumentSet>();
        set->key = iv["key"].toString().toStdString();
        set->name = iv["name"].toString().toStdString();
        set->part = iv["part"].toString().toStdString();
        set->chords = iv["kind"].toString() == "chords";
        set->trimDb = (float) (double) iv.getProperty ("trimDb", 0.0);
        if (const auto* samples = iv["samples"].getArray())
            for (const auto& sv : *samples)
            {
                int fsz = 0;
                const char* data = findResource (sv["file"].toString(), fsz);
                if (data == nullptr) continue;
                std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) fsz, false), true));
                if (reader == nullptr) continue;
                rf::KitSample tmp;
                if (! readAll (*reader, tmp)) continue;
                rf::InstSample s;
                s.ch = std::move (tmp.ch); s.length = tmp.length; s.sampleRate = tmp.sampleRate;
                s.midi = (int) sv.getProperty ("midi", -1);
                s.cents = (float) (double) sv.getProperty ("cents", 0.0);
                s.root = (int) sv.getProperty ("root", -1);
                const auto q = sv.getProperty ("q", "maj").toString();
                s.quality = q == "min" ? rf::SQMin : (q == "5" ? rf::SQNeutral : (q == "mute" ? rf::SQMute : rf::SQMaj));
                s.levelDb = (float) (double) sv["level"];
                set->samples.push_back (std::move (s));
            }
        set->finalize();
        sets.push_back (std::move (set));
    }
    return sets;
}

bool isSupportedAudioFile (const juce::String& path)
{
    return path.endsWithIgnoreCase (".wav") || path.endsWithIgnoreCase (".aif") || path.endsWithIgnoreCase (".aiff")
        || path.endsWithIgnoreCase (".flac") || path.endsWithIgnoreCase (".mp3") || path.endsWithIgnoreCase (".ogg")
        || path.endsWithIgnoreCase (".m4a") || path.endsWithIgnoreCase (".caf");
}

std::unique_ptr<rf::KitSample> loadUserSample (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) { error = "Couldn't read " + file.getFileName(); return nullptr; }
    if (reader->lengthInSamples > (juce::int64) reader->sampleRate * 30) { error = "Sample is longer than 30 seconds"; return nullptr; }
    auto s = std::make_unique<rf::KitSample>();
    if (! readAll (*reader, *s)) { error = "Couldn't decode " + file.getFileName(); return nullptr; }
    if (s->ch.size() > 2) s->ch.resize (2);
    // normalise loudness so a user sample sits at a sensible level
    const float peak = juce::Decibels::decibelsToGain (s->levelDb);
    if (peak > 0.0001f)
    {
        const float g = 0.7f / peak;
        for (auto& c : s->ch) for (auto& x : c) x *= g;
    }
    s->levelDb = juce::Decibels::gainToDecibels (0.7f);
    return s;
}
} // namespace kitloader
