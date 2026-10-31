cmake_minimum_required(VERSION 3.22)

project(PUNCHMAXXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# ---- JUCE ------------------------------------------------------------------
# Uses a JUCE folder next to this project if you have one (e.g. ../JUCE or ./JUCE),
# otherwise downloads JUCE 8.0.9 automatically.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    add_subdirectory(../JUCE ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- Plugin ----------------------------------------------------------------
juce_add_plugin(PUNCHMAXXX
    COMPANY_NAME "TRIPLE 7 AUDIO"
    COMPANY_WEBSITE "https://barnacleboi.com"
    BUNDLE_ID "com.triple7audio.punchmaxxx"
    PLUGIN_MANUFACTURER_CODE T777
    PLUGIN_CODE Pmxx
    PRODUCT_NAME "PUNCHMAXXX"
    DESCRIPTION "Drum Dynamics Engine"
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(PUNCHMAXXX)

juce_add_binary_data(PunchmaxxxAssets SOURCES
    Resources/background.png)

target_sources(PUNCHMAXXX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp)

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

# ---- Optional offline test harness (cmake -DPMX_BUILD_TESTS=ON) -------------
option(PMX_BUILD_TESTS "Build PUNCHMAXXX test harness" OFF)
if (PMX_BUILD_TESTS)
    juce_add_console_app(PunchmaxxxTests PRODUCT_NAME "PunchmaxxxTests")
    juce_generate_juce_header(PunchmaxxxTests)
    target_sources(PunchmaxxxTests PRIVATE
        Tests/TestMain.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/PresetManager.cpp)
    target_compile_definitions(PunchmaxxxTests PRIVATE
        JucePlugin_Name="PUNCHMAXXX" JucePlugin_VersionString="1.0.0"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(PunchmaxxxTests PRIVATE PunchmaxxxAssets
        juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
