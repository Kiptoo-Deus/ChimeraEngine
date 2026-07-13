#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "gui/LookAndFeel.h"

class ChimeraEngineAudioProcessor;

class ChimeraEngineAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                private juce::MidiKeyboardState::Listener
{
public:
    explicit ChimeraEngineAudioProcessorEditor(ChimeraEngineAudioProcessor&);
    ~ChimeraEngineAudioProcessorEditor() override;

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
    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

    ChimeraEngineAudioProcessor& owner;
    chimera::gui::LookAndFeel lookAndFeel;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };
    juce::Label title;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<SliderAttachment> sliderAttachments;
    juce::OwnedArray<ButtonAttachment> buttonAttachments;
    juce::ToggleButton arpToggle;
};
