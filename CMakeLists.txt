cmake_minimum_required(VERSION 3.22)

# ------------------------------------------------------------------------------
# RiddimForge - reggae instrument plugin (AU / VST3 / Standalone)
# ------------------------------------------------------------------------------

if(APPLE)
    # Universal binary (Apple Silicon + Intel), macOS 11+
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures" FORCE)
    endif()
endif()

project(RiddimForge VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local copy of JUCE if it sits next to this file, otherwise download it.
set(RF_JUCE_TAG "9.0.4" CACHE STRING "JUCE git tag to download")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${RF_JUCE_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(RF_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RF_FORMATS AU)
endif()

juce_add_plugin(RiddimForge
    COMPANY_NAME                "RiddimForge"
    BUNDLE_ID                   "com.riddimforge.instrument"
    PLUGIN_MANUFACTURER_CODE    Ridm
    PLUGIN_CODE                 Rfg1
    FORMATS                     ${RF_FORMATS}
    PRODUCT_NAME                "RiddimForge"
    DESCRIPTION                 "Reggae drums, percussion, skank, bubble, bass, keys and riddim sequencer"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    VST3_CATEGORIES             Instrument Drum Synth
    MICROPHONE_PERMISSION_ENABLED FALSE)

# drum kits sliced from your sessions, embedded in the plugin
file(GLOB RF_KIT_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Kits/*" "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Instruments/*")
juce_add_binary_data(RiddimForgeKits
    HEADER_NAME RFKits.h
    NAMESPACE   RFKits
    SOURCES     ${RF_KIT_FILES})

target_sources(RiddimForge PRIVATE
    Source/plugin/PluginProcessor.cpp
    Source/plugin/PluginEditor.cpp
    Source/plugin/KitLoader.cpp
    Source/engine/DrumSampler.cpp
    Source/engine/Generator.cpp
    Source/engine/SampledInstrument.cpp
    Source/engine/Drums.cpp
    Source/engine/Instruments.cpp
    Source/engine/Riddim.cpp
    Source/engine/Engine.cpp)

target_include_directories(RiddimForge PRIVATE Source/engine Source/plugin)

target_compile_definitions(RiddimForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# Only used for compile-checking on Linux machines without audio dev packages
option(RF_LINUX_NO_AUDIO_DEVICES "Disable ALSA/JACK (Linux compile checks only)" OFF)
if(RF_LINUX_NO_AUDIO_DEVICES)
    target_compile_definitions(RiddimForge PUBLIC JUCE_ALSA=0 JUCE_JACK=0)
endif()

target_link_libraries(RiddimForge
    PRIVATE
        RiddimForgeKits
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
