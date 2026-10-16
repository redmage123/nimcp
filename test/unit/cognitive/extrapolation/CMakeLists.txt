# ============================================================================
# Multi-Modal World Model Unit Tests
# ============================================================================
# WHAT: Unit tests for multi-modal world model (sensory fusion)
# WHY:  Verify modality processing, fusion, prediction, and entity tracking
# HOW:  GTest-based tests for all wm_* functions
# ============================================================================

# Find test sources
file(GLOB MM_WM_TEST_SOURCES "*.cpp")

# Guard: Check if test sources exist
if(NOT MM_WM_TEST_SOURCES)
    message(STATUS "No multi-modal world model unit tests found")
    return()
endif()

# Create test executable for each test file
foreach(TEST_FILE ${MM_WM_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    set(TARGET_NAME "unit_cognitive_extrapolation_${TEST_NAME}")

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
        LABELS "unit;cognitive;extrapolation;world_model;multimodal"
        TIMEOUT 60
    )
endforeach()

message(STATUS "Added multi-modal world model unit tests")
