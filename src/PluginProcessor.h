#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Envelope.h"
#include "dsp/Filter.h"
#include "dsp/Lfo.h"
#include "dsp/ModulationMatrix.h"
#include "dsp/SamplePlayer.h"
#include "dsp/SampleZone.h"
#include "engine/Arpeggiator.h"
#include "engine/ArpLibrary.h"
#include "engine/DrumKit.h"
#include "engine/Performance.h"
#include "engine/SampleLibrary.h"
#include "engine/Sequencer.h"
#include "fx/FxChain.h"
#include <array>
#include <map>
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
    int getCurrentPerformanceScene() const { return currentPerformanceScene; }
    void applyPerformanceScene(int sceneIndex);
    void setMpeExpressionEnabled(bool shouldBeEnabled) { mpeExpressionEnabled = shouldBeEnabled; }
    bool isMpeExpressionEnabled() const { return mpeExpressionEnabled; }
    void applyMidi2PerNoteController(int midiChannel, int midiNote, int controller, float value);
    void setLiveRecordingEnabled(bool shouldRecord, bool overdub, bool punch);
    bool isLiveRecordingEnabled() const { return liveRecordingEnabled; }
    void setCurrentSequencerTrack(int trackIndex);
    int getCurrentSequencerTrack() const { return currentSequencerTrack; }
    bool addPatternPhraseNote(int sectionIndex, int trackIndex, int tick, int durationTicks, int note, int velocity, int channel);
    void assignPatternSection(int sectionIndex, int phraseSlot);
    int getPatternSectionPhrase(int sectionIndex) const;
    int getPatternSectionNoteCount(int sectionIndex) const;
    bool saveUserArp(int slotIndex, const juce::String& name);
    bool assignArpToLane(int laneIndex, int userSlotIndex);
    int getArpLaneAssignment(int laneIndex) const;
    bool storePerformance(int index, const juce::String& name);
    bool recallPerformance(int index);
    juce::String getPerformanceName(int index) const;
    void captureSceneSnapshot(int sceneIndex, const juce::String& name);
    juce::String getSceneName(int sceneIndex) const;
    bool mapDrumKey(int midiNote, const juce::String& name, int waveformId);
    int getMappedDrumKeyCount() const;
    bool isDrumKitModeEnabled() const { return drumKitModeEnabled; }
    void setDrumKitModeEnabled(bool shouldBeEnabled) { drumKitModeEnabled = shouldBeEnabled; }
    juce::Result indexSampleLibrary(const juce::File& root);
    int getIndexedSampleCount() const { return indexedSampleCount; }
    void setPresetFavorite(const juce::String& presetName, bool shouldBeFavorite);
    bool isPresetFavorite(const juce::String& presetName) const;
    juce::String getPresetMetadataSummary(const juce::String& presetName) const;
    juce::String getVoiceEditSummary(int elementIndex) const;
    juce::String getModMatrixSummary(int elementIndex) const;
    float getOutputPeakLeft() const { return outputPeakLeft; }
    float getOutputPeakRight() const { return outputPeakRight; }
    int getCurrentSongNoteCount() const;
    juce::Result exportCurrentSongToMidi(const juce::File& file) const;
    juce::Result importSongFromMidi(const juce::File& file);
    juce::Result bounceDemoToWav(const juce::File& file, double durationSeconds);
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
        int laneIndex = 0;
        int partIndex = 0;
        int note = 0;
    };

    struct ArpeggiatorLane
    {
        chimera::engine::Arpeggiator engine;
        std::vector<HeldArpeggiatorNote> heldNotes;
        std::vector<ActiveArpeggiatorNote> activeNotes;
        int internalPartIndex = 0;
        int samplesUntilStep = 0;
        int samplesUntilGate = 0;
        bool enabled = true;
    };

    struct SceneSnapshot
    {
        juce::String name { "Init Scene" };
        std::array<float, 16> levels {};
        std::array<float, 16> pans {};
        std::array<bool, 16> enabled {};
        std::array<chimera::fx::EffectType, chimera::fx::InsertRack::slotCount> inserts {};
        std::array<int, chimera::engine::Performance::partCount> arpAssignments {};
        float chorus = 0.18f;
        float reverb = 0.16f;
        bool valid = false;
    };

    struct ActiveRecordingNote
    {
        int startTick = 0;
        int velocity = 100;
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
    void captureOutputMeters(const StereoSample& sample);
    void recordLiveMidiMessage(const juce::MidiMessage& message);
    void stopVoicesForNote(int partIndex, int note);
    void releaseVoice(ActiveVoice& voice);
    ActiveVoice& allocateVoice();
    void startVoice(ActiveVoice& target, int partIndex, int note, int velocity, float level = 1.0f, float pan = 0.0f);
    void addHeldArpeggiatorNote(int laneIndex, int partIndex, int note, int velocity);
    void removeHeldArpeggiatorNote(int laneIndex, int partIndex, int note);
    void advanceArpeggiators();
    void advanceArpeggiatorLane(int laneIndex);
    void refreshArpeggiatorHeldNotes(int laneIndex);
    void stopActiveArpeggiatorNotes(int laneIndex);
    void stopAllActiveArpeggiatorNotes();
    void applyFxConfiguration(bool resetFx);
    void applyMasterFxConfiguration();
    void addSequencerEventsForBlock(juce::MidiBuffer& midi, int numSamples);
    void applySequencerScenesForRange(int startTick, int endTick);
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
    std::array<ArpeggiatorLane, chimera::engine::Performance::partCount> arpeggiatorLanes;
    chimera::engine::ArpLibrary arpLibrary;
    chimera::engine::DrumKit drumKit;
    chimera::engine::PerformanceBank performanceBank;
    chimera::engine::SampleLibrary sampleLibrary;
    chimera::engine::Performance activePerformance;
    chimera::engine::Sequencer sequencer;
    std::array<SceneSnapshot, 8> sceneSnapshots;
    std::array<int, chimera::engine::Pattern::sectionCount> patternSectionPhrases {};
    std::array<int, chimera::engine::Performance::partCount> arpLaneAssignments {};
    std::map<juce::String, bool> presetFavorites;
    std::map<std::pair<int, int>, ActiveRecordingNote> activeRecordingNotes;
    std::map<std::pair<int, int>, float> midi2PerNotePressure;
    std::array<chimera::fx::WorkstationFx, 2> workstationFx;
    std::array<chimera::fx::EffectType, chimera::fx::InsertRack::slotCount> insertEffects {};
    double currentSampleRate = 44100.0;
    uint64_t voiceAgeCounter = 0;
    float chorusSend = 0.18f;
    float reverbSend = 0.16f;
    float outputPeakLeft = 0.0f;
    float outputPeakRight = 0.0f;
    double sequencerTick = 0.0;
    int indexedSampleCount = 0;
    int currentSequencerTrack = 0;
    int currentPerformanceScene = 0;
    bool sequencerPlaybackEnabled = false;
    bool sequencerDemoSeeded = false;
    bool mpeExpressionEnabled = false;
    bool liveRecordingEnabled = false;
    bool overdubRecordingEnabled = true;
    bool punchRecordingEnabled = false;
    bool drumKitModeEnabled = false;
    bool arpeggiatorWasEnabled = false;
    bool performanceModeEnabled = false;
};
