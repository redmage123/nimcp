# Platform Utils Regression Tests
# Phase: Portia Optimization (Platform Tier System)

# Helper macro to add platform regression test
macro(add_platform_regression_test TEST_NAME)
    add_executable(regression_utils_platform_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(regression_utils_platform_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME regression_utils_platform_${TEST_NAME}
             COMMAND regression_utils_platform_${TEST_NAME})

    # Set timeout for regression tests (can be lengthy)
    set_tests_properties(regression_utils_platform_${TEST_NAME}
        PROPERTIES TIMEOUT 600)  # 10 minutes

    # Label as regression test
    set_tests_properties(regression_utils_platform_${TEST_NAME}
        PROPERTIES LABELS "regression")
endmacro()

# Platform tier regression tests (memory budgets, performance)
add_platform_regression_test(test_platform_tier_regression)
