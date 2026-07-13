#pragma once

#include <cstdint>

namespace chimera::dsp
{
enum class LfoShape
{
    Sine,
    Triangle,
    Saw,
    Square,
    SampleAndHold
};

class Lfo
{
public:
    void setSampleRate(double newSampleRate);
    void setFrequency(float hz);
    void setShape(LfoShape newShape);
    void reset(float phase01 = 0.0f);
    float process();

private:
    uint32_t nextRandom();

    double sampleRate = 44100.0;
    float frequency = 1.0f;
    float phase = 0.0f;
    float heldValue = 0.0f;
    uint32_t rng = 0x12345678u;
    LfoShape shape = LfoShape::Sine;
};
}
