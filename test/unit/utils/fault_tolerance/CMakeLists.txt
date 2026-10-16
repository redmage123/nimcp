# Fault Tolerance Unit Tests
# All tests in this directory are explicitly managed here

# Helper macro to add fault tolerance test
macro(add_fault_tolerance_test TEST_NAME)
    add_executable(unit_utils_fault_tolerance_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_utils_fault_tolerance_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
    )
    add_test(NAME unit_utils_fault_tolerance_${TEST_NAME}
             COMMAND unit_utils_fault_tolerance_${TEST_NAME})
endmacro()

# Add all fault tolerance tests
add_fault_tolerance_test(test_async_checkpoint)
set_tests_properties(unit_utils_fault_tolerance_test_async_checkpoint PROPERTIES TIMEOUT 300)
add_fault_tolerance_test(test_checkpoint)
set_tests_properties(unit_utils_fault_tolerance_test_checkpoint PROPERTIES TIMEOUT 300)
add_fault_tolerance_test(test_checkpoint_pool)
set_tests_properties(unit_utils_fault_tolerance_test_checkpoint_pool PROPERTIES TIMEOUT 300)
add_fault_tolerance_test(test_diagnostics)
add_fault_tolerance_test(test_fast_recovery)
add_fault_tolerance_test(test_fault_event_bus)
add_fault_tolerance_test(test_health_monitor)
set_tests_properties(unit_utils_fault_tolerance_test_health_monitor PROPERTIES TIMEOUT 60)
add_fault_tolerance_test(test_lockfree_metrics)
add_fault_tolerance_test(test_metrics_aggregator)
add_fault_tolerance_test(test_recovery)
add_fault_tolerance_test(test_recovery_cache)
add_fault_tolerance_test(test_recovery_pool)
add_fault_tolerance_test(test_state_machine)

# Enhanced Fault Tolerance Tests (Phase FT-2: Immune System Integration)
add_fault_tolerance_test(test_distributed_fault_tolerance)
add_fault_tolerance_test(test_hierarchical_recovery)
add_fault_tolerance_test(test_recovery_evolution)
add_fault_tolerance_test(test_byzantine_fault_tolerance)
add_fault_tolerance_test(test_graceful_degradation)
add_fault_tolerance_test(test_recovery_observability)
set_tests_properties(unit_utils_fault_tolerance_test_recovery_observability PROPERTIES TIMEOUT 60)
add_fault_tolerance_test(test_chaos_engineering)
add_fault_tolerance_test(test_predictive_analysis)

# Health Agent USE Function Tests (Phase 1)
add_fault_tolerance_test(test_health_agent_use_functions)
set_tests_properties(unit_utils_fault_tolerance_test_health_agent_use_functions PROPERTIES TIMEOUT 120)

# Health Agent Connection Function Tests (Phase 2)
add_fault_tolerance_test(test_health_agent_connection_functions)
set_tests_properties(unit_utils_fault_tolerance_test_health_agent_connection_functions PROPERTIES TIMEOUT 120)

# Health Agent Consistency Function Tests (Phase 3)
add_fault_tolerance_test(test_health_agent_consistency_functions)
set_tests_properties(unit_utils_fault_tolerance_test_health_agent_consistency_functions PROPERTIES TIMEOUT 120)

# Cognitive Recovery Coordinator Tests (Brain-Driven Recovery Orchestration)
add_fault_tolerance_test(test_cognitive_recovery)
set_tests_properties(unit_utils_fault_tolerance_test_cognitive_recovery PROPERTIES TIMEOUT 120)

# Health Agent Neural Integration Tests (Phase 5.5: SNN/LNN Health Monitoring)
add_fault_tolerance_test(test_health_agent_neural_functions)
set_tests_properties(unit_utils_fault_tolerance_test_health_agent_neural_functions PROPERTIES TIMEOUT 120)

# Health Agent Behavioral Integration Tests (Phase 5.6: Dragonfly/Portia Health Monitoring)
add_fault_tolerance_test(test_health_agent_behavioral_functions)
set_tests_properties(unit_utils_fault_tolerance_test_health_agent_behavioral_functions PROPERTIES TIMEOUT 120)

# Health Agent Memory System Health Tests (Phase 5.7: Hippocampus/Mammillary Health Monitoring)
add_fault_tolerance_test(test_health_agent_memory_functions)
set_tests_properties(unit_utils_fault_tolerance_test_health_agent_memory_functions PROPERTIES TIMEOUT 120)

# Capacity Manager Tests (Phase 5.8: Dynamic Capacity Management)
add_fault_tolerance_test(test_capacity_manager_functions)
set_tests_properties(unit_utils_fault_tolerance_test_capacity_manager_functions PROPERTIES TIMEOUT 120)

# Symbolic Logic Health Tests (Phase 5.9: Symbolic Logic Health Integration)
add_fault_tolerance_test(test_symbolic_logic_health_functions)
set_tests_properties(unit_utils_fault_tolerance_test_symbolic_logic_health_functions PROPERTIES TIMEOUT 120)

# Neural Substrate Health Tests (Phase 5.10: Neural Substrate Health Integration)
add_fault_tolerance_test(test_substrate_health_functions)
set_tests_properties(unit_utils_fault_tolerance_test_substrate_health_functions PROPERTIES TIMEOUT 120)

# Thalamic/Middleware Health Tests (Phase 5.11: Thalamic/Middleware Health Integration)
add_fault_tolerance_test(test_thalamic_middleware_health_functions)
set_tests_properties(unit_utils_fault_tolerance_test_thalamic_middleware_health_functions PROPERTIES TIMEOUT 120)

# Perception/Cortical Health Tests (Phase 5.12: Perception/Cortical Health Integration)
add_fault_tolerance_test(test_perception_cortical_health_functions)
set_tests_properties(unit_utils_fault_tolerance_test_perception_cortical_health_functions PROPERTIES TIMEOUT 120)

# Brain Probe Health Tests (Phase 5.13: Brain Probes Enhancement)
add_fault_tolerance_test(test_brain_probe_health_functions)
set_tests_properties(unit_utils_fault_tolerance_test_brain_probe_health_functions PROPERTIES TIMEOUT 120)

# World Model & Imagination Health Tests (Phase 5.14: World Model & Imagination Health Integration)
add_fault_tolerance_test(test_health_agent_world_imagination_functions)
set_tests_properties(unit_utils_fault_tolerance_test_health_agent_world_imagination_functions PROPERTIES TIMEOUT 120)

# Heartbeat Module Tests (Phase 8: Heartbeat for Long Operations)
add_fault_tolerance_test(test_heartbeat_module_functions)
set_tests_properties(unit_utils_fault_tolerance_test_heartbeat_module_functions PROPERTIES TIMEOUT 120)

# State Manager Tests (Phase 8: System-Wide Health Integration)
add_fault_tolerance_test(test_state_manager)
set_tests_properties(unit_utils_fault_tolerance_test_state_manager PROPERTIES TIMEOUT 120)

# Module Recovery Tests (Phase 8: System-Wide Health Integration)
add_fault_tolerance_test(test_module_recovery)
set_tests_properties(unit_utils_fault_tolerance_test_module_recovery PROPERTIES TIMEOUT 120)

# STDP Health Metrics Tests (Plasticity Health Monitoring)
add_fault_tolerance_test(test_stdp_health_metrics)
set_tests_properties(unit_utils_fault_tolerance_test_stdp_health_metrics PROPERTIES TIMEOUT 120)

# Plasticity Anomaly Detection Tests (Plasticity Health Monitoring)
add_fault_tolerance_test(test_plasticity_anomaly_detection)
set_tests_properties(unit_utils_fault_tolerance_test_plasticity_anomaly_detection PROPERTIES TIMEOUT 120)

# Fault Tolerance Exception Handling Tests (NIMCP_THROW_TO_IMMUNE)
add_fault_tolerance_test(test_fault_tolerance_exception)
set_tests_properties(unit_utils_fault_tolerance_test_fault_tolerance_exception PROPERTIES TIMEOUT 120)

# Health Agent Macros Tests (P0-P2 fixes: NIMCP_DECLARE_HEALTH_AGENT macro validation)
add_fault_tolerance_test(test_health_agent_macros)
set_tests_properties(unit_utils_fault_tolerance_test_health_agent_macros PROPERTIES TIMEOUT 60)
