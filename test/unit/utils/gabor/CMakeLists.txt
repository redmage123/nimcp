# Gabor Filter Unit Tests
# Test coverage for nimcp_gabor.h - Unified Gabor filter library

# Helper macro to add gabor test
macro(add_gabor_test TEST_NAME)
    add_executable(unit_utils_gabor_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_utils_gabor_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_utils_gabor_${TEST_NAME}
             COMMAND unit_utils_gabor_${TEST_NAME})
endmacro()

# Gabor filter tests (50+ tests)
add_gabor_test(test_gabor_filters)
