#include "dsp/SampleZone.h"
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

bool SampleZone::matches(int midiNote, int velocity) const
{
    return midiNote >= keyLow && midiNote <= keyHigh
        && velocity >= velocityLow && velocity <= velocityHigh;
}
}
