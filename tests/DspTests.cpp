#include "dsp/Envelope.h"
#include "dsp/Element.h"
#include "dsp/Filter.h"
#include "dsp/Lfo.h"
#include "dsp/ModulationMatrix.h"
#include "dsp/SamplePlayer.h"
#include "dsp/SampleZone.h"
#include "engine/Arpeggiator.h"
#include "engine/Performance.h"
#include "engine/VoiceAllocator.h"
#include "fx/FxChain.h"
#include "preset/Preset.h"

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

void testSampleZoneLoadsAudio()
{
    chimera::dsp::SampleZone zone;
    zone.setSource(juce::File::getCurrentWorkingDirectory()
                       .getParentDirectory()
                       .getChildFile("samples/Synth/sine_C4_24bit.wav"));
    const auto result = zone.loadAudio();
    assert(result.wasOk());
    assert(zone.getNumChannels() == 1);
    assert(zone.getNumSamples() > 1000);
    assert(zone.getSourceSampleRate() > 0.0);
    assert(std::isfinite(zone.sample(0, 100)));
}

std::shared_ptr<chimera::dsp::SampleZone> loadTestZone()
{
    auto zone = std::make_shared<chimera::dsp::SampleZone>();
    zone->setSource(juce::File::getCurrentWorkingDirectory()
                        .getParentDirectory()
                        .getChildFile("samples/Synth/sine_C4_24bit.wav"));
    const auto result = zone->loadAudio();
    assert(result.wasOk());
    return zone;
}

void testSamplePlayer()
{
    chimera::dsp::SamplePlayer player;
    player.setZone(loadTestZone());
    player.start(60, 48000.0);
    assert(player.isPlaying());
    auto sum = 0.0f;
    for (int i = 0; i < 256; ++i)
        sum += std::abs(player.process());
    assert(sum > 0.01f);

    const auto firstPosition = player.getPosition();
    player.start(72, 48000.0);
    for (int i = 0; i < 16; ++i)
        player.process();
    assert(player.getPosition() > firstPosition * 0.05);
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

void testFxProcessors()
{
    chimera::fx::Distortion distortion;
    distortion.setDrive(4.0f);
    assert(distortion.process(0.5f) < 1.0f);
    assert(distortion.process(0.5f) > 0.5f);

    chimera::fx::Compressor compressor;
    compressor.prepare(1000.0);
    compressor.setParameters(-20.0f, 4.0f, 0.1f, 10.0f, 0.0f);
    auto compressed = 0.0f;
    for (int i = 0; i < 32; ++i)
        compressed = compressor.process(1.0f);
    assert(compressed < 1.0f);

    chimera::fx::Delay delay;
    delay.prepare(1000.0);
    delay.setParameters(1.0f, 0.0f, 1.0f);
    assert(std::abs(delay.process(0.75f)) < 0.001f);
    assert(delay.process(0.0f) > 0.7f);

    chimera::fx::FxChain chain;
    chain.prepare(1000.0);
    chain.add(std::make_unique<chimera::fx::Distortion>());
    assert(chain.size() == 1);
    assert(std::isfinite(chain.process(0.2f)));

    chimera::fx::MasterBus bus;
    bus.prepare(1000.0);
    assert(bus.process(2.0f) <= 0.98f);
}

void testPresetLoader()
{
    const auto temp = juce::File::createTempFile("chimera-test.chpatch");
    temp.replaceWithText(R"json({
  "format": "chpatch",
  "version": 1,
  "name": "Unit Test Patch",
  "category": "Synth",
  "elements": [
    {
      "sample": "samples/Synth/sine_C4_24bit.wav",
      "rootKey": 60,
      "keyRange": [0, 127],
      "velocityRange": [1, 127],
      "tuningCents": 5.5
    }
  ]
})json");

    chimera::preset::Patch patch;
    const auto result = chimera::preset::loadPatch(temp, patch);
    assert(result.wasOk());
    assert(patch.metadata.name == "Unit Test Patch");
    assert(patch.elements.size() == 1);
    assert(patch.elements[0].rootKey == 60);
    assert(std::abs(patch.elements[0].tuningCents - 5.5f) < 0.001f);
    temp.deleteFile();
}
}

int main()
{
    testEnvelope();
    testLfo();
    testSampleZone();
    testSampleZoneLoadsAudio();
    testSamplePlayer();
    testVoiceAllocator();
    testFilterStability();
    testModulationMatrix();
    testElement();
    testArpeggiator();
    testPerformance();
    testFxProcessors();
    testPresetLoader();
    std::cout << "DSP tests passed\n";
    return 0;
}
