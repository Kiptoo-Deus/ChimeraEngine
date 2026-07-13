#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

namespace chimera::fx
{
class Processor
{
public:
    virtual ~Processor() = default;
    virtual void prepare(double sampleRate);
    virtual void reset();
    virtual float process(float input) = 0;
};

class Distortion final : public Processor
{
public:
    void setDrive(float newDrive);
    float process(float input) override;

private:
    float drive = 2.0f;
};

class Compressor final : public Processor
{
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setParameters(float thresholdDb, float ratio, float attackMs, float releaseMs, float makeupDb);
    float process(float input) override;

private:
    float envelope = 0.0f;
    float threshold = 0.25f;
    float ratio = 4.0f;
    float attackCoeff = 0.01f;
    float releaseCoeff = 0.001f;
    float makeup = 1.0f;
    double sampleRate = 44100.0;
};

class ThreeBandEq final : public Processor
{
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setGainsDb(float lowDb, float midDb, float highDb);
    float process(float input) override;

private:
    double sampleRate = 44100.0;
    float lowState = 0.0f;
    float highState = 0.0f;
    std::array<float, 3> gains { 1.0f, 1.0f, 1.0f };
};

class Delay final : public Processor
{
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setParameters(float delayMs, float feedback, float mix);
    float process(float input) override;

private:
    std::vector<float> buffer;
    size_t writeIndex = 0;
    int delaySamples = 1;
    float feedback = 0.25f;
    float mix = 0.25f;
    double sampleRate = 44100.0;
};

class Chorus final : public Processor
{
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setParameters(float rateHz, float depthMs, float mix);
    float process(float input) override;

private:
    std::vector<float> buffer;
    size_t writeIndex = 0;
    float phase = 0.0f;
    float rate = 0.4f;
    float depthMs = 8.0f;
    float mix = 0.35f;
    double sampleRate = 44100.0;
};

class Phaser final : public Processor
{
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setParameters(float rateHz, float depth, float feedback);
    float process(float input) override;

private:
    std::array<float, 4> states {};
    float phase = 0.0f;
    float rate = 0.25f;
    float depth = 0.7f;
    float feedback = 0.2f;
    float last = 0.0f;
    double sampleRate = 44100.0;
};

class Limiter final : public Processor
{
public:
    void setCeiling(float newCeiling);
    float process(float input) override;

private:
    float ceiling = 0.98f;
};

class FxChain
{
public:
    void prepare(double sampleRate);
    void reset();
    void add(std::unique_ptr<Processor> processor);
    float process(float input);
    int size() const;

private:
    std::vector<std::unique_ptr<Processor>> processors;
    double sampleRate = 44100.0;
};

class MasterBus
{
public:
    void prepare(double sampleRate);
    void reset();
    float process(float input);
    FxChain& inserts() { return chain; }

private:
    FxChain chain;
    Limiter limiter;
};
}
