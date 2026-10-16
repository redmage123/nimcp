# ============================================================================
# Cognitive Fault Tolerance E2E Tests
# ============================================================================
# WHAT: End-to-end tests for fault tolerance pipeline
# WHY:  Verify complete health monitoring → diagnostic → self-repair workflows
# HOW:  GTest-based tests for realistic fault tolerance scenarios
# ============================================================================

# Find test sources
file(GLOB FT_E2E_SOURCES "*.cpp")

# Guard: Check if test sources exist
if(NOT FT_E2E_SOURCES)
    message(STATUS "No cognitive fault tolerance E2E tests found")
    return()
endif()

# Create test executable for each test file
foreach(TEST_FILE ${FT_E2E_SOURCES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    set(TARGET_NAME "e2e_cognitive_fault_tolerance_${TEST_NAME}")

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
        LABELS "e2e;cognitive;fault_tolerance"
        TIMEOUT 300
    )
endforeach()

message(STATUS "Added cognitive fault tolerance E2E tests")
