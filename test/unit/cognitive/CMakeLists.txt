# Unit tests for cognitive module bio-async integration

# Cognitive bio-async integration tests
add_executable(unit_cognitive_test_bio_async
    test_cognitive_bio_async.cpp
)

target_link_libraries(unit_cognitive_test_bio_async
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_bio_async
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_bio_async PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_bio_async
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;bio-async"
        TIMEOUT 90
)

# Cognitive bio-async registration tests
add_executable(unit_cognitive_test_bio_async_registration
    test_cognitive_bio_async_registration.cpp
)
target_link_libraries(unit_cognitive_test_bio_async_registration
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_cognitive_test_bio_async_registration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)
gtest_discover_tests(unit_cognitive_test_bio_async_registration)

# Cognitive Meta-Controller Tests
add_executable(unit_cognitive_test_meta_controller
    test_cognitive_meta_controller.cpp
)

target_link_libraries(unit_cognitive_test_meta_controller
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_meta_controller
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_meta_controller PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_meta_controller
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;meta_controller"
        TIMEOUT 120
)

# Emotion-Attention Integration Tests
# DISABLED: Source file nimcp_emotion_attention.c not yet implemented
message(STATUS "  [SKIP] test_emotion_attention.cpp - source file not implemented")

# Emotion-Consolidation Integration Tests
# DISABLED: Source file nimcp_emotion_consolidation.c not yet implemented
message(STATUS "  [SKIP] test_emotion_consolidation.cpp - source file not implemented")

# Working Memory subdirectory (PE tests)
add_subdirectory(working_memory)

# Wellbeing tests
add_subdirectory(wellbeing)

# Mental Health tests
add_subdirectory(mental_health)

# Working Memory - Immune Integration Tests
add_executable(unit_cognitive_test_working_memory_immune
    test_working_memory_immune_integration.cpp
)

target_link_libraries(unit_cognitive_test_working_memory_immune
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_working_memory_immune
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_working_memory_immune PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_working_memory_immune
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;working_memory;immune"
        TIMEOUT 120
)

# =============================================================================
# Reasoning Chain Unit Tests
# =============================================================================

add_executable(unit_cognitive_test_reasoning_chain
    test_reasoning_chain.cpp
)

target_link_libraries(unit_cognitive_test_reasoning_chain
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_reasoning_chain
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_reasoning_chain PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_reasoning_chain
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning"
        TIMEOUT 120
)

# Reasoning Chain - Symbolic Logic Unit Tests
add_executable(unit_cognitive_test_reasoning_symbolic_logic
    test_reasoning_symbolic_logic.cpp
)

target_link_libraries(unit_cognitive_test_reasoning_symbolic_logic
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_reasoning_symbolic_logic
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_reasoning_symbolic_logic PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_reasoning_symbolic_logic
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning;symbolic-logic"
        TIMEOUT 60
)

# Reasoning Chain - Portia Bridge Unit Tests
add_executable(unit_cognitive_test_reasoning_portia_bridge
    test_reasoning_portia_bridge.cpp
)

target_link_libraries(unit_cognitive_test_reasoning_portia_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_reasoning_portia_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_reasoning_portia_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_reasoning_portia_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning;portia-bridge"
        TIMEOUT 60
)

# Reasoning Chain - Hypothalamus Bridge Unit Tests
add_executable(unit_cognitive_test_reasoning_hypo_bridge
    test_reasoning_hypo_bridge.cpp
)

target_link_libraries(unit_cognitive_test_reasoning_hypo_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_reasoning_hypo_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_reasoning_hypo_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_reasoning_hypo_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning;hypo-bridge"
        TIMEOUT 60
)

# Reasoning Chain - Mesh Bridge Unit Tests
add_executable(unit_cognitive_test_reasoning_mesh_bridge
    test_reasoning_mesh_bridge.cpp
)

target_link_libraries(unit_cognitive_test_reasoning_mesh_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(unit_cognitive_test_reasoning_mesh_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_reasoning_mesh_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_reasoning_mesh_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning-mesh-bridge"
        TIMEOUT 90
)

# Reasoning Chain - Convergent Evidence Accumulation Unit Tests
add_executable(unit_cognitive_test_reasoning_convergent
    test_reasoning_convergent.cpp
)

target_link_libraries(unit_cognitive_test_reasoning_convergent
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(unit_cognitive_test_reasoning_convergent
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_reasoning_convergent PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_reasoning_convergent
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning-convergent"
        TIMEOUT 90
)

# Reasoning Chain - Affective Modulation Unit Tests
add_executable(unit_cognitive_test_reasoning_affective
    test_reasoning_affective.cpp
)

target_link_libraries(unit_cognitive_test_reasoning_affective
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(unit_cognitive_test_reasoning_affective
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_reasoning_affective PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_reasoning_affective
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning-affective"
        TIMEOUT 90
)

# Reasoning Chain - Abductive Reasoning Unit Tests
add_executable(unit_cognitive_test_reasoning_abduction
    test_reasoning_abduction.cpp
)

target_link_libraries(unit_cognitive_test_reasoning_abduction
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(unit_cognitive_test_reasoning_abduction
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_reasoning_abduction PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_reasoning_abduction
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning-abduction"
        TIMEOUT 90
)

# Reasoning Chain - Confidence Calibration Unit Tests
add_executable(unit_cognitive_test_reasoning_calibration
    test_reasoning_calibration.cpp
)

target_link_libraries(unit_cognitive_test_reasoning_calibration
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(unit_cognitive_test_reasoning_calibration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_reasoning_calibration PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_reasoning_calibration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning-calibration"
        TIMEOUT 90
)

# Reasoning Chain - Metacognitive Strategy Selection Unit Tests
add_executable(unit_cognitive_test_reasoning_metacognition
    test_reasoning_metacognition.cpp
)

target_link_libraries(unit_cognitive_test_reasoning_metacognition
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(unit_cognitive_test_reasoning_metacognition
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_reasoning_metacognition PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_reasoning_metacognition
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning-metacognition"
        TIMEOUT 90
)

# Brain Immune System tests - Added via test/CMakeLists.txt to avoid duplicate
# add_subdirectory(immune)

# Predictive-Immune Integration tests
add_subdirectory(predictive_immune)

# Free Energy Principle Enhancement tests
add_subdirectory(free_energy)

# Autobiographical Memory tests
add_subdirectory(autobiographical_memory)

# Global Workspace - Immune Integration Tests
add_executable(unit_cognitive_test_global_workspace_immune
    global_workspace/test_global_workspace_immune_integration.cpp
)

target_link_libraries(unit_cognitive_test_global_workspace_immune
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_global_workspace_immune
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_global_workspace_immune PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_global_workspace_immune
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;global_workspace;immune"
        TIMEOUT 120
)

# Curiosity - Immune Integration Tests
add_executable(unit_cognitive_test_curiosity_immune
    curiosity/test_curiosity_immune_integration.cpp
)

target_link_libraries(unit_cognitive_test_curiosity_immune
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_curiosity_immune
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_curiosity_immune PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_curiosity_immune
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;curiosity;immune"
        TIMEOUT 120
)

# Curiosity - FEP Bridge Tests
add_executable(unit_cognitive_test_curiosity_fep_bridge
    curiosity/test_curiosity_fep_bridge.cpp
)

target_link_libraries(unit_cognitive_test_curiosity_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_curiosity_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_curiosity_fep_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_curiosity_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;curiosity;fep"
        TIMEOUT 120
)

# ============================================================================
# Information Forager Unit Tests
# ============================================================================
add_executable(unit_cognitive_test_information_forager
    curiosity/test_information_forager.cpp
)

target_link_libraries(unit_cognitive_test_information_forager
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_information_forager
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_information_forager PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_information_forager
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;curiosity;forager"
        TIMEOUT 120
)

# ============================================================================
# Enhanced Curiosity Tests (10 Enhancements)
# ============================================================================
# WHAT: Unit tests for Enhanced Curiosity System
# WHY:  Validate 10 curiosity enhancements (boredom, interest decay, types,
#       anxiety balance, social, meta, contagion, surprise, fatigue, counterfactual)
# HOW:  Test each enhancement plus integration, thread safety, edge cases
# ============================================================================

add_executable(unit_cognitive_test_curiosity_enhanced
    curiosity/test_curiosity_enhanced.cpp
)

target_link_libraries(unit_cognitive_test_curiosity_enhanced
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_curiosity_enhanced
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_curiosity_enhanced PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_curiosity_enhanced
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;curiosity;enhanced"
        TIMEOUT 180
)

# ============================================================================
# Curiosity SNN Bridge Tests
# ============================================================================
# WHAT: Unit tests for Curiosity-SNN bidirectional integration
# WHY:  Verify curiosity dimension encoding, novelty detection, exploration drive
# HOW:  Test lifecycle, encoding, simulation, detection, callbacks
# ============================================================================

add_executable(unit_cognitive_test_curiosity_snn_bridge
    curiosity/test_curiosity_snn_bridge.cpp
)

target_link_libraries(unit_cognitive_test_curiosity_snn_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_curiosity_snn_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_cognitive_test_curiosity_snn_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_curiosity_snn_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;curiosity;snn"
        TIMEOUT 120
)

# ============================================================================
# Curiosity Plasticity Bridge Tests
# ============================================================================
# WHAT: Unit tests for Curiosity-Plasticity bidirectional integration
# WHY:  Verify learning, STDP, synapse protection, reward modulation
# HOW:  Test lifecycle, synapse management, learning, consolidation
# ============================================================================

add_executable(unit_cognitive_test_curiosity_plasticity_bridge
    curiosity/test_curiosity_plasticity_bridge.cpp
)

target_link_libraries(unit_cognitive_test_curiosity_plasticity_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_curiosity_plasticity_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_cognitive_test_curiosity_plasticity_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_curiosity_plasticity_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;curiosity;plasticity"
        TIMEOUT 120
)

# ============================================================================
# Cognitive FEP Bridge Tests (Comprehensive)
# ============================================================================
# WHAT: Unit tests for Cognitive-FEP bidirectional integration bridges
# WHY:  Ensure cognitive systems integrate with FEP predictions and precision
# HOW:  Test lifecycle, effects, and bio-async for consolidation, hierarchical,
#       meta-learning, personality, wellbeing, sleep-wake, and brain immune
# ============================================================================

add_executable(unit_cognitive_test_fep_bridges
    test_cognitive_fep_bridges.cpp
)

target_link_libraries(unit_cognitive_test_fep_bridges
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_fep_bridges
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_fep_bridges PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_fep_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;fep;bridge"
        TIMEOUT 120
)

# ============================================================================
# Sleep Bridge Integration Tests
# ============================================================================
# WHAT: Unit tests for Sleep-Cognitive integration bridges
# WHY:  Ensure cognitive functions correctly modulate based on sleep state
# HOW:  Test helper functions, default config, biological validity
# ============================================================================

# Attention Sleep Bridge Tests
add_executable(unit_cognitive_attention_sleep_bridge
    attention/sleep/test_attention_sleep_bridge.cpp
)
target_link_libraries(unit_cognitive_attention_sleep_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_cognitive_attention_sleep_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_attention_sleep_bridge
    PROPERTIES LABELS "unit;cognitive;attention;sleep")

# Working Memory Sleep Bridge Tests
add_executable(unit_cognitive_working_memory_sleep_bridge
    working_memory/sleep/test_working_memory_sleep_bridge.cpp
)
target_link_libraries(unit_cognitive_working_memory_sleep_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_cognitive_working_memory_sleep_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_working_memory_sleep_bridge
    PROPERTIES LABELS "unit;cognitive;working_memory;sleep")

# Executive Sleep Bridge Tests
add_executable(unit_cognitive_executive_sleep_bridge
    executive/sleep/test_executive_sleep_bridge.cpp
)
target_link_libraries(unit_cognitive_executive_sleep_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_cognitive_executive_sleep_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_executive_sleep_bridge
    PROPERTIES LABELS "unit;cognitive;executive;sleep")

# ============================================================================
# Substrate Bridge Tests (Phase 6)
# ============================================================================
# WHAT: Unit tests for Neural Substrate integration bridges
# WHY:  Ensure cognitive systems correctly modulate based on metabolic state
# HOW:  Test ATP, temperature, fatigue effects on cognitive functions
# ============================================================================

# Attention tests (includes substrate bridge)
add_subdirectory(attention)

# Emotion tests (includes substrate bridge)
add_subdirectory(emotion)

# Emotions tests (emotional system thread safety)
add_subdirectory(emotions)

# Ethics tests (SNN/plasticity bridges)
add_subdirectory(ethics)

# KG integration wave tests (W1 statue-finish and later waves)
add_subdirectory(kg)

# Executive tests (includes substrate bridge)
add_subdirectory(executive)

# Note: introspection and reasoning directories are added from test/CMakeLists.txt
# Their substrate subdirectories are included via those parent CMakeLists

# Memory tests (includes substrate bridge)
add_subdirectory(memory)

# Theory of Mind tests (includes substrate bridge)
add_subdirectory(tom)

# Theory of Mind SNN/Plasticity bridge tests
add_subdirectory(theory_of_mind)

# Parietal Lobe tests (Phase 7.2 - Mathematical/Scientific Reasoning)
add_subdirectory(parietal)

# Meta Learning SNN/Plasticity bridge tests
add_subdirectory(meta_learning)

# Note: introspection and mirror_neurons subdirectories are added from
# test/CMakeLists.txt to avoid duplicate target issues

# Self Model SNN/Plasticity bridge tests
add_subdirectory(self_model)

# ============================================================================
# Self-Awareness Coordinator Tests (Phase 1: Self-Awareness Unification)
# ============================================================================
# WHAT: Unit tests for the Self-Awareness Coordinator and Feedback System
# WHY:  Ensure unified self-awareness components work correctly
# HOW:  Test coordinator lifecycle, feedback loops, coherence, phi monitoring
# ============================================================================

add_executable(unit_cognitive_self_awareness_coordinator
    test_self_awareness_coordinator.cpp
)
target_link_libraries(unit_cognitive_self_awareness_coordinator
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_cognitive_self_awareness_coordinator
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)
target_compile_features(unit_cognitive_self_awareness_coordinator PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_self_awareness_coordinator
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;self-awareness;coordinator"
        TIMEOUT 60
)

# ============================================================================
# JEPA Latent Space Tests
# ============================================================================
# WHAT: Unit tests for JEPA Latent Space Representation Module
# WHY:  Latent space is core to JEPA prediction/comparison - must be correct
# HOW:  Test creation, similarity, interpolation, normalization, pooling
# ============================================================================

add_executable(unit_cognitive_jepa_latent
    jepa/test_jepa_latent.cpp
)

target_link_libraries(unit_cognitive_jepa_latent
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_jepa_latent
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_cognitive_jepa_latent PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_jepa_latent
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;jepa;latent"
        TIMEOUT 120
)

# ============================================================================
# JEPA Masking Tests
# ============================================================================
# WHAT: Unit tests for JEPA Masking Module
# WHY:  Validate all masking strategies (random, block, attention, curriculum,
#       tube, causal) and mask operations for self-supervised learning
# HOW:  Test lifecycle, all strategies, mask operations, edge cases
# ============================================================================

add_executable(unit_cognitive_jepa_masking
    jepa/test_jepa_masking.cpp
)

target_link_libraries(unit_cognitive_jepa_masking
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_jepa_masking
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_cognitive_jepa_masking PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_jepa_masking
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;jepa;masking"
        TIMEOUT 120
)

# ============================================================================
# JEPA Multimodal Tests
# ============================================================================
# WHAT: Unit tests for Multimodal JEPA module
# WHY:  Validate joint embedding space for visual and speech modalities,
#       cross-modal prediction, alignment training, and fusion strategies
# HOW:  Test creation, encoding, fusion (all types), similarity, prediction,
#       alignment training, batch management, edge cases
# ============================================================================

add_executable(unit_cognitive_jepa_multimodal
    jepa/test_jepa_multimodal.cpp
)

target_link_libraries(unit_cognitive_jepa_multimodal
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_jepa_multimodal
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_cognitive_jepa_multimodal PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_jepa_multimodal
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;jepa;multimodal"
        TIMEOUT 180
)

# ============================================================================
# JEPA Predictor Tests
# ============================================================================
# WHAT: Unit tests for JEPA Predictor module
# WHY:  Predictor is core to JEPA - predicts embeddings in latent space
# HOW:  Test creation, MLP forward pass, error computation, training, weights
# ============================================================================

add_executable(unit_cognitive_test_jepa_predictor
    jepa/test_jepa_predictor.cpp
)

target_link_libraries(unit_cognitive_test_jepa_predictor
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_jepa_predictor
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_cognitive_test_jepa_predictor PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_jepa_predictor
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;jepa;predictor"
        TIMEOUT 120
)

# ============================================================================
# JEPA Weights Tests
# ============================================================================
# WHAT: Unit tests for JEPA Weight Loading and Management Module
# WHY:  Weight loading/saving is critical for transfer learning from V-JEPA 2
# HOW:  Test file format (NJWT magic), load/save, round-trip, validation
# ============================================================================

add_executable(unit_cognitive_jepa_weights
    jepa/test_jepa_weights.cpp
)

target_link_libraries(unit_cognitive_jepa_weights
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_jepa_weights
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_definitions(unit_cognitive_jepa_weights PRIVATE NIMCP_TESTING)
target_compile_features(unit_cognitive_jepa_weights PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_jepa_weights
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;jepa;weights"
        TIMEOUT 120
)

# ============================================================================
# JEPA Context Tests
# ============================================================================
# WHAT: Unit tests for JEPA Context Encoder module
# WHY:  Context-conditioned encoding enables task-specific representations
# HOW:  Test creation, context setting, encoding with FiLM/cross-attention,
#       bio-async integration, statistics tracking
# ============================================================================

add_executable(unit_cognitive_test_jepa_context
    jepa/test_jepa_context.cpp
)

target_link_libraries(unit_cognitive_test_jepa_context
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_jepa_context
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_definitions(unit_cognitive_test_jepa_context PRIVATE NIMCP_TESTING)
target_compile_features(unit_cognitive_test_jepa_context PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_jepa_context
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;jepa;context"
        TIMEOUT 180
)

# ============================================================================
# Game Theory Tests
# ============================================================================
# WHAT: Unit tests for Game Theory module
# WHY:  Validate auction mechanisms, bargaining solutions, credit assignment
# HOW:  Test auctions (second-price, VCG), Nash bargaining, Shapley values
# ============================================================================

# Game Theory - Auction Tests
add_executable(unit_cognitive_test_game_theory_auction
    game_theory/test_auction.cpp
)

target_link_libraries(unit_cognitive_test_game_theory_auction
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_game_theory_auction
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_cognitive_test_game_theory_auction PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_game_theory_auction
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;game_theory;auction"
        TIMEOUT 120
)

# Game Theory - Bargaining Tests
add_executable(unit_cognitive_test_game_theory_bargaining
    game_theory/test_bargaining.cpp
)

target_link_libraries(unit_cognitive_test_game_theory_bargaining
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_game_theory_bargaining
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_cognitive_test_game_theory_bargaining PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_game_theory_bargaining
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;game_theory;bargaining"
        TIMEOUT 120
)

# Game Theory - Credit Assignment Tests
add_executable(unit_cognitive_test_game_theory_credit
    game_theory/test_credit_assignment.cpp
)

target_link_libraries(unit_cognitive_test_game_theory_credit
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_game_theory_credit
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_cognitive_test_game_theory_credit PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_game_theory_credit
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;game_theory;credit"
        TIMEOUT 120
)

# Game Theory - Integration Tests
add_executable(unit_cognitive_test_game_theory_integration
    game_theory/test_gt_integration.cpp
)

target_link_libraries(unit_cognitive_test_game_theory_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_game_theory_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_cognitive_test_game_theory_integration PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_game_theory_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;game_theory;integration"
        TIMEOUT 120
)

# ============================================================================
# Advanced Game Theory Module Tests
# ============================================================================
# WHAT: Unit tests for advanced game theory modules
# WHY:  Validate equilibrium computation, learning, coalition formation,
#       repeated games, fairness metrics, mechanism design, extended auctions,
#       and spatial/evolutionary games
# ============================================================================

# Game Theory - Equilibrium Tests
add_executable(unit_cognitive_test_gt_equilibrium
    game_theory/test_gt_equilibrium.cpp
)

target_link_libraries(unit_cognitive_test_gt_equilibrium
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_gt_equilibrium
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_gt_equilibrium PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_gt_equilibrium
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;game_theory;equilibrium"
        TIMEOUT 180
)

# Game Theory - Learning Tests
add_executable(unit_cognitive_test_gt_learning
    game_theory/test_gt_learning.cpp
)

target_link_libraries(unit_cognitive_test_gt_learning
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_gt_learning
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_gt_learning PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_gt_learning
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;game_theory;learning"
        TIMEOUT 180
)

# Game Theory - Coalition Tests
add_executable(unit_cognitive_test_gt_coalition
    game_theory/test_gt_coalition.cpp
)

target_link_libraries(unit_cognitive_test_gt_coalition
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_gt_coalition
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_gt_coalition PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_gt_coalition
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;game_theory;coalition"
        TIMEOUT 180
)

# Game Theory - Repeated Games Tests
add_executable(unit_cognitive_test_gt_repeated
    game_theory/test_gt_repeated.cpp
)

target_link_libraries(unit_cognitive_test_gt_repeated
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_gt_repeated
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_gt_repeated PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_gt_repeated
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;game_theory;repeated"
        TIMEOUT 180
)

# Game Theory - Fairness Tests
add_executable(unit_cognitive_test_gt_fairness
    game_theory/test_gt_fairness.cpp
)

target_link_libraries(unit_cognitive_test_gt_fairness
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_gt_fairness
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_gt_fairness PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_gt_fairness
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;game_theory;fairness"
        TIMEOUT 180
)

# Game Theory - Mechanism Design Tests
add_executable(unit_cognitive_test_gt_mechanism
    game_theory/test_gt_mechanism.cpp
)

target_link_libraries(unit_cognitive_test_gt_mechanism
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_gt_mechanism
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_gt_mechanism PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_gt_mechanism
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;game_theory;mechanism"
        TIMEOUT 180
)

# Game Theory - Extended Auction Tests
add_executable(unit_cognitive_test_gt_auction_ext
    game_theory/test_gt_auction_ext.cpp
)

target_link_libraries(unit_cognitive_test_gt_auction_ext
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_gt_auction_ext
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_gt_auction_ext PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_gt_auction_ext
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;game_theory;auction_ext"
        TIMEOUT 180
)

# Game Theory - Spatial/Evolutionary Games Tests
add_executable(unit_cognitive_test_gt_spatial
    game_theory/test_gt_spatial.cpp
)

target_link_libraries(unit_cognitive_test_gt_spatial
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_gt_spatial
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_gt_spatial PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_gt_spatial
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;game_theory;spatial"
        TIMEOUT 180
)

# Game Theory - FEP Bridge Tests
add_executable(unit_cognitive_test_gt_fep_bridge
    game_theory/test_game_theory_fep_bridge.cpp
)

target_link_libraries(unit_cognitive_test_gt_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_gt_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_gt_fep_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_gt_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;game_theory;fep"
        TIMEOUT 180
)

# ============================================================================
# Phase 7 Substrate/Thalamic Bridge Tests
# ============================================================================
# WHAT: Unit tests for Phase 7 cognitive bridges
# WHY:  Validate substrate and thalamic bridges for additional cognitive modules
#       (consolidation, emotion_tensor, empathetic_response, fault_tolerance,
#        fractal_cognitive, free_energy, game_theory, brain_immune, social,
#        theory_of_mind, predictive_immune, self_awareness_extended)
# HOW:  Test lifecycle, effects computation, attention gating, signal routing
# ============================================================================

add_executable(unit_cognitive_test_phase7_bridges
    test_phase7_bridges.cpp
)

target_link_libraries(unit_cognitive_test_phase7_bridges
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_phase7_bridges
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_phase7_bridges PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_phase7_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;bridges;phase7"
        TIMEOUT 120
)

# ============================================================================
# Cognitive Thalamic Bridges Tests
# ============================================================================
add_executable(unit_cognitive_test_cognitive_thalamic_bridges
    test_cognitive_thalamic_bridges.cpp
)

target_link_libraries(unit_cognitive_test_cognitive_thalamic_bridges
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_cognitive_thalamic_bridges
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_cognitive_thalamic_bridges PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_cognitive_thalamic_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;thalamic;bridges"
        TIMEOUT 120
)

# ============================================================================
# Consolidation Tests
# ============================================================================
add_executable(unit_cognitive_test_consolidation
    test_consolidation.cpp
)

target_link_libraries(unit_cognitive_test_consolidation
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_consolidation
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_consolidation PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_consolidation
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;consolidation"
        TIMEOUT 120
)

# ============================================================================
# Community Cache Unit Tests
# ============================================================================
add_executable(unit_cognitive_test_community_cache
    consolidation/test_community_cache.cpp
)

target_link_libraries(unit_cognitive_test_community_cache
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_community_cache
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_community_cache PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_community_cache
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;consolidation;community_cache"
        TIMEOUT 120
)

# ============================================================================
# Emotion Tensor Tests
# ============================================================================
add_executable(unit_cognitive_test_emotion_tensor
    test_emotion_tensor.cpp
)

target_link_libraries(unit_cognitive_test_emotion_tensor
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_emotion_tensor
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_emotion_tensor PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_emotion_tensor
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;emotion;tensor"
        TIMEOUT 120
)

# ============================================================================
# Empathetic Response Tests
# ============================================================================
add_executable(unit_cognitive_test_empathetic_response
    test_empathetic_response.cpp
)

target_link_libraries(unit_cognitive_test_empathetic_response
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_empathetic_response
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_empathetic_response PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_empathetic_response
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;empathetic;response"
        TIMEOUT 120
)

# ============================================================================
# Fault Tolerance Tests
# ============================================================================
add_executable(unit_cognitive_test_fault_tolerance
    test_fault_tolerance.cpp
)

target_link_libraries(unit_cognitive_test_fault_tolerance
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_fault_tolerance
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_fault_tolerance PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_fault_tolerance
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;fault_tolerance"
        TIMEOUT 120
)

# ============================================================================
# Fractal Cognitive Tests
# ============================================================================
add_executable(unit_cognitive_test_fractal_cognitive
    test_fractal_cognitive.cpp
)

target_link_libraries(unit_cognitive_test_fractal_cognitive
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_fractal_cognitive
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_fractal_cognitive PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_fractal_cognitive
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;fractal"
        TIMEOUT 120
)

# ============================================================================
# Free Energy Tests
# ============================================================================
add_executable(unit_cognitive_test_free_energy
    test_free_energy.cpp
)

target_link_libraries(unit_cognitive_test_free_energy
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_free_energy
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_free_energy PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_free_energy
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;free_energy"
        TIMEOUT 120
)

# ============================================================================
# Game Theory Main Tests
# ============================================================================
add_executable(unit_cognitive_test_game_theory
    test_game_theory.cpp
)

target_link_libraries(unit_cognitive_test_game_theory
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_game_theory
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_game_theory PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_game_theory
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;game_theory"
        TIMEOUT 120
)

# ============================================================================
# Predictive Immune Tests - DISABLED: requires nimcp_predictive_immune.c which is disabled
# ============================================================================
# add_executable(unit_cognitive_test_predictive_immune
#     test_predictive_immune.cpp
# )
#
# target_link_libraries(unit_cognitive_test_predictive_immune
#     PRIVATE
#         nimcp
#         GTest::gtest
#         GTest::gtest_main
#         Threads::Threads
#         m
# )
#
# target_include_directories(unit_cognitive_test_predictive_immune
#     PRIVATE
#         ${PROJECT_SOURCE_DIR}/include
#         ${PROJECT_SOURCE_DIR}/src
#         ${PROJECT_SOURCE_DIR}/test
# )
#
# target_compile_features(unit_cognitive_test_predictive_immune PRIVATE cxx_std_17)
#
# gtest_discover_tests(unit_cognitive_test_predictive_immune
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     PROPERTIES
#         LABELS "unit;cognitive;predictive;immune"
#         TIMEOUT 120
# )

# ============================================================================
# Self-Awareness Extended Tests
# ============================================================================
add_executable(unit_cognitive_test_self_awareness_extended
    test_self_awareness_extended.cpp
)

target_link_libraries(unit_cognitive_test_self_awareness_extended
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_self_awareness_extended
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_self_awareness_extended PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_self_awareness_extended
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;self_awareness;extended"
        TIMEOUT 120
)

# ============================================================================
# Collective Cognition Tests
# ============================================================================
add_subdirectory(collective_cognition)

# ============================================================================
# Recursive Cognition Tests (RLM-inspired)
# ============================================================================
add_subdirectory(recursive)

# ============================================================================
# Social Cognition Tests
# ============================================================================
add_subdirectory(social)

# ============================================================================
# Social Tests
# ============================================================================
add_executable(unit_cognitive_test_social
    test_social.cpp
)

target_link_libraries(unit_cognitive_test_social
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_social
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_social PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_social
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;social"
        TIMEOUT 120
)

# ============================================================================
# Imagination Engine Tests
# ============================================================================
add_executable(unit_cognitive_test_imagination_engine
    imagination/test_imagination_engine.cpp
)

target_link_libraries(unit_cognitive_test_imagination_engine
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_imagination_engine
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_imagination_engine PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_imagination_engine
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;imagination"
        TIMEOUT 180
)

# ============================================================================
# Imagination FEP Bridge Tests
# ============================================================================
# WHAT: Unit tests for Imagination-FEP bidirectional integration
# WHY:  Verify free energy computation from simulation divergence and
#       counterfactual costs, prediction accuracy tracking
# HOW:  Test lifecycle, registration, metrics, callbacks, FEP update cycle
# ============================================================================

add_executable(unit_cognitive_test_imagination_fep_bridge
    imagination/test_imagination_fep_bridge.cpp
)

target_link_libraries(unit_cognitive_test_imagination_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_imagination_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_imagination_fep_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_imagination_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;imagination;fep"
        TIMEOUT 120
)

# ============================================================================
# Omnidirectional Inference Tests (Phase 4)
# ============================================================================
# WHAT: Unit tests for Omnidirectional Inference components
# WHY:  Validate bidirectional JEPA, Hopfield memory, predictive hierarchy,
#       and temporal replay modules
# ============================================================================

# JEPA Bidirectional Tests
add_executable(unit_cognitive_test_jepa_bidirectional
    jepa/test_jepa_bidirectional.cpp
)

target_link_libraries(unit_cognitive_test_jepa_bidirectional
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_jepa_bidirectional
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_cognitive_test_jepa_bidirectional PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_jepa_bidirectional
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;jepa;bidirectional;omni"
        TIMEOUT 120
)

# Hopfield Memory Tests
add_executable(unit_cognitive_test_hopfield_memory
    memory/test_hopfield_memory.cpp
)

target_link_libraries(unit_cognitive_test_hopfield_memory
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_hopfield_memory
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_cognitive_test_hopfield_memory PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_hopfield_memory
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;memory;hopfield;omni"
        TIMEOUT 120
)

# Predictive Hierarchy Tests
add_executable(unit_cognitive_test_predictive_hierarchy
    predictive/test_predictive_hierarchy.cpp
)

target_link_libraries(unit_cognitive_test_predictive_hierarchy
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_predictive_hierarchy
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_cognitive_test_predictive_hierarchy PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_predictive_hierarchy
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;predictive;hierarchy;omni"
        TIMEOUT 120
)

# Temporal Replay Tests
add_executable(unit_cognitive_test_temporal_replay
    memory/test_temporal_replay.cpp
)

target_link_libraries(unit_cognitive_test_temporal_replay
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_temporal_replay
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_cognitive_test_temporal_replay PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_temporal_replay
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;memory;replay;omni"
        TIMEOUT 120
)

# Global Workspace - SNN Bridge Unit Tests
add_executable(unit_cognitive_gw_snn_bridge
    global_workspace/test_gw_snn_bridge.cpp
)

target_link_libraries(unit_cognitive_gw_snn_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_gw_snn_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_gw_snn_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_gw_snn_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;global_workspace;snn"
        TIMEOUT 120
)

# Global Workspace - Plasticity Bridge Unit Tests
add_executable(unit_cognitive_gw_plasticity_bridge
    global_workspace/test_gw_plasticity_bridge.cpp
)

target_link_libraries(unit_cognitive_gw_plasticity_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_gw_plasticity_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_gw_plasticity_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_gw_plasticity_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;global_workspace;plasticity"
        TIMEOUT 120
)

# ============================================================================
# Epistemic SNN/Plasticity Bridge Tests
# ============================================================================
add_subdirectory(epistemic)

# ============================================================================
# Cognitive Bias SNN/Plasticity Bridge Tests
# ============================================================================
add_subdirectory(bias)

# ============================================================================
# Salience SNN/Plasticity Bridge Tests
# ============================================================================
add_subdirectory(salience)

# ============================================================================
# Cognitive Integration Bridge Tests (Emotion-Executive, GW-Cognitive)
# ============================================================================
add_subdirectory(integration)

# ============================================================================
# JEPA FEP Bridge Tests
# ============================================================================
# WHAT: Unit tests for JEPA-FEP bidirectional integration
# WHY:  Verify free energy computation from embedding prediction error,
#       representation quality, and collapse detection
# HOW:  Test lifecycle, registration, metrics, callbacks, FEP update cycle
# ============================================================================

add_executable(unit_cognitive_test_jepa_fep_bridge
    jepa/test_jepa_fep_bridge.cpp
)

target_link_libraries(unit_cognitive_test_jepa_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_jepa_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_cognitive_test_jepa_fep_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_jepa_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;jepa;fep"
        TIMEOUT 120
)

# ============================================================================
# Symbolic Logic Integration Tests
# ============================================================================
# WHAT: Unit tests for symbolic logic cognitive layer integration
# WHY:  Verify FEP orchestrator and cognitive hub integration
# HOW:  Test hub bridge, FEP registration, inter-module communication
# ============================================================================

add_executable(unit_cognitive_test_symbolic_logic_integration
    symbolic_logic/test_symbolic_logic_integration.cpp
)

target_link_libraries(unit_cognitive_test_symbolic_logic_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_symbolic_logic_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_cognitive_test_symbolic_logic_integration PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_symbolic_logic_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;symbolic_logic;integration"
        TIMEOUT 120
)

# ============================================================================
# Extrapolation / World Model Tests
# ============================================================================
# WHAT: Unit tests for Multi-Modal World Model
# WHY:  Verify modality processing, fusion, prediction, entity tracking
# HOW:  Test all modalities, fusion types, prediction modes, entity CRUD
# ============================================================================

add_subdirectory(extrapolation)

# ============================================================================
# Exception Handling Tests
# ============================================================================
# WHAT: Unit tests for Cognitive Exception Handling
# WHY:  Verify exception creation, dispatch, and immune integration for cognitive errors
# HOW:  Test error code mapping, handler chains, recovery strategies
# ============================================================================

add_subdirectory(exception)

# ============================================================================
# Omni World Model Tests
# ============================================================================
# WHAT: Unit tests for Omnidirectional Generative World Model
# WHY:  Verify counterfactual reasoning, mental simulation, RSSM, MDN,
#       experience replay, symlog transformation, dynamics prediction
# HOW:  Test all APIs: lifecycle, state, RSSM, dynamics, latent encoding,
#       MDN, counterfactual, rollouts, learning, experience replay, bio-async
# ============================================================================

add_subdirectory(omni)

# ============================================================================
# Cognitive Health Integration Tests (Phase 8)
# ============================================================================
# WHAT: Unit tests for cognitive health integration
# WHY:  Validate collective health, RCOG health, meta-health, cognitive bridge
# HOW:  Test lifecycle, API functions, intelligent handling, statistics
# ============================================================================

add_subdirectory(health)

# ============================================================================
# Neuro-Symbolic Mathematics Tests (Mathesis-Inspired)
# ============================================================================
# WHAT: Unit tests for genius-level mathematics system
# WHY:  Validate energy consistency, hypergraph, quantum MCTS, orchestrator
# HOW:  Test all APIs: lifecycle, operations, integration with other modules
# ============================================================================

add_subdirectory(neuro_symbolic)

# ============================================================================
# Predictive Bridge Exception Tests
# ============================================================================
# WHAT: Unit tests for predictive bridge exception handling
# WHY:  Verify NIMCP_THROW_TO_IMMUNE exception handling for all predictive bridges
# HOW:  Test NULL pointer, allocation failure, and bio-async checks
# ============================================================================

add_subdirectory(predictive)

# ============================================================================
# Inner Dialogue Engine Tests
# ============================================================================
# WHAT: Unit tests for Inner Dialogue (Turn, Perspective, Convergence, Engine)
# WHY:  Verify each inner dialogue function in isolation
# HOW:  Test lifecycle, CRUD, scheduling, convergence analysis, state machine
# ============================================================================

add_subdirectory(inner_dialogue)

# ============================================================================
# Creative Cortex Validation Tests
# ============================================================================
# WHAT: Unit tests for Creative Cortex validation (archetype names, image creation)
# WHY:  Validate P0-4/P0-5/P0-6 fixes: overflow protection, error codes, name mappings
# HOW:  Test all archetype name functions, image creation edge cases, overflow
# ============================================================================

add_executable(unit_cognitive_test_creative_validation
    test_creative_validation.cpp
)

target_link_libraries(unit_cognitive_test_creative_validation
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_creative_validation
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_creative_validation PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_creative_validation
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;creative;validation"
        TIMEOUT 120
)

# ============================================================================
# Creative Cortex Tests
# ============================================================================
# WHAT: Unit tests for Creative Cortex module (art appreciation and generation)
# WHY:  Validate orchestrator lifecycle, style embeddings, modality helpers
# HOW:  Test create/destroy, config, embeddings, modality classification
# ============================================================================

add_executable(unit_cognitive_test_creative
    test_creative.cpp
)

target_link_libraries(unit_cognitive_test_creative
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_creative
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_creative PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_creative
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;creative"
        TIMEOUT 120
)

# ============================================================================
# Self-Awareness Feedback Tests
# ============================================================================
# WHAT: Unit tests for Self-Awareness Feedback System
# WHY:  Validate feedback loop management, policies, transfers, analysis
# HOW:  Test init/cleanup, policy functions, transfer operations, health
# ============================================================================

add_executable(unit_cognitive_test_self_awareness_feedback
    test_self_awareness_feedback.cpp
)

target_link_libraries(unit_cognitive_test_self_awareness_feedback
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_self_awareness_feedback
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_self_awareness_feedback PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_self_awareness_feedback
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;self_awareness;feedback"
        TIMEOUT 120
)

# ============================================================================
# Shadow Emotions Tests
# ============================================================================
# WHAT: Unit tests for Shadow Emotions module (Dark Triad detection)
# WHY:  Validate emotion tracking, self-correction, intervention system
# HOW:  Test create/destroy, emotion experience, queries, interventions
# ============================================================================

add_executable(unit_cognitive_test_shadow_emotions
    test_shadow_emotions.cpp
)

target_link_libraries(unit_cognitive_test_shadow_emotions
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_shadow_emotions
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_shadow_emotions PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_shadow_emotions
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;shadow;emotions"
        TIMEOUT 120
)

# ============================================================================
# Remorse and Regret Tests
# ============================================================================
# WHAT: Unit tests for Remorse and Regret system (evaluative negative emotions)
# WHY:  Validate event processing, emotional dynamics, atonement, forgiveness
# HOW:  Test create/destroy, events, counterfactual, update, queries
# ============================================================================

add_executable(unit_cognitive_test_remorse_regret
    test_remorse_regret.cpp
)

target_link_libraries(unit_cognitive_test_remorse_regret
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_remorse_regret
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_remorse_regret PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_remorse_regret
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;remorse;regret"
        TIMEOUT 120
)

# ============================================================================
# Cognitive Output Rubric Tests
# ============================================================================
# WHAT: Unit tests for two-tier quality evaluation of brain_decide() outputs
# WHY:  Verify structural + qualitative scoring, grade derivation, API paths
# HOW:  Test evaluator lifecycle, dimension scorers, grade boundaries, public API
# ============================================================================

add_executable(unit_cognitive_test_rubric
    test_rubric.cpp
)

target_link_libraries(unit_cognitive_test_rubric
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_rubric
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_rubric PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_rubric
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;rubric"
        TIMEOUT 120
)

# ============================================================================
# Cognitive Training Integration Tests
# ============================================================================
# WHAT: Unit tests for the brain_ti_* training integration facade
# WHY:  Verify NULL-safety, default returns, and input validation for every
#       brain_ti_* entry point — no brain instantiation required
# HOW:  GTest fixture passing NULL brain pointers, asserting safe fallbacks
# ============================================================================

add_executable(unit_cognitive_test_training_integration
    test_training_integration.cpp
)

target_link_libraries(unit_cognitive_test_training_integration PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

target_include_directories(unit_cognitive_test_training_integration PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_training_integration PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_training_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;training-integration"
        TIMEOUT 90
)

# Reasoning Chain - Causal Reasoning Unit Tests
add_executable(unit_cognitive_test_reasoning_causal
    test_reasoning_causal.cpp
)

target_link_libraries(unit_cognitive_test_reasoning_causal
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(unit_cognitive_test_reasoning_causal
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_reasoning_causal PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_reasoning_causal
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning-causal"
        TIMEOUT 90
)

# =============================================================================
# Visuospatial Reasoning Unit Tests
# =============================================================================
add_executable(unit_cognitive_test_reasoning_visuospatial
    test_reasoning_visuospatial.cpp
)

target_link_libraries(unit_cognitive_test_reasoning_visuospatial
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_reasoning_visuospatial
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_reasoning_visuospatial PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_reasoning_visuospatial
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning-visuospatial"
        TIMEOUT 90
)

# Training Integration - Unified Continuous Modulation Pipeline Tests
add_executable(unit_cognitive_test_training_integration_unified
    test_training_integration_unified.cpp
)

target_link_libraries(unit_cognitive_test_training_integration_unified PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

target_include_directories(unit_cognitive_test_training_integration_unified PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_training_integration_unified PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_training_integration_unified
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;training-integration-unified"
        TIMEOUT 90
)

# OOD Detector tests
add_executable(unit_cognitive_test_ood_detector
    test_ood_detector.cpp
)

target_link_libraries(unit_cognitive_test_ood_detector
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_ood_detector
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_ood_detector PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_ood_detector
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;ood-detector"
        TIMEOUT 90
)

# ============================================================================
# Cognitive Training Data Validation Tests
# ============================================================================

add_executable(unit_cognitive_test_training_data
    test_cognitive_training_data.cpp
)
target_link_libraries(unit_cognitive_test_training_data
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_cognitive_test_training_data
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_test_training_data PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_test_training_data
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;training-data"
        TIMEOUT 60
)

# ============================================================================
# Cognitive Modules Wiring Tests
# ============================================================================

add_executable(unit_cognitive_test_modules_wiring
    test_cognitive_modules_wiring.cpp
)
target_link_libraries(unit_cognitive_test_modules_wiring
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_cognitive_test_modules_wiring
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_test_modules_wiring PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_test_modules_wiring
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;wiring"
        TIMEOUT 60
)

# Cognitive enhancement modules tests
add_executable(unit_cognitive_test_enhancements test_cognitive_enhancements.cpp test_linker_stubs.c)
target_link_libraries(unit_cognitive_test_enhancements PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_cognitive_test_enhancements PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_cognitive_test_enhancements PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_test_enhancements WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;cognitive;enhancements" TIMEOUT 60)

# Native Language
add_executable(unit_cognitive_test_native_language test_native_language.cpp test_linker_stubs.c)
target_link_libraries(unit_cognitive_test_native_language PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_cognitive_test_native_language PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_cognitive_test_native_language PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_test_native_language WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;cognitive;language" TIMEOUT 60)

# Tokenizer
add_executable(unit_cognitive_test_tokenizer test_tokenizer.cpp test_linker_stubs.c)
target_link_libraries(unit_cognitive_test_tokenizer PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_cognitive_test_tokenizer PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_cognitive_test_tokenizer PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_test_tokenizer WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;cognitive;language" TIMEOUT 60)

# ============================================================================
# Social Interaction Tests
# ============================================================================
add_executable(unit_cognitive_test_social_interaction test_social_interaction.cpp)
target_link_libraries(unit_cognitive_test_social_interaction PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_cognitive_test_social_interaction PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_cognitive_test_social_interaction PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_test_social_interaction WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;cognitive;social" TIMEOUT 60)

# ============================================================================
# Emergent Language Tests
# ============================================================================
add_executable(unit_cognitive_test_emergent test_emergent_language.cpp)
target_link_libraries(unit_cognitive_test_emergent PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_cognitive_test_emergent PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_cognitive_test_emergent PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_test_emergent WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;cognitive;emergent" TIMEOUT 60)

# Memory Features (involuntary recall, emotional protection)
add_executable(unit_cognitive_test_memory_features test_memory_features.cpp)
target_link_libraries(unit_cognitive_test_memory_features PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_cognitive_test_memory_features PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_cognitive_test_memory_features PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_test_memory_features WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;cognitive;memory" TIMEOUT 60)

# ============================================================================
# Trauma Resilience Unit Tests
# ============================================================================
add_executable(unit_cognitive_test_trauma_resilience test_trauma_resilience.cpp)
target_link_libraries(unit_cognitive_test_trauma_resilience PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_cognitive_test_trauma_resilience PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_cognitive_test_trauma_resilience PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_test_trauma_resilience WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;cognitive;trauma_resilience" TIMEOUT 60)
