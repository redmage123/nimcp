# ============================================================================
# Attention Bridge Regression Tests
# ============================================================================
# WHAT: Regression tests for Attention-SNN and Attention-Plasticity bridges
# WHY:  Ensure numerical stability, memory safety, and performance bounds
# HOW:  GoogleTest-based tests covering edge cases, bounds, and performance

# ============================================================================
# Attention-SNN Bridge and Attention-Plasticity Bridge Regression Tests
# ============================================================================

add_executable(regression_cognitive_attention_bridge
    test_attention_bridge_regression.cpp
)

target_link_libraries(regression_cognitive_attention_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)

target_include_directories(regression_cognitive_attention_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/time
        ${CMAKE_SOURCE_DIR}/src/cognitive/attention
)

target_compile_features(regression_cognitive_attention_bridge PRIVATE cxx_std_17)

add_test(NAME regression_cognitive_attention_bridge
         COMMAND regression_cognitive_attention_bridge)

set_tests_properties(regression_cognitive_attention_bridge PROPERTIES
    TIMEOUT 180
    LABELS "regression;cognitive;attention;snn;plasticity"
)

# ============================================================================
# Attention SNN-Plasticity Regression Tests
# ============================================================================
# WHAT: Regression tests for Attention SNN and Plasticity bridges
# WHY:  Ensure stability of attention learning, focus, habituation, novelty
# HOW:  Boundary tests, long-running stability, protected synapse integrity
# ============================================================================

add_executable(regression_cognitive_attention_snn_plasticity
    test_attention_snn_plasticity_regression.cpp
)

target_link_libraries(regression_cognitive_attention_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_attention_snn_plasticity
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_attention_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_attention_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;attention;snn;plasticity"
        TIMEOUT 180
)
