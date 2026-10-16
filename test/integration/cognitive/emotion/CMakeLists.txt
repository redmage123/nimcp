# Cognitive Emotion Integration Tests
# ============================================================================
# Emotion System - SNN and Plasticity Integration Testing
# ============================================================================

# ============================================================================
# Emotion-SNN-Plasticity Integration Tests
# ============================================================================
# Bidirectional dataflow integration between emotion system, SNN, and plasticity

add_executable(integration_cognitive_emotion_snn_plasticity
    test_emotion_snn_plasticity_integration.cpp
)
target_link_libraries(integration_cognitive_emotion_snn_plasticity
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_emotion_snn_plasticity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/time
        ${CMAKE_SOURCE_DIR}/src/cognitive/emotion
        ${CMAKE_SOURCE_DIR}/src/snn
        ${CMAKE_SOURCE_DIR}/src/plasticity
)
target_compile_features(integration_cognitive_emotion_snn_plasticity PRIVATE cxx_std_17)
add_test(NAME integration_cognitive_emotion_snn_plasticity
         COMMAND integration_cognitive_emotion_snn_plasticity)
set_tests_properties(integration_cognitive_emotion_snn_plasticity PROPERTIES
    TIMEOUT 180
    LABELS "integration;cognitive;emotion;snn;plasticity"
)
