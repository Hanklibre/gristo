juce_add_plugin(Gristo
    PRODUCT_NAME "Gristo"
    COMPANY_NAME "Gristo"
    BUNDLE_ID "com.gristo.gristo"
    PLUGIN_MANUFACTURER_CODE Grst
    PLUGIN_CODE Grs1
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(Gristo PRIVATE
    Source/FactoryPresets.cpp
    Source/Parameters.cpp
    Source/Presets.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp
    Source/Widgets.cpp)

target_compile_definitions(Gristo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Gristo
    PRIVATE
        gristo_core
        gristo_sound
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Headless tests of the plugin itself (parameters, presets, state, audio), linked
# against the shared plugin code.
if(GRISTO_BUILD_TESTS)
    add_executable(gristo_plugin_tests Tests/PluginTests.cpp)
    target_include_directories(gristo_plugin_tests PRIVATE
        $<TARGET_PROPERTY:Gristo,INCLUDE_DIRECTORIES> ${CMAKE_SOURCE_DIR}/tests Source)
    target_compile_definitions(gristo_plugin_tests PRIVATE $<TARGET_PROPERTY:Gristo,COMPILE_DEFINITIONS>)
    target_link_libraries(gristo_plugin_tests PRIVATE Gristo gristo_core gristo_sound)
    add_test(NAME gristo_plugin_tests COMMAND gristo_plugin_tests)
endif()
