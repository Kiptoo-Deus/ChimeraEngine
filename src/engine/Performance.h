#pragma once

#include <array>

namespace chimera::engine
{
struct PartZone
{
    int keyLow = 0;
    int keyHigh = 127;
    int velocityLow = 1;
    int velocityHigh = 127;
    int midiChannel = 1;
    bool enabled = false;
};

class Performance
{
public:
    static constexpr int partCount = 4;
    void setPart(int index, PartZone zone);
    bool partMatches(int index, int midiNote, int velocity, int midiChannel) const;

private:
    std::array<PartZone, partCount> parts {};
};
}
