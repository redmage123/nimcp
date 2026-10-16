# ============================================================================
# Social Cognition FEP Bridge Regression Tests
# ============================================================================
# WHAT: Build configuration for social FEP bridge regression tests
# WHY:  Verify stability, performance, and numerical accuracy of social FEP bridge
# HOW:  Long-running tests with many cycles to catch regressions
#
# TEST COVERAGE:
# - Social prediction stability over many cycles
# - Free energy bounds enforcement
# - Performance regression (update time)
# - Memory leak detection (valgrind-friendly)
# - Relationship consistency over time
# - Numerical stability under stress
# - Configuration persistence
# - Statistics accuracy
# - Concurrent access safety

# ----------------------------------------------------------------------------
# Social FEP Bridge Regression Tests
# ----------------------------------------------------------------------------
add_executable(regression_social_fep
    test_social_fep_regression.cpp
)

target_link_libraries(regression_social_fep
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_social_fep
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_social_fep PRIVATE cxx_std_17)

target_compile_definitions(regression_social_fep PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_social_fep
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;social;fep;stability;performance"
        TIMEOUT 600
)
