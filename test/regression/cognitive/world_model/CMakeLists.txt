# ============================================================================
# World Model Regression Tests
# ============================================================================
# WHAT: Regression tests for world model edge cases and stability
# WHY:  Verify NULL handling, boundary conditions, numerical stability,
#       and memory management under stress
# HOW:  GTest-based tests for robustness
# ============================================================================

# Find test sources
file(GLOB WM_REGRESSION_SOURCES "*.cpp")

# Guard: Check if test sources exist
if(NOT WM_REGRESSION_SOURCES)
    message(STATUS "No world model regression tests found")
    return()
endif()

# Create test executable for each test file
foreach(TEST_FILE ${WM_REGRESSION_SOURCES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    set(TARGET_NAME "regression_cognitive_world_model_${TEST_NAME}")

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
        LABELS "regression;cognitive;world_model"
        TIMEOUT 180
    )
endforeach()

message(STATUS "Added world model regression tests")
