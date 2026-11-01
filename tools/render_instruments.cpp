// Offline test for the recorded instruments: tuning check + riddim renders
#include "Engine.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <xmmintrin.h>
using namespace rf;
static std::vector<std::unique_ptr<InstrumentSet>> loadSets (const std::string& dir)
{
    std::vector<std::unique_ptr<InstrumentSet>> sets; std::ifstream in (dir + "/manifest.txt"); std::string line;
    while (std::getline (in, line))
    {
        std::istringstream ss (line); std::string tag; ss >> tag;
        if (tag == "SET") { auto s = std::make_unique<InstrumentSet>(); int ch; ss >> s->key >> s->part >> ch >> s->trimDb; s->chords = ch != 0; std::getline (ss, s->name); sets.push_back (std::move (s)); }
        else if (tag == "SMP")
        {
            std::string fn; InstSample s; int ch, len; ss >> fn >> ch >> len >> s.midi >> s.cents >> s.root >> s.quality >> s.levelDb;
            s.length = len; s.sampleRate = 48000; std::ifstream f (dir + "/" + fn, std::ios::binary);
            for (int c = 0; c < ch; ++c) { std::vector<float> d ((size_t) len + 1, 0.0f); f.read ((char*) d.data(), len * 4); s.ch.push_back (std::move (d)); }
            sets.back()->samples.push_back (std::move (s));
        }
    }
    for (auto& s : sets) s->finalize();
    return sets;
}
static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb"); const uint32_t n = (uint32_t) L.size(), bytes = n * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); }; auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f); w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (bytes);
    for (uint32_t i = 0; i < n; ++i) { int16_t a = (int16_t) std::lround (clampf (L[i], -1, 1) * 32767), b = (int16_t) std::lround (clampf (R[i], -1, 1) * 32767); std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f); }
    std::fclose (f);
}
int main (int, char** argv)
{
    _mm_setcsr (_mm_getcsr() | 0x8040);
    auto sets = loadSets (argv[1]);
    const std::string out = argv[2];
    const int sr = 44100;
    // ---- tuning: render single notes from each note set and dump raw for pitch analysis
    {
        FILE* f = std::fopen ((out + "/tuning.f32").c_str(), "wb"); FILE* idx = std::fopen ((out + "/tuning.txt").c_str(), "w");
        for (auto& s : sets)
        {
            if (s->chords) continue;
            int lo = s->part == "bass" ? 28 : 55, hi = s->part == "bass" ? 50 : 79;
            for (int note = lo; note <= hi; note += 3)
            {
                SampledInstrument inst (true); inst.prepare ((float) sr); inst.setSet (s.get());
                inst.noteOn (note, 0.9f, -1, 0);
                std::vector<float> L ((size_t) sr / 2), R ((size_t) sr / 2);
                inst.render (L.data(), R.data(), sr / 2);
                std::fwrite (L.data(), 4, L.size(), f); std::fprintf (idx, "%s %d\n", s->key.c_str(), note);
            }
        }
        std::fclose (f); std::fclose (idx);
    }
    auto findSet = [&] (const char* k) { for (auto& s : sets) if (s->key == k) return s.get(); return (InstrumentSet*) nullptr; };
    struct Demo { const char* file; int style; int key; int prog; float bpm; const char* bass; const char* skank; const char* organ; const char* piano; const char* lead; uint32_t seed; };
    const Demo demos[] = {
        { "Recorded_Session1_OneDrop_G-F.wav", 0, 7, 0, 70, "bass1", "skanky", "bubble", "piano1", "pluckjc", 5 },
        { "Recorded_Session2_Am-G.wav", 5, 9, 6, 72, "bass2", "clean", "organ4", "piano2", "leadgtr", 9 },
        { "Recorded_Mixed_Steppers_Em-C.wav", 2, 4, 7, 74, "bass1", "chop", "percorg", "piano1", "pluckk", 13 },
    };
    for (const auto& d : demos)
    {
        Engine e;
        e.setPartSets (PartBass, { findSet (d.bass) }); e.setPartSets (PartSkank, { findSet (d.skank) });
        e.setPartSets (PartOrgan, { findSet (d.organ) }); e.setPartSets (PartPiano, { findSet (d.piano) }); e.setPartSets (PartLead, { findSet (d.lead) });
        e.params.bassType = Engine::synthTypeCount (PartBass); e.params.skankType = Engine::synthTypeCount (PartSkank);
        e.params.organType = Engine::synthTypeCount (PartOrgan); e.params.pianoType = Engine::synthTypeCount (PartPiano); e.params.leadType = Engine::synthTypeCount (PartLead);
        e.params.drumKit = 0;
        e.params.riddimOn = true; e.params.runMode = 1; e.params.freeBpm = d.bpm;
        e.params.riddim.style = d.style; e.params.riddim.key = d.key; e.params.riddim.progression = d.prog;
        e.params.riddim.partOn[PartLead] = true;
        GenSettings gs; gs.bars = 8; gs.riddimStyle = d.style; gs.minor = progressionChord (e.params.riddim, 0).quality == QMin;
        for (int p = 0; p < kNumGenParts; ++p) gs.seed[(size_t) p] = d.seed + (uint32_t) p;
        Phrase ph; generatePart (GPLead, gs, ph); generatePart (GPBass, gs, ph);
        e.params.riddim.phrase = &ph; e.params.riddim.genUse[GPLead] = true; e.params.riddim.genUse[GPBass] = true;
        e.prepare (sr, 512);
        const int n = (int) (8 * 4 * 60.0 / d.bpm * sr);
        std::vector<float> L, R, bl (512), br (512); Transport t; t.bpm = d.bpm; float pk = 0; bool bad = false;
        for (int pos = 0; pos < n; pos += 512)
        {
            e.process (bl.data(), br.data(), 512, nullptr, 0, t);
            for (int i = 0; i < 512; ++i) { L.push_back (bl[(size_t) i]); R.push_back (br[(size_t) i]); pk = std::max (pk, std::fabs (bl[(size_t) i])); bad |= ! std::isfinite (bl[(size_t) i]); }
        }
        writeWav (out + "/" + d.file, L, R, sr);
        std::printf ("%-36s peak %.3f %s\n", d.file, pk, bad ? "NaN!" : "ok");
    }
    return 0;
}
