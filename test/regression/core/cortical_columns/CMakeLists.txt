# ============================================================================
# NIMCP Cortical Columns Regression Tests
# ============================================================================
# WHAT: CMake configuration for cortical columns regression and performance tests
# WHY:  Build comprehensive test suite for cortical column modules
# HOW:  Link with GTest, nimcp library, and required dependencies

# ============================================================================
# Performance and Regression Test Executable
# ============================================================================

add_executable(regression_cortical_columns_performance
    test_cortical_columns_performance.cpp
)

# ============================================================================
# Cortical Column Regression Tests
# ============================================================================

add_executable(regression_cortical_column
    test_cortical_column_regression.cpp
)

target_compile_features(regression_cortical_column
    PRIVATE
        cxx_std_17
)

target_include_directories(regression_cortical_column
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(regression_cortical_column
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)

add_test(NAME regression_cortical_column
         COMMAND regression_cortical_column
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_column
    PROPERTIES
        TIMEOUT 300
        LABELS "regression;core;cortical_columns"
        ENVIRONMENT "NIMCP_LOG_LEVEL=WARNING"
)

# ============================================================================
# Cortical Hierarchy Regression Tests
# ============================================================================

add_executable(regression_cortical_hierarchy
    test_cortical_hierarchy_regression.cpp
)

target_compile_features(regression_cortical_hierarchy
    PRIVATE
        cxx_std_17
)

target_include_directories(regression_cortical_hierarchy
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(regression_cortical_hierarchy
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)

add_test(NAME regression_cortical_hierarchy
         COMMAND regression_cortical_hierarchy
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_hierarchy
    PROPERTIES
        TIMEOUT 300
        LABELS "regression;core;cortical_columns;hierarchy"
        ENVIRONMENT "NIMCP_LOG_LEVEL=WARNING"
)

# ============================================================================
# Cortical Features Regression Tests
# ============================================================================

add_executable(regression_cortical_features
    test_cortical_features_regression.cpp
)

target_compile_features(regression_cortical_features
    PRIVATE
        cxx_std_17
)

target_include_directories(regression_cortical_features
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(regression_cortical_features
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)

add_test(NAME regression_cortical_features
         COMMAND regression_cortical_features
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_features
    PROPERTIES
        TIMEOUT 180
        LABELS "regression;core;cortical_columns;features"
        ENVIRONMENT "NIMCP_LOG_LEVEL=WARNING"
)

# ============================================================================
# Compiler Features
# ============================================================================

target_compile_features(regression_cortical_columns_performance
    PRIVATE
        cxx_std_17
)

# ============================================================================
# Include Directories
# ============================================================================

target_include_directories(regression_cortical_columns_performance
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# ============================================================================
# Link Libraries
# ============================================================================

target_link_libraries(regression_cortical_columns_performance
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
target_compile_options(regression_cortical_columns_performance
    PRIVATE
        -O2
        -march=native
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Release>:-DNDEBUG>
)

# ============================================================================
# Test Registration
# ============================================================================

add_test(NAME regression_cortical_columns_performance
         COMMAND regression_cortical_columns_performance
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# ============================================================================
# Test Properties
# ============================================================================

set_tests_properties(regression_cortical_columns_performance
    PROPERTIES
        # Timeout: 10 minutes for comprehensive performance tests
        TIMEOUT 600

        # Labels for test categorization
        LABELS "regression;performance;core;cortical_columns;benchmarks"

        # Environment variables
        ENVIRONMENT "NIMCP_LOG_LEVEL=WARNING"

        # Test must pass
        WILL_FAIL FALSE

        # Serialize with brain_heavy to prevent CPU contention in perf tests
        RESOURCE_LOCK "brain_heavy"
)

# ============================================================================
# Category-Specific Test Filters (Optional)
# ============================================================================

# Performance benchmarks only
add_test(NAME regression_cortical_columns_performance_benchmarks
         COMMAND regression_cortical_columns_performance
                 --gtest_filter=*PerformanceBenchmarks*
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_columns_performance_benchmarks
    PROPERTIES
        TIMEOUT 300
        LABELS "regression;performance;benchmarks")

# Memory regression tests only
add_test(NAME regression_cortical_columns_memory
         COMMAND regression_cortical_columns_performance
                 --gtest_filter=*MemoryRegression*
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_columns_memory
    PROPERTIES
        TIMEOUT 180
        LABELS "regression;memory")

# Numerical stability tests only
add_test(NAME regression_cortical_columns_numerical
         COMMAND regression_cortical_columns_performance
                 --gtest_filter=*NumericalStability*
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_columns_numerical
    PROPERTIES
        TIMEOUT 120
        LABELS "regression;numerical;stability")

# Scaling tests only
add_test(NAME regression_cortical_columns_scaling
         COMMAND regression_cortical_columns_performance
                 --gtest_filter=*ScalingTests*
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_columns_scaling
    PROPERTIES
        TIMEOUT 300
        LABELS "regression;performance;scaling")

# Correctness regression tests only
add_test(NAME regression_cortical_columns_correctness
         COMMAND regression_cortical_columns_performance
                 --gtest_filter=*CorrectnessRegression*
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_columns_correctness
    PROPERTIES
        TIMEOUT 180
        LABELS "regression;correctness")

# Thread safety tests only
add_test(NAME regression_cortical_columns_thread_safety
         COMMAND regression_cortical_columns_performance
                 --gtest_filter=*ThreadSafetyRegression*
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(regression_cortical_columns_thread_safety
    PROPERTIES
        TIMEOUT 180
        LABELS "regression;thread_safety;concurrency")

# ============================================================================
# Installation (Optional)
# ============================================================================

# Install test executable to test directory
install(TARGETS regression_cortical_columns_performance
        RUNTIME DESTINATION test/regression/core/cortical_columns
        COMPONENT tests)

# ============================================================================
# CTest Integration
# ============================================================================

# Add to CTest dashboard if enabled
if(BUILD_TESTING)
    set_property(TEST regression_cortical_columns_performance
                 PROPERTY COST 100)  # High cost due to comprehensive nature

    # Performance baseline tracking (if available)
    if(ENABLE_PERFORMANCE_TRACKING)
        set_property(TEST regression_cortical_columns_performance
                     PROPERTY MEASUREMENT_REGEX "ops/sec|ms|bytes")
    endif()
endif()

# ============================================================================
# Valgrind Integration (Memory Leak Detection)
# ============================================================================

if(ENABLE_VALGRIND_TESTS)
    add_test(NAME regression_cortical_columns_valgrind
             COMMAND valgrind
                     --leak-check=full
                     --show-leak-kinds=all
                     --track-origins=yes
                     --error-exitcode=1
                     $<TARGET_FILE:regression_cortical_columns_performance>
                     --gtest_filter=*MemoryRegression*)

    set_tests_properties(regression_cortical_columns_valgrind
        PROPERTIES
            TIMEOUT 1200
            LABELS "valgrind;memory;regression")
endif()

# ============================================================================
# Code Coverage Integration
# ============================================================================

if(ENABLE_COVERAGE)
    target_compile_options(regression_cortical_columns_performance
        PRIVATE
            --coverage
            -fprofile-arcs
            -ftest-coverage)

    target_link_options(regression_cortical_columns_performance
        PRIVATE
            --coverage)
endif()

# ============================================================================
# Sanitizer Support
# ============================================================================

if(ENABLE_ASAN)
    target_compile_options(regression_cortical_columns_performance
        PRIVATE
            -fsanitize=address
            -fno-omit-frame-pointer)

    target_link_options(regression_cortical_columns_performance
        PRIVATE
            -fsanitize=address)
endif()

if(ENABLE_TSAN)
    target_compile_options(regression_cortical_columns_performance
        PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer)

    target_link_options(regression_cortical_columns_performance
        PRIVATE
            -fsanitize=thread)
endif()

if(ENABLE_UBSAN)
    target_compile_options(regression_cortical_columns_performance
        PRIVATE
            -fsanitize=undefined
            -fno-omit-frame-pointer)

    target_link_options(regression_cortical_columns_performance
        PRIVATE
            -fsanitize=undefined)
endif()

# ============================================================================
# Documentation
# ============================================================================

# Add test documentation to overall project docs
if(BUILD_DOCUMENTATION)
    install(FILES README.md
            DESTINATION doc/test/regression/cortical_columns
            COMPONENT documentation
            OPTIONAL)
endif()

# ============================================================================
# Subdirectories
# ============================================================================

# Substrate bridge regression tests
add_subdirectory(substrate)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "Cortical Columns Regression Tests:")
message(STATUS "  - Main test: regression_cortical_columns_performance")
message(STATUS "  - Column regression: regression_cortical_column (25 tests)")
message(STATUS "  - Hierarchy regression: regression_cortical_hierarchy (20 tests)")
message(STATUS "  - Features regression: regression_cortical_features (20 tests)")
message(STATUS "  - Categories: performance, memory, numerical, scaling, correctness, thread_safety")
message(STATUS "  - Timeouts: 180-300 seconds")
message(STATUS "  - Optimization: -O2 -march=native")
