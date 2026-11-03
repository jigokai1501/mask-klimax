add_executable(dsp_test dsp_test.cpp)
target_compile_features(dsp_test PRIVATE cxx_std_17)

juce_add_console_app(plugin_test PRODUCT_NAME "plugin_test")
target_sources(plugin_test PRIVATE plugin_test.cpp ../Source/PluginProcessor.cpp ../Source/PluginEditor.cpp)
target_compile_definitions(plugin_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JucePlugin_Name="mask::klimax")
target_link_libraries(plugin_test PRIVATE juce::juce_audio_processors juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
