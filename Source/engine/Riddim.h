// RiddimForge - riddim pattern sequencer and chord engine
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "Generator.h"

namespace rf
{
enum PartId { PartDrums, PartPerc, PartSkank, PartPiano, PartOrgan, PartBass, PartLead, PartPad, kNumParts };
const char* partName (int part);

enum ChordQuality { QMaj, QMin, QDom7, QMin7, QSus4, QDim, QMaj7, kNumQualities };

struct Chord
{
    int root = 9;          // pitch class 0..11 (C = 0)
    int quality = QMin;
};

std::string chordName (const Chord& c);
std::string noteName (int pitchClass);
void chordIntervals (int quality, std::vector<int>& out);
// Close voicing with every chord tone inside [lo, lo + 12)
void voiceChord (const Chord& c, int lo, std::vector<int>& out);
// Detect a chord from held MIDI notes. Returns false if nothing is held.
bool detectChord (const std::array<bool, 128>& held, Chord& out);

int numStyles();
const char* styleName (int style);
int numProgressions();
const char* progressionName (int prog);

struct RiddimSettings
{
    int style = 0;
    int key = 9;              // A
    int progression = 5;      // i - iv
    int barsPerChord = 1;
    bool chordFromMidi = false;
    float swing = 0.2f;       // 0..1
    float humanize = 0.3f;    // 0..1
    int fills = 1;            // 0 off, 1 every 4 bars, 2 every 8 bars
    std::array<bool, kNumParts> partOn { true, true, true, true, true, true, false, false };
    // generated phrase (Generator tab); parts flagged in genUse play from it instead of the style
    const Phrase* phrase = nullptr;
    std::array<bool, kNumGenParts> genUse {};
};

struct SeqEvent
{
    int part = 0;             // PartId
    int note = 60;
    float vel = 0.8f;
    float gateSteps = 1.0f;   // in 16th steps (ignored for drums)
    float offsetSteps = 0.0f; // swing (in 16th steps, >= 0)
    float offsetSec = 0.0f;   // strum / humanize (seconds, >= 0)
};

Chord progressionChord (const RiddimSettings& s, int64_t bar);

// style drum lanes (for the generator)
struct StyleLaneInfo { int note; std::string pattern; };
void styleDrumLanes (int style, std::vector<StyleLaneInfo>& out);
int styleFillType (int style);

// All events for global 16th-note step k (k >= 0), 4/4 time, 16 steps per bar
void generateStep (int64_t k, const RiddimSettings& s, const Chord& chord, std::vector<SeqEvent>& out);
void generateStep (int64_t k, const RiddimSettings& s, const Chord& chord, const Chord& nextChord, std::vector<SeqEvent>& out);

} // namespace rf
