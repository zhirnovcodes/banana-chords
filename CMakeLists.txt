cmake_minimum_required(VERSION 3.25)

# macOS: one universal build for Apple Silicon and Intel. Must be set before project(). Other platforms ignore these.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# Same as VERSION in apps/web/src/manifest.ts.
project(BananaChords VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The core does not throw or use RTTI. JUCE does, so these flags stay on core targets only.
function(bc_core_flags target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /GR- /EHs-c-)
    else()
        target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti)
    endif()
endfunction()

# params.json is the one parameter list. CMake writes it into a header the engine reads
# (defaults, ranges, kinds). Editing params.json re-runs configure.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/params.json)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/params.json BC_PARAMS_JSON)
string(JSON bc_param_count LENGTH "${BC_PARAMS_JSON}" params)
math(EXPR bc_param_last "${bc_param_count} - 1")
set(bc_param_rows "")
foreach(index RANGE ${bc_param_last})
    string(JSON param_id GET "${BC_PARAMS_JSON}" params ${index} id)
    string(JSON param_kind GET "${BC_PARAMS_JSON}" params ${index} kind)
    string(JSON param_min GET "${BC_PARAMS_JSON}" params ${index} min)
    string(JSON param_max GET "${BC_PARAMS_JSON}" params ${index} max)
    string(JSON param_default GET "${BC_PARAMS_JSON}" params ${index} default)
    string(APPEND bc_param_rows "    {\"${param_id}\", \"${param_kind}\", ${param_min}, ${param_max}, ${param_default}},\n")
endforeach()
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/ParamsJson.h CONTENT
"#pragma once
// Written by CMake from params.json. Do not edit.

namespace bc {

struct ParamSpec {
    const char* id;
    const char* kind;
    double min;
    double max;
    double def;
};

inline constexpr ParamSpec kParams[] = {
${bc_param_rows}};

} // namespace bc
" @ONLY)

add_library(bc_domain
    core/domain/Sequences.cpp
    core/domain/Harmony.cpp
)
target_include_directories(bc_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core/domain)
bc_core_flags(bc_domain)

add_library(bc_engine core/engine/Engine.cpp)
target_include_directories(bc_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/engine
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(bc_engine PUBLIC bc_domain)
bc_core_flags(bc_engine)

add_executable(bc_tests tests/main.cpp tests/sequences_tests.cpp tests/harmony_tests.cpp tests/engine_tests.cpp)
target_link_libraries(bc_tests PRIVATE bc_engine)
bc_core_flags(bc_tests)

if(EMSCRIPTEN)
    # Standalone module: instantiated directly, no Emscripten JS glue.
    add_executable(banana-chords hosts/web/boundary/api.cpp)
    target_link_libraries(banana-chords PRIVATE bc_engine)
    bc_core_flags(banana-chords)
    set_target_properties(banana-chords PROPERTIES SUFFIX ".wasm")
    target_link_options(banana-chords PRIVATE
        -O2 --no-entry -sSTANDALONE_WASM -sALLOW_MEMORY_GROWTH -sFILESYSTEM=0
        -sEXPORTED_FUNCTIONS=_bc_alloc,_bc_free,_bc_create,_bc_destroy,_bc_set_param,_bc_param,_bc_transport,_bc_midi,_bc_all_off,_bc_process,_bc_out_count,_bc_out_frame,_bc_out_kind,_bc_out_bytes,_bc_out_slot,_bc_out_value,_bc_seed,_bc_set_seed,_bc_chord_count,_bc_chord_start,_bc_chord_length,_bc_chord_notes,_bc_chord_note,_bc_current_chord
    )
    # Core tests run under Node in the wasm build too: node build-wasm/bc_tests.js
    set_target_properties(bc_tests PROPERTIES SUFFIX ".js")
else()
    # JUCE targets (VST3, AU MIDI FX, Standalone) arrive in v0.1.0, see docs/plugin-dev/04-vst-host.md in ivan-daw.
endif()
