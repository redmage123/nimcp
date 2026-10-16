# Logging Utils Integration Tests

macro(add_logging_integration_test TEST_NAME)
    add_executable(integration_utils_logging_${TEST_NAME} ${TEST_NAME}.cpp)
    target_link_libraries(integration_utils_logging_${TEST_NAME}
        PRIVATE GTest::GTest GTest::Main nimcp m pthread)
    add_test(NAME integration_utils_logging_${TEST_NAME}
             COMMAND integration_utils_logging_${TEST_NAME})
endmacro()

add_logging_integration_test(test_logging_integration)
add_logging_integration_test(test_logging_memory_integration)
add_logging_integration_test(test_logging_security_integration)
