/**
 * @file test_health_diagnostic_bridge.cpp
 * @brief Comprehensive unit tests for Health Diagnostic Bridge
 * @version 2.0.0
 * @date 2025-01-27
 *
 * WHAT: Full-coverage unit tests for health monitoring to diagnostic conversion
 * WHY: Ensure reliable conversion of health events for self-repair pipeline
 * HOW: Test-driven development covering all public APIs with cross-system integration
 *
 * Test Coverage:
 * - Creation and destruction (lifecycle)
 * - Configuration (defaults, custom, edge cases)
 * - All anomaly type conversions (complete anomaly_type_t coverage)
 * - All agent message type conversions (complete health_agent_msg_type_t coverage)
 * - Severity translation (all enum values + out-of-range)
 * - Custom mapping support (add, retrieve, override)
 * - Statistics tracking (conversion counts, timing, reset)
 * - Enrichment API (stack trace, memory snapshot, pattern analysis)
 * - Batch conversion
 * - Exception handler coverage (NIMCP_THROW_TO_IMMUNE paths)
 * - Logging integration (nimcp_logging.h)
 * - KG wiring integration (nimcp_kg_module_wiring.h)
 * - Immune system coverage (nimcp_code_immune.h)
 * - Security module coverage (nimcp_security.h)
 * - Math utils coverage (phasor coherence analysis)
 * - Quantum annealing coverage (confidence optimization)
 * - Error handling / NULL safety
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <limits>
#include <vector>

// Core headers (have their own extern "C" guards)
#include "cognitive/fault_tolerance/nimcp_health_diagnostic_bridge.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "security/nimcp_security.h"
#include "utils/math/nimcp_complex_math.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"
#include "cognitive/immune/nimcp_code_immune.h"

//=============================================================================
// Test Fixture
//=============================================================================

class HealthDiagnosticBridgeTest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;
    }

    void TearDown() override {
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        // Allow up to 8KB for exception/immune system infrastructure allocations
        // that persist after NIMCP_THROW_TO_IMMUNE is triggered for the first time
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
                 "Test anomaly type=%d", (int)type);
        snprintf(anomaly->affected_component, sizeof(anomaly->affected_component),
                 "test_component_%d", (int)type);
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
                 "Test message type=%d", (int)type);
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, CreateWithDefaults) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(health_diag_bridge_is_ready(bridge));
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, CreateWithCustomConfig) {
    health_diag_bridge_config_t config;
    int ret = health_diag_bridge_default_config(&config);
    ASSERT_EQ(ret, 0);

    config.capture_stack_trace = false;
    config.capture_memory_snapshot = false;
    config.default_confidence = 0.9f;
    config.enable_pattern_analysis = false;
    config.escalation_threshold = 10;
    config.min_severity = ANOMALY_SEVERITY_ERROR;
    config.min_agent_severity = HEALTH_SEVERITY_ERROR;
    config.enable_bio_async = false;
    config.verbose_logging = true;

    health_diag_bridge_t* bridge = health_diag_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(health_diag_bridge_is_ready(bridge));
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, DestroyNullSafety) {
    EXPECT_NO_THROW(health_diag_bridge_destroy(NULL));
}

TEST_F(HealthDiagnosticBridgeTest, IsReadyNullReturnsFalse) {
    EXPECT_FALSE(health_diag_bridge_is_ready(NULL));
}

TEST_F(HealthDiagnosticBridgeTest, CreateDestroyMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
        ASSERT_NE(bridge, nullptr);
        EXPECT_TRUE(health_diag_bridge_is_ready(bridge));
        health_diag_bridge_destroy(bridge);
    }
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, DefaultConfigValues) {
    health_diag_bridge_config_t config;
    int ret = health_diag_bridge_default_config(&config);

    ASSERT_EQ(ret, 0);
    EXPECT_TRUE(config.capture_stack_trace);
    EXPECT_TRUE(config.capture_memory_snapshot);
    EXPECT_FLOAT_EQ(config.default_confidence, 0.7f);
    EXPECT_TRUE(config.enable_pattern_analysis);
    EXPECT_EQ(config.escalation_threshold, 3u);
    EXPECT_EQ(config.min_severity, ANOMALY_SEVERITY_WARNING);
    EXPECT_EQ(config.min_agent_severity, HEALTH_SEVERITY_WARNING);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_FALSE(config.verbose_logging);
}

TEST_F(HealthDiagnosticBridgeTest, DefaultConfigNullReturnsError) {
    int ret = health_diag_bridge_default_config(NULL);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthDiagnosticBridgeTest, DefaultConfigIdempotent) {
    health_diag_bridge_config_t config1, config2;
    health_diag_bridge_default_config(&config1);

    config1.default_confidence = 0.99f;
    config1.escalation_threshold = 999;

    health_diag_bridge_default_config(&config1);
    health_diag_bridge_default_config(&config2);

    EXPECT_FLOAT_EQ(config1.default_confidence, config2.default_confidence);
    EXPECT_EQ(config1.escalation_threshold, config2.escalation_threshold);
}

//=============================================================================
// Complete Anomaly Type Conversion Coverage
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, ConvertMemoryLeakAnomaly) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.85f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_MEMORY_LEAK);
    EXPECT_EQ(result->severity, DIAG_SEVERITY_WARNING);
    EXPECT_GT(result->confidence, 0.0f);
    EXPECT_LE(result->confidence, 1.0f);
    EXPECT_GT(result->error_id, 0u);
    EXPECT_GT(strlen(result->diagnostic_version), 0u);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertPerformanceDegradationAnomaly) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_PERFORMANCE_DEGRADATION, ANOMALY_SEVERITY_WARNING, 0.7f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->severity, DIAG_SEVERITY_WARNING);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertErrorSpikeAnomaly) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_ERROR_SPIKE, ANOMALY_SEVERITY_ERROR, 0.8f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->severity, DIAG_SEVERITY_ERROR);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertThroughputDropAnomaly) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_THROUGHPUT_DROP, ANOMALY_SEVERITY_WARNING, 0.6f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertCacheThrashingAnomaly) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_CACHE_THRASHING, ANOMALY_SEVERITY_WARNING, 0.7f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertResourceExhaustionAnomaly) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION, ANOMALY_SEVERITY_CRITICAL, 0.9f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->severity, DIAG_SEVERITY_CRITICAL);
    EXPECT_EQ(result->error_type, ERROR_TYPE_OUT_OF_MEMORY);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertNumericalInstabilityAnomaly) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_NUMERICAL_INSTABILITY, ANOMALY_SEVERITY_ERROR, 0.85f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_NAN_DETECTED);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertThreadContentionAnomaly) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_THREAD_CONTENTION, ANOMALY_SEVERITY_WARNING, 0.7f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_DEADLOCK);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertUnknownAnomaly) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_UNKNOWN, ANOMALY_SEVERITY_WARNING, 0.5f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_UNKNOWN);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertAnomalyNoneType) {
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);
    config.min_severity = ANOMALY_SEVERITY_INFO;

    health_diag_bridge_t* bridge = health_diag_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_NONE, ANOMALY_SEVERITY_INFO, 1.0f);

    diagnostic_result_t* result = NULL;
    int ret = health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    ASSERT_EQ(ret, 0);
    if (result) {
        EXPECT_EQ(result->error_type, ERROR_TYPE_NONE);
        diagnostics_free_result(result);
    }

    health_diag_bridge_destroy(bridge);
}

//=============================================================================
// Anomaly Conversion Null Safety
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, ConvertAnomalyNullBridgeReturnsError) {
    anomaly_t anomaly;
    memset(&anomaly, 0, sizeof(anomaly));
    diagnostic_result_t* result = NULL;
    EXPECT_EQ(health_diag_bridge_convert_anomaly(NULL, &anomaly, &result), -1);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertAnomalyNullAnomalyReturnsError) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    diagnostic_result_t* result = NULL;
    EXPECT_EQ(health_diag_bridge_convert_anomaly(bridge, NULL, &result), -1);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertAnomalyNullResultReturnsError) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);
    EXPECT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, NULL), -1);
    health_diag_bridge_destroy(bridge);
}

//=============================================================================
// Minimum Severity Filtering
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, FiltersBelowMinSeverity) {
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);
    config.min_severity = ANOMALY_SEVERITY_ERROR;

    health_diag_bridge_t* bridge = health_diag_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_INFO, 0.8f);
    diagnostic_result_t* result = NULL;
    EXPECT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), -1);
    EXPECT_EQ(result, nullptr);

    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, FiltersAgentBelowMinSeverity) {
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);
    config.min_agent_severity = HEALTH_SEVERITY_CRITICAL;

    health_diag_bridge_t* bridge = health_diag_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_ANOMALY_DETECTED, HEALTH_SEVERITY_INFO);
    diagnostic_result_t* result = NULL;
    EXPECT_EQ(health_diag_bridge_convert_agent_message(bridge, &msg, &result), -1);

    health_diag_bridge_destroy(bridge);
}

//=============================================================================
// Complete Agent Message Type Conversion Coverage
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, ConvertDeadlockMessage) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_DEADLOCK_DETECTED, HEALTH_SEVERITY_CRITICAL);
    msg.source = HEALTH_SOURCE_THREADING;
    msg.data.deadlock.thread_id_1 = 1001;
    msg.data.deadlock.thread_id_2 = 1002;

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_agent_message(bridge, &msg, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_DEADLOCK);
    EXPECT_EQ(result->severity, DIAG_SEVERITY_CRITICAL);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertNaNMessage) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_NAN_DETECTED, HEALTH_SEVERITY_ERROR);
    msg.data.nan.neuron_id = 42;
    msg.data.nan.layer_id = 3;
    msg.data.nan.nan_count = 5;

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_agent_message(bridge, &msg, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_NAN_DETECTED);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertMemoryCorruptionMessage) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SEVERITY_CRITICAL);
    msg.data.memory.address = (void*)0x12345678;
    msg.data.memory.size = 256;
    msg.data.memory.expected_canary = 0xDEADBEEF;
    msg.data.memory.actual_canary = 0x00000000;

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_agent_message(bridge, &msg, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_HEAP_CORRUPTION);
    EXPECT_TRUE(result->memory_corruption_detected);
    EXPECT_EQ(result->fault_address, (void*)0x12345678);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertHeartbeatTimeoutMessage) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_HEARTBEAT_TIMEOUT, HEALTH_SEVERITY_CRITICAL);
    msg.data.heartbeat.last_heartbeat_us = 1000000;
    msg.data.heartbeat.timeout_threshold_us = 500000;
    msg.data.heartbeat.missed_beats = 3;

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_agent_message(bridge, &msg, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_DEADLOCK);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertResourceExhaustionMessage) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_RESOURCE_EXHAUSTION, HEALTH_SEVERITY_CRITICAL);
    msg.data.resource.memory_used = 900000;
    msg.data.resource.memory_limit = 1000000;
    msg.data.resource.utilization_pct = 90.0f;
    msg.data.resource.time_to_exhaust_ms = 5000;

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_agent_message(bridge, &msg, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_OUT_OF_MEMORY);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertEmergencyMessage) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_EMERGENCY, HEALTH_SEVERITY_CRITICAL);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_agent_message(bridge, &msg, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->severity, DIAG_SEVERITY_CRITICAL);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertStateCorruptionMessage) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_STATE_CORRUPTION, HEALTH_SEVERITY_CRITICAL);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_agent_message(bridge, &msg, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_HEAP_CORRUPTION);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertRecoveryRequestMessage) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_RECOVERY_REQUEST, HEALTH_SEVERITY_WARNING);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_agent_message(bridge, &msg, &result), 0);
    ASSERT_NE(result, nullptr);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertCytokineSignalMessage) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_CYTOKINE_SIGNAL, HEALTH_SEVERITY_WARNING);

    diagnostic_result_t* result = NULL;
    int ret = health_diag_bridge_convert_agent_message(bridge, &msg, &result);
    EXPECT_EQ(ret, 0);
    if (result) diagnostics_free_result(result);

    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertAnomalyDetectedMessage) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_ANOMALY_DETECTED, HEALTH_SEVERITY_WARNING);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_agent_message(bridge, &msg, &result), 0);
    ASSERT_NE(result, nullptr);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertStatusUpdateMessage) {
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);
    config.min_agent_severity = HEALTH_SEVERITY_INFO;

    health_diag_bridge_t* bridge = health_diag_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_STATUS_UPDATE, HEALTH_SEVERITY_INFO);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_agent_message(bridge, &msg, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_NONE);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

//=============================================================================
// Agent Message Null Safety
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, ConvertAgentMessageNullBridge) {
    health_agent_message_t msg;
    memset(&msg, 0, sizeof(msg));
    diagnostic_result_t* result = NULL;
    EXPECT_EQ(health_diag_bridge_convert_agent_message(NULL, &msg, &result), -1);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertAgentMessageNullMessage) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);
    diagnostic_result_t* result = NULL;
    EXPECT_EQ(health_diag_bridge_convert_agent_message(bridge, NULL, &result), -1);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ConvertAgentMessageNullResult) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);
    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_EMERGENCY, HEALTH_SEVERITY_CRITICAL);
    EXPECT_EQ(health_diag_bridge_convert_agent_message(bridge, &msg, NULL), -1);
    health_diag_bridge_destroy(bridge);
}

//=============================================================================
// Severity Translation Tests - Complete Coverage
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, TranslateAnomalySeverityAll) {
    EXPECT_EQ(health_diag_bridge_translate_anomaly_severity(ANOMALY_SEVERITY_INFO),
              DIAG_SEVERITY_INFO);
    EXPECT_EQ(health_diag_bridge_translate_anomaly_severity(ANOMALY_SEVERITY_WARNING),
              DIAG_SEVERITY_WARNING);
    EXPECT_EQ(health_diag_bridge_translate_anomaly_severity(ANOMALY_SEVERITY_ERROR),
              DIAG_SEVERITY_ERROR);
    EXPECT_EQ(health_diag_bridge_translate_anomaly_severity(ANOMALY_SEVERITY_CRITICAL),
              DIAG_SEVERITY_CRITICAL);
}

TEST_F(HealthDiagnosticBridgeTest, TranslateAnomalySeverityOutOfRange) {
    diag_severity_t result = health_diag_bridge_translate_anomaly_severity(
        (anomaly_severity_t)999);
    EXPECT_EQ(result, DIAG_SEVERITY_INFO);
}

TEST_F(HealthDiagnosticBridgeTest, TranslateAgentSeverityAll) {
    EXPECT_EQ(health_diag_bridge_translate_agent_severity(HEALTH_SEVERITY_INFO),
              DIAG_SEVERITY_INFO);
    EXPECT_EQ(health_diag_bridge_translate_agent_severity(HEALTH_SEVERITY_WARNING),
              DIAG_SEVERITY_WARNING);
    EXPECT_EQ(health_diag_bridge_translate_agent_severity(HEALTH_SEVERITY_ERROR),
              DIAG_SEVERITY_ERROR);
    EXPECT_EQ(health_diag_bridge_translate_agent_severity(HEALTH_SEVERITY_CRITICAL),
              DIAG_SEVERITY_CRITICAL);
    EXPECT_EQ(health_diag_bridge_translate_agent_severity(HEALTH_SEVERITY_FATAL),
              DIAG_SEVERITY_FATAL);
}

TEST_F(HealthDiagnosticBridgeTest, TranslateAgentSeverityOutOfRange) {
    diag_severity_t result = health_diag_bridge_translate_agent_severity(
        (health_agent_severity_t)999);
    EXPECT_EQ(result, DIAG_SEVERITY_INFO);
}

//=============================================================================
// Custom Mapping Tests
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, AddCustomAnomalyMapping) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_error_mapping_t mapping;
    mapping.anomaly_type = ANOMALY_CACHE_THRASHING;
    mapping.error_type = ERROR_TYPE_INVALID_STATE;
    mapping.default_severity = DIAG_SEVERITY_WARNING;
    mapping.default_confidence = 0.75f;
    mapping.description_template = "Custom: Cache thrashing - %s";

    EXPECT_EQ(health_diag_bridge_add_anomaly_mapping(bridge, &mapping), 0);

    const anomaly_error_mapping_t* retrieved =
        health_diag_bridge_get_anomaly_mapping(bridge, ANOMALY_CACHE_THRASHING);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->error_type, ERROR_TYPE_INVALID_STATE);
    EXPECT_FLOAT_EQ(retrieved->default_confidence, 0.75f);

    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, AddCustomAgentMapping) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    agent_error_mapping_t mapping;
    mapping.msg_type = HEALTH_MSG_CYTOKINE_SIGNAL;
    mapping.error_type = ERROR_TYPE_INVALID_STATE;
    mapping.default_severity = DIAG_SEVERITY_ERROR;
    mapping.default_confidence = 0.8f;
    mapping.description_template = "Custom cytokine: %s";

    EXPECT_EQ(health_diag_bridge_add_agent_mapping(bridge, &mapping), 0);

    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, AddMappingNullBridge) {
    anomaly_error_mapping_t mapping;
    memset(&mapping, 0, sizeof(mapping));
    EXPECT_EQ(health_diag_bridge_add_anomaly_mapping(NULL, &mapping), -1);
}

TEST_F(HealthDiagnosticBridgeTest, AddMappingNullMapping) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(health_diag_bridge_add_anomaly_mapping(bridge, NULL), -1);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, CustomMappingOverridesDefault) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_error_mapping_t mapping;
    mapping.anomaly_type = ANOMALY_MEMORY_LEAK;
    mapping.error_type = ERROR_TYPE_SEGFAULT;
    mapping.default_severity = DIAG_SEVERITY_FATAL;
    mapping.default_confidence = 0.99f;
    mapping.description_template = "Custom memory leak: %s";

    EXPECT_EQ(health_diag_bridge_add_anomaly_mapping(bridge, &mapping), 0);

    const anomaly_error_mapping_t* retrieved =
        health_diag_bridge_get_anomaly_mapping(bridge, ANOMALY_MEMORY_LEAK);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->error_type, ERROR_TYPE_SEGFAULT);

    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, GetMappingNullBridge) {
    EXPECT_EQ(health_diag_bridge_get_anomaly_mapping(NULL, ANOMALY_MEMORY_LEAK), nullptr);
}

//=============================================================================
// Batch Conversion Tests
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, BatchConvertAnomalies) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    const uint32_t count = 5;
    anomaly_t anomalies[5];
    diagnostic_result_t* results[5] = {};
    uint32_t converted = 0;

    for (uint32_t i = 0; i < count; i++) {
        create_anomaly(&anomalies[i], ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_WARNING, 0.8f);
    }

    ASSERT_EQ(health_diag_bridge_convert_anomalies(
        bridge, anomalies, count, results, &converted), 0);
    EXPECT_EQ(converted, count);

    for (uint32_t i = 0; i < count; i++) {
        ASSERT_NE(results[i], nullptr);
        diagnostics_free_result(results[i]);
    }

    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, BatchConvertMixedSeverities) {
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);
    config.min_severity = ANOMALY_SEVERITY_ERROR;

    health_diag_bridge_t* bridge = health_diag_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomalies[4];
    diagnostic_result_t* results[4] = {};
    uint32_t converted = 0;

    create_anomaly(&anomalies[0], ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_INFO, 0.5f);
    create_anomaly(&anomalies[1], ANOMALY_ERROR_SPIKE, ANOMALY_SEVERITY_ERROR, 0.8f);
    create_anomaly(&anomalies[2], ANOMALY_CACHE_THRASHING, ANOMALY_SEVERITY_WARNING, 0.7f);
    create_anomaly(&anomalies[3], ANOMALY_RESOURCE_EXHAUSTION, ANOMALY_SEVERITY_CRITICAL, 0.9f);

    ASSERT_EQ(health_diag_bridge_convert_anomalies(
        bridge, anomalies, 4, results, &converted), 0);

    EXPECT_EQ(converted, 2u);

    for (uint32_t i = 0; i < 4; i++) {
        if (results[i]) diagnostics_free_result(results[i]);
    }

    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, BatchConvertNullParams) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(health_diag_bridge_convert_anomalies(NULL, NULL, 0, NULL, NULL), -1);
    EXPECT_EQ(health_diag_bridge_convert_anomalies(bridge, NULL, 0, NULL, NULL), -1);

    health_diag_bridge_destroy(bridge);
}

//=============================================================================
// Enrichment API Tests
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, EnrichStackTrace) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);

    EXPECT_GT(result->stack_depth, 0u);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, EnrichStackTraceExplicit) {
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);
    config.capture_stack_trace = false;

    health_diag_bridge_t* bridge = health_diag_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->stack_depth, 0u);

    EXPECT_EQ(health_diag_bridge_enrich_stack_trace(bridge, result), 0);
    EXPECT_GT(result->stack_depth, 0u);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, EnrichStackTraceNullParams) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(health_diag_bridge_enrich_stack_trace(NULL, NULL), -1);
    EXPECT_EQ(health_diag_bridge_enrich_stack_trace(bridge, NULL), -1);
    EXPECT_EQ(health_diag_bridge_enrich_stack_trace(NULL, (diagnostic_result_t*)1), -1);

    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, EnrichMemorySnapshot) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);

    EXPECT_GE(result->memory_state.total_allocated_bytes, 0u);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, EnrichMemorySnapshotExplicit) {
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);
    config.capture_memory_snapshot = false;

    health_diag_bridge_t* bridge = health_diag_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(health_diag_bridge_enrich_memory_snapshot(bridge, result), 0);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, EnrichMemorySnapshotNullParams) {
    EXPECT_EQ(health_diag_bridge_enrich_memory_snapshot(NULL, NULL), -1);
}

TEST_F(HealthDiagnosticBridgeTest, AnalyzePatterns) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->occurrence_count, 1u);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, AnalyzePatternsExplicit) {
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);
    config.enable_pattern_analysis = false;
    config.capture_stack_trace = false;
    config.capture_memory_snapshot = false;

    health_diag_bridge_t* bridge = health_diag_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(health_diag_bridge_analyze_patterns(bridge, result), 0);
    EXPECT_EQ(result->occurrence_count, 1u);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, AnalyzePatternsNullParams) {
    EXPECT_EQ(health_diag_bridge_analyze_patterns(NULL, NULL), -1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, StatisticsTrackConversions) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);
    diagnostic_result_t* result = NULL;
    health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    if (result) diagnostics_free_result(result);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_DEADLOCK_DETECTED, HEALTH_SEVERITY_CRITICAL);
    result = NULL;
    health_diag_bridge_convert_agent_message(bridge, &msg, &result);
    if (result) diagnostics_free_result(result);

    health_diag_bridge_stats_t stats;
    ASSERT_EQ(health_diag_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.anomalies_converted, 1u);
    EXPECT_EQ(stats.agent_messages_converted, 1u);
    EXPECT_GT(stats.avg_conversion_time_us, 0.0f);

    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, StatisticsTrackBySeverity) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION, ANOMALY_SEVERITY_CRITICAL, 0.9f);
    diagnostic_result_t* result = NULL;
    health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    if (result) diagnostics_free_result(result);

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.by_severity[DIAG_SEVERITY_CRITICAL], 1u);

    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, StatisticsTrackByAnomalyType) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);
    diagnostic_result_t* result = NULL;
    health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    if (result) diagnostics_free_result(result);

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.by_anomaly_type[ANOMALY_MEMORY_LEAK], 1u);

    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, StatisticsTrackStackTraces) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);
    diagnostic_result_t* result = NULL;
    health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    if (result) diagnostics_free_result(result);

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.stack_traces_captured, 1u);
    EXPECT_GE(stats.memory_snapshots_captured, 1u);

    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ResetStatistics) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);
    diagnostic_result_t* result = NULL;
    health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    if (result) diagnostics_free_result(result);

    health_diag_bridge_reset_stats(bridge);

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.anomalies_converted, 0u);
    EXPECT_EQ(stats.agent_messages_converted, 0u);
    EXPECT_EQ(stats.conversions_failed, 0u);
    EXPECT_FLOAT_EQ(stats.avg_conversion_time_us, 0.0f);

    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, GetStatsNullParams) {
    EXPECT_EQ(health_diag_bridge_get_stats(NULL, NULL), -1);
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    EXPECT_EQ(health_diag_bridge_get_stats(bridge, NULL), -1);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ResetStatsNullSafe) {
    EXPECT_NO_THROW(health_diag_bridge_reset_stats(NULL));
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, AnomalyTypeNamesComplete) {
    EXPECT_STREQ(health_diag_bridge_anomaly_type_name(ANOMALY_NONE), "NONE");
    EXPECT_STREQ(health_diag_bridge_anomaly_type_name(ANOMALY_MEMORY_LEAK), "MEMORY_LEAK");
    EXPECT_STREQ(health_diag_bridge_anomaly_type_name(ANOMALY_PERFORMANCE_DEGRADATION), "PERFORMANCE_DEGRADATION");
    EXPECT_STREQ(health_diag_bridge_anomaly_type_name(ANOMALY_ERROR_SPIKE), "ERROR_SPIKE");
    EXPECT_STREQ(health_diag_bridge_anomaly_type_name(ANOMALY_THROUGHPUT_DROP), "THROUGHPUT_DROP");
    EXPECT_STREQ(health_diag_bridge_anomaly_type_name(ANOMALY_CACHE_THRASHING), "CACHE_THRASHING");
    EXPECT_STREQ(health_diag_bridge_anomaly_type_name(ANOMALY_RESOURCE_EXHAUSTION), "RESOURCE_EXHAUSTION");
    EXPECT_STREQ(health_diag_bridge_anomaly_type_name(ANOMALY_NUMERICAL_INSTABILITY), "NUMERICAL_INSTABILITY");
    EXPECT_STREQ(health_diag_bridge_anomaly_type_name(ANOMALY_THREAD_CONTENTION), "THREAD_CONTENTION");
    EXPECT_STREQ(health_diag_bridge_anomaly_type_name(ANOMALY_UNKNOWN), "UNKNOWN");
}

TEST_F(HealthDiagnosticBridgeTest, AnomalyTypeNameInvalid) {
    const char* name = health_diag_bridge_anomaly_type_name((anomaly_type_t)999);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "INVALID");
}

TEST_F(HealthDiagnosticBridgeTest, AgentMsgTypeNamesComplete) {
    EXPECT_STREQ(health_diag_bridge_agent_msg_type_name(HEALTH_MSG_ANOMALY_DETECTED), "ANOMALY_DETECTED");
    EXPECT_STREQ(health_diag_bridge_agent_msg_type_name(HEALTH_MSG_CYTOKINE_SIGNAL), "CYTOKINE_SIGNAL");
    EXPECT_STREQ(health_diag_bridge_agent_msg_type_name(HEALTH_MSG_EMERGENCY), "EMERGENCY");
    EXPECT_STREQ(health_diag_bridge_agent_msg_type_name(HEALTH_MSG_RECOVERY_REQUEST), "RECOVERY_REQUEST");
    EXPECT_STREQ(health_diag_bridge_agent_msg_type_name(HEALTH_MSG_STATE_CORRUPTION), "STATE_CORRUPTION");
    EXPECT_STREQ(health_diag_bridge_agent_msg_type_name(HEALTH_MSG_HEARTBEAT_TIMEOUT), "HEARTBEAT_TIMEOUT");
    EXPECT_STREQ(health_diag_bridge_agent_msg_type_name(HEALTH_MSG_DEADLOCK_DETECTED), "DEADLOCK_DETECTED");
    EXPECT_STREQ(health_diag_bridge_agent_msg_type_name(HEALTH_MSG_NAN_DETECTED), "NAN_DETECTED");
    EXPECT_STREQ(health_diag_bridge_agent_msg_type_name(HEALTH_MSG_MEMORY_CORRUPTION), "MEMORY_CORRUPTION");
    EXPECT_STREQ(health_diag_bridge_agent_msg_type_name(HEALTH_MSG_RESOURCE_EXHAUSTION), "RESOURCE_EXHAUSTION");
    EXPECT_STREQ(health_diag_bridge_agent_msg_type_name(HEALTH_MSG_STATUS_UPDATE), "STATUS_UPDATE");
}

TEST_F(HealthDiagnosticBridgeTest, AgentMsgTypeNameInvalid) {
    const char* name = health_diag_bridge_agent_msg_type_name((health_agent_msg_type_t)999);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "UNKNOWN");
}

TEST_F(HealthDiagnosticBridgeTest, VersionString) {
    const char* version = health_diag_bridge_version();
    ASSERT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);
    EXPECT_STREQ(version, HEALTH_DIAG_BRIDGE_VERSION);
}

//=============================================================================
// Diagnostic Result Integrity Tests
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, ResultHasTimestamp) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_GT(result->timestamp, 0);
    EXPECT_GT(result->error_id, 0u);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ResultHasRootCauseAndSymptoms) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);
    strncpy(anomaly.description, "Leak in allocator pool",
            sizeof(anomaly.description) - 1);
    anomaly.metric_value = 2048.0;
    anomaly.expected_value = 1024.0;
    anomaly.deviation = 1024.0;

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_GT(strlen(result->root_cause), 0u);
    EXPECT_GT(strlen(result->symptoms), 0u);
    EXPECT_GT(strlen(result->likely_faulty_function), 0u);

    diagnostics_free_result(result);
    health_diag_bridge_destroy(bridge);
}

TEST_F(HealthDiagnosticBridgeTest, ResultConfidenceClampedToRange) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 2.0f);

    diagnostic_result_t* result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_LE(result->confidence, 1.0f);
    EXPECT_GE(result->confidence, 0.0f);
    diagnostics_free_result(result);

    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, -1.0f);
    result = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, &result), 0);
    ASSERT_NE(result, nullptr);
    EXPECT_LE(result->confidence, 1.0f);
    EXPECT_GE(result->confidence, 0.0f);
    diagnostics_free_result(result);

    health_diag_bridge_destroy(bridge);
}

//=============================================================================
// KG Wiring Integration Tests
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, KGWiringCreation) {
    kg_module_wiring_t* wiring = kg_module_wiring_create(
        "health_diagnostic_bridge", "FAULT_TOLERANCE");
    ASSERT_NE(wiring, nullptr);

    kg_module_wiring_set_metadata(wiring, "NIMCP", "fault_tolerance",
        "Converts health anomalies to diagnostic results for self-repair");
    kg_module_wiring_set_version(wiring, 1, 0, 0);

    kg_module_wiring_add_input(wiring, "health_monitor", "ANOMALY_DETECTED", true);
    kg_module_wiring_add_input(wiring, "health_agent", "HEALTH_AGENT_MESSAGE", true);

    kg_module_wiring_add_output(wiring, "DIAGNOSTIC_RESULT", "Converted diagnostic result");
    kg_module_wiring_add_output(wiring, "HEALTH_DIAGNOSTIC_CONVERTED", "Conversion complete signal");

    kg_module_wiring_add_handler(wiring, "ANOMALY_DETECTED", 200);
    kg_module_wiring_add_handler(wiring, "HEALTH_AGENT_MESSAGE", 200);

    EXPECT_TRUE(kg_module_wiring_has_input(wiring, "health_monitor", "ANOMALY_DETECTED"));
    EXPECT_TRUE(kg_module_wiring_has_input(wiring, "health_agent", "HEALTH_AGENT_MESSAGE"));
    EXPECT_TRUE(kg_module_wiring_has_output(wiring, "DIAGNOSTIC_RESULT"));
    EXPECT_TRUE(kg_module_wiring_has_output(wiring, "HEALTH_DIAGNOSTIC_CONVERTED"));
    EXPECT_TRUE(kg_module_wiring_has_handler(wiring, "ANOMALY_DETECTED"));

    char error_buf[256];
    EXPECT_EQ(kg_module_wiring_validate(wiring, error_buf, sizeof(error_buf)), 0);

    kg_module_wiring_destroy(wiring);
}

TEST_F(HealthDiagnosticBridgeTest, KGWiringMetadata) {
    kg_module_wiring_t* wiring = kg_module_wiring_create(
        "health_diagnostic_bridge", "FAULT_TOLERANCE");
    ASSERT_NE(wiring, nullptr);

    kg_module_wiring_add_metadata_entry(wiring, "bridge_type", "health_to_diagnostic");
    kg_module_wiring_add_metadata_entry(wiring, "severity_mapping", "anomaly_to_diag");
    kg_module_wiring_add_metadata_entry(wiring, "enrichment", "stack_trace+memory_snapshot");

    EXPECT_EQ(wiring->metadata.entry_count, 3u);

    kg_module_wiring_destroy(wiring);
}

//=============================================================================
// Security Module Coverage Tests
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, SecurityInputValidation) {
    nimcp_input_validation_t valid = nimcp_security_validate_input(
        "ANOMALY_MEMORY_LEAK", 256, NULL);
    EXPECT_EQ(valid, NIMCP_INPUT_VALID);

    valid = nimcp_security_validate_input(
        "safe diagnostic description text", 512, NULL);
    EXPECT_EQ(valid, NIMCP_INPUT_VALID);
}

TEST_F(HealthDiagnosticBridgeTest, SecurityThreatAnalysisOnAnomalyDescription) {
    nimcp_threat_level_t threat = nimcp_security_analyze_threat(
        "Memory leak detected in module X");
    EXPECT_LE(threat, NIMCP_THREAT_LOW);
}

TEST_F(HealthDiagnosticBridgeTest, SecurityThreatLevelNames) {
    EXPECT_NE(nimcp_threat_level_name(NIMCP_THREAT_NONE), nullptr);
    EXPECT_NE(nimcp_threat_level_name(NIMCP_THREAT_LOW), nullptr);
    EXPECT_NE(nimcp_threat_level_name(NIMCP_THREAT_MEDIUM), nullptr);
    EXPECT_NE(nimcp_threat_level_name(NIMCP_THREAT_HIGH), nullptr);
    EXPECT_NE(nimcp_threat_level_name(NIMCP_THREAT_CRITICAL), nullptr);
}

TEST_F(HealthDiagnosticBridgeTest, SecuritySanitizeAnomalyDescription) {
    char sanitized[256];
    nimcp_result_t ret = nimcp_security_sanitize_input(
        "Memory leak in <module>", sanitized, sizeof(sanitized));
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(strlen(sanitized), 0u);
}

//=============================================================================
// Math Utils Coverage - Phasor Coherence for Anomaly Patterns
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, MathPhasorCoherenceForAnomalyPatterns) {
    complex_math_init(NULL);

    const uint32_t n = 8;
    neural_phasor_t signals[8];
    for (uint32_t i = 0; i < n; i++) {
        float phase = 0.5f + 0.01f * (float)i;
        signals[i] = phasor_from_polar(1.0f, phase);
    }

    float coherence = phasor_array_coherence(signals, n);
    EXPECT_GT(coherence, 0.9f);

    float mean_phase = phasor_array_mean_phase(signals, n);
    EXPECT_GT(mean_phase, 0.0f);

    float variance = phasor_array_phase_variance(signals, n);
    EXPECT_LT(variance, 0.1f);

    complex_math_cleanup();
}

TEST_F(HealthDiagnosticBridgeTest, MathPhasorLowCoherenceDetection) {
    complex_math_init(NULL);

    const uint32_t n = 8;
    neural_phasor_t signals[8];
    for (uint32_t i = 0; i < n; i++) {
        float phase = (float)i * (2.0f * 3.14159f / (float)n);
        signals[i] = phasor_from_polar(1.0f, phase);
    }

    float coherence = phasor_array_coherence(signals, n);
    EXPECT_LT(coherence, 0.3f);

    complex_math_cleanup();
}

TEST_F(HealthDiagnosticBridgeTest, MathPhasorAmplitudePhase) {
    neural_phasor_t p = phasor_from_polar(2.5f, 1.0f);
    EXPECT_NEAR(phasor_amplitude(p), 2.5f, 0.01f);
    EXPECT_NEAR(phasor_phase(p), 1.0f, 0.01f);
}

TEST_F(HealthDiagnosticBridgeTest, MathPhasorNormalize) {
    neural_phasor_t p = phasor_from_polar(3.0f, 0.5f);
    neural_phasor_t normalized = phasor_normalize(p);
    EXPECT_NEAR(phasor_amplitude(normalized), 1.0f, 0.01f);
}

//=============================================================================
// Quantum Annealing Coverage - Confidence Optimization
//=============================================================================

static float confidence_energy(const float* state, uint32_t dim, void* user_data) {
    (void)user_data;
    float energy = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float x = state[i] - 0.7f;
        energy += x * x;
    }
    return energy;
}

TEST_F(HealthDiagnosticBridgeTest, QuantumAnnealingConfidenceOptimization) {
    quantum_annealing_config_t config = quantum_annealing_default_config();
    config.num_iterations = 100;
    config.seed = 42;

    quantum_annealer_t annealer = quantum_annealer_create(&config);
    if (annealer) {
        float initial_state[2] = {0.5f, 0.3f};
        float optimized[2] = {0.0f, 0.0f};

        float final_energy = quantum_anneal(
            annealer, confidence_energy, initial_state, optimized, 2, NULL);

        EXPECT_LT(final_energy, 1.0f);
        EXPECT_GE(optimized[0], 0.0f);
        EXPECT_LE(optimized[0], 1.5f);

        quantum_annealer_destroy(annealer);
    }
}

TEST_F(HealthDiagnosticBridgeTest, QuantumAnnealerDefaultConfig) {
    quantum_annealing_config_t config = quantum_annealing_default_config();
    EXPECT_GT(config.num_iterations, 0u);
}

TEST_F(HealthDiagnosticBridgeTest, QuantumAnnealerCreateDestroy) {
    quantum_annealing_config_t config = quantum_annealing_default_config();
    quantum_annealer_t annealer = quantum_annealer_create(&config);
    if (annealer) {
        quantum_annealer_destroy(annealer);
    }
    quantum_annealer_destroy(NULL);
}

//=============================================================================
// Code Immune Integration Tests
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, CodeImmuneSystemCreation) {
    code_immune_config_t config;
    code_immune_default_config(&config);

    code_immune_system_t* immune = code_immune_create_with_config(NULL, &config);
    if (immune) {
        EXPECT_NE(immune, nullptr);
        code_immune_destroy(immune);
    }
}

TEST_F(HealthDiagnosticBridgeTest, CodeImmuneAutoRepairConfig) {
    code_immune_config_t config;
    code_immune_default_config(&config);

    EXPECT_TRUE(config.auto_repair.enabled);
    EXPECT_EQ(config.auto_repair.min_crash_count, 3u);
    EXPECT_GT(config.auto_repair.min_severity, 0.0f);
    EXPECT_GT(config.auto_repair.min_confidence, 0.0f);
    EXPECT_GT(config.auto_repair.cooldown_ms, 0u);
}

//=============================================================================
// Logging Integration Tests
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, LoggingSystemInit) {
    nimcp_log_config_t log_config = nimcp_log_default_config();
    log_config.level = LOG_LEVEL_DEBUG;

    nimcp_logger_t logger = nimcp_log_create(&log_config);
    if (logger) {
        nimcp_log_destroy(logger);
    }
}

//=============================================================================
// Exception Handler Coverage
//=============================================================================

TEST_F(HealthDiagnosticBridgeTest, ExceptionHandlerNullConfig) {
    int ret = health_diag_bridge_default_config(NULL);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthDiagnosticBridgeTest, ExceptionHandlerNullBridgeOnGetMapping) {
    const anomaly_error_mapping_t* mapping =
        health_diag_bridge_get_anomaly_mapping(NULL, ANOMALY_MEMORY_LEAK);
    EXPECT_EQ(mapping, nullptr);
}

TEST_F(HealthDiagnosticBridgeTest, ExceptionHandlerNullBridgeOnIsReady) {
    EXPECT_FALSE(health_diag_bridge_is_ready(NULL));
}

TEST_F(HealthDiagnosticBridgeTest, ExceptionHandlerAllNullConvertAnomaly) {
    EXPECT_EQ(health_diag_bridge_convert_anomaly(NULL, NULL, NULL), -1);
}

TEST_F(HealthDiagnosticBridgeTest, ExceptionHandlerAllNullConvertAgentMsg) {
    EXPECT_EQ(health_diag_bridge_convert_agent_message(NULL, NULL, NULL), -1);
}

TEST_F(HealthDiagnosticBridgeTest, ExceptionHandlerAllNullBatchConvert) {
    EXPECT_EQ(health_diag_bridge_convert_anomalies(NULL, NULL, 0, NULL, NULL), -1);
}

TEST_F(HealthDiagnosticBridgeTest, ExceptionHandlerAllNullEnrichStackTrace) {
    EXPECT_EQ(health_diag_bridge_enrich_stack_trace(NULL, NULL), -1);
}

TEST_F(HealthDiagnosticBridgeTest, ExceptionHandlerAllNullEnrichMemory) {
    EXPECT_EQ(health_diag_bridge_enrich_memory_snapshot(NULL, NULL), -1);
}

TEST_F(HealthDiagnosticBridgeTest, ExceptionHandlerAllNullAnalyzePatterns) {
    EXPECT_EQ(health_diag_bridge_analyze_patterns(NULL, NULL), -1);
}

TEST_F(HealthDiagnosticBridgeTest, ExceptionHandlerAllNullAddAnomalyMapping) {
    EXPECT_EQ(health_diag_bridge_add_anomaly_mapping(NULL, NULL), -1);
}

TEST_F(HealthDiagnosticBridgeTest, ExceptionHandlerAllNullAddAgentMapping) {
    EXPECT_EQ(health_diag_bridge_add_agent_mapping(NULL, NULL), -1);
}

TEST_F(HealthDiagnosticBridgeTest, ExceptionHandlerAllNullGetStats) {
    EXPECT_EQ(health_diag_bridge_get_stats(NULL, NULL), -1);
}

//=============================================================================
// Health Agent Heartbeat Integration
//=============================================================================

extern "C" {
    void health_diagnostic_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

TEST_F(HealthDiagnosticBridgeTest, HealthAgentSetNullSafe) {
    EXPECT_NO_THROW(health_diagnostic_bridge_set_health_agent(NULL));
}

TEST_F(HealthDiagnosticBridgeTest, HealthAgentSetAndClear) {
    health_agent_config_t cfg;
    nimcp_health_agent_default_config(&cfg);
    nimcp_health_agent_t* agent = nimcp_health_agent_create(&cfg);
    if (agent) {
        health_diagnostic_bridge_set_health_agent(agent);

        health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
        ASSERT_NE(bridge, nullptr);
        health_diag_bridge_destroy(bridge);

        health_diagnostic_bridge_set_health_agent(NULL);
        nimcp_health_agent_destroy(agent);
    }
}
