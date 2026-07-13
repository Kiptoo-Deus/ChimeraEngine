#include "dsp/Element.h"
#include <algorithm>

namespace chimera::dsp
{
void Element::setZone(std::shared_ptr<SampleZone> newZone)
{
    zone = std::move(newZone);
}

void Element::setSampleRate(double sampleRate)
{
    pitchEnvelope.setSampleRate(sampleRate);
    filterEnvelope.setSampleRate(sampleRate);
    ampEnvelope.setSampleRate(sampleRate);
    filter.setSampleRate(sampleRate);
    lfo1.setSampleRate(sampleRate);
    lfo2.setSampleRate(sampleRate);
}

void Element::noteOn(int, int velocity)
{
    noteVelocity = std::clamp(static_cast<float>(velocity) / 127.0f, 0.0f, 1.0f);
    pitchEnvelope.noteOn();
    filterEnvelope.noteOn();
    ampEnvelope.noteOn();
}

void Element::noteOff()
{
    pitchEnvelope.noteOff();
    filterEnvelope.noteOff();
    ampEnvelope.noteOff();
}

bool Element::canPlay(int midiNote, int velocity) const
{
    return zone != nullptr && zone->matches(midiNote, velocity);
}

bool Element::isActive() const
{
    return ampEnvelope.isActive();
}

float Element::process(float sampleInput, float aftertouch)
{
    const auto pitchEg = pitchEnvelope.process();
    const auto filterEg = filterEnvelope.process();
    const auto ampEg = ampEnvelope.process();
    const auto mods = matrix.evaluate(noteVelocity, pitchEg, filterEg, ampEg,
                                      lfo1.process(), lfo2.process(), aftertouch);

    filter.setCutoff(200.0f + std::clamp(mods[1], 0.0f, 1.0f) * 18000.0f);
    return filter.process(sampleInput) * std::clamp(mods[2], 0.0f, 1.0f);
}
}
