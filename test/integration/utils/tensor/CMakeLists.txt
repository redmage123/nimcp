# Tensor Utils Integration Tests
# Integration tests for tensor module with memory, neural networks, and calculus

# Helper macro to add tensor integration test
macro(add_tensor_integration_test TEST_NAME)
    add_executable(integration_utils_tensor_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_utils_tensor_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME integration_utils_tensor_${TEST_NAME}
             COMMAND integration_utils_tensor_${TEST_NAME})
endmacro()

# Memory and pipeline integration tests
add_tensor_integration_test(test_tensor_memory_integration)
