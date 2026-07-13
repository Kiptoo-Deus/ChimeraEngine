#include "dsp/ModulationMatrix.h"
#include <algorithm>

namespace chimera::dsp
{
void ModulationMatrix::setFreeSlot(int index, ModSlot slot)
{
    if (index >= 0 && index < freeSlotCount)
        freeSlots[static_cast<size_t>(index)] = slot;
}

float ModulationMatrix::sourceValue(ModSource source, float velocity, float pitchEg, float filterEg,
                                    float ampEg, float lfo1, float lfo2, float aftertouch)
{
    switch (source)
    {
        case ModSource::Velocity: return velocity;
        case ModSource::PitchEnvelope: return pitchEg;
        case ModSource::FilterEnvelope: return filterEg;
        case ModSource::AmpEnvelope: return ampEg;
        case ModSource::Lfo1: return lfo1;
        case ModSource::Lfo2: return lfo2;
        case ModSource::Aftertouch: return aftertouch;
    }

    return 0.0f;
}

int ModulationMatrix::destinationIndex(ModDestination destination)
{
    switch (destination)
    {
        case ModDestination::Pitch: return 0;
        case ModDestination::Cutoff: return 1;
        case ModDestination::Amp: return 2;
        case ModDestination::Pan: return 3;
    }

    return 0;
}

std::array<float, 4> ModulationMatrix::evaluate(float velocity, float pitchEg, float filterEg, float ampEg,
                                                float lfo1, float lfo2, float aftertouch) const
{
    std::array<float, 4> destinations {
        pitchEg + lfo1 * 0.05f,
        filterEg + velocity * 0.25f + lfo1 * 0.1f,
        ampEg * std::clamp(velocity, 0.0f, 1.0f) + lfo2 * 0.05f,
        lfo2 * 0.5f
    };

    for (const auto& slot : freeSlots)
    {
        if (!slot.enabled)
            continue;

        destinations[static_cast<size_t>(destinationIndex(slot.destination))]
            += sourceValue(slot.source, velocity, pitchEg, filterEg, ampEg, lfo1, lfo2, aftertouch) * slot.depth;
    }

    return destinations;
}
}
