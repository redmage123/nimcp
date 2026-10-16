# Thread Safety Regression Tests (P0-P3 audit fixes)

macro(add_thread_safety_regression_test TEST_NAME)
    add_executable(regression_thread_safety_${TEST_NAME} ${TEST_NAME}.cpp)
    target_link_libraries(regression_thread_safety_${TEST_NAME} PRIVATE GTest::GTest GTest::Main nimcp pthread m)
    target_include_directories(regression_thread_safety_${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME regression_thread_safety_${TEST_NAME} COMMAND regression_thread_safety_${TEST_NAME})
    set_tests_properties(regression_thread_safety_${TEST_NAME} PROPERTIES TIMEOUT 120)
endmacro()

# P0-2: Thread-local error buffer regression
add_thread_safety_regression_test(test_thread_local_error_buffer)

# P0-3: Callback wrapper mutex regression
add_thread_safety_regression_test(test_callback_wrapper_mutex)
