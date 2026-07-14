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
}

ChimeraEngineAudioProcessor::ChimeraEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
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
    arpeggiatorSamplesUntilStep = 0;
    arpeggiatorSamplesUntilGate = 0;
    arpeggiatorWasEnabled = false;
    heldArpeggiatorNotes.clear();
    activeArpeggiatorNotes.clear();
    arpeggiator.setMode(chimera::engine::ArpMode::Up);

    for (auto& voice : voices)
    {
        for (auto& envelope : voice.ampEnvelopes)
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
        voice.active = false;
        voice.note = -1;
        voice.age = 0;
        voice.elementCount = 0;
        voice.partIndex = 0;
        voice.partLevel = 1.0f;
        voice.partPan = 0.0f;
    }

    for (auto& fx : workstationFx)
    {
        fx.prepare(currentSampleRate);
        fx.reset();
        fx.inserts().setSlot(0, chimera::fx::EffectType::Compressor);
        fx.system().setChorusSend(0.18f);
        fx.system().setReverbSend(0.16f);
        fx.master().setMasterEqDb(0.0f, 0.0f, 0.0f);
        fx.master().setCompressor(-12.0f, 2.5f, 8.0f, 120.0f, 0.0f);
    }

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

    auto midiIterator = mergedMidi.cbegin();
    const auto midiEnd = mergedMidi.cend();

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto arpeggiatorEnabled = *parameters.getRawParameterValue("arpEnabled") > 0.5f;
        if (!arpeggiatorEnabled && arpeggiatorWasEnabled)
        {
            stopActiveArpeggiatorNotes();
            heldArpeggiatorNotes.clear();
            refreshArpeggiatorHeldNotes();
            arpeggiatorSamplesUntilStep = 0;
            arpeggiatorSamplesUntilGate = 0;
        }
        arpeggiatorWasEnabled = arpeggiatorEnabled;

        while (midiIterator != midiEnd && (*midiIterator).samplePosition <= sample)
        {
            handleMidiMessage((*midiIterator).getMessage());
            ++midiIterator;
        }

        if (arpeggiatorEnabled)
            advanceArpeggiator();

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

void ChimeraEngineAudioProcessor::setPerformanceModeEnabled(bool shouldBeEnabled)
{
    performanceModeEnabled = shouldBeEnabled;
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

        auto zone = std::make_shared<chimera::dsp::SampleZone>();
        zone->setSource(projectRoot().getChildFile(element.samplePath));
        zone->setRootKey(element.rootKey);
        zone->setKeyRange(element.keyLow, element.keyHigh);
        zone->setVelocityRange(element.velocityLow, element.velocityHigh);
        zone->setTuningCents(element.tuningCents);

        if (const auto result = zone->loadAudio(); result.failed())
            return result;

        elements[static_cast<size_t>(count)] = { std::move(zone), element.level, element.pan, filterModeFromString(element.filterType) };
        elements[static_cast<size_t>(count)].ampAttack = element.ampAttack;
        elements[static_cast<size_t>(count)].ampSustain = element.ampSustain;
        elements[static_cast<size_t>(count)].ampRelease = element.ampRelease;
        elements[static_cast<size_t>(count)].lfo1RateHz = element.lfo1RateHz;
        elements[static_cast<size_t>(count)].lfo1CutoffDepth = element.lfo1CutoffDepth;
        elements[static_cast<size_t>(count)].lfo2RateHz = element.lfo2RateHz;
        elements[static_cast<size_t>(count)].lfo2AmpDepth = element.lfo2AmpDepth;
        elements[static_cast<size_t>(count)].lfo2PanDepth = element.lfo2PanDepth;
        ++count;
    }

    if (count <= 0)
        return juce::Result::fail("Patch contains no loadable elements");

    setActiveElementsForPart(partIndex, std::move(elements), count, patch.metadata.name);
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
    const auto partIndex = std::clamp(message.getChannel() - 1, 0, static_cast<int>(maxParts) - 1);

    if (message.isNoteOn())
    {
        if (arpeggiatorEnabled)
        {
            const auto note = message.getNoteNumber();
            const auto velocity = std::clamp(static_cast<int>(message.getVelocity()), 1, 127);
            if (auto existing = std::find_if(heldArpeggiatorNotes.begin(), heldArpeggiatorNotes.end(),
                                             [partIndex, note](const auto& held)
                                             {
                                                 return held.partIndex == partIndex && held.note == note;
                                             });
                existing != heldArpeggiatorNotes.end())
                existing->velocity = velocity;
            else
                heldArpeggiatorNotes.push_back({ partIndex, note, velocity });

            arpeggiatorPartIndex = partIndex;
            refreshArpeggiatorHeldNotes();
            if (activeArpeggiatorNotes.empty())
                arpeggiatorSamplesUntilStep = 0;
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
                if (part.loadedElements[static_cast<size_t>(i)].zone != nullptr
                    && part.loadedElements[static_cast<size_t>(i)].zone->matches(message.getNoteNumber(), message.getVelocity()))
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
            heldArpeggiatorNotes.erase(std::remove_if(heldArpeggiatorNotes.begin(), heldArpeggiatorNotes.end(),
                                                      [partIndex, note](const auto& held)
                                                      {
                                                          return held.partIndex == partIndex && held.note == note;
                                                      }),
                                       heldArpeggiatorNotes.end());
            refreshArpeggiatorHeldNotes();
            if (heldArpeggiatorNotes.empty())
            {
                stopActiveArpeggiatorNotes();
                arpeggiatorSamplesUntilStep = 0;
                arpeggiatorSamplesUntilGate = 0;
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
                        for (auto& envelope : voice.ampEnvelopes)
                            envelope.noteOff();
            }
        }
        else
        {
            for (auto& voice : voices)
                if (voice.active && voice.partIndex == partIndex && message.getNoteNumber() == voice.note)
                    for (auto& envelope : voice.ampEnvelopes)
                        envelope.noteOff();
        }
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

void ChimeraEngineAudioProcessor::startVoice(ActiveVoice& target, int partIndex, int note, int velocity, float level, float pan)
{
    const auto& part = parts[static_cast<size_t>(std::clamp(partIndex, 0, static_cast<int>(maxParts) - 1))];
    if (!part.enabled)
        return;

    target.partIndex = partIndex;
    target.note = note;
    target.age = ++voiceAgeCounter;
    target.velocityGain = std::clamp(static_cast<float>(velocity) / 127.0f, 0.0f, 1.0f);
    target.partLevel = std::clamp(level, 0.0f, 2.0f);
    target.partPan = std::clamp(pan, -1.0f, 1.0f);
    for (auto& envelope : target.ampEnvelopes)
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
    target.elementCount = 0;

    for (int i = 0; i < part.loadedElementCount; ++i)
    {
        const auto& element = part.loadedElements[static_cast<size_t>(i)];
        if (element.zone == nullptr || !element.zone->matches(note, velocity))
            continue;

        const auto playerIndex = static_cast<size_t>(target.elementCount);
        target.players[playerIndex].setZone(element.zone);
        target.players[playerIndex].start(target.note, currentSampleRate);
        target.elementLevels[playerIndex] = element.level;
        target.elementPans[playerIndex] = element.pan;
        target.elementFilterModes[playerIndex] = element.filterMode;
        target.lfo1CutoffDepths[playerIndex] = element.lfo1CutoffDepth;
        target.lfo2AmpDepths[playerIndex] = element.lfo2AmpDepth;
        target.lfo2PanDepths[playerIndex] = element.lfo2PanDepth;
        target.lfo1[playerIndex].setFrequency(element.lfo1RateHz);
        target.lfo1[playerIndex].reset();
        target.lfo2[playerIndex].setFrequency(element.lfo2RateHz);
        target.lfo2[playerIndex].reset();
        target.filters[playerIndex].setMode(element.filterMode);
        target.ampEnvelopes[playerIndex].setStages(*parameters.getRawParameterValue("attack") + element.ampAttack,
                                                   0.05f,
                                                   0.05f,
                                                   element.ampSustain,
                                                   *parameters.getRawParameterValue("release") + element.ampRelease,
                                                   chimera::dsp::Curve::Linear);
        target.ampEnvelopes[playerIndex].noteOn();
        ++target.elementCount;

        if (target.elementCount >= static_cast<int>(maxElements))
            break;
    }

    target.active = true;
}

void ChimeraEngineAudioProcessor::advanceArpeggiator()
{
    if (heldArpeggiatorNotes.empty())
        return;

    if (arpeggiatorSamplesUntilGate > 0)
    {
        --arpeggiatorSamplesUntilGate;
        if (arpeggiatorSamplesUntilGate == 0)
            stopActiveArpeggiatorNotes();
    }

    if (arpeggiatorSamplesUntilStep > 0)
    {
        --arpeggiatorSamplesUntilStep;
        return;
    }

    stopActiveArpeggiatorNotes();

    const auto nextNotes = arpeggiator.tick();
    for (const auto note : nextNotes)
    {
        const auto velocity = [this, note]
        {
            if (auto held = std::find_if(heldArpeggiatorNotes.begin(), heldArpeggiatorNotes.end(),
                                         [this, note](const auto& candidate)
                                         {
                                             return candidate.partIndex == arpeggiatorPartIndex && candidate.note == note;
                                         });
                held != heldArpeggiatorNotes.end())
                return held->velocity;

            return 100;
        }();

        startVoice(allocateVoice(), arpeggiatorPartIndex, note, velocity);
        activeArpeggiatorNotes.push_back({ arpeggiatorPartIndex, note });
    }

    const auto stepSamples = arpeggiatorStepSamples(currentSampleRate);
    arpeggiatorSamplesUntilStep = stepSamples;
    arpeggiatorSamplesUntilGate = std::max(1, static_cast<int>(std::round(stepSamples * *parameters.getRawParameterValue("arpGate"))));
}

void ChimeraEngineAudioProcessor::refreshArpeggiatorHeldNotes()
{
    std::vector<int> notes;
    notes.reserve(heldArpeggiatorNotes.size());
    for (const auto& held : heldArpeggiatorNotes)
        if (held.partIndex == arpeggiatorPartIndex)
            notes.push_back(held.note);

    arpeggiator.setHeldNotes(std::move(notes));
}

void ChimeraEngineAudioProcessor::stopActiveArpeggiatorNotes()
{
    for (const auto activeNote : activeArpeggiatorNotes)
        for (auto& voice : voices)
            if (voice.active && voice.partIndex == activeNote.partIndex && voice.note == activeNote.note)
                for (auto& envelope : voice.ampEnvelopes)
                    envelope.noteOff();

    activeArpeggiatorNotes.clear();
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
        for (int i = 0; i < voice.elementCount; ++i)
        {
            auto& envelope = voice.ampEnvelopes[static_cast<size_t>(i)];
            const auto amp = envelope.process();
            anyEnvelopeActive = anyEnvelopeActive || envelope.isActive();

            auto& filter = voice.filters[static_cast<size_t>(i)];
            filter.setMode(voice.elementFilterModes[static_cast<size_t>(i)]);
            const auto lfo1 = voice.lfo1[static_cast<size_t>(i)].process();
            const auto lfo2 = voice.lfo2[static_cast<size_t>(i)].process();
            const auto cutoffMod = std::pow(2.0f, lfo1 * voice.lfo1CutoffDepths[static_cast<size_t>(i)] * 2.0f);
            filter.setCutoff(std::clamp(*parameters.getRawParameterValue("cutoff") * cutoffMod, 20.0f, 20000.0f));
            filter.setResonance(*parameters.getRawParameterValue("resonance"));

            auto& player = voice.players[static_cast<size_t>(i)];
            const auto filtered = filter.process(player.process());
            const auto lfoAmp = std::clamp(1.0f + lfo2 * voice.lfo2AmpDepths[static_cast<size_t>(i)], 0.0f, 2.0f);
            const auto lfoPan = lfo2 * voice.lfo2PanDepths[static_cast<size_t>(i)];
            const auto [leftPan, rightPan] = panGains(voice.elementPans[static_cast<size_t>(i)] + voice.partPan + lfoPan);
            const auto element = filtered
                * voice.elementLevels[static_cast<size_t>(i)]
                * voice.partLevel
                * amp
                * lfoAmp
                * voice.velocityGain
                * gain;

            output.left += element * leftPan;
            output.right += element * rightPan;
            anyPlaying = anyPlaying || player.isPlaying();
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
