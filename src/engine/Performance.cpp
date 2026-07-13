#include "engine/Performance.h"

namespace chimera::engine
{
void Performance::setPart(int index, PartZone zone)
{
    if (index >= 0 && index < partCount)
        parts[static_cast<size_t>(index)] = zone;
}

bool Performance::partMatches(int index, int midiNote, int velocity, int midiChannel) const
{
    if (index < 0 || index >= partCount)
        return false;

    const auto& part = parts[static_cast<size_t>(index)];
    return part.enabled
        && midiChannel == part.midiChannel
        && midiNote >= part.keyLow && midiNote <= part.keyHigh
        && velocity >= part.velocityLow && velocity <= part.velocityHigh;
}
}
