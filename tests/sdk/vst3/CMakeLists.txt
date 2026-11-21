cmake_minimum_required(VERSION 3.25)
project(Synth1XVst3PatchTest LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
get_filename_component(SYNTH1X_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(SYNTH1X_BUILD_IPLUG2 OFF CACHE BOOL "" FORCE)
set(SYNTH1X_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SYNTH1X_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory("${SYNTH1X_ROOT}" core EXCLUDE_FROM_ALL)
set(IPLUG2_DIR "${SYNTH1X_ROOT}/iPlug2" CACHE PATH "Pinned iPlug2 checkout")
set(IPLUG_DIR "${IPLUG2_DIR}/IPlug")
set(WDL_DIR "${IPLUG2_DIR}/WDL")
set(IPLUG_DEPS_DIR "${IPLUG2_DIR}/Dependencies/IPlug")
set(IPLUG2_CXX_STANDARD 20)
include("${IPLUG2_DIR}/Scripts/cmake/VST3.cmake")
if(NOT IPLUG2_VST3_SUPPORTED)
    message(FATAL_ERROR "Prepare the pinned SDKs with tools/prepare_native_sdks.py first")
endif()
# Use the actual VST3/API sources with the SDK's supported headless delegate.
get_target_property(vst3_defines iPlug2::VST3 INTERFACE_COMPILE_DEFINITIONS)
list(REMOVE_ITEM vst3_defines IPLUG_EDITOR=1)
set_target_properties(iPlug2::VST3 PROPERTIES INTERFACE_COMPILE_DEFINITIONS "${vst3_defines}")
add_executable(Synth1XVst3PatchTest PatchNotificationTest.cpp "${SYNTH1X_ROOT}/projects/Synth1X/Synth1X.cpp"
    "${SYNTH1X_ROOT}/projects/Synth1X/Synth1XVst3.cpp"
    "${VST3_SDK_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp")
target_compile_features(Synth1XVst3PatchTest PRIVATE cxx_std_20)
target_compile_definitions(Synth1XVst3PatchTest PRIVATE NO_IGRAPHICS IPLUG_EDITOR=0 _USE_MATH_DEFINES
    VST3_NUM_MIDI_IN_CHANS=16 VST3_NUM_CC_CHANS=16)
target_precompile_headers(Synth1XVst3PatchTest PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/HeadlessEditor.h>")
# Only the Synth1X/test translation units need the editor type declarations.
# Vendor sources manage their own Windows Unicode/UTF-8 include ordering.
get_target_property(vst3_sources iPlug2::VST3 INTERFACE_SOURCES)
get_target_property(iplug_sources iPlug2::IPlug INTERFACE_SOURCES)
set_source_files_properties(${vst3_sources} ${iplug_sources}
    "${VST3_SDK_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp"
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
target_include_directories(Synth1XVst3PatchTest PRIVATE "${SYNTH1X_ROOT}/projects/Synth1X"
    "${CMAKE_CURRENT_BINARY_DIR}/core/generated/version" "${IPLUG2_DIR}/IGraphics")
target_link_libraries(Synth1XVst3PatchTest PRIVATE Synth1XCore iPlug2::VST3)
enable_testing()
add_test(NAME Vst3PatchNotification COMMAND Synth1XVst3PatchTest)
set_tests_properties(Vst3PatchNotification PROPERTIES TIMEOUT 30)
