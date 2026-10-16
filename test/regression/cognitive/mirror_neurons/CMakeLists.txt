# Cognitive Mirror Neurons Performance Regression Tests

# Mirror Substrate Performance Regression Tests
add_executable(regression_cognitive_mirror_neurons_substrate
    test_mirror_substrate_regression.cpp
)
target_link_libraries(regression_cognitive_mirror_neurons_substrate
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_mirror_neurons_substrate
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
)
add_test(NAME regression_cognitive_mirror_neurons_substrate
         COMMAND regression_cognitive_mirror_neurons_substrate)
set_tests_properties(regression_cognitive_mirror_neurons_substrate PROPERTIES LABELS "regression")

# ============================================================================
# Mirror-Emotion Bridge Regression Tests (Phase 10.11.11)
# ============================================================================
# Numerical stability and performance regression for emotion bridge

add_executable(regression_cognitive_mirror_neurons_emotion_bridge
    test_mirror_emotion_bridge_regression.cpp
)
target_link_libraries(regression_cognitive_mirror_neurons_emotion_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_mirror_neurons_emotion_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
)
target_compile_features(regression_cognitive_mirror_neurons_emotion_bridge PRIVATE cxx_std_17)
add_test(NAME regression_cognitive_mirror_neurons_emotion_bridge
         COMMAND regression_cognitive_mirror_neurons_emotion_bridge)
set_tests_properties(regression_cognitive_mirror_neurons_emotion_bridge PROPERTIES
    TIMEOUT 120
    LABELS "regression;cognitive;mirror_neurons;emotion"
)

# ============================================================================
# Mirror-Attention Bridge Regression Tests (Phase 10.11.12)
# ============================================================================
# Numerical stability and performance regression for attention bridge

add_executable(regression_cognitive_mirror_neurons_attention_bridge
    test_mirror_attention_bridge_regression.cpp
)
target_link_libraries(regression_cognitive_mirror_neurons_attention_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_mirror_neurons_attention_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
)
target_compile_features(regression_cognitive_mirror_neurons_attention_bridge PRIVATE cxx_std_17)
add_test(NAME regression_cognitive_mirror_neurons_attention_bridge
         COMMAND regression_cognitive_mirror_neurons_attention_bridge)
set_tests_properties(regression_cognitive_mirror_neurons_attention_bridge PROPERTIES
    TIMEOUT 120
    LABELS "regression;cognitive;mirror_neurons;attention"
)

# ============================================================================
# Mirror-SNN-Plasticity Regression Tests
# ============================================================================
# Numerical stability, memory, and performance regression testing

add_executable(regression_cognitive_mirror_neurons_snn_plasticity
    test_mirror_snn_plasticity_regression.cpp
)
target_link_libraries(regression_cognitive_mirror_neurons_snn_plasticity
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_mirror_neurons_snn_plasticity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/time
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
        ${CMAKE_SOURCE_DIR}/src/snn
        ${CMAKE_SOURCE_DIR}/src/plasticity
)
target_compile_features(regression_cognitive_mirror_neurons_snn_plasticity PRIVATE cxx_std_17)
add_test(NAME regression_cognitive_mirror_neurons_snn_plasticity
         COMMAND regression_cognitive_mirror_neurons_snn_plasticity)
set_tests_properties(regression_cognitive_mirror_neurons_snn_plasticity PROPERTIES
    TIMEOUT 180
    LABELS "regression;cognitive;mirror_neurons;snn;plasticity"
)
