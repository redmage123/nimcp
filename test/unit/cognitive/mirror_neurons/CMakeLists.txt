# Unit Tests for Mirror Neuron Enhancements
# ============================================================================
# Phase 10.11 Mirror Neurons - Unit Testing
# ============================================================================
# Tests for STDP Learning, Motor Resonance, Hierarchical Goals, and Substrate

# Mirror Neuron Enhancement Tests (STDP, Resonance, Hierarchy)
add_executable(unit_cognitive_mirror_neurons_test_mirror_enhancements
    test_mirror_enhancements.cpp
)
target_link_libraries(unit_cognitive_mirror_neurons_test_mirror_enhancements
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(unit_cognitive_mirror_neurons_test_mirror_enhancements
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
)
target_compile_features(unit_cognitive_mirror_neurons_test_mirror_enhancements PRIVATE cxx_std_17)
add_test(NAME unit_cognitive_mirror_neurons_test_mirror_enhancements
         COMMAND unit_cognitive_mirror_neurons_test_mirror_enhancements)
set_tests_properties(unit_cognitive_mirror_neurons_test_mirror_enhancements PROPERTIES
    TIMEOUT 120
    LABELS "unit;cognitive;mirror_neurons;enhancements"
)

# Mirror Neuron Substrate Tests (Pool, CoW, Biological Backing)
add_executable(unit_cognitive_mirror_neurons_test_mirror_substrate
    test_mirror_substrate.cpp
)
target_link_libraries(unit_cognitive_mirror_neurons_test_mirror_substrate
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(unit_cognitive_mirror_neurons_test_mirror_substrate
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/axon
        ${CMAKE_SOURCE_DIR}/src/core/dendrite
        ${CMAKE_SOURCE_DIR}/src/glial/myelin_sheath
        ${CMAKE_SOURCE_DIR}/src/glial/astrocytes
        ${CMAKE_SOURCE_DIR}/src/glial/oligodendrocytes
        ${CMAKE_SOURCE_DIR}/src/glial/microglia
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
)
target_compile_features(unit_cognitive_mirror_neurons_test_mirror_substrate PRIVATE cxx_std_17)
add_test(NAME unit_cognitive_mirror_neurons_test_mirror_substrate
         COMMAND unit_cognitive_mirror_neurons_test_mirror_substrate)
set_tests_properties(unit_cognitive_mirror_neurons_test_mirror_substrate PROPERTIES
    TIMEOUT 120
    LABELS "unit;cognitive;mirror_neurons;substrate"
)

# Mirror Neuron - Immune System Integration Tests
add_executable(unit_cognitive_mirror_neurons_test_mirror_immune_integration
    test_mirror_immune_integration.cpp
)
target_link_libraries(unit_cognitive_mirror_neurons_test_mirror_immune_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(unit_cognitive_mirror_neurons_test_mirror_immune_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
target_compile_features(unit_cognitive_mirror_neurons_test_mirror_immune_integration PRIVATE cxx_std_17)
add_test(NAME unit_cognitive_mirror_neurons_test_mirror_immune_integration
         COMMAND unit_cognitive_mirror_neurons_test_mirror_immune_integration)
set_tests_properties(unit_cognitive_mirror_neurons_test_mirror_immune_integration PROPERTIES
    TIMEOUT 120
    LABELS "unit;cognitive;mirror_neurons;immune;integration"
)

# Mirror Neuron - Sleep Bridge Tests
add_executable(unit_cognitive_mirror_neurons_test_mirror_sleep_bridge
    test_mirror_neurons_sleep_bridge.cpp
)
target_link_libraries(unit_cognitive_mirror_neurons_test_mirror_sleep_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(unit_cognitive_mirror_neurons_test_mirror_sleep_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
        ${CMAKE_SOURCE_DIR}/src/cognitive
)
target_compile_features(unit_cognitive_mirror_neurons_test_mirror_sleep_bridge PRIVATE cxx_std_17)
add_test(NAME unit_cognitive_mirror_neurons_test_mirror_sleep_bridge
         COMMAND unit_cognitive_mirror_neurons_test_mirror_sleep_bridge)
set_tests_properties(unit_cognitive_mirror_neurons_test_mirror_sleep_bridge PROPERTIES
    TIMEOUT 120
    LABELS "unit;cognitive;mirror_neurons;sleep;bridge"
)

# ============================================================================
# Mirror-Emotion Bridge Tests (Phase 10.11.11)
# ============================================================================
# Tests for bidirectional mirror neuron - emotion recognition integration

add_executable(unit_cognitive_mirror_neurons_test_mirror_emotion_bridge
    test_mirror_emotion_bridge.cpp
)
target_link_libraries(unit_cognitive_mirror_neurons_test_mirror_emotion_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(unit_cognitive_mirror_neurons_test_mirror_emotion_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/tensor
)
target_compile_features(unit_cognitive_mirror_neurons_test_mirror_emotion_bridge PRIVATE cxx_std_17)
add_test(NAME unit_cognitive_mirror_neurons_test_mirror_emotion_bridge
         COMMAND unit_cognitive_mirror_neurons_test_mirror_emotion_bridge)
set_tests_properties(unit_cognitive_mirror_neurons_test_mirror_emotion_bridge PROPERTIES
    TIMEOUT 120
    LABELS "unit;cognitive;mirror_neurons;emotion;bridge"
)

# ============================================================================
# Mirror-Attention Bridge Tests (Phase 10.11.12)
# ============================================================================
# Tests for bidirectional mirror neuron - attention system integration

add_executable(unit_cognitive_mirror_neurons_test_mirror_attention_bridge
    test_mirror_attention_bridge.cpp
)
target_link_libraries(unit_cognitive_mirror_neurons_test_mirror_attention_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(unit_cognitive_mirror_neurons_test_mirror_attention_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/tensor
)
target_compile_features(unit_cognitive_mirror_neurons_test_mirror_attention_bridge PRIVATE cxx_std_17)
add_test(NAME unit_cognitive_mirror_neurons_test_mirror_attention_bridge
         COMMAND unit_cognitive_mirror_neurons_test_mirror_attention_bridge)
set_tests_properties(unit_cognitive_mirror_neurons_test_mirror_attention_bridge PROPERTIES
    TIMEOUT 120
    LABELS "unit;cognitive;mirror_neurons;attention;bridge"
)

# ============================================================================
# Mirror-SNN Bridge Tests (SNN Integration)
# ============================================================================
# Tests for bidirectional mirror neuron - SNN module integration

add_executable(unit_cognitive_mirror_neurons_test_mirror_snn_bridge
    test_mirror_snn_bridge.cpp
)
target_link_libraries(unit_cognitive_mirror_neurons_test_mirror_snn_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(unit_cognitive_mirror_neurons_test_mirror_snn_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
        ${CMAKE_SOURCE_DIR}/src/snn
)
target_compile_features(unit_cognitive_mirror_neurons_test_mirror_snn_bridge PRIVATE cxx_std_17)
add_test(NAME unit_cognitive_mirror_neurons_test_mirror_snn_bridge
         COMMAND unit_cognitive_mirror_neurons_test_mirror_snn_bridge)
set_tests_properties(unit_cognitive_mirror_neurons_test_mirror_snn_bridge PROPERTIES
    TIMEOUT 120
    LABELS "unit;cognitive;mirror_neurons;snn;bridge"
)

# ============================================================================
# Mirror-Plasticity Bridge Tests (Plasticity Integration)
# ============================================================================
# Tests for bidirectional mirror neuron - plasticity module integration

add_executable(unit_cognitive_mirror_neurons_test_mirror_plasticity_bridge
    test_mirror_plasticity_bridge.cpp
)
target_link_libraries(unit_cognitive_mirror_neurons_test_mirror_plasticity_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(unit_cognitive_mirror_neurons_test_mirror_plasticity_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/cognitive/mirror_neurons
        ${CMAKE_SOURCE_DIR}/src/plasticity
)
target_compile_features(unit_cognitive_mirror_neurons_test_mirror_plasticity_bridge PRIVATE cxx_std_17)
add_test(NAME unit_cognitive_mirror_neurons_test_mirror_plasticity_bridge
         COMMAND unit_cognitive_mirror_neurons_test_mirror_plasticity_bridge)
set_tests_properties(unit_cognitive_mirror_neurons_test_mirror_plasticity_bridge PROPERTIES
    TIMEOUT 120
    LABELS "unit;cognitive;mirror_neurons;plasticity;bridge"
)
