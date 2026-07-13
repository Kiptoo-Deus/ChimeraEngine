#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace chimera::preset
{
struct ElementDefinition
{
    juce::String samplePath;
    int rootKey = 60;
    int keyLow = 0;
    int keyHigh = 127;
    int velocityLow = 1;
    int velocityHigh = 127;
    float level = 1.0f;
    float pan = 0.0f;
};

struct PresetMetadata
{
    juce::String name;
    juce::String category;
};

struct Patch
{
    int version = 1;
    PresetMetadata metadata;
    std::vector<ElementDefinition> elements;
};

juce::var toJson(const PresetMetadata& metadata);
juce::Result loadPatch(const juce::File& file, Patch& patch);
juce::Result validatePatchSamples(const juce::File& projectRoot, const Patch& patch);
}
