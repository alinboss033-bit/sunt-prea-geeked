cmake_minimum_required(VERSION 3.22)
project(SuntPreaGeeked VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 7.0.12
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

function(geeked_plugin target product code src)
  juce_add_plugin(${target}
    COMPANY_NAME "Geeked"
    PLUGIN_MANUFACTURER_CODE Gkdp
    PLUGIN_CODE ${code}
    FORMATS VST3
    PRODUCT_NAME "${product}"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx)
  target_sources(${target} PRIVATE ${src})
  target_compile_definitions(${target} PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_compile_options(${target} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
  target_link_libraries(${target} PRIVATE
    juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endfunction()

geeked_plugin(SPG_Voce   "Sunt prea geeked Voce"   Gkv1 Source/Voice.cpp)
geeked_plugin(SPG_Reverb "Sunt prea geeked Reverb" Gkr1 Source/Reverb.cpp)
geeked_plugin(SPG_Delay  "Sunt prea geeked Delay"  Gkd1 Source/Delay.cpp)
