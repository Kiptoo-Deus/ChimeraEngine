#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "gui/LookAndFeel.h"
#include <memory>

class ChimeraEngineAudioProcessor;

class ChimeraEngineAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                private juce::MidiKeyboardState::Listener,
                                                private juce::Timer
{
public:
    explicit ChimeraEngineAudioProcessorEditor(ChimeraEngineAudioProcessor&);
    ~ChimeraEngineAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    enum class WorkstationPage
    {
        Voice,
        Performance,
        Mix,
        Arp,
        Song,
        Pattern,
        Sample,
        Utility,
        Demo
    };

private:
    class GraphicalEditor;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    enum class PanelId
    {
        Header,
        Display,
        Modes,
        Tone,
        Envelope,
        Effects,
        Performance,
        Presets,
        Mixer,
        ElementMonitor
    };

    juce::Slider& addKnob(juce::Component& parent, const juce::String& name);
    juce::TextButton& addPresetButton(juce::Component& parent, const juce::String& presetName);
    juce::TextButton& addModeButton(const juce::String& name);
    juce::Label& addPanelLabel(const juce::String& text, juce::Justification justification = juce::Justification::centredLeft);
    void addPageSurfaceControls();
    void setActivePage(WorkstationPage page);
    void refreshPageSurface();
    void setPageSurfaceVisible(bool shouldBeVisible);
    void addPartMixerControls();
    void refreshPartMixerControls();
    void addFxControls();
    void refreshFxControls();
    void addTransportControls();
    void refreshTransportControls();
    void drawPanel(juce::Graphics& g, PanelId panel, const juce::String& title);
    void updateDisplay();
    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void timerCallback() override;

    ChimeraEngineAudioProcessor& owner;
    chimera::gui::LookAndFeel lookAndFeel;
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };
    juce::Label title;
    juce::Label subtitle;
    juce::Label patchDisplay;
    juce::Label categoryDisplay;
    juce::Label lcdLine;
    juce::OwnedArray<juce::TextButton> modeButtons;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::TextButton> presetButtons;
    juce::OwnedArray<juce::ToggleButton> partEnableButtons;
    juce::OwnedArray<juce::Slider> partLevelSliders;
    juce::OwnedArray<juce::Slider> partPanSliders;
    juce::OwnedArray<juce::ComboBox> fxInsertBoxes;
    juce::OwnedArray<juce::Slider> fxSendSliders;
    juce::OwnedArray<juce::Slider> masterFxSliders;
    juce::OwnedArray<juce::Label> pageLabels;
    juce::OwnedArray<juce::TextButton> pageActionButtons;
    juce::OwnedArray<SliderAttachment> sliderAttachments;
    juce::OwnedArray<ButtonAttachment> buttonAttachments;
    juce::TextEditor presetSearch;
    juce::ComboBox presetCategoryBox;
    juce::ComboBox presetBrowserBox;
    juce::ComboBox performanceBrowserBox;
    juce::ComboBox sampleSlotBox;
    juce::ToggleButton favouriteToggle { "Fav" };
    juce::ToggleButton arpToggle;
    juce::TextButton demoSequenceButton { "Demo" };
    juce::TextButton sequencerPlayButton { "Play" };
    juce::TextButton sequencerResetButton { "Reset" };
    juce::ToggleButton mpeToggle { "MPE" };
    juce::Label sequencerTickLabel;
    std::unique_ptr<GraphicalEditor> graphicalEditor;

    juce::Rectangle<int> headerBounds;
    juce::Rectangle<int> displayBounds;
    juce::Rectangle<int> modeBounds;
    juce::Rectangle<int> toneBounds;
    juce::Rectangle<int> envelopeBounds;
    juce::Rectangle<int> effectsBounds;
    juce::Rectangle<int> performanceBounds;
    juce::Rectangle<int> presetBounds;
    juce::Rectangle<int> mixerBounds;
    juce::Rectangle<int> elementMonitorBounds;
    WorkstationPage activePage = WorkstationPage::Demo;
};
