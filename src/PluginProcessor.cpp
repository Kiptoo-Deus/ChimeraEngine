#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "preset/Preset.h"
#include <algorithm>
#include <cmath>
#include <limits>

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

    if (message.isNoteOn())
    {
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
        fx.master().setMasterEqDb(0.0f, 0.0f, 0.0f);
        fx.master().setCompressor(-12.0f, 2.5f, 8.0f, 120.0f, 0.0f);

        if (resetFx)
            fx.reset();
    }
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
            const auto expressive = std::max(modWheelValues[partIndex], aftertouchValues[partIndex]);
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
