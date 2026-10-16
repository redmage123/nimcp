# ============================================================================
# Emotion Bridge Regression Tests
# ============================================================================
# WHAT: Regression tests for Emotion-SNN and Emotion-Plasticity bridges
# WHY:  Ensure numerical stability, memory safety, and performance bounds
# HOW:  GoogleTest-based tests covering edge cases, bounds, and performance

# ============================================================================
# Emotion-SNN Bridge and Emotion-Plasticity Bridge Regression Tests
# ============================================================================

add_executable(regression_cognitive_emotion_bridge
    test_emotion_bridge_regression.cpp
)

target_link_libraries(regression_cognitive_emotion_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)

target_include_directories(regression_cognitive_emotion_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/time
        ${CMAKE_SOURCE_DIR}/src/cognitive/emotion
)

target_compile_features(regression_cognitive_emotion_bridge PRIVATE cxx_std_17)

add_test(NAME regression_cognitive_emotion_bridge
         COMMAND regression_cognitive_emotion_bridge)

set_tests_properties(regression_cognitive_emotion_bridge PROPERTIES
    TIMEOUT 180
    LABELS "regression;cognitive;emotion;snn;plasticity"
)

# ============================================================================
# Emotion SNN-Plasticity Regression Tests
# ============================================================================
# WHAT: Regression tests for Emotion SNN and Plasticity bridges
# WHY:  Ensure stability of emotional conditioning, extinction learning
# HOW:  Boundary tests, valence/arousal modulation, protected synapse integrity
# ============================================================================

add_executable(regression_cognitive_emotion_snn_plasticity
    test_emotion_snn_plasticity_regression.cpp
)

target_link_libraries(regression_cognitive_emotion_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_emotion_snn_plasticity
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_emotion_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_emotion_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;emotion;snn;plasticity"
        TIMEOUT 180
)
