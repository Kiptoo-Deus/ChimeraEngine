#include "dsp/Envelope.h"
#include "dsp/Filter.h"
#include "dsp/Lfo.h"
#include "dsp/SampleZone.h"
#include "engine/VoiceAllocator.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
void testEnvelope()
{
    chimera::dsp::Envelope env;
    env.setSampleRate(1000.0);
    env.setStages(0.01f, 0.01f, 0.01f, 0.5f, 0.01f, chimera::dsp::Curve::Linear);
    env.noteOn();
    float last = 0.0f;
    for (int i = 0; i < 10; ++i)
    {
        const auto value = env.process();
        assert(value >= last);
        last = value;
    }
    assert(last > 0.9f);
    env.noteOff();
    for (int i = 0; i < 20; ++i)
        env.process();
    assert(!env.isActive());
}

void testLfo()
{
    chimera::dsp::Lfo lfo;
    lfo.setSampleRate(4.0);
    lfo.setFrequency(1.0f);
    lfo.setShape(chimera::dsp::LfoShape::Sine);
    lfo.reset();
    assert(std::abs(lfo.process()) < 0.001f);
    assert(lfo.process() > 0.9f);
}

void testSampleZone()
{
    chimera::dsp::SampleZone zone;
    zone.setKeyRange(48, 72);
    zone.setVelocityRange(10, 100);
    assert(zone.matches(60, 64));
    assert(!zone.matches(40, 64));
    assert(!zone.matches(60, 120));
}

void testVoiceAllocator()
{
    chimera::engine::VoiceAllocator allocator(2);
    const auto first = allocator.noteOn(60);
    const auto second = allocator.noteOn(64);
    const auto stolen = allocator.noteOn(67);
    assert(first == 0);
    assert(second == 1);
    assert(stolen == 0);
    allocator.noteOff(64);
    assert(!allocator.getVoices()[1].active);
}

void testFilterStability()
{
    chimera::dsp::Filter filter;
    filter.setSampleRate(48000.0);
    filter.setCutoff(1000.0f);
    filter.setResonance(0.707f);
    auto value = 0.0f;
    for (int i = 0; i < 1024; ++i)
    {
        value = filter.process(i == 0 ? 1.0f : 0.0f);
        assert(std::isfinite(value));
    }
}
}

int main()
{
    testEnvelope();
    testLfo();
    testSampleZone();
    testVoiceAllocator();
    testFilterStability();
    std::cout << "DSP tests passed\n";
    return 0;
}
