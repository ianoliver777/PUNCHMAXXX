cmake_minimum_required(VERSION 3.22)

project(PUNCHMAXXX VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

option(PUNCHMAXXX_BUILD_TESTS "Build the headless test runner" ON)

# --- JUCE: use ./JUCE if it's there, otherwise download it -----------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Plugin ----------------------------------------------------------------
juce_add_plugin(PUNCHMAXXX
    COMPANY_NAME "TRIPLE 7 AUDIO"
    BUNDLE_ID com.triple7audio.punchmaxxx
    PLUGIN_MANUFACTURER_CODE T7au
    PLUGIN_CODE Pmxx
    PRODUCT_NAME "PUNCHMAXXX"
    DESCRIPTION "Transient Drum Processor"
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

juce_add_binary_data(PunchmaxxxAssets SOURCES
    assets/background.png
    assets/Oswald-Bold.ttf
    assets/Oswald-Medium.ttf)

target_sources(PUNCHMAXXX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp
    Source/UI/Widgets.cpp)

target_include_directories(PUNCHMAXXX PRIVATE Source)

target_compile_definitions(PUNCHMAXXX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(PUNCHMAXXX
    PRIVATE
        PunchmaxxxAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Headless tests (state recall, presets, DSP sanity, UI snapshot) -------
if(PUNCHMAXXX_BUILD_TESTS)
    # Links against the plugin's shared-code library (which already contains the JUCE modules)
    add_executable(PunchmaxxxTests tests/TestMain.cpp)
    target_include_directories(PunchmaxxxTests PRIVATE Source $<TARGET_PROPERTY:PUNCHMAXXX,INCLUDE_DIRECTORIES>)
    target_compile_definitions(PunchmaxxxTests PRIVATE $<TARGET_PROPERTY:PUNCHMAXXX,COMPILE_DEFINITIONS>)
    target_link_libraries(PunchmaxxxTests PRIVATE PUNCHMAXXX juce::juce_recommended_config_flags)
endif()
