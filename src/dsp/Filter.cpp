#include "dsp/Filter.h"
#include <algorithm>
#include <cmath>

namespace chimera::dsp
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
}

void Filter::setSampleRate(double newSampleRate)
{
    sampleRate = std::max(1.0, newSampleRate);
    update();
}

void Filter::setMode(FilterMode newMode)
{
    mode = newMode;
    update();
}

void Filter::setCutoff(float hz)
{
    cutoff = std::max(10.0f, hz);
    update();
}

void Filter::setResonance(float q)
{
    resonance = std::clamp(q, 0.1f, 20.0f);
    update();
}

void Filter::reset()
{
    z1 = z2 = z3 = z4 = 0.0f;
}

void Filter::update()
{
    const auto nyquist = static_cast<float>(sampleRate * 0.5);
    const auto f = std::clamp(cutoff, 10.0f, nyquist - 10.0f);
    const auto omega = 2.0f * pi * f / static_cast<float>(sampleRate);
    const auto sn = std::sin(omega);
    const auto cs = std::cos(omega);
    const auto alpha = sn / (2.0f * resonance);

    float b0 = 1.0f;
    float b1n = 0.0f;
    float b2n = 0.0f;
    float a0n = 1.0f;
    float a1n = 0.0f;
    float a2n = 0.0f;

    switch (mode)
    {
        case FilterMode::LowPass12:
        case FilterMode::LowPass24:
            b0 = (1.0f - cs) * 0.5f;
            b1n = 1.0f - cs;
            b2n = (1.0f - cs) * 0.5f;
            a0n = 1.0f + alpha;
            a1n = -2.0f * cs;
            a2n = 1.0f - alpha;
            break;
        case FilterMode::HighPass:
            b0 = (1.0f + cs) * 0.5f;
            b1n = -(1.0f + cs);
            b2n = (1.0f + cs) * 0.5f;
            a0n = 1.0f + alpha;
            a1n = -2.0f * cs;
            a2n = 1.0f - alpha;
            break;
        case FilterMode::BandPass:
            b0 = alpha;
            b1n = 0.0f;
            b2n = -alpha;
            a0n = 1.0f + alpha;
            a1n = -2.0f * cs;
            a2n = 1.0f - alpha;
            break;
        case FilterMode::Notch:
            b0 = 1.0f;
            b1n = -2.0f * cs;
            b2n = 1.0f;
            a0n = 1.0f + alpha;
            a1n = -2.0f * cs;
            a2n = 1.0f - alpha;
            break;
    }

    a0 = b0 / a0n;
    a1 = b1n / a0n;
    a2 = b2n / a0n;
    b1 = a1n / a0n;
    b2 = a2n / a0n;
}

float Filter::processOnePolePair(float input)
{
    const auto output = a0 * input + z1;
    z1 = a1 * input - b1 * output + z2;
    z2 = a2 * input - b2 * output;
    return output;
}

float Filter::process(float input)
{
    auto output = processOnePolePair(input);
    if (mode == FilterMode::LowPass24)
    {
        const auto savedZ1 = z1;
        const auto savedZ2 = z2;
        z1 = z3;
        z2 = z4;
        output = processOnePolePair(output);
        z3 = z1;
        z4 = z2;
        z1 = savedZ1;
        z2 = savedZ2;
    }
    return output;
}
}
