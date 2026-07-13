#include "engine/Performance.h"

namespace chimera::engine
{
const PartZone Performance::disabledPart {};
const PerformanceSlot PerformanceBank::initSlot {};

void Performance::setPart(int index, PartZone zone)
{
    if (index >= 0 && index < partCount)
        parts[static_cast<size_t>(index)] = zone;
}

const PartZone& Performance::getPart(int index) const
{
    if (index < 0 || index >= partCount)
        return disabledPart;

    return parts[static_cast<size_t>(index)];
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

std::vector<int> Performance::matchingInternalParts(int midiNote, int velocity, int midiChannel) const
{
    std::vector<int> matches;
    for (int index = 0; index < partCount; ++index)
        if (partMatches(index, midiNote, velocity, midiChannel))
            matches.push_back(parts[static_cast<size_t>(index)].internalPartIndex);

    return matches;
}

void PerformanceBank::setPerformance(int index, PerformanceSlot slot)
{
    if (index >= 0 && index < slotCount)
        slots[static_cast<size_t>(index)] = std::move(slot);
}

const PerformanceSlot& PerformanceBank::getPerformance(int index) const
{
    if (index < 0 || index >= slotCount)
        return initSlot;

    return slots[static_cast<size_t>(index)];
}
}
