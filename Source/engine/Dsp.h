// RiddimForge - shared DSP building blocks (no JUCE dependency)
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace rf
{
constexpr float kPi    = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clampf (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float mtof (float m) { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }
inline float semis (float s) { return std::pow (2.0f, s / 12.0f); }
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

// Per-sample multiplier that decays to -60 dB after t60 seconds
inline float t60Coef (float t60, float sr)
{
    t60 = std::max (t60, 0.0005f);
    return std::pow (0.001f, 1.0f / (t60 * sr));
}

//==============================================================================
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    void seed (uint32_t v) { s = v ? v : 1u; }
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    inline float bi() { return uni() * 2.0f - 1.0f; }
};

//==============================================================================
struct SineTable
{
    static constexpr int N = 4096;
    float t[N + 1];
    SineTable() { for (int i = 0; i <= N; ++i) t[i] = std::sin (kTwoPi * (float) i / (float) N); }
};
inline const SineTable& sineTable() { static SineTable s; return s; }

// phase in cycles; any value (wrapped internally)
inline float fastSin (float phase)
{
    const auto& st = sineTable();
    float p = phase - std::floor (phase);
    float x = p * (float) SineTable::N;
    int i = (int) x;
    if (i >= SineTable::N) i = SineTable::N - 1;
    float f = x - (float) i;
    return st.t[i] + (st.t[i + 1] - st.t[i]) * f;
}

//==============================================================================
// Topology-preserving state variable filter (Cytomic / Zavalishin)
struct SVF
{
    float ic1 = 0, ic2 = 0, a1 = 1, a2 = 0, a3 = 0, k = 1.4f;
    void set (float fc, float q, float sr)
    {
        fc = clampf (fc, 10.0f, sr * 0.48f);
        float g = std::tan (kPi * fc / sr);
        k  = 1.0f / std::max (q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    inline void tick (float v0, float& lp, float& bp, float& hp)
    {
        float v3 = v0 - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    inline float lp (float x) { float l, b, h; tick (x, l, b, h); return l; }
    inline float bp (float x) { float l, b, h; tick (x, l, b, h); return b; }
    inline float hp (float x) { float l, b, h; tick (x, l, b, h); return h; }
};

struct OnePole
{
    float z = 0, a = 0.5f;
    void setLP (float fc, float sr) { a = 1.0f - std::exp (-kTwoPi * clampf (fc, 5.0f, sr * 0.49f) / sr); }
    inline float lp (float x) { z += a * (x - z); return z; }
    inline float hp (float x) { z += a * (x - z); return x - z; }
    void reset() { z = 0; }
};

//==============================================================================
inline float polyBlep (float t, float dt)
{
    if (t < dt)        { t /= dt;               return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt;   return t * t + t + t + 1.0f; }
    return 0.0f;
}

struct Osc
{
    float ph = 0, inc = 0;
    void setFreq (float f, float sr) { inc = clampf (f / sr, 0.0f, 0.45f); }
    inline void adv() { ph += inc; if (ph >= 1.0f) ph -= 1.0f; }
    inline float saw()  { float o = 2.0f * ph - 1.0f - polyBlep (ph, inc); adv(); return o; }
    inline float sine() { float o = fastSin (ph); adv(); return o; }
    inline float tri()  { float o = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; adv(); return o; }
    inline float pulse (float pw)
    {
        float o = ph < pw ? 1.0f : -1.0f;
        o += polyBlep (ph, inc);
        float t2 = ph + 1.0f - pw; t2 -= std::floor (t2);
        o -= polyBlep (t2, inc);
        adv();
        return o;
    }
};

//==============================================================================
struct ADSR
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage st = Idle;
    float v = 0, s = 0.7f, aInc = 0.01f, dK = 0.999f, rK = 0.99f;

    void set (float a, float d, float sus, float r, float sr)
    {
        aInc = 1.0f / (std::max (a, 0.0005f) * sr);
        dK   = t60Coef (d, sr);
        s    = sus;
        rK   = t60Coef (r, sr);
    }
    void on()  { st = Attack; }
    void off() { if (st != Idle) st = Release; }
    void kill() { st = Idle; v = 0; }
    bool active() const { return st != Idle; }
    inline float tick()
    {
        switch (st)
        {
            case Attack:  v += aInc; if (v >= 1.0f) { v = 1.0f; st = Decay; } break;
            case Decay:   v = s + (v - s) * dK;
                          if (s <= 0.0001f) { if (v < 0.0001f) { v = 0; st = Idle; } }
                          else if (v - s < 0.0005f) { v = s; st = Sustain; }
                          break;
            case Sustain: v = s; break;
            case Release: v *= rK; if (v < 0.00005f) { v = 0; st = Idle; } break;
            case Idle:
            default:      v = 0; break;
        }
        return v;
    }
};

//==============================================================================
inline float softClip (float x)
{
    const float t = 0.85f;
    float a = std::fabs (x);
    if (a <= t) return x;
    float y = t + (1.0f - t) * std::tanh ((a - t) / (1.0f - t));
    return x < 0 ? -y : y;
}

inline void panGains (float pan, float& gl, float& gr)
{
    float p = (clampf (pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi; // 0..pi/2
    gl = std::cos (p) * 1.41421356f;
    gr = std::sin (p) * 1.41421356f;
    gl = std::min (gl, 1.0f);
    gr = std::min (gr, 1.0f);
}

// Simple linear smoother
struct Smoothed
{
    float cur = 0, target = 0, step = 0; int remaining = 0;
    void reset (float v) { cur = target = v; remaining = 0; }
    void setTarget (float v, int samples)
    {
        if (std::fabs (v - target) < 1.0e-9f) return;
        target = v; remaining = std::max (1, samples);
        step = (target - cur) / (float) remaining;
    }
    inline float next() { if (remaining > 0) { cur += step; if (--remaining == 0) cur = target; } return cur; }
};

} // namespace rf
