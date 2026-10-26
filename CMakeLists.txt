# =================================================================================================
#  Keys — top-level build script
#
#  Quick start (Windows, from this folder):
#      cmake --preset vs2026            # configure: generate build/vs2026/Keys.slnx
#      cmake --build --preset release   # compile the Standalone app, the VST3 and the tests
#      ctest --preset release           # run the unit tests
#
#  The bell samples live in assets/bell (fetch them once with scripts/fetch-bell-samples.ps1).
#  Without them Keys still plays: the bell layer falls back to a modeled bell.
# =================================================================================================
cmake_minimum_required(VERSION 3.25)

project(Keys VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(KEYS_BUILD_TESTS "Build the Catch2 unit tests" ON)
option(KEYS_BUILD_UI_SNAPSHOT "Build the KeysUiSnapshot dev tool" ON)

# -------------------------------------------------------------------------------------------------
#  JUCE: prefer the checkout next to this repo (../JUCE); otherwise download the pinned version.
# -------------------------------------------------------------------------------------------------
set(KEYS_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Path to a local JUCE checkout")

if(EXISTS "${KEYS_JUCE_DIR}/CMakeLists.txt")
    message(STATUS "Keys: using local JUCE at ${KEYS_JUCE_DIR}")
    add_subdirectory("${KEYS_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    message(STATUS "Keys: local JUCE not found, fetching JUCE 8.0.12 from GitHub")
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(juce)
endif()

# -------------------------------------------------------------------------------------------------
#  Fonts (SIL OFL), compiled into the binary.
# -------------------------------------------------------------------------------------------------
juce_add_binary_data(KeysFonts
    NAMESPACE KeysFonts
    SOURCES
        assets/fonts/ArchivoExpanded-Bold.ttf
        assets/fonts/ArchivoExpanded-ExtraBold.ttf
        assets/fonts/ArchivoSemiExpanded-SemiBold.ttf)

# -------------------------------------------------------------------------------------------------
#  KeysEngine: the sampler, the tine model and the sample libraries, as an INTERFACE library.
#  The plugin and the unit tests both compile these sources, so the tested engine is the shipped one.
# -------------------------------------------------------------------------------------------------
add_library(KeysEngine INTERFACE)

target_sources(KeysEngine INTERFACE
    source/engine/BankBuilder.cpp
    source/engine/DemoLibrary.cpp
    source/engine/KeysEngine.cpp
    source/engine/KeysLibrary.cpp
    source/engine/SampleBank.cpp
    source/engine/TineModel.cpp
)

target_include_directories(KeysEngine INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/source")

target_link_libraries(KeysEngine INTERFACE
    juce::juce_audio_basics
    juce::juce_audio_formats)

target_compile_definitions(KeysEngine INTERFACE
    KEYS_DEV_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

# -------------------------------------------------------------------------------------------------
#  The plugin: one shared-code target (Keys) plus a thin wrapper per format (Keys_Standalone, ...).
# -------------------------------------------------------------------------------------------------
set(KEYS_FORMATS Standalone VST3)
if(APPLE)
    list(APPEND KEYS_FORMATS AU)
endif()

juce_add_plugin(Keys
    COMPANY_NAME "ChrisDrae"
    PRODUCT_NAME "Keys"
    VERSION "${PROJECT_VERSION}"
    PLUGIN_MANUFACTURER_CODE Cdra
    PLUGIN_CODE Tine
    FORMATS ${KEYS_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

add_library(KeysPluginSources INTERFACE)
target_sources(KeysPluginSources INTERFACE
    source/Parameters.cpp
    source/PluginEditor.cpp
    source/PluginProcessor.cpp
    source/ui/Backdrop.cpp
    source/ui/KeysLookAndFeel.cpp
)

target_compile_definitions(Keys PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Keys
    PRIVATE
        KeysPluginSources
        KeysEngine
        KeysFonts
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# -------------------------------------------------------------------------------------------------
#  Unit tests (Catch2 v3). They link KeysEngine only, so they run without a host or a GUI.
#  Reuses Pocket Looper's downloaded Catch2 when it's there, to skip the download.
# -------------------------------------------------------------------------------------------------
if(KEYS_BUILD_TESTS)
    include(FetchContent)
    set(_keys_local_catch2 "${CMAKE_CURRENT_SOURCE_DIR}/../plugin/build/vs2026/_deps/catch2-src")
    if(EXISTS "${_keys_local_catch2}/CMakeLists.txt" AND NOT DEFINED FETCHCONTENT_SOURCE_DIR_CATCH2)
        set(FETCHCONTENT_SOURCE_DIR_CATCH2 "${_keys_local_catch2}")
    endif()
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.16.0
        GIT_SHALLOW ON
        SYSTEM)
    FetchContent_MakeAvailable(Catch2)

    enable_testing()

    add_executable(KeysTests
        tests/BankTests.cpp
        tests/EngineTests.cpp
        tests/NoteNameTests.cpp
        tests/TineModelTests.cpp
    )

    target_link_libraries(KeysTests PRIVATE
        KeysEngine
        Catch2::Catch2WithMain
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    catch_discover_tests(KeysTests)
endif()

# -------------------------------------------------------------------------------------------------
#  UI snapshot tool: renders the plugin window into a PNG, without a host.
#      build/vs2026/KeysUiSnapshot_artefacts/Release/KeysUiSnapshot.exe ui.png [--demo]
# -------------------------------------------------------------------------------------------------
if(KEYS_BUILD_UI_SNAPSHOT)
    juce_add_console_app(KeysUiSnapshot PRODUCT_NAME "KeysUiSnapshot")
    target_sources(KeysUiSnapshot PRIVATE tests/UiSnapshot.cpp)
    target_compile_definitions(KeysUiSnapshot PRIVATE
        JucePlugin_Name="Keys"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(KeysUiSnapshot PRIVATE
        KeysPluginSources
        KeysEngine
        KeysFonts
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
