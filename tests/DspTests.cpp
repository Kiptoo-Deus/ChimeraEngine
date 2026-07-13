#include "dsp/Envelope.h"
#include "dsp/Element.h"
#include "dsp/Filter.h"
#include "dsp/Lfo.h"
#include "dsp/ModulationMatrix.h"
#include "dsp/SampleZone.h"
#include "engine/Arpeggiator.h"
#include "engine/Performance.h"
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

void testModulationMatrix()
{
    chimera::dsp::ModulationMatrix matrix;
    matrix.setFreeSlot(0, { chimera::dsp::ModSource::Aftertouch, chimera::dsp::ModDestination::Cutoff, 0.5f, true });
    const auto mods = matrix.evaluate(0.5f, 0.0f, 0.25f, 1.0f, 0.0f, 0.0f, 0.5f);
    assert(mods[1] > 0.6f);
    assert(mods[2] > 0.49f && mods[2] < 0.51f);
}

void testElement()
{
    auto zone = std::make_shared<chimera::dsp::SampleZone>();
    zone->setKeyRange(60, 72);
    chimera::dsp::Element element;
    element.setSampleRate(48000.0);
    element.setZone(zone);
    assert(element.canPlay(64, 100));
    assert(!element.canPlay(50, 100));
    element.noteOn(64, 100);
    const auto out = element.process(0.25f);
    assert(std::isfinite(out));
}

void testArpeggiator()
{
    chimera::engine::Arpeggiator arp;
    arp.setMode(chimera::engine::ArpMode::UpDown);
    arp.setHeldNotes({ 67, 60, 64 });
    assert(arp.tick()[0] == 60);
    assert(arp.tick()[0] == 64);
    assert(arp.tick()[0] == 67);
    assert(arp.tick()[0] == 64);
}

void testPerformance()
{
    chimera::engine::Performance performance;
    performance.setPart(0, { 48, 72, 1, 127, 1, true });
    assert(performance.partMatches(0, 60, 64, 1));
    assert(!performance.partMatches(0, 80, 64, 1));
    assert(!performance.partMatches(0, 60, 64, 2));
}
}

int main()
{
    testEnvelope();
    testLfo();
    testSampleZone();
    testVoiceAllocator();
    testFilterStability();
    testModulationMatrix();
    testElement();
    testArpeggiator();
    testPerformance();
    std::cout << "DSP tests passed\n";
    return 0;
}
