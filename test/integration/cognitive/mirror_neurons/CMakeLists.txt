# Cognitive Mirror Neurons Integration Tests
# ============================================================================
# Phase 10.11 Mirror Neurons - Integration Testing
# ============================================================================

# Mirror Substrate Integration Tests
add_executable(integration_cognitive_mirror_neurons_substrate
    test_mirror_substrate_integration.cpp
)
target_link_libraries(integration_cognitive_mirror_neurons_substrate
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_mirror_neurons_substrate
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/core/axon
        ${CMAKE_SOURCE_DIR}/src/core/dendrite
        ${CMAKE_SOURCE_DIR}/src/glial/myelin_sheath
        ${CMAKE_SOURCE_DIR}/src/glial/astrocytes
        ${CMAKE_SOURCE_DIR}/src/glial/oligodendrocytes
        ${CMAKE_SOURCE_DIR}/src/glial/microglia
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
)
add_test(NAME integration_cognitive_mirror_neurons_substrate
         COMMAND integration_cognitive_mirror_neurons_substrate)
set_tests_properties(integration_cognitive_mirror_neurons_substrate PROPERTIES LABELS "integration")

# ============================================================================
# Mirror-Emotion-Attention Integration Tests (Phase 10.11.11-12)
# ============================================================================
# Cross-bridge integration between emotion and attention systems

add_executable(integration_cognitive_mirror_neurons_emotion_attention
    test_mirror_emotion_attention_integration.cpp
)
target_link_libraries(integration_cognitive_mirror_neurons_emotion_attention
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_mirror_neurons_emotion_attention
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
)
target_compile_features(integration_cognitive_mirror_neurons_emotion_attention PRIVATE cxx_std_17)
add_test(NAME integration_cognitive_mirror_neurons_emotion_attention
         COMMAND integration_cognitive_mirror_neurons_emotion_attention)
set_tests_properties(integration_cognitive_mirror_neurons_emotion_attention PROPERTIES
    TIMEOUT 180
    LABELS "integration;cognitive;mirror_neurons;emotion;attention"
)

# ============================================================================
# Mirror-SNN-Plasticity Integration Tests
# ============================================================================
# Bidirectional dataflow integration between SNN and Plasticity bridges

add_executable(integration_cognitive_mirror_neurons_snn_plasticity
    test_mirror_snn_plasticity_integration.cpp
)
target_link_libraries(integration_cognitive_mirror_neurons_snn_plasticity
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_mirror_neurons_snn_plasticity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/time
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
        ${CMAKE_SOURCE_DIR}/src/snn
        ${CMAKE_SOURCE_DIR}/src/plasticity
)
target_compile_features(integration_cognitive_mirror_neurons_snn_plasticity PRIVATE cxx_std_17)
add_test(NAME integration_cognitive_mirror_neurons_snn_plasticity
         COMMAND integration_cognitive_mirror_neurons_snn_plasticity)
set_tests_properties(integration_cognitive_mirror_neurons_snn_plasticity PROPERTIES
    TIMEOUT 180
    LABELS "integration;cognitive;mirror_neurons;snn;plasticity"
)

