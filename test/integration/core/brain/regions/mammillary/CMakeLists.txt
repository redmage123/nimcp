# Mammillary Bodies Integration Tests
# Test Mammillary integration with various subsystems

# Create macro for mammillary integration tests
macro(add_mammillary_integration_test TEST_NAME)
    add_executable(integration_core_brain_regions_mammillary_${TEST_NAME} ${TEST_NAME}.cpp)
    target_link_libraries(integration_core_brain_regions_mammillary_${TEST_NAME}
        PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
    )
    target_include_directories(integration_core_brain_regions_mammillary_${TEST_NAME}
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    add_test(
        NAME integration_core_brain_regions_mammillary_${TEST_NAME}
        COMMAND integration_core_brain_regions_mammillary_${TEST_NAME}
    )
    set_tests_properties(integration_core_brain_regions_mammillary_${TEST_NAME}
        PROPERTIES
        TIMEOUT 60
        LABELS "integration;brain;mammillary"
    )
endmacro()

# Add integration tests
add_mammillary_integration_test(test_mammillary_brain_init_integration)
add_mammillary_integration_test(test_mammillary_bio_async_integration)
add_mammillary_integration_test(test_mammillary_snn_plasticity_integration)
add_mammillary_integration_test(test_mammillary_cognitive_integration)
add_mammillary_integration_test(test_mammillary_immune_integration)
add_mammillary_integration_test(test_mammillary_security_integration)
add_mammillary_integration_test(test_mammillary_kg_wiring_integration)
