find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_target(Synth1XNamingLint
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/lint_naming.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
   AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"
   AND CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(synth1x_header_audit_supported ON)
    add_custom_target(Synth1XHeaderAudit
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/audit_dsp_headers.py
            --build-dir ${CMAKE_BINARY_DIR} --compiler ${CMAKE_CXX_COMPILER}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM)
endif()
set(core_generated_dir ${CMAKE_CURRENT_BINARY_DIR}/generated/core)
add_custom_command(OUTPUT ${core_generated_dir}/CoreResources.cpp ${core_generated_dir}/CoreResources.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_core_resources.py
        ${CMAKE_CURRENT_SOURCE_DIR} ${core_generated_dir}
    DEPENDS tools/embed_core_resources.py resources/init.sy1 resources/parameter_names.json
    VERBATIM)
add_library(Synth1XResources STATIC ${core_generated_dir}/CoreResources.cpp)
target_include_directories(Synth1XResources PUBLIC ${core_generated_dir})
target_compile_features(Synth1XResources PRIVATE cxx_std_20)
set_target_properties(Synth1XResources PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
add_library(Synth1XZip STATIC third_party/miniz/miniz.c third_party/miniz/miniz_tdef.c
    third_party/miniz/miniz_tinfl.c third_party/miniz/miniz_zip.c)
target_include_directories(Synth1XZip PUBLIC third_party/miniz)
set_target_properties(Synth1XZip PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Synth1XXml STATIC third_party/tinyxml2/tinyxml2.cpp)
target_include_directories(Synth1XXml PUBLIC third_party/tinyxml2)
target_compile_features(Synth1XXml PUBLIC cxx_std_11)
set_target_properties(Synth1XXml PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
include(cmake/Synth1PatchCore.cmake)
include(cmake/Synth1XArchives.cmake)
set(synth1x_core_sources
    projects/Synth1X/src/SynthEngine.cpp projects/Synth1X/src/VoiceDispatcher.cpp
    projects/Synth1X/src/SynthVoice.cpp projects/Synth1X/src/VoiceCorePool.cpp projects/Synth1X/src/SynthDiagnostics.cpp projects/Synth1X/src/dsp/OutputPipeline.cpp projects/Synth1X/src/dsp/SharedModulationRuntime.cpp
    projects/Synth1X/src/render/RenderPolicy.cpp projects/Synth1X/src/patch/PatchLoader.cpp projects/Synth1X/src/patch/PatchMapper.cpp projects/Synth1X/src/patch/Synth1PatchMapper.cpp
    projects/Synth1X/src/patch/PatchFormat.cpp
    projects/Synth1X/src/host/ProcessorBridge.cpp projects/Synth1X/src/host/ProcessorState.cpp projects/Synth1X/src/host/PatchLibrary.cpp
    projects/Synth1X/src/host/EditorPaletteStore.cpp projects/Synth1X/src/host/PatchComparison.cpp projects/Synth1X/src/host/PatchTagInference.cpp projects/Synth1X/src/host/PatchCatalog.cpp projects/Synth1X/src/host/PresetFiles.cpp
    projects/Synth1X/src/host/PresetSession.cpp)
function(synth1x_add_core target)
    add_library(${target} STATIC ${ARGN} ${synth1x_core_sources})
    target_include_directories(${target} PUBLIC projects/Synth1X/src projects/Synth1X/src/host)
    target_link_libraries(${target} PUBLIC Synth1PatchCore PRIVATE Synth1XResources Synth1XZip Synth1XXml Synth1XArchives)
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
endfunction()
synth1x_add_core(Synth1XCore)
