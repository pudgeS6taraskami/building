cmake_minimum_required(VERSION 3.22)
project(LoFiPlugin VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE скачивается автоматически при конфигурации проекта
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(LoFiPlugin
    COMPANY_NAME "LoFiLab"
    PLUGIN_MANUFACTURER_CODE Lofi
    PLUGIN_CODE Lf01
    FORMATS VST3
    PRODUCT_NAME "LoFi Machine"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Картинки, встроенные в плагин (BinaryData::background_png)
juce_add_binary_data(LoFiAssets SOURCES Assets/background.png)
set_target_properties(LoFiAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(LoFiPlugin PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/KeysEffect.cpp
    Source/BassEffect.cpp)

target_link_libraries(LoFiPlugin
    PRIVATE
        LoFiAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

target_compile_definitions(LoFiPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
