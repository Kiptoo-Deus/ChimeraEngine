#pragma once

namespace chimera::dsp
{
enum class FilterMode
{
    Bypass,
    LowPass6,
    LowPass12,
    LowPass24,
    LowPassWide,
    LowPassNarrow,
    HighPass6,
    HighPass12,
    HighPass24,
    HighPassWide,
    BandPass12,
    BandPass24,
    BandPassWide,
    BandPassNarrow,
    Notch,
    Peak,
    LowShelf,
    HighShelf
};

constexpr int filterModeCount = 18;

class Filter
{
public:
    void setSampleRate(double newSampleRate);
    void setMode(FilterMode newMode);
    void setCutoff(float hz);
    void setResonance(float q);
    void reset();
    float process(float input);

private:
    void update();
    float processOnePolePair(float input);

    double sampleRate = 44100.0;
    float cutoff = 1000.0f;
    float resonance = 0.707f;
    float a0 = 1.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float z1 = 0.0f;
    float z2 = 0.0f;
    float z3 = 0.0f;
    float z4 = 0.0f;
    FilterMode mode = FilterMode::LowPass12;
};
}
