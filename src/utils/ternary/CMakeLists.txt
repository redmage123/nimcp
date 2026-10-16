#=============================================================================
# CMakeLists.txt - Ternary Logic Module
#=============================================================================
# WHAT: Build configuration for ternary (three-valued) logic module
# WHY:  Enable ternary types, logic, storage, and tensor integration
# HOW:  Compile ternary-tensor bridge into nimcp library
#=============================================================================

# Source files for ternary module
set(TERNARY_SOURCES
    nimcp_ternary_tensor.c
)

# Header files for ternary module
set(TERNARY_HEADERS
    ${CMAKE_SOURCE_DIR}/include/utils/ternary/nimcp_ternary.h
    ${CMAKE_SOURCE_DIR}/include/utils/ternary/nimcp_ternary_types.h
    ${CMAKE_SOURCE_DIR}/include/utils/ternary/nimcp_ternary_logic.h
    ${CMAKE_SOURCE_DIR}/include/utils/ternary/nimcp_ternary_storage.h
    ${CMAKE_SOURCE_DIR}/include/utils/ternary/nimcp_ternary_vector.h
    ${CMAKE_SOURCE_DIR}/include/utils/ternary/nimcp_ternary_matrix.h
    ${CMAKE_SOURCE_DIR}/include/utils/ternary/nimcp_ternary_convert.h
    ${CMAKE_SOURCE_DIR}/include/utils/ternary/nimcp_ternary_tensor.h
)

# Create object library for inclusion in main nimcp library
add_library(nimcp_ternary OBJECT ${TERNARY_SOURCES})

# Include directories
target_include_directories(nimcp_ternary PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Compiler flags
target_compile_options(nimcp_ternary PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -O2
)

# Math library linkage (for sqrt, tanh, etc.)
target_link_libraries(nimcp_ternary PUBLIC m)

# Export sources for parent CMakeLists to include
set(TERNARY_OBJECTS $<TARGET_OBJECTS:nimcp_ternary> PARENT_SCOPE)
