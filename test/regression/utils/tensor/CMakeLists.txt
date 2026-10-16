# Tensor Utils Regression Tests
# Performance and stability regression tests for tensor module

# Helper macro to add tensor regression test
macro(add_tensor_regression_test TEST_NAME)
    add_executable(regression_utils_tensor_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(regression_utils_tensor_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    # Optimize for performance testing
    target_compile_options(regression_utils_tensor_${TEST_NAME}
        PRIVATE -O2 -march=native)
    add_test(NAME regression_utils_tensor_${TEST_NAME}
             COMMAND regression_utils_tensor_${TEST_NAME})
    # Set longer timeout for regression tests
    set_tests_properties(regression_utils_tensor_${TEST_NAME}
        PROPERTIES TIMEOUT 300)
endmacro()

# Performance regression tests
add_tensor_regression_test(test_tensor_performance)

# Tensor pool regression tests (Opt 4)
add_tensor_regression_test(test_tensor_pool_regression)
