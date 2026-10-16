# Logging Utils Unit Tests
# Phase LOG-1: Enhanced Logging System

# Helper macro to add logging test
macro(add_logging_test TEST_NAME)
    add_executable(unit_utils_logging_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_utils_logging_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_utils_logging_${TEST_NAME}
             COMMAND unit_utils_logging_${TEST_NAME})
endmacro()

# Enhanced Logging Unit Tests
add_logging_test(test_logging_basic)
add_logging_test(test_logging_levels)
add_logging_test(test_logging_async)
add_logging_test(test_logging_rotation)
add_logging_test(test_logging_format)
add_logging_test(test_logging_rate_limit)
add_logging_test(test_logging_filter)
add_logging_test(test_logging_context)
add_logging_test(test_logging_stats)
add_logging_test(test_logging_thread_safety)
