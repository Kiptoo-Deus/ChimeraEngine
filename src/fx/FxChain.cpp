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

void configureAmpModel(EffectType type, Distortion& distortion)
{
    if (type == EffectType::AmpJazzCombo)
        distortion.setDrive(1.6f);
    else if (type == EffectType::AmpUsCombo)
        distortion.setDrive(2.2f);
    else if (type == EffectType::AmpBritishCombo)
        distortion.setDrive(2.8f);
    else if (type == EffectType::AmpBritishLead)
        distortion.setDrive(4.0f);
    else if (type == EffectType::AmpUsHighGain)
        distortion.setDrive(5.0f);
    else if (type == EffectType::AmpBritishLegend)
        distortion.setDrive(6.0f);
    else
        distortion.setDrive(2.0f);
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

void Reverb::prepare(double newSampleRate)
{
    sampleRate = std::max(1.0, newSampleRate);
    const auto base = static_cast<size_t>(sampleRate * 0.029);
    for (size_t index = 0; index < buffers.size(); ++index)
        buffers[index].assign(base + index * static_cast<size_t>(sampleRate * 0.011), 0.0f);
    reset();
}

void Reverb::reset()
{
    for (auto& buffer : buffers)
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    indices = {};
    damped = 0.0f;
}

void Reverb::setParameters(float roomSize, float newDamping, float newMix)
{
    feedback = std::clamp(roomSize, 0.1f, 0.95f);
    damping = std::clamp(newDamping, 0.0f, 0.95f);
    mix = std::clamp(newMix, 0.0f, 1.0f);
}

float Reverb::process(float input)
{
    auto wet = 0.0f;
    for (size_t bufferIndex = 0; bufferIndex < buffers.size(); ++bufferIndex)
    {
        auto& buffer = buffers[bufferIndex];
        if (buffer.empty())
            continue;

        const auto delayed = buffer[indices[bufferIndex]];
        damped = delayed * (1.0f - damping) + damped * damping;
        buffer[indices[bufferIndex]] = input + damped * feedback;
        indices[bufferIndex] = (indices[bufferIndex] + 1) % buffer.size();
        wet += delayed;
    }

    wet /= static_cast<float>(buffers.size());
    return input * (1.0f - mix) + wet * mix;
}

std::unique_ptr<Processor> makeEffect(EffectType type)
{
    switch (type)
    {
        case EffectType::Distortion:
        {
            auto processor = std::make_unique<Distortion>();
            processor->setDrive(3.0f);
            return processor;
        }
        case EffectType::Compressor:
        {
            auto processor = std::make_unique<Compressor>();
            processor->setParameters(-18.0f, 3.5f, 5.0f, 80.0f, 1.0f);
            return processor;
        }
        case EffectType::ThreeBandEq:
        {
            auto processor = std::make_unique<ThreeBandEq>();
            processor->setGainsDb(1.5f, 0.0f, 1.0f);
            return processor;
        }
        case EffectType::Delay:
        {
            auto processor = std::make_unique<Delay>();
            processor->setParameters(240.0f, 0.28f, 0.3f);
            return processor;
        }
        case EffectType::Chorus:
        {
            auto processor = std::make_unique<Chorus>();
            processor->setParameters(0.35f, 9.0f, 0.35f);
            return processor;
        }
        case EffectType::Phaser:
        {
            auto processor = std::make_unique<Phaser>();
            processor->setParameters(0.28f, 0.75f, 0.25f);
            return processor;
        }
        case EffectType::Limiter:
            return std::make_unique<Limiter>();
        case EffectType::AmpUsCombo:
        case EffectType::AmpJazzCombo:
        case EffectType::AmpUsHighGain:
        case EffectType::AmpBritishLead:
        case EffectType::AmpBritishCombo:
        case EffectType::AmpBritishLegend:
        {
            auto processor = std::make_unique<Distortion>();
            configureAmpModel(type, *processor);
            return processor;
        }
        case EffectType::MultiEffect:
        {
            auto processor = std::make_unique<FxChain>();
            processor->add(makeEffect(EffectType::Compressor));
            processor->add(makeEffect(EffectType::Chorus));
            processor->add(makeEffect(EffectType::Delay));
            return processor;
        }
        case EffectType::SmallStereo:
        {
            auto processor = std::make_unique<Chorus>();
            processor->setParameters(0.18f, 4.0f, 0.22f);
            return processor;
        }
        case EffectType::None:
            break;
    }

    return nullptr;
}

void InsertRack::prepare(double newSampleRate)
{
    sampleRate = std::max(1.0, newSampleRate);
    for (auto& slot : slots)
        if (slot != nullptr)
            slot->prepare(sampleRate);
}

void InsertRack::reset()
{
    for (auto& slot : slots)
        if (slot != nullptr)
            slot->reset();
}

void InsertRack::setSlot(int index, EffectType type)
{
    if (index < 0 || index >= slotCount)
        return;

    types[static_cast<size_t>(index)] = type;
    slots[static_cast<size_t>(index)] = makeEffect(type);
    if (slots[static_cast<size_t>(index)] != nullptr)
        slots[static_cast<size_t>(index)]->prepare(sampleRate);
}

EffectType InsertRack::getSlot(int index) const
{
    if (index < 0 || index >= slotCount)
        return EffectType::None;

    return types[static_cast<size_t>(index)];
}

float InsertRack::process(float input)
{
    auto value = input;
    for (auto& slot : slots)
        if (slot != nullptr)
            value = slot->process(value);

    return value;
}

void SystemFx::prepare(double sampleRate)
{
    chorus.prepare(sampleRate);
    reverb.prepare(sampleRate);
}

void SystemFx::reset()
{
    chorus.reset();
    reverb.reset();
}

void SystemFx::setChorusSend(float send)
{
    chorusSend = std::clamp(send, 0.0f, 1.0f);
}

void SystemFx::setReverbSend(float send)
{
    reverbSend = std::clamp(send, 0.0f, 1.0f);
}

float SystemFx::process(float input)
{
    const auto chorused = chorus.process(input) * chorusSend;
    const auto reverbed = reverb.process(input + chorused) * reverbSend;
    return input + chorused + reverbed;
}

void MasterBus::prepare(double sampleRate)
{
    chain.prepare(sampleRate);
    eq.prepare(sampleRate);
    compressor.prepare(sampleRate);
}

void MasterBus::reset()
{
    chain.reset();
    eq.reset();
    compressor.reset();
}

float MasterBus::process(float input)
{
    return limiter.process(compressor.process(eq.process(chain.process(input))));
}

void MasterBus::setMasterEqDb(float lowDb, float midDb, float highDb)
{
    eq.setGainsDb(lowDb, midDb, highDb);
}

void MasterBus::setCompressor(float thresholdDb, float ratio, float attackMs, float releaseMs, float makeupDb)
{
    compressor.setParameters(thresholdDb, ratio, attackMs, releaseMs, makeupDb);
}

void WorkstationFx::prepare(double sampleRate)
{
    insertRack.prepare(sampleRate);
    systemFx.prepare(sampleRate);
    masterBus.prepare(sampleRate);
}

void WorkstationFx::reset()
{
    insertRack.reset();
    systemFx.reset();
    masterBus.reset();
}

float WorkstationFx::process(float input)
{
    return masterBus.process(systemFx.process(insertRack.process(input)));
}
}
