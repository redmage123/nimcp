# Tensor Utils Unit Tests
# Test coverage for nimcp_tensor.h

# Helper macro to add tensor test
macro(add_tensor_test TEST_NAME)
    add_executable(unit_utils_tensor_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_utils_tensor_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_utils_tensor_${TEST_NAME}
             COMMAND unit_utils_tensor_${TEST_NAME})
endmacro()

# Core tensor tests
add_tensor_test(test_tensor)

# Tensor error handling tests
add_tensor_test(test_tensor_errors)

# Tensor pool tests (Opt 4: tensor struct pooling)
add_tensor_test(test_tensor_pool)
