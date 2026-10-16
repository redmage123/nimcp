# ============================================================================
# NIMCP Dendrite Module CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for dendrite computation and integration module
# WHY:  Enable dendritic tree modeling with spines, segments, and plasticity
# HOW:  Compile as part of nimcp library
# ============================================================================

# Source files
set(DENDRITE_SOURCES
    nimcp_dendrite.c
)

# Add to parent scope for inclusion in nimcp library
set(NIMCP_DENDRITE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_dendrite.c PARENT_SCOPE)
