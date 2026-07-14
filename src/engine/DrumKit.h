#pragma once

#include <array>
#include <optional>
#include <string>

namespace chimera::engine
{
struct DrumKey
{
    int midiNote = 60;
    int waveformId = 0;
    std::string name;
    float level = 1.0f;
    float pan = 0.0f;
    int outputBus = 0;
    bool chokeGroupEnabled = false;
    int chokeGroup = 0;
};

class DrumKit
{
public:
    static constexpr int keyCount = 128;

    bool setKey(DrumKey key);
    std::optional<DrumKey> getKey(int midiNote) const;
    bool hasKey(int midiNote) const;
    int mappedKeyCount() const;

private:
    std::array<std::optional<DrumKey>, keyCount> keys {};
};
}
