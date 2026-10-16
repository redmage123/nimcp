# =============================================================================
# Regression Tests for Plasticity Module Exception Handling
# =============================================================================
# WHAT: CMakeLists.txt for plasticity module exception regression tests
# WHY:  Test API contract stability and error code consistency
# HOW:  Build test executable and register with CTest
#
# REGRESSION COVERAGE:
# - API contract verification for all exception-throwing functions
# - Error code consistency across module boundaries
# - Return value contract enforcement
# - Exception message format stability
#
# @author NIMCP Development Team
# @date 2026-01-25
# =============================================================================

# Plasticity Module Exception Regression Tests
add_executable(regression_plasticity_module_exception
    test_plasticity_module_exception_regression.cpp
)

target_include_directories(regression_plasticity_module_exception
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(regression_plasticity_module_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_plasticity_module_exception PRIVATE cxx_std_17)

# Register with CTest using gtest_discover_tests for automatic test discovery
gtest_discover_tests(regression_plasticity_module_exception
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;exception;neuromodulators;attention;stdp"
)

# =============================================================================
# Multi-Attention Exception Regression Tests
# =============================================================================
# WHAT: Regression tests for multi-attention exception handling stability
# WHY:  Ensure API contracts remain stable and error handling consistent
# HOW:  Test NULL handling, boundary conditions, long-running stability
#
# @author NIMCP Development Team
# @date 2026-01-25
# =============================================================================

add_executable(regression_attention_exception
    test_attention_exception_regression.cpp
)

target_include_directories(regression_attention_exception
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(regression_attention_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_attention_exception PRIVATE cxx_std_17)

gtest_discover_tests(regression_attention_exception
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;attention;exception"
)
