#=============================================================================
# Hemispheric Brain Architecture - Build Configuration
#=============================================================================
# Biologically-inspired two-hemisphere brain with corpus callosum
# Implements lateralized processing, split-brain mode, and per-hemisphere resources
#=============================================================================

set(HEMISPHERIC_BRAIN_SOURCES
    nimcp_lateralization.c
    nimcp_corpus_callosum.c
    nimcp_brain_hemisphere.c
    nimcp_hemispheric_brain.c
)

# Create object library for inclusion in main nimcp lib
add_library(nimcp_hemispheric_brain_obj OBJECT ${HEMISPHERIC_BRAIN_SOURCES})

target_include_directories(nimcp_hemispheric_brain_obj PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/include/core/brain
    ${CMAKE_SOURCE_DIR}/include/core/brain/hemispheric
)

# Compiler flags
target_compile_options(nimcp_hemispheric_brain_obj PRIVATE
    -Wall
    -Wextra
    $<$<CONFIG:Debug>:-g3 -O0>
    $<$<CONFIG:Release>:-O3>
)

message(STATUS "Configured Hemispheric Brain Architecture (4 modules)")
