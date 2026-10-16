# ============================================================================
# OFC (Orbitofrontal Cortex) Integration Tests
# ============================================================================
# WHAT: Build configuration for OFC module integration tests
# WHY:  Test value-based decision making and reward processing
# HOW:  Individual executables for each integration test suite

# Helper macro for OFC integration tests
macro(add_ofc_integration_test TEST_NAME)
    add_executable(integration_core_brain_regions_ofc_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_core_brain_regions_ofc_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(integration_core_brain_regions_ofc_${TEST_NAME}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(integration_core_brain_regions_ofc_${TEST_NAME}
        PRIVATE cxx_std_17
    )
    target_compile_definitions(integration_core_brain_regions_ofc_${TEST_NAME}
        PRIVATE NIMCP_TESTING
    )
    add_test(NAME integration_core_brain_regions_ofc_${TEST_NAME}
             COMMAND integration_core_brain_regions_ofc_${TEST_NAME})
    set_tests_properties(integration_core_brain_regions_ofc_${TEST_NAME}
        PROPERTIES
            LABELS "integration;core;brain;ofc"
            TIMEOUT 180
    )
endmacro()

# Brain init integration tests - Tests OFC initialization and lifecycle
add_ofc_integration_test(test_ofc_brain_init_integration)

# Decision integration tests - Tests value-based decision making
add_ofc_integration_test(test_ofc_decision_integration)

# Bio-async integration tests - Tests bio-async messaging system
add_ofc_integration_test(test_ofc_bio_async_integration)
