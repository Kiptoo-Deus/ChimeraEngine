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
    static_assert(ChimeraEngineAudioProcessor::getPartCount() == 16);
    static_assert(ChimeraEngineAudioProcessor::getMaxVoiceCount() == 128);

    ChimeraEngineAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);

    const auto sum = renderAndSum(processor, midi, 512);
    assert(sum > 0.01f);
    assert(std::isfinite(sum));

    ChimeraEngineAudioProcessor wetFxProcessor;
    wetFxProcessor.prepareToPlay(48000.0, 512);
    wetFxProcessor.getParameters().getParameter("fxMix")->setValueNotifyingHost(1.0f);
    juce::MidiBuffer wetFxMidi;
    wetFxMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    const auto wetFxSum = renderAndSum(wetFxProcessor, wetFxMidi, 512);
    assert(wetFxSum > 0.01f);
    assert(std::isfinite(wetFxSum));

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

    ChimeraEngineAudioProcessor arpProcessor;
    arpProcessor.prepareToPlay(48000.0, 512);
    arpProcessor.getParameters().getParameter("arpEnabled")->setValueNotifyingHost(1.0f);
    juce::MidiBuffer arpChord;
    arpChord.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    arpChord.addEvent(juce::MidiMessage::noteOn(1, 64, juce::uint8(100)), 0);
    arpChord.addEvent(juce::MidiMessage::noteOn(1, 67, juce::uint8(100)), 0);
    assert(renderAndSum(arpProcessor, arpChord, 512) > 0.01f);

    auto arpContinuationSum = 0.0f;
    for (int block = 0; block < 16; ++block)
    {
        juce::MidiBuffer noEvents;
        arpContinuationSum += renderAndSum(arpProcessor, noEvents, 512);
    }
    assert(arpContinuationSum > 0.01f);

    ChimeraEngineAudioProcessor multiPartProcessor;
    multiPartProcessor.prepareToPlay(48000.0, 512);
    assert(multiPartProcessor.loadSynthPresetForPart(1, "Stack").wasOk());
    assert(multiPartProcessor.getPartPatchName(1) == "Stack");
    juce::MidiBuffer multiPartMidi;
    multiPartMidi.addEvent(juce::MidiMessage::noteOn(2, 60, juce::uint8(100)), 0);
    assert(renderAndSum(multiPartProcessor, multiPartMidi, 512) > 0.01f);

    ChimeraEngineAudioProcessor performanceProcessor;
    performanceProcessor.prepareToPlay(48000.0, 512);
    assert(performanceProcessor.loadSynthPresetForPart(0, "Stack").wasOk());
    assert(performanceProcessor.loadSynthPresetForPart(1, "Velocity Split").wasOk());
    performanceProcessor.setPerformancePart(0, { 0, 59, 1, 127, 1, true, 0, 1.0f, -0.2f, "Lower" });
    performanceProcessor.setPerformancePart(1, { 60, 127, 1, 127, 1, true, 1, 0.9f, 0.2f, "Upper" });
    performanceProcessor.setPerformanceModeEnabled(true);
    assert(performanceProcessor.isPerformanceModeEnabled());

    juce::MidiBuffer lowerPerformanceMidi;
    lowerPerformanceMidi.addEvent(juce::MidiMessage::noteOn(1, 48, juce::uint8(100)), 0);
    assert(renderAndSum(performanceProcessor, lowerPerformanceMidi, 512) > 0.01f);

    juce::MidiBuffer upperPerformanceMidi;
    upperPerformanceMidi.addEvent(juce::MidiMessage::noteOn(1, 72, juce::uint8(120)), 0);
    assert(renderAndSum(performanceProcessor, upperPerformanceMidi, 512) > 0.01f);

    ChimeraEngineAudioProcessor pannedPerformanceProcessor;
    pannedPerformanceProcessor.prepareToPlay(48000.0, 512);
    pannedPerformanceProcessor.setPerformancePart(0, { 0, 127, 1, 127, 1, true, 0, 1.0f, -1.0f, "Hard Left" });
    pannedPerformanceProcessor.setPerformanceModeEnabled(true);
    juce::MidiBuffer pannedPerformanceMidi;
    pannedPerformanceMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    const auto [performanceLeft, performanceRight] = renderAndChannelSums(pannedPerformanceProcessor, pannedPerformanceMidi, 512);
    assert(performanceLeft > 0.01f);
    assert(performanceRight < performanceLeft * 0.2f);

    ChimeraEngineAudioProcessor stateSourceProcessor;
    stateSourceProcessor.prepareToPlay(48000.0, 512);
    assert(stateSourceProcessor.loadSynthPresetForPart(1, "Stack").wasOk());
    stateSourceProcessor.setPerformancePart(0, { 0, 127, 1, 127, 2, true, 1, 1.0f, 0.0f, "Restored Part" });
    stateSourceProcessor.setPerformanceModeEnabled(true);
    juce::MemoryBlock stateData;
    stateSourceProcessor.getStateInformation(stateData);

    ChimeraEngineAudioProcessor restoredStateProcessor;
    restoredStateProcessor.prepareToPlay(48000.0, 512);
    restoredStateProcessor.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
    assert(restoredStateProcessor.isPerformanceModeEnabled());
    assert(restoredStateProcessor.getPartPatchName(1) == "Stack");
    juce::MidiBuffer restoredPartMidi;
    restoredPartMidi.addEvent(juce::MidiMessage::noteOn(2, 60, juce::uint8(100)), 0);
    assert(renderAndSum(restoredStateProcessor, restoredPartMidi, 512) > 0.01f);

    std::cout << "Processor render test passed\n";
    return 0;
}
