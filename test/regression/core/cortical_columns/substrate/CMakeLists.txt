# ============================================================================
# NIMCP Cortical Substrate Bridge Regression Tests
# ============================================================================
# WHAT: CMake configuration for cortical substrate bridge regression tests
# WHY:  Build comprehensive regression suite for bridge stability
# HOW:  Link with GTest, nimcp library, and pthread for thread safety tests

# ============================================================================
# Regression Test Executable
# ============================================================================

add_executable(regression_core_cortical_columns_substrate_bridge
    test_cortical_substrate_bridge_regression.cpp
)

# ============================================================================
# Compiler Features
# ============================================================================

target_compile_features(regression_core_cortical_columns_substrate_bridge
    PRIVATE
        cxx_std_17
)

# ============================================================================
# Include Directories
# ============================================================================

target_include_directories(regression_core_cortical_columns_substrate_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# ============================================================================
# Link Libraries
# ============================================================================

target_link_libraries(regression_core_cortical_columns_substrate_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

# ============================================================================
# Compiler Options
# ============================================================================

# Enable optimizations for performance testing
target_compile_options(regression_core_cortical_columns_substrate_bridge
    PRIVATE
        -O2
        -march=native
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Release>:-DNDEBUG>
)

# ============================================================================
# Test Registration
# ============================================================================

add_test(NAME regression_core_cortical_columns_substrate_bridge
         COMMAND regression_core_cortical_columns_substrate_bridge
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# ============================================================================
# Test Properties
# ============================================================================

set_tests_properties(regression_core_cortical_columns_substrate_bridge
    PROPERTIES
        # Timeout: 10 minutes for comprehensive regression tests
        TIMEOUT 600

        # Labels for test categorization
        LABELS "regression;performance;core;cortical_columns;substrate_bridge"

        # Environment variables
        ENVIRONMENT "NIMCP_LOG_LEVEL=WARNING"

        # Test must pass
        WILL_FAIL FALSE
)

# ============================================================================
# Category-Specific Test Filters
# ============================================================================

# Performance benchmarks only
add_test(NAME regression_cortical_substrate_bridge_performance
         COMMAND regression_core_cortical_columns_substrate_bridge
                 --gtest_filter=*PerformanceBenchmarks*
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_substrate_bridge_performance
    PROPERTIES
        TIMEOUT 300
        LABELS "regression;performance;benchmarks;substrate_bridge")

# Memory regression tests only
add_test(NAME regression_cortical_substrate_bridge_memory
         COMMAND regression_core_cortical_columns_substrate_bridge
                 --gtest_filter=*MemoryRegression*
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_substrate_bridge_memory
    PROPERTIES
        TIMEOUT 180
        LABELS "regression;memory;substrate_bridge")

# Numerical stability tests only
add_test(NAME regression_cortical_substrate_bridge_numerical
         COMMAND regression_core_cortical_columns_substrate_bridge
                 --gtest_filter=*NumericalStability*
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_substrate_bridge_numerical
    PROPERTIES
        TIMEOUT 120
        LABELS "regression;numerical;stability;substrate_bridge")

# Scaling tests only
add_test(NAME regression_cortical_substrate_bridge_scaling
         COMMAND regression_core_cortical_columns_substrate_bridge
                 --gtest_filter=*ScalingTests*
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_substrate_bridge_scaling
    PROPERTIES
        TIMEOUT 300
        LABELS "regression;performance;scaling;substrate_bridge")

# Correctness regression tests only
add_test(NAME regression_cortical_substrate_bridge_correctness
         COMMAND regression_core_cortical_columns_substrate_bridge
                 --gtest_filter=*CorrectnessRegression*
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_substrate_bridge_correctness
    PROPERTIES
        TIMEOUT 180
        LABELS "regression;correctness;substrate_bridge")

# Thread safety tests only
add_test(NAME regression_cortical_substrate_bridge_thread_safety
         COMMAND regression_core_cortical_columns_substrate_bridge
                 --gtest_filter=*ThreadSafetyRegression*
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_substrate_bridge_thread_safety
    PROPERTIES
        TIMEOUT 180
        LABELS "regression;thread_safety;concurrency;substrate_bridge")

# ============================================================================
# Installation (Optional)
# ============================================================================

# Install test executable to test directory
install(TARGETS regression_core_cortical_columns_substrate_bridge
        RUNTIME DESTINATION test/regression/core/cortical_columns/substrate
        COMPONENT tests)

# ============================================================================
# CTest Integration
# ============================================================================

# Add to CTest dashboard if enabled
if(BUILD_TESTING)
    set_property(TEST regression_core_cortical_columns_substrate_bridge
                 PROPERTY COST 100)  # High cost due to comprehensive nature

    # Performance baseline tracking (if available)
    if(ENABLE_PERFORMANCE_TRACKING)
        set_property(TEST regression_core_cortical_columns_substrate_bridge
                     PROPERTY MEASUREMENT_REGEX "ops/sec|us|ms|bytes")
    endif()
endif()

# ============================================================================
# Valgrind Integration (Memory Leak Detection)
# ============================================================================

if(ENABLE_VALGRIND_TESTS)
    add_test(NAME regression_cortical_substrate_bridge_valgrind
             COMMAND valgrind
                     --leak-check=full
                     --show-leak-kinds=all
                     --track-origins=yes
                     --error-exitcode=1
                     $<TARGET_FILE:regression_core_cortical_columns_substrate_bridge>
                     --gtest_filter=*MemoryRegression*)

    set_tests_properties(regression_cortical_substrate_bridge_valgrind
        PROPERTIES
            TIMEOUT 1200
            LABELS "valgrind;memory;regression;substrate_bridge")
endif()

# ============================================================================
# Code Coverage Integration
# ============================================================================

if(ENABLE_COVERAGE)
    target_compile_options(regression_core_cortical_columns_substrate_bridge
        PRIVATE
            --coverage
            -fprofile-arcs
            -ftest-coverage)

    target_link_options(regression_core_cortical_columns_substrate_bridge
        PRIVATE
            --coverage)
endif()

# ============================================================================
# Sanitizer Support
# ============================================================================

if(ENABLE_ASAN)
    target_compile_options(regression_core_cortical_columns_substrate_bridge
        PRIVATE
            -fsanitize=address
            -fno-omit-frame-pointer)

    target_link_options(regression_core_cortical_columns_substrate_bridge
        PRIVATE
            -fsanitize=address)
endif()

if(ENABLE_TSAN)
    target_compile_options(regression_core_cortical_columns_substrate_bridge
        PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer)

    target_link_options(regression_core_cortical_columns_substrate_bridge
        PRIVATE
            -fsanitize=thread)
endif()

if(ENABLE_UBSAN)
    target_compile_options(regression_core_cortical_columns_substrate_bridge
        PRIVATE
            -fsanitize=undefined
            -fno-omit-frame-pointer)

    target_link_options(regression_core_cortical_columns_substrate_bridge
        PRIVATE
            -fsanitize=undefined)
endif()

# ============================================================================
# Documentation
# ============================================================================

# Add test documentation to overall project docs
if(BUILD_DOCUMENTATION)
    install(FILES README.md
            DESTINATION doc/test/regression/cortical_columns/substrate
            COMPONENT documentation
            OPTIONAL)
endif()

# ============================================================================
# Summary
# ============================================================================

message(STATUS "Cortical Substrate Bridge Regression Tests:")
message(STATUS "  - Main test: regression_core_cortical_columns_substrate_bridge")
message(STATUS "  - Categories: performance, memory, numerical, scaling, correctness, thread_safety")
message(STATUS "  - Total tests: 25 regression tests")
message(STATUS "  - Timeout: 600s (10 minutes)")
message(STATUS "  - Optimization: -O2 -march=native")
