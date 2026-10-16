# ============================================================================
# Salience Integration Tests
# ============================================================================
# WHAT: Integration tests for Salience SNN-Plasticity bidirectional system
# WHY:  Verify attention learning improves through experience
# HOW:  Test novelty-habituation, value learning, multi-channel processing

add_executable(integration_cognitive_salience_snn_plasticity
    test_salience_snn_plasticity_integration.cpp
)

target_link_libraries(integration_cognitive_salience_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_salience_snn_plasticity
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_salience_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_salience_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;salience;snn;plasticity"
        TIMEOUT 180
)

# ============================================================================
# Surprise Amplifier Integration Tests (Society of Thought Phase 1)
# ============================================================================

add_executable(integration_cognitive_surprise_amplifier
    test_surprise_amplifier_integration.cpp
)

target_link_libraries(integration_cognitive_surprise_amplifier
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_surprise_amplifier
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_surprise_amplifier PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_surprise_amplifier
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;salience;surprise;society_of_thought"
        TIMEOUT 180
)

# ============================================================================
# Surprise Bridges Integration Tests (Society of Thought Phase 1)
# ============================================================================

add_executable(integration_cognitive_surprise_bridges
    test_surprise_bridges_integration.cpp
)

target_link_libraries(integration_cognitive_surprise_bridges
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_surprise_bridges
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_surprise_bridges PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_surprise_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;salience;surprise;bridges;society_of_thought"
        TIMEOUT 180
)

# ============================================================================
# Surprise Bridges Phase 1b Integration Tests (Society of Thought)
# ============================================================================

add_executable(integration_cognitive_surprise_bridges_phase1b
    test_surprise_bridges_phase1b_integration.cpp
)

target_link_libraries(integration_cognitive_surprise_bridges_phase1b
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_surprise_bridges_phase1b
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_surprise_bridges_phase1b PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_surprise_bridges_phase1b
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;salience;surprise;bridges;phase1b;society_of_thought"
        TIMEOUT 180
)
