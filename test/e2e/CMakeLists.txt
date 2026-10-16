# ============================================================================
# NIMCP End-to-End (E2E) Test Framework
# ============================================================================
# WHAT: Build configuration for end-to-end integration tests
# WHY:  Test complete pipelines across multiple subsystems
# HOW:  Link full NIMCP library with GTest for comprehensive testing
#
# E2E TESTS:
# - test_brain_pipeline: Brain lifecycle (create, train, infer, save, restore)
# - test_cognitive_pipeline: Cognitive processing flow (perception → decision → memory)
# - test_distributed_pipeline: Distributed brain synchronization
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping E2E tests")
    return()
endif()

message(STATUS "")
message(STATUS "===============================================================================")
message(STATUS "NIMCP E2E Test Framework")
message(STATUS "===============================================================================")

# ============================================================================
# Common E2E Test Configuration
# ============================================================================

set(E2E_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/test/e2e
    ${CMAKE_SOURCE_DIR}/src/lib
    ${GTEST_INCLUDE_DIRS}
    ${Python3_INCLUDE_DIRS}
)

set(E2E_LIBRARIES
    nimcp_portia
    nimcp
    nimcp_middleware
    GTest::gtest
    GTest::gtest_main
    Python3::Python
    pthread
    m
)

# ============================================================================
# Helper: Create E2E Test Binary
# ============================================================================
# WHAT: Create E2E test executable with full NIMCP library linkage
# WHY:  E2E tests require complete system integration
# HOW:  Link against full nimcp library with all dependencies
#
# PARAMETERS:
#   TARGET_NAME - Test executable name
#   SOURCE_FILES - List of test source files
# ============================================================================
function(add_e2e_test TARGET_NAME)
    set(SOURCE_FILES ${ARGN})

    # Guard: Validate sources exist
    foreach(SOURCE_FILE ${SOURCE_FILES})
        if(NOT EXISTS ${SOURCE_FILE})
            message(WARNING "E2E test source not found: ${SOURCE_FILE}")
            return()
        endif()
    endforeach()

    # Create executable
    add_executable(${TARGET_NAME} ${SOURCE_FILES})

    # Add explicit dependency on nimcp library
    add_dependencies(${TARGET_NAME} nimcp)

    # Configure includes
    target_include_directories(${TARGET_NAME} PRIVATE ${E2E_INCLUDE_DIRS})

    # Enable C++20 for designated initializers
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

    # Enable test-only APIs and E2E mode
    target_compile_definitions(${TARGET_NAME} PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )

    # Link libraries
    target_link_libraries(${TARGET_NAME} PRIVATE ${E2E_LIBRARIES})

    # Set properties
    set_target_properties(${TARGET_NAME} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    # Enable coverage for Debug builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        if(CMAKE_COMPILER_IS_GNUCXX)
            target_compile_options(${TARGET_NAME} PRIVATE --coverage)
            target_link_options(${TARGET_NAME} PRIVATE --coverage)
        endif()
    endif()

    # Register with CTest (E2E tests run sequentially with longer timeout)
    add_test(NAME ${TARGET_NAME}
             COMMAND ${TARGET_NAME}
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

    # Set E2E test properties (longer timeout, sequential execution)
    set_tests_properties(${TARGET_NAME} PROPERTIES
        ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/lib/python;LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.supp"
        LABELS "e2e"
        TIMEOUT 120  # 2 minutes for E2E tests
        RUN_SERIAL TRUE  # Run sequentially to avoid resource contention
    )

    message(STATUS "  Added E2E test: ${TARGET_NAME}")
endfunction()

# ============================================================================
# E2E Test Targets
# ============================================================================

# Build the E2E test framework support files
add_e2e_test(e2e_test_brain_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_brain_pipeline.cpp
)

add_e2e_test(e2e_test_cognitive_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cognitive_pipeline.cpp
)

# Recursive Cognition Pipeline E2E Test (RLM-inspired recursive processing)
add_e2e_test(e2e_test_recursive_cognition_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_recursive_cognition_pipeline.cpp
)

add_e2e_test(e2e_test_distributed_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_distributed_pipeline.cpp
)

add_e2e_test(e2e_test_reasoning_convergent
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_convergent_e2e.cpp
)

add_e2e_test(e2e_test_reasoning_metacognition
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_metacognition_e2e.cpp
)

add_e2e_test(e2e_test_reasoning_calibration
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_calibration_e2e.cpp
)

# CB migration: runaway suppression (no e2e_test_framework — uses GTest::gtest_main).
add_e2e_test(e2e_test_snn_runaway_suppression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_runaway_suppression_e2e.cpp
)

# CB GPU port (Phase 5): long-run smoke test on the GPU LIF CB kernel —
# 1024 neurons × 200 ms with mixed E/I drive, no NaN, no runaway.
add_e2e_test(e2e_test_snn_cb_gpu
    ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_cb_gpu_e2e.cpp
)

add_e2e_test(e2e_test_reasoning_abduction
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_abduction_e2e.cpp
)

add_e2e_test(e2e_test_reasoning_kb_persistence
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_kb_persistence_e2e.cpp
)

add_e2e_test(e2e_test_reasoning_affective
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_affective_e2e.cpp
)

add_e2e_test(e2e_test_reasoning_causal
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_causal_e2e.cpp
)

add_e2e_test(e2e_test_reasoning_visuospatial
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_visuospatial_e2e.cpp
)

add_e2e_test(e2e_test_bio_async_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bio_async_pipeline.cpp
)

add_e2e_test(e2e_test_cognitive_bio_async
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cognitive_bio_async_e2e.cpp
)

add_e2e_test(e2e_test_core_bio_async
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_core_bio_async_e2e.cpp
)

add_e2e_test(e2e_test_bio_async_training_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_bio_async_training_pipeline.cpp
)

# Fixed: Lambda capture issues - now using static handlers with user_data
add_e2e_test(e2e_test_bio_async_plasticity_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_bio_async_plasticity_pipeline.cpp
)

# Fixed: Missing header path - now using cognitive/introspection/nimcp_introspection.h
add_e2e_test(e2e_test_bio_async_cognitive_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_bio_async_cognitive_pipeline.cpp
)

# Fixed: BBB functions are in nimcp library - should link correctly
add_e2e_test(e2e_test_security_attack_simulation
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_security_attack_simulation.cpp
)

# Security Bridges Pipeline E2E Tests
add_e2e_test(e2e_test_security_bridges_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_security_bridges_pipeline.cpp
)

# Swarm E2E Tests
add_e2e_test(e2e_test_swarm_formation
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_swarm_formation.cpp
)

add_e2e_test(e2e_test_swarm_threat_response
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_swarm_threat_response.cpp
)

# Asymmetric Swarm E2E Test (Role-Based Brain Configurations)
add_e2e_test(e2e_test_asymmetric_swarm
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_asymmetric_swarm.cpp
)

add_e2e_test(e2e_test_server_swarm_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_server_swarm_pipeline.cpp
)

# Neural Link Protocol (NLP) E2E Tests
add_e2e_test(e2e_test_nlp_swarm_coordination
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_nlp_swarm_coordination.cpp
)

add_e2e_test(e2e_test_nlp_war_zone
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_nlp_war_zone.cpp
)

add_e2e_test(e2e_test_nlp_disaster_sar
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_nlp_disaster_sar.cpp
)

add_e2e_test(e2e_test_nlp_neural_distributed
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_nlp_neural_distributed.cpp
)

# Portia Spider E2E Tests
add_e2e_test(e2e_test_portia_constrained_platform
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_portia_constrained_platform.cpp
)

add_e2e_test(e2e_test_portia_tier_lifecycle
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_portia_tier_lifecycle.cpp
)

add_e2e_test(e2e_test_portia_power_lifecycle
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_portia_power_lifecycle.cpp
)

add_e2e_test(e2e_test_portia_learning_adaptation
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_portia_learning_adaptation.cpp
)

add_e2e_test(e2e_test_portia_sensor_fusion_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_portia_sensor_fusion_pipeline.cpp
)

add_e2e_test(e2e_test_portia_degradation_scenario
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_portia_degradation_scenario.cpp
)

add_e2e_test(e2e_test_portia_bio_async_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_portia_bio_async_pipeline.cpp
)

add_e2e_test(e2e_test_portia_security_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_portia_security_integration.cpp
)

add_e2e_test(e2e_test_portia_planning_mission
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_portia_planning_mission.cpp
)

add_e2e_test(e2e_test_portia_threat_response
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_portia_threat_response.cpp
)

add_e2e_test(e2e_test_portia_swarm_logic_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_portia_swarm_logic_pipeline.cpp
)

# Emotion Tensor E2E Test
add_e2e_test(e2e_test_emotion_tensor_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_emotion_tensor_pipeline.cpp
)

# Second Messenger Cascade E2E Test
add_e2e_test(e2e_test_second_messenger_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_second_messenger_pipeline.cpp
)

# Positional Encoding Pipeline E2E Test
add_e2e_test(e2e_test_positional_encoding_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_positional_encoding_pipeline.cpp
)

# Introspection Pipeline E2E Test (Consciousness, Temporal Patterns, Uncertainty)
add_e2e_test(e2e_test_introspection_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_introspection_pipeline.cpp
)

# Swarm Consciousness Pipeline E2E Test (Gestalt Consciousness)
add_e2e_test(e2e_test_swarm_consciousness_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_swarm_consciousness_pipeline.cpp
)

# Enhanced Swarm Consciousness Pipeline E2E Test (Peer Callbacks, Remote Phi, Dynamics)
add_e2e_test(e2e_test_swarm_consciousness_enhanced_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_swarm_consciousness_enhanced_pipeline.cpp
)

# Enhanced Fault Tolerance Pipeline E2E Test (Immune System Integration)
add_e2e_test(e2e_test_fault_tolerance_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_fault_tolerance_pipeline.cpp
)

# Signal Exception Pipeline E2E Test (Signal -> Queue -> Exception -> Immune)
add_e2e_test(e2e_test_signal_exception_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_signal_exception_pipeline.cpp
)

# Brain Immune System Pipeline E2E Test (B cells, T cells, Antibodies, Cytokines)
add_e2e_test(e2e_test_brain_immune_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_brain_immune_pipeline.cpp
)

# Distributed Immune Response E2E Test (Brain-Swarm Integration)
add_e2e_test(e2e_test_distributed_immune_response
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_distributed_immune_response.cpp
)

# Brain Immune-Plasticity Integration Pipeline E2E Test
add_e2e_test(e2e_test_brain_immune_plasticity_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_brain_immune_plasticity_pipeline.cpp
)

# Brain Immune-Emotion Integration Pipeline E2E Test
add_e2e_test(e2e_test_brain_immune_emotion_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_brain_immune_emotion_pipeline.cpp
)

# Memory-Immune Integration Pipeline E2E Test
add_e2e_test(e2e_test_memory_immune_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_memory_immune_pipeline.cpp
)

# Immune Enhancement Pipeline E2E Test (Trained Immunity, Complement, Treg, Exhaustion,
# Tolerance, Vaccine, Mucosal, Persistence)
add_e2e_test(e2e_test_immune_enhancement_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_immune_enhancement_pipeline.cpp
)

# Enhanced Wellbeing Pipeline E2E Test (Substrate, Sleep, Mental Health, Immune Integration)
add_e2e_test(e2e_test_wellbeing_enhanced_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_wellbeing_enhanced_pipeline.cpp
)

# FEP Bridges Pipeline E2E Test (Free Energy Principle Integration)
add_e2e_test(e2e_test_fep_bridges_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_fep_bridges_pipeline.cpp
)

# FEP Orchestrator Pipeline E2E Test (Central FEP Bridge Coordination)
add_e2e_test(e2e_test_fep_orchestrator_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_fep_orchestrator_pipeline.cpp
)

# JEPA QMC Pipeline E2E Test (Quantum Monte Carlo Enhanced JEPA)
add_e2e_test(e2e_test_jepa_qmc_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_jepa_qmc_pipeline.cpp
)

# Physics Layer Pipeline E2E Test (HH Dynamics + Thermodynamics + Ephaptic Coupling)
add_e2e_test(e2e_test_physics_layer
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_physics_layer.cpp
)

# Coordinator Pipeline E2E Test (6 Coordinator/Orchestrator Integration)
add_e2e_test(e2e_test_coordinator_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_coordinator_pipeline.cpp
)

# Core Directives Pipeline E2E Test (Asimov's Laws, Golden Rule, Combinatorial Harm)
add_e2e_test(e2e_test_core_directives_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_core_directives_pipeline.cpp
)

# Medulla Oblongata Pipeline E2E Test (Brainstem Autonomic Regulation)
add_e2e_test(e2e_test_medulla_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_medulla_pipeline.cpp
)

# Medulla-Cerebellum Bridge Pipeline E2E Test (Inferior Olive Error Signaling)
add_e2e_test(e2e_test_medulla_cerebellum_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_medulla_cerebellum_pipeline.cpp
)

# Game Theory Pipeline E2E Test (Auctions, Bargaining, Credit Assignment)
add_e2e_test(e2e_test_game_theory_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_game_theory_pipeline.cpp
)

# Parietal Lobe Pipeline E2E Test (Mathematical/Scientific Reasoning)
add_e2e_test(e2e_test_parietal_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_parietal_pipeline.cpp
)

# Code Immune Pipeline E2E Test (Self-Healing: Crash -> Fix -> Recompile -> Hot-Inject)
add_e2e_test(e2e_test_code_immune_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_code_immune_pipeline.cpp
)

# Bridge Pipelines E2E Test (Sleep + Immune Bridges Integration)
add_executable(e2e_test_bridge_pipelines
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_bridge_pipelines.cpp
)
target_link_libraries(e2e_test_bridge_pipelines
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
)
target_include_directories(e2e_test_bridge_pipelines
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/swarm/sleep
        ${CMAKE_SOURCE_DIR}/src/swarm/immune
        ${CMAKE_SOURCE_DIR}/src/security/sleep
        ${CMAKE_SOURCE_DIR}/src/cognitive
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/utils/thread
)
add_test(NAME e2e_test_bridge_pipelines COMMAND e2e_test_bridge_pipelines)
set_tests_properties(e2e_test_bridge_pipelines PROPERTIES
    LABELS "e2e"
    TIMEOUT 180
)

# Perception-Cortical Bridge E2E Test
add_executable(e2e_test_perception_cortical
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_perception_cortical.cpp
)
target_link_libraries(e2e_test_perception_cortical
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
)
target_include_directories(e2e_test_perception_cortical
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/perception
        ${CMAKE_SOURCE_DIR}/src/perception/cortical
        ${CMAKE_SOURCE_DIR}/src/core/cortical_columns
)
add_test(NAME e2e_test_perception_cortical COMMAND e2e_test_perception_cortical)
set_tests_properties(e2e_test_perception_cortical PROPERTIES
    LABELS "e2e"
    TIMEOUT 180
)

# ============================================================================
# Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for triplet STDP and plasticity orchestrator
# WHY:  Verify complete plasticity pipeline functionality
# HOW:  Test synaptic plasticity, sleep integration, immune integration
# ============================================================================
add_executable(e2e_test_plasticity_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_plasticity_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
)
target_link_libraries(e2e_test_plasticity_pipeline
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(e2e_test_plasticity_pipeline
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(e2e_test_plasticity_pipeline PRIVATE cxx_std_17)
add_test(NAME e2e_test_plasticity_pipeline COMMAND e2e_test_plasticity_pipeline)
set_tests_properties(e2e_test_plasticity_pipeline PROPERTIES
    LABELS "e2e;plasticity"
    TIMEOUT 180
)

# ============================================================================
# Neural Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for neural-plasticity integration (axon/neuron/dendrite)
# WHY:  Verify complete spike-to-plasticity pipeline
# HOW:  Test neuron dynamics, spike cascade, bidirectional sync
# ============================================================================
add_executable(e2e_test_neural_plasticity_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_neural_plasticity_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
)
target_link_libraries(e2e_test_neural_plasticity_pipeline
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(e2e_test_neural_plasticity_pipeline
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test/e2e
)
target_compile_features(e2e_test_neural_plasticity_pipeline PRIVATE cxx_std_17)
add_test(NAME e2e_test_neural_plasticity_pipeline COMMAND e2e_test_neural_plasticity_pipeline)
set_tests_properties(e2e_test_neural_plasticity_pipeline PROPERTIES
    LABELS "e2e;plasticity;neural"
    TIMEOUT 180
)

# ============================================================================
# Master Connectivity E2E Test
# ============================================================================
# WHAT: E2E tests verifying full NIMCP brain connectivity across all layers
# WHY:  Ensure every component communicates with every other relevant component
# HOW:  Test neural substrate, thalamic/perception, Broca's region, cognitive,
#       brain infrastructure, and cross-layer integration
# ============================================================================
add_executable(e2e_test_master_connectivity
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_master_connectivity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
)
target_link_libraries(e2e_test_master_connectivity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(e2e_test_master_connectivity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(e2e_test_master_connectivity PRIVATE cxx_std_17)
add_test(NAME e2e_test_master_connectivity COMMAND e2e_test_master_connectivity)
set_tests_properties(e2e_test_master_connectivity PROPERTIES
    LABELS "e2e;connectivity;master"
    TIMEOUT 300
)

# ============================================================================
# Cortical Substrate Bridge Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for cortical substrate bridge integration
# WHY:  Verify metabolic/thermal substrate modulates cortical processing
# HOW:  Test sensory-to-cortical pipelines with substrate state effects
# ============================================================================
add_executable(e2e_test_cortical_substrate_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_cortical_substrate_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
)
target_link_libraries(e2e_test_cortical_substrate_pipeline
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(e2e_test_cortical_substrate_pipeline
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(e2e_test_cortical_substrate_pipeline PRIVATE cxx_std_17)
add_test(NAME e2e_test_cortical_substrate_pipeline COMMAND e2e_test_cortical_substrate_pipeline)
set_tests_properties(e2e_test_cortical_substrate_pipeline PROPERTIES
    LABELS "e2e;cortical;substrate"
    TIMEOUT 180
)

# ============================================================================
# Substrate Bridges Pipeline E2E Test
# ============================================================================
# WHAT: Comprehensive E2E tests for all substrate bridges across NIMCP
# WHY:  Verify complete substrate-cognitive-neural integration and stress scenarios
# HOW:  Test cognitive pipeline, neural pipeline, stress scenarios, and full brain integration
#
# TEST COVERAGE (16 tests):
# - Cognitive Pipeline (5): Full cognitive cycle, emotion-cognition interaction,
#   introspection monitoring, ToM processing, memory consolidation recovery
# - Neural Pipeline (4): Spike processing, signal propagation, plasticity learning,
#   glial support during sustained activity
# - Stress Scenarios (4): Low ATP cascade, hyperthermia effects, recovery sequence,
#   gradual degradation simulation
# - Full Brain Integration (3): Sensory-to-motor pathway, learning/adaptation cycle,
#   multi-modal processing
# ============================================================================
add_executable(e2e_test_substrate_bridges_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_substrate_bridges_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
)
target_link_libraries(e2e_test_substrate_bridges_pipeline
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(e2e_test_substrate_bridges_pipeline
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test/e2e
)
target_compile_features(e2e_test_substrate_bridges_pipeline PRIVATE cxx_std_17)
add_test(NAME e2e_test_substrate_bridges_pipeline COMMAND e2e_test_substrate_bridges_pipeline)
set_tests_properties(e2e_test_substrate_bridges_pipeline PROPERTIES
    LABELS "e2e;substrate;bridges"
    TIMEOUT 300
)

# ============================================================================
# Test Execution Instructions
# ============================================================================

# Training-Logic Pipeline E2E Test (Training-Logic Bridge Integration)
add_e2e_test(e2e_test_training_logic_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_training_logic_pipeline.cpp
)

# Cognitive-Training Pipeline E2E Test (Cognitive-Training Bridge Integration)
add_e2e_test(e2e_test_cognitive_training_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_cognitive_training_pipeline.cpp
)

# Perception-Training Pipeline E2E Test (Perception-Training Bridge Integration)
add_e2e_test(e2e_test_perception_training_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_perception_training_pipeline.cpp
)

# Cortical-Training Pipeline E2E Test (Cortical-Training Bridge Integration)
add_e2e_test(e2e_test_cortical_training_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_cortical_training_pipeline.cpp
)

# Unified Training Integration E2E Test (All Perception/Cortical Integrations)
add_e2e_test(e2e_test_unified_training_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_unified_training_integration.cpp
)

# CNN-Cortex Bridge E2E Test (CNN Training with A/V Cortex Feature Extraction)
add_e2e_test(e2e_test_cnn_cortex_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_cnn_cortex_bridge.cpp
)

# CNN Backpropagation E2E Test (CNN Training Convergence and Inference)
add_e2e_test(e2e_test_cnn_backprop
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_cnn_backprop.cpp
)

# SNN Pipeline E2E Test (Spiking Neural Network Full Integration)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_snn_pipeline.cpp)
    add_executable(e2e_test_snn_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_snn_pipeline.cpp
    )
    target_include_directories(e2e_test_snn_pipeline PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/snn
    )
    target_link_libraries(e2e_test_snn_pipeline PRIVATE
        nimcp_snn
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_snn_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_snn_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;snn"
            TIMEOUT 120
    )
    message(STATUS "  Added SNN E2E test: e2e_test_snn_pipeline")
endif()

# SNN Bridge Pipeline E2E Test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_snn_bridge_pipeline.cpp)
    add_executable(e2e_test_snn_bridge_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_snn_bridge_pipeline.cpp
    )
    target_include_directories(e2e_test_snn_bridge_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/snn
    )
    target_link_libraries(e2e_test_snn_bridge_pipeline PRIVATE
        nimcp_snn
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_snn_bridge_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_snn_bridge_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;snn;bridges"
            TIMEOUT 180
    )
    message(STATUS "  Added SNN Bridge E2E test: e2e_test_snn_bridge_pipeline")
endif()

# ============================================================================
# Quantum Bridges Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for quantum-enhanced cognitive bridges
# WHY:  Verify quantum acceleration works in complete cognitive pipelines
# HOW:  Test attention, curiosity, emotion, executive, WM, BCM, thalamic integration
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_quantum_bridges_pipeline.cpp)
    add_executable(e2e_test_quantum_bridges_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_quantum_bridges_pipeline.cpp
    )
    target_include_directories(e2e_test_quantum_bridges_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/plasticity/attention
        ${CMAKE_SOURCE_DIR}/include/cognitive/curiosity
        ${CMAKE_SOURCE_DIR}/include/cognitive/emotion
        ${CMAKE_SOURCE_DIR}/include/cognitive/executive
        ${CMAKE_SOURCE_DIR}/include/cognitive/memory
        ${CMAKE_SOURCE_DIR}/include/core/logic
        ${CMAKE_SOURCE_DIR}/include/plasticity/bcm
        ${CMAKE_SOURCE_DIR}/include/middleware/routing
    )
    target_link_libraries(e2e_test_quantum_bridges_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_quantum_bridges_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_quantum_bridges_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;quantum;bridges"
            TIMEOUT 180
    )
    message(STATUS "  Added Quantum Bridges E2E test: e2e_test_quantum_bridges_pipeline")
endif()

# ============================================================================
# Pink Noise Enhancement Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for 9 pink noise enhancement modules
# WHY:  Verify complete neural noise pipeline with all enhancements
# HOW:  Test multiscale, correlated, criticality, quantum, immune, sleep,
#       SIMD, monitor, and spatial noise in integrated brain scenarios
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_pink_noise_pipeline.cpp)
    add_executable(e2e_test_pink_noise_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_pink_noise_pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    )
    target_include_directories(e2e_test_pink_noise_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/plasticity/noise
    )
    target_link_libraries(e2e_test_pink_noise_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_pink_noise_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_pink_noise_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;plasticity;noise;pink"
            TIMEOUT 300
    )
    message(STATUS "  Added Pink Noise E2E test: e2e_test_pink_noise_pipeline")
endif()

# ============================================================================
# Pink Noise + Fractal Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for Pink Noise + Fractal Analysis integration
# WHY:  Verify complete noise generation and fractal analysis pipeline
# HOW:  Test neural criticality, synaptic modulation, memory consolidation,
#       sleep stage characterization, and complete simulation workflows
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_pink_noise_fractal_pipeline.cpp)
    add_executable(e2e_test_pink_noise_fractal_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_pink_noise_fractal_pipeline.cpp
    )
    target_include_directories(e2e_test_pink_noise_fractal_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/plasticity/noise
        ${CMAKE_SOURCE_DIR}/include/cognitive/memory/core
    )
    target_link_libraries(e2e_test_pink_noise_fractal_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_pink_noise_fractal_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_pink_noise_fractal_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;plasticity;noise;pink;fractal"
            TIMEOUT 300
    )
    message(STATUS "  Added Pink Noise + Fractal E2E test: e2e_test_pink_noise_fractal_pipeline")
endif()

# ============================================================================
# Enhanced Curiosity Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for Enhanced Curiosity System (10 enhancements)
# WHY:  Verify complete curiosity-driven exploration pipeline
# HOW:  Test boredom, interest, types, anxiety, social, meta, contagion,
#       surprise, fatigue, counterfactual in integrated scenarios
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_curiosity_enhanced_pipeline.cpp)
    add_executable(e2e_test_curiosity_enhanced_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_curiosity_enhanced_pipeline.cpp
    )
    target_include_directories(e2e_test_curiosity_enhanced_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/curiosity
    )
    target_link_libraries(e2e_test_curiosity_enhanced_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_curiosity_enhanced_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_curiosity_enhanced_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;cognitive;curiosity;enhanced"
            TIMEOUT 300
    )
    message(STATUS "  Added Enhanced Curiosity E2E test: e2e_test_curiosity_enhanced_pipeline")
endif()

# ============================================================================
# Information Forager E2E Pipeline Test
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cognitive/e2e_test_information_forager_pipeline.cpp)
    add_executable(e2e_test_information_forager_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/cognitive/e2e_test_information_forager_pipeline.cpp
    )
    target_include_directories(e2e_test_information_forager_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/curiosity
    )
    target_link_libraries(e2e_test_information_forager_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_information_forager_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_information_forager_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;cognitive;curiosity;forager"
            TIMEOUT 300
    )
    message(STATUS "  Added Information Forager E2E test: e2e_test_information_forager_pipeline")
endif()

# ============================================================================
# Phase 1 Bridges Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for Phase 1 bridges (dendrite, synapse, axon, glial-immune)
# WHY:  Verify complete bridge pipeline in real-world scenarios
# HOW:  Test learning, immune challenge, myelination, coordinated operation
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_phase1_bridges_pipeline.cpp)
    add_executable(e2e_test_phase1_bridges_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_phase1_bridges_pipeline.cpp
    )
    target_include_directories(e2e_test_phase1_bridges_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/plasticity/bridges
        ${CMAKE_SOURCE_DIR}/include/glial/immune
    )
    target_link_libraries(e2e_test_phase1_bridges_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_phase1_bridges_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_phase1_bridges_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;plasticity;bridges;phase1"
            TIMEOUT 300
    )
    message(STATUS "  Added Phase 1 Bridges E2E test: e2e_test_phase1_bridges_pipeline")
endif()

# ============================================================================
# Subcortical Brain Structures Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for basal ganglia action selection and reinforcement learning
# WHY:  Verify complete subcortical pipeline in realistic decision scenarios
# HOW:  Test action selection, RL learning, habit formation, pathway dynamics
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_subcortical_pipeline.cpp)
    add_executable(e2e_test_subcortical_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_subcortical_pipeline.cpp
    )
    target_include_directories(e2e_test_subcortical_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/core/brain/subcortical
    )
    target_link_libraries(e2e_test_subcortical_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_subcortical_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_subcortical_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;core;subcortical;basal_ganglia"
            TIMEOUT 180
    )
    message(STATUS "  Added Subcortical E2E test: e2e_test_subcortical_pipeline")
endif()

# ============================================================================
# Thalamus Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for thalamic relay and attention gating pipelines
# WHY:  Verify complete thalamus-cortex relay in realistic scenarios
# HOW:  Test visual/motor relay, attention modulation, arousal transitions
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_thalamus_pipeline.cpp)
    add_executable(e2e_test_thalamus_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_thalamus_pipeline.cpp
    )
    target_include_directories(e2e_test_thalamus_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/core/brain/subcortical
    )
    target_link_libraries(e2e_test_thalamus_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_thalamus_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_thalamus_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;core;subcortical;thalamus"
            TIMEOUT 180
    )
    message(STATUS "  Added Thalamus E2E test: e2e_test_thalamus_pipeline")
endif()

# ============================================================================
# Amygdala Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for amygdala fear conditioning and emotion pipelines
# WHY:  Verify complete fear learning, extinction, and renewal in realistic scenarios
# HOW:  Test Pavlovian conditioning, contextual fear, neuromodulation effects
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_amygdala_pipeline.cpp)
    add_executable(e2e_test_amygdala_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_amygdala_pipeline.cpp
    )
    target_include_directories(e2e_test_amygdala_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/core/brain/subcortical
    )
    target_link_libraries(e2e_test_amygdala_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_amygdala_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_amygdala_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;core;subcortical;amygdala"
            TIMEOUT 180
    )
    message(STATUS "  Added Amygdala E2E test: e2e_test_amygdala_pipeline")
endif()

# ============================================================================
# Neuromodulatory Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for neuromodulatory centers integration (LC, VTA, Raphe, Habenula)
# WHY:  Verify bidirectional SNN/Plasticity integration across all neuromodulators
# HOW:  Test reward learning, aversive learning, attention, mood, cross-center coordination
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_neuromodulatory_pipeline.cpp)
    add_executable(e2e_test_neuromodulatory_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_neuromodulatory_pipeline.cpp
    )
    target_include_directories(e2e_test_neuromodulatory_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/core/brain/regions
    )
    target_link_libraries(e2e_test_neuromodulatory_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_neuromodulatory_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_neuromodulatory_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;core;brain_regions;neuromodulatory;lc;vta;raphe;habenula"
            TIMEOUT 180
    )
    message(STATUS "  Added Neuromodulatory E2E test: e2e_test_neuromodulatory_pipeline")
endif()

# ============================================================================
# Dragonfly Hunting Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for complete dragonfly hunting pipeline
# WHY:  Verify 95% interception success rate with realistic physics
# HOW:  Simulate full hunt from detection to interception
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_dragonfly_hunting_pipeline.cpp)
    add_executable(e2e_test_dragonfly_hunting_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_dragonfly_hunting_pipeline.cpp
    )
    target_include_directories(e2e_test_dragonfly_hunting_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/dragonfly
    )
    target_link_libraries(e2e_test_dragonfly_hunting_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_dragonfly_hunting_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_dragonfly_hunting_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;dragonfly;hunting"
            TIMEOUT 300
    )
    message(STATUS "  Added Dragonfly Hunting E2E test: e2e_test_dragonfly_hunting_pipeline")
endif()

# ============================================================================
# Dragonfly Multi-Target E2E Test
# ============================================================================
# WHAT: E2E tests for dragonfly handling multiple simultaneous targets
# WHY:  Real scenarios have multiple prey items - CSTMD1 must select one
# HOW:  Present multiple targets, verify winner-take-all selection
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_dragonfly_multi_target.cpp)
    add_executable(e2e_test_dragonfly_multi_target
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_dragonfly_multi_target.cpp
    )
    target_include_directories(e2e_test_dragonfly_multi_target PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/dragonfly
    )
    target_link_libraries(e2e_test_dragonfly_multi_target PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_dragonfly_multi_target PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_dragonfly_multi_target
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;dragonfly;multi-target"
            TIMEOUT 300
    )
    message(STATUS "  Added Dragonfly Multi-Target E2E test: e2e_test_dragonfly_multi_target")
endif()

# ============================================================================
# Basal Ganglia Integration E2E Test
# ============================================================================
# WHAT: Test enhanced BG integration with brain lifecycle
# WHY:  Validate action selection, reward processing, emotional integration
# HOW:  Create brain → init BG → select actions → process rewards
# ============================================================================
add_e2e_test(e2e_test_basal_ganglia_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_basal_ganglia_pipeline.cpp
)

# ============================================================================
# Quantum Reasoning Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for quantum reasoning with brain integration
# WHY:  Verify Grover-accelerated SAT solving in cognitive pipelines
# HOW:  Test transitive/disjunctive reasoning, conflict detection, planning
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_quantum_reasoning_pipeline.cpp)
    add_executable(e2e_test_quantum_reasoning_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_quantum_reasoning_pipeline.cpp
    )
    target_include_directories(e2e_test_quantum_reasoning_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/reasoning
        ${CMAKE_SOURCE_DIR}/include/core/brain/inference
    )
    target_link_libraries(e2e_test_quantum_reasoning_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_quantum_reasoning_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_quantum_reasoning_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;quantum;reasoning"
            TIMEOUT 180
    )
    message(STATUS "  Added Quantum Reasoning E2E test: e2e_test_quantum_reasoning_pipeline")
endif()

# ============================================================================
# Quantum Language Processing Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for quantum-accelerated language production
# WHY:  Verify Broca quantum bridge in full language pipelines
# HOW:  Test word retrieval, syntax planning, phoneme optimization
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_quantum_language_processing.cpp)
    add_executable(e2e_test_quantum_language_processing
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_quantum_language_processing.cpp
    )
    target_include_directories(e2e_test_quantum_language_processing PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/core/brain/regions/broca
    )
    target_link_libraries(e2e_test_quantum_language_processing PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_quantum_language_processing PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_quantum_language_processing
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;quantum;language;broca"
            TIMEOUT 180
    )
    message(STATUS "  Added Quantum Language E2E test: e2e_test_quantum_language_processing")
endif()

# ============================================================================
# Auditory Processing Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for auditory cortex processing pipeline
# WHY:  Verify complete audio sensory-to-cognitive flow
# HOW:  Test sound localization, speech processing, auditory attention
# ============================================================================
add_e2e_test(e2e_test_auditory_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_auditory_pipeline.cpp
)

# ============================================================================
# Cerebellum Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for cerebellar motor coordination pipeline
# WHY:  Verify motor learning, timing, and coordination
# HOW:  Test forward models, error correction, predictive timing
# ============================================================================
add_e2e_test(e2e_test_cerebellum_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_cerebellum_pipeline.cpp
)

# ============================================================================
# Entorhinal Cortex E2E Test
# ============================================================================
# WHAT: E2E tests for entorhinal cortex spatial navigation pipeline
# WHY:  Verify grid cells, path integration, memory gateway
# HOW:  Test spatial navigation, memory encoding, hippocampus integration
# ============================================================================
add_e2e_test(e2e_test_entorhinal_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_entorhinal_pipeline.cpp
)

# ============================================================================
# Dragonfly Multimodal E2E Test
# ============================================================================
# WHAT: E2E tests for dragonfly multi-sensory integration
# WHY:  Verify visual-audio-motor coordination for hunting
# HOW:  Test cross-modal binding, attention switching, pursuit
# ============================================================================
add_e2e_test(e2e_test_dragonfly_multimodal
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_dragonfly_multimodal.cpp
)

# ============================================================================
# Dragonfly Main Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for complete dragonfly hunting pipeline
# WHY:  Verify target detection, tracking, interception
# HOW:  Test TSDN, CSTMD1, prediction, motor output
# ============================================================================
add_e2e_test(e2e_test_dragonfly_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_dragonfly_pipeline.cpp
)

# ============================================================================
# Hippocampus Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for hippocampal memory and navigation
# WHY:  Verify spatial memory, episodic encoding, consolidation
# HOW:  Test place cells, grid cells, memory formation
# ============================================================================
add_e2e_test(e2e_test_hippocampus_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_hippocampus_pipeline.cpp
)

# ============================================================================
# Perirhinal Cortex Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for perirhinal object recognition and familiarity
# WHY:  Verify object encoding, recognition memory, visual processing
# HOW:  Test object encoding, familiarity signals, visual input processing
# ============================================================================
add_e2e_test(e2e_test_perirhinal_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_perirhinal_pipeline.cpp
)

# ============================================================================
# Parahippocampal Cortex Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for parahippocampal scene and context processing
# WHY:  Verify scene encoding, spatial context, layout processing
# HOW:  Test scene encoding, landmark processing, spatial navigation
# ============================================================================
add_e2e_test(e2e_test_parahippocampal_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_parahippocampal_pipeline.cpp
)

# ============================================================================
# Mammillary Bodies Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for mammillary memory relay and Papez circuit
# WHY:  Verify memory relay, head direction, consolidation
# HOW:  Test hippocampal relay, thalamic output, Papez circuit cycles
# ============================================================================
add_e2e_test(e2e_test_mammillary_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_mammillary_pipeline.cpp
)

# ============================================================================
# Limbic System Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for limbic emotional processing
# WHY:  Verify emotion-cognition integration, motivation, reward
# HOW:  Test amygdala, reward circuits, emotional memory
# ============================================================================
add_e2e_test(e2e_test_limbic_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_limbic_pipeline.cpp
)

# ============================================================================
# Hypothalamus Alignment Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for Hypothalamus Alignment Hardening (Phase 19)
# WHY:  Verify Byrnes' Steering Subsystem model compliance
# HOW:  Test alignment constraints, callback chains, audit logging, safety invariants
# ============================================================================
add_e2e_test(e2e_test_hypothalamus_alignment_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_hypothalamus_alignment_pipeline.cpp
)

# ============================================================================
# Hypothalamus Exception Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for Hypothalamus Exception Handling with Brain Immune System
# WHY:  Verify the complete error handling pipeline works correctly under
#       realistic conditions with all components integrated
# HOW:  Test full pipeline error handling, multi-bridge error cascade, recovery
#       mechanisms, stress testing, FEP integration errors, concurrent errors,
#       and error statistics accuracy
# ============================================================================
add_e2e_test(e2e_test_hypothalamus_exception_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_hypothalamus_exception_pipeline.cpp
)
set_tests_properties(e2e_test_hypothalamus_exception_pipeline PROPERTIES
    LABELS "e2e;hypothalamus;exception;immune"
    TIMEOUT 180
)

# ============================================================================
# Brain Init Hypothalamus Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for Brain Factory Hypothalamus Initialization Pipeline
# WHY:  Verify full pipeline from brain creation through hypothalamus operation
# HOW:  Test brain creation, hypothalamus init, circadian rhythm, HPA axis,
#       homeostatic regulation, autonomic control, quantum bridge, cleanup
# ============================================================================
add_e2e_test(e2e_test_brain_init_hypothalamus_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_brain_init_hypothalamus_pipeline.cpp
)

# ============================================================================
# Hypothalamus Perception Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for Hypothalamus-Perception Bridge integration
# WHY:  Verify drive-perception modulation and sensory feedback to drives
# HOW:  Test interoception, chemosensory, predictive coding, pain modulation,
#       sleep gating, thermal salience in complete pipeline scenarios
# ============================================================================
add_e2e_test(e2e_test_hypothalamus_perception_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_hypothalamus_perception_pipeline.cpp
)

# ============================================================================
# Hypothalamus Logic Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for Hypothalamus-Logic Bridge integration
# WHY:  Verify motivated reasoning (hot cognition) and logic-driven drive updates
# HOW:  Test complete pipelines: hungry agent food search, fearful agent threat
#       assessment, curious agent exploration, conflicting drives, goal lifecycle,
#       FEP prediction loops, and extended simulation stability
# ============================================================================
add_e2e_test(e2e_test_hypothalamus_logic_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_hypothalamus_logic_pipeline.cpp
)

# ============================================================================
# Hypothalamus Parietal Integration E2E Test
# ============================================================================
# WHAT: E2E tests for Hypothalamus-Parietal integration
# WHY:  Verify hypothalamus drive states can coexist with parietal reasoning
# HOW:  Test both systems initialized, update together, stress/fatigue effects,
#       drive states and mathematical reasoning, autonomic balance, cleanup
# ============================================================================
add_e2e_test(e2e_test_hypothalamus_parietal_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_hypothalamus_parietal_integration.cpp
)

# ============================================================================
# Motor Control Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for motor control and execution
# WHY:  Verify motor planning, execution, feedback control
# HOW:  Test motor cortex, basal ganglia, motor learning
# ============================================================================
add_e2e_test(e2e_test_motor_control_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_motor_control_pipeline.cpp
)

# ============================================================================
# Prefrontal Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for prefrontal executive functions
# WHY:  Verify decision making, working memory, planning
# HOW:  Test executive control, goal maintenance, cognitive flexibility
# ============================================================================
add_e2e_test(e2e_test_prefrontal_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_prefrontal_pipeline.cpp
)

# ============================================================================
# Somatosensory Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for somatosensory processing
# WHY:  Verify touch, proprioception, body awareness
# HOW:  Test tactile processing, body schema, sensorimotor integration
# ============================================================================
add_e2e_test(e2e_test_somatosensory_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_somatosensory_pipeline.cpp
)

# ============================================================================
# Visual Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for visual processing pipeline
# WHY:  Verify visual cortex hierarchy, object recognition, attention
# HOW:  Test V1-V4, IT cortex, visual attention, scene understanding
# ============================================================================
add_e2e_test(e2e_test_visual_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_visual_pipeline.cpp
)

# ============================================================================
# Swarm Memory Consolidation E2E Test (2025-12-31)
# ============================================================================
# WHAT: E2E tests for swarm memory consolidation workflow
# WHY:  Verify complete memory lifecycle with correct struct members
# HOW:  Test storage, consolidation, distribution, forgetting
# REGRESSION: Tests NimcpHippocampusNode->memory_count, payload_size usage
# ============================================================================
add_e2e_test(e2e_test_swarm_memory_consolidation
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_swarm_memory_consolidation.cpp
)

# ============================================================================
# Brain Initialization Subsystems E2E Test (2025-12-31)
# ============================================================================
# WHAT: E2E tests for brain subsystem initialization
# WHY:  Verify correct struct members and enum values during init
# HOW:  Test hypothalamus, wellbeing, collective cognition, power management
# REGRESSION: Tests wellbeing_monitoring_enabled, POWER_EVENT_BATTERY_CRITICAL
# ============================================================================
add_e2e_test(e2e_test_brain_initialization_subsystems
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_brain_initialization_subsystems.cpp
)

# ============================================================================
# Brain PR Memory Pipeline E2E Test (2026-01-10)
# ============================================================================
# WHAT: E2E test for Prime Resonant memory brain integration
# WHY:  Verify Z-Ladder, theta-gamma, and entanglement work with brain lifecycle
# HOW:  Test complete PR memory pipeline from creation through operation
add_e2e_test(e2e_test_brain_pr_memory_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_brain_pr_memory_pipeline.cpp
)

# ============================================================================
# Wiring Diagram Pipeline E2E Test (2026-01-03)
# ============================================================================
# WHAT: E2E test for KG-based runtime module wiring system
# WHY:  Verify complete wiring pipeline from JSONL loading to handler registration
# HOW:  Test wiring loading, discovery, callback invocation, and persistence
add_e2e_test(e2e_test_wiring_diagram_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_wiring_diagram_pipeline.cpp
)

# ============================================================================
# Omnidirectional Inference Pipeline E2E Test (2026-01-04)
# ============================================================================
# WHAT: E2E test for complete omnidirectional inference pipeline
# WHY:  Verify bidirectional JEPA, Hopfield memory, predictive hierarchy,
#       temporal replay, and all cortical bridges work together
# HOW:  Test full perception-to-action pipeline with all prediction directions
add_e2e_test(e2e_test_omni_inference_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_omni_inference_pipeline.cpp
)

# ============================================================================
# GPU Kernel Pipeline E2E Tests (2025-12-31)
# ============================================================================
# WHAT: E2E tests for GPU training, inference, SNN, and LNN pipelines
# WHY:  Verify complete GPU kernel operations in realistic neural workloads
# HOW:  Test full training loops, inference batches, SNN spikes, LNN dynamics
# ============================================================================

# GPU Neural Network Training Pipeline (full forward/backward pass)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_gpu_nn_training_pipeline.cpp)
    add_e2e_test(e2e_test_gpu_nn_training_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_gpu_nn_training_pipeline.cpp
    )
    set_tests_properties(e2e_test_gpu_nn_training_pipeline PROPERTIES
        LABELS "e2e;gpu;training"
        TIMEOUT 180
    )
endif()

# GPU Inference Pipeline (batched inference with quantization)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_gpu_inference_pipeline.cpp)
    add_e2e_test(e2e_test_gpu_inference_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_gpu_inference_pipeline.cpp
    )
    set_tests_properties(e2e_test_gpu_inference_pipeline PROPERTIES
        LABELS "e2e;gpu;inference"
        TIMEOUT 180
    )
endif()

# GPU SNN Pipeline (spiking neural network with STDP)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_gpu_snn_pipeline.cpp)
    add_e2e_test(e2e_test_gpu_snn_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_gpu_snn_pipeline.cpp
    )
    set_tests_properties(e2e_test_gpu_snn_pipeline PROPERTIES
        LABELS "e2e;gpu;snn"
        TIMEOUT 180
    )
endif()

# GPU LNN Pipeline (Liquid Neural Network with ODE solvers)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_gpu_lnn_pipeline.cpp)
    add_e2e_test(e2e_test_gpu_lnn_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_gpu_lnn_pipeline.cpp
    )
    set_tests_properties(e2e_test_gpu_lnn_pipeline PROPERTIES
        LABELS "e2e;gpu;lnn"
        TIMEOUT 180
    )
endif()

# GPU Training Pipeline (basic training with optimizers)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_gpu_training_pipeline.cpp)
    add_e2e_test(e2e_test_gpu_training_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_gpu_training_pipeline.cpp
    )
    set_tests_properties(e2e_test_gpu_training_pipeline PROPERTIES
        LABELS "e2e;gpu;training"
        TIMEOUT 180
    )
endif()

# ============================================================================
# Mental Health Guardian E2E Tests (Phase 10.5.1)
# ============================================================================
# WHAT: Full lifecycle tests for guardian in complete brain context
# WHY:  Verify guardian works correctly in production-like scenarios
# HOW:  Test full brain lifecycle with guardian monitoring

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_mental_health_guardian_e2e.cpp)
    add_e2e_test(e2e_test_mental_health_guardian
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_mental_health_guardian_e2e.cpp
    )
    set_tests_properties(e2e_test_mental_health_guardian PROPERTIES
        LABELS "e2e;cognitive;mental_health;guardian"
        TIMEOUT 180
    )
endif()

# ============================================================================
# GPU Broca Pipeline E2E Test
# ============================================================================
# WHAT: End-to-end tests for GPU-accelerated Broca language production pipeline
# WHY:  Validate full GPU Broca workflow from semantic intent to motor output
# HOW:  Test realistic utterance production with performance benchmarks
#
# PIPELINE STAGES:
# - Vocabulary loading -> Lexical lookup -> Phoneme encoding -> Motor planning
# ============================================================================
if(CUDAToolkit_FOUND AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_broca_gpu_pipeline.cpp)
    add_executable(e2e_test_broca_gpu_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_broca_gpu_pipeline.cpp
    )
    target_include_directories(e2e_test_broca_gpu_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/gpu/cognitive
    )
    target_link_libraries(e2e_test_broca_gpu_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_broca_gpu_pipeline PRIVATE cxx_std_20)
    target_compile_definitions(e2e_test_broca_gpu_pipeline PRIVATE NIMCP_TESTING)
    add_test(NAME e2e_test_broca_gpu_pipeline COMMAND e2e_test_broca_gpu_pipeline)
    set_tests_properties(e2e_test_broca_gpu_pipeline PROPERTIES
        LABELS "e2e;gpu;broca;language"
        TIMEOUT 300
    )
    message(STATUS "  Added GPU Broca Pipeline E2E test: e2e_test_broca_gpu_pipeline")
endif()

# ============================================================================
# E2E Test: GPU Neural Substrate Pipeline
# ============================================================================
# WHAT: End-to-end test for GPU-accelerated neural substrate pipeline
# WHY:  Validate complete GPU substrate from initialization to simulation
# HOW:  Test all subsystems (axon, dendrite, myelin, neuromod, glial, metabolic)
#
# PIPELINE STAGES:
# - Full substrate initialization -> Multi-step simulation -> Performance benchmarks
# ============================================================================
if(CUDAToolkit_FOUND AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_substrate_gpu_pipeline.cpp)
    add_executable(e2e_test_substrate_gpu_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_substrate_gpu_pipeline.cpp
    )
    target_include_directories(e2e_test_substrate_gpu_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/gpu/substrate
    )
    target_link_libraries(e2e_test_substrate_gpu_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_substrate_gpu_pipeline PRIVATE cxx_std_20)
    target_compile_definitions(e2e_test_substrate_gpu_pipeline PRIVATE NIMCP_TESTING)
    add_test(NAME e2e_test_substrate_gpu_pipeline COMMAND e2e_test_substrate_gpu_pipeline)
    set_tests_properties(e2e_test_substrate_gpu_pipeline PROPERTIES
        LABELS "e2e;gpu;substrate;neural"
        TIMEOUT 300
    )
    message(STATUS "  Added GPU Substrate Pipeline E2E test: e2e_test_substrate_gpu_pipeline")
endif()

# ============================================================================
# Language Layer Pipeline E2E Test
# ============================================================================
# WHAT: End-to-end tests for complete language layer pipeline
# WHY:  Validate full language processing workflow with all bridges
# HOW:  Test realistic language scenarios from perception to production
#
# PIPELINE STAGES:
# - Perception input -> Comprehension (Wernicke) -> Semantic integration
# - Production (Broca) -> Motor output
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_language_layer_pipeline.cpp)
    add_executable(e2e_test_language_layer_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_language_layer_pipeline.cpp
    )
    target_include_directories(e2e_test_language_layer_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/language
    )
    target_link_libraries(e2e_test_language_layer_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_language_layer_pipeline PRIVATE cxx_std_20)
    target_compile_definitions(e2e_test_language_layer_pipeline PRIVATE NIMCP_TESTING)
    add_test(NAME e2e_test_language_layer_pipeline COMMAND e2e_test_language_layer_pipeline)
    set_tests_properties(e2e_test_language_layer_pipeline PROPERTIES
        LABELS "e2e;language;orchestrator;bridges"
        TIMEOUT 300
    )
    message(STATUS "  Added Language Layer Pipeline E2E test: e2e_test_language_layer_pipeline")
endif()

# ============================================================================
# Mirror Empathy Pipeline E2E Test (Phase 10.11.11-12)
# ============================================================================
# WHAT: End-to-end tests for mirror-emotion-attention empathy pipeline
# WHY:  Validate full empathy processing from observation to response
# HOW:  Test realistic social scenarios combining emotion and attention
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_mirror_empathy_pipeline.cpp)
    add_executable(e2e_test_mirror_empathy_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_mirror_empathy_pipeline.cpp
    )
    target_include_directories(e2e_test_mirror_empathy_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/mirror_neurons
    )
    target_link_libraries(e2e_test_mirror_empathy_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_mirror_empathy_pipeline PRIVATE cxx_std_20)
    target_compile_definitions(e2e_test_mirror_empathy_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_mirror_empathy_pipeline COMMAND e2e_test_mirror_empathy_pipeline)
    set_tests_properties(e2e_test_mirror_empathy_pipeline PROPERTIES
        LABELS "e2e;cognitive;mirror_neurons;empathy;emotion;attention"
        TIMEOUT 180
    )
    message(STATUS "  Added Mirror Empathy Pipeline E2E test: e2e_test_mirror_empathy_pipeline")
endif()

# ============================================================================
# Mirror SNN-Plasticity Learning Pipeline E2E Test
# ============================================================================
# WHAT: End-to-end tests for mirror SNN and plasticity learning integration
# WHY:  Validate full learning pipeline from observation to behavioral adaptation
# HOW:  Test realistic learning scenarios with spike encoding and STDP
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_mirror_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_mirror_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_mirror_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_mirror_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/mirror_neurons
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_mirror_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_mirror_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_mirror_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_mirror_snn_plasticity_pipeline COMMAND e2e_test_mirror_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_mirror_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;mirror_neurons;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Mirror SNN-Plasticity Pipeline E2E test: e2e_test_mirror_snn_plasticity_pipeline")
endif()

# ============================================================================
# Ethics-SNN-Plasticity Pipeline E2E Tests
# ============================================================================
# WHAT: End-to-end tests for ethics SNN and plasticity learning integration
# WHY:  Validate full ethical reasoning pipeline with protected synapses
# HOW:  Test moral scenarios, harm detection, and value learning
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_ethics_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_ethics_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_ethics_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_ethics_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/ethics
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_ethics_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_ethics_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_ethics_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_ethics_snn_plasticity_pipeline COMMAND e2e_test_ethics_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_ethics_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;ethics;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Ethics SNN-Plasticity Pipeline E2E test: e2e_test_ethics_snn_plasticity_pipeline")
endif()

# ============================================================================
# Wellbeing SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Test wellbeing + SNN + plasticity integration
# WHY:  Verify complete wellbeing learning pipeline
# HOW:  Test flourishing scenarios, stress detection, and protected synapses
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_wellbeing_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_wellbeing_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_wellbeing_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_wellbeing_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/wellbeing
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_wellbeing_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_wellbeing_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_wellbeing_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_wellbeing_snn_plasticity_pipeline COMMAND e2e_test_wellbeing_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_wellbeing_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;wellbeing;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Wellbeing SNN-Plasticity Pipeline E2E test: e2e_test_wellbeing_snn_plasticity_pipeline")
endif()

# ============================================================================
# Introspection SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Test introspection + SNN + plasticity integration
# WHY:  Verify complete metacognitive learning pipeline
# HOW:  Test calibration scenarios, confidence learning, and protected synapses
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_introspection_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_introspection_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_introspection_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_introspection_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/introspection
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_introspection_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_introspection_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_introspection_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_introspection_snn_plasticity_pipeline COMMAND e2e_test_introspection_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_introspection_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;introspection;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Introspection SNN-Plasticity Pipeline E2E test: e2e_test_introspection_snn_plasticity_pipeline")
endif()

# ============================================================================
# Attention SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Test attention + SNN + plasticity integration
# WHY:  Verify complete attention learning pipeline
# HOW:  Test focus scenarios, habituation learning, and protected synapses
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_attention_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_attention_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_attention_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_attention_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/attention
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_attention_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_attention_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_attention_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_attention_snn_plasticity_pipeline COMMAND e2e_test_attention_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_attention_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;attention;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Attention SNN-Plasticity Pipeline E2E test: e2e_test_attention_snn_plasticity_pipeline")
endif()

# ============================================================================
# Emotion SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Test emotion + SNN + plasticity integration
# WHY:  Verify complete emotional conditioning learning pipeline
# HOW:  Test conditioning scenarios, extinction learning, and protected synapses
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_emotion_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_emotion_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_emotion_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_emotion_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/emotion
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_emotion_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_emotion_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_emotion_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_emotion_snn_plasticity_pipeline COMMAND e2e_test_emotion_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_emotion_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;emotion;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Emotion SNN-Plasticity Pipeline E2E test: e2e_test_emotion_snn_plasticity_pipeline")
endif()

# ============================================================================
# Working Memory SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Test working memory + SNN + plasticity integration
# WHY:  Verify complete memory maintenance and consolidation learning pipeline
# HOW:  Test encoding scenarios, consolidation learning, and protected synapses
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_memory_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_memory_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_memory_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_memory_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/memory
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_memory_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_memory_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_memory_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_memory_snn_plasticity_pipeline COMMAND e2e_test_memory_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_memory_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;memory;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Working Memory SNN-Plasticity Pipeline E2E test: e2e_test_memory_snn_plasticity_pipeline")
endif()

# ============================================================================
# Curiosity SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Test curiosity + SNN + plasticity integration
# WHY:  Verify complete exploration-driven learning pipeline
# HOW:  Test novelty scenarios, exploration learning, and protected synapses
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_curiosity_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_curiosity_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_curiosity_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_curiosity_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/curiosity
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_curiosity_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_curiosity_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_curiosity_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_curiosity_snn_plasticity_pipeline COMMAND e2e_test_curiosity_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_curiosity_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;curiosity;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Curiosity SNN-Plasticity Pipeline E2E test: e2e_test_curiosity_snn_plasticity_pipeline")
endif()

# ============================================================================
# Global Workspace SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Full conscious access learning pipeline tests
# WHY:  Verify complete GWT-based broadcast learning pipeline
# HOW:  Test broadcast scenarios, ignition learning, and protected synapses
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_gw_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_gw_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_gw_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_gw_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/global_workspace
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_gw_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_gw_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_gw_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_gw_snn_plasticity_pipeline COMMAND e2e_test_gw_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_gw_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;global_workspace;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Global Workspace SNN-Plasticity Pipeline E2E test: e2e_test_gw_snn_plasticity_pipeline")
endif()

# ============================================================================
# Self Model SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Test self model + SNN + plasticity integration
# WHY:  Verify complete self-awareness learning pipeline
# HOW:  Test agency scenarios, boundary detection, and protected synapses
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_self_model_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_self_model_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_self_model_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_self_model_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/self_model
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_self_model_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_self_model_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_self_model_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_self_model_snn_plasticity_pipeline COMMAND e2e_test_self_model_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_self_model_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;self_model;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Self Model SNN-Plasticity Pipeline E2E test: e2e_test_self_model_snn_plasticity_pipeline")
endif()

# ============================================================================
# Theory of Mind SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Test theory of mind + SNN + plasticity integration
# WHY:  Verify complete social cognition learning pipeline
# HOW:  Test belief attribution, perspective-taking, empathy, and protected synapses
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_tom_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_tom_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_tom_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_tom_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/theory_of_mind
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_tom_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_tom_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_tom_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_tom_snn_plasticity_pipeline COMMAND e2e_test_tom_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_tom_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;theory_of_mind;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Theory of Mind SNN-Plasticity Pipeline E2E test: e2e_test_tom_snn_plasticity_pipeline")
endif()

# ============================================================================
# Reasoning SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Test reasoning + SNN + plasticity integration
# WHY:  Verify complete logical reasoning learning pipeline
# HOW:  Test deduction scenarios, causal chains, and protected synapses
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_reasoning_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_reasoning_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_reasoning_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_reasoning_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/reasoning
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_reasoning_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_reasoning_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_reasoning_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_reasoning_snn_plasticity_pipeline COMMAND e2e_test_reasoning_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_reasoning_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;reasoning;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Reasoning SNN-Plasticity Pipeline E2E test: e2e_test_reasoning_snn_plasticity_pipeline")
endif()

# ============================================================================
# Meta Learning SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Test meta learning + SNN + plasticity integration
# WHY:  Verify complete learning-to-learn pipeline
# HOW:  Test transfer learning, strategy selection, and protected synapses
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_meta_learning_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_meta_learning_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_meta_learning_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_meta_learning_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/meta_learning
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_meta_learning_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_meta_learning_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_meta_learning_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_meta_learning_snn_plasticity_pipeline COMMAND e2e_test_meta_learning_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_meta_learning_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;meta_learning;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Meta Learning SNN-Plasticity Pipeline E2E test: e2e_test_meta_learning_snn_plasticity_pipeline")
endif()

# ============================================================================
# Executive SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Test executive + SNN + plasticity integration
# WHY:  Verify complete cognitive control learning pipeline
# HOW:  Test planning, inhibition, task switching, and protected synapses
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_executive_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_executive_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_executive_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_executive_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/executive
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_executive_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_executive_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_executive_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_executive_snn_plasticity_pipeline COMMAND e2e_test_executive_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_executive_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;executive;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Executive SNN-Plasticity Pipeline E2E test: e2e_test_executive_snn_plasticity_pipeline")
endif()

# ============================================================================
# Epistemic SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Test epistemic filtering + SNN + plasticity integration
# WHY:  Verify complete evidence evaluation and source reliability pipeline
# HOW:  Test quality assessment, bias detection, belief revision, learning
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_epistemic_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_epistemic_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_epistemic_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_epistemic_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/epistemic
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_epistemic_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_epistemic_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_epistemic_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_epistemic_snn_plasticity_pipeline COMMAND e2e_test_epistemic_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_epistemic_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;epistemic;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Epistemic SNN-Plasticity Pipeline E2E test: e2e_test_epistemic_snn_plasticity_pipeline")
endif()

# ============================================================================
# Bias SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Test cognitive bias detection + SNN + plasticity integration
# WHY:  Verify complete bias detection and correction pipeline
# HOW:  Test bias encoding, conflict detection, metacognitive monitoring, learning
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_bias_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_bias_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_bias_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_bias_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/bias
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_bias_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_bias_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_bias_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_bias_snn_plasticity_pipeline COMMAND e2e_test_bias_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_bias_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;bias;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Bias SNN-Plasticity Pipeline E2E test: e2e_test_bias_snn_plasticity_pipeline")
endif()

# ============================================================================
# Salience SNN-Plasticity Pipeline E2E Test
# ============================================================================
# WHAT: Test salience detection + SNN + plasticity integration
# WHY:  Verify complete salience processing and habituation pipeline
# HOW:  Test novelty detection, urgency evaluation, habituation learning
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_salience_snn_plasticity_pipeline.cpp)
    add_executable(e2e_test_salience_snn_plasticity_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_salience_snn_plasticity_pipeline.cpp
    )
    target_include_directories(e2e_test_salience_snn_plasticity_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/salience
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_salience_snn_plasticity_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_salience_snn_plasticity_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_salience_snn_plasticity_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_salience_snn_plasticity_pipeline COMMAND e2e_test_salience_snn_plasticity_pipeline)
    set_tests_properties(e2e_test_salience_snn_plasticity_pipeline PROPERTIES
        LABELS "e2e;cognitive;salience;snn;plasticity;learning"
        TIMEOUT 300
    )
    message(STATUS "  Added Salience SNN-Plasticity Pipeline E2E test: e2e_test_salience_snn_plasticity_pipeline")
endif()

# ============================================================================
# Cognitive Integration Pipeline E2E Test
# ============================================================================
# WHAT: Test cognitive integration hub and bridges working together
# WHY:  Verify complete intra-layer cognitive processing pipeline
# HOW:  Test emotion-memory, attention-wm, curiosity-reasoning, ethics-executive, etc.
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_cognitive_integration_pipeline.cpp)
    add_executable(e2e_test_cognitive_integration_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_cognitive_integration_pipeline.cpp
    )
    target_include_directories(e2e_test_cognitive_integration_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/integration
    )
    target_link_libraries(e2e_test_cognitive_integration_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_cognitive_integration_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_cognitive_integration_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_cognitive_integration_pipeline COMMAND e2e_test_cognitive_integration_pipeline)
    set_tests_properties(e2e_test_cognitive_integration_pipeline PROPERTIES
        LABELS "e2e;cognitive;integration;hub;bridges"
        TIMEOUT 300
    )
    message(STATUS "  Added Cognitive Integration Pipeline E2E test: e2e_test_cognitive_integration_pipeline")
endif()

# ============================================================================
# Cognitive SNN-Plasticity Integration E2E Test
# ============================================================================
# WHAT: Test intra-layer bridges work alongside inter-layer SNN/plasticity bridges
# WHY:  Verify cross-layer integration for complete cognitive processing
# HOW:  Test cognitive hub with SNN and plasticity bridges
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_cognitive_snn_plasticity_integration.cpp)
    add_executable(e2e_test_cognitive_snn_plasticity_integration
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_cognitive_snn_plasticity_integration.cpp
    )
    target_include_directories(e2e_test_cognitive_snn_plasticity_integration PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/integration
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_cognitive_snn_plasticity_integration PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_cognitive_snn_plasticity_integration PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_cognitive_snn_plasticity_integration PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_cognitive_snn_plasticity_integration COMMAND e2e_test_cognitive_snn_plasticity_integration)
    set_tests_properties(e2e_test_cognitive_snn_plasticity_integration PROPERTIES
        LABELS "e2e;cognitive;integration;snn;plasticity"
        TIMEOUT 300
    )
    message(STATUS "  Added Cognitive SNN-Plasticity Integration E2E test: e2e_test_cognitive_snn_plasticity_integration")
endif()

# ============================================================================
# Recursive-Collective Cognition Integration E2E Test
# ============================================================================
# WHAT: Test recursive and collective cognition bridge integrations end-to-end
# WHY:  Verify recursive task decomposition works with collective intelligence
# HOW:  Test complete pipelines with both module bridges coordinated by FEP
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_rcog_collective_integration.cpp)
    add_executable(e2e_test_rcog_collective_integration
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_rcog_collective_integration.cpp
    )
    target_include_directories(e2e_test_rcog_collective_integration PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/recursive
        ${CMAKE_SOURCE_DIR}/include/cognitive/collective_cognition
        ${CMAKE_SOURCE_DIR}/include/cognitive/free_energy
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
    )
    target_link_libraries(e2e_test_rcog_collective_integration PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_rcog_collective_integration PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_rcog_collective_integration PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_rcog_collective_integration COMMAND e2e_test_rcog_collective_integration)
    set_tests_properties(e2e_test_rcog_collective_integration PROPERTIES
        LABELS "e2e;cognitive;recursive;collective;integration;snn;plasticity;fep"
        TIMEOUT 300
    )
    message(STATUS "  Added Recursive-Collective Cognition Integration E2E test: e2e_test_rcog_collective_integration")
endif()

# ============================================================================
# Tier 1 FEP Bridges E2E Test
# ============================================================================
# WHAT: Test all Tier 1 FEP bridges (game_theory, imagination, parietal, social, jepa)
# WHY:  Verify all Tier 1 cognitive bridges integrate correctly with FEP orchestrator
# HOW:  Test complete pipelines with all 5 bridges coordinated by FEP orchestration
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_tier1_fep_bridges.cpp)
    add_executable(e2e_test_tier1_fep_bridges
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_tier1_fep_bridges.cpp
    )
    target_include_directories(e2e_test_tier1_fep_bridges PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/game_theory
        ${CMAKE_SOURCE_DIR}/include/cognitive/imagination
        ${CMAKE_SOURCE_DIR}/include/cognitive/parietal
        ${CMAKE_SOURCE_DIR}/include/cognitive/social
        ${CMAKE_SOURCE_DIR}/include/cognitive/jepa
        ${CMAKE_SOURCE_DIR}/include/cognitive/free_energy
    )
    target_link_libraries(e2e_test_tier1_fep_bridges PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_tier1_fep_bridges PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_tier1_fep_bridges PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_tier1_fep_bridges COMMAND e2e_test_tier1_fep_bridges)
    set_tests_properties(e2e_test_tier1_fep_bridges PROPERTIES
        LABELS "e2e;cognitive;fep;tier1;game_theory;imagination;parietal;social;jepa;integration"
        TIMEOUT 300
    )
    message(STATUS "  Added Tier 1 FEP Bridges E2E test: e2e_test_tier1_fep_bridges")
endif()

# ============================================================================
# Tier 2 Hub Bridges E2E Test
# ============================================================================
# WHAT: Test all Tier 2 Hub bridges (imagination_reasoning, game_theory_executive,
#       mirror_empathy, salience_attention, predictive_attention)
# WHY:  Verify all Tier 2 cognitive bridges integrate correctly with cognitive hub
# HOW:  Test complete pipelines with all 5 bridges coordinated by cognitive hub
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_tier2_hub_bridges.cpp)
    add_executable(e2e_test_tier2_hub_bridges
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_tier2_hub_bridges.cpp
    )
    target_include_directories(e2e_test_tier2_hub_bridges PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/integration
        ${CMAKE_SOURCE_DIR}/include/cognitive/imagination
        ${CMAKE_SOURCE_DIR}/include/cognitive/game_theory
        ${CMAKE_SOURCE_DIR}/include/cognitive/social
        ${CMAKE_SOURCE_DIR}/include/cognitive/attention
        ${CMAKE_SOURCE_DIR}/include/cognitive/prediction
    )
    target_link_libraries(e2e_test_tier2_hub_bridges PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_tier2_hub_bridges PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_tier2_hub_bridges PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_tier2_hub_bridges COMMAND e2e_test_tier2_hub_bridges)
    set_tests_properties(e2e_test_tier2_hub_bridges PROPERTIES
        LABELS "e2e;cognitive;hub;tier2;imagination;reasoning;game_theory;executive;mirror;empathy;salience;attention;predictive;integration"
        TIMEOUT 300
    )
    message(STATUS "  Added Tier 2 Hub Bridges E2E test: e2e_test_tier2_hub_bridges")
endif()

# ============================================================================
# Tier 3 FEP Bridges E2E Test (FEP bridges for Tier 2 Hub bridges)
# ============================================================================
# WHAT: Test all Tier 3 FEP bridges (FEP integration for Hub bridges)
# WHY:  Verify Tier 2 Hub bridges integrate correctly with FEP orchestrator
# HOW:  Test complete pipelines with all 5 FEP bridges for Hub bridges
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_tier3_fep_bridges.cpp)
    add_executable(e2e_test_tier3_fep_bridges
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_tier3_fep_bridges.cpp
    )
    target_include_directories(e2e_test_tier3_fep_bridges PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/cognitive/integration
        ${CMAKE_SOURCE_DIR}/include/cognitive/free_energy
    )
    target_link_libraries(e2e_test_tier3_fep_bridges PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_tier3_fep_bridges PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_tier3_fep_bridges PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_tier3_fep_bridges COMMAND e2e_test_tier3_fep_bridges)
    set_tests_properties(e2e_test_tier3_fep_bridges PROPERTIES
        LABELS "e2e;cognitive;fep;tier3;imagination_reasoning;game_theory_executive;mirror_empathy;salience_attention;predictive_attention;integration"
        TIMEOUT 300
    )
    message(STATUS "  Added Tier 3 FEP Bridges E2E test: e2e_test_tier3_fep_bridges")
endif()

# ============================================================================
# Security Module Pipeline E2E Test (Phase SMB)
# ============================================================================
# WHAT: End-to-end tests for complete security module integration pipeline
# WHY:  Verify threat detection and security workflows across all module bridges
# HOW:  Test complete security scenarios spanning perception, language, executive,
#       memory, training, and rcog modules with coordinated threat response
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_security_module_pipeline.cpp)
    add_executable(e2e_test_security_module_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_security_module_pipeline.cpp
    )
    target_include_directories(e2e_test_security_module_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/perception
        ${CMAKE_SOURCE_DIR}/include/security/language
        ${CMAKE_SOURCE_DIR}/include/security/executive
        ${CMAKE_SOURCE_DIR}/include/security/memory
        ${CMAKE_SOURCE_DIR}/include/security/training
        ${CMAKE_SOURCE_DIR}/include/security/rcog
    )
    target_link_libraries(e2e_test_security_module_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_security_module_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_security_module_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_security_module_pipeline COMMAND e2e_test_security_module_pipeline)
    set_tests_properties(e2e_test_security_module_pipeline PROPERTIES
        LABELS "e2e;security;module;perception;language;executive;memory;training;rcog;pipeline"
        TIMEOUT 300
    )
    message(STATUS "  Added Security Module Pipeline E2E test: e2e_test_security_module_pipeline")
endif()

# ============================================================================
# Training Symbolic Logic Hub Pipeline E2E Test
# ============================================================================
# WHAT: Test training symbolic logic hub bridge in complete training pipeline
# WHY:  Verify logic-guided training decisions, rule learning, early stopping
# HOW:  Simulate full training loop with logic-based LR, curriculum, and constraints
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_training_symbolic_logic_pipeline.cpp)
    add_executable(e2e_test_training_symbolic_logic_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_training_symbolic_logic_pipeline.cpp
    )
    target_include_directories(e2e_test_training_symbolic_logic_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/training/integration
    )
    target_link_libraries(e2e_test_training_symbolic_logic_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_training_symbolic_logic_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_training_symbolic_logic_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_training_symbolic_logic_pipeline COMMAND e2e_test_training_symbolic_logic_pipeline)
    set_tests_properties(e2e_test_training_symbolic_logic_pipeline PROPERTIES
        LABELS "e2e;training;symbolic-logic;hub;curriculum;lr;early-stopping"
        TIMEOUT 300
    )
    message(STATUS "  Added Training Symbolic Logic Pipeline E2E test: e2e_test_training_symbolic_logic_pipeline")
endif()

# ============================================================================
# Hypothalamus Orchestrator Pipeline E2E Test
# ============================================================================
# WHAT: Test hypothalamus orchestrator with drive system integration
# WHY:  Verify complete drive coordination and event routing pipeline
# HOW:  Test drive activation, stress response, alignment safety, bridge coordination
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_hypothalamus_orchestrator_pipeline.cpp)
    add_executable(e2e_test_hypothalamus_orchestrator_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_hypothalamus_orchestrator_pipeline.cpp
    )
    target_include_directories(e2e_test_hypothalamus_orchestrator_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/core/brain/regions/hypothalamus
    )
    target_link_libraries(e2e_test_hypothalamus_orchestrator_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_hypothalamus_orchestrator_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_hypothalamus_orchestrator_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_hypothalamus_orchestrator_pipeline COMMAND e2e_test_hypothalamus_orchestrator_pipeline)
    set_tests_properties(e2e_test_hypothalamus_orchestrator_pipeline PROPERTIES
        LABELS "e2e;hypothalamus;orchestrator;drives;alignment;stress"
        TIMEOUT 300
    )
    message(STATUS "  Added Hypothalamus Orchestrator Pipeline E2E test: e2e_test_hypothalamus_orchestrator_pipeline")
endif()

# ============================================================================
# Hypothalamus FEP-Cognitive Pipeline E2E Test
# ============================================================================
# WHAT: Test hypothalamus-FEP-cognitive integration flow
# WHY:  Verify drive -> FEP computation -> cognitive effect pipeline
# HOW:  Test fatigue/curiosity/priority drives through FEP bridges to cognition
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_hypo_fep_cognitive_pipeline.cpp)
    add_executable(e2e_test_hypo_fep_cognitive_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_hypo_fep_cognitive_pipeline.cpp
    )
    target_include_directories(e2e_test_hypo_fep_cognitive_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/core/brain/regions/hypothalamus
        ${CMAKE_SOURCE_DIR}/include/core/brain/regions/hypothalamus/fep
        ${CMAKE_SOURCE_DIR}/include/cognitive/free_energy
    )
    target_link_libraries(e2e_test_hypo_fep_cognitive_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_hypo_fep_cognitive_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_hypo_fep_cognitive_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_hypo_fep_cognitive_pipeline COMMAND e2e_test_hypo_fep_cognitive_pipeline)
    set_tests_properties(e2e_test_hypo_fep_cognitive_pipeline PROPERTIES
        LABELS "e2e;hypothalamus;fep;cognitive;drives;reasoning;curiosity;workspace"
        TIMEOUT 300
    )
    message(STATUS "  Added Hypothalamus FEP-Cognitive Pipeline E2E test: e2e_test_hypo_fep_cognitive_pipeline")
endif()

# ============================================================================
# Hypothalamus Training Pipeline E2E Test
# ============================================================================
# WHAT: Test hypothalamus-training bridge integration
# WHY:  Verify drive-modulated learning, homeostatic regulation, consolidation
# HOW:  Simulate complete training scenarios through the bridge

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_hypothalamus_training_pipeline.cpp)
    add_executable(e2e_test_hypothalamus_training_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_hypothalamus_training_pipeline.cpp
    )
    target_include_directories(e2e_test_hypothalamus_training_pipeline PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/core/brain/regions/hypothalamus
    )
    target_link_libraries(e2e_test_hypothalamus_training_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_hypothalamus_training_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_hypothalamus_training_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_hypothalamus_training_pipeline COMMAND e2e_test_hypothalamus_training_pipeline)
    set_tests_properties(e2e_test_hypothalamus_training_pipeline PROPERTIES
        LABELS "e2e;hypothalamus;training;drives;homeostatic;consolidation"
        TIMEOUT 300
    )
    message(STATUS "  Added Hypothalamus Training Pipeline E2E test: e2e_test_hypothalamus_training_pipeline")
endif()

# ============================================================================
# Hypothalamus Internal Bus Pipeline E2E Test
# ============================================================================
# WHAT: Test hypothalamus internal message bus for module cross-talk
# WHY:  Verify bidirectional communication between hypothalamus modules:
#       circadian ↔ drives ↔ HPA axis ↔ homeostasis ↔ autonomic
# HOW:  Test circadian phases, stress response, sleep pressure, meal timing,
#       fight-or-flight, relaxation, multi-module interaction, modulation rules
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_hypothalamus_internal_bus_pipeline.cpp)
    add_executable(e2e_test_hypothalamus_internal_bus_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_hypothalamus_internal_bus_pipeline.cpp
    )
    target_include_directories(e2e_test_hypothalamus_internal_bus_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/core/brain/regions/hypothalamus
    )
    target_link_libraries(e2e_test_hypothalamus_internal_bus_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_hypothalamus_internal_bus_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_hypothalamus_internal_bus_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_hypothalamus_internal_bus_pipeline COMMAND e2e_test_hypothalamus_internal_bus_pipeline)
    set_tests_properties(e2e_test_hypothalamus_internal_bus_pipeline PROPERTIES
        LABELS "e2e;hypothalamus;internal-bus;circadian;stress;modulation;drives"
        TIMEOUT 300
    )
    message(STATUS "  Added Hypothalamus Internal Bus Pipeline E2E test: e2e_test_hypothalamus_internal_bus_pipeline")
endif()

# ============================================================================
# Neuromodulatory-Cognitive Integration E2E Test (Phase 4: Neuromod Expansion)
# ============================================================================
# WHAT: E2E tests for neuromodulatory-cognitive inter-layer bridges
# WHY:  Verify complete neuromod-to-behavior flow across all 6 bridges:
#       Attention, Working Memory, Emotion, Plasticity, Game Theory, Reasoning
# HOW:  Test LC/VTA/Raphe/Habenula effects on cognitive systems, full cycles
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/inter/neuromod/test_neuromod_cognitive_e2e.cpp)
    add_executable(e2e_test_neuromod_cognitive_pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/inter/neuromod/test_neuromod_cognitive_e2e.cpp
    )
    target_include_directories(e2e_test_neuromod_cognitive_pipeline PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/integration/inter
    )
    target_link_libraries(e2e_test_neuromod_cognitive_pipeline PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_neuromod_cognitive_pipeline PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_neuromod_cognitive_pipeline PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_test_neuromod_cognitive_pipeline COMMAND e2e_test_neuromod_cognitive_pipeline)
    set_tests_properties(e2e_test_neuromod_cognitive_pipeline PROPERTIES
        LABELS "e2e;neuromod;cognitive;attention;wm;emotion;plasticity;gametheory;reasoning"
        TIMEOUT 180
    )
    message(STATUS "  Added Neuromod-Cognitive Pipeline E2E test: e2e_test_neuromod_cognitive_pipeline")
endif()

# ============================================================================
# Hippocampus Memory Circuit E2E Test
# ============================================================================
# WHAT: End-to-end tests for hippocampus episodic memory system
# WHY:  Verify complete memory workflows: encoding → consolidation → retrieval
# HOW:  Test spatial navigation, pattern separation/completion, replay, bridges
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/core/brain/regions/hippocampus/test_hippocampus_e2e.cpp)
    add_executable(e2e_core_brain_regions_hippocampus_test_hippocampus_e2e
        ${CMAKE_CURRENT_SOURCE_DIR}/core/brain/regions/hippocampus/test_hippocampus_e2e.cpp
    )
    target_include_directories(e2e_core_brain_regions_hippocampus_test_hippocampus_e2e PRIVATE
        ${E2E_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include/core/brain/regions/hippocampus
    )
    target_link_libraries(e2e_core_brain_regions_hippocampus_test_hippocampus_e2e PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_core_brain_regions_hippocampus_test_hippocampus_e2e PRIVATE cxx_std_17)
    target_compile_definitions(e2e_core_brain_regions_hippocampus_test_hippocampus_e2e PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_core_brain_regions_hippocampus_test_hippocampus_e2e COMMAND e2e_core_brain_regions_hippocampus_test_hippocampus_e2e)
    set_tests_properties(e2e_core_brain_regions_hippocampus_test_hippocampus_e2e PROPERTIES
        LABELS "e2e;hippocampus;memory;episodic;spatial;consolidation"
        TIMEOUT 300
    )
    message(STATUS "  Added Hippocampus Memory Circuit E2E test: e2e_core_brain_regions_hippocampus_test_hippocampus_e2e")
endif()

# Sensory Processing Pipeline E2E Test (Phase 6: Somatosensory, Olfactory, Gustatory)
add_e2e_test(e2e_test_sensory_processing_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_sensory_processing_pipeline.cpp
)

# Motor Cortex E2E Test - End-to-end motor planning and execution scenarios
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/core/brain/regions/test_motor_e2e.cpp)
    add_executable(e2e_core_brain_regions_motor_test_motor_e2e
        ${CMAKE_CURRENT_SOURCE_DIR}/core/brain/regions/test_motor_e2e.cpp
    )
    target_include_directories(e2e_core_brain_regions_motor_test_motor_e2e PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(e2e_core_brain_regions_motor_test_motor_e2e PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_core_brain_regions_motor_test_motor_e2e PRIVATE cxx_std_17)
    target_compile_definitions(e2e_core_brain_regions_motor_test_motor_e2e PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
    add_test(NAME e2e_core_brain_regions_motor_test_motor_e2e COMMAND e2e_core_brain_regions_motor_test_motor_e2e)
    set_tests_properties(e2e_core_brain_regions_motor_test_motor_e2e PROPERTIES
        LABELS "e2e;motor;movement;planning;execution"
        TIMEOUT 300
    )
    message(STATUS "  Added Motor Cortex E2E test: e2e_core_brain_regions_motor_test_motor_e2e")
endif()

# ============================================================================
# Speech Enhancement Pipeline E2E Test (Phase B4)
# ============================================================================
# WHAT: E2E tests for speech production and comprehension enhancements
# WHY:  Verify complete speech workflows with pragmatics, discourse, prosody
# HOW:  Test conversational scenarios with all enhancement modules
# ============================================================================
add_e2e_test(e2e_test_speech_enhancement_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_speech_enhancement_pipeline.cpp
)

# ============================================================================
# Lip Reading Pipeline E2E Test (Phase B5)
# ============================================================================
# WHAT: E2E tests for audiovisual lip reading speech processing
# WHY:  Verify complete lip reading workflows with visual speech perception
# HOW:  Test visual-only, audiovisual fusion, McGurk effect, speaker adaptation
# ============================================================================
add_e2e_test(e2e_test_lip_reading_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_lip_reading_pipeline.cpp
)

# ============================================================================
# WHAT: E2E tests for Wernicke's area language comprehension
# WHY:  Verify complete language comprehension pipeline from phonemes to meaning
# HOW:  Test phonological -> lexical -> semantic -> syntactic flow
# ============================================================================
add_e2e_test(e2e_test_wernicke_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_wernicke_pipeline.cpp
)

# ============================================================================
# Thread Safety Fixes E2E Test (2026-01-15)
# ============================================================================
# WHAT: E2E tests validating critical thread safety fixes
# WHY:  Thread safety issues cause race conditions, crashes; must verify fixes
# HOW:  Test nimcp_rand, emotional system, capability checks, rate limiting
#       under realistic concurrent load scenarios
#
# TEST SCENARIOS:
# - Brain init with nimcp_rand for neural weight initialization
# - Emotional system updates during concurrent brain processing
# - Capability checks under multi-threaded access
# - Rate-limited logging under high concurrent load
# - Python binding lifecycle memory safety (mock)
# - Combined stress test with all subsystems active
# ============================================================================
add_e2e_test(e2e_test_thread_safety_fixes
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_thread_safety_fixes.cpp
)
set_tests_properties(e2e_test_thread_safety_fixes PROPERTIES
    LABELS "e2e;thread;safety;concurrent"
    TIMEOUT 300
)

# ============================================================================
# KG Hierarchy Graph Algorithms Pipeline E2E Test
# ============================================================================
# WHAT: E2E tests for KG hierarchy graph algorithms
# WHY:  Verify complete graph algorithm workflows in realistic brain topology
# HOW:  Test topological sort, binary search, BFS/DFS, connected components,
#       shortest path with full brain integration
#
# TEST SCENARIOS:
# - Topological sort for module startup ordering
# - Binary search for optimized module lookups
# - BFS/DFS traversal across brain hierarchy
# - Connected components detection
# - Shortest path finding between modules
# - Concurrent access safety
# - Performance benchmarks
# ============================================================================
add_e2e_test(e2e_test_kg_hierarchy_graph_algorithms_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_kg_hierarchy_graph_algorithms_pipeline.cpp
)
set_tests_properties(e2e_test_kg_hierarchy_graph_algorithms_pipeline PROPERTIES
    LABELS "e2e;core;brain;kg;graph;algorithms"
    TIMEOUT 180
)

# Include GPU E2E test subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gpu/CMakeLists.txt)
    add_subdirectory(gpu)
endif()

# Include Ternary E2E test subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ternary/CMakeLists.txt)
    add_subdirectory(ternary)
endif()

# Include Chemistry Layer E2E test subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/chemistry/CMakeLists.txt)
    message(STATUS "Adding Chemistry Layer E2E tests...")
    add_subdirectory(chemistry)
endif()

# Include Biology Layer E2E test subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/biology/CMakeLists.txt)
    message(STATUS "Adding Biology Layer E2E tests...")
    add_subdirectory(biology)
endif()

# Health Agent USE Functions Pipeline E2E Test (Portia/Dragonfly/Swarm Integration)
add_e2e_test(e2e_test_health_agent_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_health_agent_pipeline.cpp
)
set_tests_properties(e2e_test_health_agent_pipeline PROPERTIES TIMEOUT 300)

# Neuro-Symbolic Mathematics Pipeline E2E Test (Mathesis-inspired)
add_e2e_test(e2e_test_neuro_symbolic_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_neuro_symbolic_pipeline.cpp
)
set_tests_properties(e2e_test_neuro_symbolic_pipeline PROPERTIES TIMEOUT 180)

# Include Plasticity E2E test subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/plasticity/CMakeLists.txt)
    message(STATUS "Adding Plasticity E2E tests...")
    add_subdirectory(plasticity)
endif()

# Include Brain E2E test subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/brain/CMakeLists.txt)
    message(STATUS "Adding Brain KG E2E tests...")
    add_subdirectory(brain)
endif()

# Include Physics E2E test subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/physics/CMakeLists.txt)
    message(STATUS "Adding Physics Modules E2E tests...")
    add_subdirectory(physics)
endif()

# Include Training E2E test subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/training/CMakeLists.txt)
    message(STATUS "Adding Training Validation E2E tests...")
    add_subdirectory(training)
endif()

# Include LGSS E2E test subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lgss/CMakeLists.txt)
    message(STATUS "Adding LGSS (Layered Governance Safety System) E2E tests...")
    add_subdirectory(lgss)
endif()

# Include Fault Tolerance E2E test subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fault_tolerance/CMakeLists.txt)
    message(STATUS "Adding Fault Tolerance E2E tests...")
    add_subdirectory(fault_tolerance)
endif()

# Include Cognitive E2E test subdirectory (world model + neuro-symbolic)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cognitive/CMakeLists.txt)
    message(STATUS "Adding Cognitive E2E tests (Mathesis genius mathematics)...")
    add_subdirectory(cognitive)
endif()

# Include Core E2E test subdirectory (neural substrate)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/core/CMakeLists.txt)
    message(STATUS "Adding Core E2E tests (Neural Substrate layer)...")
    add_subdirectory(core)
endif()

# Include SNN E2E test subdirectory (bridge exception handling)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/snn/CMakeLists.txt)
    message(STATUS "Adding SNN E2E tests (Bridge exception handling)...")
    add_subdirectory(snn)
endif()

# Include Statistics E2E test subdirectory (Shannon integration)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/statistics/CMakeLists.txt)
    message(STATUS "Adding Statistics E2E tests (Shannon integration)...")
    add_subdirectory(statistics)
endif()

# Include Mesh Network E2E test subdirectory (Hyperledger-inspired architecture)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mesh/CMakeLists.txt)
    message(STATUS "Adding Mesh Network E2E tests...")
    add_subdirectory(mesh)
endif()

# Include Security E2E test subdirectory (AI Safety Enhancement)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/security/CMakeLists.txt)
    message(STATUS "Adding Security Safety Enhancement E2E tests...")
    add_subdirectory(security)
endif()

# Include Brain Regions E2E test subdirectory (KG Wiring)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/brain_regions/CMakeLists.txt)
    message(STATUS "Adding Brain Regions KG Wiring E2E tests...")
    add_subdirectory(brain_regions)
endif()

# Include Genius Profiles E2E test subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/genius/CMakeLists.txt)
    message(STATUS "Adding Genius Profiles E2E tests...")
    add_subdirectory(genius)
endif()

# Include Middleware E2E test subdirectory (Circular Buffer)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/middleware/CMakeLists.txt)
    message(STATUS "Adding Middleware E2E tests...")
    add_subdirectory(middleware)
endif()

# SRP Split E2E Tests
add_e2e_test(e2e_test_srp_split
    ${CMAKE_CURRENT_SOURCE_DIR}/srp_split/test_srp_split_e2e.cpp
)

# =============================================================================
# Query-to-Answer E2E Test (Reasoning + Language Generation Pipeline)
# =============================================================================
add_e2e_test(e2e_test_query_to_answer
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_query_to_answer_e2e.cpp
)

# =============================================================================
# Symbolic Reasoning E2E Test (Neuro-Symbolic Pipeline)
# =============================================================================
add_e2e_test(e2e_test_symbolic_reasoning
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_symbolic_reasoning_e2e.cpp
)

# =============================================================================
# Portia-Reasoning Bridge E2E Test (Budget Pipeline)
# =============================================================================
add_e2e_test(e2e_test_portia_reasoning_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_portia_bridge_e2e.cpp
)

# =============================================================================
# Hypothalamus-Reasoning Bridge E2E Test (Motivational Modulation Pipeline)
# =============================================================================
add_e2e_test(e2e_test_hypo_reasoning_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_hypo_bridge_e2e.cpp
)

# =============================================================================
# Mesh-Reasoning Bridge E2E Test (Distributed Evidence Pipeline)
# =============================================================================
add_e2e_test(e2e_test_mesh_reasoning_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_mesh_bridge_e2e.cpp
)

# =============================================================================
# Training Integration E2E Test (Hub + Brain Training Pipeline)
# =============================================================================
# WHAT: E2E tests for training integration hub and brain training context
# WHY:  Verify complete training pipelines: event routing, loss/optimizer
#       lifecycle, gradient management, LR scheduling, early stopping
# HOW:  Test simulated training loops, multi-module event delivery,
#       gradient accumulation, dropout behavior, full cleanup
# =============================================================================
add_e2e_test(e2e_test_training_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_training_integration_e2e.cpp
)

# =============================================================================
# Athena Training Pipeline E2E Test (Full lifecycle, loss reduction, gradient
# clipping, consolidation mid-training, multi-domain, state persistence)
# =============================================================================
add_e2e_test(e2e_test_athena_training_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_athena_training_pipeline.cpp
)
# Override: 6 tests each with brain create/destroy (~10s each) need more time
set_tests_properties(e2e_test_athena_training_pipeline PROPERTIES
    TIMEOUT 600
    RESOURCE_LOCK "brain_heavy"
)

# Edge Brain E2E tests
add_e2e_test(e2e_test_edge_brain
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/edge/test_edge_e2e.cpp
)

# Memory Pipeline E2E tests (engram + semantic + autobio integration)
add_e2e_test(e2e_test_memory_pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/edge/test_memory_e2e.cpp
)

# Memory Pipeline Integration tests (enhancement verification)
add_e2e_test(e2e_test_memory_pipeline_integration
    ${CMAKE_SOURCE_DIR}/test/integration/edge/test_memory_pipeline_integration.cpp
)

# Memory Regression tests (edge cases for memory enhancements)
add_e2e_test(e2e_test_memory_regression
    ${CMAKE_SOURCE_DIR}/test/regression/edge/test_memory_regression.cpp
)

# Full Memory E2E tests (store + OODB + OOD + metadata)
add_e2e_test(e2e_test_full_memory
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/test_full_memory_e2e.cpp
)

# Include Utils E2E test subdirectory (Memory + Health Agent)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/utils/CMakeLists.txt)
    message(STATUS "Adding Utils E2E tests...")
    add_subdirectory(utils)
endif()

# ---------------------------------------------------------------------------
# E2E: Full Brain Lifecycle (C + libcheck)
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_full_brain_lifecycle_e2e.c)
    add_executable(e2e_test_full_brain_lifecycle
        test_full_brain_lifecycle_e2e.c
    )
    target_include_directories(e2e_test_full_brain_lifecycle PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(e2e_test_full_brain_lifecycle PRIVATE c_std_11)
    target_link_libraries(e2e_test_full_brain_lifecycle PRIVATE
        nimcp_middleware nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME e2e_test_full_brain_lifecycle
             COMMAND e2e_test_full_brain_lifecycle
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(e2e_test_full_brain_lifecycle
        PROPERTIES
            LABELS "e2e;brain;lifecycle;subsystems"
            TIMEOUT 600
            RESOURCE_LOCK "brain_heavy")
endif()

# ---------------------------------------------------------------------------
# E2E: SNN Per-Receptor Smoke @ 1.8M neurons (C + libcheck)
# ---------------------------------------------------------------------------
# WHAT: Boots a full ~1.8M-neuron SNN brain with conductance-based PSCs and the
#       per-receptor (g_ampa / g_nmda / g_gaba_a / g_gaba_b) split, runs 100
#       forward steps under deterministic stim, and pins smoke-level invariants
#       (no NaN/Inf, no dead-pop, no runaway, bounded g_*).
# WHY:  Pod-scale regressions in the per-receptor deposit/decay kernels only
#       surface at production wiring density. Unit tests pin shape; this pins
#       behavior. Slow by construction — gate via NIMCP_E2E_QUICK to skip.
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_per_receptor_e2e.c)
    add_executable(e2e_test_snn_per_receptor
        test_snn_per_receptor_e2e.c
    )
    target_include_directories(e2e_test_snn_per_receptor PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(e2e_test_snn_per_receptor PRIVATE c_std_11)
    target_link_libraries(e2e_test_snn_per_receptor PRIVATE
        nimcp_middleware nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME e2e_test_snn_per_receptor
             COMMAND e2e_test_snn_per_receptor
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(e2e_test_snn_per_receptor
        PROPERTIES
            LABELS "e2e;snn;cb;per_receptor;heavy;slow"
            TIMEOUT 1800   # 30 min — full 1.8M brain init + 100 steps
            RESOURCE_LOCK "brain_heavy")
endif()

# ---------------------------------------------------------------------------
# E2E: Language pops (broca/wernicke/arcuate/sensorymotor) + adapter binding
# ---------------------------------------------------------------------------
# WHAT: Boots a small brain via brain_create_custom(), runs a few learn ticks
#       to trigger the lazy SNN init + init_language_pops, then asserts the
#       four substrate pops exist and broca/wernicke adapters are bound.
# WHY:  End-to-end smoke test for the whole language-substrate wiring chain.
#       Catches regressions where any link between adapter creation and
#       attach_snn_pop wiring silently drifts.
# HOW:  C99 + nimcp public API. No libcheck framework — uses fail/pass
#       sentinels + return codes (matches test_snn_per_receptor_e2e.c).
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_lang_pops_e2e.c)
    add_executable(e2e_test_lang_pops
        test_lang_pops_e2e.c
    )
    target_include_directories(e2e_test_lang_pops PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(e2e_test_lang_pops PRIVATE c_std_11)
    target_link_libraries(e2e_test_lang_pops PRIVATE
        nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME e2e_test_lang_pops
             COMMAND e2e_test_lang_pops
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(e2e_test_lang_pops
        PROPERTIES
            LABELS "e2e;snn;language;adapter"
            TIMEOUT 600
            RESOURCE_LOCK "brain_heavy"
            # DISABLED until brain_create_custom + multi_network_training are
            # fast enough for an e2e smoke. Current configuration triggers a
            # full SNN setup with ~60M synapses + repeated CSR prep; runtime
            # exceeds 6 minutes and consumes >13 GB RAM. The language wiring
            # under test is already covered unit-side by
            # unit_snn_test_lang_attach_unit + integration_snn_test_lang_attach_integration
            # and regression_snn_test_lang_synfire_ring_regression.
            DISABLED TRUE)
endif()

# ---------------------------------------------------------------------------
# E2E: SNN Layer-Specific Pyramidal Subtypes (P4.1)
# ---------------------------------------------------------------------------
# WHAT: Boots a full ~1.8M-neuron SNN brain via the production
#       brain_create_custom() path (same path as the daemon) and verifies
#       that the layer-specific subclass distribution (PYRAMIDAL_L23 /
#       PYRAMIDAL_L4_STELLATE / PYRAMIDAL_L5_BETZ) is preserved end-to-end.
# WHY:  Hierarchical-builder unit/integration tests pin the construction
#       primitives in isolation. This pins the contract through the full
#       brain build path — a future refactor that reconstructs SNN pops
#       post-hierarchy-build could silently drop the subclass tags.
# HOW:  C + libcheck (matches test_snn_per_receptor_e2e.c). C++ would trip
#       a cublas redeclaration warning when src/ headers transitively pull
#       cuda — same reason the per-receptor e2e is C, not C++.
# RUNTIME: 5-15 min. Gated by NIMCP_E2E_QUICK to keep `make test` fast.
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_layer_subtypes_e2e.c)
    add_executable(e2e_test_snn_layer_subtypes
        test_snn_layer_subtypes_e2e.c
    )
    target_include_directories(e2e_test_snn_layer_subtypes PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(e2e_test_snn_layer_subtypes PRIVATE c_std_11)
    target_link_libraries(e2e_test_snn_layer_subtypes PRIVATE
        nimcp_middleware nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME e2e_test_snn_layer_subtypes
             COMMAND e2e_test_snn_layer_subtypes
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(e2e_test_snn_layer_subtypes
        PROPERTIES
            LABELS "e2e;snn;layer_subtypes;p4.1;heavy;slow"
            TIMEOUT 1800
            RESOURCE_LOCK "brain_heavy")
endif()

# ---------------------------------------------------------------------------
# E2E: SNN Wave H Dendritic Compartments (full hierarchical brain)
# ---------------------------------------------------------------------------
# WHAT: Boots a full ~1.8M-neuron SNN brain with dendritic_enabled = 1 on
#       tier-pyramidal pops, runs 50 forward steps + 200 ms quiet taper,
#       and pins:
#         - no NaN/Inf in membrane_v / v_basal / v_apical
#         - all dendritic-tagged pops are tier-pyr (interneurons stay
#           single-compartment)
#         - no infinite plateau: plateau_active count returns to 0 within
#           200 ms of input withdrawal
# WHY:  Pod-scale regressions in the two-compartment integration only
#       surface at production wiring density. Skip via NIMCP_E2E_QUICK.
# HOW:  GoogleTest. C++ links nimcp_middleware (header bundle for brain_t
#       / brain_create_custom). Skip-aware SetUp.
# RUNTIME: 5-15 min. Gated by NIMCP_E2E_QUICK to keep `make test` fast.
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_dendritic_e2e.c)
    add_executable(e2e_test_snn_dendritic
        test_snn_dendritic_e2e.c
    )
    target_include_directories(e2e_test_snn_dendritic PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(e2e_test_snn_dendritic PRIVATE c_std_11)
    target_link_libraries(e2e_test_snn_dendritic PRIVATE
        nimcp_middleware nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME e2e_test_snn_dendritic
             COMMAND e2e_test_snn_dendritic
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(e2e_test_snn_dendritic
        PROPERTIES
            LABELS "e2e;snn;dendritic;wave_h;heavy;slow"
            TIMEOUT 1800
            RESOURCE_LOCK "brain_heavy")
endif()

# ---------------------------------------------------------------------------
# E2E: Sleep-Wake Cycle (C + libcheck)
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_sleep_wake_cycle_e2e.c)
    add_executable(e2e_test_sleep_wake_cycle
        test_sleep_wake_cycle_e2e.c
    )
    target_include_directories(e2e_test_sleep_wake_cycle PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(e2e_test_sleep_wake_cycle PRIVATE c_std_11)
    target_link_libraries(e2e_test_sleep_wake_cycle PRIVATE
        nimcp_middleware nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME e2e_test_sleep_wake_cycle
             COMMAND e2e_test_sleep_wake_cycle
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(e2e_test_sleep_wake_cycle
        PROPERTIES
            LABELS "e2e;brain;sleep_wake;glymphatic;neuropeptide"
            TIMEOUT 120)
endif()

# ---------------------------------------------------------------------------
# E2E: Parallel Init Lifecycle (C + libcheck)
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_parallel_init_lifecycle_e2e.c)
    add_executable(e2e_test_parallel_init_lifecycle
        test_parallel_init_lifecycle_e2e.c
    )
    target_include_directories(e2e_test_parallel_init_lifecycle PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(e2e_test_parallel_init_lifecycle PRIVATE c_std_11)
    target_link_libraries(e2e_test_parallel_init_lifecycle PRIVATE
        nimcp_middleware nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME e2e_test_parallel_init_lifecycle
             COMMAND e2e_test_parallel_init_lifecycle
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(e2e_test_parallel_init_lifecycle
        PROPERTIES
            LABELS "e2e;brain;parallel_init;lifecycle"
            TIMEOUT 900
            RESOURCE_LOCK "brain_heavy")
endif()

# ---------------------------------------------------------------------------
# E2E: Cycle Coordinator Driven Lifecycle (plain C, no GTest/libcheck)
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_cycle_coordinator_e2e.c)
    add_executable(e2e_test_cycle_coordinator
        test_cycle_coordinator_e2e.c
    )
    target_include_directories(e2e_test_cycle_coordinator PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(e2e_test_cycle_coordinator PRIVATE c_std_11)
    target_link_libraries(e2e_test_cycle_coordinator PRIVATE
        nimcp Threads::Threads rt m
    )
    add_test(NAME e2e_test_cycle_coordinator
             COMMAND e2e_test_cycle_coordinator
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(e2e_test_cycle_coordinator
        PROPERTIES
            LABELS "e2e;cycle_coordinator;driven"
            TIMEOUT 30)
endif()

# ---------------------------------------------------------------------------
# E2E: Wave 4 Multi-Cycle Soak (PREDICTIVE_IMMUNE + EPI + NVC + NEURO)
# ---------------------------------------------------------------------------
# WHAT: Four concurrent driven cycles (including the new BRAIN_CYCLE_PREDICTIVE_IMMUNE
#       enum added in Wave 4) running for 2s under real pthreads.
# WHY:  Wave 4 extended brain_cycle_type_t and added live driver threads.
#       This soak is the last line of defense against enum collisions, thread
#       leaks, and mutual-starvation bugs that only surface under sustained load.
# HOW:  Plain C, links libnimcp only (no GTest — owns its own main).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_wave4_e2e.c)
    add_executable(e2e_test_wave4
        test_wave4_e2e.c
    )
    target_include_directories(e2e_test_wave4 PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(e2e_test_wave4 PRIVATE c_std_11)
    target_link_libraries(e2e_test_wave4 PRIVATE
        nimcp Threads::Threads rt m
    )
    add_test(NAME e2e_test_wave4
             COMMAND e2e_test_wave4
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(e2e_test_wave4
        PROPERTIES
            LABELS "e2e;cycle_coordinator;driven;wave4"
            TIMEOUT 30)
endif()

# ---------------------------------------------------------------------------
# Wave 5 E2E — 20-iteration soak of submit_post_forward with the new physics
# stage (intuitive_physics_step wired as stage_physics_task).
# Mirrors the lightweight wave4 E2E pattern: plain C, own main(), libnimcp only.
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_wave5_e2e.c)
    add_executable(e2e_test_wave5
        test_wave5_e2e.c
    )
    target_include_directories(e2e_test_wave5 PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(e2e_test_wave5 PRIVATE c_std_11)
    target_link_libraries(e2e_test_wave5 PRIVATE
        nimcp Threads::Threads rt m
    )
    add_test(NAME e2e_test_wave5
             COMMAND e2e_test_wave5
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(e2e_test_wave5
        PROPERTIES
            LABELS "e2e;parallel_stages;physics;wave5"
            TIMEOUT 15)
endif()

# ---------------------------------------------------------------------------
# E2E: Wave 6 Multi-Cycle Soak (CHEMISTRY + PI + EPI + NVC + NEURO)
# ---------------------------------------------------------------------------
# WHAT: Five concurrent driven cycles including the new BRAIN_CYCLE_CHEMISTRY
#       (10ms) added in Wave 6, running for 2s under real pthreads.
# WHY:  Wave 6 introduced the first FAST-category driven cycle (10ms) running
#       alongside medium/slow cycles. This soak is the last line of defense
#       against enum collisions, thread leaks, and cadence failures under
#       sustained 5-cycle load.
# HOW:  Plain C, links libnimcp only (no GTest — owns its own main).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_wave6_e2e.c)
    add_executable(e2e_test_wave6
        test_wave6_e2e.c
    )
    target_include_directories(e2e_test_wave6 PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(e2e_test_wave6 PRIVATE c_std_11)
    target_link_libraries(e2e_test_wave6 PRIVATE
        nimcp Threads::Threads rt m
    )
    add_test(NAME e2e_test_wave6
             COMMAND e2e_test_wave6
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(e2e_test_wave6
        PROPERTIES
            LABELS "e2e;cycle_coordinator;driven;chemistry;wave6"
            TIMEOUT 30)
endif()

# ============================================================================
# Wave 8A E2E: 6-cycle soak including BRAIN_CYCLE_COCHLEA_BRIDGES
# ============================================================================
# WHAT: Runs COCHLEA_BRIDGES (10ms) + CHEMISTRY (10ms) + EPI/NVC/PI (100ms)
#       + NEUROGENESIS (1s) as six concurrent driven cycles on one
#       coordinator for 2 seconds.
# WHY:  Cross-cycle stability + thread-leak probe at the Wave 8A boundary.
# HOW:  Plain C, links libnimcp only (no GTest — owns its own main).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_wave8a_e2e.c)
    add_executable(e2e_test_wave8a
        test_wave8a_e2e.c
    )
    target_include_directories(e2e_test_wave8a PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(e2e_test_wave8a PRIVATE c_std_11)
    target_link_libraries(e2e_test_wave8a PRIVATE
        nimcp Threads::Threads rt m
    )
    add_test(NAME e2e_test_wave8a
             COMMAND e2e_test_wave8a
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(e2e_test_wave8a
        PROPERTIES
            LABELS "e2e;cycle_coordinator;driven;cochlea;wave8a"
            TIMEOUT 30)
endif()

# ============================================================================
# Grounded Language E2E Tests (subdirectory)
# ============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/language")
    add_subdirectory(language)
endif()

message(STATUS "")
message(STATUS "E2E Test Execution:")
message(STATUS "  All E2E tests:   ctest -L e2e")
message(STATUS "  Verbose:         ctest -L e2e -V")
message(STATUS "  Single test:     ./e2e_test_brain_pipeline")
message(STATUS "")
message(STATUS "===============================================================================")
message(STATUS "")
