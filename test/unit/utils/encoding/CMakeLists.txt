# Positional Encoding Utils Unit Tests
# Test coverage for nimcp_positional_encoding.h

# Helper macro to add encoding test
macro(add_encoding_test TEST_NAME)
    add_executable(unit_utils_encoding_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_utils_encoding_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_utils_encoding_${TEST_NAME}
             COMMAND unit_utils_encoding_${TEST_NAME})
endmacro()

# Positional encoding tests
add_encoding_test(test_positional_encoding)
