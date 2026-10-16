#=============================================================================
# CMakeLists.txt - Performance regression tests for Config Module
#=============================================================================
# WHAT: Build configuration for config performance tests
# WHY:  Detect performance regressions in config system
# HOW:  GoogleTest with timing benchmarks

cmake_minimum_required(VERSION 3.14)

# Find GoogleTest
find_package(GTest REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/utils/config
    ${GTEST_INCLUDE_DIRS}
)

#=============================================================================
# Test: Config Performance
#=============================================================================
add_executable(test_config_performance
    test_config_performance.cpp
)

target_link_libraries(test_config_performance
    nimcp_config
    nimcp_dynamic_config
    nimcp_unified_memory
    GTest::gtest
    GTest::gtest_main
    pthread
    stdc++fs  # For std::filesystem
)

add_test(NAME test_config_performance COMMAND test_config_performance)

#=============================================================================
# Test Properties
#=============================================================================

# Set test labels and longer timeout for performance tests
set_tests_properties(test_config_performance
    PROPERTIES
        LABELS "regression;performance;benchmark;config"
        TIMEOUT 300  # Performance tests may take longer
)

# Set working directory
set_tests_properties(test_config_performance
    PROPERTIES
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Disable optimizations for debug builds to get consistent timing
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test_config_performance PRIVATE -O0)
endif()

# Enable optimizations for release builds to test real performance
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(test_config_performance PRIVATE -O3 -march=native)
endif()

# Don't enable sanitizers for performance tests (they add overhead)
# Coverage can be enabled but may affect timing

# Output performance results to file if requested
if(PERFORMANCE_OUTPUT_FILE)
    set_tests_properties(test_config_performance
        PROPERTIES
            ENVIRONMENT "GTEST_OUTPUT=xml:${PERFORMANCE_OUTPUT_FILE}"
    )
endif()

# Add custom target to run only performance tests
add_custom_target(config_performance_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L "performance" -V
    DEPENDS test_config_performance
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running config performance tests..."
)

# Add custom target to generate performance report
add_custom_target(config_performance_report
    COMMAND echo "=== Config Module Performance Report ==="
    COMMAND ${CMAKE_BINARY_DIR}/test/regression/utils/config/test_config_performance
    DEPENDS test_config_performance
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating config performance report..."
)
