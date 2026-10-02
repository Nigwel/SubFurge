cmake_minimum_required(VERSION 3.22)
project(SubForge VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.9
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

option(SUBFORGE_INSTALL "Copy the VST3 into the system VST3 folder after build" OFF)

juce_add_plugin(SubForge
  COMPANY_NAME "JG BeatsLab"
  PLUGIN_MANUFACTURER_CODE JgBl
  PLUGIN_CODE SbFg
  FORMATS VST3
  PRODUCT_NAME "SubForge"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  VST3_CATEGORIES Instrument Synth
  COPY_PLUGIN_AFTER_BUILD ${SUBFORGE_INSTALL})

target_sources(SubForge PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(SubForge PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SubForge
  PRIVATE juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
