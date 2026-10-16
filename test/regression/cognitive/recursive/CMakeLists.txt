# ============================================================================
# Recursive Cognition Regression Tests
# ============================================================================
#
# WHAT: Performance and correctness regression tests
# WHY:  Detect performance degradation and behavioral regressions
# HOW:  Benchmark operations, verify numerical stability

message(STATUS "Adding recursive cognition regression tests...")

# Regression Test: Performance and correctness
add_test_binary(
    test_rcog_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rcog_regression.cpp
    regression
)

# Set longer timeout for performance tests
set_tests_properties(test_rcog_regression PROPERTIES TIMEOUT 120)

# ----------------------------------------------------------------------------
# Regression Test: SNN/Plasticity Bridge Integration
# ----------------------------------------------------------------------------
add_executable(regression_rcog_bridge
    test_rcog_bridge_regression.cpp
)

target_link_libraries(regression_rcog_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_rcog_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_rcog_bridge PRIVATE cxx_std_17)
target_compile_options(regression_rcog_bridge PRIVATE -O2)

gtest_discover_tests(regression_rcog_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;recursive;bridge;snn;plasticity"
        TIMEOUT 300
)

message(STATUS "  Configured recursive cognition regression tests")
