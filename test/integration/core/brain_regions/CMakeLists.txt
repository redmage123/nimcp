# ============================================================================
# Brain Regions Integration Tests
# ============================================================================
# WHAT: CMakeLists for brain region integration tests
# WHY:  Test brain region modules working together in hierarchical networks
# HOW:  Define test binaries for predictive coding, signal propagation, etc.
# ============================================================================

message(STATUS "  Brain region integration tests")

# Helper macro to add brain region integration test
macro(add_brain_region_integration_test TEST_NAME)
    add_executable(integration_brain_regions_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_brain_regions_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME integration_brain_regions_${TEST_NAME}
             COMMAND integration_brain_regions_${TEST_NAME})
endmacro()

# Visual cortex integration test (existing)
add_brain_region_integration_test(test_visual_cortex_integration)

# Predictive coding + PE integration test
# DISABLED: Depends on nimcp_brain_region_predictive.c which is disabled for API updates
# add_brain_region_integration_test(test_predictive_pe_integration)

# Cerebellum-vestibular integration test
# DISABLED: Uses deprecated cerebellum API - needs update to current vestibular/cerebellum interface
# add_brain_region_integration_test(test_cerebellum_vestibular_integration)

# Cerebellum-brain integration test (multi-bridge verification)
# DISABLED: Uses deprecated cerebellum API - needs update to current cerebellum interface
# add_brain_region_integration_test(test_cerebellum_brain_integration)

# Neuromodulatory bio-async and cognitive hub integration test
add_brain_region_integration_test(test_neuromodulatory_bio_async_integration)

# Neuromodulatory system integration test (Security, Immune, Logging bridges)
add_brain_region_integration_test(test_neuromodulatory_system_integration)
