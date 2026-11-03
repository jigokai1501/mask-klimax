cmake_minimum_required(VERSION 3.22)

# La version est lue ici par le workflow GitHub (nom de l'installateur).
project(MASK_KLIMAX VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS minimum")
endif()

option(KLX_BUILD_TESTS "Compiler les tests" OFF)

# JUCE : dossier local (-DJUCE_DIR=...) ou téléchargé automatiquement.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(mask-klimax
    COMPANY_NAME "mask"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.mask.klimax"
    PLUGIN_MANUFACTURER_CODE Mask
    PLUGIN_CODE Mklx
    PRODUCT_NAME "mask-klimax"
    PLUGIN_NAME "mask::klimax"
    DESCRIPTION "One-knob build-up"
    FORMATS AU VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx)

target_sources(mask-klimax PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(mask-klimax PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(mask-klimax
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Nom dans Logic : « mask: mask꞉꞉klimax » (deux caractères qui se lisent « : » ; Logic coupe aux vrais « : »).
if(APPLE)
    include(cmake/patch_au_name.cmake)
    klx_patch_au_name(mask-klimax_AU)
endif()

if(KLX_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
