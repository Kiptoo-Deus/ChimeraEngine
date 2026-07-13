cmake_minimum_required(VERSION 3.24)

project(ChimeraEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
)

FetchContent_MakeAvailable(juce)

juce_add_plugin(ChimeraEngine
    COMPANY_NAME "Chimera Engine"
    PRODUCT_NAME "Chimera Engine"
    BUNDLE_ID "com.chimeraengine.instrument"
    PLUGIN_MANUFACTURER_CODE Chim
    PLUGIN_CODE ChEn
    FORMATS Standalone VST3 AU
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(ChimeraEngine PRIVATE
    src/PluginEditor.cpp
    src/PluginProcessor.cpp
    src/dsp/Envelope.cpp
    src/dsp/Element.cpp
    src/dsp/Filter.cpp
    src/dsp/Lfo.cpp
    src/dsp/ModulationMatrix.cpp
    src/dsp/SampleZone.cpp
    src/engine/Arpeggiator.cpp
    src/engine/Performance.cpp
    src/engine/VoiceAllocator.cpp
    src/fx/FxChain.cpp
    src/gui/LookAndFeel.cpp
    src/preset/Preset.cpp
)

target_include_directories(ChimeraEngine PRIVATE src)

target_compile_definitions(ChimeraEngine
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ChimeraEngine
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

enable_testing()

add_executable(ChimeraEngineTests
    tests/DspTests.cpp
    src/dsp/Envelope.cpp
    src/dsp/Element.cpp
    src/dsp/Filter.cpp
    src/dsp/Lfo.cpp
    src/dsp/ModulationMatrix.cpp
    src/dsp/SampleZone.cpp
    src/engine/Arpeggiator.cpp
    src/engine/Performance.cpp
    src/engine/VoiceAllocator.cpp
)

target_include_directories(ChimeraEngineTests PRIVATE src)
target_link_libraries(ChimeraEngineTests PRIVATE juce::juce_audio_basics juce::juce_core)
add_test(NAME ChimeraEngineDspTests COMMAND ChimeraEngineTests)
