# ============================================================================
# Thalamic Layer Integration Tests
# ============================================================================
#
# WHAT: Integration tests for thalamic nuclei, thalamic router, BG-thalamus bridge,
#       and thalamic-immune bridge
# WHY:  Verify correct signal routing and attention-based gating through thalamus
# HOW:  Single executable combining all thalamic tests, linked with nimcp and GTest
#
# BIOLOGICAL BASIS:
# - Thalamus is the "gateway to cortex" for sensory information
# - Thalamic reticular nucleus (TRN) provides attention-based gating
# - Basal ganglia output routes through VA/VL thalamic nuclei to motor cortex
# - Immune system modulates thalamic gating (hypervigilance, sickness behavior)
#

# Collect all thalamic test source files
set(THALAMIC_TEST_SOURCES
    test_thalamus_integration.cpp
    test_thalamic_router_integration.cpp
    test_basal_ganglia_thalamus_bridge_integration.cpp
    test_thalamic_immune_bridge_integration.cpp
)

# Create single executable for all thalamic tests
add_executable(integration_thalamic
    ${THALAMIC_TEST_SOURCES}
)

target_link_libraries(integration_thalamic
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_thalamic
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

# Configure test discovery with appropriate timeout
# Thalamic tests involve complex cross-module interactions
gtest_discover_tests(integration_thalamic
    PROPERTIES
        LABELS "integration;brain;thalamic"
        TIMEOUT 180
)
