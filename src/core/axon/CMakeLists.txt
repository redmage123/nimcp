# ============================================================================
# NIMCP Axon Module CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for axon signal propagation module
# WHY:  Enable action potential modeling and myelination integration
# HOW:  Compile as part of nimcp library
# ============================================================================

# Source files
set(AXON_SOURCES
    nimcp_axon.c
)

# Add to parent scope for inclusion in nimcp library
set(NIMCP_AXON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_axon.c PARENT_SCOPE)
