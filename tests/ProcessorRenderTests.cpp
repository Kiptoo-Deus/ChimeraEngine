#include "PluginProcessor.h"

#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    ChimeraEngineAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);

    processor.processBlock(buffer, midi);

    auto sum = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            sum += std::abs(buffer.getSample(channel, sample));

    assert(sum > 0.01f);
    assert(std::isfinite(sum));

    std::cout << "Processor render test passed\n";
    return 0;
}
