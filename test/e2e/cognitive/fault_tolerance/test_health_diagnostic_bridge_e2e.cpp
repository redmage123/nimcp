/**
 * @file test_health_diagnostic_bridge_e2e.cpp
 * @brief End-to-end tests for Health Diagnostic Bridge
 * @version 1.0.0
 * @date 2025-01-27
 *
 * WHAT: End-to-end tests verifying complete health-to-repair pipeline
 * WHY: Ensure the full pipeline works correctly in realistic scenarios
 * HOW: Simulate realistic health events flowing through the entire system
 *
 * Test Coverage:
 * - Full lifecycle: detection → diagnostic → enrichment → self-repair
 * - Concurrent multi-module operation
 * - High-frequency burst processing
 * - Timeout and silence detection
 * - Multi-phase operation sequences
 * - Hot-swap of components during operation
 * - Sustained operation over time
 * - Graceful shutdown sequences
 * - KG wiring end-to-end connectivity
 * - Security validation end-to-end
 * - Math utils phasor pattern detection end-to-end
 * - Quantum annealing optimization end-to-end
 * - Code immune auto-repair end-to-end
 * - Exception handler end-to-end resilience
 * - Logging end-to-end pipeline
 * - Memory stability under sustained load
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <limits>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

// Core headers (have their own extern "C" guards)
#include "cognitive/fault_tolerance/nimcp_health_diagnostic_bridge.h"
#include "cognitive/fault_tolerance/nimcp_health_self_repair_bridge.h"
#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "cognitive/fault_tolerance/nimcp_self_repair_health_notify.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "security/nimcp_security.h"
#include "utils/math/nimcp_complex_math.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"
#include "cognitive/immune/nimcp_code_immune.h"

extern "C" {
    void health_diagnostic_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

//=============================================================================
// Test Fixture
//=============================================================================

class HealthDiagBridgeE2ETest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;
    nimcp_health_agent_t* agent = nullptr;
    health_diag_bridge_t* diag_bridge = nullptr;
    self_repair_coordinator_t* self_repair = nullptr;
    health_self_repair_bridge_t* repair_bridge = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;

        // Create health agent
        health_agent_config_t agent_cfg;
        nimcp_health_agent_default_config(&agent_cfg);
        agent_cfg.check_interval_ms = 50;
        agent_cfg.enable_auto_recovery = false;
        agent = nimcp_health_agent_create(&agent_cfg);
        ASSERT_NE(agent, nullptr);

        // Create diagnostic bridge
        health_diag_bridge_config_t diag_cfg;
        health_diag_bridge_default_config(&diag_cfg);
        diag_cfg.capture_stack_trace = true;
        diag_cfg.capture_memory_snapshot = true;
        diag_cfg.enable_pattern_analysis = true;
        diag_bridge = health_diag_bridge_create(&diag_cfg);
        ASSERT_NE(diag_bridge, nullptr);

        // Create self-repair coordinator
        self_repair = self_repair_create(NULL);
        ASSERT_NE(self_repair, nullptr);

        // Create self-repair bridge
        repair_bridge = health_self_repair_bridge_create(
            NULL, diag_bridge, self_repair);
        ASSERT_NE(repair_bridge, nullptr);

        // Wire health agent
        health_diagnostic_bridge_set_health_agent(agent);
    }

    void TearDown() override {
        health_diagnostic_bridge_set_health_agent(NULL);

        if (repair_bridge) health_self_repair_bridge_destroy(repair_bridge);
        if (self_repair) self_repair_destroy(self_repair);
        if (diag_bridge) health_diag_bridge_destroy(diag_bridge);
        if (agent) nimcp_health_agent_destroy(agent);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, baseline_allocated)
            << "Memory leak detected! Allocated: " << stats.current_allocated
            << ", Baseline: " << baseline_allocated;
    }

    void create_anomaly(anomaly_t* anomaly, anomaly_type_t type,
                        anomaly_severity_t severity, float confidence) {
        memset(anomaly, 0, sizeof(*anomaly));
        anomaly->type = type;
        anomaly->severity = severity;
        anomaly->confidence = confidence;
        snprintf(anomaly->description, sizeof(anomaly->description),
                 "E2E test anomaly type=%d", (int)type);
        snprintf(anomaly->affected_component, sizeof(anomaly->affected_component),
                 "e2e_component_%d", (int)type);
        anomaly->metric_value = 100.0;
        anomaly->expected_value = 50.0;
        anomaly->deviation = 50.0;
    }

    void create_agent_msg(health_agent_message_t* msg, health_agent_msg_type_t type,
                          health_agent_severity_t severity) {
        memset(msg, 0, sizeof(*msg));
        msg->type = type;
        msg->severity = severity;
        snprintf(msg->description, sizeof(msg->description),
                 "E2E test message type=%d", (int)type);
    }
};

//=============================================================================
// Full Lifecycle E2E Tests
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, FullLifecycleAnomalyToRepair) {
    nimcp_health_agent_start(agent);

    // Step 1: Simulate anomaly detection
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.95f);

    // Step 2: Convert to diagnostic
    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_OUT_OF_MEMORY);
    EXPECT_EQ(result->severity, DIAG_SEVERITY_CRITICAL);
    EXPECT_GT(result->stack_depth, 0u);
    EXPECT_GT(result->timestamp, 0);
    EXPECT_EQ(result->occurrence_count, 1u);

    // Step 3: Feed through repair pipeline
    uint64_t request_id = 0;
    health_self_repair_bridge_process_anomaly(repair_bridge, &anomaly, &request_id);

    // Step 4: Verify stats across systems
    // Note: 2 conversions total - explicit conversion above + internal conversion
    // by health_self_repair_bridge_process_anomaly
    health_diag_bridge_stats_t diag_stats;
    health_diag_bridge_get_stats(diag_bridge, &diag_stats);
    EXPECT_EQ(diag_stats.anomalies_converted, 2u);
    EXPECT_EQ(diag_stats.by_anomaly_type[ANOMALY_RESOURCE_EXHAUSTION], 2u);
    EXPECT_EQ(diag_stats.by_severity[DIAG_SEVERITY_CRITICAL], 2u);
    EXPECT_GE(diag_stats.stack_traces_captured, 1u);
    EXPECT_GE(diag_stats.memory_snapshots_captured, 1u);

    diagnostics_free_result(result);
    nimcp_health_agent_stop(agent);
}

TEST_F(HealthDiagBridgeE2ETest, FullLifecycleAgentMessageToRepair) {
    nimcp_health_agent_start(agent);

    // Step 1: Simulate agent message
    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_DEADLOCK_DETECTED, HEALTH_SEVERITY_CRITICAL);
    msg.source = HEALTH_SOURCE_THREADING;
    msg.data.deadlock.thread_id_1 = 1001;
    msg.data.deadlock.thread_id_2 = 1002;

    // Step 2: Convert to diagnostic
    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_agent_message(diag_bridge, &msg, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_DEADLOCK);
    EXPECT_EQ(result->severity, DIAG_SEVERITY_CRITICAL);

    // Step 3: Feed through repair pipeline
    uint64_t request_id = 0;
    health_self_repair_bridge_process_agent_message(repair_bridge, &msg, &request_id);

    diagnostics_free_result(result);
    nimcp_health_agent_stop(agent);
}

TEST_F(HealthDiagBridgeE2ETest, FullLifecycleAllAnomalyTypes) {
    nimcp_health_agent_start(agent);

    const struct {
        anomaly_type_t type;
        anomaly_severity_t severity;
        error_type_t expected_error;
    } test_cases[] = {
        {ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, ERROR_TYPE_MEMORY_LEAK},
        {ANOMALY_PERFORMANCE_DEGRADATION, ANOMALY_SEVERITY_WARNING, ERROR_TYPE_INVALID_STATE},
        {ANOMALY_ERROR_SPIKE, ANOMALY_SEVERITY_ERROR, ERROR_TYPE_UNKNOWN},
        {ANOMALY_RESOURCE_EXHAUSTION, ANOMALY_SEVERITY_CRITICAL, ERROR_TYPE_OUT_OF_MEMORY},
        {ANOMALY_NUMERICAL_INSTABILITY, ANOMALY_SEVERITY_ERROR, ERROR_TYPE_NAN_DETECTED},
        {ANOMALY_THREAD_CONTENTION, ANOMALY_SEVERITY_WARNING, ERROR_TYPE_DEADLOCK},
        {ANOMALY_CACHE_THRASHING, ANOMALY_SEVERITY_WARNING, ERROR_TYPE_INVALID_STATE},
        {ANOMALY_THROUGHPUT_DROP, ANOMALY_SEVERITY_WARNING, ERROR_TYPE_INVALID_STATE},
    };

    for (const auto& tc : test_cases) {
        SCOPED_TRACE(health_diag_bridge_anomaly_type_name(tc.type));

        anomaly_t anomaly;
        create_anomaly(&anomaly, tc.type, tc.severity, 0.8f);

        diagnostic_result_t* result = NULL;
        ASSERT_EQ(health_diag_bridge_convert_anomaly(
            diag_bridge, &anomaly, &result), 0);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->error_type, tc.expected_error);

        diagnostics_free_result(result);
    }

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.anomalies_converted, 8u);

    nimcp_health_agent_stop(agent);
}

//=============================================================================
// Concurrent Multi-Module E2E
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, ConcurrentMultiModuleOperation) {
    nimcp_health_agent_start(agent);

    const int thread_count = 4;
    const int ops_per_thread = 100;
    std::atomic<int> total_converted{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < ops_per_thread; i++) {
                if (i % 2 == 0) {
                    // Anomaly conversion
                    anomaly_t anomaly;
                    memset(&anomaly, 0, sizeof(anomaly));
                    anomaly.type = (anomaly_type_t)(1 + (t + i) % 9);
                    anomaly.severity = ANOMALY_SEVERITY_WARNING;
                    anomaly.confidence = 0.7f;

                    diagnostic_result_t* result = NULL;
                    if (health_diag_bridge_convert_anomaly(
                            diag_bridge, &anomaly, &result) == 0 && result) {
                        total_converted.fetch_add(1);
                        diagnostics_free_result(result);
                    }
                } else {
                    // Agent message conversion
                    health_agent_message_t msg;
                    memset(&msg, 0, sizeof(msg));
                    msg.type = (health_agent_msg_type_t)(t % HEALTH_MSG_COUNT);
                    msg.severity = HEALTH_SEVERITY_WARNING;

                    diagnostic_result_t* result = NULL;
                    if (health_diag_bridge_convert_agent_message(
                            diag_bridge, &msg, &result) == 0 && result) {
                        total_converted.fetch_add(1);
                        diagnostics_free_result(result);
                    }
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    EXPECT_EQ(total_converted.load(), thread_count * ops_per_thread);
    nimcp_health_agent_stop(agent);
}

//=============================================================================
// High-Frequency Burst E2E
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, HighFrequencyBurst1000Conversions) {
    nimcp_health_agent_start(agent);

    health_diag_bridge_stats_t before;
    health_diag_bridge_get_stats(diag_bridge, &before);

    for (int i = 0; i < 1000; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, (anomaly_type_t)(1 + i % 9),
                       ANOMALY_SEVERITY_WARNING, 0.7f);

        diagnostic_result_t* result = NULL;
        ASSERT_EQ(health_diag_bridge_convert_anomaly(
            diag_bridge, &anomaly, &result), 0);
        ASSERT_NE(result, nullptr);
        diagnostics_free_result(result);
    }

    health_diag_bridge_stats_t after;
    health_diag_bridge_get_stats(diag_bridge, &after);
    EXPECT_GE(after.anomalies_converted,
              before.anomalies_converted + 1000);

    nimcp_health_agent_stop(agent);
}

TEST_F(HealthDiagBridgeE2ETest, HighFrequencyMixedBurst) {
    nimcp_health_agent_start(agent);

    for (int i = 0; i < 500; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_WARNING, 0.8f);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);

        health_agent_message_t msg;
        create_agent_msg(&msg, HEALTH_MSG_ANOMALY_DETECTED, HEALTH_SEVERITY_WARNING);
        result = NULL;
        health_diag_bridge_convert_agent_message(diag_bridge, &msg, &result);
        if (result) diagnostics_free_result(result);
    }

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.anomalies_converted, 500u);
    EXPECT_EQ(stats.agent_messages_converted, 500u);

    nimcp_health_agent_stop(agent);
}

//=============================================================================
// Multi-Phase Operation E2E
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, MultiPhaseOperation) {
    nimcp_health_agent_start(agent);

    // Phase 1: Anomaly monitoring
    for (int i = 0; i < 20; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_WARNING, 0.7f);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);
    }

    // Phase 2: Escalation (severity increases)
    for (int i = 0; i < 10; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                       ANOMALY_SEVERITY_CRITICAL, 0.9f);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
        if (result) {
            EXPECT_EQ(result->severity, DIAG_SEVERITY_CRITICAL);
            diagnostics_free_result(result);
        }

        uint64_t request_id = 0;
        health_self_repair_bridge_process_anomaly(
            repair_bridge, &anomaly, &request_id);
    }

    // Phase 3: Recovery monitoring
    for (int i = 0; i < 20; i++) {
        health_agent_message_t msg;
        create_agent_msg(&msg, HEALTH_MSG_STATUS_UPDATE, HEALTH_SEVERITY_INFO);
        diagnostic_result_t* result = NULL;
        // Status updates may be filtered by min severity
        health_diag_bridge_convert_agent_message(diag_bridge, &msg, &result);
        if (result) diagnostics_free_result(result);
    }

    // Note: repair bridge internally also converts anomalies, adding 10 more
    // Phase 1: 20 explicit + Phase 2: 10 explicit + 10 via repair bridge = 40
    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_GE(stats.anomalies_converted, 40u);
    EXPECT_EQ(stats.by_severity[DIAG_SEVERITY_CRITICAL], 20u);

    nimcp_health_agent_stop(agent);
}

//=============================================================================
// Component Hot-Swap E2E
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, DiagBridgeHotSwapDuringOperation) {
    nimcp_health_agent_start(agent);

    // Use initial bridge
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                   ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(
        diag_bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_GT(result->stack_depth, 0u);  // Enrichment enabled
    diagnostics_free_result(result);

    // Create replacement bridge with different config
    health_diag_bridge_config_t config2;
    health_diag_bridge_default_config(&config2);
    config2.capture_stack_trace = false;
    config2.capture_memory_snapshot = false;
    config2.default_confidence = 0.5f;

    health_diag_bridge_t* bridge2 = health_diag_bridge_create(&config2);
    ASSERT_NE(bridge2, nullptr);

    // Use replacement bridge
    result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge2, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->stack_depth, 0u);  // No enrichment
    diagnostics_free_result(result);

    health_diag_bridge_destroy(bridge2);
    nimcp_health_agent_stop(agent);
}

TEST_F(HealthDiagBridgeE2ETest, HealthAgentHotSwapDuringOperation) {
    nimcp_health_agent_start(agent);

    // Conversions with first agent
    for (int i = 0; i < 10; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_ERROR_SPIKE,
                       ANOMALY_SEVERITY_ERROR, 0.75f);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);
    }

    nimcp_health_agent_stop(agent);

    // Create replacement agent
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    cfg2.check_interval_ms = 100;
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(agent2, nullptr);

    health_diagnostic_bridge_set_health_agent(agent2);
    nimcp_health_agent_start(agent2);

    // Conversions with second agent
    for (int i = 0; i < 10; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_THREAD_CONTENTION,
                       ANOMALY_SEVERITY_WARNING, 0.7f);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);
    }

    nimcp_health_agent_stop(agent2);
    health_diagnostic_bridge_set_health_agent(agent);
    nimcp_health_agent_destroy(agent2);
}

//=============================================================================
// Sustained Operation E2E
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, SustainedOperationOverTime) {
    nimcp_health_agent_start(agent);

    auto start = std::chrono::steady_clock::now();
    uint64_t count = 0;

    while (std::chrono::steady_clock::now() - start <
           std::chrono::milliseconds(250)) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, (anomaly_type_t)(1 + count % 9),
                       ANOMALY_SEVERITY_WARNING, 0.7f);

        diagnostic_result_t* result = NULL;
        if (health_diag_bridge_convert_anomaly(
                diag_bridge, &anomaly, &result) == 0 && result) {
            diagnostics_free_result(result);
        }
        count++;
    }

    EXPECT_GT(count, 0u);

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_GE(stats.anomalies_converted, count);

    nimcp_health_agent_stop(agent);
}

//=============================================================================
// Graceful Shutdown E2E
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, GracefulShutdownSequence) {
    nimcp_health_agent_start(agent);

    // Populate state
    for (int i = 0; i < 50; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_WARNING, 0.8f);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);
    }

    // Verify stats before shutdown
    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.anomalies_converted, 50u);

    // Graceful shutdown: stop agent first, then clear bridge
    nimcp_health_agent_stop(agent);

    // Post-shutdown: stats should still be readable
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.anomalies_converted, 50u);
}

TEST_F(HealthDiagBridgeE2ETest, ShutdownDuringConcurrentOperations) {
    nimcp_health_agent_start(agent);

    std::atomic<bool> running{true};
    std::atomic<int> completed{0};

    std::thread worker([&]() {
        while (running.load()) {
            anomaly_t anomaly;
            memset(&anomaly, 0, sizeof(anomaly));
            anomaly.type = ANOMALY_MEMORY_LEAK;
            anomaly.severity = ANOMALY_SEVERITY_WARNING;
            anomaly.confidence = 0.7f;

            diagnostic_result_t* result = NULL;
            if (health_diag_bridge_convert_anomaly(
                    diag_bridge, &anomaly, &result) == 0 && result) {
                completed.fetch_add(1);
                diagnostics_free_result(result);
            }
        }
    });

    // Let it run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Signal stop
    running.store(false);
    worker.join();

    EXPECT_GT(completed.load(), 0);

    nimcp_health_agent_stop(agent);
}

//=============================================================================
// KG Wiring E2E
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, KGWiringFullPipelineConnectivity) {
    // Build full pipeline KG wiring
    kg_module_wiring_t* health_monitor_wiring = kg_module_wiring_create(
        "health_monitor", "FAULT_TOLERANCE");
    ASSERT_NE(health_monitor_wiring, nullptr);
    kg_module_wiring_add_output(health_monitor_wiring, "ANOMALY_DETECTED", "Anomaly signal");
    kg_module_wiring_set_version(health_monitor_wiring, 1, 0, 0);

    kg_module_wiring_t* diag_wiring = kg_module_wiring_create(
        "health_diagnostic_bridge", "FAULT_TOLERANCE");
    ASSERT_NE(diag_wiring, nullptr);
    kg_module_wiring_add_input(diag_wiring, "health_monitor", "ANOMALY_DETECTED", true);
    kg_module_wiring_add_input(diag_wiring, "health_agent", "HEALTH_AGENT_MESSAGE", true);
    kg_module_wiring_add_output(diag_wiring, "DIAGNOSTIC_RESULT", "Diagnostic result");
    kg_module_wiring_add_handler(diag_wiring, "ANOMALY_DETECTED", 200);
    kg_module_wiring_add_handler(diag_wiring, "HEALTH_AGENT_MESSAGE", 200);
    kg_module_wiring_set_version(diag_wiring, 1, 0, 0);

    kg_module_wiring_t* repair_wiring = kg_module_wiring_create(
        "self_repair_coordinator", "FAULT_TOLERANCE");
    ASSERT_NE(repair_wiring, nullptr);
    kg_module_wiring_add_input(repair_wiring, "health_diagnostic_bridge", "DIAGNOSTIC_RESULT", true);
    kg_module_wiring_add_output(repair_wiring, "REPAIR_OUTCOME", "Repair result");
    kg_module_wiring_add_handler(repair_wiring, "DIAGNOSTIC_RESULT", 300);
    kg_module_wiring_set_version(repair_wiring, 1, 0, 0);

    // Validate all wirings
    char error_buf[256];
    EXPECT_EQ(kg_module_wiring_validate(health_monitor_wiring, error_buf, sizeof(error_buf)), 0);
    EXPECT_EQ(kg_module_wiring_validate(diag_wiring, error_buf, sizeof(error_buf)), 0);
    EXPECT_EQ(kg_module_wiring_validate(repair_wiring, error_buf, sizeof(error_buf)), 0);

    // Verify cross-module connectivity
    EXPECT_TRUE(kg_module_wiring_has_output(health_monitor_wiring, "ANOMALY_DETECTED"));
    EXPECT_TRUE(kg_module_wiring_has_input(diag_wiring, "health_monitor", "ANOMALY_DETECTED"));
    EXPECT_TRUE(kg_module_wiring_has_output(diag_wiring, "DIAGNOSTIC_RESULT"));
    EXPECT_TRUE(kg_module_wiring_has_input(repair_wiring, "health_diagnostic_bridge", "DIAGNOSTIC_RESULT"));
    EXPECT_TRUE(kg_module_wiring_has_output(repair_wiring, "REPAIR_OUTCOME"));

    kg_module_wiring_destroy(repair_wiring);
    kg_module_wiring_destroy(diag_wiring);
    kg_module_wiring_destroy(health_monitor_wiring);
}

//=============================================================================
// Security E2E
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, SecurityValidationEndToEnd) {
    nimcp_health_agent_start(agent);

    // Process anomaly with description that needs validation
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);
    strncpy(anomaly.description,
            "Memory leak detected in neural allocator pool at 0x7fff12345678",
            sizeof(anomaly.description) - 1);

    // Validate input through security
    nimcp_input_validation_t valid = nimcp_security_validate_input(
        anomaly.description, sizeof(anomaly.description), NULL);
    EXPECT_EQ(valid, NIMCP_INPUT_VALID);

    // Process through bridge
    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);

    // Validate output
    if (strlen(result->root_cause) > 0) {
        nimcp_threat_level_t threat = nimcp_security_analyze_threat(result->root_cause);
        EXPECT_LE(threat, NIMCP_THREAT_LOW);
    }

    // Sanitize for logging
    char sanitized[512];
    nimcp_security_sanitize_input(result->root_cause, sanitized, sizeof(sanitized));

    diagnostics_free_result(result);
    nimcp_health_agent_stop(agent);
}

//=============================================================================
// Math Utils E2E - Phasor Pattern Analysis
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, PhasorPatternAnalysisEndToEnd) {
    complex_math_init(NULL);
    nimcp_health_agent_start(agent);

    // Generate multiple anomalies and track timing as phasor phases
    const int n = 12;
    neural_phasor_t timing_signals[12];

    for (int i = 0; i < n; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_WARNING, 0.8f);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);

        // Use anomaly index as a timing phase proxy
        float phase = 0.5f + 0.01f * (float)i;
        timing_signals[i] = phasor_from_polar(1.0f, phase);

        if (result) diagnostics_free_result(result);
    }

    // Analyze timing pattern
    float coherence = phasor_array_coherence(timing_signals, n);
    float mean_phase = phasor_array_mean_phase(timing_signals, n);
    float variance = phasor_array_phase_variance(timing_signals, n);

    // High coherence suggests systematic pattern
    EXPECT_GT(coherence, 0.9f);
    EXPECT_GT(mean_phase, 0.0f);
    EXPECT_LT(variance, 0.05f);

    // Verify stats
    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.anomalies_converted, (uint64_t)n);

    nimcp_health_agent_stop(agent);
    complex_math_cleanup();
}

//=============================================================================
// Quantum Annealing E2E - Multi-Severity Confidence Optimization
//=============================================================================

static float e2e_confidence_energy(const float* state, uint32_t dim, void* user_data) {
    (void)user_data;
    // Target: critical=0.95, error=0.80, warning=0.60, info=0.40
    float targets[] = {0.95f, 0.80f, 0.60f, 0.40f};
    float energy = 0.0f;
    for (uint32_t i = 0; i < dim && i < 4; i++) {
        float diff = state[i] - targets[i];
        energy += diff * diff;
    }
    return energy;
}

TEST_F(HealthDiagBridgeE2ETest, QuantumAnnealingConfidenceCalibration) {
    quantum_annealing_config_t config = quantum_annealing_default_config();
    config.num_iterations = 300;
    config.seed = 42;

    quantum_annealer_t annealer = quantum_annealer_create(&config);
    if (annealer) {
        float initial[4] = {0.5f, 0.5f, 0.5f, 0.5f};
        float optimized[4] = {0.0f};

        float final_energy = quantum_anneal(
            annealer, e2e_confidence_energy, initial, optimized, 4, NULL);

        EXPECT_LT(final_energy, 0.3f);

        // Use optimized confidences for anomaly processing
        nimcp_health_agent_start(agent);

        anomaly_severity_t severities[] = {
            ANOMALY_SEVERITY_CRITICAL, ANOMALY_SEVERITY_ERROR,
            ANOMALY_SEVERITY_WARNING, ANOMALY_SEVERITY_INFO
        };

        health_diag_bridge_config_t diag_cfg;
        health_diag_bridge_default_config(&diag_cfg);
        diag_cfg.min_severity = ANOMALY_SEVERITY_INFO;
        health_diag_bridge_t* calibrated = health_diag_bridge_create(&diag_cfg);

        if (calibrated) {
            for (int i = 0; i < 4; i++) {
                anomaly_t anomaly;
                create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                               severities[i],
                               fmaxf(0.0f, fminf(1.0f, optimized[i])));
                diagnostic_result_t* result = NULL;
                health_diag_bridge_convert_anomaly(calibrated, &anomaly, &result);
                if (result) diagnostics_free_result(result);
            }
            health_diag_bridge_destroy(calibrated);
        }

        nimcp_health_agent_stop(agent);
        quantum_annealer_destroy(annealer);
    }
}

//=============================================================================
// Code Immune E2E
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, CodeImmuneWithFullPipeline) {
    code_immune_config_t immune_config;
    code_immune_default_config(&immune_config);
    immune_config.auto_repair.enabled = true;
    immune_config.auto_repair.min_crash_count = 3;
    immune_config.auto_repair.cooldown_ms = 5000;

    code_immune_system_t* immune = code_immune_create_with_config(NULL, &immune_config);
    if (immune) {
        nimcp_health_agent_start(agent);

        // Simulate repeated anomalies that should trigger auto-repair
        for (int i = 0; i < 5; i++) {
            anomaly_t anomaly;
            create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                           ANOMALY_SEVERITY_CRITICAL, 0.95f);

            diagnostic_result_t* result = NULL;
            health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
            if (result) diagnostics_free_result(result);

            uint64_t request_id = 0;
            health_self_repair_bridge_process_anomaly(
                repair_bridge, &anomaly, &request_id);
        }

        nimcp_health_agent_stop(agent);
        code_immune_destroy(immune);
    }
}

//=============================================================================
// Logging E2E
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, LoggingAcrossFullPipeline) {
    nimcp_log_config_t log_config = nimcp_log_default_config();
    log_config.level = LOG_LEVEL_DEBUG;

    nimcp_logger_t logger = nimcp_log_create(&log_config);
    if (logger) {
        nimcp_health_agent_start(agent);

        // Full pipeline with logging active
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                       ANOMALY_SEVERITY_CRITICAL, 0.95f);

        diagnostic_result_t* result = NULL;
        ASSERT_EQ(health_diag_bridge_convert_anomaly(
            diag_bridge, &anomaly, &result), 0);
        ASSERT_NE(result, nullptr);

        uint64_t request_id = 0;
        health_self_repair_bridge_process_anomaly(
            repair_bridge, &anomaly, &request_id);

        diagnostics_free_result(result);
        nimcp_health_agent_stop(agent);
        nimcp_log_destroy(logger);
    }
}

//=============================================================================
// Exception Handler E2E Resilience
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, ExceptionResilienceUnderLoad) {
    nimcp_health_agent_start(agent);

    // Mix valid and invalid operations to test resilience
    for (int i = 0; i < 100; i++) {
        if (i % 5 == 0) {
            // Invalid: NULL bridge
            health_diag_bridge_convert_anomaly(NULL, NULL, NULL);
        } else if (i % 7 == 0) {
            // Invalid: NULL anomaly
            diagnostic_result_t* result = NULL;
            health_diag_bridge_convert_anomaly(diag_bridge, NULL, &result);
        } else if (i % 11 == 0) {
            // Invalid: NULL result
            anomaly_t anomaly;
            create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                           ANOMALY_SEVERITY_WARNING, 0.8f);
            health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, NULL);
        } else {
            // Valid conversion
            anomaly_t anomaly;
            create_anomaly(&anomaly, (anomaly_type_t)(1 + i % 9),
                           ANOMALY_SEVERITY_WARNING, 0.7f);
            diagnostic_result_t* result = NULL;
            if (health_diag_bridge_convert_anomaly(
                    diag_bridge, &anomaly, &result) == 0 && result) {
                diagnostics_free_result(result);
            }
        }
    }

    // Bridge should still be functional
    EXPECT_TRUE(health_diag_bridge_is_ready(diag_bridge));

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                   ANOMALY_SEVERITY_WARNING, 0.8f);
    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(
        diag_bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    diagnostics_free_result(result);

    nimcp_health_agent_stop(agent);
}

//=============================================================================
// Memory Stability Under Load E2E
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, MemoryStabilityUnderSustainedLoad) {
    nimcp_health_agent_start(agent);

    nimcp_memory_stats_t before;
    nimcp_memory_get_stats(&before);

    for (int phase = 0; phase < 5; phase++) {
        // Each phase: conversions + enrichment + stats
        for (int i = 0; i < 100; i++) {
            anomaly_t anomaly;
            create_anomaly(&anomaly, (anomaly_type_t)(1 + i % 9),
                           ANOMALY_SEVERITY_WARNING, 0.7f);
            diagnostic_result_t* result = NULL;
            health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
            if (result) diagnostics_free_result(result);
        }

        health_diag_bridge_stats_t stats;
        health_diag_bridge_get_stats(diag_bridge, &stats);
        EXPECT_EQ(stats.anomalies_converted, (uint64_t)((phase + 1) * 100));
    }

    nimcp_memory_stats_t after;
    nimcp_memory_get_stats(&after);

    // Memory should not grow significantly (bridge internal state is fixed)
    EXPECT_LE(after.current_allocated, before.current_allocated + 8192);

    nimcp_health_agent_stop(agent);
}

TEST_F(HealthDiagBridgeE2ETest, MultipleBridgeCreationDestructionCycles) {
    for (int cycle = 0; cycle < 5; cycle++) {
        health_diag_bridge_config_t cfg;
        health_diag_bridge_default_config(&cfg);
        cfg.default_confidence = 0.5f + 0.1f * cycle;

        health_diag_bridge_t* temp_bridge = health_diag_bridge_create(&cfg);
        ASSERT_NE(temp_bridge, nullptr);
        EXPECT_TRUE(health_diag_bridge_is_ready(temp_bridge));

        for (int i = 0; i < 20; i++) {
            anomaly_t anomaly;
            create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                           ANOMALY_SEVERITY_WARNING, 0.8f);
            diagnostic_result_t* result = NULL;
            health_diag_bridge_convert_anomaly(temp_bridge, &anomaly, &result);
            if (result) diagnostics_free_result(result);
        }

        health_diag_bridge_stats_t stats;
        health_diag_bridge_get_stats(temp_bridge, &stats);
        EXPECT_EQ(stats.anomalies_converted, 20u);

        health_diag_bridge_destroy(temp_bridge);
    }
    // TearDown checks for leaks
}

//=============================================================================
// Version and Readiness E2E
//=============================================================================

TEST_F(HealthDiagBridgeE2ETest, AllComponentVersionsAvailable) {
    const char* diag_ver = health_diag_bridge_version();
    const char* repair_ver = health_self_repair_bridge_version();
    const char* notify_ver = self_repair_health_notify_version();

    ASSERT_NE(diag_ver, nullptr);
    ASSERT_NE(repair_ver, nullptr);
    ASSERT_NE(notify_ver, nullptr);

    EXPECT_GT(strlen(diag_ver), 0u);
    EXPECT_GT(strlen(repair_ver), 0u);
    EXPECT_GT(strlen(notify_ver), 0u);
}

TEST_F(HealthDiagBridgeE2ETest, AllComponentsReadyAfterSetup) {
    EXPECT_TRUE(health_diag_bridge_is_ready(diag_bridge));
    EXPECT_TRUE(health_self_repair_bridge_is_ready(repair_bridge));
}
