#include "dsp/SampleZone.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>

namespace chimera::dsp
{
void SampleZone::setSource(juce::File newSource)
{
    source = std::move(newSource);
}

void SampleZone::setRootKey(int midiNote)
{
    rootKey = std::clamp(midiNote, 0, 127);
}

void SampleZone::setKeyRange(int low, int high)
{
    keyLow = std::clamp(std::min(low, high), 0, 127);
    keyHigh = std::clamp(std::max(low, high), 0, 127);
}

void SampleZone::setVelocityRange(int low, int high)
{
    velocityLow = std::clamp(std::min(low, high), 1, 127);
    velocityHigh = std::clamp(std::max(low, high), 1, 127);
}

void SampleZone::setLoop(std::optional<LoopRegion> newLoop)
{
    loop = newLoop;
}

void SampleZone::setTuningCents(float cents)
{
    tuningCents = cents;
}

juce::Result SampleZone::loadAudio()
{
    if (!source.existsAsFile())
        return juce::Result::fail("Sample file does not exist: " + source.getFullPathName());

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
    if (reader == nullptr)
        return juce::Result::fail("Unsupported sample format: " + source.getFullPathName());

    if (reader->lengthInSamples <= 0 || reader->numChannels <= 0)
        return juce::Result::fail("Sample is empty: " + source.getFullPathName());

    audio.setSize(static_cast<int>(reader->numChannels),
                  static_cast<int>(std::min<int64_t>(reader->lengthInSamples, std::numeric_limits<int>::max())),
                  false, true, false);

    if (!reader->read(&audio, 0, audio.getNumSamples(), 0, true, true))
        return juce::Result::fail("Could not read sample data: " + source.getFullPathName());

    sourceSampleRate = reader->sampleRate;
    return juce::Result::ok();
}

bool SampleZone::matches(int midiNote, int velocity) const
{
    return midiNote >= keyLow && midiNote <= keyHigh
        && velocity >= velocityLow && velocity <= velocityHigh;
}

float SampleZone::sample(int channel, int64_t index) const
{
    if (channel < 0 || channel >= audio.getNumChannels() || index < 0 || index >= audio.getNumSamples())
        return 0.0f;

    return audio.getSample(channel, static_cast<int>(index));
}
}
