# Thread Utils Unit Tests (P1-P3 audit fixes)

macro(add_thread_unit_test TEST_NAME)
    add_executable(unit_utils_thread_${TEST_NAME} ${TEST_NAME}.cpp)
    target_link_libraries(unit_utils_thread_${TEST_NAME} PRIVATE GTest::GTest GTest::Main nimcp pthread)
    target_include_directories(unit_utils_thread_${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME unit_utils_thread_${TEST_NAME} COMMAND unit_utils_thread_${TEST_NAME})
    set_tests_properties(unit_utils_thread_${TEST_NAME} PROPERTIES TIMEOUT 120)
endmacro()

# P1 Audit: Mutex wrapper tests
add_thread_unit_test(test_nimcp_mutex_wrapper)
