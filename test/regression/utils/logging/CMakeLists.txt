# Logging Utils Regression Tests

macro(add_logging_regression_test TEST_NAME)
    add_executable(regression_utils_logging_${TEST_NAME} ${TEST_NAME}.cpp)
    target_link_libraries(regression_utils_logging_${TEST_NAME}
        PRIVATE GTest::GTest GTest::Main nimcp m pthread)
    add_test(NAME regression_utils_logging_${TEST_NAME}
             COMMAND regression_utils_logging_${TEST_NAME})
endmacro()

add_logging_regression_test(test_logging_performance)
add_logging_regression_test(test_logging_stress)
