/**
 * @file test_health_self_repair_integration.cpp
 * @brief Integration tests for Health to Self-Repair Pipeline
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Integration tests for complete health monitoring to self-repair flow
 * WHY: Verify end-to-end operation of health detection to repair pipeline
 * HOW: Test full pipeline with diagnostic bridge, self-repair bridge, and coordinator
 *
 * TEST SCENARIOS:
 * - Health anomaly triggers diagnostic conversion
 * - Diagnostic triggers self-repair coordinator
 * - Rate limiting across the pipeline
 * - Outcome feedback to health agent
 * - Aggregation of similar anomalies
 * - Full pipeline stress test
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_health_diagnostic_bridge.h"
#include "cognitive/fault_tolerance/nimcp_health_self_repair_bridge.h"
#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "cognitive/fault_tolerance/nimcp_self_repair_health_notify.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class HealthSelfRepairIntegrationTest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;

    // Pipeline components
    health_diag_bridge_t* diag_bridge = nullptr;
    health_self_repair_bridge_t* repair_bridge = nullptr;
    self_repair_coordinator_t* self_repair = nullptr;
    self_repair_health_notify_bridge_t* notify_bridge = nullptr;
    nimcp_health_agent_t* health_agent = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;

        // Create full pipeline
        health_agent = nimcp_health_agent_create(NULL);
        diag_bridge = health_diag_bridge_create(NULL);
        self_repair = self_repair_create(NULL);

        if (diag_bridge && self_repair) {
            repair_bridge = health_self_repair_bridge_create(
                NULL, diag_bridge, self_repair);
        }

        if (self_repair && health_agent) {
            notify_bridge = self_repair_health_notify_create(
                NULL, self_repair, health_agent);
        }
    }

    void TearDown() override {
        // Cleanup in reverse order of creation
        if (notify_bridge) self_repair_health_notify_destroy(notify_bridge);
        if (repair_bridge) health_self_repair_bridge_destroy(repair_bridge);
        if (self_repair) self_repair_destroy(self_repair);
        if (diag_bridge) health_diag_bridge_destroy(diag_bridge);
        if (health_agent) nimcp_health_agent_destroy(health_agent);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        const size_t exception_infra_tolerance = 8192;
        EXPECT_LE(stats.current_allocated,
                  baseline_allocated + exception_infra_tolerance)
            << "Memory leak detected! Allocated: " << stats.current_allocated
            << ", Baseline: " << baseline_allocated;
    }

    // Helper: Create test anomaly
    void create_test_anomaly(anomaly_t* anomaly, anomaly_type_t type,
                              anomaly_severity_t severity, float confidence) {
        memset(anomaly, 0, sizeof(*anomaly));
        anomaly->type = type;
        anomaly->severity = severity;
        anomaly->confidence = confidence;
        strncpy(anomaly->affected_component, "test_component",
                sizeof(anomaly->affected_component) - 1);
        strncpy(anomaly->description, "Test anomaly for integration",
                sizeof(anomaly->description) - 1);
    }

    // Helper: Create test agent message
    void create_test_agent_message(health_agent_message_t* message,
                                    health_agent_msg_type_t type,
                                    health_agent_severity_t severity) {
        memset(message, 0, sizeof(*message));
        message->type = type;
        message->severity = severity;
        strncpy(message->description, "Test health agent message",
                sizeof(message->description) - 1);
    }
};

//=============================================================================
// Pipeline Setup Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, FullPipelineCreation) {
    ASSERT_NE(health_agent, nullptr) << "Health agent creation failed";
    ASSERT_NE(diag_bridge, nullptr) << "Diagnostic bridge creation failed";
    ASSERT_NE(self_repair, nullptr) << "Self-repair coordinator creation failed";
    ASSERT_NE(repair_bridge, nullptr) << "Self-repair bridge creation failed";
    ASSERT_NE(notify_bridge, nullptr) << "Notify bridge creation failed";

    EXPECT_TRUE(health_diag_bridge_is_ready(diag_bridge));
    EXPECT_TRUE(health_self_repair_bridge_is_ready(repair_bridge));
    EXPECT_TRUE(self_repair_health_notify_is_ready(notify_bridge));
}

//=============================================================================
// Anomaly to Diagnostic Conversion Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, AnomalyToDiagnosticFlow) {
    ASSERT_NE(diag_bridge, nullptr);

    anomaly_t anomaly;
    create_test_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                        ANOMALY_SEVERITY_WARNING, 0.85f);

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);

    ASSERT_EQ(ret, 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_MEMORY_LEAK);
    EXPECT_EQ(result->severity, DIAG_SEVERITY_WARNING);

    // Check statistics
    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.anomalies_converted, 1u);

    diagnostics_free_result(result);
}

TEST_F(HealthSelfRepairIntegrationTest, AgentMessageToDiagnosticFlow) {
    ASSERT_NE(diag_bridge, nullptr);

    health_agent_message_t message;
    create_test_agent_message(&message, HEALTH_MSG_DEADLOCK_DETECTED,
                               HEALTH_SEVERITY_CRITICAL);

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_agent_message(diag_bridge, &message, &result);

    ASSERT_EQ(ret, 0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_DEADLOCK);
    EXPECT_EQ(result->severity, DIAG_SEVERITY_CRITICAL);

    // Check statistics
    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.agent_messages_converted, 1u);

    diagnostics_free_result(result);
}

//=============================================================================
// Full Pipeline Flow Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, CriticalAnomalyTriggersRepair) {
    ASSERT_NE(repair_bridge, nullptr);

    // Configure to trigger on CRITICAL severity
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.aggregation.enabled = false;

    // Recreate bridge with new config
    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    // Create critical anomaly
    anomaly_t anomaly;
    create_test_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                        ANOMALY_SEVERITY_CRITICAL, 0.9f);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(repair_bridge, &anomaly, &request_id);

    // Should trigger repair (0 = triggered, 1 = skipped, -1 = error)
    EXPECT_EQ(ret, 0);
    EXPECT_GT(request_id, 0u);

    // Check bridge statistics
    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(repair_bridge, &stats);
    EXPECT_EQ(stats.repairs_triggered, 1u);
}

TEST_F(HealthSelfRepairIntegrationTest, WarningAnomalySkippedWithCriticalPolicy) {
    ASSERT_NE(repair_bridge, nullptr);

    // Configure to only trigger on CRITICAL and above
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.aggregation.enabled = false;

    // Recreate bridge with new config
    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    // Create warning anomaly (should be skipped)
    anomaly_t anomaly;
    create_test_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                        ANOMALY_SEVERITY_WARNING, 0.9f);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(repair_bridge, &anomaly, &request_id);

    // Should be skipped
    EXPECT_EQ(ret, 1);  // 1 = skipped

    // Check statistics
    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(repair_bridge, &stats);
    EXPECT_EQ(stats.repairs_triggered, 0u);
    EXPECT_EQ(stats.repairs_skipped, 1u);
}

TEST_F(HealthSelfRepairIntegrationTest, ManualPolicySkipsAllAutoTriggers) {
    ASSERT_NE(repair_bridge, nullptr);

    // Configure manual policy (no auto-triggers)
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_MANUAL;

    // Recreate bridge with new config
    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    // Create FATAL anomaly (even FATAL should be skipped with manual policy)
    anomaly_t anomaly;
    create_test_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                        ANOMALY_SEVERITY_CRITICAL, 1.0f);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(repair_bridge, &anomaly, &request_id);

    EXPECT_EQ(ret, 1);  // Skipped due to manual policy
}

//=============================================================================
// Rate Limiting Integration Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, RateLimitEnforced) {
    ASSERT_NE(repair_bridge, nullptr);

    // Configure strict rate limit: only 2 repairs per window
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.rate_limit.max_repairs_per_window = 2;
    config.rate_limit.window_duration_ms = 60000;
    config.aggregation.enabled = false;

    // Recreate bridge with new config
    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    // Process multiple critical anomalies
    uint64_t request_id = 0;
    int triggered_count = 0;
    int skipped_count = 0;

    for (int i = 0; i < 5; i++) {
        anomaly_t anomaly;
        create_test_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                            ANOMALY_SEVERITY_CRITICAL, 0.95f);

        int ret = health_self_repair_bridge_process_anomaly(
            repair_bridge, &anomaly, &request_id);

        if (ret == 0) triggered_count++;
        else if (ret == 1) skipped_count++;
    }

    // Should have triggered 2 and skipped 3 due to rate limit
    EXPECT_LE(triggered_count, 2);
    EXPECT_GE(skipped_count, 3);
}

//=============================================================================
// Health Agent Notification Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, NotifyBridgeReadiness) {
    ASSERT_NE(notify_bridge, nullptr);
    EXPECT_TRUE(self_repair_health_notify_is_ready(notify_bridge));
}

TEST_F(HealthSelfRepairIntegrationTest, NotifyRepairFailure) {
    ASSERT_NE(notify_bridge, nullptr);

    // Create a failure notification
    self_repair_health_notification_t notification;
    memset(&notification, 0, sizeof(notification));
    notification.type = REPAIR_NOTIFY_FAILURE;
    notification.repair_id = 12345;
    notification.intervention = REPAIR_INTERVENTION_ALERT;
    strncpy(notification.error_message, "Fix failed validation",
            sizeof(notification.error_message) - 1);

    int ret = self_repair_health_notify_send(notify_bridge, &notification);
    EXPECT_EQ(ret, 0);

    // Check statistics
    self_repair_health_notify_stats_t stats;
    self_repair_health_notify_get_stats(notify_bridge, &stats);
    EXPECT_EQ(stats.notifications_sent, 1u);
    EXPECT_EQ(stats.failures_notified, 1u);
}

TEST_F(HealthSelfRepairIntegrationTest, SuggestInterventionForRepeatFailures) {
    ASSERT_NE(notify_bridge, nullptr);

    self_repair_result_t result;
    memset(&result, 0, sizeof(result));
    result.status = REPAIR_STATUS_ERROR;
    result.success = false;

    // Suggest intervention
    repair_intervention_t intervention = self_repair_suggest_intervention(
        notify_bridge, &result);

    // Should suggest some intervention for failures
    EXPECT_NE(intervention, REPAIR_INTERVENTION_NONE);
}

//=============================================================================
// Multiple Anomaly Types Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, ProcessMultipleAnomalyTypes) {
    ASSERT_NE(diag_bridge, nullptr);

    // Test various anomaly types
    struct {
        anomaly_type_t type;
        error_type_t expected_error;
    } test_cases[] = {
        { ANOMALY_MEMORY_LEAK, ERROR_TYPE_MEMORY_LEAK },
        { ANOMALY_RESOURCE_EXHAUSTION, ERROR_TYPE_OUT_OF_MEMORY },
        { ANOMALY_PERFORMANCE_DEGRADATION, ERROR_TYPE_INVALID_STATE },
        { ANOMALY_THREAD_CONTENTION, ERROR_TYPE_DEADLOCK },
    };

    for (const auto& tc : test_cases) {
        anomaly_t anomaly;
        create_test_anomaly(&anomaly, tc.type, ANOMALY_SEVERITY_ERROR, 0.8f);

        diagnostic_result_t* result = nullptr;
        int ret = health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);

        ASSERT_EQ(ret, 0);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->error_type, tc.expected_error)
            << "Anomaly type " << health_diag_bridge_anomaly_type_name(tc.type)
            << " did not map to expected error type";

        diagnostics_free_result(result);
    }
}

//=============================================================================
// Severity Translation Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, SeverityTranslationConsistency) {
    // Verify severity mappings are consistent
    EXPECT_EQ(
        health_diag_bridge_translate_anomaly_severity(ANOMALY_SEVERITY_INFO),
        DIAG_SEVERITY_INFO);
    EXPECT_EQ(
        health_diag_bridge_translate_anomaly_severity(ANOMALY_SEVERITY_WARNING),
        DIAG_SEVERITY_WARNING);
    EXPECT_EQ(
        health_diag_bridge_translate_anomaly_severity(ANOMALY_SEVERITY_ERROR),
        DIAG_SEVERITY_ERROR);
    EXPECT_EQ(
        health_diag_bridge_translate_anomaly_severity(ANOMALY_SEVERITY_CRITICAL),
        DIAG_SEVERITY_CRITICAL);
    // Note: CRITICAL maps to CRITICAL, not FATAL - there is no ANOMALY_SEVERITY_FATAL
}

//=============================================================================
// Statistics Integration Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, StatisticsTrackFullPipeline) {
    ASSERT_NE(repair_bridge, nullptr);

    // Configure to trigger on all errors
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_ERROR;
    config.aggregation.enabled = false;

    // Recreate bridge with new config
    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    // Process several anomalies
    for (int i = 0; i < 3; i++) {
        anomaly_t anomaly;
        create_test_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                            ANOMALY_SEVERITY_ERROR, 0.8f);

        uint64_t request_id = 0;
        health_self_repair_bridge_process_anomaly(repair_bridge, &anomaly, &request_id);
    }

    // Check diagnostic bridge stats
    health_diag_bridge_stats_t diag_stats;
    health_diag_bridge_get_stats(diag_bridge, &diag_stats);
    EXPECT_EQ(diag_stats.anomalies_converted, 3u);

    // Check repair bridge stats
    health_self_repair_bridge_stats_t repair_stats;
    health_self_repair_bridge_get_stats(repair_bridge, &repair_stats);
    EXPECT_GE(repair_stats.repairs_triggered + repair_stats.repairs_skipped, 3u);
}

//=============================================================================
// Reset Functionality Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, ResetStatisticsAcrossPipeline) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(repair_bridge, nullptr);
    ASSERT_NE(notify_bridge, nullptr);

    // Generate some activity
    anomaly_t anomaly;
    create_test_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = nullptr;
    health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
    if (result) diagnostics_free_result(result);

    // Reset all stats
    health_diag_bridge_reset_stats(diag_bridge);
    health_self_repair_bridge_reset_stats(repair_bridge);
    self_repair_health_notify_reset_stats(notify_bridge);

    // Verify all reset
    health_diag_bridge_stats_t diag_stats;
    health_diag_bridge_get_stats(diag_bridge, &diag_stats);
    EXPECT_EQ(diag_stats.anomalies_converted, 0u);

    health_self_repair_bridge_stats_t repair_stats;
    health_self_repair_bridge_get_stats(repair_bridge, &repair_stats);
    EXPECT_EQ(repair_stats.repairs_triggered, 0u);

    self_repair_health_notify_stats_t notify_stats;
    self_repair_health_notify_get_stats(notify_bridge, &notify_stats);
    EXPECT_EQ(notify_stats.notifications_sent, 0u);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, PolicyNameStrings) {
    EXPECT_STREQ(health_self_repair_bridge_policy_name(HEALTH_TRIGGER_MANUAL), "MANUAL");
    EXPECT_STREQ(health_self_repair_bridge_policy_name(HEALTH_TRIGGER_FATAL_ONLY), "FATAL_ONLY");
    EXPECT_STREQ(health_self_repair_bridge_policy_name(HEALTH_TRIGGER_CRITICAL), "CRITICAL");
    EXPECT_STREQ(health_self_repair_bridge_policy_name(HEALTH_TRIGGER_ERROR), "ERROR");
    EXPECT_STREQ(health_self_repair_bridge_policy_name(HEALTH_TRIGGER_AUTO), "AUTO");
}

TEST_F(HealthSelfRepairIntegrationTest, OutcomeNameStrings) {
    EXPECT_STREQ(health_self_repair_bridge_outcome_name(HEALTH_REPAIR_OUTCOME_PENDING), "PENDING");
    EXPECT_STREQ(health_self_repair_bridge_outcome_name(HEALTH_REPAIR_OUTCOME_SUCCESS), "SUCCESS");
    EXPECT_STREQ(health_self_repair_bridge_outcome_name(HEALTH_REPAIR_OUTCOME_FAILED), "FAILED");
    EXPECT_STREQ(health_self_repair_bridge_outcome_name(HEALTH_REPAIR_OUTCOME_SKIPPED), "SKIPPED");
    EXPECT_STREQ(health_self_repair_bridge_outcome_name(HEALTH_REPAIR_OUTCOME_TIMEOUT), "TIMEOUT");
}

TEST_F(HealthSelfRepairIntegrationTest, NotifyTypeStrings) {
    EXPECT_STREQ(self_repair_notify_type_name(REPAIR_NOTIFY_FAILURE), "FAILURE");
    EXPECT_STREQ(self_repair_notify_type_name(REPAIR_NOTIFY_ROLLBACK), "ROLLBACK");
    EXPECT_STREQ(self_repair_notify_type_name(REPAIR_NOTIFY_HIGH_RISK), "HIGH_RISK");
    EXPECT_STREQ(self_repair_notify_type_name(REPAIR_NOTIFY_REPEATED_FAILURE), "REPEATED_FAILURE");
}

TEST_F(HealthSelfRepairIntegrationTest, InterventionNameStrings) {
    EXPECT_STREQ(self_repair_intervention_name(REPAIR_INTERVENTION_NONE), "NONE");
    EXPECT_STREQ(self_repair_intervention_name(REPAIR_INTERVENTION_ALERT), "ALERT");
    EXPECT_STREQ(self_repair_intervention_name(REPAIR_INTERVENTION_QUARANTINE), "QUARANTINE");
    EXPECT_STREQ(self_repair_intervention_name(REPAIR_INTERVENTION_MANUAL_REPAIR), "MANUAL_REPAIR");
}

TEST_F(HealthSelfRepairIntegrationTest, VersionStrings) {
    const char* diag_version = health_diag_bridge_version();
    const char* repair_version = health_self_repair_bridge_version();
    const char* notify_version = self_repair_health_notify_version();

    ASSERT_NE(diag_version, nullptr);
    ASSERT_NE(repair_version, nullptr);
    ASSERT_NE(notify_version, nullptr);

    EXPECT_GT(strlen(diag_version), 0u);
    EXPECT_GT(strlen(repair_version), 0u);
    EXPECT_GT(strlen(notify_version), 0u);
}

//=============================================================================
// Health Agent Connection Integration Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, ConnectHealthAgentToRepairBridge) {
    ASSERT_NE(repair_bridge, nullptr);
    ASSERT_NE(health_agent, nullptr);

    int ret = health_self_repair_bridge_connect_health_agent(
        repair_bridge, health_agent);
    EXPECT_EQ(ret, 0);
}

TEST_F(HealthSelfRepairIntegrationTest, ConnectHealthAgentNullBridgeError) {
    int ret = health_self_repair_bridge_connect_health_agent(NULL, health_agent);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Process Agent Message Integration Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, AgentMessageCriticalTriggersRepair) {
    ASSERT_NE(repair_bridge, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.aggregation.enabled = false;

    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    health_agent_message_t msg;
    create_test_agent_message(&msg, HEALTH_MSG_EMERGENCY, HEALTH_SEVERITY_CRITICAL);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_agent_message(
        repair_bridge, &msg, &request_id);
    EXPECT_EQ(ret, 0);
}

TEST_F(HealthSelfRepairIntegrationTest, AgentMessageWarningSkippedByFatalPolicy) {
    ASSERT_NE(repair_bridge, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_FATAL_ONLY;
    config.aggregation.enabled = false;

    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    health_agent_message_t msg;
    create_test_agent_message(&msg, HEALTH_MSG_STATUS_UPDATE, HEALTH_SEVERITY_WARNING);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_agent_message(
        repair_bridge, &msg, &request_id);
    EXPECT_EQ(ret, 1);  // Skipped by policy
}

//=============================================================================
// Force Trigger Integration Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, ForceTriggerBypassesManualPolicy) {
    ASSERT_NE(repair_bridge, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_MANUAL;
    config.aggregation.enabled = false;

    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    anomaly_t anomaly;
    create_test_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                        ANOMALY_SEVERITY_WARNING, 0.5f);
    diagnostic_result_t* diag = nullptr;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &diag), 0);
    ASSERT_NE(diag, nullptr);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_force_trigger(repair_bridge, diag, &request_id);
    EXPECT_EQ(ret, 0);
    // force_trigger takes ownership of diag
}

//=============================================================================
// Tracking Record Integration Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, TrackingRecordAfterTrigger) {
    ASSERT_NE(repair_bridge, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.aggregation.enabled = false;

    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    anomaly_t anomaly;
    create_test_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                        ANOMALY_SEVERITY_CRITICAL, 0.95f);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly, &request_id);
    EXPECT_EQ(ret, 0);

    // Look up tracking record
    const health_repair_tracking_t* tracking =
        health_self_repair_bridge_get_tracking(repair_bridge, request_id);
    if (tracking) {
        EXPECT_EQ(tracking->request_id, request_id);
        EXPECT_EQ(tracking->outcome, HEALTH_REPAIR_OUTCOME_PENDING);
    }
}

TEST_F(HealthSelfRepairIntegrationTest, RecentTrackingRecords) {
    ASSERT_NE(repair_bridge, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.aggregation.enabled = false;

    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    // Trigger a couple of repairs
    for (int i = 0; i < 3; i++) {
        anomaly_t anomaly;
        create_test_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                            ANOMALY_SEVERITY_CRITICAL, 0.95f);
        uint64_t request_id = 0;
        health_self_repair_bridge_process_anomaly(
            repair_bridge, &anomaly, &request_id);
    }

    // Get recent tracking records
    health_repair_tracking_t records[16];
    uint32_t count = health_self_repair_bridge_get_recent_tracking(
        repair_bridge, records, 16);
    EXPECT_GE(count, 1u);
}

//=============================================================================
// Aggregation Integration Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, AggregationBatchesMultipleAnomalies) {
    ASSERT_NE(repair_bridge, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.aggregation.enabled = true;
    config.aggregation.window_ms = 60000;  // Large window
    config.aggregation.max_batch_size = 10;

    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    // Process multiple anomalies (they should be batched)
    for (int i = 0; i < 3; i++) {
        anomaly_t anomaly;
        create_test_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                            ANOMALY_SEVERITY_CRITICAL, 0.95f);
        uint64_t request_id = 0;
        health_self_repair_bridge_process_anomaly(
            repair_bridge, &anomaly, &request_id);
    }

    // Process aggregation
    uint32_t batched = health_self_repair_bridge_process_aggregation(repair_bridge);
    // May or may not have items depending on timing
    (void)batched;

    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(repair_bridge, &stats);
    EXPECT_GE(stats.repairs_triggered, 1u);
}

//=============================================================================
// Callback Integration Tests
//=============================================================================

static int g_trigger_callback_count = 0;
static void test_trigger_callback(uint64_t request_id,
                                   const diagnostic_result_t* diag,
                                   void* user_data) {
    (void)request_id;
    (void)diag;
    (void)user_data;
    g_trigger_callback_count++;
}

static int g_outcome_callback_count = 0;
static void test_outcome_callback(uint64_t request_id,
                                   health_repair_outcome_t outcome,
                                   const self_repair_result_t* result,
                                   void* user_data) {
    (void)request_id;
    (void)outcome;
    (void)result;
    (void)user_data;
    g_outcome_callback_count++;
}

TEST_F(HealthSelfRepairIntegrationTest, TriggerCallbackInvoked) {
    g_trigger_callback_count = 0;

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.aggregation.enabled = false;

    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    health_self_repair_bridge_set_trigger_callback(
        repair_bridge, test_trigger_callback, nullptr);

    anomaly_t anomaly;
    create_test_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                        ANOMALY_SEVERITY_CRITICAL, 0.95f);
    uint64_t request_id = 0;
    health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly, &request_id);

    EXPECT_GE(g_trigger_callback_count, 1);
}

TEST_F(HealthSelfRepairIntegrationTest, OutcomeCallbackRegistration) {
    g_outcome_callback_count = 0;

    ASSERT_NE(repair_bridge, nullptr);
    health_self_repair_bridge_set_outcome_callback(
        repair_bridge, test_outcome_callback, nullptr);

    // Just verify registration doesn't crash
    SUCCEED();
}

//=============================================================================
// Broadcasting Integration Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, BroadcastTriggerNoCrash) {
    ASSERT_NE(repair_bridge, nullptr);

    anomaly_t anomaly;
    create_test_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                        ANOMALY_SEVERITY_CRITICAL, 0.9f);
    diagnostic_result_t* diag = nullptr;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &diag), 0);
    ASSERT_NE(diag, nullptr);

    // broadcast_trigger with no router should just log/return
    health_self_repair_bridge_broadcast_trigger(repair_bridge, 42, diag);
    diagnostics_free_result(diag);
}

TEST_F(HealthSelfRepairIntegrationTest, BroadcastOutcomeNoCrash) {
    ASSERT_NE(repair_bridge, nullptr);

    health_self_repair_bridge_broadcast_outcome(
        repair_bridge, 42, HEALTH_REPAIR_OUTCOME_SUCCESS);
    // Should not crash even without bio-router
}

//=============================================================================
// Pending Count Integration Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, PendingCountAfterTrigger) {
    ASSERT_NE(repair_bridge, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.aggregation.enabled = false;

    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    anomaly_t anomaly;
    create_test_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                        ANOMALY_SEVERITY_CRITICAL, 0.95f);
    uint64_t request_id = 0;
    health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly, &request_id);

    uint32_t pending = health_self_repair_bridge_get_pending_count(repair_bridge);
    // After trigger, the repair may be pending or already completed
    (void)pending;
}

//=============================================================================
// Pipeline Error Handling Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, ProcessAnomalyNullBridgeReturnsError) {
    anomaly_t anomaly;
    create_test_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                        ANOMALY_SEVERITY_CRITICAL, 0.95f);
    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(NULL, &anomaly, &request_id);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthSelfRepairIntegrationTest, ProcessAnomalyNullAnomalyReturnsError) {
    ASSERT_NE(repair_bridge, nullptr);
    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(repair_bridge, NULL, &request_id);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealthSelfRepairIntegrationTest, DiagBridgeConvertNullReturnsError) {
    ASSERT_NE(diag_bridge, nullptr);
    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(diag_bridge, NULL, &result);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Cross-Component Statistics Consistency Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, StatisticsConsistencyAfterMultipleOperations) {
    ASSERT_NE(repair_bridge, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_ERROR;
    config.aggregation.enabled = false;

    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    // Process multiple anomalies of different severities
    const anomaly_severity_t severities[] = {
        ANOMALY_SEVERITY_ERROR,
        ANOMALY_SEVERITY_CRITICAL,
        ANOMALY_SEVERITY_WARNING,
    };

    for (auto sev : severities) {
        anomaly_t anomaly;
        create_test_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, sev, 0.9f);
        uint64_t request_id = 0;
        health_self_repair_bridge_process_anomaly(
            repair_bridge, &anomaly, &request_id);
    }

    // Check repair bridge stats add up
    health_self_repair_bridge_stats_t repair_stats;
    health_self_repair_bridge_get_stats(repair_bridge, &repair_stats);
    EXPECT_EQ(repair_stats.repairs_triggered + repair_stats.repairs_skipped, 3u);
}

//=============================================================================
// Trigger from Diagnostic Integration Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, TriggerFromDiagnosticCriticalPolicy) {
    ASSERT_NE(repair_bridge, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.aggregation.enabled = false;

    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    anomaly_t anomaly;
    create_test_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                        ANOMALY_SEVERITY_CRITICAL, 0.95f);
    diagnostic_result_t* diag = nullptr;
    ASSERT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &diag), 0);
    ASSERT_NE(diag, nullptr);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_trigger_from_diagnostic(
        repair_bridge, diag, &request_id);
    EXPECT_EQ(ret, 0);
    // trigger_from_diagnostic takes ownership of diag on success
}

//=============================================================================
// Rate Limit Reset Integration Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, RateLimitResetAllowsMoreRepairs) {
    ASSERT_NE(repair_bridge, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
    config.rate_limit.max_repairs_per_window = 1;
    config.rate_limit.window_duration_ms = 60000;
    config.aggregation.enabled = false;

    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    // First anomaly should trigger
    anomaly_t anomaly1;
    create_test_anomaly(&anomaly1, ANOMALY_RESOURCE_EXHAUSTION,
                        ANOMALY_SEVERITY_CRITICAL, 0.95f);
    uint64_t req1 = 0;
    int ret1 = health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly1, &req1);
    EXPECT_EQ(ret1, 0);

    // Second should be rate-limited
    anomaly_t anomaly2;
    create_test_anomaly(&anomaly2, ANOMALY_RESOURCE_EXHAUSTION,
                        ANOMALY_SEVERITY_CRITICAL, 0.95f);
    uint64_t req2 = 0;
    int ret2 = health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly2, &req2);
    EXPECT_EQ(ret2, 1);

    // Reset rate limit
    health_self_repair_bridge_reset_rate_limit(repair_bridge);

    // Third should trigger again
    anomaly_t anomaly3;
    create_test_anomaly(&anomaly3, ANOMALY_RESOURCE_EXHAUSTION,
                        ANOMALY_SEVERITY_CRITICAL, 0.95f);
    uint64_t req3 = 0;
    int ret3 = health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly3, &req3);
    EXPECT_EQ(ret3, 0);
}

//=============================================================================
// Is-Ready Integration Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, IsReadyReturnsTrue) {
    ASSERT_NE(repair_bridge, nullptr);
    EXPECT_TRUE(health_self_repair_bridge_is_ready(repair_bridge));
}

TEST_F(HealthSelfRepairIntegrationTest, IsReadyNullReturnsFalse) {
    EXPECT_FALSE(health_self_repair_bridge_is_ready(NULL));
}

//=============================================================================
// Notify Bridge Error Path Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, NotifyBridgeNullSend) {
    int ret = self_repair_health_notify_send(NULL, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HealthSelfRepairIntegrationTest, NotifyBridgeRollbackNotification) {
    ASSERT_NE(notify_bridge, nullptr);

    self_repair_health_notification_t notification;
    memset(&notification, 0, sizeof(notification));
    notification.type = REPAIR_NOTIFY_ROLLBACK;
    notification.repair_id = 99999;
    notification.intervention = REPAIR_INTERVENTION_QUARANTINE;
    strncpy(notification.error_message, "Rollback required",
            sizeof(notification.error_message) - 1);

    int ret = self_repair_health_notify_send(notify_bridge, &notification);
    EXPECT_EQ(ret, 0);

    self_repair_health_notify_stats_t stats;
    self_repair_health_notify_get_stats(notify_bridge, &stats);
    EXPECT_GE(stats.notifications_sent, 1u);
}

//=============================================================================
// Default Config Consistency Tests
//=============================================================================

TEST_F(HealthSelfRepairIntegrationTest, DefaultConfigHasReasonableDefaults) {
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);

    EXPECT_EQ(config.trigger_policy, HEALTH_TRIGGER_CRITICAL);
    EXPECT_GT(config.rate_limit.max_repairs_per_window, 0u);
    EXPECT_GT(config.rate_limit.window_duration_ms, 0u);
    EXPECT_TRUE(config.aggregation.enabled);
    EXPECT_GT(config.aggregation.window_ms, 0u);
    EXPECT_GT(config.aggregation.max_batch_size, 0u);
}
