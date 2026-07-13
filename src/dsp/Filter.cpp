#include "dsp/Filter.h"
#include <algorithm>
#include <cmath>

namespace chimera::dsp
{
namespace
{
constexpr float pi = 3.14159265358979323846f;

enum class BiquadShape
{
    LowPass,
    HighPass,
    BandPass,
    Notch,
    Peak,
    LowShelf,
    HighShelf,
    AllPass
};

bool cascadesMode(FilterMode mode)
{
    return mode == FilterMode::LowPass24
        || mode == FilterMode::HighPass24
        || mode == FilterMode::BandPass24;
}

float modeQ(FilterMode mode, float resonance)
{
    if (mode == FilterMode::LowPassWide || mode == FilterMode::HighPassWide || mode == FilterMode::BandPassWide)
        return 0.5f;

    if (mode == FilterMode::LowPassNarrow || mode == FilterMode::BandPassNarrow)
        return std::max(2.0f, resonance);

    return resonance;
}

BiquadShape modeShape(FilterMode mode)
{
    switch (mode)
    {
        case FilterMode::HighPass6:
        case FilterMode::HighPass12:
        case FilterMode::HighPass24:
        case FilterMode::HighPassWide:
            return BiquadShape::HighPass;
        case FilterMode::BandPass12:
        case FilterMode::BandPass24:
        case FilterMode::BandPassWide:
        case FilterMode::BandPassNarrow:
            return BiquadShape::BandPass;
        case FilterMode::Notch:
            return BiquadShape::Notch;
        case FilterMode::Peak:
            return BiquadShape::Peak;
        case FilterMode::LowShelf:
            return BiquadShape::LowShelf;
        case FilterMode::HighShelf:
            return BiquadShape::HighShelf;
        case FilterMode::Bypass:
            return BiquadShape::AllPass;
        case FilterMode::LowPass6:
        case FilterMode::LowPass12:
        case FilterMode::LowPass24:
        case FilterMode::LowPassWide:
        case FilterMode::LowPassNarrow:
            return BiquadShape::LowPass;
    }

    return BiquadShape::LowPass;
}
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
    const auto effectiveQ = modeQ(mode, resonance);
    const auto alpha = sn / (2.0f * effectiveQ);
    const auto shelfGain = std::sqrt(std::pow(10.0f, 6.0f / 20.0f));

    float b0 = 1.0f;
    float b1n = 0.0f;
    float b2n = 0.0f;
    float a0n = 1.0f;
    float a1n = 0.0f;
    float a2n = 0.0f;

    switch (modeShape(mode))
    {
        case BiquadShape::LowPass:
            b0 = (1.0f - cs) * 0.5f;
            b1n = 1.0f - cs;
            b2n = (1.0f - cs) * 0.5f;
            a0n = 1.0f + alpha;
            a1n = -2.0f * cs;
            a2n = 1.0f - alpha;
            break;
        case BiquadShape::HighPass:
            b0 = (1.0f + cs) * 0.5f;
            b1n = -(1.0f + cs);
            b2n = (1.0f + cs) * 0.5f;
            a0n = 1.0f + alpha;
            a1n = -2.0f * cs;
            a2n = 1.0f - alpha;
            break;
        case BiquadShape::BandPass:
            b0 = alpha;
            b1n = 0.0f;
            b2n = -alpha;
            a0n = 1.0f + alpha;
            a1n = -2.0f * cs;
            a2n = 1.0f - alpha;
            break;
        case BiquadShape::Notch:
            b0 = 1.0f;
            b1n = -2.0f * cs;
            b2n = 1.0f;
            a0n = 1.0f + alpha;
            a1n = -2.0f * cs;
            a2n = 1.0f - alpha;
            break;
        case BiquadShape::Peak:
            b0 = 1.0f + alpha * shelfGain;
            b1n = -2.0f * cs;
            b2n = 1.0f - alpha * shelfGain;
            a0n = 1.0f + alpha / shelfGain;
            a1n = -2.0f * cs;
            a2n = 1.0f - alpha / shelfGain;
            break;
        case BiquadShape::LowShelf:
            b0 = shelfGain * ((shelfGain + 1.0f) - (shelfGain - 1.0f) * cs + 2.0f * std::sqrt(shelfGain) * alpha);
            b1n = 2.0f * shelfGain * ((shelfGain - 1.0f) - (shelfGain + 1.0f) * cs);
            b2n = shelfGain * ((shelfGain + 1.0f) - (shelfGain - 1.0f) * cs - 2.0f * std::sqrt(shelfGain) * alpha);
            a0n = (shelfGain + 1.0f) + (shelfGain - 1.0f) * cs + 2.0f * std::sqrt(shelfGain) * alpha;
            a1n = -2.0f * ((shelfGain - 1.0f) + (shelfGain + 1.0f) * cs);
            a2n = (shelfGain + 1.0f) + (shelfGain - 1.0f) * cs - 2.0f * std::sqrt(shelfGain) * alpha;
            break;
        case BiquadShape::HighShelf:
            b0 = shelfGain * ((shelfGain + 1.0f) + (shelfGain - 1.0f) * cs + 2.0f * std::sqrt(shelfGain) * alpha);
            b1n = -2.0f * shelfGain * ((shelfGain - 1.0f) + (shelfGain + 1.0f) * cs);
            b2n = shelfGain * ((shelfGain + 1.0f) + (shelfGain - 1.0f) * cs - 2.0f * std::sqrt(shelfGain) * alpha);
            a0n = (shelfGain + 1.0f) - (shelfGain - 1.0f) * cs + 2.0f * std::sqrt(shelfGain) * alpha;
            a1n = 2.0f * ((shelfGain - 1.0f) - (shelfGain + 1.0f) * cs);
            a2n = (shelfGain + 1.0f) - (shelfGain - 1.0f) * cs - 2.0f * std::sqrt(shelfGain) * alpha;
            break;
        case BiquadShape::AllPass:
            b0 = 1.0f - alpha;
            b1n = -2.0f * cs;
            b2n = 1.0f + alpha;
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
    if (mode == FilterMode::Bypass)
        return input;

    auto output = processOnePolePair(input);
    if (cascadesMode(mode))
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
