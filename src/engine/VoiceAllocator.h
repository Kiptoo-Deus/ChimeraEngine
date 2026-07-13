#pragma once

#include <vector>

namespace chimera::engine
{
struct VoiceState
{
    int note = -1;
    int age = 0;
    bool active = false;
};

class VoiceAllocator
{
public:
    explicit VoiceAllocator(int voiceCount = 16);
    int noteOn(int midiNote);
    void noteOff(int midiNote);
    const std::vector<VoiceState>& getVoices() const { return voices; }

private:
    std::vector<VoiceState> voices;
    int counter = 0;
};
}
