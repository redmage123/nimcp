# Thread Safety Integration Tests (P1-P3 audit fixes)

macro(add_thread_safety_integration_test TEST_NAME)
    add_executable(integration_thread_safety_${TEST_NAME} ${TEST_NAME}.cpp)
    target_link_libraries(integration_thread_safety_${TEST_NAME} PRIVATE GTest::GTest GTest::Main nimcp pthread m)
    target_include_directories(integration_thread_safety_${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME integration_thread_safety_${TEST_NAME} COMMAND integration_thread_safety_${TEST_NAME})
    set_tests_properties(integration_thread_safety_${TEST_NAME} PROPERTIES TIMEOUT 120)
endmacro()

# Concurrent module initialization tests
add_thread_safety_integration_test(test_concurrent_module_init)
