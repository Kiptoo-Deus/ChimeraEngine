#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <array>

namespace
{
juce::Colour panelFill() { return juce::Colour(0xff171c22); }
juce::Colour panelLine() { return juce::Colour(0xff38414b); }
juce::Colour accent() { return juce::Colour(0xff2dd4bf); }
juce::Colour amber() { return juce::Colour(0xfff2b84b); }

void fitRow(std::initializer_list<juce::Component*> components, juce::Rectangle<int> area, int gap = 12)
{
    if (components.size() == 0)
        return;

    const auto width = (area.getWidth() - gap * (static_cast<int>(components.size()) - 1)) / static_cast<int>(components.size());
    for (auto* component : components)
    {
        component->setBounds(area.removeFromLeft(width));
        area.removeFromLeft(gap);
    }
}
}

ChimeraEngineAudioProcessorEditor::ChimeraEngineAudioProcessorEditor(ChimeraEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), owner(p)
{
    setLookAndFeel(&lookAndFeel);

    title.setText("Chimera Engine", juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centredLeft);
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    title.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    addAndMakeVisible(title);

    subtitle.setText("Element Workstation Instrument", juce::dontSendNotification);
    subtitle.setJustificationType(juce::Justification::centredRight);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff9aa7b3));
    addAndMakeVisible(subtitle);

    patchDisplay.setText(owner.getCurrentPatchName(), juce::dontSendNotification);
    patchDisplay.setJustificationType(juce::Justification::centredLeft);
    patchDisplay.setColour(juce::Label::textColourId, juce::Colour(0xff06110f));
    patchDisplay.setColour(juce::Label::backgroundColourId, accent());
    patchDisplay.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    addAndMakeVisible(patchDisplay);

    categoryDisplay.setText("VOICE/PERFORMANCE/SEQ/FX/SAMPLE  |  3977 WAVES  |  2 FLASH BOARDS", juce::dontSendNotification);
    categoryDisplay.setJustificationType(juce::Justification::centredLeft);
    categoryDisplay.setColour(juce::Label::textColourId, juce::Colour(0xffd8f7f1));
    addAndMakeVisible(categoryDisplay);

    lcdLine.setText("Ready - use MIDI input, the on-screen keyboard, or preset commands.", juce::dontSendNotification);
    lcdLine.setJustificationType(juce::Justification::centredLeft);
    lcdLine.setColour(juce::Label::textColourId, juce::Colour(0xffa7f3d0));
    addAndMakeVisible(lcdLine);

    for (const auto& mode : { "Voice", "Performance", "Mix", "Arp", "FX", "Utility" })
        ignoreUnused(addModeButton(mode));

    auto& master = addKnob(*this, "Master");
    auto& cutoff = addKnob(*this, "Cutoff");
    auto& resonance = addKnob(*this, "Resonance");
    auto& attack = addKnob(*this, "Attack");
    auto& release = addKnob(*this, "Release");
    auto& fxMix = addKnob(*this, "FX Mix");
    auto& arpGate = addKnob(*this, "Arp Gate");

    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "masterGain", master));
    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "cutoff", cutoff));
    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "resonance", resonance));
    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "attack", attack));
    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "release", release));
    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "fxMix", fxMix));
    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "arpGate", arpGate));

    arpToggle.setButtonText("ARP ON");
    addAndMakeVisible(arpToggle);
    buttonAttachments.add(new ButtonAttachment(owner.getParameters(), "arpEnabled", arpToggle));

    for (const auto& preset : { "Sine", "Saw", "Square", "Triangle", "Stack", "Velocity Split" })
        ignoreUnused(addPresetButton(*this, preset));

    for (const auto& text : {
             "Element 1  OSC  Pan L  Fine -7",
             "Element 2  OSC  Pan R  Fine +7",
             "Element 3  OSC  Center  Env Soft",
             "Element 4  Reserved",
             "Velocity zones: low / mid / high",
             "FX send follows global mix" })
        ignoreUnused(addPanelLabel(text));

    keyboardState.addListener(this);
    keyboard.setAvailableRange(24, 96);
    keyboard.setKeyWidth(18.0f);
    keyboard.setScrollButtonsVisible(false);
    addAndMakeVisible(keyboard);

    setSize(1120, 720);
    startTimerHz(8);
}

ChimeraEngineAudioProcessorEditor::~ChimeraEngineAudioProcessorEditor()
{
    stopTimer();
    keyboardState.removeListener(this);
    setLookAndFeel(nullptr);
}

void ChimeraEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0e1116));

    drawPanel(g, PanelId::Header, {});
    drawPanel(g, PanelId::Display, "LCD");
    drawPanel(g, PanelId::Modes, "Modes");
    drawPanel(g, PanelId::Tone, "Tone");
    drawPanel(g, PanelId::Envelope, "Amplitude");
    drawPanel(g, PanelId::Effects, "Effects");
    drawPanel(g, PanelId::Performance, "Arpeggiator");
    drawPanel(g, PanelId::Presets, "Voice Select");
    drawPanel(g, PanelId::ElementMonitor, "Element Monitor");

    auto keyboardFrame = keyboard.getBounds().expanded(8, 10);
    g.setColour(juce::Colour(0xff080a0d));
    g.fillRoundedRectangle(keyboardFrame.toFloat(), 6.0f);
    g.setColour(panelLine());
    g.drawRoundedRectangle(keyboardFrame.toFloat(), 6.0f, 1.0f);
}

void ChimeraEngineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(18);
    headerBounds = area.removeFromTop(54);
    area.removeFromTop(12);

    auto upper = area.removeFromTop(180);
    displayBounds = upper.removeFromLeft(520);
    upper.removeFromLeft(12);
    modeBounds = upper.removeFromTop(76);
    upper.removeFromTop(12);
    presetBounds = upper;

    area.removeFromTop(14);
    auto middle = area.removeFromTop(230);
    toneBounds = middle.removeFromLeft(330);
    middle.removeFromLeft(12);
    envelopeBounds = middle.removeFromLeft(220);
    middle.removeFromLeft(12);
    effectsBounds = middle.removeFromLeft(160);
    middle.removeFromLeft(12);
    performanceBounds = middle.removeFromLeft(170);
    middle.removeFromLeft(12);
    elementMonitorBounds = middle;

    keyboard.setBounds(area.removeFromBottom(104).reduced(8, 12));

    title.setBounds(headerBounds.reduced(18, 6).removeFromLeft(420));
    subtitle.setBounds(headerBounds.reduced(18, 6).removeFromRight(420));

    auto lcd = displayBounds.reduced(18, 26);
    patchDisplay.setBounds(lcd.removeFromTop(44));
    lcd.removeFromTop(8);
    categoryDisplay.setBounds(lcd.removeFromTop(28));
    lcd.removeFromTop(6);
    lcdLine.setBounds(lcd.removeFromTop(28));

    auto modeArea = modeBounds.reduced(14, 26);
    fitRow({ modeButtons[0], modeButtons[1], modeButtons[2], modeButtons[3], modeButtons[4], modeButtons[5] }, modeArea, 8);

    auto presetArea = presetBounds.reduced(14, 24);
    const auto presetButtonWidth = (presetArea.getWidth() - 20) / 3;
    const auto presetButtonHeight = 32;
    for (int i = 0; i < presetButtons.size(); ++i)
    {
        auto row = i / 3;
        auto column = i % 3;
        presetButtons[i]->setBounds(presetArea.getX() + column * (presetButtonWidth + 10),
                                    presetArea.getY() + row * (presetButtonHeight + 10),
                                    presetButtonWidth,
                                    presetButtonHeight);
    }

    auto toneArea = toneBounds.reduced(18, 38);
    fitRow({ sliders[0], sliders[1], sliders[2] }, toneArea.removeFromTop(150), 12);

    auto ampArea = envelopeBounds.reduced(18, 38);
    fitRow({ sliders[3], sliders[4] }, ampArea.removeFromTop(150), 12);

    auto fxArea = effectsBounds.reduced(18, 38);
    sliders[5]->setBounds(fxArea.removeFromTop(150).withWidth(116));

    auto arpArea = performanceBounds.reduced(18, 38);
    sliders[6]->setBounds(arpArea.removeFromTop(124).withWidth(116));
    arpArea.removeFromTop(8);
    arpToggle.setBounds(arpArea.removeFromTop(30));

    auto monitor = elementMonitorBounds.reduced(16, 34);
    for (int i = 0; i < labels.size(); ++i)
    {
        if (labels[i]->getAttachedComponent() != nullptr)
            continue;

        labels[i]->setBounds(monitor.removeFromTop(24));
        monitor.removeFromTop(5);
    }
}

juce::Slider& ChimeraEngineAudioProcessorEditor::addKnob(juce::Component& parent, const juce::String& name)
{
    auto* slider = sliders.add(new juce::Slider(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow));
    slider->setName(name);
    slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 20);
    slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    parent.addAndMakeVisible(slider);

    auto* label = labels.add(new juce::Label());
    label->setText(name, juce::dontSendNotification);
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::textColourId, juce::Colour(0xffdde5ec));
    label->attachToComponent(slider, false);
    parent.addAndMakeVisible(label);
    return *slider;
}

juce::TextButton& ChimeraEngineAudioProcessorEditor::addPresetButton(juce::Component& parent, const juce::String& presetName)
{
    auto* button = presetButtons.add(new juce::TextButton(presetName));
    button->onClick = [this, presetName]
    {
        const auto result = owner.loadSynthPreset(presetName);
        lcdLine.setText(result.wasOk() ? "Loaded voice: " + presetName : result.getErrorMessage(), juce::dontSendNotification);
        updateDisplay();
    };
    parent.addAndMakeVisible(button);
    return *button;
}

juce::TextButton& ChimeraEngineAudioProcessorEditor::addModeButton(const juce::String& name)
{
    auto* button = modeButtons.add(new juce::TextButton(name));
    button->onClick = [this, name]
    {
        lcdLine.setText(name + " mode selected", juce::dontSendNotification);
    };
    addAndMakeVisible(button);
    return *button;
}

juce::Label& ChimeraEngineAudioProcessorEditor::addPanelLabel(const juce::String& text, juce::Justification justification)
{
    auto* label = labels.add(new juce::Label());
    label->setText(text, juce::dontSendNotification);
    label->setJustificationType(justification);
    label->setColour(juce::Label::textColourId, juce::Colour(0xffc7d2dc));
    label->setColour(juce::Label::backgroundColourId, juce::Colour(0xff11161c));
    addAndMakeVisible(label);
    return *label;
}

void ChimeraEngineAudioProcessorEditor::drawPanel(juce::Graphics& g, PanelId panel, const juce::String& panelTitle)
{
    const auto bounds = [this, panel]
    {
        switch (panel)
        {
            case PanelId::Header: return headerBounds;
            case PanelId::Display: return displayBounds;
            case PanelId::Modes: return modeBounds;
            case PanelId::Tone: return toneBounds;
            case PanelId::Envelope: return envelopeBounds;
            case PanelId::Effects: return effectsBounds;
            case PanelId::Performance: return performanceBounds;
            case PanelId::Presets: return presetBounds;
            case PanelId::ElementMonitor: return elementMonitorBounds;
        }

        return juce::Rectangle<int>();
    }();

    if (bounds.isEmpty())
        return;

    const auto rounded = panel == PanelId::Header ? 4.0f : 6.0f;
    g.setColour(panel == PanelId::Header ? juce::Colour(0xff141922) : panelFill());
    g.fillRoundedRectangle(bounds.toFloat(), rounded);
    g.setColour(panel == PanelId::Header ? accent() : panelLine());
    g.drawRoundedRectangle(bounds.toFloat(), rounded, panel == PanelId::Header ? 1.6f : 1.0f);

    if (panelTitle.isNotEmpty())
    {
        g.setColour(panel == PanelId::Display ? amber() : juce::Colour(0xff9aa7b3));
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(panelTitle.toUpperCase(), bounds.reduced(12, 7).removeFromTop(18), juce::Justification::centredLeft);
    }
}

void ChimeraEngineAudioProcessorEditor::updateDisplay()
{
    const auto patchName = owner.getCurrentPatchName();
    if (patchDisplay.getText() != patchName)
        patchDisplay.setText(patchName, juce::dontSendNotification);
}

void ChimeraEngineAudioProcessorEditor::handleNoteOn(juce::MidiKeyboardState*, int midiChannel,
                                                     int midiNoteNumber, float velocity)
{
    owner.enqueuePreviewNoteOn(midiChannel, midiNoteNumber, velocity);
    lcdLine.setText("Preview note " + juce::String(midiNoteNumber) + " velocity " + juce::String(juce::roundToInt(velocity * 127.0f)),
                    juce::dontSendNotification);
}

void ChimeraEngineAudioProcessorEditor::handleNoteOff(juce::MidiKeyboardState*, int midiChannel,
                                                      int midiNoteNumber, float)
{
    owner.enqueuePreviewNoteOff(midiChannel, midiNoteNumber);
}

void ChimeraEngineAudioProcessorEditor::timerCallback()
{
    updateDisplay();
}
