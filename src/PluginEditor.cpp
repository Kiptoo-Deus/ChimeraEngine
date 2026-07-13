#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <functional>

namespace
{
class EditorPage final : public juce::Component
{
public:
    std::function<void()> layout;

    void resized() override
    {
        if (layout)
            layout();
    }
};
}

ChimeraEngineAudioProcessorEditor::ChimeraEngineAudioProcessorEditor(ChimeraEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), owner(p)
{
    setLookAndFeel(&lookAndFeel);

    title.setText("Chimera Engine", juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centred);
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    tabs.setTabBarDepth(34);
    tabs.addTab("Overview", juce::Colour(0xff161b22), makeOverviewPage(), true);
    tabs.addTab("Element", juce::Colour(0xff161b22), makeElementPage(), true);
    tabs.addTab("FX Chain", juce::Colour(0xff161b22), makeFxPage(), true);
    tabs.addTab("Arp", juce::Colour(0xff161b22), makeArpPage(), true);
    tabs.addTab("Presets", juce::Colour(0xff161b22), makePresetPage(), true);
    tabs.addTab("Credits", juce::Colour(0xff161b22), makeCreditsPage(), true);
    addAndMakeVisible(tabs);

    keyboardState.addListener(this);
    keyboard.setAvailableRange(36, 84);
    keyboard.setKeyWidth(22.0f);
    keyboard.setScrollButtonsVisible(false);
    addAndMakeVisible(keyboard);

    setSize(900, 640);
}

ChimeraEngineAudioProcessorEditor::~ChimeraEngineAudioProcessorEditor()
{
    keyboardState.removeListener(this);
    setLookAndFeel(nullptr);
}

void ChimeraEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101317));
    g.setColour(juce::Colour(0xff2dd4bf));
    g.drawRect(getLocalBounds(), 2);
}

void ChimeraEngineAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(18);
    title.setBounds(bounds.removeFromTop(52));
    keyboard.setBounds(bounds.removeFromBottom(86).reduced(0, 10));
    tabs.setBounds(bounds);
}

void ChimeraEngineAudioProcessorEditor::handleNoteOn(juce::MidiKeyboardState*, int midiChannel,
                                                     int midiNoteNumber, float velocity)
{
    owner.enqueuePreviewNoteOn(midiChannel, midiNoteNumber, velocity);
}

void ChimeraEngineAudioProcessorEditor::handleNoteOff(juce::MidiKeyboardState*, int midiChannel,
                                                      int midiNoteNumber, float)
{
    owner.enqueuePreviewNoteOff(midiChannel, midiNoteNumber);
}

juce::Slider& ChimeraEngineAudioProcessorEditor::addKnob(juce::Component& parent, const juce::String& name)
{
    auto* slider = sliders.add(new juce::Slider(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow));
    slider->setName(name);
    slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 22);
    parent.addAndMakeVisible(slider);

    auto* label = labels.add(new juce::Label());
    label->setText(name, juce::dontSendNotification);
    label->setJustificationType(juce::Justification::centred);
    label->attachToComponent(slider, false);
    parent.addAndMakeVisible(label);
    return *slider;
}

juce::Component* ChimeraEngineAudioProcessorEditor::makeOverviewPage()
{
    auto* page = new EditorPage();
    auto& master = addKnob(*page, "Master");
    auto& cutoff = addKnob(*page, "Cutoff");
    auto& resonance = addKnob(*page, "Resonance");
    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "masterGain", master));
    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "cutoff", cutoff));
    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "resonance", resonance));

    page->layout = [page, &master, &cutoff, &resonance]
    {
        auto area = page->getLocalBounds().reduced(32, 58);
        const auto width = 128;
        master.setBounds(area.removeFromLeft(width));
        area.removeFromLeft(24);
        cutoff.setBounds(area.removeFromLeft(width));
        area.removeFromLeft(24);
        resonance.setBounds(area.removeFromLeft(width));
    };
    return page;
}

juce::Component* ChimeraEngineAudioProcessorEditor::makeElementPage()
{
    auto* page = new EditorPage();
    auto& attack = addKnob(*page, "Attack");
    auto& release = addKnob(*page, "Release");
    auto& cutoff = addKnob(*page, "Cutoff");
    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "attack", attack));
    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "release", release));
    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "cutoff", cutoff));

    page->layout = [page, &attack, &release, &cutoff]
    {
        auto area = page->getLocalBounds().reduced(32, 58);
        attack.setBounds(area.removeFromLeft(128));
        area.removeFromLeft(24);
        release.setBounds(area.removeFromLeft(128));
        area.removeFromLeft(24);
        cutoff.setBounds(area.removeFromLeft(128));
    };
    return page;
}

juce::Component* ChimeraEngineAudioProcessorEditor::makeFxPage()
{
    auto* page = new EditorPage();
    auto& mix = addKnob(*page, "FX Mix");
    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "fxMix", mix));
    page->layout = [page, &mix]
    {
        mix.setBounds(page->getLocalBounds().reduced(32, 58).removeFromLeft(128));
    };
    return page;
}

juce::Component* ChimeraEngineAudioProcessorEditor::makeArpPage()
{
    auto* page = new EditorPage();
    auto& gate = addKnob(*page, "Gate");
    sliderAttachments.add(new SliderAttachment(owner.getParameters(), "arpGate", gate));
    arpToggle.setButtonText("Enabled");
    page->addAndMakeVisible(arpToggle);
    buttonAttachments.add(new ButtonAttachment(owner.getParameters(), "arpEnabled", arpToggle));
    page->layout = [page, &gate, this]
    {
        auto area = page->getLocalBounds().reduced(32, 58);
        gate.setBounds(area.removeFromLeft(128));
        area.removeFromLeft(32);
        arpToggle.setBounds(area.removeFromLeft(120).removeFromTop(32));
    };
    return page;
}

juce::Component* ChimeraEngineAudioProcessorEditor::makePresetPage()
{
    auto* page = new EditorPage();
    auto* label = labels.add(new juce::Label());
    label->setText("Sine\nSaw\nSquare\nTriangle", juce::dontSendNotification);
    label->setJustificationType(juce::Justification::topLeft);
    page->addAndMakeVisible(label);
    page->layout = [page, label]
    {
        label->setBounds(page->getLocalBounds().reduced(32));
    };
    return page;
}

juce::Component* ChimeraEngineAudioProcessorEditor::makeCreditsPage()
{
    auto* page = new EditorPage();
    auto* label = labels.add(new juce::Label());
    label->setText("Generated Synth Waveforms\nNo attribution-required samples bundled", juce::dontSendNotification);
    label->setJustificationType(juce::Justification::topLeft);
    page->addAndMakeVisible(label);
    page->layout = [page, label]
    {
        label->setBounds(page->getLocalBounds().reduced(32));
    };
    return page;
}
