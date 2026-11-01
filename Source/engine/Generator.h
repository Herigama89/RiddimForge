// RiddimForge - pattern generator (drums with fills, bass, skank, organ, piano, lead), 1-16 bars
#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace rf
{
enum GenPart { GPDrums, GPBass, GPSkank, GPOrgan, GPPiano, GPLead, kNumGenParts };
const char* genPartName (int p);
int genPartToRiddimPart (int p);   // GenPart -> PartId

enum GenEventKind
{
    GKDrum,       // note = drum note
    GKChord,      // note = voicing window low note (all chord tones)
    GKChordTone,  // note = voicing window low note, extra = chord tone index (arpeggio)
    GKBass,       // note = bass token (see BassToken), extra = octave offset
    GKLead        // note = semitones from key root
};

enum BassToken { BTRoot, BTThird, BTFifth, BTOctave, BTSeventh, BTSixth, BTApproachBelow, BTApproachAbove, BTSecond, BTFourth };

struct GenEvent
{
    int step = 0;          // 16th step inside the phrase
    int kind = GKDrum;
    int note = 36;
    int extra = 0;
    float vel = 0.8f;
    float gate = 1.0f;     // 16th steps
    float offset = 0.0f;   // 16th steps (flams / pushes), >= 0
};

struct Phrase
{
    int bars = 4;
    std::array<bool, kNumGenParts> has {};
    std::array<std::vector<GenEvent>, kNumGenParts> events;
    std::array<std::vector<std::vector<int>>, kNumGenParts> byStep; // indices into events, per step
    void index();
    int steps() const { return bars * 16; }
};

struct GenSettings
{
    int bars = 4;                 // 1, 2, 4, 8, 16
    int riddimStyle = 0;          // used when a drum/bass type follows the riddim style
    bool minor = true;
    int fillEvery = 2;            // 0 off, 1 every 2 bars, 2 every 4 bars, 3 phrase end only
    float fillIntensity = 0.5f;   // 0..1
    std::array<int, kNumGenParts> type {};
    std::array<float, kNumGenParts> complexity { 0.5f, 0.5f, 0.4f, 0.4f, 0.4f, 0.5f };
    std::array<uint32_t, kNumGenParts> seed { 1, 2, 3, 4, 5, 6 };
};

int numGenTypes (int part);
const char* genTypeName (int part, int type);

// Generate one part into an existing phrase (phrase.bars must already be set)
void generatePart (int part, const GenSettings& s, Phrase& out);
// Generate every part
void generateAll (const GenSettings& s, Phrase& out);

} // namespace rf
