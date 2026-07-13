#include "gui/LookAndFeel.h"

namespace chimera::gui
{
LookAndFeel::LookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff101317));
    setColour(juce::Slider::thumbColourId, juce::Colour(0xff2dd4bf));
}
}
