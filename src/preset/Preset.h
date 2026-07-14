#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace chimera::preset
{
struct EnvelopeDefinition
{
    float attack = 0.0f;
    float decay1 = 0.05f;
    float decay2 = 0.05f;
    float sustain = 1.0f;
    float release = 0.0f;
    float depth = 0.0f;
};

struct ModSlotDefinition
{
    juce::String source { "velocity" };
    juce::String destination { "amp" };
    float depth = 0.0f;
    bool enabled = false;
};

struct ElementDefinition
{
    juce::String samplePath;
    std::vector<juce::String> alternateSamples;
    std::vector<juce::String> releaseSamples;
    juce::String alternateMode { "off" };
    int rootKey = 60;
    int keyLow = 0;
    int keyHigh = 127;
    int velocityLow = 1;
    int velocityHigh = 127;
    float level = 1.0f;
    float pan = 0.0f;
    float tuningCents = 0.0f;
    juce::String filterType { "lowPass12" };
    float ampAttack = 0.0f;
    float ampDecay1 = 0.05f;
    float ampDecay2 = 0.05f;
    float ampSustain = 1.0f;
    float ampRelease = 0.0f;
    EnvelopeDefinition pitchEnvelope;
    EnvelopeDefinition filterEnvelope;
    float lfo1RateHz = 0.0f;
    float lfo1CutoffDepth = 0.0f;
    float lfo2RateHz = 0.0f;
    float lfo2AmpDepth = 0.0f;
    float lfo2PanDepth = 0.0f;
    std::vector<ModSlotDefinition> modSlots;
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
    juce::String voiceMode { "poly" };
    float portamentoTime = 0.0f;
    int pitchBendRange = 2;
    std::vector<ElementDefinition> elements;
};

juce::var toJson(const PresetMetadata& metadata);
juce::Result loadPatch(const juce::File& file, Patch& patch);
juce::Result validatePatchSamples(const juce::File& projectRoot, const Patch& patch);
}
