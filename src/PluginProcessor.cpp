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
        voice.ampEnvelope.setSampleRate(currentSampleRate);
        voice.filter.setSampleRate(currentSampleRate);
        voice.filter.reset();
        voice.player.stop();
        voice.active = false;
        voice.note = -1;
        voice.age = 0;
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
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sample, rendered);
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
    const auto patchFile = root.getChildFile("presets/Synth/Sine.chpatch");

    chimera::preset::Patch patch;
    if (const auto result = chimera::preset::loadPatch(patchFile, patch); result.failed())
        return result;

    if (patch.elements.empty())
        return juce::Result::fail("Default patch has no elements");

    const auto& element = patch.elements.front();
    auto zone = std::make_shared<chimera::dsp::SampleZone>();
    zone->setSource(root.getChildFile(element.samplePath));
    zone->setRootKey(element.rootKey);
    zone->setKeyRange(element.keyLow, element.keyHigh);
    zone->setVelocityRange(element.velocityLow, element.velocityHigh);

    if (const auto result = zone->loadAudio(); result.failed())
        return result;

    defaultZone = std::move(zone);
    for (auto& voice : voices)
        voice.player.setZone(defaultZone);

    return juce::Result::ok();
}

void ChimeraEngineAudioProcessor::handleMidiMessage(const juce::MidiMessage& message)
{
    if (message.isNoteOn())
    {
        if (defaultZone == nullptr || !defaultZone->matches(message.getNoteNumber(), message.getVelocity()))
            return;

        startVoice(allocateVoice(), message.getNoteNumber(), message.getVelocity());
        return;
    }

    if (message.isNoteOff())
        for (auto& voice : voices)
            if (voice.active && message.getNoteNumber() == voice.note)
                voice.ampEnvelope.noteOff();
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
    target.ampEnvelope.setSampleRate(currentSampleRate);
    target.ampEnvelope.setStages(*parameters.getRawParameterValue("attack"), 0.05f, 0.05f, 1.0f,
                                 *parameters.getRawParameterValue("release"), chimera::dsp::Curve::Linear);
    target.ampEnvelope.noteOn();
    target.filter.setSampleRate(currentSampleRate);
    target.filter.reset();
    target.player.setZone(defaultZone);
    target.player.start(target.note, currentSampleRate);
    target.active = true;
}

float ChimeraEngineAudioProcessor::renderVoiceSample()
{
    const auto gain = dbToGain(*parameters.getRawParameterValue("masterGain"));
    auto output = 0.0f;

    for (auto& voice : voices)
    {
        if (!voice.active)
            continue;

        voice.filter.setCutoff(*parameters.getRawParameterValue("cutoff"));
        voice.filter.setResonance(*parameters.getRawParameterValue("resonance"));

        const auto envelope = voice.ampEnvelope.process();
        const auto dry = voice.player.process();
        const auto filtered = voice.filter.process(dry);
        output += filtered * envelope * voice.velocityGain * gain;

        if (!voice.player.isPlaying() || (!voice.ampEnvelope.isActive() && envelope <= 0.0f))
        {
            voice.active = false;
            voice.note = -1;
        }
    }

    return std::clamp(output, -1.0f, 1.0f);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChimeraEngineAudioProcessor();
}
