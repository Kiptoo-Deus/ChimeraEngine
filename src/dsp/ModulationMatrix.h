#pragma once

#include <array>

namespace chimera::dsp
{
enum class ModSource
{
    Velocity,
    PitchEnvelope,
    FilterEnvelope,
    AmpEnvelope,
    Lfo1,
    Lfo2,
    Aftertouch
};

enum class ModDestination
{
    Pitch,
    Cutoff,
    Amp,
    Pan
};

struct ModSlot
{
    ModSource source = ModSource::Velocity;
    ModDestination destination = ModDestination::Amp;
    float depth = 0.0f;
    bool enabled = false;
};

class ModulationMatrix
{
public:
    static constexpr int freeSlotCount = 4;

    void setFreeSlot(int index, ModSlot slot);
    std::array<float, 4> evaluate(float velocity, float pitchEg, float filterEg, float ampEg,
                                  float lfo1, float lfo2, float aftertouch) const;

private:
    static float sourceValue(ModSource source, float velocity, float pitchEg, float filterEg,
                             float ampEg, float lfo1, float lfo2, float aftertouch);
    static int destinationIndex(ModDestination destination);

    std::array<ModSlot, freeSlotCount> freeSlots {};
};
}
