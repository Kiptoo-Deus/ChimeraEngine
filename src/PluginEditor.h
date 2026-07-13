#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class ChimeraEngineAudioProcessor;

class ChimeraEngineAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ChimeraEngineAudioProcessorEditor(ChimeraEngineAudioProcessor&);
    ~ChimeraEngineAudioProcessorEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label title;
};
