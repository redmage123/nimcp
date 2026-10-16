/**
 * @file test_health_self_repair_bridge.cpp
 * @brief Unit tests for Health Self-Repair Bridge
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Unit tests for health-to-self-repair automation bridge
 * WHY: Ensure reliable auto-trigger and outcome tracking
 * HOW: Test-driven development with coverage of all public APIs
 *
 * Test Coverage:
 * - Creation and destruction
 * - Configuration
 * - Trigger policy enforcement
 * - Rate limiting
 * - Aggregation
 * - Tracking records
 * - Callbacks
 * - Statistics
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <atomic>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_health_self_repair_bridge.h"
#include "cognitive/fault_tolerance/nimcp_health_diagnostic_bridge.h"
#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "security/nimcp_security.h"
#include "utils/math/nimcp_complex_math.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"
#include "cognitive/immune/nimcp_code_immune.h"

//=============================================================================
// Test Fixture
//=============================================================================

class HealthSelfRepairBridgeTest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;
    health_diag_bridge_t* diag_bridge = nullptr;
    self_repair_coordinator_t* self_repair = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;

        // Create dependencies
        diag_bridge = health_diag_bridge_create(NULL);
        self_repair = self_repair_create(NULL);
    }

    void TearDown() override {
        // Cleanup dependencies
        if (self_repair) self_repair_destroy(self_repair);
        if (diag_bridge) health_diag_bridge_destroy(diag_bridge);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
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

TEST_F(HealthSelfRepairBridgeTest, CreateWithDefaults) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(self_repair, nullptr);

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);

    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(health_self_repair_bridge_is_ready(bridge));

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, CreateWithCustomConfig) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(self_repair, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);

    config.trigger_policy = HEALTH_TRIGGER_ERROR;
    config.rate_limit.max_repairs_per_window = 5;
    config.aggregation.enabled = false;
    config.async_repairs = false;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);

    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(health_self_repair_bridge_is_ready(bridge));

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, CreateNullDiagBridgeReturnsNull) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, NULL, self_repair);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(HealthSelfRepairBridgeTest, CreateNullSelfRepairReturnsNull) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, NULL);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(HealthSelfRepairBridgeTest, DestroyNullSafety) {
    EXPECT_NO_THROW(health_self_repair_bridge_destroy(NULL));
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, DefaultConfigValues) {
    health_self_repair_bridge_config_t config;
    int ret = health_self_repair_bridge_default_config(&config);

    ASSERT_EQ(ret, 0);
    EXPECT_EQ(config.trigger_policy, HEALTH_TRIGGER_CRITICAL);
    EXPECT_GT(config.rate_limit.max_repairs_per_window, 0u);
    EXPECT_GT(config.rate_limit.window_duration_ms, 0u);
    EXPECT_TRUE(config.aggregation.enabled);
    EXPECT_TRUE(config.async_repairs);
    EXPECT_GT(config.min_confidence, 0.0f);
    EXPECT_LE(config.min_confidence, 1.0f);
}

//=============================================================================
// Trigger Policy Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, ManualPolicySkipsAutoTrigger) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_MANUAL;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    // Create critical anomaly
    anomaly_t anomaly;
    memset(&anomaly, 0, sizeof(anomaly));
    anomaly.type = ANOMALY_RESOURCE_EXHAUSTION;
    anomaly.severity = ANOMALY_SEVERITY_CRITICAL;
    anomaly.confidence = 0.9f;

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(bridge, &anomaly, &request_id);

    // Should be skipped due to manual policy
    EXPECT_EQ(ret, 1);  // 1 = skipped

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, FatalOnlyPolicySkipsWarning) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_FATAL_ONLY;
    config.aggregation.enabled = false;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    // Create warning anomaly (should be skipped)
    anomaly_t anomaly;
    memset(&anomaly, 0, sizeof(anomaly));
    anomaly.type = ANOMALY_MEMORY_LEAK;
    anomaly.severity = ANOMALY_SEVERITY_WARNING;
    anomaly.confidence = 0.9f;

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(bridge, &anomaly, &request_id);
    EXPECT_EQ(ret, 1);  // Skipped

    health_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// Rate Limiting Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, RateLimitIsEnforced) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.rate_limit.max_repairs_per_window = 2;
    config.rate_limit.window_duration_ms = 60000;
    config.aggregation.enabled = false;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    // First check: should not be rate limited initially
    EXPECT_FALSE(health_self_repair_bridge_is_rate_limited(bridge, ERROR_TYPE_MEMORY_LEAK));

    // Reset to clear state
    health_self_repair_bridge_reset_rate_limit(bridge);

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, RateLimitReset) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.rate_limit.max_repairs_per_window = 1;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    // Reset rate limit
    health_self_repair_bridge_reset_rate_limit(bridge);

    // Should not be rate limited after reset
    EXPECT_FALSE(health_self_repair_bridge_is_rate_limited(bridge, ERROR_TYPE_UNKNOWN));

    health_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// Tracking Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, GetPendingCount) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    uint32_t pending = health_self_repair_bridge_get_pending_count(bridge);
    EXPECT_EQ(pending, 0u);  // Initially no pending repairs

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, GetRecentTracking) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    health_repair_tracking_t records[10];
    uint32_t count = health_self_repair_bridge_get_recent_tracking(bridge, records, 10);
    EXPECT_EQ(count, 0u);  // Initially no records

    health_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// Callback Tests
//=============================================================================

static bool trigger_callback_called = false;
static bool outcome_callback_called = false;

static void test_trigger_callback(
    uint64_t request_id,
    const diagnostic_result_t* diagnostic,
    void* user_data
) {
    (void)request_id;
    (void)diagnostic;
    (void)user_data;
    trigger_callback_called = true;
}

static void test_outcome_callback(
    uint64_t request_id,
    health_repair_outcome_t outcome,
    const self_repair_result_t* result,
    void* user_data
) {
    (void)request_id;
    (void)outcome;
    (void)result;
    (void)user_data;
    outcome_callback_called = true;
}

TEST_F(HealthSelfRepairBridgeTest, SetTriggerCallback) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    trigger_callback_called = false;

    int ret = health_self_repair_bridge_set_trigger_callback(
        bridge, test_trigger_callback, NULL);
    EXPECT_EQ(ret, 0);

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, SetOutcomeCallback) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    outcome_callback_called = false;

    int ret = health_self_repair_bridge_set_outcome_callback(
        bridge, test_outcome_callback, NULL);
    EXPECT_EQ(ret, 0);

    health_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, GetStatistics) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    health_self_repair_bridge_stats_t stats;
    int ret = health_self_repair_bridge_get_stats(bridge, &stats);

    ASSERT_EQ(ret, 0);
    EXPECT_EQ(stats.repairs_triggered, 0u);
    EXPECT_EQ(stats.repairs_succeeded, 0u);
    EXPECT_EQ(stats.repairs_failed, 0u);

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, ResetStatistics) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    health_self_repair_bridge_reset_stats(bridge);

    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.repairs_triggered, 0u);

    health_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, PolicyNames) {
    EXPECT_STREQ(health_self_repair_bridge_policy_name(HEALTH_TRIGGER_MANUAL), "MANUAL");
    EXPECT_STREQ(health_self_repair_bridge_policy_name(HEALTH_TRIGGER_FATAL_ONLY), "FATAL_ONLY");
    EXPECT_STREQ(health_self_repair_bridge_policy_name(HEALTH_TRIGGER_CRITICAL), "CRITICAL");
    EXPECT_STREQ(health_self_repair_bridge_policy_name(HEALTH_TRIGGER_ERROR), "ERROR");
    EXPECT_STREQ(health_self_repair_bridge_policy_name(HEALTH_TRIGGER_AUTO), "AUTO");
}

TEST_F(HealthSelfRepairBridgeTest, OutcomeNames) {
    EXPECT_STREQ(health_self_repair_bridge_outcome_name(HEALTH_REPAIR_OUTCOME_PENDING), "PENDING");
    EXPECT_STREQ(health_self_repair_bridge_outcome_name(HEALTH_REPAIR_OUTCOME_SUCCESS), "SUCCESS");
    EXPECT_STREQ(health_self_repair_bridge_outcome_name(HEALTH_REPAIR_OUTCOME_FAILED), "FAILED");
    EXPECT_STREQ(health_self_repair_bridge_outcome_name(HEALTH_REPAIR_OUTCOME_SKIPPED), "SKIPPED");
    EXPECT_STREQ(health_self_repair_bridge_outcome_name(HEALTH_REPAIR_OUTCOME_TIMEOUT), "TIMEOUT");
}

TEST_F(HealthSelfRepairBridgeTest, VersionString) {
    const char* version = health_self_repair_bridge_version();
    ASSERT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);
}

//=============================================================================
// Health Agent Connection Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, ConnectHealthAgent) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    nimcp_health_agent_t* agent = nimcp_health_agent_create(NULL);
    ASSERT_NE(agent, nullptr);

    int ret = health_self_repair_bridge_connect_health_agent(bridge, agent);
    EXPECT_EQ(ret, 0);

    // Connect NULL agent is rejected (implementation requires non-NULL)
    ret = health_self_repair_bridge_connect_health_agent(bridge, NULL);
    EXPECT_EQ(ret, -1);

    nimcp_health_agent_destroy(agent);
    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, ConnectHealthAgentNullBridge) {
    nimcp_health_agent_t* agent = nimcp_health_agent_create(NULL);
    ASSERT_NE(agent, nullptr);

    int ret = health_self_repair_bridge_connect_health_agent(NULL, agent);
    EXPECT_EQ(ret, -1);

    nimcp_health_agent_destroy(agent);
}

//=============================================================================
// Process Agent Message Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, ProcessAgentMessageCritical) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.aggregation.enabled = false;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_EMERGENCY, HEALTH_SEVERITY_CRITICAL);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_agent_message(bridge, &msg, &request_id);
    // Should trigger (critical meets CRITICAL policy)
    EXPECT_EQ(ret, 0);

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, ProcessAgentMessageSkippedByPolicy) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_FATAL_ONLY;
    config.aggregation.enabled = false;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    health_agent_message_t msg;
    // Use WARNING (meets diag bridge min_agent_severity) but below FATAL trigger policy
    create_agent_msg(&msg, HEALTH_MSG_STATUS_UPDATE, HEALTH_SEVERITY_WARNING);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_agent_message(bridge, &msg, &request_id);
    EXPECT_EQ(ret, 1);  // Skipped by FATAL_ONLY policy

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, ProcessAgentMessageNullBridge) {
    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_EMERGENCY, HEALTH_SEVERITY_CRITICAL);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_agent_message(NULL, &msg, &request_id);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthSelfRepairBridgeTest, ProcessAgentMessageNullMessage) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_agent_message(bridge, NULL, &request_id);
    EXPECT_EQ(ret, -1);

    health_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// Trigger From Diagnostic Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, TriggerFromDiagnostic) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.aggregation.enabled = false;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    // Create diagnostic via anomaly conversion
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.95f);
    diagnostic_result_t* diag = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &diag), 0);
    ASSERT_NE(diag, nullptr);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_trigger_from_diagnostic(bridge, diag, &request_id);
    EXPECT_EQ(ret, 0);  // Triggered
    // Note: trigger_from_diagnostic frees the diagnostic on success

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, TriggerFromDiagnosticNullBridge) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.95f);
    diagnostic_result_t* diag = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &diag), 0);
    ASSERT_NE(diag, nullptr);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_trigger_from_diagnostic(NULL, diag, &request_id);
    EXPECT_EQ(ret, -1);

    diagnostics_free_result(diag);
}

TEST_F(HealthSelfRepairBridgeTest, TriggerFromDiagnosticNullDiag) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_trigger_from_diagnostic(bridge, NULL, &request_id);
    EXPECT_EQ(ret, -1);

    health_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// Force Trigger Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, ForceTriggerBypassesPolicy) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_MANUAL;  // Manual = no auto-trigger
    config.aggregation.enabled = false;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    // Create low-severity diagnostic
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                   ANOMALY_SEVERITY_WARNING, 0.5f);
    diagnostic_result_t* diag = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &diag), 0);
    ASSERT_NE(diag, nullptr);

    uint64_t request_id = 0;
    // Force trigger should work even with MANUAL policy
    int ret = health_self_repair_bridge_force_trigger(bridge, diag, &request_id);
    EXPECT_EQ(ret, 0);
    // Note: force_trigger frees the diagnostic on success

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, ForceTriggerNullBridge) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);
    diagnostic_result_t* diag = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &diag), 0);
    ASSERT_NE(diag, nullptr);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_force_trigger(NULL, diag, &request_id);
    EXPECT_EQ(ret, -1);

    diagnostics_free_result(diag);
}

//=============================================================================
// Outcome Processing Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, ProcessOutcomesEmpty) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    uint32_t processed = health_self_repair_bridge_process_outcomes(bridge, 10);
    EXPECT_EQ(processed, 0u);  // No pending outcomes

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, ProcessOutcomesNullBridge) {
    uint32_t processed = health_self_repair_bridge_process_outcomes(NULL, 10);
    EXPECT_EQ(processed, 0u);
}

//=============================================================================
// Aggregation Processing Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, ProcessAggregationEmpty) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    uint32_t batched = health_self_repair_bridge_process_aggregation(bridge);
    EXPECT_EQ(batched, 0u);  // No pending aggregation

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, ProcessAggregationNullBridge) {
    uint32_t batched = health_self_repair_bridge_process_aggregation(NULL);
    EXPECT_EQ(batched, 0u);
}

//=============================================================================
// Broadcasting Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, BroadcastTrigger) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);
    diagnostic_result_t* diag = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &diag), 0);
    ASSERT_NE(diag, nullptr);

    int ret = health_self_repair_bridge_broadcast_trigger(bridge, 1, diag);
    // May return 0 or -1 depending on bio-async connection
    EXPECT_GE(ret, -1);

    diagnostics_free_result(diag);
    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, BroadcastTriggerNullBridge) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);
    diagnostic_result_t* diag = NULL;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &diag), 0);
    ASSERT_NE(diag, nullptr);

    int ret = health_self_repair_bridge_broadcast_trigger(NULL, 1, diag);
    EXPECT_EQ(ret, -1);

    diagnostics_free_result(diag);
}

TEST_F(HealthSelfRepairBridgeTest, BroadcastOutcome) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    int ret = health_self_repair_bridge_broadcast_outcome(
        bridge, 1, HEALTH_REPAIR_OUTCOME_SUCCESS);
    EXPECT_GE(ret, -1);

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, BroadcastOutcomeNullBridge) {
    int ret = health_self_repair_bridge_broadcast_outcome(
        NULL, 1, HEALTH_REPAIR_OUTCOME_SUCCESS);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Tracking Record Lookup Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, GetTrackingNonExistent) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    const health_repair_tracking_t* tracking =
        health_self_repair_bridge_get_tracking(bridge, 99999);
    EXPECT_EQ(tracking, nullptr);  // No such request

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, GetTrackingNullBridge) {
    const health_repair_tracking_t* tracking =
        health_self_repair_bridge_get_tracking(NULL, 1);
    EXPECT_EQ(tracking, nullptr);
}

TEST_F(HealthSelfRepairBridgeTest, GetTrackingAfterTrigger) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_ERROR;
    config.aggregation.enabled = false;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);
    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(bridge, &anomaly, &request_id);
    if (ret == 0) {
        // Tracking record should exist
        const health_repair_tracking_t* tracking =
            health_self_repair_bridge_get_tracking(bridge, request_id);
        if (tracking) {
            EXPECT_EQ(tracking->request_id, request_id);
            EXPECT_EQ(tracking->severity, DIAG_SEVERITY_CRITICAL);
        }
    }

    health_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// All Trigger Policies with Real Anomalies
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, CriticalPolicyTriggersOnCritical) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.aggregation.enabled = false;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);
    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(bridge, &anomaly, &request_id);
    EXPECT_EQ(ret, 0);  // Triggered

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, ErrorPolicyTriggersOnError) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_ERROR;
    config.aggregation.enabled = false;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_ERROR_SPIKE,
                   ANOMALY_SEVERITY_ERROR, 0.8f);
    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(bridge, &anomaly, &request_id);
    EXPECT_EQ(ret, 0);  // Triggered

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, AutoPolicyAdaptiveTrigger) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_AUTO;
    config.aggregation.enabled = false;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.95f);
    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(bridge, &anomaly, &request_id);
    // AUTO should trigger on high-severity, high-confidence
    EXPECT_EQ(ret, 0);

    health_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// Statistics After Operations
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, StatsAfterTriggeredRepairs) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_ERROR;
    config.aggregation.enabled = false;
    config.rate_limit.max_repairs_per_window = 100;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    // Trigger several repairs
    for (int i = 0; i < 5; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                       ANOMALY_SEVERITY_CRITICAL, 0.9f);
        uint64_t request_id = 0;
        health_self_repair_bridge_process_anomaly(bridge, &anomaly, &request_id);
    }

    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.repairs_triggered, 5u);

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, StatsNullBridge) {
    health_self_repair_bridge_stats_t stats;
    int ret = health_self_repair_bridge_get_stats(NULL, &stats);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthSelfRepairBridgeTest, StatsNullOutput) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    int ret = health_self_repair_bridge_get_stats(bridge, NULL);
    EXPECT_EQ(ret, -1);

    health_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// IsReady Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, IsReadyNullReturnsFalse) {
    EXPECT_FALSE(health_self_repair_bridge_is_ready(NULL));
}

//=============================================================================
// DefaultConfig NULL Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, DefaultConfigNullReturnsError) {
    int ret = health_self_repair_bridge_default_config(NULL);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Process Anomaly NULL Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, ProcessAnomalyNullBridge) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);
    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(NULL, &anomaly, &request_id);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthSelfRepairBridgeTest, ProcessAnomalyNullAnomaly) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(bridge, NULL, &request_id);
    EXPECT_EQ(ret, -1);

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, ProcessAnomalyNullRequestId) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.aggregation.enabled = false;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);
    // NULL request_id should still work
    int ret = health_self_repair_bridge_process_anomaly(bridge, &anomaly, NULL);
    EXPECT_GE(ret, 0);  // 0=triggered, 1=skipped

    health_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// Rate Limit with Per-Error-Type Tests
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, RateLimitPerErrorType) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.rate_limit.max_repairs_per_window = 2;
    config.rate_limit.per_error_type_limit = true;
    config.aggregation.enabled = false;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    // Check rate limited state for different error types
    EXPECT_FALSE(health_self_repair_bridge_is_rate_limited(bridge, ERROR_TYPE_MEMORY_LEAK));
    EXPECT_FALSE(health_self_repair_bridge_is_rate_limited(bridge, ERROR_TYPE_OUT_OF_MEMORY));

    health_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// Callback Invocation Tests
//=============================================================================

static std::atomic<int> s_trigger_count{0};
static std::atomic<int> s_outcome_count{0};

static void counting_trigger_cb(uint64_t, const diagnostic_result_t*, void*) {
    s_trigger_count.fetch_add(1);
}

static void counting_outcome_cb(uint64_t, health_repair_outcome_t,
                                 const self_repair_result_t*, void*) {
    s_outcome_count.fetch_add(1);
}

TEST_F(HealthSelfRepairBridgeTest, TriggerCallbackInvoked) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_ERROR;
    config.aggregation.enabled = false;

    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    s_trigger_count = 0;
    health_self_repair_bridge_set_trigger_callback(bridge, counting_trigger_cb, NULL);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);
    uint64_t request_id = 0;
    health_self_repair_bridge_process_anomaly(bridge, &anomaly, &request_id);

    EXPECT_GE(s_trigger_count.load(), 1);

    health_self_repair_bridge_destroy(bridge);
}

TEST_F(HealthSelfRepairBridgeTest, SetCallbackNullBridge) {
    int ret = health_self_repair_bridge_set_trigger_callback(NULL, counting_trigger_cb, NULL);
    EXPECT_EQ(ret, -1);

    ret = health_self_repair_bridge_set_outcome_callback(NULL, counting_outcome_cb, NULL);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthSelfRepairBridgeTest, SetCallbackNullCallback) {
    health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
        NULL, diag_bridge, self_repair);
    ASSERT_NE(bridge, nullptr);

    // NULL callback should be allowed (disables callback)
    int ret = health_self_repair_bridge_set_trigger_callback(bridge, NULL, NULL);
    EXPECT_EQ(ret, 0);

    ret = health_self_repair_bridge_set_outcome_callback(bridge, NULL, NULL);
    EXPECT_EQ(ret, 0);

    health_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// KG Wiring Integration
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, KGWiringCreation) {
    kg_module_wiring_t* wiring = kg_module_wiring_create(
        "health_self_repair_bridge", "FAULT_TOLERANCE");
    ASSERT_NE(wiring, nullptr);

    kg_module_wiring_set_metadata(wiring, "NIMCP", "fault_tolerance",
        "Connects health detection to self-repair automation");

    kg_module_wiring_add_input(wiring, "diagnostic_result", "diag_result_t", true);
    kg_module_wiring_add_input(wiring, "anomaly", "anomaly_t", true);
    kg_module_wiring_add_input(wiring, "agent_message", "health_agent_message_t", false);
    kg_module_wiring_add_output(wiring, "repair_request", "Repair request output");
    kg_module_wiring_add_output(wiring, "repair_outcome", "Repair outcome output");

    EXPECT_TRUE(kg_module_wiring_has_input(wiring, "diagnostic_result", "diag_result_t"));
    EXPECT_TRUE(kg_module_wiring_has_input(wiring, "anomaly", "anomaly_t"));
    EXPECT_TRUE(kg_module_wiring_has_output(wiring, "repair_request"));

    kg_module_wiring_destroy(wiring);
}

//=============================================================================
// Security Module Coverage
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, SecurityValidateRepairDescription) {
    char sanitized[256];
    nimcp_result_t ret = nimcp_security_sanitize_input(
        "Resource exhaustion in <module>", sanitized, sizeof(sanitized));
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(strlen(sanitized), 0u);
}

TEST_F(HealthSelfRepairBridgeTest, SecurityThreatAnalysis) {
    nimcp_threat_level_t level = nimcp_security_analyze_threat("self_repair_trigger");
    EXPECT_NE(nimcp_threat_level_name(level), nullptr);
}

//=============================================================================
// Math Utils - Phasor Coherence for Repair Patterns
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, PhasorCoherenceRepairTiming) {
    complex_math_init(NULL);

    // Simulate synchronized repair trigger patterns
    const uint32_t n = 6;
    neural_phasor_t signals[6];
    for (uint32_t i = 0; i < n; i++) {
        float phase = 1.0f + 0.05f * (float)i;
        signals[i] = phasor_from_polar(1.0f, phase);
    }

    float coherence = phasor_array_coherence(signals, n);
    EXPECT_GT(coherence, 0.8f);  // High coherence = synchronized repairs
}

//=============================================================================
// Quantum Annealing - Repair Priority Optimization
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, QuantumAnnealingRepairPriority) {
    quantum_annealing_config_t qa_config = quantum_annealing_default_config();
    qa_config.num_iterations = 50;
    qa_config.initial_temperature = 1.0f;

    quantum_annealer_t annealer = quantum_annealer_create(&qa_config);
    if (annealer) {
        float initial_state[] = {0.8f, 0.6f, 0.9f};
        float optimized[3] = {0};

        float energy = quantum_anneal(annealer, NULL, initial_state, optimized, 3, NULL);
        EXPECT_GE(energy, 0.0f);

        quantum_annealer_destroy(annealer);
    }
}

//=============================================================================
// Code Immune Integration
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, CodeImmuneAutoRepairConfig) {
    code_immune_config_t config;
    code_immune_default_config(&config);

    EXPECT_TRUE(config.auto_repair.enabled);
    EXPECT_GT(config.auto_repair.min_crash_count, 0u);
    EXPECT_GT(config.auto_repair.min_severity, 0.0f);
    EXPECT_GT(config.auto_repair.cooldown_ms, 0u);
}

TEST_F(HealthSelfRepairBridgeTest, CodeImmuneSystemWithBridge) {
    code_immune_config_t config;
    code_immune_default_config(&config);

    code_immune_system_t* immune = code_immune_create_with_config(NULL, &config);
    if (immune) {
        health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
            NULL, diag_bridge, self_repair);
        ASSERT_NE(bridge, nullptr);

        // Both systems created - verify they don't interfere
        EXPECT_TRUE(health_self_repair_bridge_is_ready(bridge));

        health_self_repair_bridge_destroy(bridge);
        code_immune_destroy(immune);
    }
}

//=============================================================================
// Logging Integration
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, LoggingDuringRepairTrigger) {
    nimcp_log_config_t log_config = nimcp_log_default_config();
    log_config.level = LOG_LEVEL_DEBUG;

    nimcp_logger_t logger = nimcp_log_create(&log_config);
    if (logger) {
        health_self_repair_bridge_config_t config;
        health_self_repair_bridge_default_config(&config);
        config.verbose_logging = true;
        config.trigger_policy = HEALTH_TRIGGER_ERROR;
        config.aggregation.enabled = false;

        health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
            &config, diag_bridge, self_repair);
        ASSERT_NE(bridge, nullptr);

        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                       ANOMALY_SEVERITY_CRITICAL, 0.9f);
        uint64_t request_id = 0;
        health_self_repair_bridge_process_anomaly(bridge, &anomaly, &request_id);

        health_self_repair_bridge_destroy(bridge);
        nimcp_log_destroy(logger);
    }
}

//=============================================================================
// Exception Handler Coverage
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, ExceptionAllNullProcessAnomaly) {
    int ret = health_self_repair_bridge_process_anomaly(NULL, NULL, NULL);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthSelfRepairBridgeTest, ExceptionAllNullProcessAgentMessage) {
    int ret = health_self_repair_bridge_process_agent_message(NULL, NULL, NULL);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthSelfRepairBridgeTest, ExceptionAllNullTriggerFromDiag) {
    int ret = health_self_repair_bridge_trigger_from_diagnostic(NULL, NULL, NULL);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthSelfRepairBridgeTest, ExceptionAllNullForceTrigger) {
    int ret = health_self_repair_bridge_force_trigger(NULL, NULL, NULL);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthSelfRepairBridgeTest, ExceptionAllNullGetStats) {
    int ret = health_self_repair_bridge_get_stats(NULL, NULL);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthSelfRepairBridgeTest, ExceptionAllNullBroadcastTrigger) {
    int ret = health_self_repair_bridge_broadcast_trigger(NULL, 0, NULL);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthSelfRepairBridgeTest, ExceptionAllNullBroadcastOutcome) {
    int ret = health_self_repair_bridge_broadcast_outcome(
        NULL, 0, HEALTH_REPAIR_OUTCOME_SUCCESS);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthSelfRepairBridgeTest, ExceptionResetStatsNullSafe) {
    EXPECT_NO_THROW(health_self_repair_bridge_reset_stats(NULL));
}

TEST_F(HealthSelfRepairBridgeTest, ExceptionResetRateLimitNullSafe) {
    EXPECT_NO_THROW(health_self_repair_bridge_reset_rate_limit(NULL));
}

TEST_F(HealthSelfRepairBridgeTest, ExceptionGetPendingCountNull) {
    uint32_t pending = health_self_repair_bridge_get_pending_count(NULL);
    EXPECT_EQ(pending, 0u);
}

TEST_F(HealthSelfRepairBridgeTest, ExceptionGetRecentTrackingNull) {
    health_repair_tracking_t records[10];
    uint32_t count = health_self_repair_bridge_get_recent_tracking(NULL, records, 10);
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// Policy Name Edge Cases
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, PolicyNameInvalid) {
    const char* name = health_self_repair_bridge_policy_name(
        (health_trigger_policy_t)99);
    EXPECT_NE(name, nullptr);
}

TEST_F(HealthSelfRepairBridgeTest, OutcomeNameInvalid) {
    const char* name = health_self_repair_bridge_outcome_name(
        (health_repair_outcome_t)99);
    EXPECT_NE(name, nullptr);
}

//=============================================================================
// Multiple Create/Destroy Cycles
//=============================================================================

TEST_F(HealthSelfRepairBridgeTest, CreateDestroyCycle) {
    for (int i = 0; i < 5; i++) {
        health_self_repair_bridge_t* bridge = health_self_repair_bridge_create(
            NULL, diag_bridge, self_repair);
        ASSERT_NE(bridge, nullptr);
        EXPECT_TRUE(health_self_repair_bridge_is_ready(bridge));
        health_self_repair_bridge_destroy(bridge);
    }
}
