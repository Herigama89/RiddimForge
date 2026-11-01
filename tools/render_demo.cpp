// Offline renderer: bounces riddim demos and a sound audition to WAV, and prints levels/CPU.
// Build: g++ -O2 -std=c++17 -I../Source/engine render_demo.cpp ../Source/engine/*.cpp -o render_demo
#include "Engine.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

using namespace rf;

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) std::lround (clampf (L[i], -1, 1) * 32767.0f);
        int16_t b = (int16_t) std::lround (clampf (R[i], -1, 1) * 32767.0f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

struct Stats { float peak = 0; double sumSq = 0; size_t n = 0; bool bad = false; };

static void renderBlocks (Engine& e, int sr, int numSamples, std::vector<float>& L, std::vector<float>& R,
                          std::vector<MidiEvent> events, Stats& st, double& cpuSec)
{
    const int block = 256;
    std::vector<float> bl (block), br (block);
    std::sort (events.begin(), events.end(), [] (auto& a, auto& b) { return a.sample < b.sample; });
    size_t ei = 0;
    Transport t; t.hostPlaying = false; t.hasPpq = false; t.bpm = e.params.freeBpm;
    for (int pos = 0; pos < numSamples; pos += block)
    {
        int n = std::min (block, numSamples - pos);
        std::vector<MidiEvent> local;
        while (ei < events.size() && events[ei].sample < pos + n) { auto m = events[ei++]; m.sample -= pos; local.push_back (m); }
        auto t0 = std::chrono::high_resolution_clock::now();
        e.process (bl.data(), br.data(), n, local.data(), (int) local.size(), t);
        auto t1 = std::chrono::high_resolution_clock::now();
        cpuSec += std::chrono::duration<double> (t1 - t0).count();
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (bl[i]) || ! std::isfinite (br[i])) st.bad = true;
            L.push_back (bl[i]); R.push_back (br[i]);
            st.peak = std::max (st.peak, std::max (std::fabs (bl[i]), std::fabs (br[i])));
            st.sumSq += (double) bl[i] * bl[i] + (double) br[i] * br[i]; st.n += 2;
        }
    }
    (void) sr;
}

int main (int argc, char** argv)
{
#if defined(__x86_64__) || defined(_M_X64)
    _mm_setcsr (_mm_getcsr() | 0x8040); // flush denormals
#endif
    const std::string outDir = argc > 1 ? argv[1] : ".";
    const int sr = 44100;

    // ---------- 1. every style, 8 bars each, one continuous showcase -----------
    {
        std::vector<float> L, R;
        struct Demo { int style, key, prog; float bpm; int organ, bass, lead; bool pad; };
        const Demo demos[] = {
            { 0, 9, 5, 74, 0, 0, 0, false },   // One Drop  Am-Dm
            { 1, 7, 0, 78, 0, 0, 0, false },   // Rockers   G-C
            { 2, 2, 6, 76, 0, 1, 0, false },   // Steppers  Dm-C
            { 3, 0, 2, 72, 1, 3, 0, false },   // Rocksteady C-Am-F-G
            { 4, 5, 1, 112, 1, 3, 0, false },  // Ska F-Bb-C-Bb
            { 5, 10, 2, 76, 0, 0, 0, true },   // Lovers Rock Bb
            { 6, 4, 9, 70, 0, 0, 0, false },   // Nyabinghi Em
            { 7, 9, 9, 92, 0, 2, 0, false },   // Dembow Am digital
        };
        double cpu = 0; Stats total;
        for (const auto& d : demos)
        {
            Engine e;
            e.params.riddimOn = true; e.params.runMode = 1; e.params.freeBpm = d.bpm;
            e.params.riddim.style = d.style; e.params.riddim.key = d.key; e.params.riddim.progression = d.prog;
            e.params.riddim.partOn[PartPad] = d.pad;
            e.params.organType = d.organ; e.params.bassType = d.bass;
            e.prepare (sr, 512);
            const int bars = 8;
            const int n = (int) (bars * 4 * 60.0 / d.bpm * sr);
            Stats st; double c = 0;
            renderBlocks (e, sr, n, L, R, {}, st, c);
            cpu += c;
            std::printf ("%-18s peak %.3f  rms %.3f  %s  cpu %.1f%% realtime\n", styleName (d.style), st.peak,
                         std::sqrt (st.sumSq / (double) st.n), st.bad ? "NaN!" : "ok", 100.0 * c / ((double) n / sr));
            total.peak = std::max (total.peak, st.peak);
        }
        writeWav (outDir + "/RiddimForge_Styles_Demo.wav", L, R, sr);
        std::printf ("styles demo: %.1f s, cpu total %.2f s\n", (double) L.size() / sr, cpu);
    }

    // ---------- 2. one drop with a melodica lead line played over it -----------
    {
        Engine e;
        e.params.riddimOn = true; e.params.runMode = 1; e.params.freeBpm = 74;
        e.params.riddim.style = 0; e.params.riddim.key = 9; e.params.riddim.progression = 5;
        e.params.ch1Target = TargetLead; e.params.leadType = 0;
        e.prepare (sr, 512);
        const double spq = 60.0 / 74 * sr;
        std::vector<MidiEvent> ev;
        // melody in A minor (quarter-note units from bar 2)
        const int mel[][3] = { { 76, 8, 2 }, { 74, 10, 1 }, { 72, 11, 1 }, { 69, 12, 3 }, { 72, 16, 2 }, { 74, 18, 1 },
                               { 77, 19, 1 }, { 76, 20, 4 }, { 81, 24, 2 }, { 79, 26, 1 }, { 76, 27, 1 }, { 74, 28, 2 },
                               { 72, 30, 1 }, { 74, 31, 1 }, { 69, 32, 4 } };
        for (auto& m : mel)
        {
            ev.push_back ({ (int) (m[1] * spq), 0x90, (uint8_t) m[0], 100 });
            ev.push_back ({ (int) ((m[1] + m[2]) * spq - 400), 0x80, (uint8_t) m[0], 0 });
        }
        ev.push_back ({ (int) (35.5 * spq), 0x99, 90, 110 }); // dub siren on ch10
        std::vector<float> L, R; Stats st; double c = 0;
        renderBlocks (e, sr, (int) (40 * spq), L, R, ev, st, c);
        writeWav (outDir + "/RiddimForge_OneDrop_Melodica.wav", L, R, sr);
        std::printf ("melodica demo peak %.3f %s\n", st.peak, st.bad ? "NaN!" : "ok");
    }

    // ---------- 3. audition: every drum/perc sound, then each instrument -------
    {
        Engine e;
        e.params.riddimOn = false;
        e.prepare (sr, 512);
        std::vector<MidiEvent> ev;
        int pos = sr / 4;
        for (int note = 35; note <= 92; ++note)
        {
            if (drumMap (note).type < 0) continue;
            ev.push_back ({ pos, 0x99, (uint8_t) note, 110 });
            pos += (note >= 90 ? sr * 2 : (note == 49 || note == 51 || note == 52 || note == 57) ? sr : sr / 2);
        }
        // instruments: ch2 skank .. ch7 pad, play an Am chord then a C chord
        for (int ch = 1; ch <= 6; ++ch)
        {
            pos += sr / 2;
            const int base = ch == 4 ? 45 - 12 : (ch == 6 ? 57 : 57);
            int notes[3] = { base, base + 3, base + 7 };
            int count = ch == 4 ? 1 : 3;
            for (int rep = 0; rep < 2; ++rep)
            {
                for (int i = 0; i < count; ++i) ev.push_back ({ pos, (uint8_t) (0x90 | ch), (uint8_t) (notes[i] + rep * 3), 100 });
                int len = ch == 1 ? sr / 6 : (int) (sr * 0.9);
                for (int i = 0; i < count; ++i) ev.push_back ({ pos + len, (uint8_t) (0x80 | ch), (uint8_t) (notes[i] + rep * 3), 0 });
                pos += sr;
            }
        }
        std::vector<float> L, R; Stats st; double c = 0;
        renderBlocks (e, sr, pos + sr * 2, L, R, ev, st, c);
        writeWav (outDir + "/RiddimForge_Sound_Audition.wav", L, R, sr);
        std::printf ("audition peak %.3f %s length %.1fs\n", st.peak, st.bad ? "NaN!" : "ok", (double) L.size() / sr);
    }

    // ---------- 4. chord detection sanity ---------------------------------------
    {
        std::array<bool, 128> h {}; Chord c;
        h[57] = h[60] = h[64] = true; detectChord (h, c); std::printf ("A C E -> %s\n", chordName (c).c_str());
        h.fill (false); h[55] = h[59] = h[62] = h[65] = true; detectChord (h, c); std::printf ("G B D F -> %s\n", chordName (c).c_str());
        h.fill (false); h[52] = h[60] = h[67] = true; detectChord (h, c); std::printf ("E C G -> %s\n", chordName (c).c_str());
        h.fill (false); h[50] = h[53] = h[57] = h[60] = true; detectChord (h, c); std::printf ("D F A C -> %s\n", chordName (c).c_str());
    }
    return 0;
}
