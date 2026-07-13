#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Envelope.h"
#include "dsp/Filter.h"
#include "dsp/SamplePlayer.h"
#include "dsp/SampleZone.h"
#include "engine/Arpeggiator.h"
#include <array>
#include <memory>
#include <vector>

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
    juce::Result loadSynthPreset(const juce::String& presetName);
    juce::String getCurrentPatchName() const { return currentPatchName; }

private:
    struct StereoSample
    {
        float left = 0.0f;
        float right = 0.0f;
    };

    struct LoadedElement
    {
        std::shared_ptr<chimera::dsp::SampleZone> zone;
        float level = 1.0f;
        float pan = 0.0f;
        float ampAttack = 0.0f;
        float ampSustain = 1.0f;
        float ampRelease = 0.0f;
    };

    struct ActiveVoice
    {
        std::array<chimera::dsp::SamplePlayer, 8> players;
        std::array<chimera::dsp::Filter, 8> filters;
        std::array<chimera::dsp::Envelope, 8> ampEnvelopes;
        std::array<float, 8> elementLevels {};
        std::array<float, 8> elementPans {};
        int elementCount = 0;
        int note = -1;
        uint64_t age = 0;
        float velocityGain = 0.0f;
        bool active = false;
    };

    static constexpr size_t maxVoices = 16;
    static constexpr size_t maxElements = 8;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::Result loadDefaultPatch();
    juce::Result loadPatchFile(const juce::File& patchFile);
    void setActiveElements(std::array<LoadedElement, maxElements> elements, int count, const juce::String& patchName);
    void handleMidiMessage(const juce::MidiMessage& message);
    ActiveVoice& allocateVoice();
    void startVoice(ActiveVoice& target, int note, int velocity);
    void advanceArpeggiator();
    void refreshArpeggiatorHeldNotes();
    void stopActiveArpeggiatorNotes();
    StereoSample renderVoiceSample();

    juce::AudioProcessorValueTreeState parameters;
    juce::CriticalSection pendingMidiLock;
    juce::MidiBuffer pendingPreviewMidi;
    juce::CriticalSection zoneLock;
    std::array<LoadedElement, maxElements> loadedElements;
    int loadedElementCount = 0;
    juce::String currentPatchName { "Sine" };
    std::array<ActiveVoice, maxVoices> voices;
    chimera::engine::Arpeggiator arpeggiator;
    std::vector<std::pair<int, int>> heldArpeggiatorNotes;
    std::vector<int> activeArpeggiatorNotes;
    double currentSampleRate = 44100.0;
    uint64_t voiceAgeCounter = 0;
    int arpeggiatorSamplesUntilStep = 0;
    int arpeggiatorSamplesUntilGate = 0;
    bool arpeggiatorWasEnabled = false;
};
