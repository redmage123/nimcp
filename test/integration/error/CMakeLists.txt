# ============================================================================
# Cross-Module Error Propagation Tests
# ============================================================================
# WHAT: Integration tests for error propagation across module boundaries
# WHY:  Verify cleanup happens properly on errors
# HOW:  Test error context, recovery, and multi-module scenarios

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping error propagation tests")
    return()
endif()

# Error Propagation Integration Test
add_executable(integration_error_propagation
    test_cross_module_error_propagation.cpp
)

target_link_libraries(integration_error_propagation
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_error_propagation
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(integration_error_propagation PRIVATE cxx_std_17)

gtest_discover_tests(integration_error_propagation
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;error;propagation"
        TIMEOUT 120
)

message(STATUS "  Added cross-module error propagation integration test")
