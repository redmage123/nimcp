# ============================================================================
# Cognitive Integration Tests
# ============================================================================
# WHAT: Integration tests for Cognitive Integration bridges and hub
# WHY:  Validate cross-module integration flows and multi-bridge coordination
# HOW:  Test complete integration scenarios with multiple bridges working together
# ============================================================================

# Hub Module Integration Tests
add_executable(integration_cognitive_test_hub_module_integration
    test_hub_module_integration.cpp
)

target_link_libraries(integration_cognitive_test_hub_module_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_test_hub_module_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_test_hub_module_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_test_hub_module_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;hub;module"
        TIMEOUT 180
)

# Emotion-Memory Integration Tests
add_executable(integration_cognitive_test_emotion_memory_integration
    test_emotion_memory_integration.cpp
)

target_link_libraries(integration_cognitive_test_emotion_memory_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_test_emotion_memory_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_test_emotion_memory_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_test_emotion_memory_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;emotion;memory"
        TIMEOUT 180
)

# Attention-WM Integration Tests
add_executable(integration_cognitive_test_attention_wm_integration
    test_attention_wm_integration.cpp
)

target_link_libraries(integration_cognitive_test_attention_wm_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_test_attention_wm_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_test_attention_wm_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_test_attention_wm_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;attention;wm"
        TIMEOUT 180
)

# Multi-Bridge Integration Tests
add_executable(integration_cognitive_test_multi_bridge_integration
    test_multi_bridge_integration.cpp
)

target_link_libraries(integration_cognitive_test_multi_bridge_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_test_multi_bridge_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_test_multi_bridge_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_test_multi_bridge_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;multi;bridge"
        TIMEOUT 180
)

# Hub Orchestrator Integration Tests
add_executable(integration_cognitive_test_hub_orchestrator_integration
    test_hub_orchestrator_integration.cpp
)

target_link_libraries(integration_cognitive_test_hub_orchestrator_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_test_hub_orchestrator_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_test_hub_orchestrator_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_test_hub_orchestrator_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;hub;orchestrator"
        TIMEOUT 180
)

# Salience-Attention Integration Tests
add_executable(integration_cognitive_test_salience_attention_integration
    test_salience_attention_integration.cpp
)

target_link_libraries(integration_cognitive_test_salience_attention_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_test_salience_attention_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_test_salience_attention_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_test_salience_attention_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;salience;attention"
        TIMEOUT 180
)

# Predictive-Attention Integration Tests
add_executable(integration_cognitive_test_predictive_attention_integration
    test_predictive_attention_integration.cpp
)

target_link_libraries(integration_cognitive_test_predictive_attention_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_test_predictive_attention_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_test_predictive_attention_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_test_predictive_attention_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;predictive;attention"
        TIMEOUT 180
)

# Mirror-Empathy Integration Tests
add_executable(integration_cognitive_test_mirror_empathy_integration
    test_mirror_empathy_integration.cpp
)

target_link_libraries(integration_cognitive_test_mirror_empathy_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_test_mirror_empathy_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_test_mirror_empathy_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_test_mirror_empathy_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;mirror;empathy"
        TIMEOUT 180
)

# Game Theory-Executive Integration Tests
add_executable(integration_cognitive_test_game_theory_executive_integration
    test_game_theory_executive_integration.cpp
)

target_link_libraries(integration_cognitive_test_game_theory_executive_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_test_game_theory_executive_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_test_game_theory_executive_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_test_game_theory_executive_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;game_theory;executive"
        TIMEOUT 180
)

# Imagination-Reasoning Integration Tests
add_executable(integration_cognitive_test_imagination_reasoning_integration
    test_imagination_reasoning_integration.cpp
)

target_link_libraries(integration_cognitive_test_imagination_reasoning_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_test_imagination_reasoning_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_test_imagination_reasoning_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_test_imagination_reasoning_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;imagination;reasoning"
        TIMEOUT 180
)

# ============================================================================
# Tier 3 FEP Bridges Integration Tests
# ============================================================================
# WHAT: Integration tests for Tier 3 FEP bridges (FEP bridges for Tier 2 Hub bridges)
# WHY:  Validate FEP integration with Hub bridges for prediction error minimization
# HOW:  Test complete FEP-Hub bridge integration scenarios
# ============================================================================
add_executable(integration_cognitive_test_tier3_fep_bridges_integration
    test_tier3_fep_bridges_integration.cpp
)

target_link_libraries(integration_cognitive_test_tier3_fep_bridges_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_test_tier3_fep_bridges_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_test_tier3_fep_bridges_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_test_tier3_fep_bridges_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;tier3;fep;bridges"
        TIMEOUT 180
)
