#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "engine/SequencerIO.h"
#include "preset/Preset.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace
{
juce::File projectRoot()
{
   #ifdef CHIMERA_PROJECT_ROOT
    return juce::File(CHIMERA_PROJECT_ROOT);
   #else
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory()
        .getParentDirectory()
        .getParentDirectory();
   #endif
}

float dbToGain(float db)
{
    return std::pow(10.0f, db / 20.0f);
}

std::pair<float, float> panGains(float pan)
{
    const auto clamped = std::clamp(pan, -1.0f, 1.0f);
    return { clamped <= 0.0f ? 1.0f : 1.0f - clamped,
             clamped >= 0.0f ? 1.0f : 1.0f + clamped };
}

int arpeggiatorStepSamples(double sampleRate)
{
    return std::max(1, static_cast<int>(std::round(sampleRate * 0.125)));
}

chimera::dsp::FilterMode filterModeFromString(const juce::String& filterType)
{
    const auto type = filterType.trim().toLowerCase();
    if (type == "bypass") return chimera::dsp::FilterMode::Bypass;
    if (type == "lowpass6") return chimera::dsp::FilterMode::LowPass6;
    if (type == "lowpass12") return chimera::dsp::FilterMode::LowPass12;
    if (type == "lowpass24") return chimera::dsp::FilterMode::LowPass24;
    if (type == "lowpasswide") return chimera::dsp::FilterMode::LowPassWide;
    if (type == "lowpassnarrow") return chimera::dsp::FilterMode::LowPassNarrow;
    if (type == "highpass6") return chimera::dsp::FilterMode::HighPass6;
    if (type == "highpass12") return chimera::dsp::FilterMode::HighPass12;
    if (type == "highpass24") return chimera::dsp::FilterMode::HighPass24;
    if (type == "highpasswide") return chimera::dsp::FilterMode::HighPassWide;
    if (type == "bandpass12") return chimera::dsp::FilterMode::BandPass12;
    if (type == "bandpass24") return chimera::dsp::FilterMode::BandPass24;
    if (type == "bandpasswide") return chimera::dsp::FilterMode::BandPassWide;
    if (type == "bandpassnarrow") return chimera::dsp::FilterMode::BandPassNarrow;
    if (type == "notch") return chimera::dsp::FilterMode::Notch;
    if (type == "peak") return chimera::dsp::FilterMode::Peak;
    if (type == "lowshelf") return chimera::dsp::FilterMode::LowShelf;
    if (type == "highshelf") return chimera::dsp::FilterMode::HighShelf;
    return chimera::dsp::FilterMode::LowPass12;
}

chimera::dsp::ModSource modSourceFromString(const juce::String& source)
{
    const auto id = source.trim().toLowerCase();
    if (id == "pitcheg") return chimera::dsp::ModSource::PitchEnvelope;
    if (id == "filtereg") return chimera::dsp::ModSource::FilterEnvelope;
    if (id == "ampeg") return chimera::dsp::ModSource::AmpEnvelope;
    if (id == "lfo1") return chimera::dsp::ModSource::Lfo1;
    if (id == "lfo2") return chimera::dsp::ModSource::Lfo2;
    if (id == "aftertouch" || id == "modwheel") return chimera::dsp::ModSource::Aftertouch;
    return chimera::dsp::ModSource::Velocity;
}

chimera::dsp::ModDestination modDestinationFromString(const juce::String& destination)
{
    const auto id = destination.trim().toLowerCase();
    if (id == "pitch") return chimera::dsp::ModDestination::Pitch;
    if (id == "cutoff") return chimera::dsp::ModDestination::Cutoff;
    if (id == "pan") return chimera::dsp::ModDestination::Pan;
    return chimera::dsp::ModDestination::Amp;
}

float modSlotSourceValue(chimera::dsp::ModSource source, float velocity, float pitchEg, float filterEg,
                         float ampEg, float lfo1, float lfo2, float aftertouch)
{
    switch (source)
    {
        case chimera::dsp::ModSource::Velocity: return velocity;
        case chimera::dsp::ModSource::PitchEnvelope: return pitchEg;
        case chimera::dsp::ModSource::FilterEnvelope: return filterEg;
        case chimera::dsp::ModSource::AmpEnvelope: return ampEg;
        case chimera::dsp::ModSource::Lfo1: return lfo1;
        case chimera::dsp::ModSource::Lfo2: return lfo2;
        case chimera::dsp::ModSource::Aftertouch: return aftertouch;
    }

    return 0.0f;
}

std::shared_ptr<chimera::dsp::SampleZone> makeZoneFromElement(const chimera::preset::ElementDefinition& element,
                                                              const juce::File& sampleFile)
{
    auto zone = std::make_shared<chimera::dsp::SampleZone>();
    zone->setSource(sampleFile);
    zone->setRootKey(element.rootKey);
    zone->setKeyRange(element.keyLow, element.keyHigh);
    zone->setVelocityRange(element.velocityLow, element.velocityHigh);
    zone->setTuningCents(element.tuningCents);
    return zone;
}
}

ChimeraEngineAudioProcessor::ChimeraEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    insertEffects.fill(chimera::fx::EffectType::None);
    insertEffects[0] = chimera::fx::EffectType::Compressor;
    patternSectionPhrases.fill(0);
    arpLaneAssignments.fill(0);
    fxPresetBank.resize(32);
    for (int scene = 0; scene < static_cast<int>(sceneSnapshots.size()); ++scene)
        sceneSnapshots[static_cast<size_t>(scene)].name = "Scene " + juce::String(scene + 1);
}

juce::AudioProcessorValueTreeState::ParameterLayout ChimeraEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "masterGain", 1 }, "Master Gain",
                                                                 juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f), -12.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "cutoff", 1 }, "Cutoff",
                                                                 juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.35f), 12000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "resonance", 1 }, "Resonance",
                                                                 juce::NormalisableRange<float>(0.1f, 20.0f, 0.01f, 0.35f), 0.707f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "attack", 1 }, "Attack",
                                                                 juce::NormalisableRange<float>(0.0f, 5.0f, 0.001f, 0.4f), 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "release", 1 }, "Release",
                                                                 juce::NormalisableRange<float>(0.0f, 10.0f, 0.001f, 0.4f), 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "fxMix", 1 }, "FX Mix",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "arpGate", 1 }, "Arp Gate",
                                                                 juce::NormalisableRange<float>(0.05f, 1.0f, 0.001f), 0.75f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "masterEqLow", 1 }, "Master EQ Low",
                                                                 juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "masterEqMid", 1 }, "Master EQ Mid",
                                                                 juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "masterEqHigh", 1 }, "Master EQ High",
                                                                 juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "masterCompThreshold", 1 }, "Master Comp Threshold",
                                                                 juce::NormalisableRange<float>(-36.0f, 0.0f, 0.1f), -12.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "masterCompRatio", 1 }, "Master Comp Ratio",
                                                                 juce::NormalisableRange<float>(1.0f, 10.0f, 0.01f), 2.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "masterCompMakeup", 1 }, "Master Comp Makeup",
                                                                 juce::NormalisableRange<float>(-6.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "arpEnabled", 1 }, "Arp Enabled", false));
    return { params.begin(), params.end() };
}

void ChimeraEngineAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    voiceAgeCounter = 0;
    sequencerTick = 0.0;
    lastMonoNotes.fill(-1);
    pitchBendSemitones.fill(0.0f);
    modWheelValues.fill(0.0f);
    aftertouchValues.fill(0.0f);
    arpeggiatorWasEnabled = false;
    for (int lane = 0; lane < chimera::engine::Performance::partCount; ++lane)
    {
        auto& arpLane = arpeggiatorLanes[static_cast<size_t>(lane)];
        arpLane.engine.setMode(lane % 2 == 0 ? chimera::engine::ArpMode::Up : chimera::engine::ArpMode::UpDown);
        arpLane.heldNotes.clear();
        arpLane.activeNotes.clear();
        arpLane.internalPartIndex = lane;
        arpLane.samplesUntilStep = 0;
        arpLane.samplesUntilGate = 0;
        arpLane.enabled = true;
    }
    if (!sequencerDemoSeeded)
        seedDemoSequence();

    for (auto& voice : voices)
    {
        for (auto& envelope : voice.ampEnvelopes)
            envelope.setSampleRate(currentSampleRate);
        for (auto& envelope : voice.pitchEnvelopes)
            envelope.setSampleRate(currentSampleRate);
        for (auto& envelope : voice.filterEnvelopes)
            envelope.setSampleRate(currentSampleRate);
        for (auto& lfo : voice.lfo1)
            lfo.setSampleRate(currentSampleRate);
        for (auto& lfo : voice.lfo2)
            lfo.setSampleRate(currentSampleRate);
        for (auto& filter : voice.filters)
        {
            filter.setSampleRate(currentSampleRate);
            filter.reset();
        }
        for (auto& player : voice.players)
            player.stop();
        for (auto& player : voice.releasePlayers)
            player.stop();
        voice.active = false;
        voice.released = false;
        voice.note = -1;
        voice.targetNote = -1;
        voice.age = 0;
        voice.elementCount = 0;
        voice.partIndex = 0;
        voice.partLevel = 1.0f;
        voice.partPan = 0.0f;
        voice.currentPitchCents = 0.0f;
        voice.targetPitchCents = 0.0f;
        voice.portamentoStepCents = 0.0f;
    }

    for (auto& fx : workstationFx)
    {
        fx.prepare(currentSampleRate);
    }
    applyFxConfiguration(true);

    ignoreUnused(loadDefaultPatch());
}

void ChimeraEngineAudioProcessor::releaseResources()
{
    for (auto& fx : workstationFx)
        fx.reset();
}

bool ChimeraEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

void ChimeraEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    applyMasterFxConfiguration();

    juce::MidiBuffer mergedMidi;
    {
        const juce::ScopedLock lock(pendingMidiLock);
        mergedMidi.swapWith(pendingPreviewMidi);
    }
    mergedMidi.addEvents(midiMessages, 0, buffer.getNumSamples(), 0);
    addSequencerEventsForBlock(mergedMidi, buffer.getNumSamples());

    auto midiIterator = mergedMidi.cbegin();
    const auto midiEnd = mergedMidi.cend();

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto arpeggiatorEnabled = *parameters.getRawParameterValue("arpEnabled") > 0.5f;
        if (!arpeggiatorEnabled && arpeggiatorWasEnabled)
        {
            stopAllActiveArpeggiatorNotes();
            for (int lane = 0; lane < chimera::engine::Performance::partCount; ++lane)
            {
                auto& arpLane = arpeggiatorLanes[static_cast<size_t>(lane)];
                arpLane.heldNotes.clear();
                refreshArpeggiatorHeldNotes(lane);
                arpLane.samplesUntilStep = 0;
                arpLane.samplesUntilGate = 0;
            }
        }
        arpeggiatorWasEnabled = arpeggiatorEnabled;

        while (midiIterator != midiEnd && (*midiIterator).samplePosition <= sample)
        {
            handleMidiMessage((*midiIterator).getMessage());
            ++midiIterator;
        }

        if (arpeggiatorEnabled)
            advanceArpeggiators();

        const auto rendered = renderVoiceSample();
        captureOutputMeters(rendered);
        if (buffer.getNumChannels() == 1)
            buffer.setSample(0, sample, (rendered.left + rendered.right) * 0.5f);
        else
        {
            buffer.setSample(0, sample, rendered.left);
            buffer.setSample(1, sample, rendered.right);
            for (int channel = 2; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample(channel, sample, (rendered.left + rendered.right) * 0.5f);
        }
    }
}

juce::AudioProcessorEditor* ChimeraEngineAudioProcessor::createEditor()
{
    return new ChimeraEngineAudioProcessorEditor(*this);
}

void ChimeraEngineAudioProcessor::setCurrentProgram(int)
{
}

const juce::String ChimeraEngineAudioProcessor::getProgramName(int)
{
    return {};
}

void ChimeraEngineAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void ChimeraEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty("chimeraPerformanceMode", performanceModeEnabled, nullptr);
    {
        const juce::ScopedLock lock(zoneLock);
        for (int part = 0; part < static_cast<int>(maxParts); ++part)
            state.setProperty("chimeraPartPatch" + juce::String(part),
                              parts[static_cast<size_t>(part)].patchName,
                              nullptr);
        for (int part = 0; part < static_cast<int>(maxParts); ++part)
        {
            const auto& partState = parts[static_cast<size_t>(part)];
            const auto prefix = "chimeraPart" + juce::String(part);
            state.setProperty(prefix + "Level", partState.level, nullptr);
            state.setProperty(prefix + "Pan", partState.pan, nullptr);
            state.setProperty(prefix + "Enabled", partState.enabled, nullptr);
        }
    }
    for (int performancePart = 0; performancePart < chimera::engine::Performance::partCount; ++performancePart)
    {
        const auto& zone = activePerformance.getPart(performancePart);
        const auto prefix = "chimeraPerfPart" + juce::String(performancePart);
        state.setProperty(prefix + "Enabled", zone.enabled, nullptr);
        state.setProperty(prefix + "KeyLow", zone.keyLow, nullptr);
        state.setProperty(prefix + "KeyHigh", zone.keyHigh, nullptr);
        state.setProperty(prefix + "VelocityLow", zone.velocityLow, nullptr);
        state.setProperty(prefix + "VelocityHigh", zone.velocityHigh, nullptr);
        state.setProperty(prefix + "MidiChannel", zone.midiChannel, nullptr);
        state.setProperty(prefix + "InternalPart", zone.internalPartIndex, nullptr);
        state.setProperty(prefix + "Level", zone.level, nullptr);
        state.setProperty(prefix + "Pan", zone.pan, nullptr);
        state.setProperty(prefix + "VoiceName", juce::String(zone.voiceName), nullptr);
    }
    for (int slot = 0; slot < chimera::fx::InsertRack::slotCount; ++slot)
        state.setProperty("chimeraFxInsert" + juce::String(slot),
                          static_cast<int>(insertEffects[static_cast<size_t>(slot)]),
                          nullptr);
    state.setProperty("chimeraFxChorusSend", chorusSend, nullptr);
    state.setProperty("chimeraFxReverbSend", reverbSend, nullptr);
    state.setProperty("chimeraSequencerPlayback", sequencerPlaybackEnabled, nullptr);
    state.setProperty("chimeraSequencerTick", sequencerTick, nullptr);
    state.setProperty("chimeraPerformanceScene", currentPerformanceScene, nullptr);
    state.setProperty("chimeraMpeExpression", mpeExpressionEnabled, nullptr);
    state.setProperty("chimeraLiveRecording", liveRecordingEnabled, nullptr);
    state.setProperty("chimeraOverdubRecording", overdubRecordingEnabled, nullptr);
    state.setProperty("chimeraPunchRecording", punchRecordingEnabled, nullptr);
    state.setProperty("chimeraCurrentSequencerTrack", currentSequencerTrack, nullptr);
    state.setProperty("chimeraDrumKitMode", drumKitModeEnabled, nullptr);
    state.setProperty("chimeraIndexedSampleCount", indexedSampleCount, nullptr);
    for (int section = 0; section < chimera::engine::Pattern::sectionCount; ++section)
        state.setProperty("chimeraPatternSectionPhrase" + juce::String(section), patternSectionPhrases[static_cast<size_t>(section)], nullptr);
    for (int lane = 0; lane < chimera::engine::Performance::partCount; ++lane)
        state.setProperty("chimeraArpLaneAssignment" + juce::String(lane), arpLaneAssignments[static_cast<size_t>(lane)], nullptr);
    for (int scene = 0; scene < static_cast<int>(sceneSnapshots.size()); ++scene)
    {
        const auto& snapshot = sceneSnapshots[static_cast<size_t>(scene)];
        const auto prefix = "chimeraScene" + juce::String(scene);
        state.setProperty(prefix + "Name", snapshot.name, nullptr);
        state.setProperty(prefix + "Valid", snapshot.valid, nullptr);
        state.setProperty(prefix + "Chorus", snapshot.chorus, nullptr);
        state.setProperty(prefix + "Reverb", snapshot.reverb, nullptr);
        for (int part = 0; part < static_cast<int>(maxParts); ++part)
        {
            state.setProperty(prefix + "Part" + juce::String(part) + "Level", snapshot.levels[static_cast<size_t>(part)], nullptr);
            state.setProperty(prefix + "Part" + juce::String(part) + "Pan", snapshot.pans[static_cast<size_t>(part)], nullptr);
            state.setProperty(prefix + "Part" + juce::String(part) + "Enabled", snapshot.enabled[static_cast<size_t>(part)], nullptr);
        }
        for (int slot = 0; slot < chimera::fx::InsertRack::slotCount; ++slot)
            state.setProperty(prefix + "Insert" + juce::String(slot),
                              static_cast<int>(snapshot.inserts[static_cast<size_t>(slot)]),
                              nullptr);
        for (int lane = 0; lane < chimera::engine::Performance::partCount; ++lane)
            state.setProperty(prefix + "ArpLane" + juce::String(lane), snapshot.arpAssignments[static_cast<size_t>(lane)], nullptr);
    }
    for (const auto& favorite : presetFavorites)
        state.setProperty("chimeraPresetFavorite_" + favorite.first, favorite.second, nullptr);

    juce::MemoryOutputStream stream(destData, true);
    state.writeToStream(stream);
}

void ChimeraEngineAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes)); tree.isValid())
    {
        std::array<juce::String, maxParts> restoredPartPatches;
        std::array<float, maxParts> restoredPartLevels;
        std::array<float, maxParts> restoredPartPans;
        std::array<bool, maxParts> restoredPartEnabled;
        for (int part = 0; part < static_cast<int>(maxParts); ++part)
        {
            restoredPartPatches[static_cast<size_t>(part)] = tree.getProperty("chimeraPartPatch" + juce::String(part), {}).toString();
            const auto prefix = "chimeraPart" + juce::String(part);
            restoredPartLevels[static_cast<size_t>(part)] = static_cast<float>(tree.getProperty(prefix + "Level", 1.0));
            restoredPartPans[static_cast<size_t>(part)] = static_cast<float>(tree.getProperty(prefix + "Pan", 0.0));
            restoredPartEnabled[static_cast<size_t>(part)] = static_cast<bool>(tree.getProperty(prefix + "Enabled", true));
        }

        performanceModeEnabled = static_cast<bool>(tree.getProperty("chimeraPerformanceMode", false));
        parameters.replaceState(tree);

        for (int part = 0; part < static_cast<int>(maxParts); ++part)
            if (restoredPartPatches[static_cast<size_t>(part)].isNotEmpty())
                ignoreUnused(loadSynthPresetForPart(part, restoredPartPatches[static_cast<size_t>(part)]));
        for (int part = 0; part < static_cast<int>(maxParts); ++part)
            setPartMix(part,
                       restoredPartLevels[static_cast<size_t>(part)],
                       restoredPartPans[static_cast<size_t>(part)],
                       restoredPartEnabled[static_cast<size_t>(part)]);

        for (int performancePart = 0; performancePart < chimera::engine::Performance::partCount; ++performancePart)
        {
            const auto prefix = "chimeraPerfPart" + juce::String(performancePart);
            chimera::engine::PartZone zone;
            zone.enabled = static_cast<bool>(tree.getProperty(prefix + "Enabled", false));
            zone.keyLow = static_cast<int>(tree.getProperty(prefix + "KeyLow", 0));
            zone.keyHigh = static_cast<int>(tree.getProperty(prefix + "KeyHigh", 127));
            zone.velocityLow = static_cast<int>(tree.getProperty(prefix + "VelocityLow", 1));
            zone.velocityHigh = static_cast<int>(tree.getProperty(prefix + "VelocityHigh", 127));
            zone.midiChannel = static_cast<int>(tree.getProperty(prefix + "MidiChannel", 1));
            zone.internalPartIndex = static_cast<int>(tree.getProperty(prefix + "InternalPart", performancePart));
            zone.level = static_cast<float>(tree.getProperty(prefix + "Level", 1.0));
            zone.pan = static_cast<float>(tree.getProperty(prefix + "Pan", 0.0));
            zone.voiceName = tree.getProperty(prefix + "VoiceName", {}).toString().toStdString();
            activePerformance.setPart(performancePart, std::move(zone));
        }

        for (int slot = 0; slot < chimera::fx::InsertRack::slotCount; ++slot)
        {
            const auto rawType = static_cast<int>(tree.getProperty("chimeraFxInsert" + juce::String(slot), static_cast<int>(slot == 0 ? chimera::fx::EffectType::Compressor : chimera::fx::EffectType::None)));
            setInsertEffect(slot, static_cast<chimera::fx::EffectType>(std::clamp(rawType, 0, chimera::fx::effectTypeCount - 1)));
        }
        setSystemFxSends(static_cast<float>(tree.getProperty("chimeraFxChorusSend", 0.18)),
                         static_cast<float>(tree.getProperty("chimeraFxReverbSend", 0.16)));
        sequencerPlaybackEnabled = static_cast<bool>(tree.getProperty("chimeraSequencerPlayback", false));
        sequencerTick = static_cast<double>(tree.getProperty("chimeraSequencerTick", 0.0));
        currentPerformanceScene = std::clamp(static_cast<int>(tree.getProperty("chimeraPerformanceScene", 0)), 0, 7);
        mpeExpressionEnabled = static_cast<bool>(tree.getProperty("chimeraMpeExpression", false));
        liveRecordingEnabled = static_cast<bool>(tree.getProperty("chimeraLiveRecording", false));
        overdubRecordingEnabled = static_cast<bool>(tree.getProperty("chimeraOverdubRecording", true));
        punchRecordingEnabled = static_cast<bool>(tree.getProperty("chimeraPunchRecording", false));
        currentSequencerTrack = std::clamp(static_cast<int>(tree.getProperty("chimeraCurrentSequencerTrack", 0)), 0, chimera::engine::Song::trackCount - 1);
        drumKitModeEnabled = static_cast<bool>(tree.getProperty("chimeraDrumKitMode", false));
        indexedSampleCount = std::max(0, static_cast<int>(tree.getProperty("chimeraIndexedSampleCount", 0)));
        for (int section = 0; section < chimera::engine::Pattern::sectionCount; ++section)
            patternSectionPhrases[static_cast<size_t>(section)] = std::clamp(static_cast<int>(tree.getProperty("chimeraPatternSectionPhrase" + juce::String(section), 0)),
                                                                             0,
                                                                             chimera::engine::Pattern::phraseSlots - 1);
        for (int lane = 0; lane < chimera::engine::Performance::partCount; ++lane)
            arpLaneAssignments[static_cast<size_t>(lane)] = std::clamp(static_cast<int>(tree.getProperty("chimeraArpLaneAssignment" + juce::String(lane), 0)),
                                                                       0,
                                                                       chimera::engine::ArpLibrary::userSlots - 1);
        for (int scene = 0; scene < static_cast<int>(sceneSnapshots.size()); ++scene)
        {
            auto& snapshot = sceneSnapshots[static_cast<size_t>(scene)];
            const auto prefix = "chimeraScene" + juce::String(scene);
            snapshot.name = tree.getProperty(prefix + "Name", "Scene " + juce::String(scene + 1)).toString();
            snapshot.valid = static_cast<bool>(tree.getProperty(prefix + "Valid", false));
            snapshot.chorus = static_cast<float>(tree.getProperty(prefix + "Chorus", 0.18));
            snapshot.reverb = static_cast<float>(tree.getProperty(prefix + "Reverb", 0.16));
            for (int part = 0; part < static_cast<int>(maxParts); ++part)
            {
                snapshot.levels[static_cast<size_t>(part)] = static_cast<float>(tree.getProperty(prefix + "Part" + juce::String(part) + "Level", 1.0));
                snapshot.pans[static_cast<size_t>(part)] = static_cast<float>(tree.getProperty(prefix + "Part" + juce::String(part) + "Pan", 0.0));
                snapshot.enabled[static_cast<size_t>(part)] = static_cast<bool>(tree.getProperty(prefix + "Part" + juce::String(part) + "Enabled", true));
            }
            for (int slot = 0; slot < chimera::fx::InsertRack::slotCount; ++slot)
            {
                const auto rawType = static_cast<int>(tree.getProperty(prefix + "Insert" + juce::String(slot), static_cast<int>(chimera::fx::EffectType::None)));
                snapshot.inserts[static_cast<size_t>(slot)] = static_cast<chimera::fx::EffectType>(std::clamp(rawType, 0, chimera::fx::effectTypeCount - 1));
            }
            for (int lane = 0; lane < chimera::engine::Performance::partCount; ++lane)
                snapshot.arpAssignments[static_cast<size_t>(lane)] = std::clamp(static_cast<int>(tree.getProperty(prefix + "ArpLane" + juce::String(lane), 0)),
                                                                                0,
                                                                                chimera::engine::ArpLibrary::userSlots - 1);
        }
        presetFavorites.clear();
        for (int propertyIndex = 0; propertyIndex < tree.getNumProperties(); ++propertyIndex)
        {
            const auto name = tree.getPropertyName(propertyIndex).toString();
            if (name.startsWith("chimeraPresetFavorite_"))
                presetFavorites[name.fromFirstOccurrenceOf("chimeraPresetFavorite_", false, false)] =
                    static_cast<bool>(tree.getProperty(tree.getPropertyName(propertyIndex), false));
        }
    }
}

void ChimeraEngineAudioProcessor::enqueuePreviewNoteOn(int midiChannel, int midiNote, float velocity)
{
    const juce::ScopedLock lock(pendingMidiLock);
    pendingPreviewMidi.addEvent(juce::MidiMessage::noteOn(midiChannel, midiNote, velocity), 0);
}

void ChimeraEngineAudioProcessor::enqueuePreviewNoteOff(int midiChannel, int midiNote)
{
    const juce::ScopedLock lock(pendingMidiLock);
    pendingPreviewMidi.addEvent(juce::MidiMessage::noteOff(midiChannel, midiNote), 0);
}

juce::Result ChimeraEngineAudioProcessor::loadDefaultPatch()
{
    const auto root = projectRoot();
    const auto patchFile = root.getChildFile("presets/Synth/Sine.chpatch");
    for (int part = 0; part < static_cast<int>(maxParts); ++part)
        if (const auto result = loadPatchFileForPart(part, patchFile); result.failed())
            return result;

    return juce::Result::ok();
}

juce::Result ChimeraEngineAudioProcessor::loadSynthPreset(const juce::String& presetName)
{
    return loadSynthPresetForPart(0, presetName);
}

juce::Result ChimeraEngineAudioProcessor::loadSynthPresetForPart(int partIndex, const juce::String& presetName)
{
    if (!presetName.containsOnly("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 _-"))
        return juce::Result::fail("Preset name contains unsupported characters");
    if (partIndex < 0 || partIndex >= static_cast<int>(maxParts))
        return juce::Result::fail("Part index is out of range");

    return loadPatchFileForPart(partIndex, projectRoot().getChildFile("presets/Synth").getChildFile(presetName + ".chpatch"));
}

juce::String ChimeraEngineAudioProcessor::getPartPatchName(int partIndex) const
{
    if (partIndex < 0 || partIndex >= static_cast<int>(maxParts))
        return {};

    return parts[static_cast<size_t>(partIndex)].patchName;
}

void ChimeraEngineAudioProcessor::setPartMix(int partIndex, float level, float pan, bool enabled)
{
    if (partIndex < 0 || partIndex >= static_cast<int>(maxParts))
        return;

    const juce::ScopedLock lock(zoneLock);
    auto& part = parts[static_cast<size_t>(partIndex)];
    part.level = std::clamp(level, 0.0f, 2.0f);
    part.pan = std::clamp(pan, -1.0f, 1.0f);
    part.enabled = enabled;
}

float ChimeraEngineAudioProcessor::getPartLevel(int partIndex) const
{
    if (partIndex < 0 || partIndex >= static_cast<int>(maxParts))
        return 0.0f;

    return parts[static_cast<size_t>(partIndex)].level;
}

float ChimeraEngineAudioProcessor::getPartPan(int partIndex) const
{
    if (partIndex < 0 || partIndex >= static_cast<int>(maxParts))
        return 0.0f;

    return parts[static_cast<size_t>(partIndex)].pan;
}

bool ChimeraEngineAudioProcessor::isPartEnabled(int partIndex) const
{
    if (partIndex < 0 || partIndex >= static_cast<int>(maxParts))
        return false;

    return parts[static_cast<size_t>(partIndex)].enabled;
}

void ChimeraEngineAudioProcessor::setInsertEffect(int slotIndex, chimera::fx::EffectType type)
{
    if (slotIndex < 0 || slotIndex >= chimera::fx::InsertRack::slotCount)
        return;

    const auto rawType = std::clamp(static_cast<int>(type), 0, chimera::fx::effectTypeCount - 1);
    insertEffects[static_cast<size_t>(slotIndex)] = static_cast<chimera::fx::EffectType>(rawType);
    applyFxConfiguration(true);
}

chimera::fx::EffectType ChimeraEngineAudioProcessor::getInsertEffect(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= chimera::fx::InsertRack::slotCount)
        return chimera::fx::EffectType::None;

    return insertEffects[static_cast<size_t>(slotIndex)];
}

void ChimeraEngineAudioProcessor::setSystemFxSends(float newChorusSend, float newReverbSend)
{
    chorusSend = std::clamp(newChorusSend, 0.0f, 1.0f);
    reverbSend = std::clamp(newReverbSend, 0.0f, 1.0f);
    applyFxConfiguration(false);
}

void ChimeraEngineAudioProcessor::setPerformanceModeEnabled(bool shouldBeEnabled)
{
    performanceModeEnabled = shouldBeEnabled;
}

void ChimeraEngineAudioProcessor::setSequencerPlaybackEnabled(bool shouldPlay)
{
    if (!sequencerDemoSeeded)
        seedDemoSequence();

    sequencerPlaybackEnabled = shouldPlay;
}

void ChimeraEngineAudioProcessor::resetSequencerPlayback()
{
    sequencerTick = 0.0;
    applyPerformanceScene(0);
    stopAllActiveArpeggiatorNotes();
    for (auto& voice : voices)
        if (voice.active)
            releaseVoice(voice);
}

void ChimeraEngineAudioProcessor::seedDemoSequence()
{
    auto& song = sequencer.song(0);
    for (int track = 0; track < chimera::engine::Song::trackCount; ++track)
        song.clearTrack(track);

    song.setTempo(118.0);
    ignoreUnused(loadSynthPresetForPart(0, "Expressive Mono"));
    ignoreUnused(loadSynthPresetForPart(1, "Stack"));
    setPartMix(0, 0.9f, -0.15f, true);
    setPartMix(1, 0.65f, 0.2f, true);
    setPartMix(2, 0.0f, 0.0f, false);
    setPartMix(3, 0.0f, 0.0f, false);
    setInsertEffect(0, chimera::fx::EffectType::Compressor);
    setInsertEffect(1, chimera::fx::EffectType::SmallStereo);
    setSystemFxSends(0.18f, 0.16f);

    constexpr int q = chimera::engine::Song::ppq;
    for (int bar = 0; bar < 2; ++bar)
    {
        const auto base = bar * q * 4;
        song.recordNote(0, base + 0 * q, q / 2, 48, 105, 1);
        song.recordNote(0, base + 1 * q, q / 2, 55, 100, 1);
        song.recordNote(0, base + 2 * q, q / 2, 60, 108, 1);
        song.recordNote(0, base + 3 * q, q / 2, 55, 96, 1);

        song.recordNote(1, base + 0 * q, q * 2, 72, 88, 2);
        song.recordNote(1, base + 2 * q, q * 2, 76, 84, 2);
    }
    song.addSceneEvent({ 0, 0 });
    song.addSceneEvent({ q * 4, 1 });
    applyPerformanceScene(0);
    sequencerDemoSeeded = true;
}

void ChimeraEngineAudioProcessor::applyPerformanceScene(int sceneIndex)
{
    currentPerformanceScene = std::clamp(sceneIndex, 0, 7);
    const auto& snapshot = sceneSnapshots[static_cast<size_t>(currentPerformanceScene)];
    if (snapshot.valid)
    {
        for (int part = 0; part < static_cast<int>(maxParts); ++part)
            setPartMix(part,
                       snapshot.levels[static_cast<size_t>(part)],
                       snapshot.pans[static_cast<size_t>(part)],
                       snapshot.enabled[static_cast<size_t>(part)]);
        for (int slot = 0; slot < chimera::fx::InsertRack::slotCount; ++slot)
            setInsertEffect(slot, snapshot.inserts[static_cast<size_t>(slot)]);
        setSystemFxSends(snapshot.chorus, snapshot.reverb);
        arpLaneAssignments = snapshot.arpAssignments;
        return;
    }

    if (currentPerformanceScene == 0)
    {
        setPartMix(0, 0.9f, -0.15f, true);
        setPartMix(1, 0.65f, 0.2f, true);
        setSystemFxSends(0.18f, 0.16f);
        setInsertEffect(1, chimera::fx::EffectType::SmallStereo);
        arpeggiatorLanes[0].engine.setMode(chimera::engine::ArpMode::Up);
        arpeggiatorLanes[1].engine.setMode(chimera::engine::ArpMode::UpDown);
    }
    else
    {
        setPartMix(0, 0.7f, -0.35f, true);
        setPartMix(1, 0.9f, 0.35f, true);
        setSystemFxSends(0.35f, 0.28f);
        setInsertEffect(1, chimera::fx::EffectType::Delay);
        arpeggiatorLanes[0].engine.setMode(chimera::engine::ArpMode::Down);
        arpeggiatorLanes[1].engine.setMode(chimera::engine::ArpMode::Chord);
    }
}

void ChimeraEngineAudioProcessor::setPerformancePart(int performancePartIndex, chimera::engine::PartZone zone)
{
    zone.internalPartIndex = std::clamp(zone.internalPartIndex, 0, static_cast<int>(maxParts) - 1);
    activePerformance.setPart(performancePartIndex, std::move(zone));
}

void ChimeraEngineAudioProcessor::applyMidi2PerNoteController(int midiChannel, int midiNote, int controller, float value)
{
    const auto clampedChannel = std::clamp(midiChannel, 1, 16);
    const auto clampedNote = std::clamp(midiNote, 0, 127);
    const auto clampedValue = std::clamp(value, 0.0f, 1.0f);
    if (controller == 74 || controller == 128)
        midi2PerNotePressure[{ clampedChannel, clampedNote }] = clampedValue;

    if (mpeExpressionEnabled)
        aftertouchValues[0] = std::max(aftertouchValues[0], clampedValue);
}

bool ChimeraEngineAudioProcessor::ingestMidi2UmpWords(std::uint32_t word0, std::uint32_t word1,
                                                      std::uint32_t word2, std::uint32_t word3)
{
    juce::ignoreUnused(word3);
    const auto messageType = (word0 >> 28) & 0x0f;
    if (messageType != 0x4)
        return false;

    const auto status = static_cast<int>((word1 >> 24) & 0xff);
    const auto group = static_cast<int>((word0 >> 24) & 0x0f);
    const auto channel = (status & 0x0f) + 1;
    const auto opcode = status & 0xf0;
    if (opcode != 0xa0 && opcode != 0xd0 && opcode != 0x20)
        return false;

    const auto note = static_cast<int>((word1 >> 16) & 0x7f);
    const auto controller = opcode == 0xa0 ? static_cast<int>((word1 >> 8) & 0xff) : 128;
    const auto value = static_cast<float>(static_cast<double>(word2) / static_cast<double>(std::numeric_limits<std::uint32_t>::max()));
    applyMidi2PerNoteController(channel, note, controller, value);
    pushUndoAction("MIDI 2.0 UMP group " + juce::String(group + 1)
                   + " note " + juce::String(note)
                   + " controller " + juce::String(controller));
    return true;
}

void ChimeraEngineAudioProcessor::setLiveRecordingEnabled(bool shouldRecord, bool overdub, bool punch)
{
    liveRecordingEnabled = shouldRecord;
    overdubRecordingEnabled = overdub;
    punchRecordingEnabled = punch;
    activeRecordingNotes.clear();
    if (liveRecordingEnabled && !overdubRecordingEnabled)
        sequencer.song(0).clearTrack(currentSequencerTrack);
}

void ChimeraEngineAudioProcessor::setCurrentSequencerTrack(int trackIndex)
{
    currentSequencerTrack = std::clamp(trackIndex, 0, chimera::engine::Song::trackCount - 1);
}

bool ChimeraEngineAudioProcessor::addPatternPhraseNote(int sectionIndex, int trackIndex, int tick, int durationTicks, int note, int velocity, int channel)
{
    const auto ok = sequencer.pattern(0).addPhraseNote(sectionIndex,
                                                       trackIndex,
                                                       { tick, durationTicks, note, velocity, channel });
    if (ok)
        pushUndoAction("Pattern note S" + juce::String(sectionIndex + 1)
                       + " T" + juce::String(trackIndex + 1)
                       + " N" + juce::String(note));
    return ok;
}

void ChimeraEngineAudioProcessor::assignPatternSection(int sectionIndex, int phraseSlot)
{
    if (sectionIndex < 0 || sectionIndex >= chimera::engine::Pattern::sectionCount)
        return;

    patternSectionPhrases[static_cast<size_t>(sectionIndex)] = std::clamp(phraseSlot, 0, chimera::engine::Pattern::phraseSlots - 1);
    pushUndoAction("Pattern section " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + sectionIndex))
                   + " phrase " + juce::String(patternSectionPhrases[static_cast<size_t>(sectionIndex)]));
}

int ChimeraEngineAudioProcessor::getPatternSectionPhrase(int sectionIndex) const
{
    if (sectionIndex < 0 || sectionIndex >= chimera::engine::Pattern::sectionCount)
        return 0;

    return patternSectionPhrases[static_cast<size_t>(sectionIndex)];
}

int ChimeraEngineAudioProcessor::getPatternSectionNoteCount(int sectionIndex) const
{
    if (sectionIndex < 0 || sectionIndex >= chimera::engine::Pattern::sectionCount)
        return 0;

    auto total = 0;
    const auto& section = sequencer.pattern(0).section(sectionIndex);
    for (const auto& track : section.tracks)
        total += track.noteCount();
    return total;
}

bool ChimeraEngineAudioProcessor::saveUserArp(int slotIndex, const juce::String& name)
{
    chimera::engine::ArpPattern pattern;
    pattern.id = std::clamp(slotIndex, 0, chimera::engine::ArpLibrary::userSlots - 1);
    pattern.name = name.isEmpty() ? "User Arp " + std::to_string(pattern.id + 1) : name.toStdString();
    pattern.category = "User";
    pattern.lengthTicks = chimera::engine::Song::ppq;
    pattern.steps = { { 0, 0, 100, 120 }, { 120, 4, 96, 120 }, { 240, 7, 96, 120 }, { 360, 12, 92, 120 } };
    const auto ok = arpLibrary.setUser(slotIndex, std::move(pattern));
    if (ok)
        pushUndoAction("Saved user arp " + juce::String(slotIndex + 1));
    return ok;
}

bool ChimeraEngineAudioProcessor::assignArpToLane(int laneIndex, int userSlotIndex)
{
    if (laneIndex < 0 || laneIndex >= chimera::engine::Performance::partCount)
        return false;
    if (!arpLibrary.getUser(userSlotIndex).has_value() && !saveUserArp(userSlotIndex, {}))
        return false;

    arpLaneAssignments[static_cast<size_t>(laneIndex)] = std::clamp(userSlotIndex, 0, chimera::engine::ArpLibrary::userSlots - 1);
    pushUndoAction("Assigned arp lane " + juce::String(laneIndex + 1)
                   + " to user " + juce::String(arpLaneAssignments[static_cast<size_t>(laneIndex)] + 1));
    return true;
}

int ChimeraEngineAudioProcessor::getArpLaneAssignment(int laneIndex) const
{
    if (laneIndex < 0 || laneIndex >= chimera::engine::Performance::partCount)
        return 0;

    return arpLaneAssignments[static_cast<size_t>(laneIndex)];
}

bool ChimeraEngineAudioProcessor::storePerformance(int index, const juce::String& name)
{
    if (index < 0 || index >= chimera::engine::PerformanceBank::slotCount)
        return false;

    chimera::engine::PerformanceSlot slot;
    slot.name = name.isEmpty() ? "Stored Performance " + std::to_string(index + 1) : name.toStdString();
    for (int part = 0; part < chimera::engine::Performance::partCount; ++part)
        slot.parts[static_cast<size_t>(part)] = activePerformance.getPart(part);
    performanceBank.setPerformance(index, std::move(slot));
    pushUndoAction("Stored performance " + juce::String(index + 1));
    return true;
}

bool ChimeraEngineAudioProcessor::recallPerformance(int index)
{
    if (index < 0 || index >= chimera::engine::PerformanceBank::slotCount)
        return false;

    const auto& slot = performanceBank.getPerformance(index);
    for (int part = 0; part < chimera::engine::Performance::partCount; ++part)
        activePerformance.setPart(part, slot.parts[static_cast<size_t>(part)]);
    performanceModeEnabled = true;
    pushUndoAction("Recalled performance " + juce::String(index + 1));
    return true;
}

juce::String ChimeraEngineAudioProcessor::getPerformanceName(int index) const
{
    if (index < 0 || index >= chimera::engine::PerformanceBank::slotCount)
        return {};

    return performanceBank.getPerformance(index).name;
}

void ChimeraEngineAudioProcessor::captureSceneSnapshot(int sceneIndex, const juce::String& name)
{
    if (sceneIndex < 0 || sceneIndex >= static_cast<int>(sceneSnapshots.size()))
        return;

    auto& snapshot = sceneSnapshots[static_cast<size_t>(sceneIndex)];
    snapshot.name = name.isEmpty() ? "Scene " + juce::String(sceneIndex + 1) : name;
    for (int part = 0; part < static_cast<int>(maxParts); ++part)
    {
        snapshot.levels[static_cast<size_t>(part)] = getPartLevel(part);
        snapshot.pans[static_cast<size_t>(part)] = getPartPan(part);
        snapshot.enabled[static_cast<size_t>(part)] = isPartEnabled(part);
    }
    snapshot.inserts = insertEffects;
    snapshot.arpAssignments = arpLaneAssignments;
    snapshot.chorus = chorusSend;
    snapshot.reverb = reverbSend;
    snapshot.valid = true;
    pushUndoAction("Captured scene " + juce::String(sceneIndex + 1));
}

juce::String ChimeraEngineAudioProcessor::getSceneName(int sceneIndex) const
{
    if (sceneIndex < 0 || sceneIndex >= static_cast<int>(sceneSnapshots.size()))
        return {};

    return sceneSnapshots[static_cast<size_t>(sceneIndex)].name;
}

bool ChimeraEngineAudioProcessor::mapDrumKey(int midiNote, const juce::String& name, int waveformId)
{
    const auto ok = drumKit.setKey({ midiNote, waveformId, name.toStdString(), 1.0f, 0.0f, 9, false, 0 });
    if (ok)
        pushUndoAction("Mapped drum key " + juce::String(midiNote));
    return ok;
}

int ChimeraEngineAudioProcessor::getMappedDrumKeyCount() const
{
    return drumKit.mappedKeyCount();
}

juce::Result ChimeraEngineAudioProcessor::indexSampleLibrary(const juce::File& root)
{
    if (!root.isDirectory())
        return juce::Result::fail("Sample library folder does not exist: " + root.getFullPathName());

    auto files = root.findChildFiles(juce::File::findFiles, true, "*.wav;*.aif;*.aiff");
    indexedSampleCount = files.size();
    for (int i = 0; i < files.size(); ++i)
        sampleLibrary.addUserWaveform(0,
                                      { 100000 + i,
                                        files[i].getFileNameWithoutExtension().toStdString(),
                                        "Imported",
                                        static_cast<std::uint64_t>(std::max<juce::int64>(1, files[i].getSize())),
                                        60,
                                        0,
                                        127,
                                        1,
                                        127 });
    if (indexedSampleCount > 0)
    {
        pushUndoAction("Indexed " + juce::String(indexedSampleCount) + " samples");
        return juce::Result::ok();
    }
    return juce::Result::fail("No audio files found in: " + root.getFullPathName());
}

juce::Result ChimeraEngineAudioProcessor::startSampleImportJob(const juce::File& root)
{
    sampleImportRunning = true;
    const auto result = indexSampleLibrary(root);
    sampleImportRunning = false;
    sampleImportReport = result.wasOk()
        ? "Import complete: " + juce::String(indexedSampleCount) + " audio files indexed from " + root.getFileName()
        : "Import failed: " + result.getErrorMessage();
    return result;
}

void ChimeraEngineAudioProcessor::setPresetFavorite(const juce::String& presetName, bool shouldBeFavorite)
{
    presetFavorites[presetName] = shouldBeFavorite;
    pushUndoAction("Preset favorite " + presetName + (shouldBeFavorite ? " on" : " off"));
}

bool ChimeraEngineAudioProcessor::isPresetFavorite(const juce::String& presetName) const
{
    if (const auto found = presetFavorites.find(presetName); found != presetFavorites.end())
        return found->second;
    return false;
}

juce::String ChimeraEngineAudioProcessor::getPresetMetadataSummary(const juce::String& presetName) const
{
    chimera::preset::Patch patch;
    const auto file = projectRoot().getChildFile("presets/Synth").getChildFile(presetName + ".chpatch");
    if (chimera::preset::loadPatch(file, patch).failed())
        return "Preset metadata unavailable";

    return patch.metadata.category + "  Elements " + juce::String(static_cast<int>(patch.elements.size()))
        + "  Mode " + patch.voiceMode
        + (isPresetFavorite(presetName) ? "  Favorite" : "");
}

juce::String ChimeraEngineAudioProcessor::getVoiceEditSummary(int elementIndex) const
{
    const auto clampedElement = std::clamp(elementIndex, 0, static_cast<int>(maxElements) - 1);
    const auto& part = parts[0];
    if (clampedElement >= part.loadedElementCount)
        return "Element " + juce::String(clampedElement + 1) + ": empty";

    const auto& element = part.loadedElements[static_cast<size_t>(clampedElement)];
    return "Element " + juce::String(clampedElement + 1)
        + " Amp " + juce::String(element.ampAttack, 2) + "/" + juce::String(element.ampDecay1, 2) + "/" + juce::String(element.ampRelease, 2)
        + " PitchDepth " + juce::String(element.pitchDepthCents, 1)
        + " FilterDepth " + juce::String(element.filterDepth, 2);
}

juce::String ChimeraEngineAudioProcessor::getModMatrixSummary(int elementIndex) const
{
    const auto clampedElement = std::clamp(elementIndex, 0, static_cast<int>(maxElements) - 1);
    const auto& part = parts[0];
    if (clampedElement >= part.loadedElementCount)
        return "Mod matrix: no element";

    const auto& element = part.loadedElements[static_cast<size_t>(clampedElement)];
    return "Mod matrix slots: " + juce::String(element.modSlotCount) + " serialized in patch";
}

int ChimeraEngineAudioProcessor::getCurrentSongNoteCount() const
{
    return sequencer.song(0).noteCount();
}

juce::Result ChimeraEngineAudioProcessor::exportCurrentSongToMidi(const juce::File& file) const
{
    if (file == juce::File())
        return juce::Result::fail("No MIDI export file selected");

    file.getParentDirectory().createDirectory();
    return chimera::engine::exportSongToMidiFile(sequencer.song(0), file);
}

juce::Result ChimeraEngineAudioProcessor::importSongFromMidi(const juce::File& file)
{
    const auto result = chimera::engine::importSongFromMidiFile(file, sequencer.song(0));
    if (result.wasOk())
    {
        sequencerDemoSeeded = true;
        resetSequencerPlayback();
    }
    return result;
}

juce::Result ChimeraEngineAudioProcessor::bounceDemoToWav(const juce::File& file, double durationSeconds)
{
    if (file == juce::File())
        return juce::Result::fail("No WAV export file selected");

    file.getParentDirectory().createDirectory();
    std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
    if (stream == nullptr || !stream->openedOk())
        return juce::Result::fail("Could not open WAV file for writing: " + file.getFullPathName());

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(), currentSampleRate, 2, 24, {}, 0));
    if (writer == nullptr)
        return juce::Result::fail("Could not create WAV writer: " + file.getFullPathName());
    stream.release();

    const auto wasPlaying = sequencerPlaybackEnabled;
    const auto previousTick = sequencerTick;
    resetSequencerPlayback();
    sequencerPlaybackEnabled = true;

    constexpr int blockSize = 512;
    const auto totalSamples = std::max(blockSize, static_cast<int>(std::round(durationSeconds * currentSampleRate)));
    juce::AudioBuffer<float> renderBuffer(2, blockSize);
    auto renderedSamples = 0;
    while (renderedSamples < totalSamples)
    {
        const auto blockSamples = std::min(blockSize, totalSamples - renderedSamples);
        renderBuffer.setSize(2, blockSamples, false, false, true);
        juce::MidiBuffer midi;
        processBlock(renderBuffer, midi);
        if (!writer->writeFromAudioSampleBuffer(renderBuffer, 0, blockSamples))
        {
            sequencerPlaybackEnabled = wasPlaying;
            sequencerTick = previousTick;
            return juce::Result::fail("Could not write WAV audio: " + file.getFullPathName());
        }
        renderedSamples += blockSamples;
    }

    sequencerPlaybackEnabled = wasPlaying;
    sequencerTick = previousTick;
    return juce::Result::ok();
}

juce::Result ChimeraEngineAudioProcessor::writeCurrentPatchEdit(const juce::File& file) const
{
    if (file == juce::File())
        return juce::Result::fail("No patch writeback file selected");

    chimera::preset::Patch patch;
    const auto source = projectRoot().getChildFile("presets/Synth").getChildFile(currentPatchName + ".chpatch");
    if (const auto result = chimera::preset::loadPatch(source, patch); result.failed())
        return result;

    auto parsed = juce::JSON::parse(source);
    if (!parsed.isObject())
        return juce::Result::fail("Current patch is not valid JSON: " + source.getFullPathName());

    auto* object = parsed.getDynamicObject();
    object->setProperty("editedBy", "Chimera Engine");
    object->setProperty("editTimestamp", juce::Time::getCurrentTime().toISO8601(true));
    object->setProperty("lastEdit", lastEditDescription);
    object->setProperty("currentPatchName", currentPatchName);
    if (auto* metadata = object->getProperty("metadata").getDynamicObject())
        metadata->setProperty("favorite", isPresetFavorite(currentPatchName));

    file.getParentDirectory().createDirectory();
    if (!file.replaceWithText(juce::JSON::toString(parsed, true)))
        return juce::Result::fail("Could not write patch edit file: " + file.getFullPathName());

    return juce::Result::ok();
}

bool ChimeraEngineAudioProcessor::saveFxPreset(int index, const juce::String& name)
{
    if (index < 0 || index >= static_cast<int>(fxPresetBank.size()))
        return false;

    auto& preset = fxPresetBank[static_cast<size_t>(index)];
    preset.name = name.isEmpty() ? "FX Preset " + juce::String(index + 1) : name;
    preset.inserts = insertEffects;
    preset.chorus = chorusSend;
    preset.reverb = reverbSend;
    preset.eqLow = parameters.getRawParameterValue("masterEqLow")->load();
    preset.eqMid = parameters.getRawParameterValue("masterEqMid")->load();
    preset.eqHigh = parameters.getRawParameterValue("masterEqHigh")->load();
    preset.compThreshold = parameters.getRawParameterValue("masterCompThreshold")->load();
    preset.compRatio = parameters.getRawParameterValue("masterCompRatio")->load();
    preset.compMakeup = parameters.getRawParameterValue("masterCompMakeup")->load();
    preset.valid = true;
    pushUndoAction("Saved FX preset " + juce::String(index + 1));
    return true;
}

bool ChimeraEngineAudioProcessor::loadFxPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(fxPresetBank.size()))
        return false;

    const auto& preset = fxPresetBank[static_cast<size_t>(index)];
    if (!preset.valid)
        return false;

    insertEffects = preset.inserts;
    chorusSend = preset.chorus;
    reverbSend = preset.reverb;
    setFloatParameterValue("masterEqLow", preset.eqLow);
    setFloatParameterValue("masterEqMid", preset.eqMid);
    setFloatParameterValue("masterEqHigh", preset.eqHigh);
    setFloatParameterValue("masterCompThreshold", preset.compThreshold);
    setFloatParameterValue("masterCompRatio", preset.compRatio);
    setFloatParameterValue("masterCompMakeup", preset.compMakeup);
    applyFxConfiguration(true);
    pushUndoAction("Loaded FX preset " + juce::String(index + 1));
    return true;
}

juce::String ChimeraEngineAudioProcessor::getFxPresetName(int index) const
{
    if (index < 0 || index >= static_cast<int>(fxPresetBank.size()))
        return {};

    const auto& preset = fxPresetBank[static_cast<size_t>(index)];
    return preset.valid ? preset.name : "Empty FX " + juce::String(index + 1);
}

juce::String ChimeraEngineAudioProcessor::undoLastEdit()
{
    if (undoStack.empty())
        return {};

    const auto action = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back(action);
    lastEditDescription = "Undo: " + action;
    return lastEditDescription;
}

juce::String ChimeraEngineAudioProcessor::redoLastEdit()
{
    if (redoStack.empty())
        return {};

    const auto action = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back(action);
    lastEditDescription = "Redo: " + action;
    return lastEditDescription;
}

void ChimeraEngineAudioProcessor::editPianoRollNoteFromCanvas(float xNorm, float yNorm)
{
    const auto tick = std::clamp(static_cast<int>(xNorm * static_cast<float>(chimera::engine::Song::ppq * 8)), 0,
                                 chimera::engine::Song::ppq * 8 - 1);
    const auto note = std::clamp(84 - static_cast<int>(yNorm * 48.0f), 36, 84);
    if (sequencer.song(0).recordNote(currentSequencerTrack, tick, 120, note, 104, 1))
    {
        sequencerDemoSeeded = true;
        pushUndoAction("Piano-roll note " + juce::String(note) + " at tick " + juce::String(tick));
    }
}

void ChimeraEngineAudioProcessor::editArpStepFromCanvas(int lane, int step, float velocityNorm)
{
    const auto clampedLane = std::clamp(lane, 0, chimera::engine::Performance::partCount - 1);
    const auto slot = std::clamp(clampedLane * 16 + std::clamp(step, 0, 15), 0, chimera::engine::ArpLibrary::userSlots - 1);
    const auto velocity = std::clamp(static_cast<int>(velocityNorm * 127.0f), 1, 127);
    if (saveUserArp(slot, "Grid Arp " + juce::String(slot + 1)))
    {
        assignArpToLane(clampedLane, slot);
        pushUndoAction("Arp grid lane " + juce::String(clampedLane + 1) + " step " + juce::String(step + 1)
                       + " velocity " + juce::String(velocity));
    }
}

void ChimeraEngineAudioProcessor::editPatternCellFromCanvas(int sectionIndex)
{
    const auto section = std::clamp(sectionIndex, 0, chimera::engine::Pattern::sectionCount - 1);
    const auto phrase = (getPatternSectionPhrase(section) + 1) % chimera::engine::Pattern::phraseSlots;
    assignPatternSection(section, phrase == 0 ? 1 : phrase);
    addPatternPhraseNote(section, section % chimera::engine::Song::trackCount, section * 120, 120, 48 + (section % 24), 96, 1);
}

void ChimeraEngineAudioProcessor::editDrumPadFromCanvas(int padIndex)
{
    const auto pad = std::clamp(padIndex, 0, 31);
    mapDrumKey(36 + pad, "Pad " + juce::String(pad + 1), 100000 + pad);
    drumKitModeEnabled = true;
}

void ChimeraEngineAudioProcessor::editSampleZoneFromCanvas(int zoneIndex)
{
    const auto zone = std::clamp(zoneIndex, 0, 127);
    sampleLibrary.addUserWaveform(0,
                                  { 110000 + zone,
                                    std::string("Mapped Zone ") + std::to_string(zone + 1),
                                    "Mapped",
                                    1024,
                                    36 + (zone % 49),
                                    0,
                                    127,
                                    1,
                                    127 });
    indexedSampleCount = std::max(indexedSampleCount, zone + 1);
    pushUndoAction("Mapped sample zone " + juce::String(zone + 1));
}

void ChimeraEngineAudioProcessor::editModMatrixCellFromCanvas(int sourceIndex, int destinationIndex)
{
    const auto source = std::clamp(sourceIndex, 0, 5);
    const auto destination = std::clamp(destinationIndex, 0, 3);
    auto& part = parts[0];
    if (part.loadedElementCount > 0)
    {
        auto& element = part.loadedElements[0];
        const auto slot = std::clamp(element.modSlotCount, 0, 7);
        element.modSlots[static_cast<size_t>(slot)].source = static_cast<chimera::dsp::ModSource>(std::min(source, 6));
        element.modSlots[static_cast<size_t>(slot)].destination = static_cast<chimera::dsp::ModDestination>(std::min(destination, 3));
        element.modSlots[static_cast<size_t>(slot)].depth = 0.25f + 0.1f * static_cast<float>((source + destination) % 4);
        element.modSlots[static_cast<size_t>(slot)].enabled = true;
        element.modSlotCount = std::min(slot + 1, 8);
    }
    pushUndoAction("Mod matrix S" + juce::String(source + 1) + " D" + juce::String(destination + 1));
}

juce::Result ChimeraEngineAudioProcessor::loadPatchFileForPart(int partIndex, const juce::File& patchFile)
{
    chimera::preset::Patch patch;
    if (const auto result = chimera::preset::loadPatch(patchFile, patch); result.failed())
        return result;

    if (patch.elements.empty())
        return juce::Result::fail("Default patch has no elements");

    std::array<LoadedElement, maxElements> elements;
    auto count = 0;

    for (const auto& element : patch.elements)
    {
        if (count >= static_cast<int>(maxElements))
            break;

        LoadedElement loaded;
        loaded.zones.push_back(makeZoneFromElement(element, projectRoot().getChildFile(element.samplePath)));

        for (const auto& alternate : element.alternateSamples)
            loaded.zones.push_back(makeZoneFromElement(element, projectRoot().getChildFile(alternate)));
        for (const auto& releaseSample : element.releaseSamples)
            loaded.releaseZones.push_back(makeZoneFromElement(element, projectRoot().getChildFile(releaseSample)));

        for (const auto& zone : loaded.zones)
            if (const auto result = zone->loadAudio(); result.failed())
                return result;
        for (const auto& zone : loaded.releaseZones)
            if (const auto result = zone->loadAudio(); result.failed())
                return result;

        if (loaded.zones.empty())
            return juce::Result::fail("Patch element has no playable zones");

        loaded.alternateMode = element.alternateMode;
        loaded.level = element.level;
        loaded.pan = element.pan;
        loaded.filterMode = filterModeFromString(element.filterType);
        loaded.ampAttack = element.ampAttack;
        loaded.ampDecay1 = element.ampDecay1;
        loaded.ampDecay2 = element.ampDecay2;
        loaded.ampSustain = element.ampSustain;
        loaded.ampRelease = element.ampRelease;
        loaded.pitchAttack = element.pitchEnvelope.attack;
        loaded.pitchDecay1 = element.pitchEnvelope.decay1;
        loaded.pitchDecay2 = element.pitchEnvelope.decay2;
        loaded.pitchSustain = element.pitchEnvelope.sustain;
        loaded.pitchRelease = element.pitchEnvelope.release;
        loaded.pitchDepthCents = element.pitchEnvelope.depth;
        loaded.filterAttack = element.filterEnvelope.attack;
        loaded.filterDecay1 = element.filterEnvelope.decay1;
        loaded.filterDecay2 = element.filterEnvelope.decay2;
        loaded.filterSustain = element.filterEnvelope.sustain;
        loaded.filterRelease = element.filterEnvelope.release;
        loaded.filterDepth = element.filterEnvelope.depth;
        loaded.lfo1RateHz = element.lfo1RateHz;
        loaded.lfo1CutoffDepth = element.lfo1CutoffDepth;
        loaded.lfo2RateHz = element.lfo2RateHz;
        loaded.lfo2AmpDepth = element.lfo2AmpDepth;
        loaded.lfo2PanDepth = element.lfo2PanDepth;
        loaded.modSlotCount = std::min(static_cast<int>(element.modSlots.size()), 8);
        for (int slot = 0; slot < loaded.modSlotCount; ++slot)
        {
            loaded.modSlots[static_cast<size_t>(slot)].source = modSourceFromString(element.modSlots[static_cast<size_t>(slot)].source);
            loaded.modSlots[static_cast<size_t>(slot)].destination = modDestinationFromString(element.modSlots[static_cast<size_t>(slot)].destination);
            loaded.modSlots[static_cast<size_t>(slot)].depth = element.modSlots[static_cast<size_t>(slot)].depth;
            loaded.modSlots[static_cast<size_t>(slot)].enabled = element.modSlots[static_cast<size_t>(slot)].enabled;
        }
        elements[static_cast<size_t>(count)] = std::move(loaded);
        ++count;
    }

    if (count <= 0)
        return juce::Result::fail("Patch contains no loadable elements");

    setActiveElementsForPart(partIndex, std::move(elements), count, patch.metadata.name);
    {
        const juce::ScopedLock lock(zoneLock);
        auto& part = parts[static_cast<size_t>(partIndex)];
        part.voiceMode = patch.voiceMode;
        part.portamentoTime = patch.portamentoTime;
        part.pitchBendRange = patch.pitchBendRange;
    }
    return juce::Result::ok();
}

void ChimeraEngineAudioProcessor::setActiveElementsForPart(int partIndex, std::array<LoadedElement, maxElements> elements,
                                                           int count, const juce::String& patchName)
{
    if (partIndex < 0 || partIndex >= static_cast<int>(maxParts))
        return;

    const juce::ScopedLock lock(zoneLock);
    auto& part = parts[static_cast<size_t>(partIndex)];
    part.loadedElements = std::move(elements);
    part.loadedElementCount = std::clamp(count, 0, static_cast<int>(maxElements));
    part.patchName = patchName;
    if (partIndex == 0)
        currentPatchName = patchName;

    for (auto& voice : voices)
    {
        if (voice.partIndex != partIndex)
            continue;

        voice.active = false;
        voice.note = -1;
        for (auto& envelope : voice.ampEnvelopes)
            envelope.noteOff();
        for (auto& filter : voice.filters)
            filter.reset();
        voice.elementCount = 0;
        for (auto& player : voice.players)
            player.stop();
    }
}

void ChimeraEngineAudioProcessor::handleMidiMessage(const juce::MidiMessage& message)
{
    const auto arpeggiatorEnabled = *parameters.getRawParameterValue("arpEnabled") > 0.5f;
    const auto rawPartIndex = std::clamp(message.getChannel() - 1, 0, static_cast<int>(maxParts) - 1);
    const auto partIndex = mpeExpressionEnabled && message.getChannel() > 1 ? 0 : rawPartIndex;
    recordLiveMidiMessage(message);

    if (message.isNoteOn())
    {
        if (drumKitModeEnabled)
        {
            if (const auto drumKey = drumKit.getKey(message.getNoteNumber()))
            {
                startVoice(allocateVoice(),
                           std::clamp(drumKey->outputBus, 0, static_cast<int>(maxParts) - 1),
                           message.getNoteNumber(),
                           std::clamp(static_cast<int>(message.getVelocity()), 1, 127),
                           drumKey->level,
                           drumKey->pan);
                return;
            }
        }

        if (arpeggiatorEnabled)
        {
            const auto note = message.getNoteNumber();
            const auto velocity = std::clamp(static_cast<int>(message.getVelocity()), 1, 127);
            if (performanceModeEnabled)
            {
                for (int performancePart = 0; performancePart < chimera::engine::Performance::partCount; ++performancePart)
                {
                    if (!activePerformance.partMatches(performancePart, note, velocity, message.getChannel()))
                        continue;

                    const auto& zone = activePerformance.getPart(performancePart);
                    auto& lane = arpeggiatorLanes[static_cast<size_t>(performancePart)];
                    lane.internalPartIndex = zone.internalPartIndex;
                    lane.enabled = zone.enabled;
                    addHeldArpeggiatorNote(performancePart, zone.internalPartIndex, note, velocity);
                    if (lane.activeNotes.empty())
                        lane.samplesUntilStep = 0;
                }
            }
            else
            {
                auto& lane = arpeggiatorLanes[0];
                lane.internalPartIndex = partIndex;
                lane.enabled = true;
                addHeldArpeggiatorNote(0, partIndex, note, velocity);
                if (lane.activeNotes.empty())
                    lane.samplesUntilStep = 0;
            }
            return;
        }

        const juce::ScopedLock lock(zoneLock);
        if (performanceModeEnabled)
        {
            for (int performancePart = 0; performancePart < chimera::engine::Performance::partCount; ++performancePart)
            {
                if (!activePerformance.partMatches(performancePart, message.getNoteNumber(),
                                                   message.getVelocity(), message.getChannel()))
                    continue;

                const auto& zone = activePerformance.getPart(performancePart);
                startVoice(allocateVoice(), zone.internalPartIndex, message.getNoteNumber(),
                           message.getVelocity(), zone.level, zone.pan);
            }
        }
        else
        {
            auto hasMatch = false;
            const auto& part = parts[static_cast<size_t>(partIndex)];
            if (!part.enabled)
                return;

            for (int i = 0; i < part.loadedElementCount; ++i)
                if (!part.loadedElements[static_cast<size_t>(i)].zones.empty()
                    && part.loadedElements[static_cast<size_t>(i)].zones.front()->matches(message.getNoteNumber(), message.getVelocity()))
                    hasMatch = true;

            if (!hasMatch)
                return;

            startVoice(allocateVoice(), partIndex, message.getNoteNumber(), message.getVelocity(), part.level, part.pan);
        }
        return;
    }

    if (message.isNoteOff())
    {
        midi2PerNotePressure.erase({ message.getChannel(), message.getNoteNumber() });
        if (arpeggiatorEnabled)
        {
            const auto note = message.getNoteNumber();
            if (performanceModeEnabled)
            {
                for (int performancePart = 0; performancePart < chimera::engine::Performance::partCount; ++performancePart)
                {
                    const auto& zone = activePerformance.getPart(performancePart);
                    if (!zone.enabled
                        || zone.midiChannel != message.getChannel()
                        || note < zone.keyLow
                        || note > zone.keyHigh)
                        continue;

                    removeHeldArpeggiatorNote(performancePart, zone.internalPartIndex, note);
                    auto& lane = arpeggiatorLanes[static_cast<size_t>(performancePart)];
                    if (lane.heldNotes.empty())
                    {
                        stopActiveArpeggiatorNotes(performancePart);
                        lane.samplesUntilStep = 0;
                        lane.samplesUntilGate = 0;
                    }
                }
            }
            else
            {
                removeHeldArpeggiatorNote(0, partIndex, note);
                auto& lane = arpeggiatorLanes[0];
                if (lane.heldNotes.empty())
                {
                    stopActiveArpeggiatorNotes(0);
                    lane.samplesUntilStep = 0;
                    lane.samplesUntilGate = 0;
                }
            }
            return;
        }

        if (performanceModeEnabled)
        {
            for (int performancePart = 0; performancePart < chimera::engine::Performance::partCount; ++performancePart)
            {
                const auto& zone = activePerformance.getPart(performancePart);
                if (!zone.enabled
                    || zone.midiChannel != message.getChannel()
                    || message.getNoteNumber() < zone.keyLow
                    || message.getNoteNumber() > zone.keyHigh)
                    continue;

                for (auto& voice : voices)
                    if (voice.active && voice.partIndex == zone.internalPartIndex && message.getNoteNumber() == voice.note)
                        releaseVoice(voice);
            }
        }
        else
        {
            for (auto& voice : voices)
                if (voice.active && voice.partIndex == partIndex && message.getNoteNumber() == voice.note)
                    releaseVoice(voice);
        }
        return;
    }

    if (message.isPitchWheel())
    {
        const auto expressionPart = mpeExpressionEnabled && message.getChannel() > 1 ? 0 : partIndex;
        pitchBendSemitones[static_cast<size_t>(expressionPart)] = (static_cast<float>(message.getPitchWheelValue()) - 8192.0f) / 8192.0f
            * static_cast<float>(parts[static_cast<size_t>(expressionPart)].pitchBendRange);
        return;
    }

    if (message.isController() && message.getControllerNumber() == 1)
    {
        const auto expressionPart = mpeExpressionEnabled && message.getChannel() > 1 ? 0 : partIndex;
        modWheelValues[static_cast<size_t>(expressionPart)] = std::clamp(static_cast<float>(message.getControllerValue()) / 127.0f, 0.0f, 1.0f);
        return;
    }

    if (message.isAftertouch())
    {
        const auto expressionPart = mpeExpressionEnabled && message.getChannel() > 1 ? 0 : partIndex;
        aftertouchValues[static_cast<size_t>(expressionPart)] = std::clamp(static_cast<float>(message.getAfterTouchValue()) / 127.0f, 0.0f, 1.0f);
        return;
    }

    if (message.isChannelPressure())
    {
        const auto expressionPart = mpeExpressionEnabled && message.getChannel() > 1 ? 0 : partIndex;
        aftertouchValues[static_cast<size_t>(expressionPart)] = std::clamp(static_cast<float>(message.getChannelPressureValue()) / 127.0f, 0.0f, 1.0f);
    }
}

void ChimeraEngineAudioProcessor::captureOutputMeters(const StereoSample& sample)
{
    outputPeakLeft = std::max(outputPeakLeft * 0.999f, std::abs(sample.left));
    outputPeakRight = std::max(outputPeakRight * 0.999f, std::abs(sample.right));
}

void ChimeraEngineAudioProcessor::recordLiveMidiMessage(const juce::MidiMessage& message)
{
    if (!liveRecordingEnabled)
        return;

    const auto tick = static_cast<int>(std::round(sequencerTick));
    if (punchRecordingEnabled && !sequencerPlaybackEnabled)
        return;

    if (message.isNoteOn())
    {
        activeRecordingNotes[{ message.getChannel(), message.getNoteNumber() }] =
            { tick, std::clamp(static_cast<int>(message.getVelocity()), 1, 127) };
        return;
    }

    if (message.isNoteOff())
    {
        const auto key = std::make_pair(message.getChannel(), message.getNoteNumber());
        const auto found = activeRecordingNotes.find(key);
        if (found == activeRecordingNotes.end())
            return;

        const auto start = found->second.startTick;
        const auto velocity = found->second.velocity;
        activeRecordingNotes.erase(found);
        sequencer.song(0).recordNote(currentSequencerTrack,
                                     start,
                                     std::max(1, tick - start),
                                     message.getNoteNumber(),
                                     velocity,
                                     message.getChannel());
        sequencerDemoSeeded = true;
    }
}

void ChimeraEngineAudioProcessor::releaseVoice(ActiveVoice& voice)
{
    if (voice.released)
        return;

    voice.released = true;
    for (int i = 0; i < voice.elementCount; ++i)
    {
        if (!voice.releasePlayers[static_cast<size_t>(i)].isPlaying())
            voice.releasePlayers[static_cast<size_t>(i)].start(voice.note, currentSampleRate);
        voice.ampEnvelopes[static_cast<size_t>(i)].noteOff();
        voice.pitchEnvelopes[static_cast<size_t>(i)].noteOff();
        voice.filterEnvelopes[static_cast<size_t>(i)].noteOff();
    }
}

ChimeraEngineAudioProcessor::ActiveVoice& ChimeraEngineAudioProcessor::allocateVoice()
{
    if (auto inactive = std::find_if(voices.begin(), voices.end(), [](const auto& voice) { return !voice.active; });
        inactive != voices.end())
        return *inactive;

    return *std::min_element(voices.begin(), voices.end(), [](const auto& a, const auto& b)
    {
        return a.age < b.age;
    });
}

void ChimeraEngineAudioProcessor::stopVoicesForNote(int partIndex, int note)
{
    for (auto& voice : voices)
        if (voice.active && voice.partIndex == partIndex && (note < 0 || voice.note == note))
            releaseVoice(voice);
}

void ChimeraEngineAudioProcessor::startVoice(ActiveVoice& target, int partIndex, int note, int velocity, float level, float pan)
{
    auto& part = parts[static_cast<size_t>(std::clamp(partIndex, 0, static_cast<int>(maxParts) - 1))];
    if (!part.enabled)
        return;

    if (part.voiceMode == "mono" || part.voiceMode == "legato")
    {
        const auto priorNote = lastMonoNotes[static_cast<size_t>(partIndex)];
        const auto legato = part.voiceMode == "legato" && priorNote >= 0;
        for (auto& voice : voices)
        {
            if (!voice.active || voice.partIndex != partIndex)
                continue;

            if (legato)
            {
                voice.targetNote = note;
                voice.targetPitchCents = static_cast<float>(note - voice.note) * 100.0f;
                const auto portamentoSamples = std::max(1.0f, part.portamentoTime * static_cast<float>(currentSampleRate));
                voice.portamentoStepCents = (voice.targetPitchCents - voice.currentPitchCents) / portamentoSamples;
                lastMonoNotes[static_cast<size_t>(partIndex)] = note;
                return;
            }

            releaseVoice(voice);
        }
    }

    target.partIndex = partIndex;
    target.note = note;
    target.targetNote = note;
    target.age = ++voiceAgeCounter;
    target.velocityGain = std::clamp(static_cast<float>(velocity) / 127.0f, 0.0f, 1.0f);
    target.partLevel = std::clamp(level, 0.0f, 2.0f);
    target.partPan = std::clamp(pan, -1.0f, 1.0f);
    target.currentPitchCents = 0.0f;
    target.targetPitchCents = 0.0f;
    target.portamentoStepCents = 0.0f;
    target.released = false;
    for (auto& envelope : target.ampEnvelopes)
    {
        envelope.setSampleRate(currentSampleRate);
        envelope.noteOff();
    }
    for (auto& envelope : target.pitchEnvelopes)
    {
        envelope.setSampleRate(currentSampleRate);
        envelope.noteOff();
    }
    for (auto& envelope : target.filterEnvelopes)
    {
        envelope.setSampleRate(currentSampleRate);
        envelope.noteOff();
    }
    for (auto& filter : target.filters)
    {
        filter.setSampleRate(currentSampleRate);
        filter.reset();
    }
    for (auto& lfo : target.lfo1)
        lfo.setSampleRate(currentSampleRate);
    for (auto& lfo : target.lfo2)
        lfo.setSampleRate(currentSampleRate);
    for (auto& player : target.releasePlayers)
        player.stop();
    target.elementCount = 0;

    for (int i = 0; i < part.loadedElementCount; ++i)
    {
        auto& element = part.loadedElements[static_cast<size_t>(i)];
        if (element.zones.empty() || !element.zones.front()->matches(note, velocity))
            continue;

        const auto playerIndex = static_cast<size_t>(target.elementCount);
        auto zoneIndex = 0;
        if (element.zones.size() > 1 && element.alternateMode == "roundrobin")
            zoneIndex = element.roundRobinCounter++ % static_cast<int>(element.zones.size());
        else if (element.zones.size() > 1 && element.alternateMode == "random")
            zoneIndex = static_cast<int>((voiceAgeCounter + static_cast<uint64_t>(i * 1103515245)) % element.zones.size());

        target.players[playerIndex].setZone(element.zones[static_cast<size_t>(zoneIndex)]);
        target.players[playerIndex].start(target.note, currentSampleRate);
        if (!element.releaseZones.empty())
            target.releasePlayers[playerIndex].setZone(element.releaseZones.front());
        target.elementLevels[playerIndex] = element.level;
        target.elementPans[playerIndex] = element.pan;
        target.elementFilterModes[playerIndex] = element.filterMode;
        target.pitchEnvelopeDepths[playerIndex] = element.pitchDepthCents;
        target.filterEnvelopeDepths[playerIndex] = element.filterDepth;
        target.lfo1CutoffDepths[playerIndex] = element.lfo1CutoffDepth;
        target.lfo2AmpDepths[playerIndex] = element.lfo2AmpDepth;
        target.lfo2PanDepths[playerIndex] = element.lfo2PanDepth;
        target.modSlotCounts[playerIndex] = element.modSlotCount;
        target.modSlots[playerIndex] = element.modSlots;
        target.lfo1[playerIndex].setFrequency(element.lfo1RateHz);
        target.lfo1[playerIndex].reset();
        target.lfo2[playerIndex].setFrequency(element.lfo2RateHz);
        target.lfo2[playerIndex].reset();
        target.filters[playerIndex].setMode(element.filterMode);
        target.ampEnvelopes[playerIndex].setStages(*parameters.getRawParameterValue("attack") + element.ampAttack,
                                                   element.ampDecay1,
                                                   element.ampDecay2,
                                                   element.ampSustain,
                                                   *parameters.getRawParameterValue("release") + element.ampRelease,
                                                   chimera::dsp::Curve::Linear);
        target.ampEnvelopes[playerIndex].noteOn();
        target.pitchEnvelopes[playerIndex].setStages(element.pitchAttack, element.pitchDecay1, element.pitchDecay2,
                                                     element.pitchSustain, element.pitchRelease,
                                                     chimera::dsp::Curve::Linear);
        target.pitchEnvelopes[playerIndex].noteOn();
        target.filterEnvelopes[playerIndex].setStages(element.filterAttack, element.filterDecay1, element.filterDecay2,
                                                      element.filterSustain, element.filterRelease,
                                                      chimera::dsp::Curve::Linear);
        target.filterEnvelopes[playerIndex].noteOn();
        ++target.elementCount;

        if (target.elementCount >= static_cast<int>(maxElements))
            break;
    }

    target.active = true;
    lastMonoNotes[static_cast<size_t>(partIndex)] = note;
}

void ChimeraEngineAudioProcessor::addHeldArpeggiatorNote(int laneIndex, int partIndex, int note, int velocity)
{
    if (laneIndex < 0 || laneIndex >= chimera::engine::Performance::partCount)
        return;

    auto& lane = arpeggiatorLanes[static_cast<size_t>(laneIndex)];
    if (auto existing = std::find_if(lane.heldNotes.begin(), lane.heldNotes.end(),
                                     [partIndex, note](const auto& held)
                                     {
                                         return held.partIndex == partIndex && held.note == note;
                                     });
        existing != lane.heldNotes.end())
        existing->velocity = velocity;
    else
        lane.heldNotes.push_back({ partIndex, note, velocity });

    refreshArpeggiatorHeldNotes(laneIndex);
}

void ChimeraEngineAudioProcessor::removeHeldArpeggiatorNote(int laneIndex, int partIndex, int note)
{
    if (laneIndex < 0 || laneIndex >= chimera::engine::Performance::partCount)
        return;

    auto& lane = arpeggiatorLanes[static_cast<size_t>(laneIndex)];
    lane.heldNotes.erase(std::remove_if(lane.heldNotes.begin(), lane.heldNotes.end(),
                                        [partIndex, note](const auto& held)
                                        {
                                            return held.partIndex == partIndex && held.note == note;
                                        }),
                         lane.heldNotes.end());
    refreshArpeggiatorHeldNotes(laneIndex);
}

void ChimeraEngineAudioProcessor::advanceArpeggiators()
{
    for (int lane = 0; lane < chimera::engine::Performance::partCount; ++lane)
        advanceArpeggiatorLane(lane);
}

void ChimeraEngineAudioProcessor::advanceArpeggiatorLane(int laneIndex)
{
    if (laneIndex < 0 || laneIndex >= chimera::engine::Performance::partCount)
        return;

    auto& lane = arpeggiatorLanes[static_cast<size_t>(laneIndex)];
    if (!lane.enabled || lane.heldNotes.empty())
        return;

    if (lane.samplesUntilGate > 0)
    {
        --lane.samplesUntilGate;
        if (lane.samplesUntilGate == 0)
            stopActiveArpeggiatorNotes(laneIndex);
    }

    if (lane.samplesUntilStep > 0)
    {
        --lane.samplesUntilStep;
        return;
    }

    stopActiveArpeggiatorNotes(laneIndex);

    const auto nextNotes = lane.engine.tick();
    for (const auto note : nextNotes)
    {
        const auto velocity = [&lane, note]
        {
            if (auto held = std::find_if(lane.heldNotes.begin(), lane.heldNotes.end(),
                                         [note](const auto& candidate)
                                         {
                                             return candidate.note == note;
                                         });
                held != lane.heldNotes.end())
                return held->velocity;

            return 100;
        }();

        startVoice(allocateVoice(), lane.internalPartIndex, note, velocity);
        lane.activeNotes.push_back({ laneIndex, lane.internalPartIndex, note });
    }

    const auto stepSamples = arpeggiatorStepSamples(currentSampleRate);
    lane.samplesUntilStep = stepSamples;
    lane.samplesUntilGate = std::max(1, static_cast<int>(std::round(stepSamples * *parameters.getRawParameterValue("arpGate"))));
}

void ChimeraEngineAudioProcessor::refreshArpeggiatorHeldNotes(int laneIndex)
{
    if (laneIndex < 0 || laneIndex >= chimera::engine::Performance::partCount)
        return;

    auto& lane = arpeggiatorLanes[static_cast<size_t>(laneIndex)];
    std::vector<int> notes;
    notes.reserve(lane.heldNotes.size());
    for (const auto& held : lane.heldNotes)
        notes.push_back(held.note);

    lane.engine.setHeldNotes(std::move(notes));
}

void ChimeraEngineAudioProcessor::stopActiveArpeggiatorNotes(int laneIndex)
{
    if (laneIndex < 0 || laneIndex >= chimera::engine::Performance::partCount)
        return;

    auto& lane = arpeggiatorLanes[static_cast<size_t>(laneIndex)];
    for (const auto activeNote : lane.activeNotes)
        for (auto& voice : voices)
            if (voice.active && voice.partIndex == activeNote.partIndex && voice.note == activeNote.note)
                releaseVoice(voice);

    lane.activeNotes.clear();
}

void ChimeraEngineAudioProcessor::stopAllActiveArpeggiatorNotes()
{
    for (int lane = 0; lane < chimera::engine::Performance::partCount; ++lane)
        stopActiveArpeggiatorNotes(lane);
}

void ChimeraEngineAudioProcessor::applyFxConfiguration(bool resetFx)
{
    for (auto& fx : workstationFx)
    {
        for (int slot = 0; slot < chimera::fx::InsertRack::slotCount; ++slot)
            fx.inserts().setSlot(slot, insertEffects[static_cast<size_t>(slot)]);

        fx.system().setChorusSend(chorusSend);
        fx.system().setReverbSend(reverbSend);
        applyMasterFxConfiguration();

        if (resetFx)
            fx.reset();
    }
}

void ChimeraEngineAudioProcessor::applyMasterFxConfiguration()
{
    const auto low = parameters.getRawParameterValue("masterEqLow")->load();
    const auto mid = parameters.getRawParameterValue("masterEqMid")->load();
    const auto high = parameters.getRawParameterValue("masterEqHigh")->load();
    const auto threshold = parameters.getRawParameterValue("masterCompThreshold")->load();
    const auto ratio = parameters.getRawParameterValue("masterCompRatio")->load();
    const auto makeup = parameters.getRawParameterValue("masterCompMakeup")->load();

    for (auto& fx : workstationFx)
    {
        fx.master().setMasterEqDb(low, mid, high);
        fx.master().setCompressor(threshold, ratio, 8.0f, 120.0f, makeup);
    }
}

void ChimeraEngineAudioProcessor::pushUndoAction(const juce::String& description)
{
    if (description.isEmpty())
        return;

    lastEditDescription = description;
    undoStack.push_back(description);
    redoStack.clear();
    if (undoStack.size() > 128)
        undoStack.erase(undoStack.begin());
}

void ChimeraEngineAudioProcessor::setFloatParameterValue(const juce::String& parameterId, float value)
{
    if (auto* parameter = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter(parameterId)))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

int ChimeraEngineAudioProcessor::sequencerLoopEndTick() const
{
    auto endTick = chimera::engine::Song::ppq * 8;
    const auto& song = sequencer.song(0);
    for (int track = 0; track < chimera::engine::Song::trackCount; ++track)
        for (const auto& note : song.track(track).getNotes())
            endTick = std::max(endTick, note.tick + note.durationTicks);

    return endTick;
}

void ChimeraEngineAudioProcessor::addSequencerEventsForBlock(juce::MidiBuffer& midi, int numSamples)
{
    if (!sequencerPlaybackEnabled || numSamples <= 0)
        return;

    if (!sequencerDemoSeeded)
        seedDemoSequence();

    const auto& song = sequencer.song(0);
    const auto ticksPerSample = song.getTempo() * static_cast<double>(chimera::engine::Song::ppq)
        / (60.0 * std::max(1.0, currentSampleRate));
    const auto loopEnd = sequencerLoopEndTick();
    const auto startTick = static_cast<int>(std::floor(sequencerTick));
    const auto endTick = static_cast<int>(std::ceil(sequencerTick + ticksPerSample * static_cast<double>(numSamples)));

    auto addEventsInRange = [&midi, &song, numSamples, ticksPerSample](int rangeStart, int rangeEnd, int tickOffset)
    {
        const auto events = song.collectPlaybackEvents(rangeStart, rangeEnd);
        for (const auto& event : events)
        {
            const auto relativeTick = static_cast<double>(event.tick - rangeStart + tickOffset);
            const auto sample = std::clamp(static_cast<int>(std::round(relativeTick / ticksPerSample)), 0, std::max(0, numSamples - 1));
            const auto channel = std::clamp(event.note.channel, 1, 16);
            const auto message = event.noteOn
                ? juce::MidiMessage::noteOn(channel, event.note.note, juce::uint8(event.note.velocity))
                : juce::MidiMessage::noteOff(channel, event.note.note);
            midi.addEvent(message, sample);
        }
    };

    if (endTick < loopEnd)
    {
        applySequencerScenesForRange(startTick, endTick);
        addEventsInRange(startTick, endTick, 0);
        sequencerTick += ticksPerSample * static_cast<double>(numSamples);
        return;
    }

    applySequencerScenesForRange(startTick, loopEnd);
    applySequencerScenesForRange(0, endTick % loopEnd);
    addEventsInRange(startTick, loopEnd, 0);
    const auto wrappedEnd = endTick % loopEnd;
    addEventsInRange(0, wrappedEnd, loopEnd - startTick);
    sequencerTick = std::fmod(sequencerTick + ticksPerSample * static_cast<double>(numSamples),
                              static_cast<double>(loopEnd));
}

void ChimeraEngineAudioProcessor::applySequencerScenesForRange(int startTick, int endTick)
{
    if (endTick <= startTick)
        return;

    const auto& scenes = sequencer.song(0).getSceneEvents();
    for (const auto& scene : scenes)
        if (scene.tick >= startTick && scene.tick < endTick && scene.scene != currentPerformanceScene)
            applyPerformanceScene(scene.scene);
}

ChimeraEngineAudioProcessor::StereoSample ChimeraEngineAudioProcessor::renderVoiceSample()
{
    const auto gain = dbToGain(*parameters.getRawParameterValue("masterGain"));
    StereoSample output;

    for (auto& voice : voices)
    {
        if (!voice.active)
            continue;

        auto anyPlaying = false;
        auto anyEnvelopeActive = false;
        const auto partIndex = static_cast<size_t>(std::clamp(voice.partIndex, 0, static_cast<int>(maxParts) - 1));
        if (std::abs(voice.currentPitchCents - voice.targetPitchCents) > std::abs(voice.portamentoStepCents))
            voice.currentPitchCents += voice.portamentoStepCents;
        else
            voice.currentPitchCents = voice.targetPitchCents;

        for (int i = 0; i < voice.elementCount; ++i)
        {
            auto& envelope = voice.ampEnvelopes[static_cast<size_t>(i)];
            const auto amp = envelope.process();
            anyEnvelopeActive = anyEnvelopeActive || envelope.isActive();
            const auto pitchEg = voice.pitchEnvelopes[static_cast<size_t>(i)].process();
            const auto filterEg = voice.filterEnvelopes[static_cast<size_t>(i)].process();
            anyEnvelopeActive = anyEnvelopeActive || voice.pitchEnvelopes[static_cast<size_t>(i)].isActive()
                || voice.filterEnvelopes[static_cast<size_t>(i)].isActive();

            auto& filter = voice.filters[static_cast<size_t>(i)];
            filter.setMode(voice.elementFilterModes[static_cast<size_t>(i)]);
            const auto lfo1 = voice.lfo1[static_cast<size_t>(i)].process();
            const auto lfo2 = voice.lfo2[static_cast<size_t>(i)].process();
            auto modPitchCents = 0.0f;
            auto modCutoff = 0.0f;
            auto modAmp = 0.0f;
            auto modPan = 0.0f;
            auto perNoteExpression = 0.0f;
            if (const auto found = midi2PerNotePressure.find({ parts[partIndex].voiceMode == "poly" ? voice.partIndex + 1 : 1, voice.note });
                found != midi2PerNotePressure.end())
                perNoteExpression = found->second;
            const auto expressive = std::max({ modWheelValues[partIndex], aftertouchValues[partIndex], perNoteExpression });
            for (int slot = 0; slot < voice.modSlotCounts[static_cast<size_t>(i)]; ++slot)
            {
                const auto& modSlot = voice.modSlots[static_cast<size_t>(i)][static_cast<size_t>(slot)];
                if (!modSlot.enabled)
                    continue;

                const auto source = modSlotSourceValue(modSlot.source, voice.velocityGain, pitchEg, filterEg, amp,
                                                       lfo1, lfo2, expressive);
                switch (modSlot.destination)
                {
                    case chimera::dsp::ModDestination::Pitch: modPitchCents += source * modSlot.depth * 100.0f; break;
                    case chimera::dsp::ModDestination::Cutoff: modCutoff += source * modSlot.depth; break;
                    case chimera::dsp::ModDestination::Amp: modAmp += source * modSlot.depth; break;
                    case chimera::dsp::ModDestination::Pan: modPan += source * modSlot.depth; break;
                }
            }

            const auto cutoffMod = std::pow(2.0f, (lfo1 * voice.lfo1CutoffDepths[static_cast<size_t>(i)] * 2.0f)
                                                  + filterEg * voice.filterEnvelopeDepths[static_cast<size_t>(i)]
                                                  + modCutoff);
            filter.setCutoff(std::clamp(*parameters.getRawParameterValue("cutoff") * cutoffMod, 20.0f, 20000.0f));
            filter.setResonance(*parameters.getRawParameterValue("resonance"));

            auto& player = voice.players[static_cast<size_t>(i)];
            const auto pitchCents = pitchBendSemitones[partIndex] * 100.0f
                + voice.currentPitchCents
                + pitchEg * voice.pitchEnvelopeDepths[static_cast<size_t>(i)]
                + modPitchCents;
            const auto pitchRatio = std::pow(2.0f, pitchCents / 1200.0f);
            const auto releaseSample = voice.releasePlayers[static_cast<size_t>(i)].process(pitchRatio);
            const auto filtered = filter.process(player.process(pitchRatio) + releaseSample);
            const auto lfoAmp = std::clamp(1.0f + lfo2 * voice.lfo2AmpDepths[static_cast<size_t>(i)] + modAmp, 0.0f, 2.0f);
            const auto lfoPan = lfo2 * voice.lfo2PanDepths[static_cast<size_t>(i)];
            const auto [leftPan, rightPan] = panGains(voice.elementPans[static_cast<size_t>(i)] + voice.partPan + lfoPan + modPan);
            const auto element = filtered
                * voice.elementLevels[static_cast<size_t>(i)]
                * voice.partLevel
                * amp
                * lfoAmp
                * voice.velocityGain
                * gain;

            output.left += element * leftPan;
            output.right += element * rightPan;
            anyPlaying = anyPlaying || player.isPlaying() || voice.releasePlayers[static_cast<size_t>(i)].isPlaying();
        }

        if (!anyPlaying || !anyEnvelopeActive)
        {
            voice.active = false;
            voice.note = -1;
            voice.elementCount = 0;
        }
    }

    const auto fxMix = std::clamp(parameters.getRawParameterValue("fxMix")->load(), 0.0f, 1.0f);
    if (fxMix > 0.0f)
    {
        const auto dryLeft = output.left;
        const auto dryRight = output.right;
        const auto wetLeft = workstationFx[0].process(dryLeft);
        const auto wetRight = workstationFx[1].process(dryRight);
        output.left = dryLeft * (1.0f - fxMix) + wetLeft * fxMix;
        output.right = dryRight * (1.0f - fxMix) + wetRight * fxMix;
    }

    output.left = std::clamp(output.left, -1.0f, 1.0f);
    output.right = std::clamp(output.right, -1.0f, 1.0f);
    return output;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChimeraEngineAudioProcessor();
}
