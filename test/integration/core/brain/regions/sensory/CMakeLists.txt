# ============================================================================
# Sensory Modules Integration Tests
# ============================================================================
# WHAT: Build configuration for Phase 6 sensory module integration tests
# WHY:  Test interactions between somatosensory, olfactory, and gustatory
#       modules with bio-async, quantum bridges, brainstem, KG wiring, and
#       cross-sensory chemosensory/tactile-motor integration
# HOW:  Individual executables for each integration test suite

# Helper macro for sensory integration tests
macro(add_sensory_integration_test TEST_NAME)
    add_executable(integration_core_brain_regions_sensory_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_core_brain_regions_sensory_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(integration_core_brain_regions_sensory_${TEST_NAME}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(integration_core_brain_regions_sensory_${TEST_NAME}
        PRIVATE cxx_std_17
    )
    target_compile_definitions(integration_core_brain_regions_sensory_${TEST_NAME}
        PRIVATE NIMCP_TESTING
    )
    add_test(NAME integration_core_brain_regions_sensory_${TEST_NAME}
             COMMAND integration_core_brain_regions_sensory_${TEST_NAME})
    set_tests_properties(integration_core_brain_regions_sensory_${TEST_NAME}
        PROPERTIES
            LABELS "integration;core;brain;sensory"
            TIMEOUT 180
    )
endmacro()

# Comprehensive sensory integration tests (Phase 6)
# Tests somatosensory, olfactory, gustatory with:
# - Bio-async bridges
# - Quantum bridges
# - Brainstem bridges (cerebellum, medulla)
# - Cross-sensory bridges (chemosensory, tactile-motor)
# - Knowledge graph wiring
# - Swarm integration
add_sensory_integration_test(test_sensory_integration)
