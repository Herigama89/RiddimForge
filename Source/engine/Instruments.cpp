#include "Instruments.h"

namespace rf
{
//==============================================================================
// Polyphonic voice manager with delayed starts (for strums/humanize) and gates
struct VoiceBase
{
    bool busy = false, started = false, held = false, sustained = false;
    int note = -1, gate = -1, delay = 0;
    float vel = 1.0f;
    uint32_t age = 0;
};

template <class V, int N>
class PolyInstrument : public InstrumentBase
{
public:
    void prepare (float sampleRate) override
    {
        ctx.sr = sampleRate;
        sineTable();
        for (auto& v : voices) { v.busy = false; v.prepare (sampleRate); }
        preparePost (sampleRate);
    }

    void noteOn (int note, float velocity, int gateSamples, int delaySamples) override
    {
        V* t = nullptr;
        for (auto& v : voices) if (! v.busy) { t = &v; break; }
        if (t == nullptr)
        {
            uint32_t best = UINT32_MAX;
            for (auto& v : voices) if (! v.held && v.age < best) { best = v.age; t = &v; }
            if (t == nullptr)
                for (auto& v : voices) if (v.age < best) { best = v.age; t = &v; }
        }
        t->busy = true; t->started = false; t->held = true; t->sustained = false;
        t->note = note; t->vel = clampf (velocity, 0.0f, 1.0f);
        t->gate = gateSamples; t->delay = std::max (0, delaySamples);
        t->age = ++counter;
        if (t->delay == 0) { t->begin (ctx); t->started = true; }
    }

    void noteOff (int note) override
    {
        for (auto& v : voices)
        {
            if (! v.busy || v.note != note || ! v.held || v.gate >= 0) continue;
            if (! v.started) { v.busy = false; continue; }
            v.held = false;
            if (pedal) { v.sustained = true; continue; }
            v.end (ctx);
        }
    }

    void setPedal (bool down) override
    {
        pedal = down;
        if (! down)
            for (auto& v : voices)
                if (v.busy && v.sustained) { v.sustained = false; v.end (ctx); }
    }

    void allOff() override
    {
        pedal = false;
        for (auto& v : voices)
        {
            if (! v.busy) continue;
            if (! v.started) { v.busy = false; continue; }
            v.held = false; v.sustained = false; v.gate = -1;
            v.end (ctx);
        }
    }

    void render (float* L, float* R, int n) override
    {
        for (auto& v : voices)
        {
            if (! v.busy) continue;
            for (int i = 0; i < n; ++i)
            {
                if (! v.started)
                {
                    if (v.delay > 0) { --v.delay; continue; }
                    v.begin (ctx); v.started = true;
                }
                if (v.gate > 0 && --v.gate == 0 && v.held) { v.held = false; v.end (ctx); }
                float l = 0, r = 0;
                if (! v.tick (ctx, l, r)) { v.busy = false; break; }
                L[i] += l; R[i] += r;
            }
        }
        post (L, R, n);
    }

    int activeVoices() const override
    {
        int c = 0;
        for (auto& v : voices) if (v.busy) ++c;
        return c;
    }

protected:
    virtual void preparePost (float) {}
    virtual void post (float*, float*, int) {}

    std::array<V, (size_t) N> voices;
    uint32_t counter = 0;
    bool pedal = false;
};

//==============================================================================
// SKANK GUITAR - Karplus-Strong plucked strings, palm-muted chop
//==============================================================================
struct SkankVoice : VoiceBase
{
    float sr = 44100;
    std::vector<float> buf;
    int w = 0, t = 0, exciteLen = 0;
    float D = 100, lpS = 0, g = 0.6f, rho = 0.99f;
    float pickEnv = 0, pickK = 1;
    ADSR env; OnePole exLp, pickHp; Rng rng;

    void prepare (float s) { sr = s; buf.assign ((size_t) (s / 25.0f) + 8, 0.0f); rng.seed ((uint32_t) (size_t) this); }
    inline float at (int i) const { return buf[(size_t) i]; }

    void begin (const InstrCtx& c)
    {
        float f = mtof ((float) note + c.bend);
        D = clampf (sr / f - 0.5f, 2.0f, (float) buf.size() - 2.0f);
        std::fill (buf.begin(), buf.end(), 0.0f);
        w = 0; t = 0; lpS = 0;
        const float mute = c.extra;
        const float t60 = 1.3f - 1.15f * mute;
        rho = std::pow (0.001f, 1.0f / (t60 * f));
        g = (0.3f + 0.65f * c.tone * (0.65f + 0.35f * vel)) * (1.0f - 0.3f * mute);
        g = clampf (g, 0.15f, 0.98f);
        exLp.reset(); exLp.setLP (1500.0f + 7000.0f * c.tone * vel, sr);
        exciteLen = (int) D;
        env.set (0.0008f, 4.0f, 1.0f, 0.03f + 0.06f * (1.0f - mute), sr);
        env.v = 0; env.on();
        pickEnv = 0.3f * vel; pickK = t60Coef (0.012f, sr);
        pickHp.reset(); pickHp.setLP (3000.0f, sr);
    }
    void end (const InstrCtx&) { env.off(); }

    bool tick (const InstrCtx&, float& l, float& r)
    {
        const int size = (int) buf.size();
        float rp = (float) w - D;
        while (rp < 0) rp += (float) size;
        int i0 = (int) rp; float fr = rp - (float) i0;
        int i1 = i0 + 1; if (i1 >= size) i1 = 0;
        float s = at (i0) + (at (i1) - at (i0)) * fr;
        lpS += g * (s - lpS);
        float in = t < exciteLen ? exLp.lp (rng.bi()) * 1.5f : 0.0f;
        buf[(size_t) w] = lpS * rho + in;
        if (++w >= size) w = 0;
        ++t;
        float pick = pickHp.hp (rng.bi()) * pickEnv; pickEnv *= pickK;
        float e = env.tick();
        float out = (s * 0.9f + pick) * e * (0.35f + 0.65f * vel) * 0.5f;
        l = r = out;
        return env.active();
    }
};

class SkankGuitar : public PolyInstrument<SkankVoice, 18>
{
    SVF hp[2], pres[2], lp[2];
    float lastTone = -1, sr = 44100;
    void preparePost (float s) override
    {
        sr = s;
        for (int c = 0; c < 2; ++c) { hp[c].reset(); pres[c].reset(); lp[c].reset(); hp[c].set (230, 0.7f, s); pres[c].set (2700, 1.2f, s); }
        lastTone = -1;
    }
    void post (float* L, float* R, int n) override
    {
        if (std::fabs (ctx.tone - lastTone) > 1.0e-6f)
        {
            lastTone = ctx.tone;
            for (auto& f : lp) f.set (2500.0f + 7000.0f * ctx.tone, 0.7f, sr);
        }
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = hp[c].hp (ch[c][i]);
                x += 0.6f * pres[c].bp (x);
                x = lp[c].lp (x);
                ch[c][i] = std::tanh (x * 2.2f) / 2.2f * 2.6f;
            }
    }
};

//==============================================================================
// PIANO - additive acoustic piano, FM electric piano, honky-tonk
//==============================================================================
struct PianoVoice : VoiceBase
{
    static constexpr int NP = 12;
    float sr = 44100;
    int mode = 0, np = 0;
    float ph[NP] {}, inc[NP] {}, amp[NP] {}, k[NP] {};
    float relGain = 1, relK = 1, atk = 0, atkInc = 1;
    bool released = false;
    float hamEnv = 0, hamK = 1; OnePole hamLp; Rng rng;
    // e-piano
    float cph = 0, cinc = 0, mph = 0, minc = 0, idx = 0, idxK = 1, tph = 0, tinc = 0, tEnv = 0, tK = 1, ampEnv = 0, ampK = 1;

    void prepare (float s) { sr = s; rng.seed ((uint32_t) (size_t) this ^ 0xABCDu); }

    void begin (const InstrCtx& c)
    {
        mode = c.preset;
        const float f = mtof ((float) note + c.bend);
        const float T = clampf (6.5f * std::pow (110.0f / f, 0.55f), 0.7f, 9.0f);
        released = false; relGain = 1;
        relK = t60Coef (note >= 90 ? T : 0.22f, sr);
        atk = 0; atkInc = 1.0f / (0.0015f * sr);

        if (mode == 1)
        {
            cph = mph = tph = 0;
            cinc = f / sr; minc = f / sr;
            idx = (0.6f + 2.2f * vel) * (0.6f + 0.8f * c.tone);
            idxK = t60Coef (0.7f, sr);
            tinc = std::min (f * 14.0f / sr, 0.45f);
            tEnv = 0.16f * vel; tK = t60Coef (0.12f, sr);
            ampEnv = 1.0f; ampK = t60Coef (T * 0.8f, sr);
        }
        else
        {
            const float detune = mode == 2 ? 0.0028f : 0.0004f;
            const float B = 0.00038f;
            const float bright = 1.75f - 0.9f * vel * (0.5f + c.tone);
            np = 0;
            for (int n = 1; n <= 8 && np < NP; ++n)
            {
                float fn = (float) n * f * std::sqrt (1.0f + B * (float) (n * n));
                if (fn > sr * 0.45f) break;
                float a = std::pow ((float) n, -bright);
                float tn = T / (1.0f + 0.45f * (float) (n - 1));
                ph[np] = 0; inc[np] = fn / sr; amp[np] = a; k[np] = t60Coef (tn, sr); ++np;
                if (n <= 4 && np < NP && fn * (1.0f + detune) < sr * 0.45f)
                {
                    ph[np] = 0.25f; inc[np] = fn * (1.0f + detune) / sr; amp[np] = a * 0.8f; k[np] = t60Coef (tn * 0.9f, sr); ++np;
                }
            }
        }
        hamEnv = 0.12f * vel; hamK = t60Coef (0.025f, sr);
        hamLp.reset(); hamLp.setLP (2000.0f + 3000.0f * vel, sr);
    }
    void end (const InstrCtx&) { released = true; }

    bool tick (const InstrCtx&, float& l, float& r)
    {
        float out = 0, level;
        if (atk < 1.0f) { atk += atkInc; if (atk > 1.0f) atk = 1.0f; }
        if (mode == 1)
        {
            float m = fastSin (mph) * idx;
            out = fastSin (cph + m * 0.15915f) * ampEnv;
            out += fastSin (tph) * tEnv;
            cph += cinc; if (cph >= 1) cph -= 1;
            mph += minc; if (mph >= 1) mph -= 1;
            tph += tinc; if (tph >= 1) tph -= 1;
            idx = 0.25f + (idx - 0.25f) * idxK;
            tEnv *= tK; ampEnv *= ampK;
            out *= 0.32f;
            level = ampEnv;
        }
        else
        {
            for (int p = 0; p < np; ++p)
            {
                out += fastSin (ph[p]) * amp[p];
                ph[p] += inc[p]; if (ph[p] >= 1) ph[p] -= 1;
                amp[p] *= k[p];
            }
            out *= 0.2f;
            level = np > 0 ? amp[0] : 0.0f;
        }
        out += hamLp.lp (rng.bi()) * hamEnv; hamEnv *= hamK;
        if (released) relGain *= relK;
        out *= relGain * atk * (0.3f + 0.7f * vel);
        l = r = out;
        return level * relGain > 0.00008f;
    }
};

class Piano : public PolyInstrument<PianoVoice, 20> {};

//==============================================================================
// ORGAN - tonewheel drawbars with key click, percussion and rotary speaker
//==============================================================================
static const float kDrawRatios[9] = { 0.5f, 1.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f };
static const float kDrawPresets[4][9] = {
    { 6, 8, 8, 4, 0, 0, 0, 0, 0 }, // Bubble
    { 8, 8, 8, 0, 0, 0, 0, 0, 0 }, // Jazz 888
    { 8, 8, 8, 8, 8, 8, 8, 8, 8 }, // Full
    { 0, 0, 8, 8, 0, 0, 0, 0, 0 }, // Flute
};
static const float kDrawPerc[4] = { 0.5f, 0.0f, 0.0f, 0.0f };

struct OrganVoice : VoiceBase
{
    float sr = 44100;
    float ph[9] {}, inc[9] {}, lvl[9] {};
    float clickEnv = 0, clickK = 1, percEnv = 0, percK = 1, percPh = 0, percInc = 0;
    ADSR env; OnePole clickLp; Rng rng;

    void prepare (float s) { sr = s; rng.seed ((uint32_t) (size_t) this ^ 0x1234u); }

    void begin (const InstrCtx& c)
    {
        const int p = std::clamp (c.preset, 0, 3);
        const float f = mtof ((float) note + c.bend);
        for (int i = 0; i < 9; ++i)
        {
            inc[i] = f * kDrawRatios[i] / sr;
            lvl[i] = inc[i] < 0.45f ? kDrawPresets[p][i] / 8.0f : 0.0f;
            ph[i] = rng.uni();
        }
        env.set (0.004f, 0.05f, 1.0f, 0.03f, sr); env.v = 0; env.on();
        clickEnv = 0.18f + 0.12f * c.tone; clickK = t60Coef (0.007f, sr);
        clickLp.reset(); clickLp.setLP (2500.0f + 3000.0f * c.tone, sr);
        percInc = std::min (f * 3.0f / sr, 0.45f); percPh = 0;
        percEnv = kDrawPerc[p]; percK = t60Coef (0.45f, sr);
    }
    void end (const InstrCtx&) { env.off(); }

    bool tick (const InstrCtx&, float& l, float& r)
    {
        float s = 0;
        for (int i = 0; i < 9; ++i)
        {
            if (lvl[i] > 0) s += fastSin (ph[i]) * lvl[i];
            ph[i] += inc[i]; if (ph[i] >= 1) ph[i] -= 1;
        }
        s *= 0.09f;
        s += fastSin (percPh) * percEnv * 0.25f; percPh += percInc; if (percPh >= 1) percPh -= 1; percEnv *= percK;
        float e = env.tick();
        s = s * e + clickLp.lp (rng.bi()) * clickEnv * 0.3f;
        clickEnv *= clickK;
        s *= 0.85f + 0.15f * vel;
        l = r = s;
        return env.active();
    }
};

class Organ : public PolyInstrument<OrganVoice, 20>
{
    std::vector<float> buf; int w = 0; float sr = 44100;
    float hornPh = 0, drumPh = 0, hornRate = 0.8f, drumRate = 0.7f;
    OnePole split;
    void preparePost (float s) override
    {
        sr = s; buf.assign ((size_t) (s * 0.02f), 0.0f); w = 0;
        split.reset(); split.setLP (800.0f, s);
    }
    void post (float* L, float* R, int n) override
    {
        const int mode = (int) std::lround (ctx.extra * 2.0f); // 0 off, 1 slow, 2 fast
        const float tHorn = mode == 2 ? 6.6f : 0.8f;
        const float tDrum = mode == 2 ? 5.7f : 0.65f;
        const int size = (int) buf.size();
        for (int i = 0; i < n; ++i)
        {
            float in = 0.5f * (L[i] + R[i]);
            in = std::tanh (in * 1.4f) / 1.4f;
            if (mode == 0) { L[i] = R[i] = in; continue; }
            hornRate += (tHorn - hornRate) * 0.00004f;
            drumRate += (tDrum - drumRate) * 0.00002f;
            float low = split.lp (in);
            float high = in - low;
            buf[(size_t) w] = high;
            float hs = fastSin (hornPh), hc = fastSin (hornPh + 0.25f);
            auto rd = [&] (float d) {
                float rp = (float) w - d; while (rp < 0) rp += (float) size;
                int i0 = (int) rp; float fr = rp - (float) i0; int i1 = (i0 + 1) % size;
                return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * fr;
            };
            float yl = rd ((0.0025f + 0.0011f * hs) * sr);
            float yr = rd ((0.0025f - 0.0011f * hs) * sr);
            float dm = 1.0f + 0.12f * fastSin (drumPh);
            L[i] = low * dm + yl * (1.0f + 0.28f * hc);
            R[i] = low * dm + yr * (1.0f - 0.28f * hc);
            if (++w >= size) w = 0;
            hornPh += hornRate / sr; if (hornPh >= 1) hornPh -= 1;
            drumPh += drumRate / sr; if (drumPh >= 1) drumPh -= 1;
        }
    }
};

//==============================================================================
// BASS - roots sub, dub, digital (80s dancehall), finger
//==============================================================================
struct BassVoice : VoiceBase
{
    float sr = 44100, freq = 55, ph = 0, fEnv = 1, fK = 1, thump = 0, thumpK = 1;
    int mode = 0, cnt = 0;
    Osc osc; SVF filt; ADSR env; OnePole thLp; Rng rng;

    void prepare (float s) { sr = s; rng.seed ((uint32_t) (size_t) this ^ 0x5151u); }

    void begin (const InstrCtx& c)
    {
        mode = c.preset;
        freq = mtof ((float) note);
        ph = 0; osc.ph = 0; cnt = 0; filt.reset(); fEnv = 1;
        switch (mode)
        {
            case 0: env.set (0.004f, 0.8f, 0.75f, 0.07f, sr); fK = 1; break;
            case 1: env.set (0.006f, 1.4f, 0.85f, 0.1f, sr); fK = 1; break;
            case 2: env.set (0.002f, 0.45f, 0.6f, 0.06f, sr); fK = t60Coef (0.35f, sr); break;
            default: env.set (0.003f, 1.5f, 0.5f, 0.06f, sr); fK = t60Coef (0.25f, sr); break;
        }
        env.v = 0; env.on();
        thump = 0.12f * vel; thumpK = t60Coef (0.025f, sr);
        thLp.reset(); thLp.setLP (350.0f, sr);
    }
    void end (const InstrCtx&) { env.off(); }

    bool tick (const InstrCtx& c, float& l, float& r)
    {
        const float fr = freq * semis (c.bend);
        float s = 0;
        switch (mode)
        {
            case 0:
            {
                float h = 0.06f + 0.3f * c.tone;
                s = fastSin (ph) + h * fastSin (2.0f * ph) + 0.35f * h * fastSin (3.0f * ph);
                ph += fr / sr; if (ph >= 1) ph -= 1;
                s *= 0.8f;
                break;
            }
            case 1:
            {
                float tri = ph < 0.5f ? 4 * ph - 1 : 3 - 4 * ph;
                s = fastSin (ph) + 0.35f * tri;
                ph += fr / sr; if (ph >= 1) ph -= 1;
                s = std::tanh (s * 1.8f) / 1.3f;
                if ((cnt++ & 15) == 0) filt.set (300.0f + 700.0f * c.tone, 0.7f, sr);
                s = filt.lp (s);
                break;
            }
            case 2:
            {
                osc.setFreq (fr, sr);
                s = osc.pulse (0.3f) * 0.6f + fastSin (osc.ph) * 0.4f;
                if ((cnt++ & 15) == 0) filt.set ((350.0f + 2200.0f * c.tone) * (0.35f + 0.65f * fEnv), 1.1f, sr);
                s = filt.lp (s);
                fEnv *= fK;
                break;
            }
            default:
            {
                osc.setFreq (fr, sr);
                s = osc.saw() * 0.7f;
                if ((cnt++ & 15) == 0) filt.set (180.0f + 350.0f * c.tone + 2200.0f * vel * fEnv, 0.9f, sr);
                s = filt.lp (s) * 1.2f;
                fEnv *= fK;
                break;
            }
        }
        float out = s * env.tick() * (0.55f + 0.45f * vel) * 0.7f;
        out += thLp.lp (rng.bi()) * thump; thump *= thumpK;
        l = r = out;
        return env.active();
    }
};

class Bass : public PolyInstrument<BassVoice, 6> {};

//==============================================================================
// LEAD - melodica, square lead, synth horns, bubble synth, whistle
//==============================================================================
struct LeadVoice : VoiceBase
{
    float sr = 44100, freq = 440, t = 0, vibPh = 0, fEnv = 1, fK = 1, scoop = 0, scoopK = 1;
    int mode = 0, cnt = 0;
    Osc o1, o2; SVF filt, form; ADSR env; Rng rng; OnePole breath;

    void prepare (float s) { sr = s; rng.seed ((uint32_t) (size_t) this ^ 0x7777u); }

    void begin (const InstrCtx& c)
    {
        mode = c.preset;
        freq = mtof ((float) note);
        t = 0; vibPh = 0; cnt = 0; fEnv = 1; scoop = 0;
        filt.reset(); form.reset(); o1.ph = 0; o2.ph = 0.3f;
        switch (mode)
        {
            case 0: env.set (0.03f, 0.3f, 0.85f, 0.08f, sr); form.set (1400, 1.4f, sr); break;
            case 1: env.set (0.005f, 0.2f, 0.8f, 0.06f, sr); break;
            case 2: env.set (0.03f, 0.4f, 0.8f, 0.15f, sr); fK = t60Coef (0.6f, sr); scoop = -0.45f; scoopK = t60Coef (0.06f, sr); break;
            case 3: env.set (0.002f, 0.35f, 0.0f, 0.05f, sr); fK = t60Coef (0.25f, sr); break;
            default: env.set (0.04f, 0.3f, 0.9f, 0.1f, sr); break;
        }
        env.v = 0; env.on();
        breath.reset(); breath.setLP (2500.0f, sr);
    }
    void end (const InstrCtx&) { env.off(); }

    bool tick (const InstrCtx& c, float& l, float& r)
    {
        t += 1.0f / sr;
        float vibDepth = c.mod * 0.4f;
        if (mode == 0) vibDepth += 0.18f * clampf ((t - 0.25f) * 2.0f, 0.0f, 1.0f);
        if (mode == 4) vibDepth += 0.15f;
        vibPh += 5.5f / sr; if (vibPh >= 1) vibPh -= 1;
        float pitch = c.bend + vibDepth * fastSin (vibPh) + scoop;
        scoop *= scoopK;
        const float f = freq * semis (pitch);
        o1.setFreq (f, sr);
        float s = 0;
        const bool upd = (cnt++ & 15) == 0;
        switch (mode)
        {
            case 0: // melodica (reed)
            {
                o2.setFreq (f, sr);
                s = o1.pulse (0.35f) * 0.55f + o2.saw() * 0.3f;
                s = 0.6f * form.bp (s) + 0.4f * s;
                if (upd) filt.set (2500.0f + 3500.0f * c.tone, 0.7f, sr);
                s = filt.lp (s) + breath.lp (rng.bi()) * 0.06f;
                break;
            }
            case 1: // square lead
            {
                o2.setFreq (f * 1.004f, sr);
                s = 0.5f * (o1.pulse (0.5f) + o2.pulse (0.5f));
                if (upd) filt.set (1800.0f + 4500.0f * c.tone, 0.8f, sr);
                s = filt.lp (s);
                break;
            }
            case 2: // synth horns
            {
                o2.setFreq (f * 1.0047f, sr);
                s = 0.5f * (o1.saw() + o2.saw());
                float att = clampf (t / 0.06f, 0.0f, 1.0f);
                if (upd) filt.set ((500.0f + (1500.0f + 2500.0f * c.tone) * vel * att) * (0.6f + 0.4f * fEnv), 0.9f, sr);
                fEnv *= fK;
                s = filt.lp (s) * 1.2f;
                break;
            }
            case 3: // bubble synth pluck
            {
                o2.setFreq (f * 0.5f, sr);
                s = o1.saw() * 0.6f + o2.pulse (0.5f) * 0.4f;
                if (upd) filt.set (350.0f + (1500.0f + 4000.0f * c.tone) * fEnv, 1.6f, sr);
                fEnv *= fK;
                s = filt.lp (s);
                break;
            }
            default: // whistle
            {
                s = o1.sine() + 0.08f * fastSin (2.0f * o1.ph) + breath.lp (rng.bi()) * 0.03f;
                break;
            }
        }
        float out = s * env.tick() * (0.4f + 0.6f * vel) * 0.38f;
        l = r = out;
        return env.active();
    }
};

class Lead : public PolyInstrument<LeadVoice, 10> {};

//==============================================================================
// PAD - strings, warm pad, choir, glass
//==============================================================================
struct PadVoice : VoiceBase
{
    float sr = 44100, freq = 220, lfoPh = 0, fmIdx = 0, fmK = 1, cph = 0, mph = 0;
    int mode = 0, cnt = 0;
    Osc o[3]; SVF fl, fr, f1, f2, f3; ADSR env;

    void prepare (float s) { sr = s; }

    void begin (const InstrCtx& c)
    {
        mode = c.preset;
        freq = mtof ((float) note);
        cnt = 0;
        o[0].ph = 0.1f; o[1].ph = 0.5f; o[2].ph = 0.8f;
        fl.reset(); fr.reset(); f1.reset(); f2.reset(); f3.reset();
        switch (mode)
        {
            case 0: env.set (0.25f, 1.0f, 0.9f, 0.7f, sr); break;
            case 1: env.set (0.5f, 1.5f, 0.9f, 1.2f, sr); break;
            case 2: env.set (0.3f, 1.0f, 0.9f, 0.8f, sr);
                    f1.set (700, 5.0f, sr); f2.set (1150, 6.0f, sr); f3.set (2600, 6.0f, sr); break;
            default: env.set (0.02f, 2.0f, 0.5f, 1.2f, sr); fmIdx = 1.2f; fmK = t60Coef (1.5f, sr); cph = mph = 0; break;
        }
        env.v = 0; env.on();
    }
    void end (const InstrCtx&) { env.off(); }

    bool tick (const InstrCtx& c, float& l, float& r)
    {
        lfoPh += 0.35f / sr; if (lfoPh >= 1) lfoPh -= 1;
        const float lfo = fastSin (lfoPh);
        const float f = freq * semis (c.bend + 0.04f * lfo + c.mod * 0.2f * fastSin (lfoPh * 14.0f));
        const bool upd = (cnt++ & 15) == 0;
        float sl = 0, sr2 = 0;
        switch (mode)
        {
            case 0:
            {
                o[0].setFreq (f * 0.9942f, sr); o[1].setFreq (f, sr); o[2].setFreq (f * 1.0058f, sr);
                float a = o[0].saw(), b = o[1].saw(), d = o[2].saw();
                sl = a * 0.7f + b * 0.5f + d * 0.3f;
                sr2 = d * 0.7f + b * 0.5f + a * 0.3f;
                if (upd) { float fc = 2000.0f + 3500.0f * c.tone; fl.set (fc, 0.7f, sr); fr.set (fc, 0.7f, sr); }
                sl = fl.lp (sl) * 0.5f; sr2 = fr.lp (sr2) * 0.5f;
                break;
            }
            case 1:
            {
                o[0].setFreq (f * 0.9965f, sr); o[1].setFreq (f * 1.0035f, sr); o[2].setFreq (f * 0.5f, sr);
                float a = o[0].saw(), b = o[1].saw(), d = o[2].pulse (0.5f) * 0.5f;
                sl = a + d; sr2 = b + d;
                if (upd) { float fc = (700.0f + 1500.0f * c.tone) * (1.0f + 0.25f * lfo); fl.set (fc, 0.9f, sr); fr.set (fc, 0.9f, sr); }
                sl = fl.lp (sl) * 0.45f; sr2 = fr.lp (sr2) * 0.45f;
                break;
            }
            case 2:
            {
                o[0].setFreq (f * 0.997f, sr); o[1].setFreq (f * 1.003f, sr);
                float a = o[0].saw(), b = o[1].saw();
                float va = f1.bp (a) + 0.7f * f2.bp (a) + 0.25f * f3.bp (a);
                float vb = f1.bp (b) + 0.7f * f2.bp (b) + 0.25f * f3.bp (b);
                sl = va * 0.9f; sr2 = vb * 0.9f;
                break;
            }
            default:
            {
                float m = fastSin (mph) * (fmIdx * (0.5f + c.tone));
                float s = fastSin (cph + m * 0.15915f) + 0.3f * fastSin (2.0f * cph + 0.1f);
                cph += f / sr; if (cph >= 1) cph -= 1;
                mph += f * 3.5f / sr; if (mph >= 1) mph -= 1;
                fmIdx = 0.35f + (fmIdx - 0.35f) * fmK;
                sl = s * 0.5f; sr2 = s * 0.5f;
                break;
            }
        }
        float e = env.tick() * (0.5f + 0.5f * vel) * 0.32f;
        l = sl * e; r = sr2 * e;
        return env.active();
    }
};

class Pad : public PolyInstrument<PadVoice, 16> {};

//==============================================================================
std::unique_ptr<InstrumentBase> makeSkankGuitar() { return std::make_unique<SkankGuitar>(); }
std::unique_ptr<InstrumentBase> makePiano()       { return std::make_unique<Piano>(); }
std::unique_ptr<InstrumentBase> makeOrgan()       { return std::make_unique<Organ>(); }
std::unique_ptr<InstrumentBase> makeBass()        { return std::make_unique<Bass>(); }
std::unique_ptr<InstrumentBase> makeLead()        { return std::make_unique<Lead>(); }
std::unique_ptr<InstrumentBase> makePad()         { return std::make_unique<Pad>(); }

} // namespace rf
