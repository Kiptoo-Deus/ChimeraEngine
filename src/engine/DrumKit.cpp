#include "engine/DrumKit.h"

#include <algorithm>

namespace chimera::engine
{
bool DrumKit::setKey(DrumKey key)
{
    if (key.midiNote < 0 || key.midiNote >= keyCount)
        return false;
    if (key.waveformId <= 0 || key.name.empty())
        return false;

    key.level = std::clamp(key.level, 0.0f, 2.0f);
    key.pan = std::clamp(key.pan, -1.0f, 1.0f);
    key.outputBus = std::clamp(key.outputBus, 0, 15);
    key.chokeGroup = std::clamp(key.chokeGroup, 0, 31);
    keys[static_cast<size_t>(key.midiNote)] = std::move(key);
    return true;
}

std::optional<DrumKey> DrumKit::getKey(int midiNote) const
{
    if (midiNote < 0 || midiNote >= keyCount)
        return std::nullopt;

    return keys[static_cast<size_t>(midiNote)];
}

bool DrumKit::hasKey(int midiNote) const
{
    return getKey(midiNote).has_value();
}

int DrumKit::mappedKeyCount() const
{
    return static_cast<int>(std::count_if(keys.begin(), keys.end(),
                                          [](const auto& key) { return key.has_value(); }));
}
}
