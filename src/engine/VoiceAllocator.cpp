#include "engine/VoiceAllocator.h"
#include <algorithm>

namespace chimera::engine
{
VoiceAllocator::VoiceAllocator(int voiceCount)
    : voices(static_cast<size_t>(std::max(1, voiceCount)))
{
}

int VoiceAllocator::noteOn(int midiNote)
{
    auto it = std::find_if(voices.begin(), voices.end(), [](const auto& voice) { return !voice.active; });
    if (it == voices.end())
        it = std::min_element(voices.begin(), voices.end(), [](const auto& a, const auto& b) { return a.age < b.age; });

    it->note = midiNote;
    it->active = true;
    it->age = ++counter;
    return static_cast<int>(std::distance(voices.begin(), it));
}

void VoiceAllocator::noteOff(int midiNote)
{
    for (auto& voice : voices)
        if (voice.active && voice.note == midiNote)
            voice.active = false;
}
}
