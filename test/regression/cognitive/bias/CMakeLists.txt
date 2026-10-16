# ============================================================================
# Cognitive Bias Regression Tests
# ============================================================================
# WHAT: Regression tests for Bias SNN-Plasticity bridges
# WHY:  Ensure bias detection maintains accuracy and performance
# HOW:  Test baselines, boundaries, performance thresholds

add_executable(regression_cognitive_bias_snn_plasticity
    test_bias_snn_plasticity_regression.cpp
)

target_link_libraries(regression_cognitive_bias_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_bias_snn_plasticity
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_bias_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_bias_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;bias;snn;plasticity"
        TIMEOUT 300
)
