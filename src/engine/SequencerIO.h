#pragma once

#include "engine/Sequencer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace chimera::engine
{
juce::Result exportSongToMidiFile(const Song& song, const juce::File& file);
juce::Result importSongFromMidiFile(const juce::File& file, Song& song);
}
