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

    for (auto& voice : voices)
    {
        for (auto& envelope : voice.ampEnvelopes)
            envelope.setSampleRate(currentSampleRate);
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
    }

    ignoreUnused(loadDefaultPatch());
}

void ChimeraEngineAudioProcessor::releaseResources()
{
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
        while (midiIterator != midiEnd && (*midiIterator).samplePosition <= sample)
        {
            handleMidiMessage((*midiIterator).getMessage());
            ++midiIterator;
        }

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
    juce::MemoryOutputStream stream(destData, true);
    parameters.state.writeToStream(stream);
}

void ChimeraEngineAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes)); tree.isValid())
        parameters.replaceState(tree);
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
    return loadPatchFile(root.getChildFile("presets/Synth/Sine.chpatch"));
}

juce::Result ChimeraEngineAudioProcessor::loadSynthPreset(const juce::String& presetName)
{
    if (!presetName.containsOnly("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 _-"))
        return juce::Result::fail("Preset name contains unsupported characters");

    return loadPatchFile(projectRoot().getChildFile("presets/Synth").getChildFile(presetName + ".chpatch"));
}

juce::Result ChimeraEngineAudioProcessor::loadPatchFile(const juce::File& patchFile)
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

        elements[static_cast<size_t>(count)] = { std::move(zone), element.level, element.pan };
        elements[static_cast<size_t>(count)].ampAttack = element.ampAttack;
        elements[static_cast<size_t>(count)].ampSustain = element.ampSustain;
        elements[static_cast<size_t>(count)].ampRelease = element.ampRelease;
        ++count;
    }

    if (count <= 0)
        return juce::Result::fail("Patch contains no loadable elements");

    setActiveElements(std::move(elements), count, patch.metadata.name);
    return juce::Result::ok();
}

void ChimeraEngineAudioProcessor::setActiveElements(std::array<LoadedElement, maxElements> elements, int count,
                                                    const juce::String& patchName)
{
    const juce::ScopedLock lock(zoneLock);
    loadedElements = std::move(elements);
    loadedElementCount = std::clamp(count, 0, static_cast<int>(maxElements));
    currentPatchName = patchName;

    for (auto& voice : voices)
    {
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
    if (message.isNoteOn())
    {
        const juce::ScopedLock lock(zoneLock);
        auto hasMatch = false;
        for (int i = 0; i < loadedElementCount; ++i)
            if (loadedElements[static_cast<size_t>(i)].zone != nullptr
                && loadedElements[static_cast<size_t>(i)].zone->matches(message.getNoteNumber(), message.getVelocity()))
                hasMatch = true;

        if (!hasMatch)
            return;

        startVoice(allocateVoice(), message.getNoteNumber(), message.getVelocity());
        return;
    }

    if (message.isNoteOff())
        for (auto& voice : voices)
            if (voice.active && message.getNoteNumber() == voice.note)
                for (auto& envelope : voice.ampEnvelopes)
                    envelope.noteOff();
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

void ChimeraEngineAudioProcessor::startVoice(ActiveVoice& target, int note, int velocity)
{
    target.note = note;
    target.age = ++voiceAgeCounter;
    target.velocityGain = std::clamp(static_cast<float>(velocity) / 127.0f, 0.0f, 1.0f);
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
    target.elementCount = 0;

    for (int i = 0; i < loadedElementCount; ++i)
    {
        const auto& element = loadedElements[static_cast<size_t>(i)];
        if (element.zone == nullptr || !element.zone->matches(note, velocity))
            continue;

        const auto playerIndex = static_cast<size_t>(target.elementCount);
        target.players[playerIndex].setZone(element.zone);
        target.players[playerIndex].start(target.note, currentSampleRate);
        target.elementLevels[playerIndex] = element.level;
        target.elementPans[playerIndex] = element.pan;
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
            filter.setCutoff(*parameters.getRawParameterValue("cutoff"));
            filter.setResonance(*parameters.getRawParameterValue("resonance"));

            auto& player = voice.players[static_cast<size_t>(i)];
            const auto filtered = filter.process(player.process());
            const auto [leftPan, rightPan] = panGains(voice.elementPans[static_cast<size_t>(i)]);
            const auto element = filtered
                * voice.elementLevels[static_cast<size_t>(i)]
                * amp
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

    output.left = std::clamp(output.left, -1.0f, 1.0f);
    output.right = std::clamp(output.right, -1.0f, 1.0f);
    return output;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChimeraEngineAudioProcessor();
}
