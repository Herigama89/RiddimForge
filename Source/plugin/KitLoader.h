#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "DrumSampler.h"
#include "SampledInstrument.h"

namespace kitloader
{
// Decode the kits embedded in the plugin binary (kits.json + FLAC files)
std::vector<std::unique_ptr<rf::Kit>> loadEmbeddedKits();

// Decode the recorded instruments embedded in the plugin (instruments.json + FLAC files)
std::vector<std::unique_ptr<rf::InstrumentSet>> loadEmbeddedInstruments();

// Load a WAV/AIFF/FLAC/etc. chosen by the user to replace one drum slot
std::unique_ptr<rf::KitSample> loadUserSample (const juce::File& file, juce::String& error);

bool isSupportedAudioFile (const juce::String& path);
} // namespace kitloader
