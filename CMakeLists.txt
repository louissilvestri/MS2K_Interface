cmake_minimum_required(VERSION 3.22)
project(MS2K_Interface VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(MS2K_Interface
    PRODUCT_NAME "MS2K_Interface"
    COMPANY_NAME "MS2K"
    ICON_BIG   "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon-small.png")

target_sources(MS2K_Interface PRIVATE
    Source/Main.cpp
    Source/model/ParameterModel.cpp
    Source/midi/SysexCodec.cpp
    Source/midi/MidiMessages.cpp
    Source/midi/MidiEngine.cpp
    Source/midi/rtmidi/RtMidi.cpp
    Source/ui/Components.cpp
    Source/ui/ModSeqPanel.cpp
    Source/ui/MS2000LookAndFeel.cpp)

# RtMidi: use the Windows MultiMedia (winmm) backend.
target_compile_definitions(MS2K_Interface PRIVATE __WINDOWS_MM__=1)

target_compile_definitions(MS2K_Interface PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MS2K_Interface,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MS2K_Interface,JUCE_VERSION>")

target_link_libraries(MS2K_Interface PRIVATE
    juce::juce_audio_devices
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# Self-contained binary on MinGW (no external libstdc++/libgcc/winpthread DLLs),
# so the app runs by double-click regardless of which toolchain is on PATH.
if (MINGW)
    target_link_options(MS2K_Interface PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_libraries(MS2K_Interface PRIVATE winmm) # RtMidi winmm backend
endif()

# ---- VST3 plugin (MIDI effect) for DAW automation ----
# Shares the model/MIDI-message/UI code, but emits MIDI on the host's output bus
# instead of opening a hardware port (so no RtMidi / MidiEngine here).
juce_add_plugin(MS2K_Plugin
    PRODUCT_NAME "MS2K Editor"
    COMPANY_NAME "MS2K"
    PLUGIN_MANUFACTURER_CODE Ms2k
    PLUGIN_CODE Ms2e
    FORMATS VST3
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    VST3_CATEGORIES "Fx" "Instrument"
    ICON_BIG   "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon-small.png"
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(MS2K_Plugin PRIVATE
    Source/plugin/PluginProcessor.cpp
    Source/plugin/PluginEditor.cpp
    Source/model/ParameterModel.cpp
    Source/midi/SysexCodec.cpp
    Source/midi/MidiMessages.cpp
    Source/ui/Components.cpp
    Source/ui/ModSeqPanel.cpp
    Source/ui/MS2000LookAndFeel.cpp)

target_compile_definitions(MS2K_Plugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MS2K_Plugin PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

if (MINGW)
    target_link_options(MS2K_Plugin PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# ---- standalone unit tests for the pure-C++ core (no JUCE) ----
enable_testing()
add_executable(codec_tests   tests/test_sysex_codec.cpp  Source/midi/SysexCodec.cpp)
add_executable(model_tests   tests/test_param_model.cpp  Source/model/ParameterModel.cpp)
add_executable(message_tests tests/test_midi_messages.cpp Source/midi/MidiMessages.cpp Source/model/ParameterModel.cpp)
add_test(NAME codec   COMMAND codec_tests)
add_test(NAME model   COMMAND model_tests)
add_test(NAME message COMMAND message_tests)
