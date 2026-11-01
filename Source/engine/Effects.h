// RiddimForge - dub tape delay, reverb, dub sweep filter
#pragma once
#include "Dsp.h"

namespace rf
{
//==============================================================================
// Ping-pong tape echo with filtered, saturated feedback and a little wow
class DubDelay
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        size = (int) (sr * 3.0f) + 4;
        bl.assign ((size_t) size, 0.0f);
        br.assign ((size_t) size, 0.0f);
        w = 0; cur = target = sr * 0.3f;
        for (auto* f : { &hpL, &hpR }) { f->reset(); f->set (140.0f, 0.7f, sr); }
        lastTone = -1;
    }
    void setTimeSeconds (float s) { target = clampf (s, 0.01f, 2.9f) * sr; }
    void setFeedback (float f) { fb = clampf (f, 0.0f, 0.97f); }
    void setTone (float t)
    {
        if (std::fabs (t - lastTone) < 1.0e-6f) return;
        lastTone = t;
        float fc = 700.0f * std::pow (10.0f, t * 1.0f); // 700 .. 7000
        lpL.set (fc, 0.7f, sr); lpR.set (fc, 0.7f, sr);
    }
    void clear() { std::fill (bl.begin(), bl.end(), 0.0f); std::fill (br.begin(), br.end(), 0.0f); }

    inline void process (float inL, float inR, float& outL, float& outR)
    {
        cur += (target - cur) * 0.0004f;
        wow += 0.45f / sr; if (wow >= 1) wow -= 1;
        float d = cur * (1.0f + 0.0015f * fastSin (wow));
        float yl = read (bl, d), yr = read (br, d);
        float fbl = std::tanh (lpL.lp (hpL.hp (yr)) * 1.2f) / 1.2f;
        float fbr = std::tanh (lpR.lp (hpR.hp (yl)) * 1.2f) / 1.2f;
        bl[(size_t) w] = 0.5f * (inL + inR) + inL * 0.3f + fbl * fb;
        br[(size_t) w] = inR * 0.3f + fbr * fb;
        if (++w >= size) w = 0;
        outL = yl; outR = yr;
    }

private:
    inline float read (const std::vector<float>& b, float d) const
    {
        float rp = (float) w - d;
        while (rp < 0) rp += (float) size;
        int i0 = (int) rp; float fr = rp - (float) i0;
        int i1 = i0 + 1; if (i1 >= size) i1 = 0;
        return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * fr;
    }
    float sr = 44100, cur = 0, target = 0, fb = 0.45f, wow = 0, lastTone = -1;
    int size = 0, w = 0;
    std::vector<float> bl, br;
    SVF hpL, hpR, lpL, lpR;
};

//==============================================================================
// Freeverb-style stereo reverb
class Reverb
{
public:
    void prepare (float sampleRate)
    {
        static const int combT[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int apT[4] = { 556, 441, 341, 225 };
        const float sc = sampleRate / 44100.0f;
        for (int c = 0; c < 8; ++c)
        {
            combL[c].init ((int) ((float) combT[c] * sc));
            combR[c].init ((int) ((float) (combT[c] + 23) * sc));
        }
        for (int a = 0; a < 4; ++a)
        {
            apL[a].init ((int) ((float) apT[a] * sc));
            apR[a].init ((int) ((float) (apT[a] + 23) * sc));
        }
        preHp.reset(); preHp.setLP (180.0f, sampleRate);
        preHp2.reset(); preHp2.setLP (180.0f, sampleRate);
    }
    void set (float size, float damp)
    {
        float fb = 0.7f + 0.28f * clampf (size, 0.0f, 1.0f);
        float d = 0.05f + 0.6f * clampf (damp, 0.0f, 1.0f);
        for (auto* c : { combL, combR })
            for (int i = 0; i < 8; ++i) { c[i].fb = fb; c[i].d1 = d; c[i].d2 = 1.0f - d; }
    }
    inline void process (float inL, float inR, float& outL, float& outR)
    {
        float in = (preHp.hp (inL) + preHp2.hp (inR)) * 0.015f;
        float l = 0, r = 0;
        for (int c = 0; c < 8; ++c) { l += combL[c].tick (in); r += combR[c].tick (in); }
        for (int a = 0; a < 4; ++a) { l = apL[a].tick (l); r = apR[a].tick (r); }
        outL = l * 3.0f; outR = r * 3.0f;
    }

private:
    struct Comb
    {
        std::vector<float> b; int i = 0; float st = 0, fb = 0.84f, d1 = 0.2f, d2 = 0.8f;
        void init (int n) { b.assign ((size_t) std::max (n, 4), 0.0f); i = 0; st = 0; }
        inline float tick (float x)
        {
            float o = b[(size_t) i];
            st = o * d2 + st * d1;
            b[(size_t) i] = x + st * fb;
            if (++i >= (int) b.size()) i = 0;
            return o;
        }
    };
    struct AP
    {
        std::vector<float> b; int i = 0;
        void init (int n) { b.assign ((size_t) std::max (n, 4), 0.0f); i = 0; }
        inline float tick (float x)
        {
            float bo = b[(size_t) i];
            float o = -x + bo;
            b[(size_t) i] = x + bo * 0.5f;
            if (++i >= (int) b.size()) i = 0;
            return o;
        }
    };
    Comb combL[8], combR[8];
    AP apL[4], apR[4];
    OnePole preHp, preHp2;
};

//==============================================================================
// One-knob dub filter: left = low-pass sweep, right = high-pass sweep, centre = off
class DubFilter
{
public:
    void prepare (float sampleRate) { sr = sampleRate; l.reset(); r.reset(); amt.reset (0); }
    void process (float* L, float* R, int n, float amount)
    {
        amt.setTarget (amount, n);
        for (int i = 0; i < n; i += 16)
        {
            const int m = std::min (16, n - i);
            float a = 0;
            for (int k = 0; k < m; ++k) a = amt.next();
            if (std::fabs (a) < 0.02f)
            {
                // keep filter state warm but pass audio untouched
                l.reset(); r.reset();
                continue;
            }
            const bool lpMode = a < 0;
            const float fc = lpMode ? 20000.0f * std::pow (0.0075f, -a) : 25.0f * std::pow (90.0f, a);
            l.set (fc, 1.3f, sr); r.set (fc, 1.3f, sr);
            for (int k = i; k < i + m; ++k)
            {
                float ll, lb, lh, rl, rb, rh;
                l.tick (L[k], ll, lb, lh);
                r.tick (R[k], rl, rb, rh);
                L[k] = lpMode ? ll : lh;
                R[k] = lpMode ? rl : rh;
            }
        }
    }

private:
    float sr = 44100;
    SVF l, r;
    Smoothed amt;
};

} // namespace rf
