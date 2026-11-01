// RiddimForge - melodic instruments (skank guitar, piano, organ, bass, lead, pad)
#pragma once
#include "Dsp.h"
#include <memory>

namespace rf
{
struct InstrCtx
{
    float sr = 44100;
    float bend = 0;     // semitones
    float mod = 0;      // 0..1 (mod wheel)
    int preset = 0;     // instrument-specific sound type
    float tone = 0.5f;  // 0..1 brightness
    float extra = 0.5f; // instrument-specific (skank mute, organ rotary...)
};

class InstrumentBase
{
public:
    virtual ~InstrumentBase() = default;
    virtual void prepare (float sampleRate) = 0;
    // gateSamples < 0 : hold until noteOff. delaySamples : start later inside/after this block
    virtual void noteOn (int note, float velocity, int gateSamples = -1, int delaySamples = 0) = 0;
    virtual void noteOff (int note) = 0;
    virtual void setPedal (bool down) = 0;
    virtual void allOff() = 0;
    virtual void render (float* L, float* R, int n) = 0;
    virtual int activeVoices() const = 0;
    InstrCtx ctx;
};

std::unique_ptr<InstrumentBase> makeSkankGuitar();
std::unique_ptr<InstrumentBase> makePiano();
std::unique_ptr<InstrumentBase> makeOrgan();
std::unique_ptr<InstrumentBase> makeBass();
std::unique_ptr<InstrumentBase> makeLead();
std::unique_ptr<InstrumentBase> makePad();

} // namespace rf
