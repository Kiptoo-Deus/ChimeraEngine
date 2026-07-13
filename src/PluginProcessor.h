#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Envelope.h"
#include "dsp/Filter.h"
#include "dsp/SamplePlayer.h"
#include "dsp/SampleZone.h"
#include <array>
#include <memory>

class ChimeraEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    ChimeraEngineAudioProcessor();
    ~ChimeraEngineAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Chimera Engine"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 0; }
    int getCurrentProgram() override { return -1; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    void enqueuePreviewNoteOn(int midiChannel, int midiNote, float velocity);
    void enqueuePreviewNoteOff(int midiChannel, int midiNote);

private:
    struct ActiveVoice
    {
        chimera::dsp::SamplePlayer player;
        chimera::dsp::Envelope ampEnvelope;
        chimera::dsp::Filter filter;
        int note = -1;
        uint64_t age = 0;
        float velocityGain = 0.0f;
        bool active = false;
    };

    static constexpr size_t maxVoices = 16;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::Result loadDefaultPatch();
    void handleMidiMessage(const juce::MidiMessage& message);
    ActiveVoice& allocateVoice();
    void startVoice(ActiveVoice& target, int note, int velocity);
    float renderVoiceSample();

    juce::AudioProcessorValueTreeState parameters;
    juce::CriticalSection pendingMidiLock;
    juce::MidiBuffer pendingPreviewMidi;
    std::shared_ptr<chimera::dsp::SampleZone> defaultZone;
    std::array<ActiveVoice, maxVoices> voices;
    double currentSampleRate = 44100.0;
    uint64_t voiceAgeCounter = 0;
};
