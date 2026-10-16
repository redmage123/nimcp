# ============================================================================
# Predictive Bridge Regression Tests
# ============================================================================
# WHAT: Regression tests for predictive bridge API contract stability
# WHY:  Ensure exception handling behavior does not regress across versions
# HOW:  GTest-based tests for error codes, return values, and API contracts
# ============================================================================

# Find test sources
file(GLOB PREDICTIVE_REGRESSION_TEST_SOURCES "*.cpp")

# Guard: Check if test sources exist
if(NOT PREDICTIVE_REGRESSION_TEST_SOURCES)
    message(STATUS "No predictive bridge regression tests found")
    return()
endif()

# Create test executable for each test file
foreach(TEST_FILE ${PREDICTIVE_REGRESSION_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    set(TARGET_NAME "regression_cognitive_predictive_${TEST_NAME}")

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
        LABELS "regression;cognitive;predictive;exception"
        TIMEOUT 120
    )
endforeach()

message(STATUS "Added predictive bridge regression tests")
