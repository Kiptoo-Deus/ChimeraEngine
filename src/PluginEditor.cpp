#include "PluginEditor.h"
#include "PluginProcessor.h"

ChimeraEngineAudioProcessorEditor::ChimeraEngineAudioProcessorEditor(ChimeraEngineAudioProcessor& p)
    : AudioProcessorEditor(&p)
{
    title.setText("Chimera Engine", juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centred);
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);
    setSize(900, 560);
}

void ChimeraEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101317));
    g.setColour(juce::Colour(0xff2dd4bf));
    g.drawRect(getLocalBounds(), 2);
}

void ChimeraEngineAudioProcessorEditor::resized()
{
    title.setBounds(getLocalBounds().removeFromTop(64));
}
