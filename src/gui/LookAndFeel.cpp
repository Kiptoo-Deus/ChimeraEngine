#include "gui/LookAndFeel.h"

#include <cmath>

namespace chimera::gui
{
namespace
{
juce::Colour metalLight() { return juce::Colour(0xff4f5a66); }
juce::Colour displayGreen() { return juce::Colour(0xff6ee7b7); }
juce::Colour amber() { return juce::Colour(0xfff0b64b); }
juce::Colour blueGrey() { return juce::Colour(0xff8ea0b2); }
}

LookAndFeel::LookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff0b0d10));
    setColour(juce::Slider::thumbColourId, amber());
    setColour(juce::Slider::rotarySliderFillColourId, displayGreen());
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2a3037));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe7edf3));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff07090c));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff2f3943));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222832));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff315b65));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff080b0f));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffdce8f3));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3b4652));
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff080b0f));
    setColour(juce::TextEditor::textColourId, juce::Colour(0xffdce8f3));
    setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3b4652));
    setColour(juce::Label::textColourId, juce::Colour(0xffe7edf3));
    setColour(juce::ToggleButton::textColourId, juce::Colour(0xffe7edf3));
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                   float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height)).reduced(7.0f, 4.0f);
    const auto diameter = std::min(area.getWidth(), area.getHeight() - 18.0f);
    const auto knob = juce::Rectangle<float>(area.getCentreX() - diameter * 0.5f,
                                             area.getY() + 2.0f,
                                             diameter,
                                             diameter);
    const auto centre = knob.getCentre();
    const auto radius = knob.getWidth() * 0.5f;
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    for (int i = 0; i <= 10; ++i)
    {
        const auto tickAngle = rotaryStartAngle + static_cast<float>(i) / 10.0f * (rotaryEndAngle - rotaryStartAngle);
        const auto inner = centre + juce::Point<float>(std::cos(tickAngle - juce::MathConstants<float>::halfPi),
                                                       std::sin(tickAngle - juce::MathConstants<float>::halfPi)) * (radius + 3.0f);
        const auto outer = centre + juce::Point<float>(std::cos(tickAngle - juce::MathConstants<float>::halfPi),
                                                       std::sin(tickAngle - juce::MathConstants<float>::halfPi)) * (radius + 8.0f);
        g.setColour(i % 5 == 0 ? metalLight() : juce::Colour(0xff3a424b));
        g.drawLine({ inner, outer }, i % 5 == 0 ? 1.1f : 0.7f);
    }

    juce::ColourGradient shadow(juce::Colour(0xff050607), knob.getCentreX(), knob.getBottom(),
                                juce::Colour(0x00000000), knob.getCentreX(), knob.getY(), false);
    g.setGradientFill(shadow);
    g.fillEllipse(knob.translated(0.0f, 3.0f));

    juce::ColourGradient body(juce::Colour(0xff56616d), knob.getX(), knob.getY(),
                              juce::Colour(0xff11161c), knob.getRight(), knob.getBottom(), false);
    body.addColour(0.45, juce::Colour(0xff2c343d));
    g.setGradientFill(body);
    g.fillEllipse(knob);
    g.setColour(juce::Colour(0xff07090c));
    g.drawEllipse(knob, 1.2f);
    g.setColour(juce::Colour(0xff7f8b98).withAlpha(0.45f));
    g.drawEllipse(knob.reduced(4.0f), 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle(-2.0f, -radius + 9.0f, 4.0f, radius * 0.52f, 1.8f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
    g.setColour(slider.isEnabled() ? amber() : blueGrey().withAlpha(0.55f));
    g.fillPath(pointer);

    auto nameArea = area.withTop(knob.getBottom() + 3.0f).withHeight(14.0f);
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.setColour(juce::Colour(0xffaeb9c4));
    g.drawText(slider.getName().toUpperCase(), nameArea, juce::Justification::centred);
}

void LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                   float minSliderPos, float maxSliderPos, const juce::Slider::SliderStyle style,
                                   juce::Slider& slider)
{
    juce::ignoreUnused(minSliderPos, maxSliderPos);
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                               static_cast<float>(width), static_cast<float>(height)).reduced(2.0f);
    const auto horizontal = style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar;
    const auto vertical = style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical;
    if (!horizontal && !vertical)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    if (vertical)
    {
        const auto track = bounds.withWidth(std::min(7.0f, bounds.getWidth() * 0.45f)).withCentre({ bounds.getCentreX(), bounds.getCentreY() });
        g.setColour(juce::Colour(0xff06080b));
        g.fillRoundedRectangle(track.expanded(2.0f, 1.0f), 3.0f);
        g.setColour(juce::Colour(0xff303943));
        g.fillRoundedRectangle(track, 3.0f);
        const auto fill = track.withTop(std::clamp(sliderPos, track.getY(), track.getBottom()));
        g.setColour(slider.findColour(juce::Slider::trackColourId).withAlpha(0.92f));
        g.fillRoundedRectangle(fill, 3.0f);

        const auto cap = juce::Rectangle<float>(bounds.getCentreX() - 10.0f, sliderPos - 5.0f, 20.0f, 10.0f);
        juce::ColourGradient capGradient(juce::Colour(0xffe0e6ed), cap.getX(), cap.getY(),
                                         juce::Colour(0xff5b6570), cap.getX(), cap.getBottom(), false);
        g.setGradientFill(capGradient);
        g.fillRoundedRectangle(cap, 2.0f);
        g.setColour(juce::Colour(0xff14181d));
        g.drawRoundedRectangle(cap, 2.0f, 1.0f);
        return;
    }

    const auto track = bounds.withHeight(std::min(7.0f, bounds.getHeight() * 0.45f)).withCentre({ bounds.getCentreX(), bounds.getCentreY() });
    g.setColour(juce::Colour(0xff06080b));
    g.fillRoundedRectangle(track.expanded(1.0f, 2.0f), 3.0f);
    g.setColour(juce::Colour(0xff313a44));
    g.fillRoundedRectangle(track, 3.0f);
    const auto fill = track.withRight(std::clamp(sliderPos, track.getX(), track.getRight()));
    g.setColour(slider.findColour(juce::Slider::trackColourId).withMultipliedSaturation(0.85f));
    g.fillRoundedRectangle(fill, 3.0f);

    const auto cap = juce::Rectangle<float>(sliderPos - 7.0f, bounds.getCentreY() - 9.0f, 14.0f, 18.0f);
    juce::ColourGradient capGradient(juce::Colour(0xffd8dee6), cap.getX(), cap.getY(),
                                     juce::Colour(0xff59636f), cap.getRight(), cap.getBottom(), false);
    g.setGradientFill(capGradient);
    g.fillRoundedRectangle(cap, 2.5f);
    g.setColour(juce::Colour(0xff14181d));
    g.drawRoundedRectangle(cap, 2.5f, 1.0f);

    if (slider.getName().isNotEmpty() && bounds.getHeight() > 18.0f)
    {
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.setColour(blueGrey());
        g.drawText(slider.getName().toUpperCase(), bounds.withHeight(11.0f), juce::Justification::centredLeft);
    }
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                       bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const auto on = button.getToggleState();
    const auto base = on ? juce::Colour(0xff285b62) : backgroundColour;
    juce::ColourGradient gradient(base.brighter(shouldDrawButtonAsHighlighted ? 0.18f : 0.08f), bounds.getX(), bounds.getY(),
                                  base.darker(shouldDrawButtonAsDown ? 0.55f : 0.32f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(shouldDrawButtonAsDown || on ? displayGreen().withAlpha(0.7f) : juce::Colour(0xff48525d));
    g.drawRoundedRectangle(bounds, 3.0f, on ? 1.4f : 1.0f);
    g.setColour(juce::Colour(0xffffffff).withAlpha(0.08f));
    g.drawLine(bounds.getX() + 2.0f, bounds.getY() + 1.0f, bounds.getRight() - 2.0f, bounds.getY() + 1.0f);
}

void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const auto led = bounds.removeFromLeft(std::min(22.0f, bounds.getHeight())).reduced(4.0f);
    g.setColour(juce::Colour(0xff050608));
    g.fillEllipse(led.expanded(2.0f));
    g.setColour(button.getToggleState() ? displayGreen() : juce::Colour(0xff303741));
    g.fillEllipse(led);
    g.setColour(button.getToggleState() ? displayGreen().brighter(0.5f) : juce::Colour(0xff5b6570));
    g.drawEllipse(led, 1.0f);

    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(button.getToggleState() ? juce::Colour(0xffe7fff7) : blueGrey());
    g.drawText(button.getButtonText().toUpperCase(), bounds, juce::Justification::centredLeft);
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                               int buttonW, int buttonH, juce::ComboBox& box)
{
    juce::ignoreUnused(buttonX, buttonY, buttonW, buttonH);
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
    juce::ColourGradient gradient(juce::Colour(0xff11161c), bounds.getX(), bounds.getY(),
                                  juce::Colour(0xff07090c), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId).withAlpha(isButtonDown ? 0.95f : 0.75f));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

    juce::Path arrow;
    const auto cx = bounds.getRight() - 12.0f;
    const auto cy = bounds.getCentreY();
    arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour(amber());
    g.fillPath(arrow);
}

void LookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
}

void LookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor&)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
    g.setColour(juce::Colour(0xff07090c));
    g.fillRoundedRectangle(bounds, 3.0f);
}

void LookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
    g.setColour(editor.hasKeyboardFocus(true) ? displayGreen().withAlpha(0.8f) : juce::Colour(0xff3b4652));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}
}
