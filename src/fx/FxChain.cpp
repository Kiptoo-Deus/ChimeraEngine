#include "fx/FxChain.h"
#include <algorithm>
#include <cmath>

namespace chimera::fx
{
namespace
{
constexpr float pi = 3.14159265358979323846f;

float dbToGain(float db)
{
    return std::pow(10.0f, db / 20.0f);
}
}

void Processor::prepare(double)
{
}

void Processor::reset()
{
}

void Distortion::setDrive(float newDrive)
{
    drive = std::max(1.0f, newDrive);
}

float Distortion::process(float input)
{
    return std::tanh(input * drive);
}

void Compressor::prepare(double newSampleRate)
{
    sampleRate = std::max(1.0, newSampleRate);
    setParameters(20.0f * std::log10(std::max(threshold, 0.0001f)), ratio, 10.0f, 80.0f, 0.0f);
}

void Compressor::reset()
{
    envelope = 0.0f;
}

void Compressor::setParameters(float thresholdDb, float newRatio, float attackMs, float releaseMs, float makeupDb)
{
    threshold = dbToGain(thresholdDb);
    ratio = std::max(1.0f, newRatio);
    attackCoeff = std::exp(-1.0f / static_cast<float>(sampleRate * std::max(0.1f, attackMs) * 0.001f));
    releaseCoeff = std::exp(-1.0f / static_cast<float>(sampleRate * std::max(1.0f, releaseMs) * 0.001f));
    makeup = dbToGain(makeupDb);
}

float Compressor::process(float input)
{
    const auto level = std::abs(input);
    const auto coeff = level > envelope ? attackCoeff : releaseCoeff;
    envelope = coeff * envelope + (1.0f - coeff) * level;

    auto gain = 1.0f;
    if (envelope > threshold)
    {
        const auto over = envelope / threshold;
        const auto compressed = std::pow(over, (1.0f / ratio) - 1.0f);
        gain = compressed;
    }

    return input * gain * makeup;
}

void ThreeBandEq::prepare(double newSampleRate)
{
    sampleRate = std::max(1.0, newSampleRate);
    reset();
}

void ThreeBandEq::reset()
{
    lowState = 0.0f;
    highState = 0.0f;
}

void ThreeBandEq::setGainsDb(float lowDb, float midDb, float highDb)
{
    gains = { dbToGain(lowDb), dbToGain(midDb), dbToGain(highDb) };
}

float ThreeBandEq::process(float input)
{
    const auto lowCoeff = 1.0f - std::exp(-2.0f * pi * 250.0f / static_cast<float>(sampleRate));
    const auto highCoeff = 1.0f - std::exp(-2.0f * pi * 4000.0f / static_cast<float>(sampleRate));
    lowState += lowCoeff * (input - lowState);
    highState += highCoeff * (input - highState);
    const auto low = lowState;
    const auto high = input - highState;
    const auto mid = input - low - high;
    return low * gains[0] + mid * gains[1] + high * gains[2];
}

void Delay::prepare(double newSampleRate)
{
    sampleRate = std::max(1.0, newSampleRate);
    buffer.assign(static_cast<size_t>(sampleRate * 2.0), 0.0f);
    setParameters(static_cast<float>(delaySamples) * 1000.0f / static_cast<float>(sampleRate), feedback, mix);
}

void Delay::reset()
{
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
}

void Delay::setParameters(float delayMs, float newFeedback, float newMix)
{
    delaySamples = std::max(1, static_cast<int>(sampleRate * delayMs * 0.001));
    feedback = std::clamp(newFeedback, 0.0f, 0.95f);
    mix = std::clamp(newMix, 0.0f, 1.0f);
}

float Delay::process(float input)
{
    if (buffer.empty())
        return input;

    const auto readIndex = (writeIndex + buffer.size() - static_cast<size_t>(delaySamples)) % buffer.size();
    const auto delayed = buffer[readIndex];
    buffer[writeIndex] = input + delayed * feedback;
    writeIndex = (writeIndex + 1) % buffer.size();
    return input * (1.0f - mix) + delayed * mix;
}

void Chorus::prepare(double newSampleRate)
{
    sampleRate = std::max(1.0, newSampleRate);
    buffer.assign(static_cast<size_t>(sampleRate), 0.0f);
}

void Chorus::reset()
{
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
    phase = 0.0f;
}

void Chorus::setParameters(float rateHz, float newDepthMs, float newMix)
{
    rate = std::max(0.01f, rateHz);
    depthMs = std::clamp(newDepthMs, 0.1f, 25.0f);
    mix = std::clamp(newMix, 0.0f, 1.0f);
}

float Chorus::process(float input)
{
    if (buffer.empty())
        return input;

    const auto lfo = 0.5f + 0.5f * std::sin(phase * 2.0f * pi);
    const auto delaySamples = static_cast<size_t>(1 + sampleRate * depthMs * 0.001f * lfo);
    const auto readIndex = (writeIndex + buffer.size() - delaySamples) % buffer.size();
    const auto delayed = buffer[readIndex];
    buffer[writeIndex] = input;
    writeIndex = (writeIndex + 1) % buffer.size();
    phase += rate / static_cast<float>(sampleRate);
    phase -= std::floor(phase);
    return input * (1.0f - mix) + delayed * mix;
}

void Phaser::prepare(double newSampleRate)
{
    sampleRate = std::max(1.0, newSampleRate);
    reset();
}

void Phaser::reset()
{
    states = {};
    phase = 0.0f;
    last = 0.0f;
}

void Phaser::setParameters(float rateHz, float newDepth, float newFeedback)
{
    rate = std::max(0.01f, rateHz);
    depth = std::clamp(newDepth, 0.0f, 1.0f);
    feedback = std::clamp(newFeedback, 0.0f, 0.95f);
}

float Phaser::process(float input)
{
    auto x = input + last * feedback;
    const auto sweep = 0.1f + depth * (0.8f * (0.5f + 0.5f * std::sin(phase * 2.0f * pi)));
    for (auto& state : states)
    {
        const auto y = -sweep * x + state;
        state = x + sweep * y;
        x = y;
    }
    phase += rate / static_cast<float>(sampleRate);
    phase -= std::floor(phase);
    last = x;
    return 0.5f * (input + x);
}

void Limiter::setCeiling(float newCeiling)
{
    ceiling = std::clamp(newCeiling, 0.01f, 1.0f);
}

float Limiter::process(float input)
{
    return std::clamp(input, -ceiling, ceiling);
}

void FxChain::prepare(double newSampleRate)
{
    sampleRate = std::max(1.0, newSampleRate);
    for (auto& processor : processors)
        processor->prepare(sampleRate);
}

void FxChain::reset()
{
    for (auto& processor : processors)
        processor->reset();
}

void FxChain::add(std::unique_ptr<Processor> processor)
{
    processor->prepare(sampleRate);
    processors.push_back(std::move(processor));
}

float FxChain::process(float input)
{
    auto value = input;
    for (auto& processor : processors)
        value = processor->process(value);
    return value;
}

int FxChain::size() const
{
    return static_cast<int>(processors.size());
}

void MasterBus::prepare(double sampleRate)
{
    chain.prepare(sampleRate);
}

void MasterBus::reset()
{
    chain.reset();
}

float MasterBus::process(float input)
{
    return limiter.process(chain.process(input));
}
}
