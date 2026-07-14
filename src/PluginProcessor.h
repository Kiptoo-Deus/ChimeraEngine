#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Envelope.h"
#include "dsp/Filter.h"
#include "dsp/Lfo.h"
#include "dsp/ModulationMatrix.h"
#include "dsp/SamplePlayer.h"
#include "dsp/SampleZone.h"
#include "engine/Arpeggiator.h"
#include "engine/Performance.h"
#include "engine/Sequencer.h"
#include "fx/FxChain.h"
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
    juce::Result loadSynthPresetForPart(int partIndex, const juce::String& presetName);
    juce::String getCurrentPatchName() const { return currentPatchName; }
    juce::String getPartPatchName(int partIndex) const;
    void setPartMix(int partIndex, float level, float pan, bool enabled);
    float getPartLevel(int partIndex) const;
    float getPartPan(int partIndex) const;
    bool isPartEnabled(int partIndex) const;
    void setInsertEffect(int slotIndex, chimera::fx::EffectType type);
    chimera::fx::EffectType getInsertEffect(int slotIndex) const;
    void setSystemFxSends(float chorusSend, float reverbSend);
    float getChorusSend() const { return chorusSend; }
    float getReverbSend() const { return reverbSend; }
    void setPerformanceModeEnabled(bool shouldBeEnabled);
    bool isPerformanceModeEnabled() const { return performanceModeEnabled; }
    void setPerformancePart(int performancePartIndex, chimera::engine::PartZone zone);
    void setSequencerPlaybackEnabled(bool shouldPlay);
    bool isSequencerPlaybackEnabled() const { return sequencerPlaybackEnabled; }
    void resetSequencerPlayback();
    void seedDemoSequence();
    int getSequencerTick() const { return static_cast<int>(sequencerTick); }
    static constexpr int getPartCount() { return static_cast<int>(maxParts); }
    static constexpr int getMaxVoiceCount() { return static_cast<int>(maxVoices); }

private:
    struct StereoSample
    {
        float left = 0.0f;
        float right = 0.0f;
    };

    struct LoadedElement
    {
        std::vector<std::shared_ptr<chimera::dsp::SampleZone>> zones;
        std::vector<std::shared_ptr<chimera::dsp::SampleZone>> releaseZones;
        juce::String alternateMode { "off" };
        float level = 1.0f;
        float pan = 0.0f;
        chimera::dsp::FilterMode filterMode = chimera::dsp::FilterMode::LowPass12;
        float ampAttack = 0.0f;
        float ampDecay1 = 0.05f;
        float ampDecay2 = 0.05f;
        float ampSustain = 1.0f;
        float ampRelease = 0.0f;
        float pitchAttack = 0.0f;
        float pitchDecay1 = 0.05f;
        float pitchDecay2 = 0.05f;
        float pitchSustain = 1.0f;
        float pitchRelease = 0.0f;
        float pitchDepthCents = 0.0f;
        float filterAttack = 0.0f;
        float filterDecay1 = 0.05f;
        float filterDecay2 = 0.05f;
        float filterSustain = 1.0f;
        float filterRelease = 0.0f;
        float filterDepth = 0.0f;
        float lfo1RateHz = 0.0f;
        float lfo1CutoffDepth = 0.0f;
        float lfo2RateHz = 0.0f;
        float lfo2AmpDepth = 0.0f;
        float lfo2PanDepth = 0.0f;
        std::array<chimera::dsp::ModSlot, 8> modSlots {};
        int modSlotCount = 0;
        int roundRobinCounter = 0;
    };

    struct ActiveVoice
    {
        std::array<chimera::dsp::SamplePlayer, 8> players;
        std::array<chimera::dsp::SamplePlayer, 8> releasePlayers;
        std::array<chimera::dsp::Filter, 8> filters;
        std::array<chimera::dsp::Envelope, 8> ampEnvelopes;
        std::array<chimera::dsp::Envelope, 8> pitchEnvelopes;
        std::array<chimera::dsp::Envelope, 8> filterEnvelopes;
        std::array<chimera::dsp::Lfo, 8> lfo1;
        std::array<chimera::dsp::Lfo, 8> lfo2;
        std::array<float, 8> elementLevels {};
        std::array<float, 8> elementPans {};
        std::array<chimera::dsp::FilterMode, 8> elementFilterModes {};
        std::array<float, 8> pitchEnvelopeDepths {};
        std::array<float, 8> filterEnvelopeDepths {};
        std::array<float, 8> lfo1CutoffDepths {};
        std::array<float, 8> lfo2AmpDepths {};
        std::array<float, 8> lfo2PanDepths {};
        std::array<std::array<chimera::dsp::ModSlot, 8>, 8> modSlots {};
        std::array<int, 8> modSlotCounts {};
        int elementCount = 0;
        int partIndex = 0;
        int note = -1;
        int targetNote = -1;
        uint64_t age = 0;
        float velocityGain = 0.0f;
        float partLevel = 1.0f;
        float partPan = 0.0f;
        float currentPitchCents = 0.0f;
        float targetPitchCents = 0.0f;
        float portamentoStepCents = 0.0f;
        bool active = false;
        bool released = false;
    };

    struct PartState
    {
        std::array<LoadedElement, 8> loadedElements;
        int loadedElementCount = 0;
        juce::String patchName { "Sine" };
        juce::String voiceMode { "poly" };
        float portamentoTime = 0.0f;
        int pitchBendRange = 2;
        float level = 1.0f;
        float pan = 0.0f;
        bool enabled = true;
    };

    struct HeldArpeggiatorNote
    {
        int partIndex = 0;
        int note = 0;
        int velocity = 100;
    };

    struct ActiveArpeggiatorNote
    {
        int partIndex = 0;
        int note = 0;
    };

    static constexpr size_t maxParts = 16;
    static constexpr size_t maxVoices = 128;
    static constexpr size_t maxElements = 8;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::Result loadDefaultPatch();
    juce::Result loadPatchFileForPart(int partIndex, const juce::File& patchFile);
    void setActiveElementsForPart(int partIndex, std::array<LoadedElement, maxElements> elements, int count,
                                  const juce::String& patchName);
    void handleMidiMessage(const juce::MidiMessage& message);
    void stopVoicesForNote(int partIndex, int note);
    void releaseVoice(ActiveVoice& voice);
    ActiveVoice& allocateVoice();
    void startVoice(ActiveVoice& target, int partIndex, int note, int velocity, float level = 1.0f, float pan = 0.0f);
    void advanceArpeggiator();
    void refreshArpeggiatorHeldNotes();
    void stopActiveArpeggiatorNotes();
    void applyFxConfiguration(bool resetFx);
    void addSequencerEventsForBlock(juce::MidiBuffer& midi, int numSamples);
    int sequencerLoopEndTick() const;
    StereoSample renderVoiceSample();

    juce::AudioProcessorValueTreeState parameters;
    juce::CriticalSection pendingMidiLock;
    juce::MidiBuffer pendingPreviewMidi;
    juce::CriticalSection zoneLock;
    std::array<PartState, maxParts> parts;
    std::array<int, maxParts> lastMonoNotes {};
    std::array<float, maxParts> pitchBendSemitones {};
    std::array<float, maxParts> modWheelValues {};
    std::array<float, maxParts> aftertouchValues {};
    juce::String currentPatchName { "Sine" };
    std::array<ActiveVoice, maxVoices> voices;
    chimera::engine::Arpeggiator arpeggiator;
    chimera::engine::Performance activePerformance;
    chimera::engine::Sequencer sequencer;
    std::array<chimera::fx::WorkstationFx, 2> workstationFx;
    std::array<chimera::fx::EffectType, chimera::fx::InsertRack::slotCount> insertEffects {};
    std::vector<HeldArpeggiatorNote> heldArpeggiatorNotes;
    std::vector<ActiveArpeggiatorNote> activeArpeggiatorNotes;
    int arpeggiatorPartIndex = 0;
    double currentSampleRate = 44100.0;
    uint64_t voiceAgeCounter = 0;
    int arpeggiatorSamplesUntilStep = 0;
    int arpeggiatorSamplesUntilGate = 0;
    float chorusSend = 0.18f;
    float reverbSend = 0.16f;
    double sequencerTick = 0.0;
    bool sequencerPlaybackEnabled = false;
    bool sequencerDemoSeeded = false;
    bool arpeggiatorWasEnabled = false;
    bool performanceModeEnabled = false;
};
