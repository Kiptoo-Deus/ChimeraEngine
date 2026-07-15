cmake_minimum_required(VERSION 3.24)

project(ChimeraEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

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
    src/dsp/SamplePlayer.cpp
    src/dsp/SampleZone.cpp
    src/engine/Arpeggiator.cpp
    src/engine/ArpLibrary.cpp
    src/engine/DrumKit.cpp
    src/engine/Performance.cpp
    src/engine/SampleLibrary.cpp
    src/engine/Sequencer.cpp
    src/engine/SequencerIO.cpp
    src/engine/VoiceAllocator.cpp
    src/fx/FxChain.cpp
    src/gui/LookAndFeel.cpp
    src/preset/Preset.cpp
    src/preset/VoiceBank.cpp
)

target_include_directories(ChimeraEngine PRIVATE src)

target_compile_definitions(ChimeraEngine
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    PRIVATE
        CHIMERA_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(ChimeraEngine
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

set(CHIMERA_SAMPLE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sample_sources")
set(CHIMERA_SAMPLE_SOURCE_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/sample_sources")
set(CHIMERA_SAMPLE_SOURCE_INDEX "${CHIMERA_SAMPLE_SOURCE_BUILD_DIR}/chimera_sample_sources.json")
set(CHIMERA_SAMPLE_SOURCE_MANIFEST "${CHIMERA_SAMPLE_SOURCE_BUILD_DIR}/manifest.csv")
set(CHIMERA_SOURCE_PRESET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/presets/Source")
set(CHIMERA_SOURCE_PRESET_STAMP "${CMAKE_CURRENT_BINARY_DIR}/sample_sources/source-presets.stamp")

add_custom_command(
    OUTPUT "${CHIMERA_SAMPLE_SOURCE_INDEX}"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/integrate_sample_sources.py"
            --source-root "${CHIMERA_SAMPLE_SOURCE_DIR}"
            --output "${CHIMERA_SAMPLE_SOURCE_INDEX}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/integrate_sample_sources.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Indexing approved local sample sources"
    VERBATIM
)

add_custom_target(ChimeraEngineSampleSourceIndex ALL
    DEPENDS "${CHIMERA_SAMPLE_SOURCE_INDEX}"
)

add_custom_command(
    OUTPUT "${CHIMERA_SOURCE_PRESET_STAMP}"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_source_presets.py"
            --project-root "${CMAKE_CURRENT_SOURCE_DIR}"
            --index "${CHIMERA_SAMPLE_SOURCE_INDEX}"
            --output-dir "${CHIMERA_SOURCE_PRESET_DIR}"
    COMMAND ${CMAKE_COMMAND} -E touch "${CHIMERA_SOURCE_PRESET_STAMP}"
    DEPENDS
            "${CHIMERA_SAMPLE_SOURCE_INDEX}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_source_presets.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating playable presets from approved sample sources"
    VERBATIM
)

add_custom_target(ChimeraEngineSourcePresets ALL
    DEPENDS "${CHIMERA_SOURCE_PRESET_STAMP}"
)

add_dependencies(ChimeraEngine ChimeraEngineSourcePresets)

function(chimera_attach_sample_source_index target_name)
    if(TARGET ${target_name})
        add_dependencies(${target_name} ChimeraEngineSampleSourceIndex)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_DIR:${target_name}>/Contents/Resources/ChimeraSampleSources"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CHIMERA_SAMPLE_SOURCE_INDEX}"
                    "$<TARGET_BUNDLE_DIR:${target_name}>/Contents/Resources/ChimeraSampleSources/chimera_sample_sources.json"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CHIMERA_SAMPLE_SOURCE_MANIFEST}"
                    "$<TARGET_BUNDLE_DIR:${target_name}>/Contents/Resources/ChimeraSampleSources/manifest.csv"
            COMMENT "Bundling Chimera sample source catalog for ${target_name}"
            VERBATIM
        )
    endif()
endfunction()

chimera_attach_sample_source_index(ChimeraEngine_Standalone)
chimera_attach_sample_source_index(ChimeraEngine_VST3)
chimera_attach_sample_source_index(ChimeraEngine_AU)

enable_testing()

add_executable(ChimeraEngineTests
    tests/DspTests.cpp
    src/dsp/Envelope.cpp
    src/dsp/Element.cpp
    src/dsp/Filter.cpp
    src/dsp/Lfo.cpp
    src/dsp/ModulationMatrix.cpp
    src/dsp/SamplePlayer.cpp
    src/dsp/SampleZone.cpp
    src/engine/Arpeggiator.cpp
    src/engine/ArpLibrary.cpp
    src/engine/DrumKit.cpp
    src/engine/Performance.cpp
    src/engine/SampleLibrary.cpp
    src/engine/Sequencer.cpp
    src/engine/SequencerIO.cpp
    src/engine/VoiceAllocator.cpp
    src/fx/FxChain.cpp
    src/preset/Preset.cpp
    src/preset/VoiceBank.cpp
)

target_include_directories(ChimeraEngineTests PRIVATE src)
target_compile_definitions(ChimeraEngineTests PRIVATE CHIMERA_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ChimeraEngineTests PRIVATE juce::juce_audio_basics juce::juce_audio_formats juce::juce_core)
add_test(NAME ChimeraEngineDspTests COMMAND ChimeraEngineTests)

add_executable(ChimeraEngineProcessorTests
    tests/ProcessorRenderTests.cpp
    src/PluginEditor.cpp
    src/PluginProcessor.cpp
    src/dsp/Envelope.cpp
    src/dsp/Element.cpp
    src/dsp/Filter.cpp
    src/dsp/Lfo.cpp
    src/dsp/ModulationMatrix.cpp
    src/dsp/SamplePlayer.cpp
    src/dsp/SampleZone.cpp
    src/engine/Arpeggiator.cpp
    src/engine/ArpLibrary.cpp
    src/engine/DrumKit.cpp
    src/engine/Performance.cpp
    src/engine/SampleLibrary.cpp
    src/engine/Sequencer.cpp
    src/engine/SequencerIO.cpp
    src/engine/VoiceAllocator.cpp
    src/fx/FxChain.cpp
    src/gui/LookAndFeel.cpp
    src/preset/Preset.cpp
    src/preset/VoiceBank.cpp
)

target_include_directories(ChimeraEngineProcessorTests PRIVATE src)
target_compile_definitions(ChimeraEngineProcessorTests
    PRIVATE
        CHIMERA_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)
target_link_libraries(ChimeraEngineProcessorTests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
add_test(NAME ChimeraEngineProcessorRenderTests COMMAND ChimeraEngineProcessorTests)

add_test(NAME ChimeraEngineSampleLicenseAudit
         COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/audit_sample_licenses.py)

add_test(NAME ChimeraEnginePresetValidation
         COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/validate_presets.py)

add_test(NAME ChimeraEngineBuildArtifactValidation
         COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/validate_build_artifacts.py
                 ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME ChimeraEngineSampleSourceIntegration
         COMMAND ${Python3_EXECUTABLE}
                 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/integrate_sample_sources.py
                 --source-root ${CMAKE_CURRENT_SOURCE_DIR}/sample_sources
                 --output ${CMAKE_CURRENT_BINARY_DIR}/sample_sources/test-chimera-sample-sources.json
                 --strict)

add_test(NAME ChimeraEngineSourcePresetGeneration
         COMMAND ${Python3_EXECUTABLE}
                 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_source_presets.py
                 --project-root ${CMAKE_CURRENT_SOURCE_DIR}
                 --index ${CHIMERA_SAMPLE_SOURCE_INDEX}
                 --output-dir ${CMAKE_CURRENT_SOURCE_DIR}/presets/Source)
