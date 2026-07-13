#include "PluginProcessor.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
float renderAndSum(ChimeraEngineAudioProcessor& processor, juce::MidiBuffer& midi, int samples)
{
    juce::AudioBuffer<float> buffer(2, samples);
    processor.processBlock(buffer, midi);

    auto sum = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            sum += std::abs(buffer.getSample(channel, sample));

    return sum;
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

    std::cout << "Processor render test passed\n";
    return 0;
}
