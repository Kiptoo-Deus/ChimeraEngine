#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <array>
#include <cmath>

namespace
{
juce::Colour panelFill() { return juce::Colour(0xff171c22); }
juce::Colour panelLine() { return juce::Colour(0xff38414b); }
juce::Colour accent() { return juce::Colour(0xff2dd4bf); }
juce::Colour amber() { return juce::Colour(0xfff2b84b); }

juce::String effectName(chimera::fx::EffectType type)
{
    switch (type)
    {
        case chimera::fx::EffectType::None: return "None";
        case chimera::fx::EffectType::Distortion: return "Distortion";
        case chimera::fx::EffectType::Compressor: return "Compressor";
        case chimera::fx::EffectType::ThreeBandEq: return "3-Band EQ";
        case chimera::fx::EffectType::Delay: return "Delay";
        case chimera::fx::EffectType::Chorus: return "Chorus";
        case chimera::fx::EffectType::Phaser: return "Phaser";
        case chimera::fx::EffectType::Limiter: return "Limiter";
        case chimera::fx::EffectType::AmpUsCombo: return "US Combo";
        case chimera::fx::EffectType::AmpJazzCombo: return "Jazz Combo";
        case chimera::fx::EffectType::AmpUsHighGain: return "US High Gain";
        case chimera::fx::EffectType::AmpBritishLead: return "British Lead";
        case chimera::fx::EffectType::AmpBritishCombo: return "British Combo";
        case chimera::fx::EffectType::AmpBritishLegend: return "British Legend";
        case chimera::fx::EffectType::MultiEffect: return "Multi FX";
        case chimera::fx::EffectType::SmallStereo: return "Small Stereo";
    }

    return "None";
}

int effectComboId(chimera::fx::EffectType type)
{
    return static_cast<int>(type) + 1;
}

chimera::fx::EffectType effectFromComboId(int id)
{
    return static_cast<chimera::fx::EffectType>(std::clamp(id - 1, 0, chimera::fx::effectTypeCount - 1));
}

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

    categoryDisplay.setText("ARP/VOICE/DRUM  |  7881 ARPS  |  256 USER ARPS  |  4 PERF ARPS", juce::dontSendNotification);
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
    addTransportControls();

    addFxControls();

    for (const auto& preset : { "Sine", "Saw", "Square", "Triangle", "Stack", "Velocity Split" })
        ignoreUnused(addPresetButton(*this, preset));

    addPartMixerControls();

    for (const auto& text : {
             "SEQ -> SCENE -> 4 ARP LANES",
             "ARP 1 -> PART 1 -> INSERT FX",
             "ARP 2 -> PART 2 -> SYSTEM FX",
             "MPE -> PER-NOTE EXPRESSION",
             "SCENE -> MIXER + FX VARIATION",
             "MIDI IN -> MULTITIMBRAL ENGINE" })
        ignoreUnused(addPanelLabel(text));

    keyboardState.addListener(this);
    keyboard.setAvailableRange(24, 96);
    keyboard.setKeyWidth(18.0f);
    keyboard.setScrollButtonsVisible(false);
    addAndMakeVisible(keyboard);

    setSize(1120, 760);
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
    drawPanel(g, PanelId::Mixer, "16-Part Mixer");
    drawPanel(g, PanelId::ElementMonitor, "MIDI Flow");

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
    effectsBounds = middle.removeFromLeft(300);
    middle.removeFromLeft(12);
    performanceBounds = middle.removeFromLeft(170);
    middle.removeFromLeft(12);
    elementMonitorBounds = middle;

    mixerBounds = area.removeFromTop(86);
    area.removeFromTop(8);
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
    sliders[5]->setBounds(fxArea.removeFromLeft(104).removeFromTop(150));
    fxArea.removeFromLeft(10);
    auto insertGrid = fxArea.removeFromTop(92);
    const auto insertWidth = (insertGrid.getWidth() - 6) / 2;
    for (int i = 0; i < fxInsertBoxes.size(); ++i)
    {
        const auto row = i / 2;
        const auto column = i % 2;
        fxInsertBoxes[i]->setBounds(insertGrid.getX() + column * (insertWidth + 6),
                                    insertGrid.getY() + row * 22,
                                    insertWidth,
                                    20);
    }
    fxArea.removeFromTop(6);
    for (int i = 0; i < fxSendSliders.size(); ++i)
    {
        fxSendSliders[i]->setBounds(fxArea.removeFromTop(26));
        fxArea.removeFromTop(6);
    }

    auto arpArea = performanceBounds.reduced(18, 38);
    sliders[6]->setBounds(arpArea.removeFromTop(86).withWidth(116));
    arpArea.removeFromTop(8);
    arpToggle.setBounds(arpArea.removeFromTop(30));
    arpArea.removeFromTop(8);
    fitRow({ &demoSequenceButton, &sequencerPlayButton, &sequencerResetButton, &mpeToggle }, arpArea.removeFromTop(28), 5);
    arpArea.removeFromTop(4);
    sequencerTickLabel.setBounds(arpArea.removeFromTop(22));

    auto monitor = elementMonitorBounds.reduced(16, 34);
    for (int i = 0; i < labels.size(); ++i)
    {
        if (labels[i]->getAttachedComponent() != nullptr)
            continue;

        labels[i]->setBounds(monitor.removeFromTop(24));
        monitor.removeFromTop(5);
    }

    auto mixer = mixerBounds.reduced(12, 24);
    const auto stripGap = 4;
    const auto stripWidth = (mixer.getWidth() - stripGap * (ChimeraEngineAudioProcessor::getPartCount() - 1))
        / ChimeraEngineAudioProcessor::getPartCount();
    for (int part = 0; part < ChimeraEngineAudioProcessor::getPartCount(); ++part)
    {
        auto strip = mixer.removeFromLeft(stripWidth);
        auto* enable = partEnableButtons[part];
        auto* level = partLevelSliders[part];
        auto* pan = partPanSliders[part];
        enable->setBounds(strip.removeFromTop(20));
        strip.removeFromTop(2);
        level->setBounds(strip.removeFromTop(30));
        strip.removeFromTop(2);
        pan->setBounds(strip.removeFromTop(12));
        mixer.removeFromLeft(stripGap);
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

void ChimeraEngineAudioProcessorEditor::addPartMixerControls()
{
    for (int part = 0; part < ChimeraEngineAudioProcessor::getPartCount(); ++part)
    {
        auto* enable = partEnableButtons.add(new juce::ToggleButton(juce::String(part + 1)));
        enable->setToggleState(owner.isPartEnabled(part), juce::dontSendNotification);
        enable->setColour(juce::ToggleButton::tickColourId, accent());
        enable->setColour(juce::ToggleButton::textColourId, juce::Colour(0xffdbeafe));
        enable->onClick = [this, part]
        {
            owner.setPartMix(part,
                             owner.getPartLevel(part),
                             owner.getPartPan(part),
                             partEnableButtons[part]->getToggleState());
            lcdLine.setText("Part " + juce::String(part + 1) + (owner.isPartEnabled(part) ? " enabled" : " muted"),
                            juce::dontSendNotification);
        };
        addAndMakeVisible(enable);

        auto* level = partLevelSliders.add(new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox));
        level->setRange(0.0, 1.5, 0.01);
        level->setValue(owner.getPartLevel(part), juce::dontSendNotification);
        level->setColour(juce::Slider::trackColourId, accent());
        level->setColour(juce::Slider::thumbColourId, juce::Colour(0xfff2b84b));
        level->onValueChange = [this, part]
        {
            owner.setPartMix(part,
                             static_cast<float>(partLevelSliders[part]->getValue()),
                             owner.getPartPan(part),
                             owner.isPartEnabled(part));
        };
        addAndMakeVisible(level);

        auto* pan = partPanSliders.add(new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox));
        pan->setRange(-1.0, 1.0, 0.01);
        pan->setValue(owner.getPartPan(part), juce::dontSendNotification);
        pan->setColour(juce::Slider::trackColourId, juce::Colour(0xff64748b));
        pan->setColour(juce::Slider::thumbColourId, accent());
        pan->onValueChange = [this, part]
        {
            owner.setPartMix(part,
                             owner.getPartLevel(part),
                             static_cast<float>(partPanSliders[part]->getValue()),
                             owner.isPartEnabled(part));
        };
        addAndMakeVisible(pan);
    }
}

void ChimeraEngineAudioProcessorEditor::refreshPartMixerControls()
{
    for (int part = 0; part < ChimeraEngineAudioProcessor::getPartCount(); ++part)
    {
        if (partEnableButtons[part]->getToggleState() != owner.isPartEnabled(part))
            partEnableButtons[part]->setToggleState(owner.isPartEnabled(part), juce::dontSendNotification);
        if (std::abs(static_cast<float>(partLevelSliders[part]->getValue()) - owner.getPartLevel(part)) > 0.001f)
            partLevelSliders[part]->setValue(owner.getPartLevel(part), juce::dontSendNotification);
        if (std::abs(static_cast<float>(partPanSliders[part]->getValue()) - owner.getPartPan(part)) > 0.001f)
            partPanSliders[part]->setValue(owner.getPartPan(part), juce::dontSendNotification);
    }
}

void ChimeraEngineAudioProcessorEditor::addFxControls()
{
    for (int slot = 0; slot < chimera::fx::InsertRack::slotCount; ++slot)
    {
        auto* box = fxInsertBoxes.add(new juce::ComboBox());
        for (int type = 0; type < chimera::fx::effectTypeCount; ++type)
            box->addItem("I" + juce::String(slot + 1) + " " + effectName(static_cast<chimera::fx::EffectType>(type)),
                         effectComboId(static_cast<chimera::fx::EffectType>(type)));
        box->setSelectedId(effectComboId(owner.getInsertEffect(slot)), juce::dontSendNotification);
        box->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d1117));
        box->setColour(juce::ComboBox::textColourId, juce::Colour(0xffe5eef7));
        box->setColour(juce::ComboBox::outlineColourId, panelLine());
        box->onChange = [this, slot]
        {
            owner.setInsertEffect(slot, effectFromComboId(fxInsertBoxes[slot]->getSelectedId()));
            lcdLine.setText("Insert " + juce::String(slot + 1) + ": " + effectName(owner.getInsertEffect(slot)),
                            juce::dontSendNotification);
        };
        addAndMakeVisible(box);
    }

    for (const auto name : { "Chorus", "Reverb" })
    {
        auto* slider = fxSendSliders.add(new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight));
        slider->setName(name);
        slider->setRange(0.0, 1.0, 0.01);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 46, 18);
        slider->setColour(juce::Slider::trackColourId, accent());
        slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(slider);
    }

    fxSendSliders[0]->setValue(owner.getChorusSend(), juce::dontSendNotification);
    fxSendSliders[1]->setValue(owner.getReverbSend(), juce::dontSendNotification);
    fxSendSliders[0]->onValueChange = [this]
    {
        owner.setSystemFxSends(static_cast<float>(fxSendSliders[0]->getValue()), owner.getReverbSend());
    };
    fxSendSliders[1]->onValueChange = [this]
    {
        owner.setSystemFxSends(owner.getChorusSend(), static_cast<float>(fxSendSliders[1]->getValue()));
    };
}

void ChimeraEngineAudioProcessorEditor::refreshFxControls()
{
    for (int slot = 0; slot < fxInsertBoxes.size(); ++slot)
    {
        const auto id = effectComboId(owner.getInsertEffect(slot));
        if (fxInsertBoxes[slot]->getSelectedId() != id)
            fxInsertBoxes[slot]->setSelectedId(id, juce::dontSendNotification);
    }

    if (std::abs(static_cast<float>(fxSendSliders[0]->getValue()) - owner.getChorusSend()) > 0.001f)
        fxSendSliders[0]->setValue(owner.getChorusSend(), juce::dontSendNotification);
    if (std::abs(static_cast<float>(fxSendSliders[1]->getValue()) - owner.getReverbSend()) > 0.001f)
        fxSendSliders[1]->setValue(owner.getReverbSend(), juce::dontSendNotification);
}

void ChimeraEngineAudioProcessorEditor::addTransportControls()
{
    demoSequenceButton.onClick = [this]
    {
        owner.seedDemoSequence();
        owner.resetSequencerPlayback();
        owner.setSequencerPlaybackEnabled(true);
        lcdLine.setText("Demo sequence armed and playing", juce::dontSendNotification);
        refreshTransportControls();
    };
    addAndMakeVisible(demoSequenceButton);

    sequencerPlayButton.onClick = [this]
    {
        owner.setSequencerPlaybackEnabled(!owner.isSequencerPlaybackEnabled());
        lcdLine.setText(owner.isSequencerPlaybackEnabled() ? "Sequencer playing" : "Sequencer stopped",
                        juce::dontSendNotification);
        refreshTransportControls();
    };
    addAndMakeVisible(sequencerPlayButton);

    sequencerResetButton.onClick = [this]
    {
        owner.resetSequencerPlayback();
        lcdLine.setText("Sequencer reset", juce::dontSendNotification);
        refreshTransportControls();
    };
    addAndMakeVisible(sequencerResetButton);

    mpeToggle.setToggleState(owner.isMpeExpressionEnabled(), juce::dontSendNotification);
    mpeToggle.onClick = [this]
    {
        owner.setMpeExpressionEnabled(mpeToggle.getToggleState());
        lcdLine.setText(owner.isMpeExpressionEnabled() ? "MPE expression enabled" : "MPE expression disabled",
                        juce::dontSendNotification);
    };
    addAndMakeVisible(mpeToggle);

    sequencerTickLabel.setJustificationType(juce::Justification::centredLeft);
    sequencerTickLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc7d2dc));
    addAndMakeVisible(sequencerTickLabel);
    refreshTransportControls();
}

void ChimeraEngineAudioProcessorEditor::refreshTransportControls()
{
    sequencerPlayButton.setButtonText(owner.isSequencerPlaybackEnabled() ? "Stop" : "Play");
    if (mpeToggle.getToggleState() != owner.isMpeExpressionEnabled())
        mpeToggle.setToggleState(owner.isMpeExpressionEnabled(), juce::dontSendNotification);
    sequencerTickLabel.setText("TICK " + juce::String(owner.getSequencerTick())
                                   + "  VAR " + juce::String(owner.getCurrentPerformanceScene() + 1)
                                   + (owner.isMpeExpressionEnabled() ? "  MPE" : ""),
                               juce::dontSendNotification);
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
            case PanelId::Mixer: return mixerBounds;
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
    refreshPartMixerControls();
    refreshFxControls();
    refreshTransportControls();
}
