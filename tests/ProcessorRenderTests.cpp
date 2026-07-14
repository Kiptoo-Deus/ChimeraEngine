#include "PluginProcessor.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
std::pair<float, float> renderAndChannelSums(ChimeraEngineAudioProcessor& processor, juce::MidiBuffer& midi, int samples)
{
    juce::AudioBuffer<float> buffer(2, samples);
    processor.processBlock(buffer, midi);

    auto left = 0.0f;
    auto right = 0.0f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        left += std::abs(buffer.getSample(0, sample));
        right += std::abs(buffer.getSample(1, sample));
    }

    return { left, right };
}

float renderAndSum(ChimeraEngineAudioProcessor& processor, juce::MidiBuffer& midi, int samples)
{
    const auto [left, right] = renderAndChannelSums(processor, midi, samples);
    return left + right;
}

void setFloatParameter(ChimeraEngineAudioProcessor& processor, const juce::String& parameterId, float value)
{
    auto* parameter = dynamic_cast<juce::AudioParameterFloat*>(processor.getParameters().getParameter(parameterId));
    assert(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float getFloatParameter(ChimeraEngineAudioProcessor& processor, const juce::String& parameterId)
{
    auto* parameter = dynamic_cast<juce::AudioParameterFloat*>(processor.getParameters().getParameter(parameterId));
    assert(parameter != nullptr);
    return parameter->get();
}
}

int main()
{
    static_assert(ChimeraEngineAudioProcessor::getPartCount() == 16);
    static_assert(ChimeraEngineAudioProcessor::getMaxVoiceCount() == 128);

    ChimeraEngineAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);

    const auto sum = renderAndSum(processor, midi, 512);
    assert(sum > 0.01f);
    assert(std::isfinite(sum));

    ChimeraEngineAudioProcessor wetFxProcessor;
    wetFxProcessor.prepareToPlay(48000.0, 512);
    wetFxProcessor.getParameters().getParameter("fxMix")->setValueNotifyingHost(1.0f);
    juce::MidiBuffer wetFxMidi;
    wetFxMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    const auto wetFxSum = renderAndSum(wetFxProcessor, wetFxMidi, 512);
    assert(wetFxSum > 0.01f);
    assert(std::isfinite(wetFxSum));

    ChimeraEngineAudioProcessor configuredFxProcessor;
    configuredFxProcessor.prepareToPlay(48000.0, 512);
    configuredFxProcessor.setInsertEffect(0, chimera::fx::EffectType::Phaser);
    configuredFxProcessor.setInsertEffect(1, chimera::fx::EffectType::Delay);
    configuredFxProcessor.setSystemFxSends(0.4f, 0.35f);
    configuredFxProcessor.getParameters().getParameter("fxMix")->setValueNotifyingHost(1.0f);
    assert(configuredFxProcessor.getInsertEffect(0) == chimera::fx::EffectType::Phaser);
    assert(configuredFxProcessor.getInsertEffect(1) == chimera::fx::EffectType::Delay);
    assert(std::abs(configuredFxProcessor.getChorusSend() - 0.4f) < 0.001f);
    assert(std::abs(configuredFxProcessor.getReverbSend() - 0.35f) < 0.001f);
    juce::MidiBuffer configuredFxMidi;
    configuredFxMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    const auto configuredFxSum = renderAndSum(configuredFxProcessor, configuredFxMidi, 512);
    assert(configuredFxSum > 0.01f);
    assert(std::isfinite(configuredFxSum));

    ChimeraEngineAudioProcessor masterFxProcessor;
    masterFxProcessor.prepareToPlay(48000.0, 512);
    masterFxProcessor.getParameters().getParameter("fxMix")->setValueNotifyingHost(1.0f);
    juce::MidiBuffer masterFxReferenceMidi;
    masterFxReferenceMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    const auto masterFxReference = renderAndSum(masterFxProcessor, masterFxReferenceMidi, 512);

    ChimeraEngineAudioProcessor shapedMasterFxProcessor;
    shapedMasterFxProcessor.prepareToPlay(48000.0, 512);
    shapedMasterFxProcessor.getParameters().getParameter("fxMix")->setValueNotifyingHost(1.0f);
    setFloatParameter(shapedMasterFxProcessor, "masterEqLow", -9.0f);
    setFloatParameter(shapedMasterFxProcessor, "masterEqMid", 6.0f);
    setFloatParameter(shapedMasterFxProcessor, "masterEqHigh", 9.0f);
    setFloatParameter(shapedMasterFxProcessor, "masterCompThreshold", -30.0f);
    setFloatParameter(shapedMasterFxProcessor, "masterCompRatio", 8.0f);
    setFloatParameter(shapedMasterFxProcessor, "masterCompMakeup", 6.0f);
    juce::MidiBuffer masterFxMidi;
    masterFxMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    const auto masterFxSum = renderAndSum(shapedMasterFxProcessor, masterFxMidi, 512);
    assert(masterFxSum > 0.01f);
    assert(std::isfinite(masterFxSum));
    assert(std::abs(masterFxReference - masterFxSum) > 0.001f);

    ChimeraEngineAudioProcessor sequencerProcessor;
    sequencerProcessor.prepareToPlay(48000.0, 512);
    sequencerProcessor.setSequencerPlaybackEnabled(true);
    auto sequencerSum = 0.0f;
    for (int block = 0; block < 12; ++block)
    {
        juce::MidiBuffer sequencerMidi;
        sequencerSum += renderAndSum(sequencerProcessor, sequencerMidi, 512);
    }
    assert(sequencerSum > 0.01f);
    assert(sequencerProcessor.getSequencerTick() > 0);
    auto sceneChanged = false;
    for (int block = 0; block < 300; ++block)
    {
        juce::MidiBuffer sequencerMidi;
        juce::ignoreUnused(renderAndSum(sequencerProcessor, sequencerMidi, 512));
        if (sequencerProcessor.getCurrentPerformanceScene() == 1)
        {
            sceneChanged = true;
            break;
        }
    }
    assert(sceneChanged);
    const auto exportDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("chimera-engine-tests");
    exportDir.createDirectory();
    const auto midiExport = exportDir.getChildFile("processor-song.mid");
    midiExport.deleteFile();
    assert(sequencerProcessor.exportCurrentSongToMidi(midiExport).wasOk());
    assert(midiExport.existsAsFile());
    ChimeraEngineAudioProcessor importedSongProcessor;
    importedSongProcessor.prepareToPlay(48000.0, 512);
    assert(importedSongProcessor.importSongFromMidi(midiExport).wasOk());
    assert(importedSongProcessor.getCurrentSongNoteCount() > 0);
    const auto wavExport = exportDir.getChildFile("processor-demo.wav");
    wavExport.deleteFile();
    assert(sequencerProcessor.bounceDemoToWav(wavExport, 1.0).wasOk());
    assert(wavExport.existsAsFile());
    assert(wavExport.getSize() > 1024);
    sequencerProcessor.resetSequencerPlayback();
    assert(sequencerProcessor.getSequencerTick() == 0);
    assert(sequencerProcessor.getCurrentPerformanceScene() == 0);

    ChimeraEngineAudioProcessor recordingProcessor;
    recordingProcessor.prepareToPlay(48000.0, 512);
    recordingProcessor.setLiveRecordingEnabled(true, true, false);
    juce::MidiBuffer recordOn;
    recordOn.addEvent(juce::MidiMessage::noteOn(1, 62, juce::uint8(100)), 0);
    juce::ignoreUnused(renderAndSum(recordingProcessor, recordOn, 512));
    recordingProcessor.setSequencerPlaybackEnabled(true);
    for (int block = 0; block < 4; ++block)
    {
        juce::MidiBuffer noEvents;
        juce::ignoreUnused(renderAndSum(recordingProcessor, noEvents, 512));
    }
    juce::MidiBuffer recordOff;
    recordOff.addEvent(juce::MidiMessage::noteOff(1, 62), 0);
    juce::ignoreUnused(renderAndSum(recordingProcessor, recordOff, 512));
    assert(recordingProcessor.getCurrentSongNoteCount() > 0);
    assert(recordingProcessor.addPatternPhraseNote(0, 0, 0, 120, 60, 100, 1));
    recordingProcessor.assignPatternSection(0, 12);
    assert(recordingProcessor.getPatternSectionPhrase(0) == 12);
    assert(recordingProcessor.getPatternSectionNoteCount(0) == 1);
    assert(recordingProcessor.saveUserArp(3, "Test User Arp"));
    assert(recordingProcessor.assignArpToLane(2, 3));
    assert(recordingProcessor.getArpLaneAssignment(2) == 3);

    juce::MidiBuffer chord;
    chord.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    chord.addEvent(juce::MidiMessage::noteOn(1, 64, juce::uint8(100)), 0);
    chord.addEvent(juce::MidiMessage::noteOn(1, 67, juce::uint8(100)), 0);

    ChimeraEngineAudioProcessor chordProcessor;
    chordProcessor.prepareToPlay(48000.0, 512);
    const auto chordSum = renderAndSum(chordProcessor, chord, 512);
    assert(chordSum > 0.01f);
    assert(std::isfinite(chordSum));

    juce::MidiBuffer release;
    release.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    const auto releaseSum = renderAndSum(chordProcessor, release, 128);
    assert(releaseSum > 0.0f);

    ChimeraEngineAudioProcessor previewProcessor;
    previewProcessor.prepareToPlay(48000.0, 512);
    previewProcessor.enqueuePreviewNoteOn(1, 72, 0.8f);
    juce::MidiBuffer emptyMidi;
    const auto previewSum = renderAndSum(previewProcessor, emptyMidi, 512);
    assert(previewSum > 0.01f);

    ChimeraEngineAudioProcessor presetProcessor;
    presetProcessor.prepareToPlay(48000.0, 512);
    assert(presetProcessor.loadSynthPreset("Saw").wasOk());
    assert(presetProcessor.getCurrentPatchName() == "Saw");
    juce::MidiBuffer sawMidi;
    sawMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    assert(renderAndSum(presetProcessor, sawMidi, 512) > 0.01f);
    assert(presetProcessor.loadSynthPreset("Square").wasOk());
    assert(presetProcessor.getCurrentPatchName() == "Square");
    assert(presetProcessor.loadSynthPreset("Stack").wasOk());
    assert(presetProcessor.getCurrentPatchName() == "Stack");
    juce::MidiBuffer stackMidi;
    stackMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    assert(renderAndSum(presetProcessor, stackMidi, 512) > 0.01f);
    assert(presetProcessor.loadSynthPreset("Velocity Split").wasOk());
    assert(presetProcessor.getCurrentPatchName() == "Velocity Split");

    ChimeraEngineAudioProcessor lfoPanProcessor;
    lfoPanProcessor.prepareToPlay(48000.0, 512);
    lfoPanProcessor.getParameters().getParameter("fxMix")->setValueNotifyingHost(0.0f);
    assert(lfoPanProcessor.loadSynthPreset("LFO Pan").wasOk());
    juce::MidiBuffer lfoPanMidi;
    lfoPanMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    const auto [lfoPanLeft, lfoPanRight] = renderAndChannelSums(lfoPanProcessor, lfoPanMidi, 512);
    assert(lfoPanLeft > 0.01f && lfoPanRight > 0.01f);
    assert(std::abs(lfoPanLeft - lfoPanRight) > 0.001f);

    ChimeraEngineAudioProcessor expressiveProcessor;
    expressiveProcessor.prepareToPlay(48000.0, 512);
    expressiveProcessor.getParameters().getParameter("fxMix")->setValueNotifyingHost(0.0f);
    assert(expressiveProcessor.loadSynthPreset("Expressive Mono").wasOk());
    juce::MidiBuffer expressiveMidi;
    expressiveMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    expressiveMidi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 96), 64);
    expressiveMidi.addEvent(juce::MidiMessage::channelPressureChange(1, 80), 96);
    expressiveMidi.addEvent(juce::MidiMessage::pitchWheel(1, 12000), 128);
    expressiveMidi.addEvent(juce::MidiMessage::noteOn(1, 67, juce::uint8(100)), 192);
    const auto expressiveSum = renderAndSum(expressiveProcessor, expressiveMidi, 512);
    assert(expressiveSum > 0.01f);
    assert(std::isfinite(expressiveSum));
    juce::MidiBuffer expressiveRelease;
    expressiveRelease.addEvent(juce::MidiMessage::noteOff(1, 67), 0);
    const auto expressiveReleaseSum = renderAndSum(expressiveProcessor, expressiveRelease, 512);
    assert(expressiveReleaseSum > 0.0f);
    assert(std::isfinite(expressiveReleaseSum));

    ChimeraEngineAudioProcessor mpeProcessor;
    mpeProcessor.prepareToPlay(48000.0, 512);
    mpeProcessor.getParameters().getParameter("fxMix")->setValueNotifyingHost(0.0f);
    assert(mpeProcessor.loadSynthPresetForPart(0, "Expressive Mono").wasOk());
    mpeProcessor.setPartMix(1, 1.0f, 0.0f, false);
    mpeProcessor.setMpeExpressionEnabled(true);
    juce::MidiBuffer mpeMidi;
    mpeMidi.addEvent(juce::MidiMessage::noteOn(2, 60, juce::uint8(100)), 0);
    mpeMidi.addEvent(juce::MidiMessage::controllerEvent(2, 1, 100), 32);
    mpeMidi.addEvent(juce::MidiMessage::channelPressureChange(2, 90), 64);
    mpeMidi.addEvent(juce::MidiMessage::pitchWheel(2, 12000), 96);
    const auto mpeSum = renderAndSum(mpeProcessor, mpeMidi, 512);
    assert(mpeSum > 0.01f);
    assert(std::isfinite(mpeSum));

    juce::MidiBuffer lowVelocityMidi;
    lowVelocityMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(40)), 0);
    const auto lowVelocitySum = renderAndSum(presetProcessor, lowVelocityMidi, 512);
    assert(lowVelocitySum > 0.01f);

    ChimeraEngineAudioProcessor midVelocityProcessor;
    midVelocityProcessor.prepareToPlay(48000.0, 512);
    assert(midVelocityProcessor.loadSynthPreset("Velocity Split").wasOk());
    juce::MidiBuffer midVelocityMidi;
    midVelocityMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(90)), 0);
    const auto midVelocitySum = renderAndSum(midVelocityProcessor, midVelocityMidi, 512);
    assert(midVelocitySum > 0.01f);

    ChimeraEngineAudioProcessor highVelocityProcessor;
    highVelocityProcessor.prepareToPlay(48000.0, 512);
    assert(highVelocityProcessor.loadSynthPreset("Velocity Split").wasOk());
    juce::MidiBuffer highVelocityMidi;
    highVelocityMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(120)), 0);
    const auto highVelocitySum = renderAndSum(highVelocityProcessor, highVelocityMidi, 512);
    assert(highVelocitySum > 0.01f);
    assert(std::abs(lowVelocitySum - highVelocitySum) > 0.001f);

    ChimeraEngineAudioProcessor stereoProcessor;
    stereoProcessor.prepareToPlay(48000.0, 512);
    assert(stereoProcessor.loadSynthPreset("Stack").wasOk());
    juce::MidiBuffer stereoMidi;
    stereoMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    const auto [left, right] = renderAndChannelSums(stereoProcessor, stereoMidi, 512);
    assert(left > 0.01f && right > 0.01f);
    assert(std::abs(left - right) > 0.001f);

    ChimeraEngineAudioProcessor arpProcessor;
    arpProcessor.prepareToPlay(48000.0, 512);
    arpProcessor.getParameters().getParameter("arpEnabled")->setValueNotifyingHost(1.0f);
    juce::MidiBuffer arpChord;
    arpChord.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    arpChord.addEvent(juce::MidiMessage::noteOn(1, 64, juce::uint8(100)), 0);
    arpChord.addEvent(juce::MidiMessage::noteOn(1, 67, juce::uint8(100)), 0);
    assert(renderAndSum(arpProcessor, arpChord, 512) > 0.01f);

    auto arpContinuationSum = 0.0f;
    for (int block = 0; block < 16; ++block)
    {
        juce::MidiBuffer noEvents;
        arpContinuationSum += renderAndSum(arpProcessor, noEvents, 512);
    }
    assert(arpContinuationSum > 0.01f);

    ChimeraEngineAudioProcessor performanceArpProcessor;
    performanceArpProcessor.prepareToPlay(48000.0, 512);
    assert(performanceArpProcessor.loadSynthPresetForPart(0, "Stack").wasOk());
    assert(performanceArpProcessor.loadSynthPresetForPart(1, "Velocity Split").wasOk());
    assert(performanceArpProcessor.loadSynthPresetForPart(2, "Saw").wasOk());
    assert(performanceArpProcessor.loadSynthPresetForPart(3, "Square").wasOk());
    performanceArpProcessor.setPerformancePart(0, { 0, 127, 1, 127, 1, true, 0, 1.0f, -0.4f, "Arp Lane 1" });
    performanceArpProcessor.setPerformancePart(1, { 0, 127, 1, 127, 1, true, 1, 0.9f, 0.1f, "Arp Lane 2" });
    performanceArpProcessor.setPerformancePart(2, { 0, 127, 1, 127, 1, true, 2, 0.8f, 0.4f, "Arp Lane 3" });
    performanceArpProcessor.setPerformancePart(3, { 0, 127, 1, 127, 1, true, 3, 0.8f, 0.7f, "Arp Lane 4" });
    performanceArpProcessor.setPerformanceModeEnabled(true);
    performanceArpProcessor.getParameters().getParameter("arpEnabled")->setValueNotifyingHost(1.0f);
    juce::MidiBuffer performanceArpChord;
    performanceArpChord.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    performanceArpChord.addEvent(juce::MidiMessage::noteOn(1, 64, juce::uint8(100)), 0);
    performanceArpChord.addEvent(juce::MidiMessage::noteOn(1, 67, juce::uint8(100)), 0);
    auto performanceArpSum = renderAndSum(performanceArpProcessor, performanceArpChord, 512);
    for (int block = 0; block < 20; ++block)
    {
        juce::MidiBuffer noEvents;
        performanceArpSum += renderAndSum(performanceArpProcessor, noEvents, 512);
    }
    assert(performanceArpSum > 0.01f);
    assert(std::isfinite(performanceArpSum));

    ChimeraEngineAudioProcessor multiPartProcessor;
    multiPartProcessor.prepareToPlay(48000.0, 512);
    assert(multiPartProcessor.loadSynthPresetForPart(1, "Stack").wasOk());
    assert(multiPartProcessor.getPartPatchName(1) == "Stack");
    juce::MidiBuffer multiPartMidi;
    multiPartMidi.addEvent(juce::MidiMessage::noteOn(2, 60, juce::uint8(100)), 0);
    assert(renderAndSum(multiPartProcessor, multiPartMidi, 512) > 0.01f);

    ChimeraEngineAudioProcessor partMixProcessor;
    partMixProcessor.prepareToPlay(48000.0, 512);
    partMixProcessor.setPartMix(1, 1.0f, -1.0f, true);
    juce::MidiBuffer pannedPartMidi;
    pannedPartMidi.addEvent(juce::MidiMessage::noteOn(2, 60, juce::uint8(100)), 0);
    const auto [partMixLeft, partMixRight] = renderAndChannelSums(partMixProcessor, pannedPartMidi, 512);
    assert(partMixLeft > 0.01f);
    assert(partMixRight < partMixLeft * 0.2f);

    ChimeraEngineAudioProcessor disabledPartProcessor;
    disabledPartProcessor.prepareToPlay(48000.0, 512);
    disabledPartProcessor.setPartMix(1, 1.0f, 0.0f, false);
    juce::MidiBuffer disabledPartMidi;
    disabledPartMidi.addEvent(juce::MidiMessage::noteOn(2, 60, juce::uint8(100)), 0);
    assert(renderAndSum(disabledPartProcessor, disabledPartMidi, 512) == 0.0f);

    ChimeraEngineAudioProcessor performanceProcessor;
    performanceProcessor.prepareToPlay(48000.0, 512);
    assert(performanceProcessor.loadSynthPresetForPart(0, "Stack").wasOk());
    assert(performanceProcessor.loadSynthPresetForPart(1, "Velocity Split").wasOk());
    performanceProcessor.setPerformancePart(0, { 0, 59, 1, 127, 1, true, 0, 1.0f, -0.2f, "Lower" });
    performanceProcessor.setPerformancePart(1, { 60, 127, 1, 127, 1, true, 1, 0.9f, 0.2f, "Upper" });
    performanceProcessor.setPerformanceModeEnabled(true);
    assert(performanceProcessor.isPerformanceModeEnabled());
    assert(performanceProcessor.storePerformance(7, "Stored Layer"));
    assert(performanceProcessor.recallPerformance(7));
    assert(performanceProcessor.getPerformanceName(7) == "Stored Layer");

    juce::MidiBuffer lowerPerformanceMidi;
    lowerPerformanceMidi.addEvent(juce::MidiMessage::noteOn(1, 48, juce::uint8(100)), 0);
    assert(renderAndSum(performanceProcessor, lowerPerformanceMidi, 512) > 0.01f);

    juce::MidiBuffer upperPerformanceMidi;
    upperPerformanceMidi.addEvent(juce::MidiMessage::noteOn(1, 72, juce::uint8(120)), 0);
    assert(renderAndSum(performanceProcessor, upperPerformanceMidi, 512) > 0.01f);

    ChimeraEngineAudioProcessor pannedPerformanceProcessor;
    pannedPerformanceProcessor.prepareToPlay(48000.0, 512);
    pannedPerformanceProcessor.setPerformancePart(0, { 0, 127, 1, 127, 1, true, 0, 1.0f, -1.0f, "Hard Left" });
    pannedPerformanceProcessor.setPerformanceModeEnabled(true);
    juce::MidiBuffer pannedPerformanceMidi;
    pannedPerformanceMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    const auto [performanceLeft, performanceRight] = renderAndChannelSums(pannedPerformanceProcessor, pannedPerformanceMidi, 512);
    assert(performanceLeft > 0.01f);
    assert(performanceRight < performanceLeft * 0.2f);

    ChimeraEngineAudioProcessor sceneSnapshotProcessor;
    sceneSnapshotProcessor.prepareToPlay(48000.0, 512);
    sceneSnapshotProcessor.setPartMix(0, 0.42f, -0.5f, true);
    sceneSnapshotProcessor.setInsertEffect(2, chimera::fx::EffectType::Phaser);
    sceneSnapshotProcessor.assignArpToLane(0, 5);
    sceneSnapshotProcessor.captureSceneSnapshot(3, "Breakdown");
    sceneSnapshotProcessor.setPartMix(0, 1.0f, 0.5f, true);
    sceneSnapshotProcessor.setInsertEffect(2, chimera::fx::EffectType::None);
    sceneSnapshotProcessor.applyPerformanceScene(3);
    assert(sceneSnapshotProcessor.getSceneName(3) == "Breakdown");
    assert(std::abs(sceneSnapshotProcessor.getPartLevel(0) - 0.42f) < 0.001f);
    assert(sceneSnapshotProcessor.getInsertEffect(2) == chimera::fx::EffectType::Phaser);
    assert(sceneSnapshotProcessor.getArpLaneAssignment(0) == 5);

    ChimeraEngineAudioProcessor drumProcessor;
    drumProcessor.prepareToPlay(48000.0, 512);
    assert(drumProcessor.mapDrumKey(36, "Kick", 1));
    assert(drumProcessor.getMappedDrumKeyCount() == 1);
    drumProcessor.setDrumKitModeEnabled(true);
    juce::MidiBuffer drumMidi;
    drumMidi.addEvent(juce::MidiMessage::noteOn(1, 36, juce::uint8(110)), 0);
    assert(renderAndSum(drumProcessor, drumMidi, 512) > 0.01f);

    ChimeraEngineAudioProcessor metadataProcessor;
    metadataProcessor.prepareToPlay(48000.0, 512);
    metadataProcessor.setPresetFavorite("Stack", true);
    assert(metadataProcessor.isPresetFavorite("Stack"));
    assert(metadataProcessor.getPresetMetadataSummary("Stack").contains("Favorite"));
    assert(metadataProcessor.getVoiceEditSummary(0).contains("Element"));
    assert(metadataProcessor.getModMatrixSummary(0).contains("Mod matrix"));
    assert(metadataProcessor.indexSampleLibrary(exportDir).wasOk());
    assert(metadataProcessor.getIndexedSampleCount() > 0);
    metadataProcessor.applyMidi2PerNoteController(1, 60, 74, 0.75f);
    juce::MidiBuffer meteredMidi;
    meteredMidi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    assert(renderAndSum(metadataProcessor, meteredMidi, 512) > 0.01f);
    assert(metadataProcessor.getOutputPeakLeft() > 0.0f || metadataProcessor.getOutputPeakRight() > 0.0f);

    ChimeraEngineAudioProcessor stateSourceProcessor;
    stateSourceProcessor.prepareToPlay(48000.0, 512);
    stateSourceProcessor.applyPerformanceScene(1);
    stateSourceProcessor.setMpeExpressionEnabled(true);
    assert(stateSourceProcessor.loadSynthPresetForPart(1, "Stack").wasOk());
    stateSourceProcessor.setPartMix(1, 0.5f, -0.75f, true);
    stateSourceProcessor.setInsertEffect(0, chimera::fx::EffectType::SmallStereo);
    stateSourceProcessor.setInsertEffect(1, chimera::fx::EffectType::Delay);
    stateSourceProcessor.setSystemFxSends(0.22f, 0.33f);
    setFloatParameter(stateSourceProcessor, "masterEqLow", -4.0f);
    setFloatParameter(stateSourceProcessor, "masterEqMid", 2.5f);
    setFloatParameter(stateSourceProcessor, "masterEqHigh", 5.0f);
    setFloatParameter(stateSourceProcessor, "masterCompThreshold", -24.0f);
    setFloatParameter(stateSourceProcessor, "masterCompRatio", 5.0f);
    setFloatParameter(stateSourceProcessor, "masterCompMakeup", 3.0f);
    stateSourceProcessor.assignPatternSection(2, 44);
    assert(stateSourceProcessor.saveUserArp(6, "State Arp"));
    assert(stateSourceProcessor.assignArpToLane(1, 6));
    stateSourceProcessor.captureSceneSnapshot(2, "State Scene");
    stateSourceProcessor.setPresetFavorite("Stack", true);
    stateSourceProcessor.setDrumKitModeEnabled(true);
    stateSourceProcessor.setLiveRecordingEnabled(true, true, true);
    stateSourceProcessor.setPerformancePart(0, { 0, 127, 1, 127, 2, true, 1, 1.0f, 0.0f, "Restored Part" });
    stateSourceProcessor.setPerformanceModeEnabled(true);
    juce::MemoryBlock stateData;
    stateSourceProcessor.getStateInformation(stateData);

    ChimeraEngineAudioProcessor restoredStateProcessor;
    restoredStateProcessor.prepareToPlay(48000.0, 512);
    restoredStateProcessor.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
    assert(restoredStateProcessor.isPerformanceModeEnabled());
    assert(restoredStateProcessor.getCurrentPerformanceScene() == 1);
    assert(restoredStateProcessor.isMpeExpressionEnabled());
    assert(restoredStateProcessor.getPartPatchName(1) == "Stack");
    assert(std::abs(restoredStateProcessor.getPartLevel(1) - 0.5f) < 0.001f);
    assert(std::abs(restoredStateProcessor.getPartPan(1) + 0.75f) < 0.001f);
    assert(restoredStateProcessor.isPartEnabled(1));
    assert(restoredStateProcessor.getInsertEffect(0) == chimera::fx::EffectType::SmallStereo);
    assert(restoredStateProcessor.getInsertEffect(1) == chimera::fx::EffectType::Delay);
    assert(std::abs(restoredStateProcessor.getChorusSend() - 0.22f) < 0.001f);
    assert(std::abs(restoredStateProcessor.getReverbSend() - 0.33f) < 0.001f);
    assert(std::abs(getFloatParameter(restoredStateProcessor, "masterEqLow") + 4.0f) < 0.001f);
    assert(std::abs(getFloatParameter(restoredStateProcessor, "masterEqMid") - 2.5f) < 0.001f);
    assert(std::abs(getFloatParameter(restoredStateProcessor, "masterEqHigh") - 5.0f) < 0.001f);
    assert(std::abs(getFloatParameter(restoredStateProcessor, "masterCompThreshold") + 24.0f) < 0.001f);
    assert(std::abs(getFloatParameter(restoredStateProcessor, "masterCompRatio") - 5.0f) < 0.001f);
    assert(std::abs(getFloatParameter(restoredStateProcessor, "masterCompMakeup") - 3.0f) < 0.001f);
    assert(restoredStateProcessor.getPatternSectionPhrase(2) == 44);
    assert(restoredStateProcessor.getArpLaneAssignment(1) == 6);
    assert(restoredStateProcessor.getSceneName(2) == "State Scene");
    assert(restoredStateProcessor.isPresetFavorite("Stack"));
    assert(restoredStateProcessor.isDrumKitModeEnabled());
    assert(restoredStateProcessor.isLiveRecordingEnabled());
    juce::MidiBuffer restoredPartMidi;
    restoredPartMidi.addEvent(juce::MidiMessage::noteOn(2, 60, juce::uint8(100)), 0);
    assert(renderAndSum(restoredStateProcessor, restoredPartMidi, 512) > 0.01f);

    std::cout << "Processor render test passed\n";
    return 0;
}
