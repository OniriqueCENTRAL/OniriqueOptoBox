cmake_minimum_required(VERSION 3.22)

project(OniriqueOptoBox VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    # Universal binary, and the oldest macOS the JUCE 9 runtime supports. A
    # build to look at the interface can do with this Mac's own architecture.
    option(OPTOBOX_UNIVERSAL "Build for both Apple silicon and Intel" ON)

    if(OPTOBOX_UNIVERSAL)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()

    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# JUCE is looked for next to this file first, then in the PitchBox checkout
# beside this one, so the plugins of the range can share a single copy.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    set(OPTOBOX_JUCE_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
else()
    set(OPTOBOX_JUCE_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/../PitchBox/JUCE")
endif()

set(OPTOBOX_JUCE_DIR "${OPTOBOX_JUCE_DEFAULT}" CACHE PATH "Path to JUCE")

if(NOT EXISTS "${OPTOBOX_JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found at ${OPTOBOX_JUCE_DIR}.\n"
        "Run:  git clone --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git")
endif()

add_subdirectory("${OPTOBOX_JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# The DSP, built on its own so the offline tests can link it without dragging
# in JUCE. Everything that decides how the plugin sounds lives here.
add_library(optobox_core STATIC
    Source/dsp/Opto.cpp)

target_include_directories(optobox_core PUBLIC Source)
set_target_properties(optobox_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    # MSVC's <cmath> only declares M_PI when asked. PUBLIC so the tests get it too.
    target_compile_definitions(optobox_core PUBLIC _USE_MATH_DEFINES)
    target_compile_options(optobox_core PRIVATE /W4)
else()
    target_compile_options(optobox_core PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# The plugin.
set(OPTOBOX_FORMATS VST3)

if(APPLE)
    list(APPEND OPTOBOX_FORMATS AU)
endif()

# The system VST3 folder on Windows is under Program Files, which a normal build
# cannot write to, so installing after the build is opt-in there.
if(WIN32)
    option(OPTOBOX_COPY_AFTER_BUILD "Install the plugin after building" OFF)
else()
    option(OPTOBOX_COPY_AFTER_BUILD "Install the plugin after building" ON)
endif()

juce_add_plugin(OniriqueOptoBox
    COMPANY_NAME "Onirique"
    BUNDLE_ID "com.onirique.optobox"
    PLUGIN_MANUFACTURER_CODE Onrq
    PLUGIN_CODE Opbx
    FORMATS ${OPTOBOX_FORMATS}
    PRODUCT_NAME "OptoBox"
    # What hosts list it as. The file on disk keeps the short name.
    PLUGIN_NAME "ONIRIQUE OptoBox"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD ${OPTOBOX_COPY_AFTER_BUILD})

# The Onirique sigil, engraved on the meter's face.
juce_add_binary_data(OptoBoxAssets SOURCES Resources/sigil.png)

target_sources(OniriqueOptoBox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/Faceplate.cpp
    Source/ui/Hardware.cpp
    Source/ui/VuMeter.cpp)

# The licence every Onirique plugin carries: the folder is copied as it is from
# plugin-license/ in the ONIRIQUE CENTRAL repository, and is only ever changed there.
include(License/OniriqueLicense.cmake)
onirique_add_license(OniriqueOptoBox)

target_compile_definitions(OniriqueOptoBox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

# The descriptions are in French: the sources are UTF-8, and MSVC has to be told.
if(MSVC)
    target_compile_options(OniriqueOptoBox PRIVATE /utf-8)
endif()

target_link_libraries(OniriqueOptoBox
    PRIVATE
        optobox_core
        OptoBoxAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Offline test: the whole audio path without a host.
enable_testing()

add_executable(test_opto tests/test_opto.cpp)
target_link_libraries(test_opto PRIVATE optobox_core)
target_include_directories(test_opto PRIVATE tests)
add_test(NAME opto COMMAND test_opto)

# A development tool, not part of the product: renders the editor to a PNG
# without a host, and puts the processor through what a host does to it.
# Built only when asked for by name.
add_executable(optobox_snapshot EXCLUDE_FROM_ALL tools/snapshot.cpp)

target_include_directories(optobox_snapshot PRIVATE
    tests
    $<TARGET_PROPERTY:OniriqueOptoBox,INCLUDE_DIRECTORIES>)

target_compile_definitions(optobox_snapshot PRIVATE
    $<TARGET_PROPERTY:OniriqueOptoBox,COMPILE_DEFINITIONS>)

target_link_libraries(optobox_snapshot PRIVATE OniriqueOptoBox optobox_core OptoBoxAssets)
