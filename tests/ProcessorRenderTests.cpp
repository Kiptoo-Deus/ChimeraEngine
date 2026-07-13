#include "PluginProcessor.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
std::pair<float, float> renderAndChannelSums(ChimeraEngineAudioProcessor& processor, juce::MidiBuffer& midi, int samples)
{
    juce::AudioBuffer<float> buffer(2, samples);
    processor.processBlock(buffer, midi);

    auto left = 0.0f;
    auto right = 0.0f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        left += std::abs(buffer.getSample(0, sample));
        right += std::abs(buffer.getSample(1, sample));
    }

    return { left, right };
}

float renderAndSum(ChimeraEngineAudioProcessor& processor, juce::MidiBuffer& midi, int samples)
{
    const auto [left, right] = renderAndChannelSums(processor, midi, samples);
    return left + right;
}
}

int main()
{
    ChimeraEngineAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);

    const auto sum = renderAndSum(processor, midi, 512);
    assert(sum > 0.01f);
    assert(std::isfinite(sum));

    juce::MidiBuffer chord;
    chord.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    chord.addEvent(juce::MidiMessage::noteOn(1, 64, juce::uint8(100)), 0);
    chord.addEvent(juce::MidiMessage::noteOn(1, 67, juce::uint8(100)), 0);

    ChimeraEngineAudioProcessor chordProcessor;
    chordProcessor.prepareToPlay(48000.0, 512);
    const auto chordSum = renderAndSum(chordProcessor, chord, 512);
    assert(chordSum > 0.01f);
    assert(std::isfinite(chordSum));

    juce::MidiBuffer release;
    release.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    const auto releaseSum = renderAndSum(chordProcessor, release, 128);
    assert(releaseSum > 0.0f);

    ChimeraEngineAudioProcessor previewProcessor;
    previewProcessor.prepareToPlay(48000.0, 512);
    previewProcessor.enqueuePreviewNoteOn(1, 72, 0.8f);
    juce::MidiBuffer emptyMidi;
    const auto previewSum = renderAndSum(previewProcessor, emptyMidi, 512);
    assert(previewSum > 0.01f);

    ChimeraEngineAudioProcessor presetProcessor;
    presetProcessor.prepareToPlay(48000.0, 512);
    assert(presetProcessor.loadSynthPreset("Saw").wasOk());
    assert(presetProcessor.getCurrentPatchName() == "Saw");
    juce::MidiBuffer sawMidi;
    sawMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    assert(renderAndSum(presetProcessor, sawMidi, 512) > 0.01f);
    assert(presetProcessor.loadSynthPreset("Square").wasOk());
    assert(presetProcessor.getCurrentPatchName() == "Square");
    assert(presetProcessor.loadSynthPreset("Stack").wasOk());
    assert(presetProcessor.getCurrentPatchName() == "Stack");
    juce::MidiBuffer stackMidi;
    stackMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    assert(renderAndSum(presetProcessor, stackMidi, 512) > 0.01f);
    assert(presetProcessor.loadSynthPreset("Velocity Split").wasOk());
    assert(presetProcessor.getCurrentPatchName() == "Velocity Split");

    juce::MidiBuffer lowVelocityMidi;
    lowVelocityMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(40)), 0);
    const auto lowVelocitySum = renderAndSum(presetProcessor, lowVelocityMidi, 512);
    assert(lowVelocitySum > 0.01f);

    ChimeraEngineAudioProcessor midVelocityProcessor;
    midVelocityProcessor.prepareToPlay(48000.0, 512);
    assert(midVelocityProcessor.loadSynthPreset("Velocity Split").wasOk());
    juce::MidiBuffer midVelocityMidi;
    midVelocityMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(90)), 0);
    const auto midVelocitySum = renderAndSum(midVelocityProcessor, midVelocityMidi, 512);
    assert(midVelocitySum > 0.01f);

    ChimeraEngineAudioProcessor highVelocityProcessor;
    highVelocityProcessor.prepareToPlay(48000.0, 512);
    assert(highVelocityProcessor.loadSynthPreset("Velocity Split").wasOk());
    juce::MidiBuffer highVelocityMidi;
    highVelocityMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(120)), 0);
    const auto highVelocitySum = renderAndSum(highVelocityProcessor, highVelocityMidi, 512);
    assert(highVelocitySum > 0.01f);
    assert(std::abs(lowVelocitySum - highVelocitySum) > 0.001f);

    ChimeraEngineAudioProcessor stereoProcessor;
    stereoProcessor.prepareToPlay(48000.0, 512);
    assert(stereoProcessor.loadSynthPreset("Stack").wasOk());
    juce::MidiBuffer stereoMidi;
    stereoMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    const auto [left, right] = renderAndChannelSums(stereoProcessor, stereoMidi, 512);
    assert(left > 0.01f && right > 0.01f);
    assert(std::abs(left - right) > 0.001f);

    std::cout << "Processor render test passed\n";
    return 0;
}
