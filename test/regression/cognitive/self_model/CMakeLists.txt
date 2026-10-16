# ============================================================================
# Self Model SNN-Plasticity Regression Tests
# ============================================================================
# WHAT: Regression tests for Self Model SNN and Plasticity bridges
# WHY:  Ensure numerical stability, protected synapse integrity, performance
# HOW:  Test weight bounds, memory leaks, protected synapse integrity under stress
# ============================================================================

# Self Model SNN-Plasticity Regression Tests
add_executable(regression_cognitive_self_model_snn_plasticity
    test_self_model_snn_plasticity_regression.cpp
)
target_link_libraries(regression_cognitive_self_model_snn_plasticity
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(regression_cognitive_self_model_snn_plasticity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(regression_cognitive_self_model_snn_plasticity PRIVATE cxx_std_17)
gtest_discover_tests(regression_cognitive_self_model_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;self_model;snn;plasticity"
        TIMEOUT 180
)
