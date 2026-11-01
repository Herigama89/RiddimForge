#include "Generator.h"
#include "Dsp.h"
#include "Riddim.h"
#include <cstring>
#include <map>

namespace rf
{
const char* genPartName (int p)
{
    static const char* n[kNumGenParts] = { "Drums", "Bass", "Skank Guitar", "Organ", "Piano", "Lead" };
    return p >= 0 && p < kNumGenParts ? n[p] : "";
}

int genPartToRiddimPart (int p)
{
    static const int m[kNumGenParts] = { PartDrums, PartBass, PartSkank, PartOrgan, PartPiano, PartLead };
    return p >= 0 && p < kNumGenParts ? m[p] : PartDrums;
}

namespace
{
const std::vector<std::vector<const char*>>& typeNames()
{
    static const std::vector<std::vector<const char*>> t = {
        { "Follow Riddim Style", "One Drop", "Rockers", "Steppers", "Rocksteady", "Ska", "Lovers Rock", "Nyabinghi", "Dancehall Dembow" },
        { "Roots One Drop", "Steppers Pulse", "Rockers Drive", "Walking Ska", "Dub Sparse", "Digital Dancehall", "Lovers Melodic" },
        { "Skank 2 & 4", "Double Chop", "Offbeat Ska", "Muted Scratch", "Rockers Chop" },
        { "Classic Bubble", "Double Bubble", "Shuffle Bubble", "Stabs 2 & 4", "Sustain Hold" },
        { "Skank Double", "Offbeat 8ths", "Rocksteady Comp", "Lovers Arpeggio", "Stabs" },
        { "Melodica Line", "Horn Riff", "Call & Response", "Sparse Hook" },
    };
    return t;
}

float velChar (char c)
{
    switch (c) { case 'X': return 1.0f; case 'x': return 0.78f; case 'o': return 0.6f; case 'g': return 0.36f; default: return 0.0f; }
}

bool isHat (int n) { return n == 42 || n == 44 || n == 46; }
bool isSnareish (int n) { return n == 37 || n == 38 || n == 40 || n == 39; }

struct Gen
{
    Rng r;
    explicit Gen (uint32_t seed) { r.seed (seed * 2654435761u + 0x9e37u); for (int i = 0; i < 5; ++i) r.next(); }
    float uni() { return r.uni(); }
    bool chance (float p) { return r.uni() < p; }
    int pick (int n) { return std::min (n - 1, (int) (r.uni() * (float) n)); }
    template <class T> const T& choose (const std::vector<T>& v) { return v[(size_t) pick ((int) v.size())]; }
    int weighted (const std::vector<float>& w)
    {
        float t = 0; for (float x : w) t += x;
        float u = r.uni() * t;
        for (size_t i = 0; i < w.size(); ++i) { if (u < w[i]) return (int) i; u -= w[i]; }
        return (int) w.size() - 1;
    }
};

//==============================================================================
// DRUMS
//==============================================================================
using Bar = std::vector<GenEvent>;  // steps 0..15

Bar drumBarFromLanes (const std::vector<StyleLaneInfo>& lanes, int barInPattern)
{
    Bar b;
    for (const auto& l : lanes)
    {
        const size_t len = l.pattern.size();
        for (int s = 0; s < 16; ++s)
        {
            const char c = l.pattern[(size_t) ((barInPattern * 16 + s) % (int) len)];
            const float v = velChar (c);
            if (v > 0) b.push_back ({ s, GKDrum, l.note, 0, v, 1.0f, 0.0f });
        }
    }
    return b;
}

bool hasAt (const Bar& b, int step, int note)
{
    for (auto& e : b) if (e.step == step && e.note == note) return true;
    return false;
}

void mutateDrumBar (Bar& b, Gen& g, float cx, int style, bool sparse)
{
    // hats: ghost 16ths, thin out at low complexity, occasional open hat
    bool hasHats = false;
    for (auto& e : b) hasHats |= isHat (e.note);
    if (hasHats)
    {
        for (int s = 1; s < 16; s += 2)
            if (! hasAt (b, s, 42) && g.chance (0.28f * cx)) b.push_back ({ s, GKDrum, 42, 0, 0.25f + 0.15f * g.uni(), 1, 0 });
        if (cx < 0.35f)
            b.erase (std::remove_if (b.begin(), b.end(), [&] (const GenEvent& e) { return e.note == 42 && e.vel < 0.4f && g.chance (0.5f); }), b.end());
        if (g.chance (0.35f * cx))
        {
            int s = g.chance (0.5f) ? 14 : 6;
            for (auto& e : b) if (e.step == s && e.note == 42) e.note = 46;
            if (! hasAt (b, s, 46)) b.push_back ({ s, GKDrum, 46, 0, 0.6f, 1, 0 });
        }
    }
    // kick pickups (one drop / lovers / rocksteady feel)
    if (! sparse && g.chance (0.35f * cx))
    {
        int s = g.choose (std::vector<int> { 15, 11, 7 });
        if (! hasAt (b, s, 36)) b.push_back ({ s, GKDrum, 36, 0, 0.55f + 0.2f * g.uni(), 1, 0 });
    }
    // snare ghosts
    int ghosts = 0;
    for (int s = 1; s < 16 && ghosts < 3; s += 2)
        if (g.chance (0.11f * cx))
        {
            bool busy = false;
            for (auto& e : b) busy |= (e.step == s && isSnareish (e.note));
            if (! busy) { b.push_back ({ s, GKDrum, 38, 0, 0.16f + 0.12f * g.uni(), 1, 0 }); ++ghosts; }
        }
    // percussion colour at higher complexity
    if (cx > 0.55f && g.chance (cx - 0.35f) && style != 6)
    {
        if (! hasAt (b, 4, 54)) b.push_back ({ 4, GKDrum, 54, 0, 0.55f, 1, 0 });
        if (! hasAt (b, 12, 54)) b.push_back ({ 12, GKDrum, 54, 0, 0.55f, 1, 0 });
    }
    if (cx > 0.6f && g.chance (0.25f * cx) && style != 6)
        b.push_back ({ g.chance (0.5f) ? 14 : 10, GKDrum, g.chance (0.5f) ? 65 : 66, 0, 0.6f, 1, 0 });
    // velocity breathing
    for (auto& e : b) e.vel = clampf (e.vel * (0.92f + 0.16f * g.uni()), 0.05f, 1.0f);
}

void writeFill (Bar& b, Gen& g, float intensity, int style, int variant)
{
    const int len = intensity < 0.3f ? 4 : (intensity < 0.6f ? 8 : (intensity < 0.9f ? 12 : 16));
    const int from = 16 - len;
    // clear the fill region of kit hits (the drummer leaves the groove)
    b.erase (std::remove_if (b.begin(), b.end(), [&] (const GenEvent& e) {
        return e.step >= from && (isHat (e.note) || isSnareish (e.note) || e.note == 36 || e.note == 54 || e.note == 86 || e.note == 87);
    }), b.end());

    enum { Timbale, Toms, Rotos, SnareRoll, Mixed, Nyabinghi };
    int type;
    if (style == 6) type = Nyabinghi;
    else type = g.weighted ({ 1.2f, 1.2f, 0.8f, 0.8f, 1.0f });
    (void) variant;

    const float density = 0.55f + 0.45f * intensity;
    int idx = 0;
    for (int s = from; s < 16; ++s)
    {
        const bool strong = (s % 2) == 0;
        if (! strong && ! g.chance (density)) continue;
        if (strong && s < 15 && g.chance (0.12f * (1.0f - intensity))) continue;
        const float pos = (float) (s - from) / (float) std::max (1, len - 1);
        const float vel = clampf (0.55f + 0.45f * pos + 0.1f * (g.uni() - 0.5f), 0.3f, 1.0f);
        int note = 38;
        switch (type)
        {
            case Timbale:   note = (idx % 2 == 0) ? 65 : 66; break;
            case Toms:      note = pos < 0.34f ? 48 : (pos < 0.67f ? 45 : 41); break;
            case Rotos:     note = (idx % 2 == 0) ? 93 : 94; break;
            case SnareRoll: note = 38; break;
            case Mixed:     note = (idx % 3 == 0) ? 38 : (pos < 0.5f ? 48 : 41); break;
            case Nyabinghi: note = (idx % 3 == 2) ? 87 : 86; break;
            default: break;
        }
        b.push_back ({ s, GKDrum, note, 0, vel, 1, 0 });
        // flams / doubles at high intensity
        if (intensity > 0.55f && g.chance (0.25f * intensity))
            b.push_back ({ s, GKDrum, note, 0, vel * 0.6f, 1, 0.5f });
        ++idx;
    }
    // last note: kick + low drum accent
    if (style != 6 && g.chance (0.6f)) b.push_back ({ 15, GKDrum, 36, 0, 0.85f, 1, 0 });
}

void genDrums (const GenSettings& s, Phrase& ph)
{
    Gen g (s.seed[GPDrums]);
    const int type = s.type[GPDrums];
    const int style = type <= 0 ? s.riddimStyle : type - 1;
    const float cx = s.complexity[GPDrums];
    std::vector<StyleLaneInfo> lanes;
    styleDrumLanes (style, lanes);
    const bool sparse = style == 0 || style == 5 || style == 6;

    // two-bar motif, developed across the phrase
    Bar motif[2];
    for (int m = 0; m < 2; ++m) { motif[m] = drumBarFromLanes (lanes, m); mutateDrumBar (motif[m], g, cx, style, sparse); }

    auto isFillBar = [&] (int bar) {
        switch (s.fillEvery)
        {
            case 1: return (bar % 2) == 1;
            case 2: return (bar % 4) == 3 || (s.bars < 4 && bar == s.bars - 1);
            case 3: return bar == s.bars - 1;
            default: return false;
        }
    };

    int fillCount = 0;
    for (int bar = 0; bar < s.bars; ++bar)
    {
        Bar b = motif[bar % 2];
        if (bar >= 2 && g.chance (0.5f * cx)) mutateDrumBar (b, g, cx * 0.5f, style, sparse); // development
        if (isFillBar (bar) && s.fillEvery > 0)
        {
            // fills grow towards the end of the phrase
            const float grow = bar == s.bars - 1 ? 0.25f : 0.0f;
            writeFill (b, g, clampf (s.fillIntensity + grow * s.fillIntensity, 0.0f, 1.0f), style, fillCount++);
        }
        const bool afterFill = s.fillEvery > 0 && isFillBar ((bar + s.bars - 1) % s.bars) && s.bars > 1;
        if (afterFill && style != 6 && g.chance (0.85f))
        {
            b.push_back ({ 0, GKDrum, 49, 0, 0.85f, 1, 0 });
            if (! hasAt (b, 0, 36)) b.push_back ({ 0, GKDrum, 36, 0, 0.9f, 1, 0 });
        }
        for (auto e : b) { e.step += bar * 16; ph.events[GPDrums].push_back (e); }
    }
}

//==============================================================================
// BASS
//==============================================================================
void genBass (const GenSettings& s, Phrase& ph)
{
    Gen g (s.seed[GPBass]);
    const int type = s.type[GPBass];
    const float cx = s.complexity[GPBass];
    static const std::vector<std::vector<std::string>> rhythms = {
        { "x--.x.x-..x-x-..", "x---..x.x--.x-..", "x-.x..x-..x-x-..", "x--...x-.xx-..x." },  // roots one drop
        { "x.x.x.x.x.x.x.x.", "x-x-x-x-x-x-x.x.", "x.x.x.x.x.x.xxx." },                     // steppers
        { "x-.xx-.xx-.xx-.x", "x-.x.xx-x-.x.xx.", "x-.xx-.x.xx-x-.x" },                     // rockers
        { "x---x---x---x---", "x---x---x---x-x-" },                                         // walking ska
        { "x-------......x-", "x-----......x---", "x-------..x-----" },                     // dub sparse
        { "x..x..x.x..x..x.", "x..x..x...x.x...", "x..x..x.x..x.x.." },                     // digital
        { "x---..x.x---..x.", "x--.x-x.x--.x.x.", "x---..x.x-x-..x." },                     // lovers
    };
    const auto& set = rhythms[(size_t) std::clamp (type, 0, (int) rhythms.size() - 1)];
    std::string motifR[2] = { g.choose (set), g.choose (set) };
    // complexity: add syncopated pickups / remove notes
    for (auto& rr : motifR)
        for (int i = 1; i < 16; ++i)
        {
            if (rr[(size_t) i] == '.' && rr[(size_t) i - 1] != 'x' && g.chance (0.12f * cx)) rr[(size_t) i] = 'x';
            else if (rr[(size_t) i] == 'x' && i > 2 && cx < 0.3f && g.chance (0.2f)) rr[(size_t) i] = '.';
        }

    auto tokenFor = [&] (int idxInBar, int step, bool lastInBar) -> std::pair<int, int>
    {
        if (type == 3) // walking: chord tones per beat, approach on beat 4
        {
            static const int walk[2][4] = { { BTRoot, BTThird, BTFifth, BTSixth }, { BTRoot, BTFifth, BTOctave, BTFifth } };
            const int beat = step / 4;
            if (beat == 3 && g.chance (0.4f + 0.4f * cx)) return { BTApproachBelow, 0 };
            return { walk[g.chance (0.5f) ? 0 : 1][beat], 0 };
        }
        if (idxInBar == 0) return { g.chance (0.15f) ? BTOctave : BTRoot, 0 };
        if (lastInBar && step >= 12 && g.chance (0.25f + 0.35f * cx))
            return { g.chance (0.7f) ? BTApproachBelow : BTApproachAbove, 0 };
        const float nr = 0.3f + 0.6f * cx;
        int t = g.weighted ({ 1.0f, 0.35f * nr, 0.8f * nr, 0.5f * nr, 0.3f * nr, 0.2f * nr, 0, 0, 0.15f * nr, 0.1f * nr });
        return { t, 0 };
    };

    // build two motif bars with notes, then repeat; every 4th bar varies its ending
    std::vector<GenEvent> motif[2];
    for (int m = 0; m < 2; ++m)
    {
        const auto& rr = motifR[m];
        int lastHit = -1;
        for (int i = 0; i < 16; ++i) if (rr[(size_t) i] == 'x') lastHit = i;
        int idx = 0;
        for (int i = 0; i < 16; ++i)
        {
            if (rr[(size_t) i] != 'x') continue;
            int hold = 1;
            while (i + hold < 16 && rr[(size_t) (i + hold)] == '-') ++hold;
            auto tk = tokenFor (idx, i, i == lastHit);
            const float gate = hold == 1 ? 0.8f : (float) hold * 0.93f;
            motif[m].push_back ({ i, GKBass, tk.first, tk.second, (idx == 0 ? 0.9f : 0.78f) * (0.9f + 0.2f * g.uni()), gate, 0 });
            ++idx;
        }
    }
    for (int bar = 0; bar < s.bars; ++bar)
    {
        auto b = motif[bar % 2];
        if ((bar % 4) == 3 && ! b.empty())
        {
            // turnaround: rewrite notes in the last beat as a run
            static const int runUp[] = { BTFifth, BTSixth, BTSeventh, BTOctave };
            static const int runDn[] = { BTOctave, BTSeventh, BTFifth, BTApproachBelow };
            const int* run = g.chance (0.5f) ? runUp : runDn;
            b.erase (std::remove_if (b.begin(), b.end(), [] (const GenEvent& e) { return e.step >= 12; }), b.end());
            if (g.chance (0.4f + 0.5f * cx))
                for (int k = 0; k < 4; ++k) if (k % 2 == 0 || cx > 0.5f) b.push_back ({ 12 + k, GKBass, run[k], 0, 0.75f, 0.85f, 0 });
        }
        for (auto e : b) { e.step += bar * 16; ph.events[GPBass].push_back (e); }
    }
}

//==============================================================================
// CHORD PARTS
//==============================================================================
void addPattern (Phrase& ph, int part, int bars, const std::map<int, std::pair<float, float>>& hits, int window, Gen& g,
                 float cx, int ghostWindow = -1)
{
    for (int bar = 0; bar < bars; ++bar)
    {
        for (const auto& [step, vg] : hits)
            ph.events[(size_t) part].push_back ({ bar * 16 + step, GKChord, window, 0, vg.first * (0.92f + 0.12f * g.uni()), vg.second, 0 });
        // anticipations into the next bar
        if (g.chance (0.18f * cx) && ! hits.count (15))
            ph.events[(size_t) part].push_back ({ bar * 16 + 15, GKChord, window, 0, 0.55f, 0.6f, 0 });
        if (ghostWindow > 0)
            for (int st = 1; st < 16; st += 2)
                if (! hits.count (st) && g.chance (0.45f * cx))
                    ph.events[(size_t) part].push_back ({ bar * 16 + st, GKChord, ghostWindow, 0, 0.22f, 0.3f, 0 });
    }
}

void genSkank (const GenSettings& s, Phrase& ph)
{
    Gen g (s.seed[GPSkank]);
    const float cx = s.complexity[GPSkank];
    const int win = cx > 0.6f && g.chance (0.5f) ? 67 : 64;
    switch (s.type[GPSkank])
    {
        case 1:  addPattern (ph, GPSkank, s.bars, { { 4, { 0.9f, 0.6f } }, { 5, { 0.6f, 0.6f } }, { 12, { 0.9f, 0.6f } }, { 13, { 0.6f, 0.6f } } }, win, g, cx); break;
        case 2:  addPattern (ph, GPSkank, s.bars, { { 2, { 0.85f, 1.2f } }, { 6, { 0.85f, 1.2f } }, { 10, { 0.85f, 1.2f } }, { 14, { 0.85f, 1.2f } } }, win, g, cx); break;
        case 3:  addPattern (ph, GPSkank, s.bars, { { 4, { 0.9f, 1.3f } }, { 12, { 0.9f, 1.3f } } }, win, g, cx, win); break;
        case 4:  addPattern (ph, GPSkank, s.bars, { { 4, { 0.9f, 1.2f } }, { 7, { 0.55f, 0.6f } }, { 12, { 0.9f, 1.2f } }, { 15, { 0.55f, 0.6f } } }, win, g, cx); break;
        default: addPattern (ph, GPSkank, s.bars, { { 4, { 0.85f, 1.5f } }, { 12, { 0.85f, 1.5f } } }, win, g, cx); break;
    }
}

void genOrgan (const GenSettings& s, Phrase& ph)
{
    Gen g (s.seed[GPOrgan]);
    const float cx = s.complexity[GPOrgan];
    auto bubble = [&] (const char* pat, float lg, float hg)
    {
        for (int bar = 0; bar < s.bars; ++bar)
            for (int st = 0; st < 16; ++st)
            {
                const char c = pat[st];
                if (c == 'L') ph.events[GPOrgan].push_back ({ bar * 16 + st, GKChord, 55, 0, ((st & 1) ? 0.6f : 0.74f) * (0.9f + 0.15f * g.uni()), lg, 0 });
                if (c == 'H') ph.events[GPOrgan].push_back ({ bar * 16 + st, GKChord, 62, 0, 0.85f, hg, 0 });
                if (c == 'l' && g.chance (0.3f + 0.6f * cx)) ph.events[GPOrgan].push_back ({ bar * 16 + st, GKChord, 55, 0, 0.5f, lg, 0 });
            }
    };
    switch (s.type[GPOrgan])
    {
        case 1: bubble ("lLLLHlLLlLLLHlLL", 0.5f, 1.1f); break;
        case 2: bubble ("..L.H.L...L.H.L.", 0.9f, 1.1f); break;
        case 3: bubble ("....H.......H...", 0.5f, 1.4f); break;
        case 4:
            for (int bar = 0; bar < s.bars; ++bar)
                ph.events[GPOrgan].push_back ({ bar * 16, GKChord, 58, 0, 0.7f, 15.5f, 0 });
            break;
        default: bubble ("..LLH.LL..LLH.LL", 0.55f, 1.1f); break;
    }
}

void genPiano (const GenSettings& s, Phrase& ph)
{
    Gen g (s.seed[GPPiano]);
    const float cx = s.complexity[GPPiano];
    switch (s.type[GPPiano])
    {
        case 1: addPattern (ph, GPPiano, s.bars, { { 2, { 0.75f, 1.2f } }, { 6, { 0.75f, 1.2f } }, { 10, { 0.75f, 1.2f } }, { 14, { 0.75f, 1.2f } } }, 60, g, cx); break;
        case 2: addPattern (ph, GPPiano, s.bars, { { 2, { 0.6f, 1.0f } }, { 4, { 0.85f, 1.2f } }, { 6, { 0.6f, 1.0f } }, { 10, { 0.6f, 1.0f } }, { 12, { 0.85f, 1.2f } }, { 14, { 0.6f, 1.0f } } }, 60, g, cx); break;
        case 3:
        {
            // broken-chord arpeggio in 8ths (lovers rock)
            static const int shapes[3][8] = { { 0, 1, 2, 3, 2, 1, 0, 1 }, { 0, 2, 1, 3, 0, 2, 1, 3 }, { 0, 1, 2, 1, 3, 2, 1, 2 } };
            const int sh = g.pick (3);
            for (int bar = 0; bar < s.bars; ++bar)
                for (int i = 0; i < 8; ++i)
                    if (i != 0 || cx > 0.3f)
                        ph.events[GPPiano].push_back ({ bar * 16 + i * 2, GKChordTone, 60, shapes[sh][i], 0.55f + 0.2f * g.uni(), 1.8f, 0 });
            break;
        }
        case 4:
        {
            std::vector<int> pos = { 0, 3, 6, 10, 12, 14 };
            std::map<int, std::pair<float, float>> h;
            for (int p : pos) if (g.chance (0.35f + 0.4f * cx)) h[p] = { 0.85f, 0.7f };
            if (h.empty()) h[4] = { 0.85f, 0.7f };
            addPattern (ph, GPPiano, s.bars, h, 60, g, cx);
            break;
        }
        default: addPattern (ph, GPPiano, s.bars, { { 4, { 0.8f, 1.2f } }, { 5, { 0.45f, 0.6f } }, { 12, { 0.8f, 1.2f } }, { 13, { 0.45f, 0.6f } } }, 60, g, cx); break;
    }
}

//==============================================================================
// LEAD
//==============================================================================
void genLead (const GenSettings& s, Phrase& ph)
{
    Gen g (s.seed[GPLead]);
    const float cx = s.complexity[GPLead];
    const int type = s.type[GPLead];
    const std::vector<int> scale = s.minor ? std::vector<int> { 0, 3, 5, 7, 10 } : std::vector<int> { 0, 2, 4, 7, 9 };
    auto pitchOf = [&] (int deg) {
        const int n = (int) scale.size();
        const int oct = (deg >= 0) ? deg / n : -((-deg + n - 1) / n);
        const int d = ((deg % n) + n) % n;
        return scale[(size_t) d] + 12 * oct;
    };

    static const std::vector<std::vector<std::string>> rhythms = {
        { "x---..x-x---....", "x-----x.x-x-....", "..x-x---x-------" },          // melodica: long tones
        { "x.x...x.x.x.....", "x.xx..x...x.x...", "..x.x.x...x.x.x." },          // horn riff: punchy
        { "x.x.x---........", "..x-x.x.x---....", "x-x-x.x---......" },          // call
        { "x-------........", "....x---x-------", "x---....x---...." },          // sparse hook
    };
    const auto& set = rhythms[(size_t) std::clamp (type, 0, 3)];

    int deg = g.chance (0.5f) ? 2 : 3;   // start on 5th/4th-ish degree
    auto makeBar = [&] (const std::string& rr, bool resolve, bool answer) {
        std::vector<GenEvent> b;
        int lastHit = -1;
        for (int i = 0; i < 16; ++i) if (rr[(size_t) i] == 'x') lastHit = i;
        for (int i = 0; i < 16; ++i)
        {
            if (rr[(size_t) i] != 'x') continue;
            int hold = 1;
            while (i + hold < 16 && rr[(size_t) (i + hold)] == '-') ++hold;
            int step = g.weighted ({ 0.25f, 1.0f, 0.9f, 0.35f * (0.5f + cx), 0.2f * cx });
            int dir = g.chance (answer ? 0.6f : 0.45f) ? -1 : 1;
            deg += dir * step;
            deg = std::clamp (deg, -2, 8);
            if (resolve && i == lastHit) deg = g.chance (0.6f) ? 0 : 5;
            b.push_back ({ i, GKLead, pitchOf (deg), 0, 0.7f + 0.25f * g.uni(), hold == 1 ? 0.8f : (float) hold * 0.95f, 0 });
        }
        return b;
    };

    std::vector<GenEvent> riff;
    for (int bar = 0; bar < s.bars; ++bar)
    {
        std::vector<GenEvent> b;
        const int inGroup = bar % 4;
        switch (type)
        {
            case 1: { if (bar == 0 || inGroup == 0) { int d = deg; riff = makeBar (g.choose (set), false, false); deg = d; }
                      b = riff; if (inGroup == 3 && g.chance (0.5f + 0.3f * cx)) b = makeBar (g.choose (set), true, true); break; }
            case 2: if (inGroup == 0) b = makeBar (g.choose (set), false, false);
                    else if (inGroup == 2) b = makeBar (g.choose (set), true, true);
                    break;
            case 3: if (inGroup % 2 == 0) b = makeBar (g.choose (set), inGroup == 2, false); break;
            default: if (inGroup != 3 || g.chance (0.3f * cx)) b = makeBar (g.choose (set), inGroup == 3 || inGroup == 1, inGroup >= 2); break;
        }
        for (auto e : b) { e.step += bar * 16; ph.events[GPLead].push_back (e); }
    }
}
} // namespace

//==============================================================================
int numGenTypes (int part) { return part >= 0 && part < kNumGenParts ? (int) typeNames()[(size_t) part].size() : 0; }
const char* genTypeName (int part, int type)
{
    if (part < 0 || part >= kNumGenParts) return "";
    const auto& v = typeNames()[(size_t) part];
    return v[(size_t) std::clamp (type, 0, (int) v.size() - 1)];
}

void Phrase::index()
{
    for (int p = 0; p < kNumGenParts; ++p)
    {
        byStep[(size_t) p].assign ((size_t) steps(), {});
        auto& ev = events[(size_t) p];
        std::sort (ev.begin(), ev.end(), [] (const GenEvent& a, const GenEvent& b) { return a.step < b.step; });
        for (size_t i = 0; i < ev.size(); ++i)
            if (ev[i].step >= 0 && ev[i].step < steps()) byStep[(size_t) p][(size_t) ev[i].step].push_back ((int) i);
    }
}

void generatePart (int part, const GenSettings& s, Phrase& out)
{
    out.bars = std::clamp (s.bars, 1, 16);
    out.events[(size_t) part].clear();
    GenSettings ss = s; ss.bars = out.bars;
    switch (part)
    {
        case GPDrums: genDrums (ss, out); break;
        case GPBass:  genBass (ss, out); break;
        case GPSkank: genSkank (ss, out); break;
        case GPOrgan: genOrgan (ss, out); break;
        case GPPiano: genPiano (ss, out); break;
        case GPLead:  genLead (ss, out); break;
        default: break;
    }
    out.has[(size_t) part] = true;
    out.index();
}

void generateAll (const GenSettings& s, Phrase& out)
{
    for (int p = 0; p < kNumGenParts; ++p) generatePart (p, s, out);
}

} // namespace rf
