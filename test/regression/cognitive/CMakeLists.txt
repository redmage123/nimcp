# ============================================================================
# Cognitive Regression Tests
# ============================================================================

# Emotion-Cognition Performance Regression Test
add_executable(regression_cognitive_emotion_performance
    test_emotion_performance.cpp
)

target_link_libraries(regression_cognitive_emotion_performance
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_emotion_performance
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_emotion_performance PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_emotion_performance
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;emotion;performance"
        TIMEOUT 180
)

# Positional encoding regression tests for working memory
add_executable(regression_cognitive_working_memory_pe_test
    test_working_memory_pe_regression.cpp
)
target_link_libraries(regression_cognitive_working_memory_pe_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_cognitive_working_memory_pe_test PRIVATE cxx_std_17)
gtest_discover_tests(regression_cognitive_working_memory_pe_test
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;cognitive;working_memory;positional_encoding"
)

# Subdirectories are added from test/CMakeLists.txt to avoid duplicate targets
# when regression/cognitive is included as a subdirectory.
# See test/CMakeLists.txt for the individual subdirectory includes.

# Recursive Cognition Regression Tests (RLM-inspired)
add_subdirectory(recursive)

# Omnidirectional Inference Regression Tests
add_subdirectory(omni)

# Emotion Bridge Regression Tests (SNN and Plasticity)
add_subdirectory(emotion)

# Ethics Bridge Regression Tests (SNN and Plasticity)
add_subdirectory(ethics)

# Attention Bridge Regression Tests (SNN and Plasticity)
add_subdirectory(attention)

# Global Workspace Bridge Regression Tests (SNN and Plasticity)
add_subdirectory(global_workspace)

# Executive Bridge Regression Tests (SNN and Plasticity)
add_subdirectory(executive)

# Self Model Bridge Regression Tests (SNN and Plasticity)
add_subdirectory(self_model)

# Introspection Bridge Regression Tests (SNN and Plasticity)
add_subdirectory(introspection)

# Mirror Neurons Bridge Regression Tests (SNN and Plasticity)
add_subdirectory(mirror_neurons)

# Meta Learning Bridge Regression Tests (SNN and Plasticity)
add_subdirectory(meta_learning)

# Wellbeing Bridge Regression Tests - added from test/CMakeLists.txt (line 621)
# to avoid duplicate add_subdirectory errors

# Theory of Mind Bridge Regression Tests (SNN and Plasticity)
add_subdirectory(theory_of_mind)

# Memory Bridge Regression Tests (SNN and Plasticity)
add_subdirectory(memory)

# Epistemic Bridge Regression Tests (SNN and Plasticity)
add_subdirectory(epistemic)

# Cognitive Bias Bridge Regression Tests (SNN and Plasticity)
add_subdirectory(bias)

# Salience Bridge Regression Tests (SNN and Plasticity)
add_subdirectory(salience)

# Cognitive Integration Hub and Bridges Regression Tests
add_subdirectory(integration)

# Parietal FEP Bridge Regression Tests
add_subdirectory(parietal)

# Imagination FEP Bridge Regression Tests
add_subdirectory(imagination)

# Social Cognition FEP Bridge Regression Tests
add_subdirectory(social)

# Phase 8 Health Cognitive Regression Tests
add_subdirectory(health)

# ============================================================================
# Cognitive Bridge Completeness Tests
# ============================================================================
# DISABLED: test_cognitive_bridge_completeness.cpp not yet implemented
# add_executable(regression_cognitive_bridge_completeness
#     test_cognitive_bridge_completeness.cpp
# )
#
# target_link_libraries(regression_cognitive_bridge_completeness
#     PRIVATE
#         nimcp
#         GTest::gtest
#         GTest::gtest_main
#         Threads::Threads
#         m
# )

# target_include_directories(regression_cognitive_bridge_completeness
#     PRIVATE
#         ${PROJECT_SOURCE_DIR}/include
#         ${PROJECT_SOURCE_DIR}/src
# )
#
# target_compile_features(regression_cognitive_bridge_completeness PRIVATE cxx_std_17)
#
# gtest_discover_tests(regression_cognitive_bridge_completeness
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     PROPERTIES
#         LABELS "regression;cognitive;bridge;completeness"
#         TIMEOUT 180
# )

# ============================================================================
# Cognitive Thalamic Routing Tests
# ============================================================================
add_executable(regression_cognitive_thalamic_routing
    test_cognitive_thalamic_routing.cpp
)

target_link_libraries(regression_cognitive_thalamic_routing
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_thalamic_routing
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_thalamic_routing PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_thalamic_routing
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;thalamic;routing"
        TIMEOUT 180
)

# ============================================================================
# Mental Health Guardian Intervention Regression Tests (Phase 10.5.1)
# ============================================================================
# WHAT: Tests to ensure intervention level thresholds work correctly
# WHY:  Prevent regressions in graduated intervention logic
# HOW:  Test boundary conditions and level transitions

add_executable(regression_cognitive_guardian_intervention
    test_guardian_intervention_regression.cpp
)

target_link_libraries(regression_cognitive_guardian_intervention
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_guardian_intervention
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_guardian_intervention PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_guardian_intervention
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;mental_health;guardian;intervention"
        TIMEOUT 120
)

# ============================================================================
# World Model Regression Tests (Omni and Multimodal)
# ============================================================================
# WHAT: Comprehensive regression tests for World Model modules
# WHY:  Test edge cases, error handling, boundary conditions, memory safety
# HOW:  Cover NULL pointers, limits, numerical stability, state consistency

add_executable(regression_cognitive_world_model
    test_world_model_regression.cpp
)

target_link_libraries(regression_cognitive_world_model
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_world_model
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_world_model PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_world_model
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;world_model;omni;multimodal"
        TIMEOUT 300
)

# ============================================================================
# Neuro-Symbolic Mathematics Regression Tests
# ============================================================================
add_subdirectory(neuro_symbolic)

# ============================================================================
# Predictive Bridge Exception Regression Tests
# ============================================================================
# WHAT: Regression tests for predictive bridge API contract stability
# WHY:  Ensure exception handling behavior does not regress across versions
# HOW:  Test error codes, return values, and API contracts
# ============================================================================

add_subdirectory(predictive)

# Inner Dialogue Engine Regression Tests
add_subdirectory(inner_dialogue)

# ============================================================================
# Creative Cortex Regression Tests (P0-4/P0-5/P0-6)
# ============================================================================
# WHAT: Regression tests for archetype name mapping bugs and overflow protection
# WHY:  Lock down fixes for six wrong archetype names and image overflow
# HOW:  Assert correct names AND assert old wrong names are NOT returned

add_executable(regression_cognitive_creative
    test_creative_regression.cpp
)

target_link_libraries(regression_cognitive_creative
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_creative
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_cognitive_creative PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_creative
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;creative"
        TIMEOUT 120
)

# ============================================================================
# Cognitive & Training Walkthrough P1/P2/P3 Regression Tests
# ============================================================================
# WHAT: Regression tests for walkthrough P1-P3 fixes across cognitive/training modules
# WHY:  Lock down buffer overflow, OOB access, memory leak, dangling pointer fixes
# HOW:  Test capacity limits, boundary conditions, memory safety, false positive removal

add_executable(regression_cognitive_training_walkthrough
    test_cognitive_training_walkthrough_fixes.cpp
)

target_link_libraries(regression_cognitive_training_walkthrough
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_training_walkthrough
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_cognitive_training_walkthrough PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_training_walkthrough
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;training;walkthrough"
        TIMEOUT 120
)

# ============================================================================
# Cognitive Pass3 Fix Regression Tests (P1/P2/P3 fixes 2026-02-08)
# ============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_cognitive_pass3_fixes.cpp")
add_executable(regression_cognitive_pass3_fixes
    test_cognitive_pass3_fixes.cpp
)

target_link_libraries(regression_cognitive_pass3_fixes
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_pass3_fixes
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_cognitive_pass3_fixes PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_pass3_fixes
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;pass3"
        TIMEOUT 120
)
endif()

# ============================================================================
# Community Cache Regression Tests
# ============================================================================
add_executable(regression_cognitive_community_cache
    consolidation/test_community_cache_regression.cpp
)

target_link_libraries(regression_cognitive_community_cache
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_community_cache
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_cognitive_community_cache PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_community_cache
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;consolidation;community_cache"
        TIMEOUT 120
)

# ============================================================================
# Cognitive Pipeline Regression Tests
# ============================================================================

add_executable(regression_cognitive_pipeline
    test_cognitive_regression.cpp
)
target_link_libraries(regression_cognitive_pipeline
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(regression_cognitive_pipeline
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(regression_cognitive_pipeline PRIVATE cxx_std_17)
gtest_discover_tests(regression_cognitive_pipeline
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;pipeline"
        TIMEOUT 120
)
