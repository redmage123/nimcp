# Emotion Unit Tests
# ============================================================================
# Tests for Emotion System Bridges and Integration
# ============================================================================

# Substrate bridge tests
add_subdirectory(substrate)

# ============================================================================
# Emotion-Plasticity Bridge Tests
# ============================================================================
# Tests for bidirectional emotion system - plasticity module integration

add_executable(unit_cognitive_emotion_test_emotion_plasticity_bridge
    test_emotion_plasticity_bridge.cpp
)
target_link_libraries(unit_cognitive_emotion_test_emotion_plasticity_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(unit_cognitive_emotion_test_emotion_plasticity_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/cognitive/emotion
        ${CMAKE_SOURCE_DIR}/src/plasticity
)
target_compile_features(unit_cognitive_emotion_test_emotion_plasticity_bridge PRIVATE cxx_std_17)
add_test(NAME unit_cognitive_emotion_test_emotion_plasticity_bridge
         COMMAND unit_cognitive_emotion_test_emotion_plasticity_bridge)
set_tests_properties(unit_cognitive_emotion_test_emotion_plasticity_bridge PROPERTIES
    TIMEOUT 120
    LABELS "unit;cognitive;emotion;plasticity;bridge"
)

# ============================================================================
# Emotion-SNN Bridge Tests
# ============================================================================
# Tests for bidirectional emotion system - SNN module integration

add_executable(unit_cognitive_emotion_test_emotion_snn_bridge
    test_emotion_snn_bridge.cpp
)
target_link_libraries(unit_cognitive_emotion_test_emotion_snn_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(unit_cognitive_emotion_test_emotion_snn_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/cognitive/emotion
        ${CMAKE_SOURCE_DIR}/src/snn
)
target_compile_features(unit_cognitive_emotion_test_emotion_snn_bridge PRIVATE cxx_std_17)
add_test(NAME unit_cognitive_emotion_test_emotion_snn_bridge
         COMMAND unit_cognitive_emotion_test_emotion_snn_bridge)
set_tests_properties(unit_cognitive_emotion_test_emotion_snn_bridge PROPERTIES
    TIMEOUT 120
    LABELS "unit;cognitive;emotion;snn;bridge"
)

# ============================================================================
# Health Emotion Bridge Tests (Phase 9: Section 28)
# ============================================================================
# Tests for health agent emotion integration (thresholds, shadow detection)

add_executable(unit_cognitive_emotion_test_health_emotion_bridge_functions
    test_health_emotion_bridge_functions.cpp
)
target_link_libraries(unit_cognitive_emotion_test_health_emotion_bridge_functions
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        m
        pthread
)
target_include_directories(unit_cognitive_emotion_test_health_emotion_bridge_functions
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_emotion_test_health_emotion_bridge_functions PRIVATE cxx_std_17)
add_test(NAME unit_cognitive_emotion_test_health_emotion_bridge_functions
         COMMAND unit_cognitive_emotion_test_health_emotion_bridge_functions)
set_tests_properties(unit_cognitive_emotion_test_health_emotion_bridge_functions PROPERTIES
    TIMEOUT 120
    LABELS "unit;cognitive;emotion;health;bridge;phase9"
)
