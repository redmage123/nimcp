# ============================================================================
# LNN (Liquid Neural Network) Library CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for LNN library
# WHY:  Provides biologically-inspired liquid time-constant neural networks
# HOW:  Compile C sources into static library with SIMD optimizations
# ============================================================================

# LNN source files
set(LNN_SOURCES
    nimcp_lnn.c
    nimcp_lnn_config.c
    nimcp_lnn_neuron.c
    nimcp_lnn_layer.c
    nimcp_lnn_network.c
    nimcp_lnn_ode.c
    nimcp_lnn_wiring.c
    nimcp_lnn_gradient.c
    nimcp_lnn_training.c
    nimcp_lnn_bio_async.c
    nimcp_lnn_immune.c
    nimcp_lnn_parallel.c
    nimcp_lnn_sleep_bridge.c
)

# Create static library
add_library(nimcp_lnn STATIC ${LNN_SOURCES})

# Public include directories
target_include_directories(nimcp_lnn
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/lnn
)

# Link dependencies
target_link_libraries(nimcp_lnn
    PUBLIC
        nimcp
        Threads::Threads
        m
)

# Require C11 standard
target_compile_features(nimcp_lnn PUBLIC c_std_11)

# Compiler warnings
if(MSVC)
    target_compile_options(nimcp_lnn PRIVATE /W4)
else()
    target_compile_options(nimcp_lnn PRIVATE -Wall -Wextra -Wpedantic)
endif()

# SIMD optimizations (AVX2 for vectorized ODE solving)
if(NOT MSVC)
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
    if(COMPILER_SUPPORTS_AVX2)
        target_compile_options(nimcp_lnn PRIVATE -mavx2)
        message(STATUS "LNN: Enabling AVX2 SIMD optimizations")
    endif()
endif()

message(STATUS "LNN library configured")
