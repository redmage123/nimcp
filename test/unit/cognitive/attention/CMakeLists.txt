# Attention Unit Tests

# Note: Sleep bridge tests are added directly from parent cognitive/CMakeLists.txt
# to avoid target name conflicts

# Substrate bridge tests
add_subdirectory(substrate)

# ============================================================================
# Attention-SNN Bridge Tests
# ============================================================================
# WHAT: Unit tests for Attention System - SNN Bidirectional Integration Bridge
# WHY:  Validate spike encoding/decoding of attention weights and salience maps
# HOW:  Test lifecycle, encoding, simulation, decoding, modulation, bio-async
# ============================================================================

add_executable(unit_cognitive_attention_snn_bridge
    test_attention_snn_bridge.cpp
)

target_link_libraries(unit_cognitive_attention_snn_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_attention_snn_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_attention_snn_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_attention_snn_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;attention;snn;bridge"
        TIMEOUT 120
)

# ============================================================================
# Attention-Plasticity Bridge Tests
# ============================================================================
# WHAT: Unit tests for Attention System - Plasticity Bidirectional Bridge
# WHY:  Validate attention-modulated learning through STDP, BCM, habituation
# HOW:  Test lifecycle, synapse management, events, STDP, callbacks, bio-async
# ============================================================================

add_executable(unit_cognitive_attention_plasticity_bridge
    test_attention_plasticity_bridge.cpp
)

target_link_libraries(unit_cognitive_attention_plasticity_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_attention_plasticity_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_attention_plasticity_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_attention_plasticity_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;attention;plasticity;bridge"
        TIMEOUT 120
)
