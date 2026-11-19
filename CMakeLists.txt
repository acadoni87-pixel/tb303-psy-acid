cmake_minimum_required(VERSION 3.22)
project(TB303_Psychedelic_Acid VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build Universal Binary for Intel & Apple Silicon")

add_subdirectory(JUCE)

juce_add_plugin(TB303_Psychedelic_Acid
    COMPANY_NAME "Electrovision"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Elvs
    PLUGIN_CODE P303
    FORMATS VST3 AU
    PRODUCT_NAME "TB-303 Psychedelic Acid")

juce_generate_juce_header(TB303_Psychedelic_Acid)

target_sources(TB303_Psychedelic_Acid PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(TB303_Psychedelic_Acid PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TB303_Psychedelic_Acid PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
