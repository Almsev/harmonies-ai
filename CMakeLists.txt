cmake_minimum_required(VERSION 3.10)
project(harminies C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build options
option(BUILD_WASM "Build WebAssembly target" ON)
option(BUILD_HARMINIES "Build the harminies C executables" ON)
option(ENABLE_PROFILING "Enable profiling with gprof" OFF)
option(ENABLE_PERF "Enable performance profiling (debug symbols, no omit frame pointer)" OFF)
option(BUILD_DEBUG "Build debug version with sanitizers" OFF)

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Detect if we're building with Emscripten
if(EMSCRIPTEN)
    set(BUILD_WASM ON)
    set(BUILD_HARMINIES OFF)
    message(STATUS "Emscripten detected - building WASM target only")
endif()

# WASM build
if(BUILD_WASM AND EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".js")
    
    add_executable(harminies harminies.c)
    
    # Emscripten-specific settings - must be set this way for proper parsing
    target_compile_options(harminies PRIVATE
        -O3
    )
    
    target_link_options(harminies PRIVATE
        -sWASM=1
        -sEXPORTED_FUNCTIONS=['_wasm_init_game','_wasm_get_actions','_wasm_do_action','_wasm_player_score','_wasm_get_ai_action','_wasm_get_state','_wasm_get_animal','_malloc','_free']
        -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue','UTF8ToString','HEAP8','HEAP16','HEAP32','HEAPU8','HEAPU16','HEAPU32']
        -sALLOW_MEMORY_GROWTH=1
        -sMODULARIZE=1
        -sEXPORT_NAME=HarminiesModule
        -sENVIRONMENT=web,worker
        -sEXPORT_ES6=0
        -sINITIAL_MEMORY=16777216
        --no-entry
    )
endif()

# Native C builds (controlled by option, not available with Emscripten)
if(BUILD_HARMINIES AND NOT EMSCRIPTEN)
    add_executable(harminies_test harminies.c)

    # Profiling build (gprof)
    if(ENABLE_PROFILING)
        add_executable(harminies_test_profile harminies.c)
        target_compile_options(harminies_test_profile PRIVATE -pg -O2)
        target_link_options(harminies_test_profile PRIVATE -pg)
    endif()

    # Perf-friendly build (for perf record/report)
    if(ENABLE_PERF)
        add_executable(harminies_test_perf harminies.c)
        target_compile_options(harminies_test_perf PRIVATE -O2 -g -fno-omit-frame-pointer)
    endif()

    # Debug build with sanitizers
    if(BUILD_DEBUG)
        add_executable(harminies_test_debug harminies.c)
        target_compile_options(harminies_test_debug PRIVATE 
            -g3 -O0 -fno-omit-frame-pointer
            -fsanitize=address -fsanitize=undefined
            -fno-sanitize-recover=all
        )
        target_link_options(harminies_test_debug PRIVATE 
            -fsanitize=address -fsanitize=undefined
        )
    endif()

    # Optimized build of the test executable
    add_executable(harminies_test_optimized harminies.c)
    target_compile_definitions(harminies_test_optimized PRIVATE OPTIMIZED)

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(harminies_test_optimized PRIVATE -O3 -march=native -flto -fstrict-aliasing)
    elseif(MSVC)
        target_compile_options(harminies_test_optimized PRIVATE /O2 /GL)
    endif()
endif()
