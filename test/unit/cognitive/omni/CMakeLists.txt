# ============================================================================
# Omni World Model Unit Tests
# ============================================================================
# WHAT: Unit tests for omnidirectional world model (DreamerV3/JEPA-inspired)
# WHY:  Verify RSSM, MDN, counterfactual reasoning, and dynamics prediction
# HOW:  GTest-based tests for all omni_wm_* functions
# ============================================================================

# Find test sources
file(GLOB OMNI_WM_TEST_SOURCES "*.cpp")

# Guard: Check if test sources exist
if(NOT OMNI_WM_TEST_SOURCES)
    message(STATUS "No omni world model unit tests found")
    return()
endif()

# Create test executable for each test file
foreach(TEST_FILE ${OMNI_WM_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    set(TARGET_NAME "unit_cognitive_omni_${TEST_NAME}")

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
        LABELS "unit;cognitive;omni;world_model"
        TIMEOUT 60
    )
endforeach()

message(STATUS "Added omni world model unit tests")

# Include bridges subdirectory
add_subdirectory(bridges)
