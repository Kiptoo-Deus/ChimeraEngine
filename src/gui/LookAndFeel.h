#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace chimera::gui
{
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override;
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float minSliderPos, float maxSliderPos, const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                      int buttonW, int buttonH, juce::ComboBox& box) override;
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;
    void fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& editor) override;
    void drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& editor) override;
};
}
