#include "dsp/Envelope.h"
#include "dsp/Element.h"
#include "dsp/Filter.h"
#include "dsp/Lfo.h"
#include "dsp/ModulationMatrix.h"
#include "dsp/SamplePlayer.h"
#include "dsp/SampleZone.h"
#include "engine/Arpeggiator.h"
#include "engine/Performance.h"
#include "engine/SampleLibrary.h"
#include "engine/Sequencer.h"
#include "engine/VoiceAllocator.h"
#include "fx/FxChain.h"
#include "preset/Preset.h"
#include "preset/VoiceBank.h"

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

void testSampleLibrary()
{
    static_assert(chimera::engine::SampleLibrary::factoryWaveformSlots == 3977);
    static_assert(chimera::engine::SampleLibrary::factoryRomBytes == 741ull * 1024ull * 1024ull);
    static_assert(chimera::engine::SampleLibrary::userFlashBoards == 2);
    static_assert(chimera::engine::SampleLibrary::userFlashBoardBytes == 1024ull * 1024ull * 1024ull);

    chimera::engine::SampleLibrary library;
    assert(library.totalUserCapacityBytes() == 2ull * 1024ull * 1024ull * 1024ull);

    assert(library.addFactoryWaveform({ 1, "Init Sine", "Synth", 1024, 60, 0, 127, 1, 127 }));
    assert(!library.addFactoryWaveform({ 1, "Duplicate", "Synth", 1024, 60, 0, 127, 1, 127 }));
    assert(!library.addFactoryWaveform({ 2, "", "Synth", 1024, 60, 0, 127, 1, 127 }));
    assert(library.factoryWaveformCount() == 1);
    assert(library.factoryUsedBytes() == 1024);

    assert(library.addUserWaveform(0, { 1001, "User Brass", "Brass", 2048, 60, 36, 96, 1, 127 }));
    assert(library.addUserWaveform(1, { 2001, "User Pad", "Pad", 4096, 60, 0, 127, 1, 127 }));
    assert(!library.addUserWaveform(2, { 3001, "No Board", "Pad", 4096, 60, 0, 127, 1, 127 }));
    assert(!library.addUserWaveform(0, { 3002, "Too Large", "Pad", chimera::engine::SampleLibrary::userFlashBoardBytes, 60, 0, 127, 1, 127 }));
    assert(library.userWaveformCount(0) == 1);
    assert(library.userWaveformCount(1) == 1);
    assert(library.userUsedBytes(0) == 2048);
    assert(library.findWaveform(2001) != nullptr);
    assert(library.findWaveform(9999) == nullptr);
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
    static_assert(chimera::dsp::filterModeCount == 18);

    const auto modes = {
        chimera::dsp::FilterMode::Bypass,
        chimera::dsp::FilterMode::LowPass6,
        chimera::dsp::FilterMode::LowPass12,
        chimera::dsp::FilterMode::LowPass24,
        chimera::dsp::FilterMode::LowPassWide,
        chimera::dsp::FilterMode::LowPassNarrow,
        chimera::dsp::FilterMode::HighPass6,
        chimera::dsp::FilterMode::HighPass12,
        chimera::dsp::FilterMode::HighPass24,
        chimera::dsp::FilterMode::HighPassWide,
        chimera::dsp::FilterMode::BandPass12,
        chimera::dsp::FilterMode::BandPass24,
        chimera::dsp::FilterMode::BandPassWide,
        chimera::dsp::FilterMode::BandPassNarrow,
        chimera::dsp::FilterMode::Notch,
        chimera::dsp::FilterMode::Peak,
        chimera::dsp::FilterMode::LowShelf,
        chimera::dsp::FilterMode::HighShelf,
    };

    assert(static_cast<int>(modes.size()) == chimera::dsp::filterModeCount);

    for (const auto mode : modes)
    {
        chimera::dsp::Filter filter;
        filter.setSampleRate(48000.0);
        filter.setMode(mode);
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

    performance.setPart(1, { 60, 84, 1, 127, 1, true, 3 });
    const auto matches = performance.matchingInternalParts(64, 100, 1);
    assert(matches.size() == 2);
    assert(matches[0] == 0);
    assert(matches[1] == 3);

    chimera::engine::PerformanceBank bank;
    assert(bank.size() == 512);
    chimera::engine::PerformanceSlot slot;
    slot.name = "Layered Split";
    slot.parts[0] = { 0, 59, 1, 127, 1, true, 0, 1.0f, -0.1f, "Left" };
    slot.parts[1] = { 60, 127, 1, 127, 1, true, 1, 0.8f, 0.1f, "Right" };
    bank.setPerformance(511, slot);
    assert(bank.getPerformance(511).name == "Layered Split");
    assert(bank.getPerformance(999).name == "Init Performance");
}

void testSequencer()
{
    static_assert(chimera::engine::Sequencer::songCount == 64);
    static_assert(chimera::engine::Sequencer::patternCount == 64);
    static_assert(chimera::engine::Song::trackCount == 16);
    static_assert(chimera::engine::Song::ppq == 480);
    static_assert(chimera::engine::Song::maxNotes == 130000);
    static_assert(chimera::engine::Pattern::sectionCount == 16);
    static_assert(chimera::engine::Pattern::maxMeasures == 256);
    static_assert(chimera::engine::Pattern::phraseSlots == 256);

    chimera::engine::Sequencer sequencer;
    auto& song = sequencer.song(0);
    song.setTempo(2.0);
    assert(song.getTempo() == 5.0);
    song.setTempo(400.0);
    assert(song.getTempo() == 300.0);
    song.setTempo(120.0);
    assert(song.getTempo() == 120.0);

    assert(song.addNote(0, { 0, 480, 60, 100, 1 }));
    assert(song.addNote(15, { 480, 240, 64, 90, 2 }));
    assert(!song.addNote(16, { 0, 480, 60, 100, 1 }));
    assert(!song.addNote(0, { -1, 480, 60, 100, 1 }));
    assert(!song.addNote(0, { 0, 0, 60, 100, 1 }));
    assert(song.noteCount() == 2);
    assert(song.track(0).noteCount() == 1);
    assert(song.track(99).noteCount() == 0);
    assert(sequencer.totalNoteCount() == 2);

    auto& pattern = sequencer.pattern(0);
    pattern.setSectionMeasures(0, 512);
    assert(pattern.sectionMeasures(0) == 256);
    pattern.setSectionMeasures(0, 0);
    assert(pattern.sectionMeasures(0) == 1);
}

void testFxProcessors()
{
    static_assert(chimera::fx::effectTypeCount == 16);

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
    bus.setMasterEqDb(1.0f, 0.0f, -1.0f);
    bus.setCompressor(-12.0f, 2.0f, 2.0f, 30.0f, 0.0f);
    assert(bus.process(2.0f) <= 0.98f);

    for (const auto type : {
             chimera::fx::EffectType::Distortion,
             chimera::fx::EffectType::Compressor,
             chimera::fx::EffectType::ThreeBandEq,
             chimera::fx::EffectType::Delay,
             chimera::fx::EffectType::Chorus,
             chimera::fx::EffectType::Phaser,
             chimera::fx::EffectType::Limiter,
             chimera::fx::EffectType::AmpUsCombo,
             chimera::fx::EffectType::AmpJazzCombo,
             chimera::fx::EffectType::AmpUsHighGain,
             chimera::fx::EffectType::AmpBritishLead,
             chimera::fx::EffectType::AmpBritishCombo,
             chimera::fx::EffectType::AmpBritishLegend,
             chimera::fx::EffectType::MultiEffect,
             chimera::fx::EffectType::SmallStereo,
         })
    {
        auto effect = chimera::fx::makeEffect(type);
        assert(effect != nullptr);
        effect->prepare(1000.0);
        auto value = 0.0f;
        for (int i = 0; i < 16; ++i)
            value = effect->process(i == 0 ? 0.25f : 0.0f);
        assert(std::isfinite(value));
    }

    chimera::fx::InsertRack rack;
    rack.prepare(1000.0);
    rack.setSlot(0, chimera::fx::EffectType::AmpBritishLead);
    rack.setSlot(1, chimera::fx::EffectType::Delay);
    assert(rack.getSlot(0) == chimera::fx::EffectType::AmpBritishLead);
    assert(rack.getSlot(99) == chimera::fx::EffectType::None);
    assert(std::isfinite(rack.process(0.2f)));

    chimera::fx::SystemFx systemFx;
    systemFx.prepare(1000.0);
    systemFx.setChorusSend(0.3f);
    systemFx.setReverbSend(0.2f);
    assert(std::isfinite(systemFx.process(0.2f)));

    chimera::fx::WorkstationFx workstationFx;
    workstationFx.prepare(1000.0);
    workstationFx.inserts().setSlot(0, chimera::fx::EffectType::MultiEffect);
    workstationFx.system().setChorusSend(0.2f);
    workstationFx.system().setReverbSend(0.2f);
    workstationFx.master().setMasterEqDb(0.5f, 0.0f, 0.5f);
    assert(std::isfinite(workstationFx.process(0.2f)));
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
      "tuningCents": 5.5,
      "filterType": "highPass12"
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
    assert(patch.elements[0].filterType == "highPass12");
    temp.deleteFile();
}

void testVoiceBank()
{
    static_assert(chimera::preset::VoiceBank::presetNormalSlots == 1024);
    static_assert(chimera::preset::VoiceBank::presetDrumSlots == 64);
    static_assert(chimera::preset::VoiceBank::gmNormalSlots == 128);
    static_assert(chimera::preset::VoiceBank::gmDrumSlots == 1);
    static_assert(chimera::preset::VoiceBank::userBankCount == 4);
    static_assert(chimera::preset::VoiceBank::userNormalSlotsPerBank == 128);
    static_assert(chimera::preset::VoiceBank::userDrumSlots == 32);
    static_assert(chimera::preset::VoiceBank::totalNormalVoiceSlots == 2176);
    static_assert(chimera::preset::VoiceBank::totalDrumKitSlots == 129);

    chimera::preset::VoiceBank bank;
    assert(bank.setVoice({ chimera::preset::VoiceBankId::Preset,
                           chimera::preset::VoiceKind::Normal,
                           0,
                           "Init Piano",
                           "Piano",
                           "presets/Synth/Sine.chpatch" }));
    assert(bank.setVoice({ chimera::preset::VoiceBankId::User1,
                           chimera::preset::VoiceKind::Normal,
                           127,
                           "Layer Stack",
                           "Synth",
                           "presets/Synth/Stack.chpatch" }));
    assert(bank.setVoice({ chimera::preset::VoiceBankId::Preset,
                           chimera::preset::VoiceKind::DrumKit,
                           63,
                           "Init Kit",
                           "Drums",
                           "" }));
    assert(!bank.setVoice({ chimera::preset::VoiceBankId::User1,
                            chimera::preset::VoiceKind::DrumKit,
                            0,
                            "Wrong Kind",
                            "Drums",
                            "" }));
    assert(!bank.setVoice({ chimera::preset::VoiceBankId::Preset,
                            chimera::preset::VoiceKind::Normal,
                            1024,
                            "Out Of Range",
                            "Piano",
                            "" }));
    assert(bank.voiceCount() == 3);

    const auto synths = bank.findByCategory("Synth");
    assert(synths.size() == 1);
    assert(synths[0].name == "Layer Stack");

    auto preset = bank.getVoice(chimera::preset::VoiceBankId::Preset, chimera::preset::VoiceKind::Normal, 0);
    assert(preset.has_value());
    assert(preset->name == "Init Piano");

    assert(bank.setVoice({ chimera::preset::VoiceBankId::Preset,
                           chimera::preset::VoiceKind::Normal,
                           0,
                           "Updated Piano",
                           "Piano",
                           "presets/Synth/Triangle.chpatch" }));
    assert(bank.voiceCount() == 3);
    preset = bank.getVoice(chimera::preset::VoiceBankId::Preset, chimera::preset::VoiceKind::Normal, 0);
    assert(preset.has_value());
    assert(preset->name == "Updated Piano");
}
}

int main()
{
    testEnvelope();
    testLfo();
    testSampleZone();
    testSampleZoneLoadsAudio();
    testSamplePlayer();
    testSampleLibrary();
    testVoiceAllocator();
    testFilterStability();
    testModulationMatrix();
    testElement();
    testArpeggiator();
    testPerformance();
    testSequencer();
    testFxProcessors();
    testPresetLoader();
    testVoiceBank();
    std::cout << "DSP tests passed\n";
    return 0;
}
