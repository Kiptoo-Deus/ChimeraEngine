#pragma once

#include <array>
#include <string>
#include <vector>

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
    int internalPartIndex = 0;
    float level = 1.0f;
    float pan = 0.0f;
    std::string voiceName;
};

struct PerformanceSlot
{
    std::string name { "Init Performance" };
    std::array<PartZone, 4> parts {};
};

class Performance
{
public:
    static constexpr int partCount = 4;
    void setPart(int index, PartZone zone);
    const PartZone& getPart(int index) const;
    bool partMatches(int index, int midiNote, int velocity, int midiChannel) const;
    std::vector<int> matchingInternalParts(int midiNote, int velocity, int midiChannel) const;

private:
    static const PartZone disabledPart;
    std::array<PartZone, partCount> parts {};
};

class PerformanceBank
{
public:
    static constexpr int slotCount = 512;

    void setPerformance(int index, PerformanceSlot slot);
    const PerformanceSlot& getPerformance(int index) const;
    int size() const { return slotCount; }

private:
    static const PerformanceSlot initSlot;
    std::array<PerformanceSlot, slotCount> slots {};
};
}
