# Utils Fault Tolerance Regression Tests
# Enhanced Fault Tolerance Module Regression (Phase FT-2: Immune System)

# Helper macro to add regression test
macro(add_ft_regression_test TEST_NAME)
    add_executable(regression_utils_fault_tolerance_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(regression_utils_fault_tolerance_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            pthread
    )
    target_include_directories(regression_utils_fault_tolerance_${TEST_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME regression_utils_fault_tolerance_${TEST_NAME}
             COMMAND regression_utils_fault_tolerance_${TEST_NAME})
    set_tests_properties(regression_utils_fault_tolerance_${TEST_NAME} PROPERTIES LABELS "regression")
endmacro()

# Existing regression tests
add_ft_regression_test(test_checkpoint_format)
add_ft_regression_test(test_diagnostics_regression)
add_ft_regression_test(test_fast_recovery_regression)
add_ft_regression_test(test_fault_event_bus_regression)
set_tests_properties(regression_utils_fault_tolerance_test_fault_event_bus_regression
                     PROPERTIES TIMEOUT 180 RESOURCE_LOCK "brain_heavy")
add_ft_regression_test(test_lockfree_metrics_regression)
add_ft_regression_test(test_recovery_cache_regression)
add_ft_regression_test(test_recovery_pool_regression)

# Enhanced Fault Tolerance Regression Tests (Phase FT-2)
add_ft_regression_test(test_enhanced_fault_tolerance_regression)

# Health Agent USE Function Regression Tests (Phase 1)
add_ft_regression_test(test_health_agent_use_regression)
set_tests_properties(regression_utils_fault_tolerance_test_health_agent_use_regression
                     PROPERTIES TIMEOUT 120)

# Health Agent Connection Regression Tests (Phase 2)
add_ft_regression_test(test_health_agent_connection_regression)
set_tests_properties(regression_utils_fault_tolerance_test_health_agent_connection_regression
                     PROPERTIES TIMEOUT 120)

# Health Agent Consistency Regression Tests (Phase 3)
add_ft_regression_test(test_health_agent_consistency_regression)
set_tests_properties(regression_utils_fault_tolerance_test_health_agent_consistency_regression
                     PROPERTIES TIMEOUT 120)

# Health Agent Neural Integration Regression Tests (Phase 5.5: SNN/LNN)
add_ft_regression_test(test_health_agent_neural_regression)
set_tests_properties(regression_utils_fault_tolerance_test_health_agent_neural_regression
                     PROPERTIES TIMEOUT 120)

# Health Agent Behavioral Integration Regression Tests (Phase 5.6: Dragonfly/Portia)
add_ft_regression_test(test_health_agent_behavioral_regression)
set_tests_properties(regression_utils_fault_tolerance_test_health_agent_behavioral_regression
                     PROPERTIES TIMEOUT 120)

# Health Agent Memory System Regression Tests (Phase 5.7: Hippocampus/Mammillary)
add_ft_regression_test(test_health_agent_memory_regression)
set_tests_properties(regression_utils_fault_tolerance_test_health_agent_memory_regression
                     PROPERTIES TIMEOUT 120)

# Capacity Manager Regression Tests (Phase 5.8: Dynamic Capacity Management)
add_ft_regression_test(test_capacity_manager_regression)
set_tests_properties(regression_utils_fault_tolerance_test_capacity_manager_regression
                     PROPERTIES TIMEOUT 120)

# Symbolic Logic Health Regression Tests (Phase 5.9: Symbolic Logic Health Integration)
add_ft_regression_test(test_symbolic_logic_health_regression)
set_tests_properties(regression_utils_fault_tolerance_test_symbolic_logic_health_regression
                     PROPERTIES TIMEOUT 120)

# Neural Substrate Health Regression Tests (Phase 5.10: Neural Substrate Health Integration)
add_ft_regression_test(test_substrate_health_regression)
set_tests_properties(regression_utils_fault_tolerance_test_substrate_health_regression
                     PROPERTIES TIMEOUT 120)

# Thalamic/Middleware Health Regression Tests (Phase 5.11: Thalamic/Middleware Health Integration)
add_ft_regression_test(test_thalamic_middleware_health_regression)
set_tests_properties(regression_utils_fault_tolerance_test_thalamic_middleware_health_regression
                     PROPERTIES TIMEOUT 120)

# Perception/Cortical Health Regression Tests (Phase 5.12: Perception/Cortical Health Integration)
add_ft_regression_test(test_perception_cortical_health_regression)
set_tests_properties(regression_utils_fault_tolerance_test_perception_cortical_health_regression
                     PROPERTIES TIMEOUT 120)

# Brain Probe Health Regression Tests (Phase 5.13: Brain Probes Enhancement)
add_ft_regression_test(test_brain_probe_health_regression)
set_tests_properties(regression_utils_fault_tolerance_test_brain_probe_health_regression
                     PROPERTIES TIMEOUT 120)

# World Model & Imagination Health Regression Tests (Phase 5.14: World Model & Imagination Health Integration)
add_ft_regression_test(test_world_imagination_health_regression)
set_tests_properties(regression_utils_fault_tolerance_test_world_imagination_health_regression
                     PROPERTIES TIMEOUT 120)

# Heartbeat Module Regression Tests (Phase 8: Heartbeat for Long Operations)
add_ft_regression_test(test_heartbeat_module_regression)
set_tests_properties(regression_utils_fault_tolerance_test_heartbeat_module_regression
                     PROPERTIES TIMEOUT 120)

# State Manager Regression Tests (Phase 8: System-Wide Health Integration)
add_ft_regression_test(test_state_manager_regression)
set_tests_properties(regression_utils_fault_tolerance_test_state_manager_regression
                     PROPERTIES TIMEOUT 120)

# Module Recovery Regression Tests (Phase 8: System-Wide Health Integration)
add_ft_regression_test(test_module_recovery_regression)
set_tests_properties(regression_utils_fault_tolerance_test_module_recovery_regression
                     PROPERTIES TIMEOUT 120)

# DISABLED: Test needs API updates to match current stdp_health/plasticity_anomaly API
# TODO: Update struct field names to match current API (stdp_anomaly_report_t, stdp_health_stats_t)
#
# # STDP Health Metrics Regression Tests (Plasticity Health Monitoring)
# add_ft_regression_test(test_stdp_health_regression)
# set_tests_properties(regression_utils_fault_tolerance_test_stdp_health_regression
#                      PROPERTIES TIMEOUT 120)
#
# # Plasticity Anomaly Detection Regression Tests (Plasticity Health Monitoring)
# add_ft_regression_test(test_plasticity_anomaly_regression)
# set_tests_properties(regression_utils_fault_tolerance_test_plasticity_anomaly_regression
#                      PROPERTIES TIMEOUT 120)

# Fault Tolerance Exception Handling Regression Tests (NIMCP_THROW_TO_IMMUNE)
add_ft_regression_test(test_fault_tolerance_exception_regression)
set_tests_properties(regression_utils_fault_tolerance_test_fault_tolerance_exception_regression
                     PROPERTIES TIMEOUT 120)
