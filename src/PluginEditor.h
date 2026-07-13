#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "gui/LookAndFeel.h"

class ChimeraEngineAudioProcessor;

class ChimeraEngineAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ChimeraEngineAudioProcessorEditor(ChimeraEngineAudioProcessor&);
    ~ChimeraEngineAudioProcessorEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    juce::Component* makeOverviewPage();
    juce::Component* makeElementPage();
    juce::Component* makeFxPage();
    juce::Component* makeArpPage();
    juce::Component* makePresetPage();
    juce::Component* makeCreditsPage();
    juce::Slider& addKnob(juce::Component& parent, const juce::String& name);

    ChimeraEngineAudioProcessor& owner;
    chimera::gui::LookAndFeel lookAndFeel;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Label title;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<SliderAttachment> sliderAttachments;
    juce::OwnedArray<ButtonAttachment> buttonAttachments;
    juce::ToggleButton arpToggle;
};
