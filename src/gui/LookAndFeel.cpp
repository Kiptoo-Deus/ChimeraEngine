#include "gui/LookAndFeel.h"

namespace chimera::gui
{
LookAndFeel::LookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff101317));
    setColour(juce::Slider::thumbColourId, juce::Colour(0xff2dd4bf));
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff2dd4bf));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff30363d));
    setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0d1117));
    setColour(juce::TabbedComponent::backgroundColourId, juce::Colour(0xff161b22));
    setColour(juce::Label::textColourId, juce::Colours::white);
    setColour(juce::ToggleButton::textColourId, juce::Colours::white);
}
}
