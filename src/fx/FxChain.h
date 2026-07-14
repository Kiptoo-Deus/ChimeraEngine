#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

namespace chimera::fx
{
enum class EffectType
{
    None,
    Distortion,
    Compressor,
    ThreeBandEq,
    Delay,
    Chorus,
    Phaser,
    Limiter,
    AmpUsCombo,
    AmpJazzCombo,
    AmpUsHighGain,
    AmpBritishLead,
    AmpBritishCombo,
    AmpBritishLegend,
    MultiEffect,
    SmallStereo
};

constexpr int effectTypeCount = 16;

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

class FxChain final : public Processor
{
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void add(std::unique_ptr<Processor> processor);
    float process(float input) override;
    int size() const;

private:
    std::vector<std::unique_ptr<Processor>> processors;
    double sampleRate = 44100.0;
};

class Reverb final : public Processor
{
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setParameters(float roomSize, float damping, float mix);
    float process(float input) override;

private:
    std::array<std::vector<float>, 4> buffers;
    std::array<size_t, 4> indices {};
    float feedback = 0.65f;
    float damping = 0.25f;
    float mix = 0.25f;
    float damped = 0.0f;
    double sampleRate = 44100.0;
};

std::unique_ptr<Processor> makeEffect(EffectType type);

class InsertRack
{
public:
    static constexpr int slotCount = 8;

    void prepare(double sampleRate);
    void reset();
    void setSlot(int index, EffectType type);
    EffectType getSlot(int index) const;
    float process(float input);

private:
    std::array<EffectType, slotCount> types {};
    std::array<std::unique_ptr<Processor>, slotCount> slots {};
    double sampleRate = 44100.0;
};

class SystemFx
{
public:
    void prepare(double sampleRate);
    void reset();
    void setChorusSend(float send);
    void setReverbSend(float send);
    float process(float input);

private:
    Chorus chorus;
    Reverb reverb;
    float chorusSend = 0.25f;
    float reverbSend = 0.2f;
};

class MasterBus
{
public:
    void prepare(double sampleRate);
    void reset();
    float process(float input);
    FxChain& inserts() { return chain; }
    void setMasterEqDb(float lowDb, float midDb, float highDb);
    void setCompressor(float thresholdDb, float ratio, float attackMs, float releaseMs, float makeupDb);

private:
    FxChain chain;
    ThreeBandEq eq;
    Compressor compressor;
    Limiter limiter;
};

class WorkstationFx
{
public:
    void prepare(double sampleRate);
    void reset();
    InsertRack& inserts() { return insertRack; }
    SystemFx& system() { return systemFx; }
    MasterBus& master() { return masterBus; }
    float process(float input);

private:
    InsertRack insertRack;
    SystemFx systemFx;
    MasterBus masterBus;
};
}
