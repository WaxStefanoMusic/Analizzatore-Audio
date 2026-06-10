cmake_minimum_required(VERSION 3.22)

project(AnalizzatoreAudio VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
# JUCE via FetchContent
# ----------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ----------------------------------------------------------------------------
# Plugin target
# ----------------------------------------------------------------------------
juce_add_plugin(AnalizzatoreAudio
    COMPANY_NAME            "MagroAudio"
    PLUGIN_MANUFACTURER_CODE Magr
    PLUGIN_CODE             Anaa
    FORMATS                 VST3 Standalone
    PRODUCT_NAME            "Analizzatore Audio"
    BUNDLE_ID               com.magroaudio.analizzatoreaudio
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES         "Fx" "Analyzer"
)

juce_generate_juce_header(AnalizzatoreAudio)

target_sources(AnalizzatoreAudio PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Analyzer/LufsMeter.cpp
    Source/Analyzer/LufsMeter.h
    Source/Analyzer/TruePeakDetector.cpp
    Source/Analyzer/TruePeakDetector.h
    Source/Analyzer/SpectrumAnalyzer.cpp
    Source/Analyzer/SpectrumAnalyzer.h
    Source/Analyzer/Snapshot.h
    Source/DSP/TruePeakLimiter.cpp
    Source/DSP/TruePeakLimiter.h
    Source/StreamingTargets.h
    Source/GUI/MetersPanel.cpp
    Source/GUI/MetersPanel.h
    Source/GUI/SpectrumPanel.cpp
    Source/GUI/SpectrumPanel.h
    Source/GUI/ABPanel.cpp
    Source/GUI/ABPanel.h
    Source/GUI/TargetPanel.cpp
    Source/GUI/TargetPanel.h
    Source/GUI/Theme.h
    Source/GUI/HelpBadge.h
    Source/UpdateChecker.cpp
    Source/UpdateChecker.h
    Source/Version.h
)

target_compile_definitions(AnalizzatoreAudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
)

# Force MSVC to interpret all source files as UTF-8 instead of the system code
# page (cp1252 on Italian Windows). Without this, multi-byte characters such as
# accented letters and box symbols in string literals get split into garbage.
if (MSVC)
    target_compile_options(AnalizzatoreAudio PRIVATE /utf-8)
endif()

target_link_libraries(AnalizzatoreAudio PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_include_directories(AnalizzatoreAudio PRIVATE Source)
