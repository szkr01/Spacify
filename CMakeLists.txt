cmake_minimum_required(VERSION 3.22)

project(Spacify VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The plug-in is shipped as a self-contained binary, so the MSVC runtime is
# linked statically and no redistributable is needed on the target machine.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "" FORCE)
    add_compile_options(/MP /utf-8)
endif()

option(SPACIFY_BUILD_TOOLS "Build the offline UI snapshot / DSP verification tools" OFF)

add_subdirectory(external/JUCE)

# ------------------------------------------------------------------------------
# Shared source list
# ------------------------------------------------------------------------------
set(SPACIFY_SOURCES
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/ui/SpacifyKnob.cpp
    source/ui/SpacifyLookAndFeel.cpp)

set(SPACIFY_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

# ------------------------------------------------------------------------------
# The VST3 plug-in
# ------------------------------------------------------------------------------
juce_add_plugin(Spacify
    COMPANY_NAME              "Spacify Audio"
    BUNDLE_ID                 "com.spacifyaudio.spacify"
    PLUGIN_MANUFACTURER_CODE  Spcf
    PLUGIN_CODE               Ssp1
    FORMATS                   VST3
    PRODUCT_NAME              "Spacify"
    DESCRIPTION               "Adds a fuzzy, one-octave-up reverb layer on top of the dry signal."
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           "Fx")

target_sources(Spacify PRIVATE ${SPACIFY_SOURCES})

target_compile_definitions(Spacify PUBLIC ${SPACIFY_DEFINITIONS})

target_link_libraries(Spacify
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
# Development-only verification tools (never part of the delivered VST3)
# ------------------------------------------------------------------------------
if(SPACIFY_BUILD_TOOLS)
    juce_add_console_app(SpacifyDspTests PRODUCT_NAME "SpacifyDspTests")
    target_sources(SpacifyDspTests PRIVATE tools/DspTests.cpp ${SPACIFY_SOURCES})
    target_include_directories(SpacifyDspTests PRIVATE source)
    target_compile_definitions(SpacifyDspTests PRIVATE ${SPACIFY_DEFINITIONS})
    target_link_libraries(SpacifyDspTests
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    juce_add_console_app(SpacifyRenderEditor PRODUCT_NAME "SpacifyRenderEditor")
    target_sources(SpacifyRenderEditor PRIVATE tools/RenderEditor.cpp ${SPACIFY_SOURCES})
    target_include_directories(SpacifyRenderEditor PRIVATE source)
    target_compile_definitions(SpacifyRenderEditor PRIVATE ${SPACIFY_DEFINITIONS})
    target_link_libraries(SpacifyRenderEditor
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
