#include "Drums.h"

namespace rf
{
//==============================================================================
namespace
{
struct MapTable
{
    DrumMapEntry e[128];
    void set (int note, int type, float tune, float pan, const char* name)
    {
        e[note] = { type, tune, pan, note >= 60, name };
    }
    MapTable()
    {
        set (35, KickBoom,   1.0f,  0.0f,  "Kick Boom");
        set (36, Kick,       1.0f,  0.0f,  "Kick");
        set (37, CrossStick, 1.0f,  0.05f, "Cross Stick");
        set (38, Snare,      1.0f,  0.05f, "Snare");
        set (39, Clap,       1.0f,  0.0f,  "Hand Clap");
        set (40, Rimshot,    1.0f,  0.05f, "Rimshot");
        set (41, Tom,        0.62f, -0.35f, "Floor Tom Lo");
        set (42, HatClosed,  1.0f,  0.35f, "Hi-Hat Closed");
        set (43, Tom,        0.72f, -0.28f, "Floor Tom Hi");
        set (44, HatPedal,   1.0f,  0.35f, "Hi-Hat Pedal");
        set (45, Tom,        0.85f, -0.12f, "Low Tom");
        set (46, HatOpen,    1.0f,  0.35f, "Hi-Hat Open");
        set (47, Tom,        1.0f,  0.0f,  "Low-Mid Tom");
        set (48, Tom,        1.18f, 0.12f, "Hi-Mid Tom");
        set (49, Crash,      1.0f,  0.3f,  "Crash");
        set (50, Tom,        1.38f, 0.25f, "High Tom");
        set (51, Ride,       1.0f,  -0.35f, "Ride");
        set (52, China,      1.0f,  -0.45f, "China");
        set (53, RideBell,   1.0f,  -0.35f, "Ride Bell");
        set (54, Tambourine, 1.0f,  -0.5f, "Tambourine");
        set (55, Splash,     1.0f,  0.45f, "Splash");
        set (56, Cowbell,    1.0f,  0.3f,  "Cowbell");
        set (57, Crash,      1.15f, -0.3f, "Crash 2");
        set (58, Syndrum,    1.0f,  0.2f,  "Syndrum");
        set (59, Ride,       1.1f,  -0.4f, "Ride 2");
        // percussion bus
        set (60, BongoHi,    1.0f,  -0.35f, "Bongo Hi");
        set (61, BongoLo,    1.0f,  -0.25f, "Bongo Lo");
        set (62, CongaMute,  1.0f,  -0.15f, "Conga Mute");
        set (63, CongaOpen,  1.0f,  -0.15f, "Conga Open");
        set (64, CongaLow,   1.0f,  -0.4f, "Conga Low");
        set (65, TimbaleHi,  1.0f,  0.4f,  "Timbale Hi");
        set (66, TimbaleLo,  1.0f,  0.25f, "Timbale Lo");
        set (67, AgogoHi,    1.0f,  0.5f,  "Agogo Hi");
        set (68, AgogoLo,    1.0f,  0.5f,  "Agogo Lo");
        set (69, Cabasa,     1.0f,  0.45f, "Cabasa");
        set (70, Maracas,    1.0f,  0.5f,  "Maracas");
        set (71, TimbaleRim, 1.0f,  0.35f, "Timbale Rim (Paila)");
        set (72, TimbaleRim, 0.82f, 0.3f,  "Timbale Shell");
        set (73, GuiroShort, 1.0f,  -0.45f, "Guiro Short");
        set (74, GuiroLong,  1.0f,  -0.45f, "Guiro Long");
        set (75, Claves,     1.0f,  0.5f,  "Claves");
        set (76, WoodHi,     1.0f,  0.4f,  "Wood Block Hi");
        set (77, WoodLo,     1.0f,  0.4f,  "Wood Block Lo");
        set (78, CongaOpen,  1.12f, -0.1f, "Conga Slap Hi");
        set (79, TimbaleHi,  1.12f, 0.45f, "Timbale Hi 2");
        set (80, TriangleMute, 1.0f, 0.55f, "Triangle Mute");
        set (81, TriangleOpen, 1.0f, 0.55f, "Triangle Open");
        set (82, Shaker,     1.0f,  0.45f, "Shaker");
        set (84, NyaBass,    1.0f,  0.0f,  "Nyabinghi Bass");
        set (85, NyaFunde,   1.0f,  -0.3f, "Funde");
        set (86, NyaRepeater, 1.0f, 0.3f,  "Repeater");
        set (87, NyaRepeaterSlap, 1.0f, 0.3f, "Repeater Slap");
        set (88, Syndrum,    1.0f,  0.2f,  "Syndrum Pew");
        set (89, Syndrum,    0.7f,  -0.2f, "Syndrum Low");
        set (90, DubSiren,   1.0f,  0.0f,  "Dub Siren");
        set (91, AirHorn,    1.0f,  0.0f,  "Air Horn");
        set (92, Laser,      1.0f,  0.0f,  "Laser Zap");
        set (93, Tom,        2.4f,  0.3f,  "Rototom Hi");
        set (94, Tom,        1.9f,  0.25f, "Rototom Lo");
    }
};
const MapTable& table() { static MapTable t; return t; }

constexpr float k808[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
} // namespace

const DrumMapEntry& drumMap (int note)
{
    static DrumMapEntry none;
    if (note < 0 || note > 127) return none;
    return table().e[note];
}

//==============================================================================
static void partials (DrumRecipe& r, float f, std::initializer_list<float> ratios,
                      std::initializer_list<float> amps, std::initializer_list<float> t60s)
{
    r.nPartials = (int) std::min<size_t> (3, ratios.size());
    auto ri = ratios.begin(); auto ai = amps.begin(); auto ti = t60s.begin();
    for (int i = 0; i < r.nPartials; ++i)
    {
        r.freq[i] = f * *ri++;
        r.amp[i]  = *ai++;
        r.t60[i]  = *ti++;
    }
}
static void noise (DrumRecipe& r, int mode, float fc, float q, float amp, float t60, float attack = 0)
{
    r.noiseMode = mode; r.noiseFc = fc; r.noiseQ = q; r.noiseAmp = amp; r.noiseT60 = t60; r.noiseAttack = attack;
}
static void metal (DrumRecipe& r, float scale, float amp, float t60, float bp, float hp, int count = 6)
{
    r.metalCount = count;
    for (int i = 0; i < count; ++i) r.metalFreq[i] = k808[i] * scale;
    r.metalAmp = amp; r.metalT60 = t60; r.metalBpFc = bp; r.metalHpFc = hp;
}

void buildDrumRecipe (int type, float keyTune, const DrumTweaks& tw, DrumRecipe& r)
{
    r = DrumRecipe();
    const float kt  = semis (tw.kickTune);
    const float st  = semis (tw.snareTune);
    const float tt  = semis (tw.tomTune) * keyTune;
    const float pt  = semis (tw.percTune) * keyTune;
    const float kd  = tw.kitDecay;
    const float pd  = tw.percDecay;
    const float hat = 0.8f + 0.6f * tw.hatTone;

    switch (type)
    {
        case Kick:
            partials (r, 52.0f * kt, { 1.0f, 2.02f }, { 1.0f, 0.12f }, { 0.55f * kd, 0.08f });
            r.pitchDrop = 2.3f; r.pitchT60 = 0.09f;
            noise (r, 3, 3000, 0.7f, 0.25f, 0.012f);
            r.clickAmp = 0.35f; r.clickFreq = 1800; r.clickT60 = 0.004f;
            r.gain = 1.05f; r.drive = 0.35f; r.length = 1.5f;
            break;
        case KickBoom:
            partials (r, 44.0f * kt, { 1.0f, 1.5f }, { 1.0f, 0.08f }, { 0.95f * kd, 0.15f });
            r.pitchDrop = 1.8f; r.pitchT60 = 0.12f;
            noise (r, 3, 1200, 0.7f, 0.25f, 0.02f);
            r.clickAmp = 0.15f; r.clickFreq = 900;
            r.gain = 1.1f; r.drive = 0.4f; r.length = 2.0f;
            break;
        case Snare: // reggae snare: tuned high and cracking
            partials (r, 245.0f * st, { 1.0f, 1.56f, 2.24f }, { 1.0f, 0.55f, 0.3f }, { 0.13f * kd, 0.09f * kd, 0.06f });
            r.pitchDrop = 0.25f; r.pitchT60 = 0.02f;
            noise (r, 1, 4800, 0.7f, 0.95f, 0.2f * kd);
            r.clickAmp = 0.35f; r.clickFreq = 3200;
            r.gain = 0.7f; r.drive = 0.2f;
            break;
        case Rimshot:
            partials (r, 410.0f * st, { 1.0f, 1.71f, 4.1f }, { 1.0f, 0.6f, 0.45f }, { 0.08f * kd, 0.06f, 0.05f });
            r.pitchDrop = 0.15f; r.pitchT60 = 0.015f;
            noise (r, 1, 3000, 1.1f, 0.9f, 0.09f * kd);
            r.clickAmp = 0.9f; r.clickFreq = 3800; r.clickT60 = 0.003f;
            r.gain = 0.85f; r.drive = 0.4f;
            break;
        case CrossStick: // the one-drop "tok"
            partials (r, 520.0f * st, { 1.0f, 2.3f, 3.6f }, { 1.0f, 0.6f, 0.3f }, { 0.05f, 0.035f, 0.025f });
            r.pitchDrop = 0.08f; r.pitchT60 = 0.01f;
            noise (r, 1, 1800, 3.0f, 0.55f, 0.035f);
            r.clickAmp = 0.7f; r.clickFreq = 4200; r.clickT60 = 0.002f;
            r.gain = 0.46f;
            break;
        case Clap:
            noise (r, 1, 1150, 1.3f, 1.0f, 0.22f * kd);
            r.bursts = 4; r.burstSpacing = 0.0095f; r.burstT60 = 0.012f;
            r.gain = 0.7f;
            break;
        case Tom:
        {
            float f = 140.0f * tt;
            partials (r, f, { 1.0f, 1.5f, 2.45f }, { 1.0f, 0.25f, 0.1f }, { 0.55f * kd * (140.0f / f + 0.2f), 0.2f, 0.1f });
            r.pitchDrop = 0.35f; r.pitchT60 = 0.09f;
            noise (r, 1, 1400, 0.8f, 0.3f, 0.04f);
            r.clickAmp = 0.25f; r.clickFreq = 2000;
            r.gain = 0.85f; r.drive = 0.15f;
            break;
        }
        case HatClosed:
            metal (r, hat, 1.0f, 0.055f * kd, 10000, 7000);
            noise (r, 2, 8000, 0.7f, 0.35f, 0.045f * kd);
            r.gain = 0.68f; r.choke = 1;
            break;
        case HatPedal:
            metal (r, hat, 0.8f, 0.04f, 9500, 6500);
            noise (r, 2, 7500, 0.7f, 0.25f, 0.035f);
            r.gain = 0.42f; r.choke = 1;
            break;
        case HatOpen:
            metal (r, hat, 1.0f, 0.5f * kd, 10000, 6800);
            noise (r, 2, 8000, 0.7f, 0.35f, 0.42f * kd);
            r.gain = 0.42f; r.choke = 1; r.length = 1.5f;
            break;
        case Crash:
            metal (r, 1.5f * hat * keyTune, 0.8f, 1.9f * kd, 8000, 4500);
            noise (r, 2, 5500, 0.7f, 0.7f, 1.7f * kd, 0.002f);
            r.gain = 0.42f; r.length = 3.5f;
            break;
        case Splash:
            metal (r, 1.9f * hat, 0.8f, 0.75f * kd, 9500, 6000);
            noise (r, 2, 7000, 0.7f, 0.6f, 0.65f * kd);
            r.gain = 0.4f; r.length = 2.0f;
            break;
        case China:
            metal (r, 1.2f * hat, 1.0f, 1.4f * kd, 5000, 3000);
            noise (r, 1, 4500, 0.8f, 0.6f, 1.2f * kd);
            r.gain = 0.42f; r.drive = 0.6f; r.length = 3.0f;
            break;
        case Ride:
            metal (r, 1.3f * hat * keyTune, 0.45f, 2.2f * kd, 7500, 5500);
            partials (r, 2900.0f * keyTune, { 1.0f, 1.37f }, { 0.35f, 0.2f }, { 0.5f, 0.35f });
            noise (r, 2, 7000, 0.7f, 0.2f, 0.25f);
            r.gain = 0.38f; r.length = 3.5f;
            break;
        case RideBell:
            partials (r, 1250.0f, { 1.0f, 2.71f, 4.32f }, { 1.0f, 0.55f, 0.3f }, { 1.4f * kd, 0.9f, 0.5f });
            metal (r, 1.4f * hat, 0.25f, 1.0f, 7000, 5000);
            r.clickAmp = 0.3f; r.clickFreq = 5000;
            r.gain = 0.35f; r.length = 2.5f;
            break;
        case Tambourine:
            metal (r, 3.1f * pt, 0.6f, 0.22f * pd, 9000, 6500);
            noise (r, 2, 7000, 0.7f, 0.7f, 0.25f * pd, 0.003f);
            r.bursts = 2; r.burstSpacing = 0.018f; r.burstT60 = 0.06f;
            r.gain = 0.5f;
            break;
        case Cowbell:
            r.metalCount = 2; r.metalFreq[0] = 540.0f * pt; r.metalFreq[1] = 800.0f * pt;
            r.metalAmp = 1.0f; r.metalT60 = 0.35f * pd; r.metalBpFc = 1700; r.metalHpFc = 450;
            r.clickAmp = 0.2f; r.clickFreq = 2600;
            r.gain = 0.55f;
            break;
        case BongoHi:
        case BongoLo:
        {
            float f = (type == BongoHi ? 420.0f : 300.0f) * pt;
            partials (r, f, { 1.0f, 1.55f }, { 1.0f, 0.3f }, { 0.2f * pd, 0.08f });
            r.pitchDrop = 0.15f; r.pitchT60 = 0.04f;
            noise (r, 1, 2200, 1.0f, 0.3f, 0.02f);
            r.clickAmp = 0.3f; r.clickFreq = 3000;
            r.gain = 0.5f;
            break;
        }
        case CongaMute:
            partials (r, 335.0f * pt, { 1.0f, 1.6f }, { 1.0f, 0.4f }, { 0.07f, 0.04f });
            noise (r, 1, 2500, 0.9f, 0.55f, 0.04f);
            r.clickAmp = 0.4f; r.clickFreq = 3500;
            r.gain = 0.5f;
            break;
        case CongaOpen:
        case CongaLow:
        {
            float f = (type == CongaOpen ? 280.0f : 205.0f) * pt;
            partials (r, f, { 1.0f, 1.52f, 2.3f }, { 1.0f, 0.25f, 0.1f }, { 0.4f * pd, 0.18f, 0.08f });
            r.pitchDrop = 0.08f; r.pitchT60 = 0.03f;
            noise (r, 1, 1800, 0.9f, 0.25f, 0.025f);
            r.clickAmp = 0.25f; r.clickFreq = 2500;
            r.gain = 0.56f;
            break;
        }
        case TimbaleHi:
        case TimbaleLo:
        {
            float f = (type == TimbaleHi ? 525.0f : 385.0f) * pt;
            partials (r, f, { 1.0f, 1.62f, 2.31f }, { 1.0f, 0.6f, 0.35f }, { 0.45f * pd, 0.3f * pd, 0.2f });
            r.pitchDrop = 0.06f; r.pitchT60 = 0.02f;
            noise (r, 1, 3200, 1.0f, 0.45f, 0.04f);
            metal (r, 2.4f * pt, 0.12f, 0.25f * pd, 6000, 3500, 4);
            r.clickAmp = 0.55f; r.clickFreq = 3800; r.clickT60 = 0.003f;
            r.gain = 0.65f; r.drive = 0.2f;
            break;
        }
        case TimbaleRim:
            partials (r, 1800.0f * pt, { 1.0f, 1.61f }, { 1.0f, 0.6f }, { 0.07f, 0.05f });
            noise (r, 1, 4200, 1.5f, 0.5f, 0.035f);
            r.clickAmp = 0.8f; r.clickFreq = 5000; r.clickT60 = 0.002f;
            r.gain = 0.36f;
            break;
        case AgogoHi:
        case AgogoLo:
        {
            float f = (type == AgogoHi ? 920.0f : 690.0f) * pt;
            partials (r, f, { 1.0f, 2.76f, 5.4f }, { 1.0f, 0.45f, 0.2f }, { 0.6f * pd, 0.3f, 0.15f });
            r.clickAmp = 0.3f; r.clickFreq = 4000;
            r.gain = 0.45f;
            break;
        }
        case Cabasa:
            noise (r, 2, 7000, 0.7f, 1.0f, 0.08f * pd, 0.006f);
            r.gain = 0.45f;
            break;
        case Maracas:
            noise (r, 1, 5000, 1.6f, 1.0f, 0.06f * pd, 0.008f);
            r.gain = 0.55f;
            break;
        case Shaker:
            noise (r, 1, 6500, 1.1f, 1.0f, 0.12f * pd, 0.022f);
            r.gain = 0.5f;
            break;
        case GuiroShort:
        case GuiroLong:
            noise (r, 1, 3400 * pt, 2.4f, 1.0f, 0.02f);
            r.bursts = type == GuiroShort ? 9 : 26;
            r.burstSpacing = 0.0115f; r.burstT60 = 0.009f; r.burstJitter = 0.15f;
            r.gain = 0.34f; r.length = 1.0f;
            break;
        case Claves:
            partials (r, 2450.0f * pt, { 1.0f }, { 1.0f }, { 0.12f * pd });
            r.clickAmp = 0.2f; r.clickFreq = 5000;
            r.gain = 0.5f;
            break;
        case WoodHi:
        case WoodLo:
        {
            float f = (type == WoodHi ? 1050.0f : 780.0f) * pt;
            partials (r, f, { 1.0f, 2.4f }, { 1.0f, 0.4f }, { 0.09f, 0.05f });
            noise (r, 1, f * 1.8f, 4.0f, 0.3f, 0.03f);
            r.clickAmp = 0.3f; r.clickFreq = 4000;
            r.gain = 0.42f;
            break;
        }
        case TriangleMute:
        case TriangleOpen:
            partials (r, 1850.0f * pt, { 1.0f, 2.98f, 5.1f }, { 0.8f, 1.0f, 0.5f },
                      { type == TriangleOpen ? 1.6f : 0.12f, type == TriangleOpen ? 1.2f : 0.1f, 0.6f * (type == TriangleOpen ? 1.0f : 0.15f) });
            r.clickAmp = 0.15f; r.clickFreq = 6000;
            r.gain = 0.25f; r.choke = 2; r.length = 2.5f;
            break;
        case NyaBass:
            partials (r, 58.0f * pt, { 1.0f, 1.47f, 2.1f }, { 1.0f, 0.3f, 0.1f }, { 0.9f * pd, 0.4f, 0.2f });
            r.pitchDrop = 0.5f; r.pitchT60 = 0.06f;
            noise (r, 3, 800, 0.7f, 0.3f, 0.05f);
            r.gain = 1.0f; r.drive = 0.3f; r.length = 2.0f;
            break;
        case NyaFunde:
            partials (r, 135.0f * pt, { 1.0f, 1.6f }, { 1.0f, 0.3f }, { 0.35f * pd, 0.15f });
            r.pitchDrop = 0.2f; r.pitchT60 = 0.04f;
            noise (r, 1, 900, 0.8f, 0.35f, 0.04f);
            r.gain = 0.55f;
            break;
        case NyaRepeater:
        case NyaRepeaterSlap:
        {
            bool slap = type == NyaRepeaterSlap;
            partials (r, 310.0f * pt, { 1.0f, 1.58f, 2.2f }, { 1.0f, 0.5f, 0.25f },
                      { (slap ? 0.09f : 0.4f) * pd, slap ? 0.06f : 0.2f, 0.1f });
            r.pitchDrop = 0.08f; r.pitchT60 = 0.025f;
            noise (r, 1, 2500, 1.0f, slap ? 0.8f : 0.3f, slap ? 0.05f : 0.03f);
            r.clickAmp = slap ? 0.6f : 0.3f; r.clickFreq = 3000;
            r.gain = slap ? 0.36f : 0.45f;
            break;
        }
        case Syndrum:
            partials (r, 175.0f * keyTune * semis (tw.tomTune), { 1.0f }, { 1.0f }, { 0.65f * kd });
            r.pitchDrop = 4.0f; r.pitchT60 = 0.4f;
            noise (r, 3, 3000, 0.7f, 0.08f, 0.1f);
            r.gain = 0.7f; r.length = 1.5f;
            break;
        case DubSiren:  r.special = 1; r.length = 1.9f; r.gain = 0.32f; r.choke = 3; break;
        case AirHorn:   r.special = 2; r.length = 0.75f; r.gain = 0.3f; r.choke = 4; break;
        case Laser:     r.special = 3; r.length = 0.45f; r.gain = 0.4f; break;
        default: break;
    }
}

//==============================================================================
void DrumVoice::start (const DrumRecipe& recipe, float velocity, int delaySamples, uint32_t a, int n)
{
    r = recipe;
    vel = velocity;
    delay = std::max (0, delaySamples);
    age = a;
    note = n;
    chokeGroup = r.choke;
    active = true;
    started = false;
    choking = false;
    chokeGain = 1;
    rng.seed ((uint32_t) (a * 2654435761u + (uint32_t) n * 40503u + 7u));
    if (delay == 0) begin();
}

void DrumVoice::begin()
{
    started = true;
    t = 0;
    maxLen = (int) (r.length * sr);
    chokeK = t60Coef (0.025f, sr);
    panGains (r.pan, gl, gr);

    for (int i = 0; i < r.nPartials; ++i) { ph[i] = 0; env[i] = r.amp[i]; envK[i] = t60Coef (r.t60[i], sr); }
    pEnv = 1.0f; pK = t60Coef (r.pitchT60, sr);

    nEnv = r.noiseAmp; nK = t60Coef (r.noiseT60, sr);
    nAtk = r.noiseAttack > 0 ? 0.0f : 1.0f;
    nAtkInc = r.noiseAttack > 0 ? 1.0f / (r.noiseAttack * sr) : 1.0f;
    nf.reset(); nf.set (r.noiseFc, r.noiseQ, sr);

    for (int i = 0; i < r.metalCount; ++i) { mPh[i] = rng.uni(); mInc[i] = r.metalFreq[i] / sr; }
    mEnv = r.metalAmp; mK = t60Coef (r.metalT60, sr);
    mbp.reset(); mbp.set (r.metalBpFc, 0.9f, sr);
    mhp.reset(); mhp.set (r.metalHpFc, 0.7f, sr);

    cEnv = r.clickAmp; cK = t60Coef (r.clickT60, sr); cPh = 0;

    burstLeft = r.bursts > 0 ? r.bursts - 1 : 0;
    burstInterval = (int) (r.burstSpacing * sr);
    burstTimer = burstInterval;
    burstK = t60Coef (r.burstT60, sr);
    tailK = nK;
    if (r.bursts > 0) nK = burstK;

    so1 = Osc(); so2 = Osc(); so3 = Osc(); sf.reset(); lfoPh = 0;
    if (r.special == 1) sf.set (2400, 0.9f, sr);
    if (r.special == 2) sf.set (1400, 1.2f, sr);
}

float DrumVoice::special (float ts)
{
    switch (r.special)
    {
        case 1: // dub siren: rising pitch, accelerating wobble
        {
            float base = 520.0f * std::pow (2.0f, ts * 0.55f);
            float rate = 3.0f + 4.5f * ts;
            lfoPh += rate / sr; if (lfoPh >= 1) lfoPh -= 1;
            float lfo = lfoPh < 0.5f ? 4 * lfoPh - 1 : 3 - 4 * lfoPh;
            so1.setFreq (base * (1.0f + 0.32f * lfo), sr);
            float s = 0.7f * so1.pulse (0.5f);
            s = sf.lp (s);
            float a = std::min (1.0f, ts / 0.01f) * std::min (1.0f, (r.length - ts) / 0.25f);
            return s * std::max (0.0f, a);
        }
        case 2: // air horn: detuned cluster with flutter
        {
            float f = 233.0f * (1.0f - 0.07f * std::exp (-ts * 30.0f));
            so1.setFreq (f, sr); so2.setFreq (f * 1.012f, sr); so3.setFreq (f * 1.19f, sr);
            float s = so1.saw() + so2.saw() + 0.8f * so3.saw();
            lfoPh += 31.0f / sr; if (lfoPh >= 1) lfoPh -= 1;
            float am = 0.72f + 0.28f * fastSin (lfoPh);
            float b = sf.bp (s) * 1.6f + s * 0.25f;
            float a = std::min (1.0f, ts / 0.015f) * std::min (1.0f, (r.length - ts) / 0.15f);
            return std::tanh (b * am * 0.6f) * std::max (0.0f, a);
        }
        case 3: // laser zap
        {
            float f = 3200.0f * std::exp (-ts * 12.0f) + 180.0f;
            lfoPh += 40.0f / sr; if (lfoPh >= 1) lfoPh -= 1;
            so1.setFreq (f * (1.0f + 0.05f * fastSin (lfoPh)), sr);
            float s = so1.pulse (0.5f) * 0.5f + fastSin (so1.ph) * 0.5f;
            float a = std::min (1.0f, ts / 0.003f) * std::pow (0.001f, ts / r.length);
            return s * a;
        }
        default: return 0;
    }
}

bool DrumVoice::render (float* kitL, float* kitR, float* percL, float* percR, int n)
{
    if (! active) return false;
    float* L = r.percBus ? percL : kitL;
    float* R = r.percBus ? percR : kitR;
    const float v = vel * vel * 0.65f + vel * 0.35f;
    const float invSr = 1.0f / sr;

    for (int i = 0; i < n; ++i)
    {
        if (! started)
        {
            if (delay > 0) { --delay; continue; }
            begin();
        }

        float out = 0;

        if (r.nPartials > 0)
        {
            float fm = 1.0f + r.pitchDrop * pEnv;
            pEnv *= pK;
            for (int p = 0; p < r.nPartials; ++p)
            {
                ph[p] += r.freq[p] * fm * invSr;
                if (ph[p] >= 1.0f) ph[p] -= std::floor (ph[p]);
                out += fastSin (ph[p]) * env[p];
                env[p] *= envK[p];
            }
        }

        if (r.noiseMode != 0)
        {
            if (r.bursts > 0 && burstLeft > 0 && --burstTimer <= 0)
            {
                --burstLeft;
                nEnv = r.noiseAmp * (0.8f + 0.4f * rng.uni());
                burstTimer = (int) ((float) burstInterval * (1.0f + r.burstJitter * rng.bi()));
                if (burstLeft == 0) nK = tailK;
            }
            float l, b, h;
            nf.tick (rng.bi(), l, b, h);
            float ns = r.noiseMode == 1 ? b * 1.6f : (r.noiseMode == 2 ? h : l);
            if (nAtk < 1.0f) { nAtk += nAtkInc; if (nAtk > 1.0f) nAtk = 1.0f; }
            out += ns * nEnv * nAtk;
            nEnv *= nK;
        }

        if (r.metalCount > 0)
        {
            float m = 0;
            for (int k = 0; k < r.metalCount; ++k)
            {
                mPh[k] += mInc[k]; if (mPh[k] >= 1.0f) mPh[k] -= 1.0f;
                m += mPh[k] < 0.5f ? 1.0f : -1.0f;
            }
            m /= (float) r.metalCount;
            m = mhp.hp (mbp.bp (m) * 2.0f);
            out += m * mEnv;
            mEnv *= mK;
        }

        if (r.clickAmp > 0)
        {
            cPh += r.clickFreq * invSr;
            out += (0.6f * fastSin (cPh) + 0.4f * rng.bi()) * cEnv;
            cEnv *= cK;
        }

        if (r.special != 0) out += special ((float) t * invSr);

        out *= r.gain * v;
        if (r.drive > 0) out = std::tanh (out * (1.0f + 2.0f * r.drive)) / (1.0f + r.drive);

        if (choking) { chokeGain *= chokeK; out *= chokeGain; }

        L[i] += out * gl;
        R[i] += out * gr;

        ++t;
        if (t > maxLen || chokeGain < 0.0005f) { active = false; return false; }
        if ((t & 255) == 0 && r.special == 0 && t > 256)
        {
            float level = nEnv + mEnv + cEnv;
            for (int p = 0; p < r.nPartials; ++p) level += env[p];
            if (level < 0.0002f && burstLeft == 0) { active = false; return false; }
        }
    }
    return true;
}

//==============================================================================
void DrumKit::prepare (float sampleRate)
{
    sr = sampleRate;
    sineTable();
    for (auto& v : voices) { v.prepare (sr); v.active = false; }
}

void DrumKit::trigger (int note, float velocity, int delaySamples, const DrumTweaks& tw)
{
    const auto& m = drumMap (note);
    if (m.type < 0) return;

    DrumRecipe rec;
    buildDrumRecipe (m.type, m.tune, tw, rec);
    rec.pan = m.pan;
    rec.percBus = m.percBus;

    if (rec.choke != 0)
        for (auto& v : voices)
            if (v.active && v.chokeGroup == rec.choke) v.choke();

    // re-hitting the same note: let the old one fade quickly to avoid build-up
    int sameCount = 0;
    for (auto& v : voices) if (v.active && v.note == note) ++sameCount;
    if (sameCount >= 3)
        for (auto& v : voices) if (v.active && v.note == note) { v.choke(); break; }

    DrumVoice* target = nullptr;
    for (auto& v : voices) if (! v.active) { target = &v; break; }
    if (target == nullptr)
    {
        uint32_t oldest = UINT32_MAX;
        for (auto& v : voices) if (v.age < oldest) { oldest = v.age; target = &v; }
    }
    target->start (rec, clampf (velocity, 0.0f, 1.0f), delaySamples, ++counter, note);
}

void DrumKit::render (float* kitL, float* kitR, float* percL, float* percR, int n)
{
    for (auto& v : voices)
        if (v.active) v.render (kitL, kitR, percL, percR, n);
}

void DrumKit::allOff()
{
    for (auto& v : voices) if (v.active) v.choke();
}

} // namespace rf
