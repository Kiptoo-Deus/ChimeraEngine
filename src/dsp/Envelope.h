#pragma once

namespace chimera::dsp
{
enum class Curve
{
    Linear,
    Exponential
};

class Envelope
{
public:
    void setSampleRate(double newSampleRate);
    void setStages(float attackSeconds, float decay1Seconds, float decay2Seconds,
                   float sustainLevel, float releaseSeconds, Curve newCurve);
    void noteOn();
    void noteOff();
    float process();
    bool isActive() const;

private:
    enum class Stage { Idle, Attack, Decay1, Decay2, Sustain, Release };

    float advanceToward(float target, float seconds);

    double sampleRate = 44100.0;
    float attack = 0.01f;
    float decay1 = 0.1f;
    float decay2 = 0.1f;
    float sustain = 0.7f;
    float release = 0.2f;
    float value = 0.0f;
    Curve curve = Curve::Exponential;
    Stage stage = Stage::Idle;
};
}
