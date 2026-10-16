# ============================================================================
# Predictive Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for predictive bridge exception propagation and recovery
# WHY:  Verify cross-bridge exception handling and recovery scenarios
# HOW:  GTest-based tests for multi-bridge workflows with exception injection
# ============================================================================

# Find test sources
file(GLOB PREDICTIVE_INTEGRATION_TEST_SOURCES "*.cpp")

# Guard: Check if test sources exist
if(NOT PREDICTIVE_INTEGRATION_TEST_SOURCES)
    message(STATUS "No predictive bridge integration tests found")
    return()
endif()

# Create test executable for each test file
foreach(TEST_FILE ${PREDICTIVE_INTEGRATION_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    set(TARGET_NAME "integration_cognitive_predictive_${TEST_NAME}")

    add_executable(${TARGET_NAME} ${TEST_FILE})

    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
    )

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            Threads::Threads
            m
    )

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)
    target_compile_definitions(${TARGET_NAME} PRIVATE NIMCP_TESTING)

    # Register test with CTest
    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
    set_tests_properties(${TARGET_NAME} PROPERTIES
        LABELS "integration;cognitive;predictive;exception"
        TIMEOUT 180
    )
endforeach()

message(STATUS "Added predictive bridge integration tests")
