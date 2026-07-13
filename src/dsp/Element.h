#pragma once

#include "dsp/Envelope.h"
#include "dsp/Filter.h"
#include "dsp/Lfo.h"
#include "dsp/ModulationMatrix.h"
#include "dsp/SampleZone.h"

#include <memory>

namespace chimera::dsp
{
class Element
{
public:
    void setZone(std::shared_ptr<SampleZone> newZone);
    void setSampleRate(double sampleRate);
    void noteOn(int midiNote, int velocity);
    void noteOff();
    float process(float sampleInput, float aftertouch = 0.0f);
    bool canPlay(int midiNote, int velocity) const;
    bool isActive() const;
    ModulationMatrix& modulation() { return matrix; }

private:
    std::shared_ptr<SampleZone> zone;
    Envelope pitchEnvelope;
    Envelope filterEnvelope;
    Envelope ampEnvelope;
    Filter filter;
    Lfo lfo1;
    Lfo lfo2;
    ModulationMatrix matrix;
    float noteVelocity = 0.0f;
};
}
