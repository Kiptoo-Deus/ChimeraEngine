#pragma once

#include "dsp/SampleZone.h"
#include <memory>

namespace chimera::dsp
{
class SamplePlayer
{
public:
    void setZone(std::shared_ptr<SampleZone> newZone);
    void start(int midiNote, double outputSampleRate);
    void stop();
    float process(float pitchRatio = 1.0f);
    bool isPlaying() const { return playing; }
    double getPosition() const { return position; }

private:
    std::shared_ptr<SampleZone> zone;
    double position = 0.0;
    double increment = 1.0;
    bool playing = false;
};
}
