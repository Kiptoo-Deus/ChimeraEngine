#include "dsp/Lfo.h"
#include <algorithm>
#include <cmath>

namespace chimera::dsp
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
}

void Lfo::setSampleRate(double newSampleRate)
{
    sampleRate = std::max(1.0, newSampleRate);
}

void Lfo::setFrequency(float hz)
{
    frequency = std::max(0.0f, hz);
}

void Lfo::setShape(LfoShape newShape)
{
    shape = newShape;
}

void Lfo::reset(float phase01)
{
    phase = phase01 - std::floor(phase01);
}

uint32_t Lfo::nextRandom()
{
    rng = rng * 1664525u + 1013904223u;
    return rng;
}

float Lfo::process()
{
    float out = 0.0f;
    switch (shape)
    {
        case LfoShape::Sine:
            out = std::sin(phase * 2.0f * pi);
            break;
        case LfoShape::Triangle:
            out = 4.0f * std::abs(phase - 0.5f) - 1.0f;
            break;
        case LfoShape::Saw:
            out = phase * 2.0f - 1.0f;
            break;
        case LfoShape::Square:
            out = phase < 0.5f ? 1.0f : -1.0f;
            break;
        case LfoShape::SampleAndHold:
            out = heldValue;
            break;
    }

    const auto previousPhase = phase;
    phase += frequency / static_cast<float>(sampleRate);
    phase -= std::floor(phase);

    if (shape == LfoShape::SampleAndHold && phase < previousPhase)
        heldValue = (static_cast<float>(nextRandom() >> 8) / 8388607.5f) - 1.0f;

    return std::clamp(out, -1.0f, 1.0f);
}
}
