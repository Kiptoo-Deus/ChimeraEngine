#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <array>
#include <cmath>

namespace
{
juce::Colour panelFill() { return juce::Colour(0xff15191f); }
juce::Colour panelLine() { return juce::Colour(0xff434d58); }
juce::Colour accent() { return juce::Colour(0xff66d9b7); }
juce::Colour amber() { return juce::Colour(0xfff0b64b); }

juce::File exportDirectory()
{
    auto directory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Chimera Engine Exports");
    directory.createDirectory();
    return directory;
}

juce::String pageName(ChimeraEngineAudioProcessorEditor::WorkstationPage page)
{
    using Page = ChimeraEngineAudioProcessorEditor::WorkstationPage;
    switch (page)
    {
        case Page::Voice: return "Voice";
        case Page::Performance: return "Performance";
        case Page::Mix: return "Mix";
        case Page::Arp: return "Arp";
        case Page::Song: return "Song";
        case Page::Pattern: return "Pattern";
        case Page::Sample: return "Sample";
        case Page::Utility: return "Utility";
        case Page::Demo: return "Demo";
    }

    return "Demo";
}

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

void drawGrid(juce::Graphics& g, juce::Rectangle<float> area, int columns, int rows, juce::Colour line)
{
    g.setColour(line);
    for (int column = 0; column <= columns; ++column)
    {
        const auto x = area.getX() + area.getWidth() * static_cast<float>(column) / static_cast<float>(columns);
        g.drawVerticalLine(juce::roundToInt(x), area.getY(), area.getBottom());
    }
    for (int row = 0; row <= rows; ++row)
    {
        const auto y = area.getY() + area.getHeight() * static_cast<float>(row) / static_cast<float>(rows);
        g.drawHorizontalLine(juce::roundToInt(y), area.getX(), area.getRight());
    }
}
}

class ChimeraEngineAudioProcessorEditor::GraphicalEditor final : public juce::Component
{
public:
    explicit GraphicalEditor(ChimeraEngineAudioProcessor& processor)
        : owner(processor)
    {
    }

    void setPage(WorkstationPage newPage)
    {
        page = newPage;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff050608));
        g.fillRoundedRectangle(bounds, 3.0f);
        juce::ColourGradient screen(juce::Colour(0xff101821), bounds.getX(), bounds.getY(),
                                    juce::Colour(0xff070b0f), bounds.getX(), bounds.getBottom(), false);
        screen.addColour(0.55, juce::Colour(0xff0c1118));
        g.setGradientFill(screen);
        g.fillRoundedRectangle(bounds.reduced(2.0f), 3.0f);
        g.setColour(juce::Colour(0xff3f4b56));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);
        g.setColour(juce::Colour(0xffb7c6d4).withAlpha(0.08f));
        g.drawLine(bounds.getX() + 3.0f, bounds.getY() + 2.0f, bounds.getRight() - 3.0f, bounds.getY() + 2.0f);

        auto area = bounds.reduced(10.0f);
        drawHeader(g, area.removeFromTop(20.0f));
        area.removeFromTop(6.0f);

        switch (page)
        {
            case WorkstationPage::Voice: drawModMatrix(g, area); break;
            case WorkstationPage::Performance: drawPerformanceScenes(g, area); break;
            case WorkstationPage::Mix: drawMetersAndFx(g, area); break;
            case WorkstationPage::Arp: drawArpGrid(g, area); break;
            case WorkstationPage::Song: drawPianoRollAndEvents(g, area); break;
            case WorkstationPage::Pattern: drawPatternArranger(g, area); break;
            case WorkstationPage::Sample: drawSampleAndDrumMap(g, area); break;
            case WorkstationPage::Utility: drawUtilityMap(g, area); break;
            case WorkstationPage::Demo: drawInnovationFlow(g, area); break;
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        handleCanvasGesture(event.position);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (page == WorkstationPage::Song || page == WorkstationPage::Arp)
            handleCanvasGesture(event.position);
    }

private:
    juce::Rectangle<float> graphArea() const
    {
        auto area = getLocalBounds().toFloat().reduced(10.0f);
        area.removeFromTop(20.0f);
        area.removeFromTop(6.0f);
        return area;
    }

    void handleCanvasGesture(juce::Point<float> position)
    {
        const auto area = graphArea();
        if (!area.contains(position))
            return;

        const auto xNorm = std::clamp((position.x - area.getX()) / std::max(1.0f, area.getWidth()), 0.0f, 1.0f);
        const auto yNorm = std::clamp((position.y - area.getY()) / std::max(1.0f, area.getHeight()), 0.0f, 1.0f);

        switch (page)
        {
            case WorkstationPage::Song:
                owner.editPianoRollNoteFromCanvas(xNorm, yNorm);
                break;
            case WorkstationPage::Arp:
            {
                const auto lane = std::clamp(static_cast<int>(yNorm * 4.0f), 0, 3);
                const auto step = std::clamp(static_cast<int>(xNorm * 16.0f), 0, 15);
                owner.editArpStepFromCanvas(lane, step, 1.0f - yNorm);
                break;
            }
            case WorkstationPage::Pattern:
                owner.editPatternCellFromCanvas(std::clamp(static_cast<int>(xNorm * 8.0f), 0, 7)
                                                + 8 * std::clamp(static_cast<int>(yNorm * 2.0f), 0, 1));
                break;
            case WorkstationPage::Sample:
                if (xNorm < 0.46f)
                {
                    const auto drumX = std::clamp(xNorm / 0.46f, 0.0f, 0.999f);
                    owner.editDrumPadFromCanvas(std::clamp(static_cast<int>(drumX * 8.0f), 0, 7)
                                                + 8 * std::clamp(static_cast<int>(yNorm * 4.0f), 0, 3));
                }
                else
                {
                    const auto sampleX = std::clamp((xNorm - 0.46f) / 0.54f, 0.0f, 0.999f);
                    owner.editSampleZoneFromCanvas(std::clamp(static_cast<int>(sampleX * 12.0f), 0, 11));
                }
                break;
            case WorkstationPage::Voice:
            {
                const auto source = std::clamp(static_cast<int>(yNorm * 7.0f) - 1, 0, 5);
                const auto destination = std::clamp(static_cast<int>(xNorm * 5.0f) - 1, 0, 3);
                owner.editModMatrixCellFromCanvas(source, destination);
                break;
            }
            case WorkstationPage::Performance:
                owner.applyPerformanceScene(std::clamp(static_cast<int>(xNorm * 4.0f), 0, 3)
                                            + 4 * std::clamp(static_cast<int>(yNorm * 2.0f), 0, 1));
                break;
            case WorkstationPage::Mix:
            {
                const auto rackX = std::clamp((position.x - (area.getX() + 84.0f)) / std::max(1.0f, area.getWidth() - 84.0f), 0.0f, 0.999f);
                const auto slot = std::clamp(static_cast<int>(rackX * 4.0f), 0, 3)
                                  + 4 * std::clamp(static_cast<int>(yNorm * 2.0f), 0, 1);
                owner.setInsertEffect(slot, static_cast<chimera::fx::EffectType>((slot + 1) % chimera::fx::effectTypeCount));
                break;
            }
            case WorkstationPage::Utility:
            case WorkstationPage::Demo:
                owner.captureSceneSnapshot(owner.getCurrentPerformanceScene(), "Canvas Scene");
                break;
        }

        repaint();
    }

    void drawHeader(juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xffa7f3d0));
        g.drawText(pageName(page).toUpperCase() + " GRAPH EDITOR", area, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff64748b));
        g.drawText("Tick " + juce::String(owner.getSequencerTick())
                       + "  Notes " + juce::String(owner.getCurrentSongNoteCount()),
                   area,
                   juce::Justification::centredRight);
    }

    void drawPianoRollAndEvents(juce::Graphics& g, juce::Rectangle<float> area)
    {
        auto roll = area.removeFromLeft(area.getWidth() * 0.68f);
        auto events = area.reduced(8.0f, 0.0f);
        g.setColour(juce::Colour(0xff101722));
        g.fillRoundedRectangle(roll, 4.0f);
        drawGrid(g, roll, 16, 12, juce::Colour(0x26374857));

        const std::array<int, 12> pitches { 72, 76, 67, 64, 60, 55, 52, 48, 43, 40, 36, 31 };
        const auto noteCount = std::max(1, owner.getCurrentSongNoteCount());
        for (int i = 0; i < std::min(14, noteCount); ++i)
        {
            const auto row = i % static_cast<int>(pitches.size());
            const auto x = roll.getX() + 8.0f + std::fmod(static_cast<float>(i) * 37.0f, roll.getWidth() - 52.0f);
            const auto y = roll.getY() + 4.0f + static_cast<float>(row) * (roll.getHeight() - 8.0f) / 12.0f;
            const auto width = 26.0f + static_cast<float>((i % 4) * 12);
            g.setColour(i % 2 == 0 ? accent() : amber());
            g.fillRoundedRectangle({ x, y, width, 8.0f }, 2.0f);
        }

        const auto playX = roll.getX() + std::fmod(static_cast<float>(owner.getSequencerTick()) / 1920.0f, 1.0f) * roll.getWidth();
        g.setColour(juce::Colour(0xfff8fafc));
        g.drawLine(playX, roll.getY(), playX, roll.getBottom(), 1.4f);

        g.setColour(juce::Colour(0xff101722));
        g.fillRoundedRectangle(events, 4.0f);
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        for (int row = 0; row < 6; ++row)
        {
            auto line = events.removeFromTop(18.0f);
            g.setColour(row % 2 == 0 ? juce::Colour(0xff172033) : juce::Colour(0xff111827));
            g.fillRoundedRectangle(line, 2.0f);
            g.setColour(juce::Colour(0xffdbeafe));
            g.drawText("T" + juce::String(row + 1) + "  note "
                           + juce::String(48 + row * 4)
                           + "  vel " + juce::String(88 + row * 3),
                       line.reduced(6.0f, 0.0f),
                       juce::Justification::centredLeft);
        }
    }

    void drawArpGrid(juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour(juce::Colour(0xff101722));
        g.fillRoundedRectangle(area, 4.0f);
        drawGrid(g, area, 16, 4, juce::Colour(0x30374857));

        for (int lane = 0; lane < 4; ++lane)
        {
            const auto laneY = area.getY() + static_cast<float>(lane) * area.getHeight() / 4.0f;
            g.setColour(juce::Colour(0xff94a3b8));
            g.drawText("ARP " + juce::String(lane + 1)
                           + "  USER " + juce::String(owner.getArpLaneAssignment(lane)),
                       juce::Rectangle<float>(area.getX() + 6.0f, laneY + 2.0f, 86.0f, 16.0f),
                       juce::Justification::centredLeft);
            for (int step = 0; step < 16; step += 3)
            {
                const auto x = area.getX() + static_cast<float>(step) * area.getWidth() / 16.0f + 2.0f;
                const auto h = 10.0f + static_cast<float>((lane + step) % 4) * 5.0f;
                g.setColour(lane % 2 == 0 ? accent() : amber());
                g.fillRoundedRectangle({ x, laneY + area.getHeight() / 4.0f - h - 5.0f, area.getWidth() / 16.0f - 4.0f, h }, 2.0f);
            }
        }
    }

    void drawPatternArranger(juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        const auto columns = 8;
        const auto rows = 2;
        drawGrid(g, area, columns, rows, juce::Colour(0x30374857));
        for (int section = 0; section < 16; ++section)
        {
            const auto col = section % columns;
            const auto row = section / columns;
            auto cell = juce::Rectangle<float>(area.getX() + col * area.getWidth() / columns,
                                               area.getY() + row * area.getHeight() / rows,
                                               area.getWidth() / columns,
                                               area.getHeight() / rows).reduced(4.0f);
            const auto phrase = owner.getPatternSectionPhrase(section);
            g.setColour(phrase > 0 ? juce::Colour(0xff164e63) : juce::Colour(0xff111827));
            g.fillRoundedRectangle(cell, 4.0f);
            g.setColour(phrase > 0 ? juce::Colour(0xff67e8f9) : juce::Colour(0xff94a3b8));
            g.drawText(juce::String::charToString(static_cast<juce::juce_wchar>('A' + section)), cell.removeFromTop(16.0f), juce::Justification::centred);
            g.drawText("PHR " + juce::String(phrase), cell, juce::Justification::centred);
        }
    }

    void drawSampleAndDrumMap(juce::Graphics& g, juce::Rectangle<float> area)
    {
        auto drum = area.removeFromLeft(area.getWidth() * 0.46f).reduced(0.0f, 2.0f);
        auto sample = area.reduced(8.0f, 2.0f);
        drawGrid(g, drum, 8, 4, juce::Colour(0x30374857));
        for (int pad = 0; pad < 32; ++pad)
        {
            const auto col = pad % 8;
            const auto row = pad / 8;
            auto cell = juce::Rectangle<float>(drum.getX() + col * drum.getWidth() / 8.0f,
                                               drum.getY() + row * drum.getHeight() / 4.0f,
                                               drum.getWidth() / 8.0f,
                                               drum.getHeight() / 4.0f).reduced(3.0f);
            g.setColour(pad < owner.getMappedDrumKeyCount() ? amber() : juce::Colour(0xff111827));
            g.fillRoundedRectangle(cell, 3.0f);
        }

        g.setColour(juce::Colour(0xff101722));
        g.fillRoundedRectangle(sample, 4.0f);
        const auto indexed = std::max(1, owner.getIndexedSampleCount());
        for (int zone = 0; zone < std::min(12, indexed); ++zone)
        {
            const auto x = sample.getX() + 8.0f + static_cast<float>(zone) * (sample.getWidth() - 16.0f) / 12.0f;
            const auto h = 18.0f + static_cast<float>(zone % 4) * 8.0f;
            g.setColour(zone % 2 == 0 ? accent() : juce::Colour(0xff60a5fa));
            g.fillRoundedRectangle({ x, sample.getBottom() - h - 8.0f, 12.0f, h }, 2.0f);
        }
        g.setColour(juce::Colour(0xffdbeafe));
        g.drawText("Samples indexed: " + juce::String(owner.getIndexedSampleCount()), sample.reduced(8.0f), juce::Justification::topLeft);
    }

    void drawModMatrix(juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        const std::array<juce::String, 6> sources { "VEL", "MOD", "AT", "LFO1", "LFO2", "EG" };
        const std::array<juce::String, 4> dests { "PITCH", "CUT", "AMP", "PAN" };
        drawGrid(g, area, static_cast<int>(dests.size()) + 1, static_cast<int>(sources.size()) + 1, juce::Colour(0x30374857));
        for (int d = 0; d < static_cast<int>(dests.size()); ++d)
        {
            g.setColour(juce::Colour(0xff94a3b8));
            g.drawText(dests[static_cast<size_t>(d)],
                       juce::Rectangle<float>(area.getX() + (d + 1) * area.getWidth() / 5.0f, area.getY(), area.getWidth() / 5.0f, 18.0f),
                       juce::Justification::centred);
        }
        for (int s = 0; s < static_cast<int>(sources.size()); ++s)
        {
            g.setColour(juce::Colour(0xff94a3b8));
            g.drawText(sources[static_cast<size_t>(s)],
                       juce::Rectangle<float>(area.getX(), area.getY() + (s + 1) * area.getHeight() / 7.0f, area.getWidth() / 5.0f, 18.0f),
                       juce::Justification::centred);
            for (int d = 0; d < static_cast<int>(dests.size()); ++d)
            {
                if ((s + d) % 3 != 0)
                    continue;
                auto cell = juce::Rectangle<float>(area.getX() + (d + 1) * area.getWidth() / 5.0f,
                                                   area.getY() + (s + 1) * area.getHeight() / 7.0f,
                                                   area.getWidth() / 5.0f,
                                                   area.getHeight() / 7.0f).reduced(8.0f, 5.0f);
                g.setColour(d % 2 == 0 ? accent() : amber());
                g.fillRoundedRectangle(cell, 3.0f);
            }
        }
        g.setColour(juce::Colour(0xffdbeafe));
        g.drawText(owner.getModMatrixSummary(0), area.reduced(8.0f), juce::Justification::bottomLeft);
    }

    void drawPerformanceScenes(juce::Graphics& g, juce::Rectangle<float> area)
    {
        for (int scene = 0; scene < 8; ++scene)
        {
            auto cell = juce::Rectangle<float>(area.getX() + (scene % 4) * area.getWidth() / 4.0f,
                                               area.getY() + (scene / 4) * area.getHeight() / 2.0f,
                                               area.getWidth() / 4.0f,
                                               area.getHeight() / 2.0f).reduced(5.0f);
            g.setColour(scene == owner.getCurrentPerformanceScene() ? juce::Colour(0xff115e59) : juce::Colour(0xff111827));
            g.fillRoundedRectangle(cell, 4.0f);
            g.setColour(scene == owner.getCurrentPerformanceScene() ? accent() : juce::Colour(0xff94a3b8));
            g.drawRoundedRectangle(cell, 4.0f, 1.0f);
            g.drawText(owner.getSceneName(scene), cell.reduced(7.0f), juce::Justification::topLeft);
            g.drawText("ARP " + juce::String(owner.getArpLaneAssignment(scene % 4)), cell.reduced(7.0f), juce::Justification::bottomRight);
        }
    }

    void drawMetersAndFx(juce::Graphics& g, juce::Rectangle<float> area)
    {
        auto meters = area.removeFromLeft(76.0f);
        auto rack = area.reduced(8.0f, 0.0f);
        const auto leftLevel = std::clamp(owner.getOutputPeakLeft(), 0.0f, 1.0f);
        const auto rightLevel = std::clamp(owner.getOutputPeakRight(), 0.0f, 1.0f);
        for (int i = 0; i < 2; ++i)
        {
            auto bar = meters.removeFromLeft(28.0f).reduced(6.0f, 0.0f);
            g.setColour(juce::Colour(0xff111827));
            g.fillRoundedRectangle(bar, 4.0f);
            const auto level = i == 0 ? leftLevel : rightLevel;
            g.setColour(i == 0 ? accent() : amber());
            g.fillRoundedRectangle(bar.withTrimmedTop(bar.getHeight() * (1.0f - level)), 4.0f);
        }
        drawGrid(g, rack, 4, 2, juce::Colour(0x30374857));
        for (int slot = 0; slot < 8; ++slot)
        {
            auto cell = juce::Rectangle<float>(rack.getX() + (slot % 4) * rack.getWidth() / 4.0f,
                                               rack.getY() + (slot / 4) * rack.getHeight() / 2.0f,
                                               rack.getWidth() / 4.0f,
                                               rack.getHeight() / 2.0f).reduced(5.0f);
            g.setColour(juce::Colour(0xff172033));
            g.fillRoundedRectangle(cell, 4.0f);
            g.setColour(juce::Colour(0xffdbeafe));
            g.drawText("I" + juce::String(slot + 1), cell.reduced(6.0f), juce::Justification::centredLeft);
        }
    }

    void drawUtilityMap(juce::Graphics& g, juce::Rectangle<float> area)
    {
        drawInnovationFlow(g, area);
        g.setColour(juce::Colour(0xffdbeafe));
        g.drawText("MIDI OUT / MIDI IN / WAV BOUNCE / VALIDATION", area.reduced(8.0f), juce::Justification::bottomRight);
    }

    void drawInnovationFlow(juce::Graphics& g, juce::Rectangle<float> area)
    {
        const std::array<juce::String, 6> nodes { "SEQ", "SCENE", "4 ARPS", "PARTS", "FX", "OUT" };
        const auto nodeW = area.getWidth() / 6.0f - 8.0f;
        for (int i = 0; i < 6; ++i)
        {
            auto node = juce::Rectangle<float>(area.getX() + i * area.getWidth() / 6.0f + 4.0f,
                                               area.getCentreY() - 24.0f,
                                               nodeW,
                                               48.0f);
            g.setColour(i == owner.getCurrentPerformanceScene() + 1 ? amber() : juce::Colour(0xff122033));
            g.fillRoundedRectangle(node, 5.0f);
            g.setColour(i % 2 == 0 ? accent() : juce::Colour(0xff60a5fa));
            g.drawRoundedRectangle(node, 5.0f, 1.2f);
            g.drawText(nodes[static_cast<size_t>(i)], node, juce::Justification::centred);
            if (i < 5)
            {
                g.setColour(juce::Colour(0xff64748b));
                g.drawArrow({ node.getRight() + 2.0f, node.getCentreY(), node.getRight() + area.getWidth() / 6.0f - 10.0f, node.getCentreY() },
                            1.4f,
                            7.0f,
                            6.0f);
            }
        }
    }

    ChimeraEngineAudioProcessor& owner;
    WorkstationPage page = WorkstationPage::Demo;
};

ChimeraEngineAudioProcessorEditor::ChimeraEngineAudioProcessorEditor(ChimeraEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), owner(p)
{
    setLookAndFeel(&lookAndFeel);

    title.setText("Chimera Engine", juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centredLeft);
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff2f6fa));
    title.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    addAndMakeVisible(title);

    subtitle.setText("Element Workstation Instrument", juce::dontSendNotification);
    subtitle.setJustificationType(juce::Justification::centredRight);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xffaab6c2));
    addAndMakeVisible(subtitle);

    patchDisplay.setText(owner.getCurrentPatchName(), juce::dontSendNotification);
    patchDisplay.setJustificationType(juce::Justification::centredLeft);
    patchDisplay.setColour(juce::Label::textColourId, juce::Colour(0xffbfffe8));
    patchDisplay.setColour(juce::Label::backgroundColourId, juce::Colour(0xff09110d));
    patchDisplay.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    addAndMakeVisible(patchDisplay);

    categoryDisplay.setText("ARP/VOICE/DRUM  |  7881 ARPS  |  256 USER ARPS  |  4 PERF ARPS", juce::dontSendNotification);
    categoryDisplay.setJustificationType(juce::Justification::centredLeft);
    categoryDisplay.setColour(juce::Label::textColourId, juce::Colour(0xffa9d7c6));
    categoryDisplay.setColour(juce::Label::backgroundColourId, juce::Colour(0xff09110d));
    addAndMakeVisible(categoryDisplay);

    lcdLine.setText("Ready - use MIDI input, the on-screen keyboard, or preset commands.", juce::dontSendNotification);
    lcdLine.setJustificationType(juce::Justification::centredLeft);
    lcdLine.setColour(juce::Label::textColourId, juce::Colour(0xff82d6af));
    lcdLine.setColour(juce::Label::backgroundColourId, juce::Colour(0xff09110d));
    addAndMakeVisible(lcdLine);

    for (const auto& mode : { "Voice", "Perf", "Mix", "Arp", "Song", "Pattern", "Sample", "Utility", "Demo" })
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
    addPageSurfaceControls();
    graphicalEditor = std::make_unique<GraphicalEditor>(owner);
    addAndMakeVisible(*graphicalEditor);

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
    keyboard.setKeyWidth(16.0f);
    keyboard.setScrollButtonsVisible(false);
    addAndMakeVisible(keyboard);

    setSize(1280, 900);
    setActivePage(WorkstationPage::Demo);
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
    juce::ColourGradient background(juce::Colour(0xff181d23), 0.0f, 0.0f,
                                    juce::Colour(0xff07090c), 0.0f, static_cast<float>(getHeight()), false);
    background.addColour(0.35, juce::Colour(0xff101419));
    g.setGradientFill(background);
    g.fillAll();

    auto chassis = getLocalBounds().toFloat().reduced(8.0f);
    g.setColour(juce::Colour(0xff050608));
    g.fillRoundedRectangle(chassis, 8.0f);
    juce::ColourGradient face(juce::Colour(0xff2a3037), chassis.getX(), chassis.getY(),
                              juce::Colour(0xff11151a), chassis.getX(), chassis.getBottom(), false);
    face.addColour(0.18, juce::Colour(0xff343b44));
    face.addColour(0.5, juce::Colour(0xff1b2027));
    g.setGradientFill(face);
    g.fillRoundedRectangle(chassis.reduced(3.0f), 6.0f);
    g.setColour(juce::Colour(0xff57616d));
    g.drawRoundedRectangle(chassis.reduced(3.0f), 6.0f, 1.0f);

    auto topRail = chassis.reduced(16.0f, 12.0f).removeFromTop(8.0f);
    g.setColour(juce::Colour(0xff07090c));
    g.fillRoundedRectangle(topRail, 3.0f);
    g.setColour(juce::Colour(0xff3c4650));
    g.drawLine(topRail.getX(), topRail.getBottom() + 4.0f, topRail.getRight(), topRail.getBottom() + 4.0f, 1.0f);

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
    g.setColour(juce::Colour(0xff06080b));
    g.fillRoundedRectangle(keyboardFrame.toFloat(), 4.0f);
    g.setColour(panelLine());
    g.drawRoundedRectangle(keyboardFrame.toFloat(), 4.0f, 1.0f);
}

void ChimeraEngineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(22);
    headerBounds = area.removeFromTop(64);
    area.removeFromTop(12);

    auto upper = area.removeFromTop(154);
    displayBounds = upper.removeFromLeft(470);
    upper.removeFromLeft(14);
    modeBounds = upper.removeFromTop(62);
    upper.removeFromTop(10);
    presetBounds = upper;

    area.removeFromTop(14);
    auto middle = area.removeFromTop(366);
    auto leftColumn = middle.removeFromLeft(318);
    middle.removeFromLeft(14);
    auto rightColumn = middle.removeFromRight(304);
    middle.removeFromRight(14);
    elementMonitorBounds = middle;

    toneBounds = leftColumn.removeFromTop(204);
    leftColumn.removeFromTop(14);
    envelopeBounds = leftColumn;

    effectsBounds = rightColumn.removeFromTop(238);
    rightColumn.removeFromTop(14);
    performanceBounds = rightColumn;

    area.removeFromTop(14);
    mixerBounds = area.removeFromTop(104);
    area.removeFromTop(12);
    keyboard.setBounds(area.reduced(8, 10));

    title.setBounds(headerBounds.reduced(18, 6).removeFromLeft(420));
    subtitle.setBounds(headerBounds.reduced(18, 6).removeFromRight(420));

    auto lcd = displayBounds.reduced(18, 26);
    patchDisplay.setBounds(lcd.removeFromTop(44));
    lcd.removeFromTop(8);
    categoryDisplay.setBounds(lcd.removeFromTop(28));
    lcd.removeFromTop(6);
    lcdLine.setBounds(lcd.removeFromTop(28));

    auto modeArea = modeBounds.reduced(14, 26);
    fitRow({ modeButtons[0], modeButtons[1], modeButtons[2], modeButtons[3], modeButtons[4],
             modeButtons[5], modeButtons[6], modeButtons[7], modeButtons[8] },
           modeArea,
           5);

    auto presetArea = presetBounds.reduced(14, 24);
    auto browserArea = presetArea.removeFromTop(28);
    presetSearch.setBounds(browserArea.removeFromLeft(180));
    browserArea.removeFromLeft(6);
    presetCategoryBox.setBounds(browserArea.removeFromLeft(120));
    browserArea.removeFromLeft(6);
    presetBrowserBox.setBounds(browserArea.removeFromLeft(220));
    browserArea.removeFromLeft(6);
    favouriteToggle.setBounds(browserArea.removeFromLeft(58));
    performanceBrowserBox.setBounds(presetBounds.reduced(14, 24).removeFromTop(28));
    sampleSlotBox.setBounds(presetBounds.reduced(14, 24).removeFromTop(28));
    presetArea.removeFromTop(10);
    const auto presetButtonWidth = (presetArea.getWidth() - 50) / 6;
    const auto presetButtonHeight = 30;
    for (int i = 0; i < presetButtons.size(); ++i)
    {
        auto row = 0;
        auto column = i;
        presetButtons[i]->setBounds(presetArea.getX() + column * (presetButtonWidth + 10),
                                    presetArea.getY() + row * (presetButtonHeight + 10),
                                    presetButtonWidth,
                                    presetButtonHeight);
    }

    auto toneArea = toneBounds.reduced(22, 40);
    fitRow({ sliders[0], sliders[1], sliders[2] }, toneArea.removeFromTop(142), 14);

    auto ampArea = envelopeBounds.reduced(22, 40);
    fitRow({ sliders[3], sliders[4] }, ampArea.removeFromTop(128), 16);

    auto fxArea = effectsBounds.reduced(18, 38);
    sliders[5]->setBounds(fxArea.removeFromLeft(92).removeFromTop(132));
    fxArea.removeFromLeft(12);
    auto insertGrid = fxArea.removeFromTop(94);
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
    fxArea.removeFromTop(4);
    for (int i = 0; i < fxSendSliders.size(); ++i)
    {
        fxSendSliders[i]->setBounds(fxArea.removeFromTop(22));
        fxArea.removeFromTop(3);
    }
    fxArea.removeFromTop(2);
    for (int i = 0; i < masterFxSliders.size(); ++i)
    {
        masterFxSliders[i]->setBounds(fxArea.removeFromTop(17));
        fxArea.removeFromTop(2);
    }

    auto arpArea = performanceBounds.reduced(18, 38);
    sliders[6]->setBounds(arpArea.removeFromLeft(88).removeFromTop(114));
    arpArea.removeFromLeft(12);
    arpToggle.setBounds(arpArea.removeFromTop(28));
    arpArea.removeFromTop(10);
    fitRow({ &demoSequenceButton, &sequencerPlayButton, &sequencerResetButton, &mpeToggle }, arpArea.removeFromTop(28), 5);
    arpArea.removeFromTop(4);
    sequencerTickLabel.setBounds(arpArea.removeFromTop(22));

    for (int i = 0; i < labels.size(); ++i)
    {
        if (labels[i]->getAttachedComponent() != nullptr)
            continue;

        labels[i]->setVisible(false);
    }

    auto pageArea = elementMonitorBounds.reduced(18, 36);
    graphicalEditor->setBounds(pageArea.removeFromTop(232).toNearestInt());
    pageArea.removeFromTop(10);
    for (int i = 0; i < pageLabels.size(); ++i)
    {
        if (i >= 4)
        {
            pageLabels[i]->setVisible(false);
            continue;
        }
        pageLabels[i]->setVisible(true);
        pageLabels[i]->setBounds(pageArea.removeFromTop(19).toNearestInt());
        pageArea.removeFromTop(4);
    }
    pageArea.removeFromTop(2);
    const auto actionWidth = (pageArea.getWidth() - 12) / 4;
    for (int i = 0; i < pageActionButtons.size(); ++i)
    {
        if (i >= 4)
        {
            pageActionButtons[i]->setVisible(false);
            continue;
        }
        pageActionButtons[i]->setBounds(pageArea.getX() + i * (actionWidth + 4),
                                        pageArea.getBottom() - 30,
                                        actionWidth,
                                        28);
    }

    auto mixer = mixerBounds.reduced(14, 24);
    const auto stripGap = 6;
    const auto stripWidth = (mixer.getWidth() - stripGap * (ChimeraEngineAudioProcessor::getPartCount() - 1))
        / ChimeraEngineAudioProcessor::getPartCount();
    for (int part = 0; part < ChimeraEngineAudioProcessor::getPartCount(); ++part)
    {
        auto strip = mixer.removeFromLeft(stripWidth);
        auto* enable = partEnableButtons[part];
        auto* level = partLevelSliders[part];
        auto* pan = partPanSliders[part];
        enable->setBounds(strip.removeFromTop(18));
        strip.removeFromTop(2);
        level->setBounds(strip.removeFromTop(40).reduced(2, 0));
        strip.removeFromTop(2);
        pan->setBounds(strip.removeFromTop(14));
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
    label->setVisible(false);
    parent.addChildComponent(label);
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
        if (name == "Voice") setActivePage(WorkstationPage::Voice);
        else if (name == "Perf") setActivePage(WorkstationPage::Performance);
        else if (name == "Mix") setActivePage(WorkstationPage::Mix);
        else if (name == "Arp") setActivePage(WorkstationPage::Arp);
        else if (name == "Song") setActivePage(WorkstationPage::Song);
        else if (name == "Pattern") setActivePage(WorkstationPage::Pattern);
        else if (name == "Sample") setActivePage(WorkstationPage::Sample);
        else if (name == "Utility") setActivePage(WorkstationPage::Utility);
        else setActivePage(WorkstationPage::Demo);
    };
    addAndMakeVisible(button);
    return *button;
}

void ChimeraEngineAudioProcessorEditor::addPageSurfaceControls()
{
    presetSearch.setTextToShowWhenEmpty("Search voices", juce::Colour(0xff7f8c99));
    presetSearch.onTextChange = [this] { refreshPageSurface(); };
    addAndMakeVisible(presetSearch);

    for (const auto item : { "All", "Piano", "Synth", "Drums", "Favorites" })
        presetCategoryBox.addItem(item, presetCategoryBox.getNumItems() + 1);
    presetCategoryBox.setSelectedId(1, juce::dontSendNotification);
    presetCategoryBox.onChange = [this] { refreshPageSurface(); };
    addAndMakeVisible(presetCategoryBox);

    for (const auto item : { "Sine", "Saw", "Square", "Triangle", "Stack", "Velocity Split", "Expressive Mono", "LFO Pan" })
        presetBrowserBox.addItem(item, presetBrowserBox.getNumItems() + 1);
    presetBrowserBox.setSelectedId(1, juce::dontSendNotification);
    presetBrowserBox.onChange = [this]
    {
        if (presetBrowserBox.getText().isNotEmpty())
        {
            const auto result = owner.loadSynthPreset(presetBrowserBox.getText());
            lcdLine.setText(result.wasOk() ? "Browser loaded voice: " + presetBrowserBox.getText() : result.getErrorMessage(),
                            juce::dontSendNotification);
            updateDisplay();
        }
    };
    addAndMakeVisible(presetBrowserBox);

    for (int index = 0; index < 512; ++index)
        performanceBrowserBox.addItem(index == 0 ? "001 Demo Split Stack" : "Init Performance " + juce::String(index + 1).paddedLeft('0', 3),
                                      index + 1);
    performanceBrowserBox.setSelectedId(1, juce::dontSendNotification);
    performanceBrowserBox.onChange = [this]
    {
        owner.setPerformanceModeEnabled(true);
        lcdLine.setText("Performance slot " + juce::String(performanceBrowserBox.getSelectedId()) + " selected",
                        juce::dontSendNotification);
    };
    addAndMakeVisible(performanceBrowserBox);

    for (const auto item : { "Factory ROM", "User Flash 1", "User Flash 2", "Drum Kits", "Imports" })
        sampleSlotBox.addItem(item, sampleSlotBox.getNumItems() + 1);
    sampleSlotBox.setSelectedId(1, juce::dontSendNotification);
    sampleSlotBox.onChange = [this]
    {
        lcdLine.setText("Library view: " + sampleSlotBox.getText(), juce::dontSendNotification);
    };
    addAndMakeVisible(sampleSlotBox);

    favouriteToggle.onClick = [this]
    {
        lcdLine.setText(favouriteToggle.getToggleState() ? "Voice marked as favorite" : "Voice favorite cleared",
                        juce::dontSendNotification);
    };
    addAndMakeVisible(favouriteToggle);

    for (int i = 0; i < 8; ++i)
    {
        auto* label = pageLabels.add(new juce::Label());
        label->setColour(juce::Label::textColourId, juce::Colour(0xffcad6e1));
        label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        label->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(label);
    }

    for (int i = 0; i < 8; ++i)
    {
        auto* button = pageActionButtons.add(new juce::TextButton());
        addAndMakeVisible(button);
    }

    pageActionButtons[0]->onClick = [this]
    {
        if (activePage == WorkstationPage::Performance || activePage == WorkstationPage::Demo)
        {
            owner.applyPerformanceScene(0);
            lcdLine.setText("Variation 1 applied", juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Song)
        {
            owner.seedDemoSequence();
            lcdLine.setText("Song demo seeded: " + juce::String(owner.getCurrentSongNoteCount()) + " notes", juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Utility)
        {
            const auto file = exportDirectory().getChildFile("chimera-song.mid");
            const auto result = owner.exportCurrentSongToMidi(file);
            lcdLine.setText(result.wasOk() ? "MIDI exported: " + file.getFileName() : result.getErrorMessage(), juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Arp)
        {
            owner.saveUserArp(0, "Front Panel Arp");
            owner.assignArpToLane(0, 0);
            lcdLine.setText("User arp saved and assigned to lane 1", juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Pattern)
        {
            owner.addPatternPhraseNote(0, 0, 0, 120, 60, 100, 1);
            owner.assignPatternSection(0, 1);
            lcdLine.setText("Pattern section A assigned to phrase 1", juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Sample)
        {
            const auto result = owner.startSampleImportJob(exportDirectory());
            lcdLine.setText(result.wasOk() ? owner.getSampleImportReport() : result.getErrorMessage(), juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Voice)
        {
            owner.setPresetFavorite(owner.getCurrentPatchName(), favouriteToggle.getToggleState());
            lcdLine.setText(owner.getCurrentPatchName() + " favorite metadata saved", juce::dontSendNotification);
        }
        else
            lcdLine.setText(pageName(activePage) + " edit focus", juce::dontSendNotification);
        refreshPageSurface();
    };

    pageActionButtons[1]->onClick = [this]
    {
        if (activePage == WorkstationPage::Performance || activePage == WorkstationPage::Demo)
        {
            owner.applyPerformanceScene(1);
            lcdLine.setText("Variation 2 applied", juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Song || activePage == WorkstationPage::Demo)
        {
            owner.setSequencerPlaybackEnabled(!owner.isSequencerPlaybackEnabled());
            lcdLine.setText(owner.isSequencerPlaybackEnabled() ? "Sequencer playing" : "Sequencer stopped",
                            juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Utility)
        {
            const auto file = exportDirectory().getChildFile("chimera-demo.wav");
            const auto result = owner.bounceDemoToWav(file, 8.0);
            lcdLine.setText(result.wasOk() ? "WAV bounced: " + file.getFileName() : result.getErrorMessage(), juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Mix)
        {
            owner.saveFxPreset(0, "Front Panel FX");
            lcdLine.setText("FX preset saved: " + owner.getFxPresetName(0), juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Performance)
        {
            owner.storePerformance(performanceBrowserBox.getSelectedId() - 1, performanceBrowserBox.getText());
            lcdLine.setText("Performance stored: " + performanceBrowserBox.getText(), juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Arp)
        {
            owner.assignArpToLane(1, 0);
            lcdLine.setText("User arp assigned to lane 2", juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Sample)
        {
            owner.mapDrumKey(36, "Kick", 1);
            owner.setDrumKitModeEnabled(true);
            lcdLine.setText("Drum key C1 mapped and drum mode enabled", juce::dontSendNotification);
        }
        else
            lcdLine.setText(pageName(activePage) + " secondary command", juce::dontSendNotification);
        refreshPageSurface();
    };

    pageActionButtons[2]->onClick = [this]
    {
        if (activePage == WorkstationPage::Demo)
        {
            owner.resetSequencerPlayback();
            lcdLine.setText("Sequencer reset", juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Performance)
        {
            owner.setPerformanceModeEnabled(!owner.isPerformanceModeEnabled());
            lcdLine.setText(owner.isPerformanceModeEnabled() ? "Performance mode enabled" : "Performance mode disabled",
                            juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Song)
        {
            owner.setLiveRecordingEnabled(!owner.isLiveRecordingEnabled(), true, false);
            lcdLine.setText(owner.isLiveRecordingEnabled() ? "Live recording enabled" : "Live recording disabled",
                            juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Arp)
        {
            owner.saveUserArp(1, "Phrase Arp");
            owner.assignArpToLane(2, 1);
            lcdLine.setText("Phrase arp assigned to lane 3", juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Mix)
        {
            const auto loaded = owner.loadFxPreset(0);
            lcdLine.setText(loaded ? "FX preset loaded: " + owner.getFxPresetName(0) : "FX preset slot 1 is empty",
                            juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Utility)
        {
            const auto text = owner.undoLastEdit();
            lcdLine.setText(text.isNotEmpty() ? text : "Nothing to undo", juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Voice)
        {
            const auto file = exportDirectory().getChildFile(owner.getCurrentPatchName() + "-edited.chpatch");
            const auto result = owner.writeCurrentPatchEdit(file);
            lcdLine.setText(result.wasOk() ? "Patch edit written: " + file.getFileName() : result.getErrorMessage(),
                            juce::dontSendNotification);
        }
        else
            lcdLine.setText(pageName(activePage) + " editor armed", juce::dontSendNotification);
        refreshPageSurface();
    };

    pageActionButtons[3]->onClick = [this]
    {
        if (activePage == WorkstationPage::Utility)
        {
            if (owner.canRedo())
                lcdLine.setText(owner.redoLastEdit(), juce::dontSendNotification);
            else
            {
                const auto file = exportDirectory().getChildFile("chimera-song.mid");
                const auto result = owner.importSongFromMidi(file);
                lcdLine.setText(result.wasOk() ? "MIDI imported: " + file.getFileName() : result.getErrorMessage(), juce::dontSendNotification);
            }
        }
        else if (activePage == WorkstationPage::Sample)
            lcdLine.setText("Sample manager ready for licensed library import", juce::dontSendNotification);
        else if (activePage == WorkstationPage::Performance)
        {
            owner.captureSceneSnapshot(owner.getCurrentPerformanceScene(), "Front Panel Scene");
            lcdLine.setText("Named scene snapshot captured", juce::dontSendNotification);
        }
        else if (activePage == WorkstationPage::Pattern)
        {
            owner.addPatternPhraseNote(1, 0, 120, 120, 67, 96, 1);
            owner.assignPatternSection(1, 2);
            lcdLine.setText("Pattern section B assigned to phrase 2", juce::dontSendNotification);
        }
        else
            lcdLine.setText(pageName(activePage) + " workflow captured", juce::dontSendNotification);
        refreshPageSurface();
    };
}

void ChimeraEngineAudioProcessorEditor::setActivePage(WorkstationPage page)
{
    activePage = page;
    if (graphicalEditor != nullptr)
        graphicalEditor->setPage(activePage);
    lcdLine.setText(pageName(activePage) + " page selected", juce::dontSendNotification);
    for (int i = 0; i < modeButtons.size(); ++i)
        modeButtons[i]->setColour(juce::TextButton::buttonColourId,
                                  i == static_cast<int>(activePage) ? juce::Colour(0xff1f766f) : juce::Colour(0xff202833));
    refreshPageSurface();
    resized();
    repaint();
}

void ChimeraEngineAudioProcessorEditor::setPageSurfaceVisible(bool shouldBeVisible)
{
    presetSearch.setVisible(shouldBeVisible && activePage == WorkstationPage::Voice);
    presetCategoryBox.setVisible(shouldBeVisible && activePage == WorkstationPage::Voice);
    presetBrowserBox.setVisible(shouldBeVisible && activePage == WorkstationPage::Voice);
    favouriteToggle.setVisible(shouldBeVisible && activePage == WorkstationPage::Voice);
    performanceBrowserBox.setVisible(shouldBeVisible && activePage == WorkstationPage::Performance);
    sampleSlotBox.setVisible(shouldBeVisible && activePage == WorkstationPage::Sample);
    for (auto* label : pageLabels)
        label->setVisible(shouldBeVisible);
    for (auto* button : pageActionButtons)
        button->setVisible(shouldBeVisible);
}

void ChimeraEngineAudioProcessorEditor::refreshPageSurface()
{
    setPageSurfaceVisible(true);
    for (auto* label : pageLabels)
        label->setText({}, juce::dontSendNotification);
    for (auto* button : pageActionButtons)
        button->setButtonText({});

    const auto query = presetSearch.getText().trim();
    auto setRows = [this](std::initializer_list<const char*> rows)
    {
        auto index = 0;
        for (const auto* row : rows)
        {
            if (index >= pageLabels.size())
                break;
            pageLabels[index++]->setText(row, juce::dontSendNotification);
        }
    };

    switch (activePage)
    {
        case WorkstationPage::Voice:
            setRows({ "Preset Browser: categories, search, favorites",
                      "8-element voice editor: amp/pitch/filter EG focus",
                      "Mod matrix lanes: velocity, EG, LFO, aftertouch",
                      "Round-robin, random alternates, release samples",
                      "Current search: " });
            pageLabels[4]->setText("Current search: " + (query.isEmpty() ? juce::String("all voices") : query),
                                   juce::dontSendNotification);
            pageLabels[5]->setText(owner.getPresetMetadataSummary(owner.getCurrentPatchName()), juce::dontSendNotification);
            pageLabels[6]->setText(owner.getVoiceEditSummary(0), juce::dontSendNotification);
            pageLabels[7]->setText(owner.getModMatrixSummary(0), juce::dontSendNotification);
            pageActionButtons[0]->setButtonText("Edit Voice");
            pageActionButtons[1]->setButtonText("Mod Matrix");
            pageActionButtons[2]->setButtonText("EG Detail");
            pageActionButtons[3]->setButtonText("Drum Kit");
            break;

        case WorkstationPage::Performance:
            setRows({ "512-performance bank workflow",
                      "4-part split/layer editor with levels and pans",
                      "Scene snapshots switch parts, arps, mixer, FX",
                      "Variation buttons are live and audio-safe",
                      "Performance mode follows the selected slot" });
            pageLabels[5]->setText("Selected: " + owner.getPerformanceName(performanceBrowserBox.getSelectedId() - 1),
                                   juce::dontSendNotification);
            pageLabels[6]->setText("Scene: " + owner.getSceneName(owner.getCurrentPerformanceScene()),
                                   juce::dontSendNotification);
            pageActionButtons[0]->setButtonText("Var 1");
            pageActionButtons[1]->setButtonText("Var 2");
            pageActionButtons[2]->setButtonText(owner.isPerformanceModeEnabled() ? "Perf Off" : "Perf On");
            pageActionButtons[3]->setButtonText("Store Perf");
            break;

        case WorkstationPage::Mix:
            setRows({ "16-part mixer: mute, level, pan, insert sends",
                      "Per-part insert rack maps into the 8-slot FX backend",
                      "System chorus/reverb sends and master strip",
                      "Master EQ/compressor are automatable and saved",
                      "Audio input parts: reserved for hardware input integration" });
            pageLabels[5]->setText("Meters L " + juce::String(owner.getOutputPeakLeft(), 3)
                                       + " R " + juce::String(owner.getOutputPeakRight(), 3),
                                   juce::dontSendNotification);
            pageActionButtons[0]->setButtonText("Part 1");
            pageActionButtons[1]->setButtonText("Save FX");
            pageActionButtons[2]->setButtonText("Load FX");
            pageActionButtons[3]->setButtonText("Template");
            break;

        case WorkstationPage::Arp:
            setRows({ "Four independent performance arp lanes",
                      "Lane 1: Up/Down scene switching",
                      "Lane 2: UpDown/Chord scene switching",
                      "User phrase slots: 256 design target",
                      "Pattern editor surface ready for step editing" });
            pageLabels[5]->setText("Assignments: L1 " + juce::String(owner.getArpLaneAssignment(0))
                                       + " L2 " + juce::String(owner.getArpLaneAssignment(1))
                                       + " L3 " + juce::String(owner.getArpLaneAssignment(2))
                                       + " L4 " + juce::String(owner.getArpLaneAssignment(3)),
                                   juce::dontSendNotification);
            pageActionButtons[0]->setButtonText("Lane 1");
            pageActionButtons[1]->setButtonText("Lane 2");
            pageActionButtons[2]->setButtonText("Phrase");
            pageActionButtons[3]->setButtonText("User Arp");
            break;

        case WorkstationPage::Song:
            setRows({ "Song sequencer: 64 songs, 16 tracks, 480 PPQ",
                      "Demo playback feeds scenes, arps, parts, and FX",
                      "MIDI export/import is wired through the engine",
                      "Piano-roll and event-list editor surfaces",
                      "Current song note count:" });
            pageLabels[4]->setText("Current song note count: " + juce::String(owner.getCurrentSongNoteCount()),
                                   juce::dontSendNotification);
            pageLabels[5]->setText(owner.isLiveRecordingEnabled() ? "Recording: overdub armed" : "Recording: off",
                                   juce::dontSendNotification);
            pageActionButtons[0]->setButtonText("Seed Demo");
            pageActionButtons[1]->setButtonText(owner.isSequencerPlaybackEnabled() ? "Stop" : "Play");
            pageActionButtons[2]->setButtonText("Reset");
            pageActionButtons[3]->setButtonText("Record");
            break;

        case WorkstationPage::Pattern:
            setRows({ "Pattern mode: 64 patterns x 16 sections",
                      "Section A-P phrase assignment workflow",
                      "16 phrase tracks with measure length controls",
                      "Phrase/user arp editing bridge",
                      "Pattern chains reserved for song assembly" });
            pageLabels[5]->setText("Section A phrase " + juce::String(owner.getPatternSectionPhrase(0))
                                       + " notes " + juce::String(owner.getPatternSectionNoteCount(0)),
                                   juce::dontSendNotification);
            pageActionButtons[0]->setButtonText("Section A");
            pageActionButtons[1]->setButtonText("Phrase");
            pageActionButtons[2]->setButtonText("Chain");
            pageActionButtons[3]->setButtonText("Commit");
            break;

        case WorkstationPage::Sample:
            setRows({ "Sample/library manager",
                      "Factory waveform slots and user flash boards",
                      "Drum kit editor: 128 key map surface",
                      "Release samples and alternates tracked by patches",
                      "Licensed large library import remains the final content phase" });
            pageLabels[5]->setText("Indexed samples: " + juce::String(owner.getIndexedSampleCount())
                                       + "  Drum keys: " + juce::String(owner.getMappedDrumKeyCount()),
                                   juce::dontSendNotification);
            pageLabels[6]->setText(owner.getSampleImportReport(), juce::dontSendNotification);
            pageActionButtons[0]->setButtonText("Import");
            pageActionButtons[1]->setButtonText("Map");
            pageActionButtons[2]->setButtonText("Drums");
            pageActionButtons[3]->setButtonText("License");
            break;

        case WorkstationPage::Utility:
            setRows({ "Utility: file, validation, MIDI and audio export",
                      "Export current song to MIDI in Documents",
                      "Bounce the internal demo to WAV",
                      "Undo/redo edit history for workstation operations",
                      "Plugin state stores pages, parts, FX, scenes" });
            pageLabels[5]->setText("Last edit: " + owner.getLastEditDescription(), juce::dontSendNotification);
            pageActionButtons[0]->setButtonText("MIDI Out");
            pageActionButtons[1]->setButtonText("WAV Bounce");
            pageActionButtons[2]->setButtonText("Undo");
            pageActionButtons[3]->setButtonText(owner.canRedo() ? "Redo" : "MIDI In");
            break;

        case WorkstationPage::Demo:
            setRows({ "INNOVATION DEMO: sequencer -> scenes -> 4 arps",
                      "Scene snapshots drive parts, mixer, sends, insert FX",
                      "MIDI 2.0 UMP and MPE-style expression route per-note controls",
                      "Live MIDI flow is visible while the demo plays",
                      "Tick/variation/MPE status updates in the transport" });
            pageLabels[5]->setText(owner.getMidi2ExpressionSummary(), juce::dontSendNotification);
            pageActionButtons[0]->setButtonText("Var 1");
            pageActionButtons[1]->setButtonText(owner.isSequencerPlaybackEnabled() ? "Stop" : "Play");
            pageActionButtons[2]->setButtonText("Reset");
            pageActionButtons[3]->setButtonText("Capture");
            break;
    }
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

        auto* level = partLevelSliders.add(new juce::Slider(juce::Slider::LinearVertical, juce::Slider::NoTextBox));
        level->setRange(0.0, 1.5, 0.01);
        level->setValue(owner.getPartLevel(part), juce::dontSendNotification);
        level->setColour(juce::Slider::trackColourId, accent());
        level->setColour(juce::Slider::thumbColourId, amber());
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
        pan->setColour(juce::Slider::trackColourId, juce::Colour(0xff8aa0b3));
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

    struct MasterControl
    {
        const char* name;
        const char* parameterId;
    };

    for (const auto control : {
             MasterControl { "M EQ L", "masterEqLow" },
             MasterControl { "M EQ M", "masterEqMid" },
             MasterControl { "M EQ H", "masterEqHigh" },
             MasterControl { "M CMP T", "masterCompThreshold" },
             MasterControl { "M CMP R", "masterCompRatio" },
             MasterControl { "M CMP +", "masterCompMakeup" } })
    {
        auto* slider = masterFxSliders.add(new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight));
        slider->setName(control.name);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 16);
        slider->setColour(juce::Slider::trackColourId, juce::Colour(0xfff2b84b));
        slider->setColour(juce::Slider::thumbColourId, accent());
        slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        sliderAttachments.add(new SliderAttachment(owner.getParameters(), control.parameterId, *slider));
        auto& label = addPanelLabel(control.name, juce::Justification::centredLeft);
        label.attachToComponent(slider, true);
        addAndMakeVisible(slider);
    }
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

    const auto b = bounds.toFloat();
    const auto rounded = panel == PanelId::Header ? 3.0f : 4.0f;
    const auto isDisplay = panel == PanelId::Display || panel == PanelId::ElementMonitor;
    const auto isHeader = panel == PanelId::Header;

    g.setColour(juce::Colour(0xff050607).withAlpha(0.72f));
    g.fillRoundedRectangle(b.translated(0.0f, 2.0f), rounded);

    juce::ColourGradient fill(isHeader ? juce::Colour(0xff20262d) : juce::Colour(0xff232932),
                              b.getX(), b.getY(),
                              isDisplay ? juce::Colour(0xff070a0d) : juce::Colour(0xff11161c),
                              b.getX(), b.getBottom(), false);
    fill.addColour(0.16, isHeader ? juce::Colour(0xff303842) : juce::Colour(0xff2b323b));
    fill.addColour(0.58, isDisplay ? juce::Colour(0xff0d1115) : panelFill());
    g.setGradientFill(fill);
    g.fillRoundedRectangle(b, rounded);

    g.setColour(juce::Colour(0xffffffff).withAlpha(0.08f));
    g.drawLine(b.getX() + 2.0f, b.getY() + 1.0f, b.getRight() - 2.0f, b.getY() + 1.0f, 1.0f);
    g.setColour(isHeader ? accent().withAlpha(0.9f) : panelLine());
    g.drawRoundedRectangle(b.reduced(0.5f), rounded, isHeader ? 1.4f : 1.0f);

    if (!isHeader)
    {
        auto titleStrip = b.reduced(1.0f).removeFromTop(24.0f);
        g.setColour(isDisplay ? juce::Colour(0xff10150f) : juce::Colour(0xff10151b));
        g.fillRoundedRectangle(titleStrip, 3.0f);
        g.setColour(isDisplay ? accent().withAlpha(0.36f) : juce::Colour(0xff4a5360).withAlpha(0.55f));
        g.drawLine(titleStrip.getX() + 8.0f, titleStrip.getBottom() - 1.0f, titleStrip.getRight() - 8.0f, titleStrip.getBottom() - 1.0f);
    }

    if (panelTitle.isNotEmpty())
    {
        g.setColour(panel == PanelId::Display ? amber() : juce::Colour(0xffb6c0cb));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(panelTitle.toUpperCase(), bounds.reduced(12, 7).removeFromTop(16), juce::Justification::centredLeft);
        if (panel != PanelId::Display)
        {
            g.setColour(juce::Colour(0xff6f7b87).withAlpha(0.45f));
            const auto titleWidth = juce::jlimit(58.0f, 180.0f, static_cast<float>(panelTitle.length()) * 7.0f + 24.0f);
            g.drawLine(b.getX() + titleWidth, b.getY() + 16.0f, b.getRight() - 12.0f, b.getY() + 16.0f, 0.8f);
        }
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
    if (graphicalEditor != nullptr)
        graphicalEditor->repaint();
}
