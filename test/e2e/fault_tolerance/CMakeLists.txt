# E2E Fault Tolerance Tests
# Health Agent Connection E2E Tests (Phase 2)

# Helper macro to add e2e test
macro(add_e2e_fault_tolerance_test TEST_NAME)
    add_executable(e2e_fault_tolerance_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(e2e_fault_tolerance_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            pthread
    )
    target_include_directories(e2e_fault_tolerance_${TEST_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME e2e_fault_tolerance_${TEST_NAME}
             COMMAND e2e_fault_tolerance_${TEST_NAME})
    set_tests_properties(e2e_fault_tolerance_${TEST_NAME} PROPERTIES LABELS "e2e")
endmacro()

# Health Agent Connection E2E Tests (Phase 2)
add_e2e_fault_tolerance_test(test_health_agent_connection_e2e)
set_tests_properties(e2e_fault_tolerance_test_health_agent_connection_e2e PROPERTIES TIMEOUT 300)

# Health Agent Consistency E2E Tests (Phase 3)
add_e2e_fault_tolerance_test(test_health_agent_consistency_e2e)
set_tests_properties(e2e_fault_tolerance_test_health_agent_consistency_e2e PROPERTIES TIMEOUT 300)

# Health Agent Active Cognitive Integration E2E Tests (Phase 5)
add_e2e_fault_tolerance_test(test_health_agent_active_cognitive_e2e)
set_tests_properties(e2e_fault_tolerance_test_health_agent_active_cognitive_e2e PROPERTIES TIMEOUT 600)

# Health Agent Neural Integration E2E Tests (Phase 5.5: SNN/LNN)
add_e2e_fault_tolerance_test(test_health_agent_neural_e2e)
set_tests_properties(e2e_fault_tolerance_test_health_agent_neural_e2e PROPERTIES TIMEOUT 300)

# Health Agent Behavioral Integration E2E Tests (Phase 5.6: Dragonfly/Portia)
add_e2e_fault_tolerance_test(test_health_agent_behavioral_e2e)
set_tests_properties(e2e_fault_tolerance_test_health_agent_behavioral_e2e PROPERTIES TIMEOUT 300)

# Health Agent Memory System E2E Tests (Phase 5.7: Hippocampus/Mammillary)
add_e2e_fault_tolerance_test(test_health_agent_memory_e2e)
set_tests_properties(e2e_fault_tolerance_test_health_agent_memory_e2e PROPERTIES TIMEOUT 300)

# Capacity Manager E2E Tests (Phase 5.8: Dynamic Capacity Management)
add_e2e_fault_tolerance_test(test_capacity_manager_e2e)
set_tests_properties(e2e_fault_tolerance_test_capacity_manager_e2e PROPERTIES TIMEOUT 300)

# Symbolic Logic Health E2E Tests (Phase 5.9: Symbolic Logic Health Integration)
add_e2e_fault_tolerance_test(test_symbolic_logic_health_e2e)
set_tests_properties(e2e_fault_tolerance_test_symbolic_logic_health_e2e PROPERTIES TIMEOUT 300)

# Neural Substrate Health E2E Tests (Phase 5.10: Neural Substrate Health Integration)
add_e2e_fault_tolerance_test(test_substrate_health_e2e)
set_tests_properties(e2e_fault_tolerance_test_substrate_health_e2e PROPERTIES TIMEOUT 300)

# Thalamic/Middleware Health E2E Tests (Phase 5.11: Thalamic/Middleware Health Integration)
add_e2e_fault_tolerance_test(test_thalamic_middleware_health_e2e)
set_tests_properties(e2e_fault_tolerance_test_thalamic_middleware_health_e2e PROPERTIES TIMEOUT 300)

# Perception/Cortical Health E2E Tests (Phase 5.12: Perception/Cortical Health Integration)
add_e2e_fault_tolerance_test(test_perception_cortical_health_e2e)
set_tests_properties(e2e_fault_tolerance_test_perception_cortical_health_e2e PROPERTIES TIMEOUT 300)

# Brain Probe Health E2E Tests (Phase 5.13: Brain Probes Enhancement)
add_e2e_fault_tolerance_test(test_brain_probe_health_e2e)
set_tests_properties(e2e_fault_tolerance_test_brain_probe_health_e2e PROPERTIES TIMEOUT 300)

# World Model & Imagination Health E2E Tests (Phase 5.14: World Model & Imagination Health Integration)
add_e2e_fault_tolerance_test(test_world_imagination_health_e2e)
set_tests_properties(e2e_fault_tolerance_test_world_imagination_health_e2e PROPERTIES TIMEOUT 300)

# Crash Recovery E2E Tests (Phase 6: End-to-End Testing)
add_e2e_fault_tolerance_test(test_crash_recovery)
set_tests_properties(e2e_fault_tolerance_test_crash_recovery PROPERTIES TIMEOUT 300)

# Resilience Stress E2E Tests (Phase 6: End-to-End Testing)
add_e2e_fault_tolerance_test(test_resilience_stress)
set_tests_properties(e2e_fault_tolerance_test_resilience_stress PROPERTIES TIMEOUT 600)

# Exception Resilience E2E Tests (Phase 6: Exception System Stress Testing)
add_e2e_fault_tolerance_test(test_exception_resilience_e2e)
set_tests_properties(e2e_fault_tolerance_test_exception_resilience_e2e PROPERTIES TIMEOUT 600)

# Exception Brain Integration E2E Tests (Phase 6: Exception-Brain Integration)
add_e2e_fault_tolerance_test(test_exception_brain_integration_e2e)
set_tests_properties(e2e_fault_tolerance_test_exception_brain_integration_e2e PROPERTIES TIMEOUT 300)

# Exception Flow E2E Tests (Complete Exception Handling Pipeline)
add_e2e_fault_tolerance_test(test_exception_flow_e2e)
set_tests_properties(e2e_fault_tolerance_test_exception_flow_e2e PROPERTIES TIMEOUT 300)

# Heartbeat Module E2E Tests (Phase 8: Heartbeat for Long Operations)
add_e2e_fault_tolerance_test(test_heartbeat_module_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_module_e2e PROPERTIES TIMEOUT 300)

# State Checkpoint Recovery E2E Tests (Phase 8: System-Wide Health Integration)
add_e2e_fault_tolerance_test(test_state_checkpoint_recovery_e2e)
set_tests_properties(e2e_fault_tolerance_test_state_checkpoint_recovery_e2e PROPERTIES TIMEOUT 300)

# Self-Repair Bio-Async E2E Tests (Phase 8: Self-Repair Pipeline Bio-Async Integration)
add_e2e_fault_tolerance_test(test_self_repair_bio_async_e2e)
set_tests_properties(e2e_fault_tolerance_test_self_repair_bio_async_e2e PROPERTIES TIMEOUT 300)

# Health Self-Repair E2E Tests (Health Monitoring Self-Repair Integration)
add_e2e_fault_tolerance_test(test_health_self_repair_e2e)
set_tests_properties(e2e_fault_tolerance_test_health_self_repair_e2e PROPERTIES TIMEOUT 300)

# Plasticity Health E2E Tests (Plasticity Health Monitoring Pipeline)
add_e2e_fault_tolerance_test(test_plasticity_health_e2e)
set_tests_properties(e2e_fault_tolerance_test_plasticity_health_e2e PROPERTIES TIMEOUT 300)

# Exception Recovery E2E Tests (Complete Exception-to-Recovery Flow)
add_e2e_fault_tolerance_test(test_exception_recovery_e2e)
set_tests_properties(e2e_fault_tolerance_test_exception_recovery_e2e PROPERTIES TIMEOUT 600)

# Exception Full Pipeline E2E Tests - DISABLED (API issues)
# Tests moved to existing exception test suite

# Exception Immune Response E2E Tests - DISABLED (API issues)
# Tests moved to existing exception test suite

# Exception Macro E2E Tests (Try/Catch Macro System)
add_e2e_fault_tolerance_test(test_exception_macro_e2e)
set_tests_properties(e2e_fault_tolerance_test_exception_macro_e2e PROPERTIES TIMEOUT 300)

# Exception Immune E2E Tests (Exception-Immune System Integration)
add_e2e_fault_tolerance_test(test_exception_immune_e2e)
set_tests_properties(e2e_fault_tolerance_test_exception_immune_e2e PROPERTIES TIMEOUT 600)

# Exception Complete Flow E2E Tests (Complete Exception Lifecycle from API to Brain)
add_e2e_fault_tolerance_test(test_exception_complete_flow_e2e)
set_tests_properties(e2e_fault_tolerance_test_exception_complete_flow_e2e PROPERTIES TIMEOUT 600)

# Exception Recovery Scenarios E2E Tests (Multi-Exception Recovery, Circuit Breaker, Retry with Backoff)
add_e2e_fault_tolerance_test(test_exception_recovery_scenarios_e2e)
set_tests_properties(e2e_fault_tolerance_test_exception_recovery_scenarios_e2e PROPERTIES TIMEOUT 600)

# Exception Stress E2E Tests (High Rate Exception Handling, Storm Prevention, Concurrent Threading)
add_e2e_fault_tolerance_test(test_exception_stress_e2e)
set_tests_properties(e2e_fault_tolerance_test_exception_stress_e2e PROPERTIES TIMEOUT 900)

# Exception Module E2E Tests (Complete Exception Handling Pipelines for Converted Modules)
add_e2e_fault_tolerance_test(test_exception_module_e2e)
set_tests_properties(e2e_fault_tolerance_test_exception_module_e2e PROPERTIES TIMEOUT 900)

# Exception Comprehensive E2E Tests (Complete Exception Pipeline Coverage)
add_e2e_fault_tolerance_test(test_exception_e2e_comprehensive)
set_tests_properties(e2e_fault_tolerance_test_exception_e2e_comprehensive PROPERTIES TIMEOUT 900)

# Brain State Manager E2E Tests (Phase 8: System-Wide Health Integration)
add_e2e_fault_tolerance_test(test_brain_state_manager_e2e)
set_tests_properties(e2e_fault_tolerance_test_brain_state_manager_e2e PROPERTIES TIMEOUT 600)

# Fault Tolerance Exception Handling E2E Tests (NIMCP_THROW_TO_IMMUNE)
add_e2e_fault_tolerance_test(test_fault_tolerance_exception_e2e)
set_tests_properties(e2e_fault_tolerance_test_fault_tolerance_exception_e2e PROPERTIES TIMEOUT 600)

# Health Workflow E2E Tests (Phase 8: Complete Training Workflow with Health Monitoring)
add_e2e_fault_tolerance_test(test_health_workflow_e2e)
set_tests_properties(e2e_fault_tolerance_test_health_workflow_e2e PROPERTIES TIMEOUT 600)

# Heartbeat B01 Memory Core E2E Tests (Phase 8: Per-Batch Heartbeat Testing)
add_e2e_fault_tolerance_test(test_heartbeat_B01_memory_core_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B01_memory_core_e2e PROPERTIES TIMEOUT 300)

# Heartbeat B02 Memory (non-core) E2E Tests (Phase 8: Per-Batch Heartbeat Testing)
add_e2e_fault_tolerance_test(test_heartbeat_B02_memory_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B02_memory_e2e PROPERTIES TIMEOUT 300)

# Heartbeat B03 Immune E2E Tests (Phase 8: Per-Batch Heartbeat Testing)
add_e2e_fault_tolerance_test(test_heartbeat_B03_immune_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B03_immune_e2e PROPERTIES TIMEOUT 300)

# Heartbeat B04 Parietal E2E Tests (Phase 8: Per-Batch Heartbeat Testing)
add_e2e_fault_tolerance_test(test_heartbeat_B04_parietal_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B04_parietal_e2e PROPERTIES TIMEOUT 300)

# Heartbeat B05 Mirror Neurons E2E Tests (Phase 8: Per-Batch Heartbeat Testing)
add_e2e_fault_tolerance_test(test_heartbeat_B05_mirror_neurons_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B05_mirror_neurons_e2e PROPERTIES TIMEOUT 300)

# Heartbeat B06 Integration E2E Tests (Phase 8: Per-Batch Heartbeat Testing)
add_e2e_fault_tolerance_test(test_heartbeat_B06_integration_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B06_integration_e2e PROPERTIES TIMEOUT 300)

# Heartbeat B07 Game Theory E2E Tests (Phase 8: Per-Batch Heartbeat Testing)
add_e2e_fault_tolerance_test(test_heartbeat_B07_game_theory_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B07_game_theory_e2e PROPERTIES TIMEOUT 300)

# Heartbeat B08 Ethics/Reasoning E2E Tests (Phase 8: Per-Batch Heartbeat Testing)
add_e2e_fault_tolerance_test(test_heartbeat_B08_ethics_reasoning_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B08_ethics_reasoning_e2e PROPERTIES TIMEOUT 300)

# Heartbeat B09 Omni/Recursive E2E Tests (Phase 8: Per-Batch Heartbeat Testing)
add_e2e_fault_tolerance_test(test_heartbeat_B09_omni_recursive_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B09_omni_recursive_e2e PROPERTIES TIMEOUT 300)

# Heartbeat B10 FEP/FaultTolerance E2E Tests (Phase 8: Per-Batch Heartbeat Testing)
add_e2e_fault_tolerance_test(test_heartbeat_B10_fep_ft_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B10_fep_ft_e2e PROPERTIES TIMEOUT 300)

# Heartbeat B11 Wellbeing/Introspection E2E Tests (Phase 8: Per-Batch Heartbeat Testing)
add_e2e_fault_tolerance_test(test_heartbeat_B11_wellbeing_introspection_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B11_wellbeing_introspection_e2e PROPERTIES TIMEOUT 300)

# Heartbeat B12 Neuro-Symbolic/JEPA E2E Tests (Phase 8: Per-Batch Heartbeat Testing)
add_e2e_fault_tolerance_test(test_heartbeat_B12_neuro_symbolic_jepa_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B12_neuro_symbolic_jepa_e2e PROPERTIES TIMEOUT 300)

# Heartbeat B13 Knowledge/MentalHealth/Imagination E2E Tests (Phase 8: Per-Batch Heartbeat Testing)
add_e2e_fault_tolerance_test(test_heartbeat_B13_knowledge_mentalhealth_imagination_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B13_knowledge_mentalhealth_imagination_e2e PROPERTIES TIMEOUT 300)

# B14: cognitive/curiosity + cognitive/collective_cognition + cognitive/logic
add_e2e_fault_tolerance_test(test_heartbeat_B14_curiosity_collective_logic_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B14_curiosity_collective_logic_e2e PROPERTIES TIMEOUT 300)

# B15: cognitive/theory_of_mind + cognitive/symbolic_logic + cognitive/self_model + cognitive/predictive
add_e2e_fault_tolerance_test(test_heartbeat_B15_tom_symboliclogic_selfmodel_predictive_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B15_tom_symboliclogic_selfmodel_predictive_e2e PROPERTIES TIMEOUT 300)

# B16: cognitive/executive + cognitive/consolidation + cognitive/autobiographical_memory + cognitive/attention
add_e2e_fault_tolerance_test(test_heartbeat_B16_executive_consolidation_autobio_attention_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B16_executive_consolidation_autobio_attention_e2e PROPERTIES TIMEOUT 300)

# B17: cognitive/emotion + emotional_tagging + emotion_recognition + emotions + emotion_tensor + grief + joy + remorse
add_e2e_fault_tolerance_test(test_heartbeat_B17_emotion_cluster_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B17_emotion_cluster_e2e PROPERTIES TIMEOUT 300)

# B18: cognitive/empathetic_response + cognitive/ethics + cognitive/epistemic
add_e2e_fault_tolerance_test(test_heartbeat_B18_empathy_ethics_epistemic_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B18_empathy_ethics_epistemic_e2e PROPERTIES TIMEOUT 300)

# B19: All remaining 89 uncovered modules (final batch for 100% coverage)
add_e2e_fault_tolerance_test(test_heartbeat_B19_remaining_modules_e2e)
set_tests_properties(e2e_fault_tolerance_test_heartbeat_B19_remaining_modules_e2e PROPERTIES TIMEOUT 300)

# B20: cognitive/salience surprise bridges (plasticity, SNN, substrate, thalamic, pink_noise, imagination, self_model)
add_e2e_fault_tolerance_test(test_surprise_bridges_B20_e2e)
set_tests_properties(e2e_fault_tolerance_test_surprise_bridges_B20_e2e PROPERTIES TIMEOUT 300)

# B22: Mirror neuron bridge heartbeat/utility E2E tests
add_e2e_fault_tolerance_test(test_mirror_bridges_B22_e2e)
set_tests_properties(e2e_fault_tolerance_test_mirror_bridges_B22_e2e PROPERTIES TIMEOUT 300)

# B23: Parietal bridge heartbeat/utility E2E tests
add_e2e_fault_tolerance_test(test_parietal_bridges_B23_e2e)
set_tests_properties(e2e_fault_tolerance_test_parietal_bridges_B23_e2e PROPERTIES TIMEOUT 300)

# B24: Predictive+Personality+Self-Awareness bridge heartbeat/utility E2E tests
add_e2e_fault_tolerance_test(test_predictive_personality_selfawareness_bridges_B24_e2e)
set_tests_properties(e2e_fault_tolerance_test_predictive_personality_selfawareness_bridges_B24_e2e PROPERTIES TIMEOUT 300)

# B25: Memory/Core bridge heartbeat/utility E2E tests
add_e2e_fault_tolerance_test(test_memory_core_bridges_B25_e2e)
set_tests_properties(e2e_fault_tolerance_test_memory_core_bridges_B25_e2e PROPERTIES TIMEOUT 300)

# B26: Emotion cluster bridge heartbeat/utility E2E tests
add_e2e_fault_tolerance_test(test_emotion_cluster_bridges_B26_e2e)
set_tests_properties(e2e_fault_tolerance_test_emotion_cluster_bridges_B26_e2e PROPERTIES TIMEOUT 300)

# B25nb: Memory/Core non-bridge module heartbeat/utility E2E tests
add_e2e_fault_tolerance_test(test_memory_core_nonbridge_B25nb_e2e)
set_tests_properties(e2e_fault_tolerance_test_memory_core_nonbridge_B25nb_e2e PROPERTIES TIMEOUT 300)

# Health Agent E2E Tests - System-wide health agent macro validation
add_executable(e2e_fault_tolerance_test_health_agent_e2e
    test_health_agent_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)
target_link_libraries(e2e_fault_tolerance_test_health_agent_e2e
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(e2e_fault_tolerance_test_health_agent_e2e
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test/e2e
)
target_compile_features(e2e_fault_tolerance_test_health_agent_e2e PRIVATE cxx_std_17)
add_test(NAME e2e_fault_tolerance_test_health_agent_e2e
         COMMAND e2e_fault_tolerance_test_health_agent_e2e)
set_tests_properties(e2e_fault_tolerance_test_health_agent_e2e PROPERTIES
    LABELS "e2e"
    TIMEOUT 600
)

# ============================================================================
# P1-P3 Remediation E2E Tests
# ============================================================================

# Recovery E2E Tests (Circuit Breaker + Retry Framework Integration)
add_executable(e2e_fault_tolerance_test_recovery_e2e
    test_recovery_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)
target_link_libraries(e2e_fault_tolerance_test_recovery_e2e
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(e2e_fault_tolerance_test_recovery_e2e
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test/e2e
)
target_compile_features(e2e_fault_tolerance_test_recovery_e2e PRIVATE cxx_std_17)
add_test(NAME e2e_fault_tolerance_test_recovery_e2e
         COMMAND e2e_fault_tolerance_test_recovery_e2e)
set_tests_properties(e2e_fault_tolerance_test_recovery_e2e PROPERTIES
    LABELS "e2e;fault_tolerance;recovery;circuit_breaker;retry"
    TIMEOUT 600
)

# Graceful Degradation E2E Tests (Tier Transitions + Feature Shedding + Load Shedding)
add_executable(e2e_fault_tolerance_test_graceful_degradation_e2e
    test_graceful_degradation_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)
target_link_libraries(e2e_fault_tolerance_test_graceful_degradation_e2e
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(e2e_fault_tolerance_test_graceful_degradation_e2e
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test/e2e
)
target_compile_features(e2e_fault_tolerance_test_graceful_degradation_e2e PRIVATE cxx_std_17)
add_test(NAME e2e_fault_tolerance_test_graceful_degradation_e2e
         COMMAND e2e_fault_tolerance_test_graceful_degradation_e2e)
set_tests_properties(e2e_fault_tolerance_test_graceful_degradation_e2e PROPERTIES
    LABELS "e2e;fault_tolerance;graceful_degradation;tier;load_shedding"
    TIMEOUT 600
)

message(STATUS "Added P1-P3 Remediation E2E tests:")
message(STATUS "  - e2e_fault_tolerance_test_recovery_e2e (Circuit Breaker + Retry)")
message(STATUS "  - e2e_fault_tolerance_test_graceful_degradation_e2e (Graceful Degradation)")
