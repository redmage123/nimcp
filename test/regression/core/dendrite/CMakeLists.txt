# ============================================================================
# NIMCP Dendrite Performance Regression Tests
# ============================================================================
# WHAT: Performance regression tests for dendrite module
# WHY:  Detect performance regressions across versions
# HOW:  Measure operation times against established baselines
# ============================================================================

add_executable(regression_core_dendrite_performance
    test_dendrite_performance_regression.cpp
    ${CMAKE_SOURCE_DIR}/src/core/dendrite/nimcp_dendrite.c
)

target_link_libraries(regression_core_dendrite_performance
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(regression_core_dendrite_performance PRIVATE cxx_std_17)

target_include_directories(regression_core_dendrite_performance
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Enable NIMCP_TESTING for test APIs
target_compile_definitions(regression_core_dendrite_performance PRIVATE NIMCP_TESTING)

add_test(NAME regression_core_dendrite_performance
         COMMAND regression_core_dendrite_performance)

set_tests_properties(regression_core_dendrite_performance
    PROPERTIES
        TIMEOUT 300
        LABELS "regression;core;dendrite;performance"
)
