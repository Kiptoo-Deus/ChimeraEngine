#include "dsp/SamplePlayer.h"
#include <cmath>

namespace chimera::dsp
{
void SamplePlayer::setZone(std::shared_ptr<SampleZone> newZone)
{
    zone = std::move(newZone);
}

void SamplePlayer::start(int midiNote, double outputSampleRate)
{
    position = 0.0;
    playing = zone != nullptr && zone->getNumSamples() > 0 && outputSampleRate > 0.0;

    if (!playing)
        return;

    const auto semitones = static_cast<double>(midiNote - zone->getRootKey()) + zone->getTuningCents() / 100.0;
    const auto pitchRatio = std::pow(2.0, semitones / 12.0);
    increment = pitchRatio * zone->getSourceSampleRate() / outputSampleRate;
}

void SamplePlayer::stop()
{
    playing = false;
}

float SamplePlayer::process()
{
    if (!playing || zone == nullptr)
        return 0.0f;

    const auto index = static_cast<int64_t>(position);
    if (index >= zone->getNumSamples() - 1)
    {
        playing = false;
        return 0.0f;
    }

    const auto frac = static_cast<float>(position - static_cast<double>(index));
    const auto a = zone->sample(0, index);
    const auto b = zone->sample(0, index + 1);
    position += increment;
    return a + (b - a) * frac;
}
}
