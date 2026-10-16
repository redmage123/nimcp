# ============================================================================
# Epistemic Regression Tests
# ============================================================================
# WHAT: Regression tests for Epistemic SNN-Plasticity bridges
# WHY:  Ensure epistemic filtering maintains performance and accuracy
# HOW:  Test baselines, boundaries, performance thresholds

add_executable(regression_cognitive_epistemic_snn_plasticity
    test_epistemic_snn_plasticity_regression.cpp
)

target_link_libraries(regression_cognitive_epistemic_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_epistemic_snn_plasticity
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_epistemic_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_epistemic_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;epistemic;snn;plasticity"
        TIMEOUT 300
)
