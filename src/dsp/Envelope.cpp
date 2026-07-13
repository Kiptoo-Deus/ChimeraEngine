#include "dsp/Envelope.h"
#include <algorithm>
#include <cmath>

namespace chimera::dsp
{
void Envelope::setSampleRate(double newSampleRate)
{
    sampleRate = std::max(1.0, newSampleRate);
}

void Envelope::setStages(float attackSeconds, float decay1Seconds, float decay2Seconds,
                         float sustainLevel, float releaseSeconds, Curve newCurve)
{
    attack = std::max(0.0f, attackSeconds);
    decay1 = std::max(0.0f, decay1Seconds);
    decay2 = std::max(0.0f, decay2Seconds);
    sustain = std::clamp(sustainLevel, 0.0f, 1.0f);
    release = std::max(0.0f, releaseSeconds);
    curve = newCurve;
}

void Envelope::noteOn()
{
    stage = Stage::Attack;
}

void Envelope::noteOff()
{
    stage = Stage::Release;
}

bool Envelope::isActive() const
{
    return stage != Stage::Idle;
}

float Envelope::advanceToward(float target, float seconds)
{
    if (seconds <= 0.0f)
        return target;

    const auto step = 1.0f / static_cast<float>(seconds * sampleRate);
    if (curve == Curve::Linear)
        return value + std::clamp(target - value, -step, step);

    const auto coeff = 1.0f - std::exp(-1.0f / std::max(1.0f, seconds * static_cast<float>(sampleRate) * 0.2f));
    return value + (target - value) * coeff;
}

float Envelope::process()
{
    switch (stage)
    {
        case Stage::Idle:
            value = 0.0f;
            break;
        case Stage::Attack:
            value = advanceToward(1.0f, attack);
            if (value >= 0.999f)
            {
                value = 1.0f;
                stage = Stage::Decay1;
            }
            break;
        case Stage::Decay1:
            value = advanceToward(0.8f, decay1);
            if (std::abs(value - 0.8f) < 0.001f)
                stage = Stage::Decay2;
            break;
        case Stage::Decay2:
            value = advanceToward(sustain, decay2);
            if (std::abs(value - sustain) < 0.001f)
                stage = Stage::Sustain;
            break;
        case Stage::Sustain:
            value = sustain;
            break;
        case Stage::Release:
            value = advanceToward(0.0f, release);
            if (value <= 0.001f)
            {
                value = 0.0f;
                stage = Stage::Idle;
            }
            break;
    }

    return std::clamp(value, 0.0f, 1.0f);
}
}
