# Platform Utils Integration Tests
# Phase: Portia Optimization (Platform Tier System)

# Helper macro to add platform integration test
macro(add_platform_integration_test TEST_NAME)
    add_executable(integration_utils_platform_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_utils_platform_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME integration_utils_platform_${TEST_NAME}
             COMMAND integration_utils_platform_${TEST_NAME})

    # Set timeout for integration tests (longer than unit tests)
    set_tests_properties(integration_utils_platform_${TEST_NAME}
        PROPERTIES TIMEOUT 300)  # 5 minutes
endmacro()

# Platform tier integration tests
# TODO: Fix these tests to use correct brain API
# add_platform_integration_test(test_platform_tier_integration)
# add_platform_integration_test(test_platform_tier_brain_integration)
