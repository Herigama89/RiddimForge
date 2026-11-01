#include "Riddim.h"
#include "Dsp.h"
#include <cstring>

namespace rf
{
const char* partName (int part)
{
    static const char* names[kNumParts] = { "Drums", "Perc", "Skank", "Piano", "Organ", "Bass", "Lead", "Pad" };
    return part >= 0 && part < kNumParts ? names[part] : "";
}

std::string noteName (int pc)
{
    static const char* n[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return n[((pc % 12) + 12) % 12];
}

std::string chordName (const Chord& c)
{
    static const char* suf[kNumQualities] = { "", "m", "7", "m7", "sus4", "dim", "maj7" };
    return noteName (c.root) + suf[std::clamp (c.quality, 0, (int) kNumQualities - 1)];
}

void chordIntervals (int q, std::vector<int>& out)
{
    out.clear();
    switch (q)
    {
        case QMin:  out = { 0, 3, 7 }; break;
        case QDom7: out = { 0, 4, 7, 10 }; break;
        case QMin7: out = { 0, 3, 7, 10 }; break;
        case QSus4: out = { 0, 5, 7 }; break;
        case QDim:  out = { 0, 3, 6 }; break;
        case QMaj7: out = { 0, 4, 7, 11 }; break;
        default:    out = { 0, 4, 7 }; break;
    }
}

void voiceChord (const Chord& c, int lo, std::vector<int>& out)
{
    std::vector<int> iv;
    chordIntervals (c.quality, iv);
    out.clear();
    for (int i : iv)
    {
        int pc = (c.root + i) % 12;
        out.push_back (lo + ((pc - lo % 12) % 12 + 12) % 12);
    }
    std::sort (out.begin(), out.end());
}

bool detectChord (const std::array<bool, 128>& held, Chord& out)
{
    int mask = 0, lowest = -1, count = 0;
    for (int n = 0; n < 128; ++n)
        if (held[(size_t) n]) { mask |= 1 << (n % 12); if (lowest < 0) lowest = n; ++count; }
    if (count == 0) return false;

    struct T { int q; std::vector<int> iv; };
    static const T temps[] = {
        { QMaj7, { 0, 4, 7, 11 } }, { QDom7, { 0, 4, 7, 10 } }, { QMin7, { 0, 3, 7, 10 } },
        { QMaj, { 0, 4, 7 } }, { QMin, { 0, 3, 7 } }, { QSus4, { 0, 5, 7 } }, { QDim, { 0, 3, 6 } },
        { QMaj, { 0, 4 } }, { QMin, { 0, 3 } }, { QMaj, { 0, 7 } },
    };
    int bestScore = -100;
    Chord best { lowest % 12, QMaj };
    for (int r = 0; r < 12; ++r)
    {
        if (! (mask & (1 << r))) continue;
        for (const auto& t : temps)
        {
            int tm = 0;
            for (int i : t.iv) tm |= 1 << ((r + i) % 12);
            if ((tm & mask) != tm) continue;
            int extras = 0;
            for (int b = 0; b < 12; ++b) if ((mask & ~tm) & (1 << b)) ++extras;
            int score = (int) t.iv.size() * 10 - extras * 6 + (r == lowest % 12 ? 3 : 0);
            if (score > bestScore) { bestScore = score; best = { r, t.q }; }
        }
    }
    out = best;
    return true;
}

//==============================================================================
namespace
{
struct ProgDef { const char* name; std::vector<std::pair<int, int>> chords; };

const std::vector<ProgDef>& progs()
{
    static const std::vector<ProgDef> p = {
        { "I - IV",              { { 0, QMaj }, { 5, QMaj } } },
        { "I - IV - V - IV",     { { 0, QMaj }, { 5, QMaj }, { 7, QMaj }, { 5, QMaj } } },
        { "I - vi - IV - V",     { { 0, QMaj }, { 9, QMin }, { 5, QMaj }, { 7, QMaj } } },
        { "I - V - vi - IV",     { { 0, QMaj }, { 7, QMaj }, { 9, QMin }, { 5, QMaj } } },
        { "I - ii",              { { 0, QMaj }, { 2, QMin } } },
        { "i - iv",              { { 0, QMin }, { 5, QMin } } },
        { "i - VII",             { { 0, QMin }, { 10, QMaj } } },
        { "i - VI - VII - i",    { { 0, QMin }, { 8, QMaj }, { 10, QMaj }, { 0, QMin } } },
        { "i - iv - v - i",      { { 0, QMin }, { 5, QMin }, { 7, QMin }, { 0, QMin } } },
        { "i (one-chord dub)",   { { 0, QMin } } },
        { "I (one-chord)",       { { 0, QMaj } } },
        { "I - IV - I - V",      { { 0, QMaj }, { 5, QMaj }, { 0, QMaj }, { 7, QMaj } } },
        { "i7 - iv7",            { { 0, QMin7 }, { 5, QMin7 } } },
        { "IV - V - iii - vi",   { { 5, QMaj }, { 7, QMaj }, { 4, QMin }, { 9, QMin } } },
    };
    return p;
}

struct Lane { int note; const char* pat; };

struct StyleDef
{
    const char* name;
    std::vector<Lane> lanes;
    const char* skank; float skankGate;
    const char* piano; float pianoGate;
    const char* organ;          // L = low bubble chord, H = high chord
    const char* bass;           // R r 2 3 4 5 6 7 O, '-' = hold
    int fillType;               // 0 timbale/toms, 1 nyabinghi repeater roll
};

const std::vector<StyleDef>& styles()
{
    static const std::vector<StyleDef> s = {
        { "One Drop",
          { { 36, "........X......." }, { 37, "........X......." }, { 42, "x.xgx.xgx.xgx.xg" },
            { 82, "g.x.g.x.g.x.g.x." } },
          "....x.......x...", 1.6f, "....x.......x...", 1.2f, "..LLH.LL..LLH.LL",
          "R-.R..5-..3-R-..R-.R..5-..O-7-5.", 0 },
        { "Rockers",
          { { 36, "X.......X......." }, { 40, "........X......." }, { 42, "x.Xgx.Xgx.Xgx.Xg" },
            { 54, "....x.......x..." }, { 82, "gxgxgxgxgxgxgxgx" } },
          "....x.......x...", 1.4f, "....x.x.....x.x.", 0.9f, "..LLH.LL..LLH.LL",
          "R-.RR-.5R-.RO-5.", 0 },
        { "Steppers",
          { { 36, "X...X...X...X..." }, { 40, "........X......." }, { 42, "x...x...x...x..." },
            { 46, "..o...o...o...o." }, { 82, "gxgxgxgxgxgxgxgx" } },
          "....x.......x...", 1.4f, "................", 1.0f, "..LLH.LL..LLH.LL",
          "R.R.R.R.R.R.5.O.", 0 },
        { "Rocksteady",
          { { 36, "X.......X......." }, { 37, "........X......." }, { 38, "....g.......g..." },
            { 42, "x.x.x.x.x.x.x.x." }, { 62, "....x.......x..x" }, { 63, "......x.x.....x." } },
          "....x.......x...", 1.5f, "..x...x...x...x.", 1.0f, "..LLH.LL..LLH.LL",
          "R--.3--.5--.R--.O--.7--.5--.3--.", 0 },
        { "Ska",
          { { 36, "X.......X......." }, { 38, "....X.......X..." }, { 44, "x...x...x...x..." },
            { 46, "..o...o...o...o." } },
          "..x...x...x...x.", 1.2f, "..x...x...x...x.", 1.0f, "..H...H...H...H.",
          "R---3---5---6---O---6---5---3---", 0 },
        { "Lovers Rock",
          { { 36, "........X......." }, { 37, "........X......." }, { 42, "x.xgx.xgx.xgx.xg" },
            { 62, "..x.......x....." }, { 63, "......x.......x." }, { 82, "g.x.g.x.g.x.g.x." } },
          "....x.......x...", 1.8f, "....x.......x..x", 1.3f, "..LLH.LL..LLH.LL",
          "R---..3.5---..6.O---..7.5---3-..", 0 },
        { "Nyabinghi",
          { { 84, "X.......X......." }, { 85, "X.g.X.g.X.g.X.g." }, { 86, "..x.x..x.xx..x.x" },
            { 87, "......X.......X." }, { 82, "gxgxgxgxgxgxgxgx" } },
          "....x.......x...", 1.4f, "................", 1.0f, "................",
          "R-------.....5--", 1 },
        { "Dancehall Dembow",
          { { 36, "X...X...X...X..." }, { 38, "...x..x....x..x." }, { 42, "x.x.x.x.x.x.x.x." },
            { 82, "..x...x...x...x." } },
          "....x.......x...", 1.2f, "................", 1.0f, "................",
          "R..R..R.R..R..5.", 0 },
    };
    return s;
}

inline float velFor (char c)
{
    switch (c)
    {
        case 'X': return 1.0f;
        case 'x': return 0.78f;
        case 'o': return 0.6f;
        case 'g': return 0.36f;
        default:  return 0.0f;
    }
}

inline int bassInterval (char c, int quality)
{
    const bool minor = quality == QMin || quality == QMin7 || quality == QDim;
    switch (c)
    {
        case 'R': case 'r': return 0;
        case '2': return 2;
        case '3': return minor ? 3 : 4;
        case '4': return 5;
        case '5': return quality == QDim ? 6 : 7;
        case '6': return minor ? 8 : 9;
        case '7': return quality == QMaj7 ? 11 : 10;
        case 'O': return 12;
        default:  return -1;
    }
}

inline char patAt (const char* p, int64_t k)
{
    const size_t len = std::strlen (p);
    return len == 0 ? '.' : p[(size_t) (k % (int64_t) len)];
}
} // namespace

int numStyles() { return (int) styles().size(); }
const char* styleName (int s) { return styles()[(size_t) std::clamp (s, 0, numStyles() - 1)].name; }
int numProgressions() { return (int) progs().size(); }
const char* progressionName (int p) { return progs()[(size_t) std::clamp (p, 0, numProgressions() - 1)].name; }

void styleDrumLanes (int style, std::vector<StyleLaneInfo>& out)
{
    out.clear();
    for (const auto& l : styles()[(size_t) std::clamp (style, 0, numStyles() - 1)].lanes) out.push_back ({ l.note, l.pat });
}
int styleFillType (int style) { return styles()[(size_t) std::clamp (style, 0, numStyles() - 1)].fillType; }

Chord progressionChord (const RiddimSettings& s, int64_t bar)
{
    const auto& p = progs()[(size_t) std::clamp (s.progression, 0, numProgressions() - 1)];
    const int64_t bpc = std::max (1, s.barsPerChord);
    const auto& c = p.chords[(size_t) ((std::max<int64_t> (0, bar) / bpc) % (int64_t) p.chords.size())];
    return { (s.key + c.first) % 12, c.second };
}

void generateStep (int64_t k, const RiddimSettings& s, const Chord& chord, std::vector<SeqEvent>& out)
{
    generateStep (k, s, chord, chord, out);
}

static int bassRootFor (const Chord& c) { return 33 + ((c.root - 9) % 12 + 12) % 12; }  // A1 .. G#2

static int resolveBassToken (int token, int extra, const Chord& chord, const Chord& next)
{
    const bool minor = chord.quality == QMin || chord.quality == QMin7 || chord.quality == QDim;
    const int root = bassRootFor (chord);
    int n = root;
    switch (token)
    {
        case BTThird:   n = root + (minor ? 3 : 4); break;
        case BTFifth:   n = root + (chord.quality == QDim ? 6 : 7); break;
        case BTOctave:  n = root + 12; break;
        case BTSeventh: n = root + (chord.quality == QMaj7 ? 11 : 10); break;
        case BTSixth:   n = root + (minor ? 8 : 9); break;
        case BTSecond:  n = root + 2; break;
        case BTFourth:  n = root + 5; break;
        case BTApproachBelow: { int nr = bassRootFor (next); if (nr - root > 6) nr -= 12; if (root - nr > 6) nr += 12; n = nr - 1; break; }
        case BTApproachAbove: { int nr = bassRootFor (next); if (nr - root > 6) nr -= 12; if (root - nr > 6) nr += 12; n = nr + 2; break; }
        default: break;
    }
    return std::clamp (n + 12 * extra, 28, 60);
}

void generateStep (int64_t k, const RiddimSettings& s, const Chord& chord, const Chord& nextChord, std::vector<SeqEvent>& out)
{
    if (k < 0) return;
    const auto& st = styles()[(size_t) std::clamp (s.style, 0, numStyles() - 1)];
    const int64_t bar = k / 16;
    const int step = (int) (k % 16);

    Rng rng;
    rng.seed ((uint32_t) ((uint64_t) k * 2654435761ull) ^ 0x5bd1e995u);
    const float swingOff = (step & 1) ? clampf (s.swing, 0.0f, 1.0f) * 0.5f : 0.0f;

    auto push = [&] (int part, int note, float vel, float gate, float extraSec)
    {
        SeqEvent e;
        e.part = part; e.note = note;
        e.vel = clampf (vel * (1.0f + s.humanize * 0.16f * rng.bi()), 0.05f, 1.0f);
        e.gateSteps = gate;
        e.offsetSteps = swingOff;
        e.offsetSec = extraSec + s.humanize * 0.007f * rng.uni();
        out.push_back (e);
    };

    const Phrase* ph = s.phrase;
    const int64_t local = ph != nullptr && ph->steps() > 0 ? k % ph->steps() : 0;
    auto usePh = [&] (int gp) { return ph != nullptr && s.genUse[(size_t) gp] && ph->has[(size_t) gp]; };
    auto phEvents = [&] (int gp) -> const std::vector<int>& { return ph->byStep[(size_t) gp][(size_t) local]; };
    auto phPush = [&] (int part, int note, const GenEvent& e, float extraSec)
    {
        SeqEvent ev;
        ev.part = part; ev.note = note;
        ev.vel = clampf (e.vel * (1.0f + s.humanize * 0.16f * rng.bi()), 0.05f, 1.0f);
        ev.gateSteps = e.gate;
        ev.offsetSteps = swingOff + e.offset;
        ev.offsetSec = extraSec + s.humanize * 0.007f * rng.uni();
        out.push_back (ev);
    };
    std::vector<int> v;

    if (usePh (GPDrums))
    {
        for (int i : phEvents (GPDrums))
        {
            const auto& e = ph->events[GPDrums][(size_t) i];
            const int part = e.note >= 60 ? PartPerc : PartDrums;
            if (s.partOn[(size_t) part]) phPush (part, e.note, e, 0.0f);
        }
    }
    else
    {
    // ---- drums & percussion -------------------------------------------------
    const int fillEvery = s.fills == 1 ? 4 : (s.fills == 2 ? 8 : 0);
    const bool inFill = fillEvery > 0 && (bar % fillEvery) == fillEvery - 1 && step >= 12;
    const bool afterFill = fillEvery > 0 && bar > 0 && (bar % fillEvery) == 0 && step == 0;

    for (const auto& lane : st.lanes)
    {
        const int part = lane.note >= 60 ? PartPerc : PartDrums;
        if (! s.partOn[(size_t) part]) continue;
        if (inFill && (lane.note == 37 || lane.note == 38 || lane.note == 40 || lane.note == 86 || lane.note == 87)) continue;
        float lv = velFor (patAt (lane.pat, k));
        if (lv > 0) push (part, lane.note, lv, 1.0f, 0.0f);
    }

    if (inFill)
    {
        const int variant = (int) ((bar / fillEvery) % 2);
        const int i = step - 12;
        if (st.fillType == 1)
        {
            if (s.partOn[PartPerc])
            {
                push (PartPerc, 86, 0.6f + 0.13f * (float) i, 1.0f, 0.0f);
                if (step & 1) push (PartPerc, 87, 0.5f, 1.0f, 0.0f);
            }
        }
        else if (variant == 0)
        {
            static const int notes[4] = { 65, 65, 66, 66 };
            if (s.partOn[PartPerc])
            {
                push (PartPerc, notes[i], 0.65f + 0.11f * (float) i, 1.0f, 0.0f);
                if (step == 15) push (PartPerc, 66, 0.9f, 1.0f, 0.06f);
            }
        }
        else
        {
            static const int notes[4] = { 50, 48, 45, 41 };
            if (s.partOn[PartDrums]) push (PartDrums, notes[i], 0.7f + 0.1f * (float) i, 1.0f, 0.0f);
        }
    }
    if (afterFill && s.partOn[PartDrums] && st.fillType == 0) push (PartDrums, 49, 0.8f, 1.0f, 0.0f);
    }

    // ---- chord-following parts ---------------------------------------------
    auto strum = [&] (int part, const GenEvent& e)
    {
        voiceChord (chord, e.note, v);
        for (size_t n = 0; n < v.size(); ++n)
            phPush (part, v[v.size() - 1 - n], e, 0.0035f * (float) n);
    };

    if (usePh (GPSkank))
    {
        if (s.partOn[PartSkank]) for (int i : phEvents (GPSkank)) strum (PartSkank, ph->events[GPSkank][(size_t) i]);
    }
    else if (s.partOn[PartSkank] && patAt (st.skank, k) == 'x')
    {
        voiceChord (chord, 64, v);
        for (size_t n = 0; n < v.size(); ++n)
        {
            int idx = (int) (v.size() - 1 - n); // upstroke: high string first
            push (PartSkank, v[(size_t) idx], 0.82f, st.skankGate, 0.0035f * (float) n);
        }
    }

    if (usePh (GPPiano))
    {
        if (s.partOn[PartPiano])
            for (int i : phEvents (GPPiano))
            {
                const auto& e = ph->events[GPPiano][(size_t) i];
                voiceChord (chord, e.note, v);
                if (e.kind == GKChordTone)
                {
                    const int sz = (int) v.size();
                    phPush (PartPiano, v[(size_t) (e.extra % sz)] + 12 * (e.extra / sz), e, 0.0f);
                }
                else
                {
                    v.push_back (v.front() + 12);
                    for (int n : v) phPush (PartPiano, n, e, 0.0f);
                }
            }
    }
    else if (s.partOn[PartPiano] && patAt (st.piano, k) == 'x')
    {
        voiceChord (chord, 60, v);
        v.push_back (v.front() + 12);
        for (int n : v) push (PartPiano, n, 0.72f, st.pianoGate, 0.0f);
    }

    if (usePh (GPOrgan))
    {
        if (s.partOn[PartOrgan])
            for (int i : phEvents (GPOrgan))
            {
                const auto& e = ph->events[GPOrgan][(size_t) i];
                voiceChord (chord, e.note, v);
                for (int n : v) phPush (PartOrgan, n, e, 0.0f);
            }
    }
    else if (s.partOn[PartOrgan])
    {
        char c = patAt (st.organ, k);
        if (c == 'L' || c == 'H')
        {
            voiceChord (chord, c == 'L' ? 55 : 62, v);
            const float vel = c == 'H' ? 0.85f : ((step & 1) ? 0.6f : 0.75f);
            for (int n : v) push (PartOrgan, n, vel, c == 'H' ? 1.1f : 0.55f, 0.0f);
        }
    }

    if (usePh (GPBass))
    {
        if (s.partOn[PartBass])
            for (int i : phEvents (GPBass))
            {
                const auto& e = ph->events[GPBass][(size_t) i];
                phPush (PartBass, resolveBassToken (e.note, e.extra, chord, nextChord), e, 0.0f);
            }
    }
    else if (s.partOn[PartBass])
    {
        const char c = patAt (st.bass, k);
        const int iv = bassInterval (c, chord.quality);
        if (iv >= 0)
        {
            const size_t len = std::strlen (st.bass);
            int hold = 1;
            while (hold < 16 && patAt (st.bass, k + hold) == '-') ++hold;
            (void) len;
            const float gate = hold == 1 ? 0.8f : (float) hold * 0.92f;
            int root = 33 + ((chord.root - 9) % 12 + 12) % 12; // A1 .. G#2
            push (PartBass, root + iv, c == 'r' ? 0.55f : 0.88f, gate, 0.0f);
        }
    }

    if (usePh (GPLead) && s.partOn[PartLead])
        for (int i : phEvents (GPLead))
        {
            const auto& e = ph->events[GPLead][(size_t) i];
            int n = 60 + s.key + e.note;
            while (n < 62) n += 12;
            while (n > 86) n -= 12;
            phPush (PartLead, n, e, 0.0f);
        }

    if (s.partOn[PartPad] && step == 0)
    {
        const int bpc = s.chordFromMidi ? 1 : std::max (1, s.barsPerChord);
        if (bar % bpc == 0)
        {
            voiceChord (chord, 57, v);
            for (int n : v) push (PartPad, n, 0.7f, (float) (16 * bpc) - 0.5f, 0.0f);
        }
    }
}

} // namespace rf
