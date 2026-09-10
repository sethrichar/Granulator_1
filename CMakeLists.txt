cmake_minimum_required(VERSION 3.22)

project(Ruminate VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(RUMINATE_BUILD_TESTS "Build the offline DSP test harness" ON)

# ---------------------------------------------------------------------------
# JUCE: use a local checkout if one is available, otherwise fetch it.
#   - set -DJUCE_DIR=/path/to/JUCE on the cmake command line, or
#   - export JUCE_DIR=/path/to/JUCE, or
#   - clone JUCE into libs/JUCE
# ---------------------------------------------------------------------------
set(RUMINATE_JUCE_TAG "8.0.9")

if(NOT JUCE_DIR AND DEFINED ENV{JUCE_DIR})
    set(JUCE_DIR "$ENV{JUCE_DIR}")
endif()
if(NOT JUCE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE/CMakeLists.txt")
    set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE")
endif()

if(JUCE_DIR)
    message(STATUS "Using local JUCE at ${JUCE_DIR}")
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    message(STATUS "Fetching JUCE ${RUMINATE_JUCE_TAG}")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${RUMINATE_JUCE_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
if(APPLE)
    set(RUMINATE_FORMATS VST3 AU Standalone)
else()
    set(RUMINATE_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Ruminate
    COMPANY_NAME                "Seth Richardson"
    BUNDLE_ID                   "com.sethrichardson.ruminate"
    PLUGIN_MANUFACTURER_CODE    Srch
    PLUGIN_CODE                 Rumn
    FORMATS                     ${RUMINATE_FORMATS}
    PRODUCT_NAME                "Ruminate"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Delay Reverb
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(Ruminate PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/dsp/GrainEngine.cpp)

target_include_directories(Ruminate PUBLIC Source)

target_compile_definitions(Ruminate PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(Ruminate
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Offline test harness (no audio device needed). Renders through the real
# processor and checks the DSP for silence / NaNs / runaway feedback.
# ---------------------------------------------------------------------------
if(RUMINATE_BUILD_TESTS)
    add_executable(RuminateTests Tests/RuminateTests.cpp)
    target_link_libraries(RuminateTests PRIVATE Ruminate juce::juce_audio_utils juce::juce_dsp)
    enable_testing()
    add_test(NAME RuminateTests COMMAND RuminateTests)
endif()
