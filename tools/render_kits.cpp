// Offline test: loads raw-exported kits (see kit_raw.py), generates 16-bar phrases and renders them.
#include "Engine.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <xmmintrin.h>
using namespace rf;

static std::vector<std::unique_ptr<Kit>> loadKits (const std::string& dir)
{
    std::vector<std::unique_ptr<Kit>> kits;
    std::ifstream in (dir + "/manifest.txt");
    std::string line;
    while (std::getline (in, line))
    {
        std::istringstream ss (line);
        std::string tag; ss >> tag;
        if (tag == "KIT") { kits.push_back (std::make_unique<Kit>()); std::getline (ss, kits.back()->name); }
        else if (tag == "ART")
        {
            KitArtic a; int nn, nr; ss >> a.key >> a.choke >> nn;
            for (int i = 0; i < nn; ++i) { int n; ss >> n; a.notes.push_back (n); }
            ss >> nr;
            for (int i = 0; i < nr; ++i) { std::string r; ss >> r; auto c = r.find (','); a.routeBus.push_back (std::stoi (r.substr (0, c))); a.routeMode.push_back (std::stoi (r.substr (c + 1))); }
            kits.back()->artics.push_back (a);
        }
        else if (tag == "SMP")
        {
            std::string fn; KitSample s; int ch, len; ss >> fn >> s.levelDb >> ch >> len;
            s.length = len; s.sampleRate = 48000;
            std::ifstream f (dir + "/" + fn, std::ios::binary);
            for (int c = 0; c < ch; ++c) { std::vector<float> d ((size_t) len); f.read ((char*) d.data(), len * 4); s.ch.push_back (std::move (d)); }
            kits.back()->artics.back().samples.push_back (std::move (s));
        }
    }
    for (auto& k : kits) k->buildIndex();
    if (kits.size() > 0) kits[0]->outputGainDb = 5.0f;   // matches kits.json
    if (kits.size() > 1) kits[1]->outputGainDb = 6.0f;
    return kits;
}

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    const uint32_t n = (uint32_t) L.size(), bytes = n * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (bytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) std::lround (clampf (L[i], -1, 1) * 32767), b = (int16_t) std::lround (clampf (R[i], -1, 1) * 32767);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    _mm_setcsr (_mm_getcsr() | 0x8040);
    const std::string dir = argv[1], out = argv[2];
    auto kits = loadKits (dir);
    std::printf ("loaded %zu kits\n", kits.size());
    for (auto& k : kits) { size_t n = 0; for (auto& a : k->artics) n += a.samples.size(); std::printf ("  %s: %zu articulations, %zu samples\n", k->name.c_str(), k->artics.size(), n); }

    const int sr = 44100;  // deliberately != 48k to test resampling
    struct Demo { const char* file; int kit; int style; int key; int prog; float bpm; int bassType; int skankType; int organType; int pianoType; int leadType; uint32_t seed; float cx; };
    const Demo demos[] = {
        { "Gen_StudioKit_OneDrop_16bars.wav", 1, 0, 9, 5, 70, 0, 0, 0, 0, 0, 11, 0.55f },
        { "Gen_Kit2_Steppers_16bars.wav",     2, 2, 2, 6, 74, 1, 1, 1, 1, 1, 23, 0.6f },
        { "Gen_StudioKit_Rockers_16bars.wav", 1, 1, 7, 3, 76, 2, 3, 2, 3, 2, 37, 0.7f },
    };
    for (const auto& d : demos)
    {
        Engine e;
        std::vector<const Kit*> kp; for (auto& k : kits) kp.push_back (k.get());
        e.setKits (kp);
        e.params.drumKit = d.kit;
        e.params.riddimOn = true; e.params.runMode = 1; e.params.freeBpm = d.bpm;
        e.params.riddim.style = d.style; e.params.riddim.key = d.key; e.params.riddim.progression = d.prog;
        e.params.riddim.partOn[PartLead] = true;
        e.params.leadType = 0;

        GenSettings gs; gs.bars = 16; gs.riddimStyle = d.style; gs.fillEvery = 2; gs.fillIntensity = 0.6f;
        gs.minor = progressionChord (e.params.riddim, 0).quality == QMin;
        gs.type = { 0, d.bassType, d.skankType, d.organType, d.pianoType, d.leadType };
        for (int p = 0; p < kNumGenParts; ++p) { gs.seed[(size_t) p] = d.seed + (uint32_t) p * 7; gs.complexity[(size_t) p] = d.cx; }
        Phrase ph; generateAll (gs, ph);
        e.params.riddim.phrase = &ph;
        e.params.riddim.genUse.fill (true);
        size_t cnt = 0; for (auto& v : ph.events) cnt += v.size();

        e.prepare (sr, 512);
        const int n = (int) (16 * 4 * 60.0 / d.bpm * sr) + sr * 2;
        std::vector<float> L, R, bl (512), br (512);
        Transport t; t.bpm = d.bpm;
        float peak = 0; double cpu = 0; bool bad = false;
        for (int pos = 0; pos < n; pos += 512)
        {
            auto t0 = std::chrono::high_resolution_clock::now();
            e.process (bl.data(), br.data(), 512, nullptr, 0, t);
            cpu += std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
            for (int i = 0; i < 512; ++i) { L.push_back (bl[(size_t) i]); R.push_back (br[(size_t) i]); peak = std::max (peak, std::max (std::fabs (bl[(size_t) i]), std::fabs (br[(size_t) i]))); bad |= ! std::isfinite (bl[(size_t) i]); }
        }
        writeWav (out + "/" + d.file, L, R, sr);
        std::printf ("%-36s events %4zu  peak %.3f  cpu %.1f%%  %s\n", d.file, cnt, peak, 100.0 * cpu / ((double) n / sr), bad ? "NaN!" : "ok");
    }

    // audition every articulation of each kit through the mic mixer
    for (int ki = 1; ki <= (int) kits.size(); ++ki)
    {
        Engine e; std::vector<const Kit*> kp; for (auto& k : kits) kp.push_back (k.get()); e.setKits (kp);
        e.params.drumKit = ki; e.params.riddimOn = false; e.prepare (sr, 512);
        std::vector<MidiEvent> ev; int pos = sr / 4;
        for (auto& a : kits[(size_t) ki - 1]->artics)
        {
            if (a.notes.empty()) continue;
            for (float v : { 0.45f, 0.8f, 1.0f }) { ev.push_back ({ pos, 0x99, (uint8_t) a.notes[0], (uint8_t) (v * 127) }); pos += sr * 0.35; }
            pos += sr * 0.5;
        }
        std::vector<float> L, R, bl (512), br (512); Transport t; size_t ei = 0;
        for (int p = 0; p < pos + sr * 2; p += 512)
        {
            std::vector<MidiEvent> loc;
            while (ei < ev.size() && ev[ei].sample < p + 512) { auto m = ev[ei++]; m.sample -= p; loc.push_back (m); }
            e.process (bl.data(), br.data(), 512, loc.data(), (int) loc.size(), t);
            for (int i = 0; i < 512; ++i) { L.push_back (bl[(size_t) i]); R.push_back (br[(size_t) i]); }
        }
        writeWav (out + "/Audition_" + std::to_string (ki) + ".wav", L, R, sr);
    }
    return 0;
}
