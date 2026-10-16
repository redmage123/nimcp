# Broca Region Integration Tests

# Helper macro for broca integration tests
macro(add_broca_integration_test TEST_NAME)
    add_executable(integration_core_brain_regions_broca_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_core_brain_regions_broca_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME integration_core_brain_regions_broca_${TEST_NAME}
             COMMAND integration_core_brain_regions_broca_${TEST_NAME})
endmacro()

# Broca bridge integration tests
add_broca_integration_test(test_nimcp_broca_bridge_integration)
add_broca_integration_test(test_nimcp_broca_full_pipeline)

# Speech Enhancement integration tests (Phase B4)
add_broca_integration_test(test_nimcp_speech_enhancement_integration)
