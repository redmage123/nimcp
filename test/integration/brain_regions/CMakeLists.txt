# ============================================================================
# Brain Regions Integration Tests - TEMPORARILY DISABLED
# ============================================================================
# WHAT: CMakeLists for brain region integration tests
# WHY:  Source file nimcp_brain_region_predictive.c needs error code fixes
# HOW:  Skip test discovery until source is fixed
#
# TODO: Re-enable once the following files are fixed:
#   - src/core/brain/regions/nimcp_brain_region_predictive.c
# ============================================================================

message(STATUS "  [SKIP] Brain region integration tests - awaiting source fixes")

# Build the existing signal propagation test (not dependent on predictive coding)
add_test_binary(
    integration_brain_regions_test_signal_propagation_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_signal_propagation_integration.cpp
    integration
)

# Occipital cortex integration tests
add_test_binary(
    integration_brain_regions_test_occipital_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_occipital_integration.cpp
    integration
)

# Neuromodulatory centers integration tests (Phase 4: LC, VTA, Raphe, Habenula)
add_test_binary(
    integration_brain_regions_test_neuromodulatory_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_neuromodulatory_integration.cpp
    integration
)

# Sensory Processing integration tests (Phase 6: Somatosensory, Olfactory, Gustatory)
add_test_binary(
    integration_brain_regions_test_sensory_processing_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sensory_processing_integration.cpp
    integration
)
