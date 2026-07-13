#pragma once

#include <juce_core/juce_core.h>
#include <optional>

namespace chimera::dsp
{
struct LoopRegion
{
    int64_t start = 0;
    int64_t end = 0;
    int64_t crossfade = 0;
};

class SampleZone
{
public:
    void setSource(juce::File newSource);
    void setRootKey(int midiNote);
    void setKeyRange(int low, int high);
    void setVelocityRange(int low, int high);
    void setLoop(std::optional<LoopRegion> newLoop);
    void setTuningCents(float cents);

    bool matches(int midiNote, int velocity) const;
    const juce::File& getSource() const { return source; }
    int getRootKey() const { return rootKey; }
    float getTuningCents() const { return tuningCents; }

private:
    juce::File source;
    int rootKey = 60;
    int keyLow = 0;
    int keyHigh = 127;
    int velocityLow = 1;
    int velocityHigh = 127;
    float tuningCents = 0.0f;
    std::optional<LoopRegion> loop;
};
}
