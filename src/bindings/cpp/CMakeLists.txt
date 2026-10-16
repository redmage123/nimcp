# ============================================================================
# NIMCP C++ Bindings - Header-only interface library
# ============================================================================
# WHAT: CMake target for idiomatic C++20 wrapper around NIMCP C API
# WHY:  Provides RAII, exceptions, typed enums, and std::function callbacks
# HOW:  INTERFACE library (header-only) that links against the C library
# ============================================================================

add_library(nimcp_cpp INTERFACE)

target_include_directories(nimcp_cpp INTERFACE
    ${CMAKE_SOURCE_DIR}/include/bindings/cpp
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(nimcp_cpp INTERFACE nimcp)

target_compile_features(nimcp_cpp INTERFACE cxx_std_20)
