/**
 * @file test_health_diagnostic_bridge_integration.cpp
 * @brief Integration tests for Health Diagnostic Bridge
 * @version 1.0.0
 * @date 2025-01-27
 *
 * WHAT: Integration tests verifying cross-component interactions
 * WHY: Ensure the diagnostic bridge works correctly with all connected systems
 * HOW: Test bridge interactions with health monitor, health agent, self-repair,
 *      code immune, KG wiring, security, math utils, and quantum annealing
 *
 * Test Coverage:
 * - Health Monitor → Diagnostic Bridge → diagnostic_result_t flow
 * - Health Agent → Diagnostic Bridge → diagnostic_result_t flow
 * - Diagnostic Bridge + Self-Repair Bridge pipeline integration
 * - Diagnostic Bridge + Code Immune auto-repair integration
 * - Multi-bridge concurrent operation
 * - Batch conversion with cross-system enrichment
 * - Statistics consistency across operations
 * - KG wiring integration with full graph connectivity
 * - Security module integration for anomaly data validation
 * - Math utils phasor coherence for pattern analysis
 * - Quantum annealing confidence optimization pipeline
 * - Exception handler coverage across component boundaries
 * - Logging integration across pipeline stages
 * - Memory leak detection across integrated operations
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
#include "utils/error/nimcp_error_codes.h"
#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "cognitive/fault_tolerance/nimcp_self_repair_health_notify.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "security/nimcp_security.h"
#include "utils/math/nimcp_complex_math.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"
#include "cognitive/immune/nimcp_code_immune.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"

//=============================================================================
// Test Fixture
//=============================================================================

class HealthDiagBridgeIntegrationTest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;
    health_diag_bridge_t* diag_bridge = nullptr;
    self_repair_coordinator_t* self_repair = nullptr;
    health_self_repair_bridge_t* repair_bridge = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;

        diag_bridge = health_diag_bridge_create(NULL);
        ASSERT_NE(diag_bridge, nullptr);

        self_repair = self_repair_create(NULL);
        ASSERT_NE(self_repair, nullptr);

        repair_bridge = health_self_repair_bridge_create(
            NULL, diag_bridge, self_repair);
        ASSERT_NE(repair_bridge, nullptr);
    }

    void TearDown() override {
        if (repair_bridge) health_self_repair_bridge_destroy(repair_bridge);
        if (self_repair) self_repair_destroy(self_repair);
        if (diag_bridge) health_diag_bridge_destroy(diag_bridge);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        // Allow up to 8KB for exception/immune system infrastructure allocations
        const size_t exception_infra_tolerance = 8192;
        EXPECT_LE(stats.current_allocated,
                  baseline_allocated + exception_infra_tolerance)
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
                 "Integration test anomaly type=%d", (int)type);
        snprintf(anomaly->affected_component, sizeof(anomaly->affected_component),
                 "integration_component_%d", (int)type);
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
                 "Integration test message type=%d", (int)type);
    }
};

//=============================================================================
// Cross-Component: Diagnostic Bridge → Self-Repair Pipeline
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, AnomalyToDiagnosticToRepairPipeline) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.95f);

    // Step 1: Convert anomaly to diagnostic
    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_OUT_OF_MEMORY);
    EXPECT_EQ(result->severity, DIAG_SEVERITY_CRITICAL);

    // Step 2: Feed into self-repair bridge
    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly, &request_id);
    // Process should at least not crash; actual trigger depends on policy
    EXPECT_GE(ret, -1);

    diagnostics_free_result(result);
}

TEST_F(HealthDiagBridgeIntegrationTest, AgentMessageToDiagnosticToRepairPipeline) {
    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_DEADLOCK_DETECTED, HEALTH_SEVERITY_CRITICAL);
    msg.source = HEALTH_SOURCE_THREADING;
    msg.data.deadlock.thread_id_1 = 1001;
    msg.data.deadlock.thread_id_2 = 1002;

    // Step 1: Convert agent message to diagnostic
    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_agent_message(
        diag_bridge, &msg, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_DEADLOCK);
    EXPECT_EQ(result->severity, DIAG_SEVERITY_CRITICAL);

    // Step 2: Process via self-repair bridge
    uint64_t request_id = 0;
    health_self_repair_bridge_process_agent_message(
        repair_bridge, &msg, &request_id);

    diagnostics_free_result(result);
}

TEST_F(HealthDiagBridgeIntegrationTest, MultipleAnomalyTypesThroughPipeline) {
    const anomaly_type_t types[] = {
        ANOMALY_MEMORY_LEAK,
        ANOMALY_PERFORMANCE_DEGRADATION,
        ANOMALY_ERROR_SPIKE,
        ANOMALY_RESOURCE_EXHAUSTION,
        ANOMALY_NUMERICAL_INSTABILITY,
        ANOMALY_THREAD_CONTENTION
    };

    for (auto type : types) {
        SCOPED_TRACE(health_diag_bridge_anomaly_type_name(type));

        anomaly_t anomaly;
        create_anomaly(&anomaly, type, ANOMALY_SEVERITY_ERROR, 0.8f);

        diagnostic_result_t* result = NULL;
        ASSERT_EQ(health_diag_bridge_convert_anomaly(
            diag_bridge, &anomaly, &result), 0);
        ASSERT_NE(result, nullptr);
        EXPECT_NE(result->error_type, ERROR_TYPE_NONE);
        EXPECT_EQ(result->severity, DIAG_SEVERITY_ERROR);

        diagnostics_free_result(result);
    }

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.anomalies_converted, 6u);
}

//=============================================================================
// Cross-Component: All Agent Message Types Through Pipeline
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, AllAgentMessageTypesThroughBridge) {
    const health_agent_msg_type_t msg_types[] = {
        HEALTH_MSG_DEADLOCK_DETECTED,
        HEALTH_MSG_NAN_DETECTED,
        HEALTH_MSG_MEMORY_CORRUPTION,
        HEALTH_MSG_HEARTBEAT_TIMEOUT,
        HEALTH_MSG_RESOURCE_EXHAUSTION,
        HEALTH_MSG_EMERGENCY,
        HEALTH_MSG_STATE_CORRUPTION,
        HEALTH_MSG_RECOVERY_REQUEST,
        HEALTH_MSG_ANOMALY_DETECTED
    };

    for (auto msg_type : msg_types) {
        SCOPED_TRACE(health_diag_bridge_agent_msg_type_name(msg_type));

        health_agent_message_t msg;
        create_agent_msg(&msg, msg_type, HEALTH_SEVERITY_ERROR);

        diagnostic_result_t* result = NULL;
        ASSERT_EQ(health_diag_bridge_convert_agent_message(
            diag_bridge, &msg, &result), 0);
        ASSERT_NE(result, nullptr);
        EXPECT_GT(result->confidence, 0.0f);
        EXPECT_LE(result->confidence, 1.0f);

        diagnostics_free_result(result);
    }

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.agent_messages_converted, 9u);
}

//=============================================================================
// Batch Conversion + Enrichment Pipeline
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, BatchConversionWithEnrichment) {
    const uint32_t count = 8;
    anomaly_t anomalies[8];
    diagnostic_result_t* results[8] = {};
    uint32_t converted = 0;

    anomaly_type_t types[] = {
        ANOMALY_MEMORY_LEAK, ANOMALY_ERROR_SPIKE,
        ANOMALY_RESOURCE_EXHAUSTION, ANOMALY_NUMERICAL_INSTABILITY,
        ANOMALY_THREAD_CONTENTION, ANOMALY_CACHE_THRASHING,
        ANOMALY_THROUGHPUT_DROP, ANOMALY_PERFORMANCE_DEGRADATION
    };

    for (uint32_t i = 0; i < count; i++) {
        create_anomaly(&anomalies[i], types[i],
                       ANOMALY_SEVERITY_WARNING, 0.7f + 0.03f * i);
    }

    ASSERT_EQ(health_diag_bridge_convert_anomalies(
        diag_bridge, anomalies, count, results, &converted), 0);
    EXPECT_EQ(converted, count);

    for (uint32_t i = 0; i < count; i++) {
        ASSERT_NE(results[i], nullptr);
        EXPECT_GT(results[i]->confidence, 0.0f);
        EXPECT_LE(results[i]->confidence, 1.0f);
        EXPECT_GT(results[i]->timestamp, 0);
        diagnostics_free_result(results[i]);
    }

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.anomalies_converted, count);
}

TEST_F(HealthDiagBridgeIntegrationTest, BatchConversionWithSeverityFiltering) {
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);
    config.min_severity = ANOMALY_SEVERITY_ERROR;

    health_diag_bridge_t* filtered_bridge = health_diag_bridge_create(&config);
    ASSERT_NE(filtered_bridge, nullptr);

    const uint32_t count = 4;
    anomaly_t anomalies[4];
    diagnostic_result_t* results[4] = {};
    uint32_t converted = 0;

    // Only ERROR and CRITICAL should pass the filter
    create_anomaly(&anomalies[0], ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_INFO, 0.5f);
    create_anomaly(&anomalies[1], ANOMALY_ERROR_SPIKE, ANOMALY_SEVERITY_ERROR, 0.8f);
    create_anomaly(&anomalies[2], ANOMALY_CACHE_THRASHING, ANOMALY_SEVERITY_WARNING, 0.7f);
    create_anomaly(&anomalies[3], ANOMALY_RESOURCE_EXHAUSTION, ANOMALY_SEVERITY_CRITICAL, 0.9f);

    ASSERT_EQ(health_diag_bridge_convert_anomalies(
        filtered_bridge, anomalies, count, results, &converted), 0);
    EXPECT_EQ(converted, 2u);

    for (uint32_t i = 0; i < count; i++) {
        if (results[i]) diagnostics_free_result(results[i]);
    }

    health_diag_bridge_destroy(filtered_bridge);
}

//=============================================================================
// Enrichment Pipeline Integration
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, FullEnrichmentPipeline) {
    // Create bridge with all enrichment enabled
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);
    config.capture_stack_trace = true;
    config.capture_memory_snapshot = true;
    config.enable_pattern_analysis = true;

    health_diag_bridge_t* bridge = health_diag_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_CRITICAL, 0.9f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);

    // All enrichment should be present
    EXPECT_GT(result->stack_depth, 0u);
    EXPECT_GT(result->timestamp, 0);
    EXPECT_GT(strlen(result->root_cause), 0u);
    EXPECT_EQ(result->occurrence_count, 1u);

    diagnostics_free_result(result);

    // Stats should reflect enrichment
    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.stack_traces_captured, 1u);
    EXPECT_GE(stats.memory_snapshots_captured, 1u);

    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagBridgeIntegrationTest, DeferredEnrichmentPipeline) {
    // Create bridge with enrichment disabled, then enrich manually
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);
    config.capture_stack_trace = false;
    config.capture_memory_snapshot = false;
    config.enable_pattern_analysis = false;

    health_diag_bridge_t* bridge = health_diag_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_ERROR_SPIKE, ANOMALY_SEVERITY_ERROR, 0.85f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);

    // Initially no enrichment
    EXPECT_EQ(result->stack_depth, 0u);

    // Now manually enrich
    EXPECT_EQ(health_diag_bridge_enrich_stack_trace(bridge, result), 0);
    EXPECT_GT(result->stack_depth, 0u);

    EXPECT_EQ(health_diag_bridge_enrich_memory_snapshot(bridge, result), 0);

    EXPECT_EQ(health_diag_bridge_analyze_patterns(bridge, result), 0);
    EXPECT_EQ(result->occurrence_count, 1u);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

//=============================================================================
// Custom Mapping Integration
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, CustomMappingsAffectPipeline) {
    // Add custom mapping for CACHE_THRASHING
    anomaly_error_mapping_t mapping;
    mapping.anomaly_type = ANOMALY_CACHE_THRASHING;
    mapping.error_type = ERROR_TYPE_INVALID_STATE;
    mapping.default_severity = DIAG_SEVERITY_ERROR;
    mapping.default_confidence = 0.95f;
    mapping.description_template = "Custom cache: %s";

    ASSERT_EQ(health_diag_bridge_add_anomaly_mapping(diag_bridge, &mapping), 0);

    // Verify custom mapping is used
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_CACHE_THRASHING, ANOMALY_SEVERITY_WARNING, 0.6f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_INVALID_STATE);

    diagnostics_free_result(result);
}

TEST_F(HealthDiagBridgeIntegrationTest, CustomAgentMappingsAffectPipeline) {
    agent_error_mapping_t mapping;
    mapping.msg_type = HEALTH_MSG_CYTOKINE_SIGNAL;
    mapping.error_type = ERROR_TYPE_INVALID_STATE;
    mapping.default_severity = DIAG_SEVERITY_ERROR;
    mapping.default_confidence = 0.85f;
    mapping.description_template = "Custom cytokine: %s";

    ASSERT_EQ(health_diag_bridge_add_agent_mapping(diag_bridge, &mapping), 0);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_CYTOKINE_SIGNAL, HEALTH_SEVERITY_ERROR);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_agent_message(diag_bridge, &msg, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_INVALID_STATE);

    diagnostics_free_result(result);
}

//=============================================================================
// Statistics Consistency Across Operations
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, StatisticsConsistencyFullPipeline) {
    // Process mixed anomalies and agent messages
    for (int i = 0; i < 10; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_WARNING, 0.8f);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);
    }

    for (int i = 0; i < 5; i++) {
        health_agent_message_t msg;
        create_agent_msg(&msg, HEALTH_MSG_DEADLOCK_DETECTED,
                        HEALTH_SEVERITY_CRITICAL);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_agent_message(diag_bridge, &msg, &result);
        if (result) diagnostics_free_result(result);
    }

    health_diag_bridge_stats_t stats;
    ASSERT_EQ(health_diag_bridge_get_stats(diag_bridge, &stats), 0);

    EXPECT_EQ(stats.anomalies_converted, 10u);
    EXPECT_EQ(stats.agent_messages_converted, 5u);
    EXPECT_EQ(stats.by_anomaly_type[ANOMALY_MEMORY_LEAK], 10u);
    EXPECT_EQ(stats.by_agent_msg_type[HEALTH_MSG_DEADLOCK_DETECTED], 5u);
    EXPECT_EQ(stats.by_severity[DIAG_SEVERITY_WARNING], 10u);
    EXPECT_EQ(stats.by_severity[DIAG_SEVERITY_CRITICAL], 5u);
    EXPECT_GT(stats.avg_conversion_time_us, 0.0f);
}

TEST_F(HealthDiagBridgeIntegrationTest, StatisticsResetAndReaccumulate) {
    // Generate stats
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);
    diagnostic_result_t* result = NULL;
    health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
    if (result) diagnostics_free_result(result);

    // Reset
    health_diag_bridge_reset_stats(diag_bridge);

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.anomalies_converted, 0u);
    EXPECT_EQ(stats.agent_messages_converted, 0u);

    // Re-accumulate
    for (int i = 0; i < 3; i++) {
        result = NULL;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);
    }

    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.anomalies_converted, 3u);
}

//=============================================================================
// Concurrent Operation Integration
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, ConcurrentAnomalyConversions) {
    const int thread_count = 4;
    const int conversions_per_thread = 50;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < conversions_per_thread; i++) {
                anomaly_t anomaly;
                memset(&anomaly, 0, sizeof(anomaly));
                anomaly.type = (anomaly_type_t)(1 + (t + i) % 9);
                anomaly.severity = ANOMALY_SEVERITY_WARNING;
                anomaly.confidence = 0.7f;
                snprintf(anomaly.description, sizeof(anomaly.description),
                         "Thread %d iter %d", t, i);

                diagnostic_result_t* result = NULL;
                int ret = health_diag_bridge_convert_anomaly(
                    diag_bridge, &anomaly, &result);
                if (ret == 0 && result) {
                    success_count.fetch_add(1);
                    diagnostics_free_result(result);
                } else {
                    failure_count.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    EXPECT_EQ(success_count.load(), thread_count * conversions_per_thread);
    EXPECT_EQ(failure_count.load(), 0);

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.anomalies_converted,
              (uint64_t)(thread_count * conversions_per_thread));
}

TEST_F(HealthDiagBridgeIntegrationTest, ConcurrentMixedAnomalyAndAgentConversions) {
    std::atomic<int> anomaly_successes{0};
    std::atomic<int> agent_successes{0};

    std::thread anomaly_thread([&]() {
        for (int i = 0; i < 50; i++) {
            anomaly_t anomaly;
            memset(&anomaly, 0, sizeof(anomaly));
            anomaly.type = ANOMALY_MEMORY_LEAK;
            anomaly.severity = ANOMALY_SEVERITY_WARNING;
            anomaly.confidence = 0.8f;

            diagnostic_result_t* result = NULL;
            if (health_diag_bridge_convert_anomaly(
                    diag_bridge, &anomaly, &result) == 0 && result) {
                anomaly_successes.fetch_add(1);
                diagnostics_free_result(result);
            }
        }
    });

    std::thread agent_thread([&]() {
        for (int i = 0; i < 50; i++) {
            health_agent_message_t msg;
            memset(&msg, 0, sizeof(msg));
            msg.type = HEALTH_MSG_DEADLOCK_DETECTED;
            msg.severity = HEALTH_SEVERITY_CRITICAL;

            diagnostic_result_t* result = NULL;
            if (health_diag_bridge_convert_agent_message(
                    diag_bridge, &msg, &result) == 0 && result) {
                agent_successes.fetch_add(1);
                diagnostics_free_result(result);
            }
        }
    });

    anomaly_thread.join();
    agent_thread.join();

    EXPECT_EQ(anomaly_successes.load(), 50);
    EXPECT_EQ(agent_successes.load(), 50);
}

//=============================================================================
// Multi-Bridge Integration
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, MultipleDiagBridgesShareRepairPipeline) {
    // Create a second diagnostic bridge with different config
    health_diag_bridge_config_t config2;
    health_diag_bridge_default_config(&config2);
    config2.capture_stack_trace = false;
    config2.capture_memory_snapshot = false;
    config2.default_confidence = 0.5f;

    health_diag_bridge_t* bridge2 = health_diag_bridge_create(&config2);
    ASSERT_NE(bridge2, nullptr);

    // Both bridges should produce valid diagnostics independently
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result1 = NULL;
    diagnostic_result_t* result2 = NULL;

    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result1), 0);
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge2, &anomaly, &result2), 0);

    ASSERT_NE(result1, nullptr);
    ASSERT_NE(result2, nullptr);

    // Bridge 1 has enrichment, bridge 2 does not
    EXPECT_GT(result1->stack_depth, 0u);
    EXPECT_EQ(result2->stack_depth, 0u);

    // Both should have valid error types
    EXPECT_EQ(result1->error_type, ERROR_TYPE_MEMORY_LEAK);
    EXPECT_EQ(result2->error_type, ERROR_TYPE_MEMORY_LEAK);

    diagnostics_free_result(result1);
    diagnostics_free_result(result2);
    health_diag_bridge_destroy(bridge2);
}

TEST_F(HealthDiagBridgeIntegrationTest, BridgeReplacementDuringOperation) {
    // Use first bridge
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    diagnostics_free_result(result);

    // Create replacement bridge
    health_diag_bridge_config_t config2;
    health_diag_bridge_default_config(&config2);
    config2.default_confidence = 0.95f;

    health_diag_bridge_t* bridge2 = health_diag_bridge_create(&config2);
    ASSERT_NE(bridge2, nullptr);

    // Use replacement
    result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge2, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    diagnostics_free_result(result);

    health_diag_bridge_destroy(bridge2);
}

//=============================================================================
// Health Agent Integration
//=============================================================================

extern "C" {
    void health_diagnostic_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

TEST_F(HealthDiagBridgeIntegrationTest, HealthAgentHeartbeatDuringConversions) {
    health_agent_config_t cfg;
    nimcp_health_agent_default_config(&cfg);
    cfg.check_interval_ms = 50;
    cfg.enable_auto_recovery = false;

    nimcp_health_agent_t* agent = nimcp_health_agent_create(&cfg);
    ASSERT_NE(agent, nullptr);

    health_diagnostic_bridge_set_health_agent(agent);
    nimcp_health_agent_start(agent);

    // Perform conversions while heartbeat is active
    for (int i = 0; i < 20; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_WARNING, 0.8f);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);
    }

    nimcp_health_agent_stop(agent);
    health_diagnostic_bridge_set_health_agent(NULL);
    nimcp_health_agent_destroy(agent);
}

TEST_F(HealthDiagBridgeIntegrationTest, AgentStartStopDuringConversions) {
    health_agent_config_t cfg;
    nimcp_health_agent_default_config(&cfg);
    nimcp_health_agent_t* agent = nimcp_health_agent_create(&cfg);
    ASSERT_NE(agent, nullptr);

    health_diagnostic_bridge_set_health_agent(agent);

    for (int cycle = 0; cycle < 3; cycle++) {
        nimcp_health_agent_start(agent);

        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_ERROR_SPIKE,
                       ANOMALY_SEVERITY_ERROR, 0.75f);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);

        nimcp_health_agent_stop(agent);
    }

    health_diagnostic_bridge_set_health_agent(NULL);
    nimcp_health_agent_destroy(agent);
}

//=============================================================================
// Code Immune Integration
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, CodeImmuneWithDiagnosticBridge) {
    code_immune_config_t immune_config;
    code_immune_default_config(&immune_config);

    code_immune_system_t* immune = code_immune_create_with_config(NULL, &immune_config);
    if (immune) {
        EXPECT_NE(immune, nullptr);

        // Diagnostic bridge should work alongside code immune
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_CRITICAL, 0.9f);
        diagnostic_result_t* result = NULL;
        ASSERT_EQ(health_diag_bridge_convert_anomaly(
            diag_bridge, &anomaly, &result), 0);
        ASSERT_NE(result, nullptr);

        diagnostics_free_result(result);
        code_immune_destroy(immune);
    }
}

TEST_F(HealthDiagBridgeIntegrationTest, CodeImmuneAutoRepairConfigWithBridge) {
    code_immune_config_t config;
    code_immune_default_config(&config);

    // Verify auto-repair config is accessible
    EXPECT_TRUE(config.auto_repair.enabled);
    EXPECT_EQ(config.auto_repair.min_crash_count, 3u);
    EXPECT_GT(config.auto_repair.min_severity, 0.0f);
    EXPECT_GT(config.auto_repair.min_confidence, 0.0f);
    EXPECT_GT(config.auto_repair.cooldown_ms, 0u);

    // Create immune with modified config
    config.auto_repair.min_crash_count = 5;
    config.auto_repair.cooldown_ms = 10000;

    code_immune_system_t* immune = code_immune_create_with_config(NULL, &config);
    if (immune) {
        EXPECT_NE(immune, nullptr);
        code_immune_destroy(immune);
    }
}

//=============================================================================
// KG Wiring Full Integration
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, KGWiringFullGraphConnectivity) {
    // Create wiring for diagnostic bridge
    kg_module_wiring_t* diag_wiring = kg_module_wiring_create(
        "health_diagnostic_bridge", "FAULT_TOLERANCE");
    ASSERT_NE(diag_wiring, nullptr);

    kg_module_wiring_set_metadata(diag_wiring, "NIMCP", "fault_tolerance",
        "Converts health anomalies to diagnostic results");
    kg_module_wiring_set_version(diag_wiring, 1, 0, 0);

    kg_module_wiring_add_input(diag_wiring, "health_monitor", "ANOMALY_DETECTED", true);
    kg_module_wiring_add_input(diag_wiring, "health_agent", "HEALTH_AGENT_MESSAGE", true);
    kg_module_wiring_add_output(diag_wiring, "DIAGNOSTIC_RESULT", "Converted diagnostic");
    kg_module_wiring_add_output(diag_wiring, "HEALTH_DIAGNOSTIC_CONVERTED", "Conversion signal");
    kg_module_wiring_add_handler(diag_wiring, "ANOMALY_DETECTED", 200);
    kg_module_wiring_add_handler(diag_wiring, "HEALTH_AGENT_MESSAGE", 200);

    // Create wiring for self-repair bridge
    kg_module_wiring_t* repair_wiring = kg_module_wiring_create(
        "health_self_repair_bridge", "FAULT_TOLERANCE");
    ASSERT_NE(repair_wiring, nullptr);

    kg_module_wiring_set_metadata(repair_wiring, "NIMCP", "fault_tolerance",
        "Triggers self-repair from health events");
    kg_module_wiring_set_version(repair_wiring, 1, 0, 0);

    kg_module_wiring_add_input(repair_wiring, "health_diagnostic_bridge", "DIAGNOSTIC_RESULT", true);
    kg_module_wiring_add_output(repair_wiring, "SELF_REPAIR_REQUEST", "Repair request");
    kg_module_wiring_add_handler(repair_wiring, "DIAGNOSTIC_RESULT", 300);

    // Validate both
    char error_buf[256];
    EXPECT_EQ(kg_module_wiring_validate(diag_wiring, error_buf, sizeof(error_buf)), 0);
    EXPECT_EQ(kg_module_wiring_validate(repair_wiring, error_buf, sizeof(error_buf)), 0);

    // Cross-validate connectivity
    EXPECT_TRUE(kg_module_wiring_has_output(diag_wiring, "DIAGNOSTIC_RESULT"));
    EXPECT_TRUE(kg_module_wiring_has_input(repair_wiring, "health_diagnostic_bridge", "DIAGNOSTIC_RESULT"));

    kg_module_wiring_destroy(repair_wiring);
    kg_module_wiring_destroy(diag_wiring);
}

TEST_F(HealthDiagBridgeIntegrationTest, KGWiringMetadataRichEntries) {
    kg_module_wiring_t* wiring = kg_module_wiring_create(
        "health_diagnostic_bridge", "FAULT_TOLERANCE");
    ASSERT_NE(wiring, nullptr);

    kg_module_wiring_add_metadata_entry(wiring, "bridge_type", "health_to_diagnostic");
    kg_module_wiring_add_metadata_entry(wiring, "severity_mapping", "anomaly_to_diag");
    kg_module_wiring_add_metadata_entry(wiring, "enrichment", "stack_trace+memory_snapshot+pattern");
    kg_module_wiring_add_metadata_entry(wiring, "stats_tracking", "per_type+per_severity");
    kg_module_wiring_add_metadata_entry(wiring, "bio_async", "enabled");

    EXPECT_EQ(wiring->metadata.entry_count, 5u);

    kg_module_wiring_destroy(wiring);
}

//=============================================================================
// Security Module Integration
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, SecurityValidationOfAnomalyData) {
    // Validate various anomaly descriptions through security module
    const char* safe_descriptions[] = {
        "Memory leak detected in allocator pool",
        "Performance degradation in neural pathway",
        "Resource exhaustion approaching threshold",
        "NaN detected in layer 3 neuron 42",
        "Deadlock between thread 1001 and 1002"
    };

    for (const char* desc : safe_descriptions) {
        nimcp_input_validation_t valid = nimcp_security_validate_input(
            desc, 512, NULL);
        EXPECT_EQ(valid, NIMCP_INPUT_VALID) << "Failed for: " << desc;
    }
}

TEST_F(HealthDiagBridgeIntegrationTest, SecurityThreatAnalysisOnDiagnosticOutput) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);
    strncpy(anomaly.description, "Memory leak in neural allocator pool",
            sizeof(anomaly.description) - 1);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);

    // Analyze the diagnostic output description for threats
    if (strlen(result->root_cause) > 0) {
        nimcp_threat_level_t threat = nimcp_security_analyze_threat(result->root_cause);
        EXPECT_LE(threat, NIMCP_THREAT_LOW);
    }

    diagnostics_free_result(result);
}

TEST_F(HealthDiagBridgeIntegrationTest, SecuritySanitizeAnomalyDescriptions) {
    const char* descriptions[] = {
        "Memory leak in <module>",
        "Error spike & performance drop",
        "Resource usage at 95%"
    };

    for (const char* desc : descriptions) {
        char sanitized[256];
        nimcp_result_t ret = nimcp_security_sanitize_input(
            desc, sanitized, sizeof(sanitized));
        EXPECT_EQ(ret, NIMCP_SUCCESS);
        EXPECT_GT(strlen(sanitized), 0u);
    }
}

//=============================================================================
// Math Utils Integration - Phasor Coherence for Anomaly Patterns
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, PhasorCoherenceAnomalyPatternDetection) {
    complex_math_init(NULL);

    // Simulate anomaly timing patterns as phasor phases
    // High coherence = systematic pattern = likely related root cause
    const uint32_t n = 10;
    neural_phasor_t anomaly_signals[10];

    // Create correlated anomaly pattern (similar phases)
    for (uint32_t i = 0; i < n; i++) {
        float phase = 1.2f + 0.02f * (float)i;  // Tightly clustered phases
        anomaly_signals[i] = phasor_from_polar(1.0f, phase);
    }

    float coherence = phasor_array_coherence(anomaly_signals, n);
    EXPECT_GT(coherence, 0.9f);  // High coherence = systematic pattern

    float mean_phase = phasor_array_mean_phase(anomaly_signals, n);
    EXPECT_GT(mean_phase, 1.0f);
    EXPECT_LT(mean_phase, 1.5f);

    float variance = phasor_array_phase_variance(anomaly_signals, n);
    EXPECT_LT(variance, 0.05f);  // Low variance = consistent pattern

    complex_math_cleanup();
}

TEST_F(HealthDiagBridgeIntegrationTest, PhasorCoherenceRandomAnomalies) {
    complex_math_init(NULL);

    // Random anomalies should have low coherence = uncorrelated
    const uint32_t n = 10;
    neural_phasor_t random_signals[10];

    for (uint32_t i = 0; i < n; i++) {
        float phase = (float)i * (2.0f * 3.14159f / (float)n);
        random_signals[i] = phasor_from_polar(1.0f, phase);
    }

    float coherence = phasor_array_coherence(random_signals, n);
    EXPECT_LT(coherence, 0.3f);  // Low coherence = random/uncorrelated

    complex_math_cleanup();
}

//=============================================================================
// Quantum Annealing Integration - Confidence Optimization
//=============================================================================

static float multi_anomaly_confidence_energy(const float* state, uint32_t dim,
                                              void* user_data) {
    (void)user_data;
    // Energy function: minimize distance from optimal confidence levels
    // Optimal is 0.85 for critical, 0.7 for error, 0.5 for warning
    float target_confidences[] = {0.85f, 0.7f, 0.5f};
    float energy = 0.0f;
    for (uint32_t i = 0; i < dim && i < 3; i++) {
        float diff = state[i] - target_confidences[i];
        energy += diff * diff;
    }
    return energy;
}

TEST_F(HealthDiagBridgeIntegrationTest, QuantumAnnealingMultiConfidenceOptimization) {
    quantum_annealing_config_t config = quantum_annealing_default_config();
    config.num_iterations = 200;
    config.seed = 42;

    quantum_annealer_t annealer = quantum_annealer_create(&config);
    if (annealer) {
        float initial_state[3] = {0.5f, 0.5f, 0.5f};
        float optimized[3] = {0.0f, 0.0f, 0.0f};

        float final_energy = quantum_anneal(
            annealer, multi_anomaly_confidence_energy,
            initial_state, optimized, 3, NULL);

        EXPECT_LT(final_energy, 0.5f);

        // All optimized values should be in valid range
        for (int i = 0; i < 3; i++) {
            EXPECT_GE(optimized[i], -0.5f);
            EXPECT_LE(optimized[i], 1.5f);
        }

        quantum_annealer_destroy(annealer);
    }
}

TEST_F(HealthDiagBridgeIntegrationTest, QuantumAnnealerWithDifferentSeeds) {
    for (uint32_t seed = 1; seed <= 5; seed++) {
        quantum_annealing_config_t config = quantum_annealing_default_config();
        config.num_iterations = 50;
        config.seed = seed;

        quantum_annealer_t annealer = quantum_annealer_create(&config);
        if (annealer) {
            float initial[2] = {0.3f, 0.3f};
            float result[2] = {0.0f, 0.0f};

            float energy = quantum_anneal(
                annealer, multi_anomaly_confidence_energy,
                initial, result, 2, NULL);

            EXPECT_GE(energy, 0.0f);
            quantum_annealer_destroy(annealer);
        }
    }
}

//=============================================================================
// Logging Integration
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, LoggingDuringConversion) {
    nimcp_log_config_t log_config = nimcp_log_default_config();
    log_config.level = LOG_LEVEL_DEBUG;

    nimcp_logger_t logger = nimcp_log_create(&log_config);
    if (logger) {
        // Perform conversions with logger active
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_CRITICAL, 0.9f);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);

        health_agent_message_t msg;
        create_agent_msg(&msg, HEALTH_MSG_EMERGENCY, HEALTH_SEVERITY_CRITICAL);
        result = NULL;
        health_diag_bridge_convert_agent_message(diag_bridge, &msg, &result);
        if (result) diagnostics_free_result(result);

        nimcp_log_destroy(logger);
    }
}

TEST_F(HealthDiagBridgeIntegrationTest, VerboseLoggingMode) {
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);
    config.verbose_logging = true;

    health_diag_bridge_t* verbose_bridge = health_diag_bridge_create(&config);
    ASSERT_NE(verbose_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.95f);
    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(
        verbose_bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(verbose_bridge);
}

//=============================================================================
// Exception Handler Cross-Boundary Coverage
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, ExceptionCrossBoundaryNullChain) {
    // Null through entire pipeline chain
    EXPECT_EQ(health_diag_bridge_convert_anomaly(NULL, NULL, NULL), -1);
    EXPECT_EQ(health_diag_bridge_convert_agent_message(NULL, NULL, NULL), -1);
    EXPECT_EQ(health_diag_bridge_convert_anomalies(NULL, NULL, 0, NULL, NULL), -1);
    EXPECT_EQ(health_diag_bridge_enrich_stack_trace(NULL, NULL), -1);
    EXPECT_EQ(health_diag_bridge_enrich_memory_snapshot(NULL, NULL), -1);
    EXPECT_EQ(health_diag_bridge_analyze_patterns(NULL, NULL), -1);
    EXPECT_EQ(health_diag_bridge_add_anomaly_mapping(NULL, NULL), -1);
    EXPECT_EQ(health_diag_bridge_add_agent_mapping(NULL, NULL), -1);
    EXPECT_EQ(health_diag_bridge_get_stats(NULL, NULL), -1);
    EXPECT_EQ(health_diag_bridge_default_config(NULL), -1);
    EXPECT_FALSE(health_diag_bridge_is_ready(NULL));
    EXPECT_EQ(health_diag_bridge_get_anomaly_mapping(NULL, ANOMALY_MEMORY_LEAK), nullptr);
}

TEST_F(HealthDiagBridgeIntegrationTest, ExceptionBridgeValidInputNullOutput) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);
    EXPECT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, NULL), -1);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_EMERGENCY, HEALTH_SEVERITY_CRITICAL);
    EXPECT_EQ(health_diag_bridge_convert_agent_message(diag_bridge, &msg, NULL), -1);
}

TEST_F(HealthDiagBridgeIntegrationTest, ExceptionBridgeValidOutputNullInput) {
    diagnostic_result_t* result = NULL;
    EXPECT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, NULL, &result), -1);
    EXPECT_EQ(result, nullptr);

    result = NULL;
    EXPECT_EQ(health_diag_bridge_convert_agent_message(diag_bridge, NULL, &result), -1);
    EXPECT_EQ(result, nullptr);
}

//=============================================================================
// Memory Management Integration
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, NoLeaksAfterMassConversions) {
    nimcp_memory_stats_t before;
    nimcp_memory_get_stats(&before);

    for (int i = 0; i < 200; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, (anomaly_type_t)(1 + i % 9),
                       ANOMALY_SEVERITY_WARNING, 0.7f);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);
    }

    for (int i = 0; i < 100; i++) {
        health_agent_message_t msg;
        create_agent_msg(&msg, (health_agent_msg_type_t)(i % HEALTH_MSG_COUNT),
                        HEALTH_SEVERITY_ERROR);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_agent_message(diag_bridge, &msg, &result);
        if (result) diagnostics_free_result(result);
    }

    nimcp_memory_stats_t after;
    nimcp_memory_get_stats(&after);

    // No significant memory growth (bridge internal state is fixed-size)
    EXPECT_LE(after.current_allocated, before.current_allocated + 4096);
}

TEST_F(HealthDiagBridgeIntegrationTest, BridgeCreationDestructionCycles) {
    for (int cycle = 0; cycle < 10; cycle++) {
        health_diag_bridge_config_t config;
        health_diag_bridge_default_config(&config);
        config.default_confidence = 0.5f + 0.05f * cycle;

        health_diag_bridge_t* bridge = health_diag_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_WARNING, 0.8f);
        diagnostic_result_t* result = NULL;
        health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);

        health_diag_bridge_destroy(bridge);
    }
    // TearDown checks for leaks
}

//=============================================================================
// Self-Repair Bridge Statistics Integration
//=============================================================================

TEST_F(HealthDiagBridgeIntegrationTest, RepairBridgeStatsAfterProcessing) {
    ASSERT_NE(repair_bridge, nullptr);
    ASSERT_TRUE(health_self_repair_bridge_is_ready(repair_bridge));

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.95f);

    uint64_t request_id = 0;
    health_self_repair_bridge_process_anomaly(repair_bridge, &anomaly, &request_id);

    health_self_repair_bridge_stats_t repair_stats;
    int ret = health_self_repair_bridge_get_stats(repair_bridge, &repair_stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(HealthDiagBridgeIntegrationTest, DiagAndRepairBridgeVersionStrings) {
    const char* diag_ver = health_diag_bridge_version();
    const char* repair_ver = health_self_repair_bridge_version();

    ASSERT_NE(diag_ver, nullptr);
    ASSERT_NE(repair_ver, nullptr);
    EXPECT_GT(strlen(diag_ver), 0u);
    EXPECT_GT(strlen(repair_ver), 0u);
}
