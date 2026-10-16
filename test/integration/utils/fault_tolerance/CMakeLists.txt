# Utils Fault Tolerance Integration Tests
# Enhanced Fault Tolerance Module Integration (Phase FT-2: Immune System)

# Helper macro to add integration test
macro(add_ft_integration_test TEST_NAME)
    add_executable(integration_utils_fault_tolerance_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_utils_fault_tolerance_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            pthread
    )
    target_include_directories(integration_utils_fault_tolerance_${TEST_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME integration_utils_fault_tolerance_${TEST_NAME}
             COMMAND integration_utils_fault_tolerance_${TEST_NAME})
    set_tests_properties(integration_utils_fault_tolerance_${TEST_NAME} PROPERTIES LABELS "integration")
endmacro()

# Existing integration tests
add_ft_integration_test(test_async_checkpoint_integration)
add_ft_integration_test(test_brain_recovery_integration)
add_ft_integration_test(test_checkpoint_recovery)
add_ft_integration_test(test_diagnostics_integration)
add_ft_integration_test(test_fast_recovery_integration)
add_ft_integration_test(test_fault_event_bus_integration)
add_ft_integration_test(test_fault_tolerance_integration)
add_ft_integration_test(test_lockfree_metrics_integration)
add_ft_integration_test(test_recovery_cache_integration)
add_ft_integration_test(test_recovery_pool_integration)

# Enhanced Fault Tolerance Integration Tests (Phase FT-2)
add_ft_integration_test(test_enhanced_fault_tolerance_integration)

# Health Agent USE Function Integration Tests (Phase 1)
add_ft_integration_test(test_health_agent_use_integration)
set_tests_properties(integration_utils_fault_tolerance_test_health_agent_use_integration
                     PROPERTIES TIMEOUT 180)

# Health Agent Connection Integration Tests (Phase 2)
add_ft_integration_test(test_health_agent_connection_integration)
set_tests_properties(integration_utils_fault_tolerance_test_health_agent_connection_integration
                     PROPERTIES TIMEOUT 180)

# Health Agent Consistency Integration Tests (Phase 3)
add_ft_integration_test(test_health_agent_consistency_integration)
set_tests_properties(integration_utils_fault_tolerance_test_health_agent_consistency_integration
                     PROPERTIES TIMEOUT 180)

# Health Agent Neural Integration Tests (Phase 5.5: SNN/LNN Health Monitoring)
add_ft_integration_test(test_health_agent_neural_integration)
set_tests_properties(integration_utils_fault_tolerance_test_health_agent_neural_integration
                     PROPERTIES TIMEOUT 180)

# Health Agent Behavioral Integration Tests (Phase 5.6: Dragonfly/Portia)
add_ft_integration_test(test_health_agent_behavioral_integration)
set_tests_properties(integration_utils_fault_tolerance_test_health_agent_behavioral_integration
                     PROPERTIES TIMEOUT 180)

# Health Agent Memory Integration Tests (Phase 5.7: Hippocampus/Mammillary)
add_ft_integration_test(test_health_agent_memory_integration)
set_tests_properties(integration_utils_fault_tolerance_test_health_agent_memory_integration
                     PROPERTIES TIMEOUT 180)

# Capacity Manager Integration Tests (Phase 5.8: Dynamic Capacity Management)
add_ft_integration_test(test_capacity_manager_integration)
set_tests_properties(integration_utils_fault_tolerance_test_capacity_manager_integration
                     PROPERTIES TIMEOUT 180)

# Symbolic Logic Health Integration Tests (Phase 5.9: Symbolic Logic Health Integration)
add_ft_integration_test(test_symbolic_logic_health_integration)
set_tests_properties(integration_utils_fault_tolerance_test_symbolic_logic_health_integration
                     PROPERTIES TIMEOUT 180)

# Neural Substrate Health Integration Tests (Phase 5.10: Neural Substrate Health Integration)
add_ft_integration_test(test_substrate_health_integration)
set_tests_properties(integration_utils_fault_tolerance_test_substrate_health_integration
                     PROPERTIES TIMEOUT 180)

# Thalamic/Middleware Health Integration Tests (Phase 5.11: Thalamic/Middleware Health Integration)
add_ft_integration_test(test_thalamic_middleware_health_integration)
set_tests_properties(integration_utils_fault_tolerance_test_thalamic_middleware_health_integration
                     PROPERTIES TIMEOUT 180)

# Perception/Cortical Health Integration Tests (Phase 5.12: Perception/Cortical Health Integration)
add_ft_integration_test(test_perception_cortical_health_integration)
set_tests_properties(integration_utils_fault_tolerance_test_perception_cortical_health_integration
                     PROPERTIES TIMEOUT 180)

# Brain Probe Health Integration Tests (Phase 5.13: Brain Probes Enhancement)
add_ft_integration_test(test_brain_probe_health_integration)
set_tests_properties(integration_utils_fault_tolerance_test_brain_probe_health_integration
                     PROPERTIES TIMEOUT 180)

# World Model & Imagination Health Integration Tests (Phase 5.14: World Model & Imagination Health Integration)
add_ft_integration_test(test_world_imagination_health_integration)
set_tests_properties(integration_utils_fault_tolerance_test_world_imagination_health_integration
                     PROPERTIES TIMEOUT 180)

# Heartbeat Module Integration Tests (Phase 8: Heartbeat for Long Operations)
add_ft_integration_test(test_heartbeat_module_integration)
set_tests_properties(integration_utils_fault_tolerance_test_heartbeat_module_integration
                     PROPERTIES TIMEOUT 180)

# State Manager Integration Tests (Phase 8: System-Wide Health Integration)
add_ft_integration_test(test_state_manager_integration)
set_tests_properties(integration_utils_fault_tolerance_test_state_manager_integration
                     PROPERTIES TIMEOUT 180)

# Module Recovery Integration Tests (Phase 8: System-Wide Health Integration)
add_ft_integration_test(test_module_recovery_integration)
set_tests_properties(integration_utils_fault_tolerance_test_module_recovery_integration
                     PROPERTIES TIMEOUT 180)

# STDP Health Metrics Integration Tests (Plasticity Health Monitoring)
add_ft_integration_test(test_stdp_health_integration)
set_tests_properties(integration_utils_fault_tolerance_test_stdp_health_integration
                     PROPERTIES TIMEOUT 180)

# Plasticity Anomaly Detection Integration Tests (Plasticity Health Monitoring)
add_ft_integration_test(test_plasticity_anomaly_integration)
set_tests_properties(integration_utils_fault_tolerance_test_plasticity_anomaly_integration
                     PROPERTIES TIMEOUT 180)

# Fault Tolerance Exception Handling Integration Tests (NIMCP_THROW_TO_IMMUNE)
add_ft_integration_test(test_fault_tolerance_exception_integration)
set_tests_properties(integration_utils_fault_tolerance_test_fault_tolerance_exception_integration
                     PROPERTIES TIMEOUT 180)
