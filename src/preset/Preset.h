#pragma once

#include <juce_core/juce_core.h>

namespace chimera::preset
{
struct PresetMetadata
{
    juce::String name;
    juce::String category;
};

juce::var toJson(const PresetMetadata& metadata);
}
