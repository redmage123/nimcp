# ============================================================================
# Cognitive Integration Tests
# ============================================================================

# Emotion-Cognition Integration Test
add_executable(integration_cognitive_emotion_integration
    test_emotion_integration.cpp
)

target_link_libraries(integration_cognitive_emotion_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_emotion_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_emotion_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_emotion_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;emotion"
        TIMEOUT 120
)

# Working Memory + Positional Encoding Integration Test
add_executable(integration_cognitive_working_memory_pe
    test_working_memory_pe_integration.cpp
)

target_link_libraries(integration_cognitive_working_memory_pe
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_working_memory_pe
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_working_memory_pe PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_working_memory_pe
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;working_memory;positional_encoding"
        TIMEOUT 120
)

# Note: Subdirectories (logic, introspection, fault_tolerance, reasoning, bio_async, mirror_neurons)
# are added directly from the main test/CMakeLists.txt to avoid duplicate target issues

# Free Energy Principle Enhancement integration tests
add_subdirectory(free_energy)
add_subdirectory(autobiographical_memory)

# Cognitive Substrate Bridges Integration
add_subdirectory(substrate)

# Game Theory Integration
add_subdirectory(game_theory)

# Parietal Lobe Integration
add_subdirectory(parietal)

# Cross-Bridge Integration (thalamic-substrate, bio-async, metabolic cascade)
add_subdirectory(cross_bridge)

# Collective Cognition Integration Tests
add_subdirectory(collective_cognition)

# Recursive Cognition Integration Tests (RLM-inspired)
add_subdirectory(recursive)

# Omnidirectional Inference Integration Tests
add_subdirectory(omni)

# Mental Health Guardian Integration Tests (Phase 10.5.1)
add_subdirectory(mental_health)

# Emotion SNN-Plasticity Integration Tests
add_subdirectory(emotion)

# Ethics SNN-Plasticity Integration Tests
add_subdirectory(ethics)

# Attention SNN-Plasticity Integration Tests
add_subdirectory(attention)

# Memory SNN-Plasticity Integration Tests
add_subdirectory(memory)

# Executive SNN-Plasticity Integration Tests
add_subdirectory(executive)

# Global Workspace SNN-Plasticity Integration Tests
add_subdirectory(global_workspace)

# Theory of Mind SNN-Plasticity Integration Tests
add_subdirectory(theory_of_mind)

# Introspection SNN-Plasticity Integration Tests
add_subdirectory(introspection)

# Meta Learning SNN-Plasticity Integration Tests
add_subdirectory(meta_learning)

# Mirror Neurons SNN-Plasticity Integration Tests
add_subdirectory(mirror_neurons)

# Self Model SNN-Plasticity Integration Tests
add_subdirectory(self_model)

# NOTE: Wellbeing and Memory integration tests are added via test/CMakeLists.txt to avoid duplicates

# Curiosity - FEP Bridge Integration Test
add_executable(integration_cognitive_curiosity_fep_bridge
    curiosity/test_curiosity_fep_bridge_integration.cpp
)

target_link_libraries(integration_cognitive_curiosity_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_curiosity_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_curiosity_fep_bridge PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_curiosity_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;curiosity;fep"
        TIMEOUT 120
)

# Information Forager Integration Test
add_executable(integration_cognitive_information_forager
    curiosity/test_information_forager_integration.cpp
)

target_link_libraries(integration_cognitive_information_forager
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_information_forager
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_information_forager PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_information_forager
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;curiosity;forager"
        TIMEOUT 120
)

# Enhanced Curiosity Integration Test
add_executable(integration_cognitive_curiosity_enhanced
    curiosity/test_curiosity_enhanced_integration.cpp
)

target_link_libraries(integration_cognitive_curiosity_enhanced
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_curiosity_enhanced
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_curiosity_enhanced PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_curiosity_enhanced
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;curiosity;enhanced"
        TIMEOUT 180
)

# Curiosity SNN-Plasticity Integration Tests
add_executable(integration_cognitive_curiosity_snn_plasticity
    curiosity/test_curiosity_snn_plasticity_integration.cpp
)

target_link_libraries(integration_cognitive_curiosity_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_curiosity_snn_plasticity
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_curiosity_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_curiosity_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;curiosity;snn;plasticity"
        TIMEOUT 180
)

# Epistemic SNN-Plasticity Integration Tests
add_subdirectory(epistemic)

# Cognitive Bias SNN-Plasticity Integration Tests
add_subdirectory(bias)

# Salience SNN-Plasticity Integration Tests
add_subdirectory(salience)

# Cognitive Integration Hub and Bridges Integration Tests
add_subdirectory(integration)

# Imagination FEP Integration Tests
add_subdirectory(imagination)

# Social Cognition FEP Bridge Integration Tests
add_subdirectory(social)

# Exception Handling Integration Tests
add_subdirectory(exception)

# World Model Integration Tests
add_subdirectory(world_model)

# Phase 8 Health Cognitive Integration Tests
add_subdirectory(health)

# Neuro-Symbolic Mathematics Integration Tests (Mathesis-Inspired)
add_subdirectory(neuro_symbolic)

# ============================================================================
# Predictive Bridge Exception Integration Tests
# ============================================================================
# WHAT: Integration tests for predictive bridge exception propagation and recovery
# WHY:  Verify cross-bridge exception handling and recovery scenarios
# HOW:  Test multi-bridge workflows with exception injection
# ============================================================================

add_subdirectory(predictive)

# Inner Dialogue Engine Integration Tests
add_subdirectory(inner_dialogue)

# VAE-FEP Bridge Integration Tests
add_subdirectory(vae)

# ============================================================================
# Community Cache Integration Tests
# ============================================================================
add_executable(integration_cognitive_community_cache
    consolidation/test_community_cache_integration.cpp
)

target_link_libraries(integration_cognitive_community_cache
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_community_cache
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_community_cache PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_community_cache
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;consolidation;community_cache"
        TIMEOUT 120
)

# ============================================================================
# Cognitive Pipeline Integration Tests
# ============================================================================

add_executable(integration_cognitive_pipeline
    test_cognitive_pipeline_integration.cpp
)
target_link_libraries(integration_cognitive_pipeline
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(integration_cognitive_pipeline
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(integration_cognitive_pipeline PRIVATE cxx_std_17)
gtest_discover_tests(integration_cognitive_pipeline
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;pipeline"
        TIMEOUT 120
)

# Language Pipeline Integration
add_executable(integration_cognitive_language_pipeline test_language_pipeline_integration.cpp test_linker_stubs.c)
target_link_libraries(integration_cognitive_language_pipeline PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(integration_cognitive_language_pipeline PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(integration_cognitive_language_pipeline PRIVATE cxx_std_17)
gtest_discover_tests(integration_cognitive_language_pipeline WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "integration;cognitive;language" TIMEOUT 120)
