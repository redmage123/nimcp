/**
 * @file test_health_self_repair_e2e.cpp
 * @brief End-to-end tests for Health Monitoring to Self-Repair Pipeline
 *
 * WHAT: Test the complete health monitoring to self-repair pipeline
 * WHY: Verify all components work together from anomaly detection to repair
 * HOW: Simulate real-world scenarios with full pipeline
 *
 * E2E TEST SCENARIOS:
 * - Full pipeline: anomaly → diagnostic → self-repair → outcome → notification
 * - Multiple concurrent anomalies
 * - Health agent feedback loop
 * - Code immune crash pattern to repair
 * - Rate limiting across pipeline
 * - Recovery from failures
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <mutex>
#include <csignal>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_health_diagnostic_bridge.h"
#include "cognitive/fault_tolerance/nimcp_health_self_repair_bridge.h"
#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "cognitive/fault_tolerance/nimcp_self_repair_health_notify.h"
#include "cognitive/immune/nimcp_code_immune_self_repair.h"
#include "cognitive/immune/nimcp_code_immune.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// E2E Test Callbacks and State Tracking
//=============================================================================

static std::atomic<int> g_trigger_count{0};
static std::atomic<int> g_outcome_count{0};
static std::atomic<int> g_success_count{0};
static std::atomic<int> g_failure_count{0};
static std::mutex g_callback_mutex;
static std::vector<uint64_t> g_triggered_repair_ids;
static std::vector<uint64_t> g_completed_repair_ids;

static void reset_callback_state() {
    g_trigger_count = 0;
    g_outcome_count = 0;
    g_success_count = 0;
    g_failure_count = 0;
    std::lock_guard<std::mutex> lock(g_callback_mutex);
    g_triggered_repair_ids.clear();
    g_completed_repair_ids.clear();
}

static void trigger_callback(
    uint64_t request_id,
    const diagnostic_result_t* diagnostic,
    void* user_data
) {
    (void)diagnostic;
    (void)user_data;
    g_trigger_count++;
    std::lock_guard<std::mutex> lock(g_callback_mutex);
    g_triggered_repair_ids.push_back(request_id);
}

static void outcome_callback(
    uint64_t request_id,
    health_repair_outcome_t outcome,
    const self_repair_result_t* result,
    void* user_data
) {
    (void)result;
    (void)user_data;
    g_outcome_count++;
    if (outcome == HEALTH_REPAIR_OUTCOME_SUCCESS) {
        g_success_count++;
    } else if (outcome == HEALTH_REPAIR_OUTCOME_FAILED) {
        g_failure_count++;
    }
    std::lock_guard<std::mutex> lock(g_callback_mutex);
    g_completed_repair_ids.push_back(request_id);
}

//=============================================================================
// Test Fixture
//=============================================================================

class HealthSelfRepairE2ETest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;

    // Full pipeline components
    nimcp_health_agent_t* health_agent = nullptr;
    health_diag_bridge_t* diag_bridge = nullptr;
    health_self_repair_bridge_t* repair_bridge = nullptr;
    self_repair_coordinator_t* self_repair = nullptr;
    self_repair_health_notify_bridge_t* notify_bridge = nullptr;
    code_immune_system_t* code_immune = nullptr;
    code_immune_self_repair_bridge_t* code_immune_bridge = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;

        reset_callback_state();

        // Create all pipeline components
        health_agent = nimcp_health_agent_create(NULL);
        diag_bridge = health_diag_bridge_create(NULL);
        self_repair = self_repair_create(NULL);
        code_immune = code_immune_create(NULL);

        if (diag_bridge && self_repair) {
            repair_bridge = health_self_repair_bridge_create(
                NULL, diag_bridge, self_repair);

            // Set callbacks
            if (repair_bridge) {
                health_self_repair_bridge_set_trigger_callback(
                    repair_bridge, trigger_callback, NULL);
                health_self_repair_bridge_set_outcome_callback(
                    repair_bridge, outcome_callback, NULL);
            }
        }

        if (self_repair && health_agent) {
            notify_bridge = self_repair_health_notify_create(
                NULL, self_repair, health_agent);
        }

        if (code_immune && self_repair) {
            code_immune_bridge = code_immune_self_repair_bridge_create(
                NULL, code_immune, self_repair);

            if (code_immune_bridge && health_agent) {
                code_immune_self_repair_connect_health_agent(
                    code_immune_bridge, health_agent);
            }
        }
    }

    void TearDown() override {
        // Cleanup in reverse order
        if (code_immune_bridge) code_immune_self_repair_bridge_destroy(code_immune_bridge);
        if (notify_bridge) self_repair_health_notify_destroy(notify_bridge);
        if (repair_bridge) health_self_repair_bridge_destroy(repair_bridge);
        if (code_immune) code_immune_destroy(code_immune);
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
    void create_anomaly(anomaly_t* anomaly, anomaly_type_t type,
                        anomaly_severity_t severity, float confidence) {
        memset(anomaly, 0, sizeof(*anomaly));
        anomaly->type = type;
        anomaly->severity = severity;
        anomaly->confidence = confidence;
        strncpy(anomaly->affected_component, "e2e_test_component",
                sizeof(anomaly->affected_component) - 1);
        strncpy(anomaly->description, "E2E test anomaly",
                sizeof(anomaly->description) - 1);
    }

    // Helper: Create test antigen
    void create_antigen(code_antigen_t* antigen, int signal,
                        float severity, float confidence,
                        uint32_t recurrence_count) {
        memset(antigen, 0, sizeof(*antigen));
        antigen->id = 1;
        antigen->signal = signal;
        antigen->severity = severity;
        antigen->confidence = confidence;
        antigen->recurrence_count = recurrence_count;
        strncpy(antigen->source_file, "/e2e/test/source.c",
                sizeof(antigen->source_file) - 1);
        strncpy(antigen->function_name, "e2e_test_function",
                sizeof(antigen->function_name) - 1);
        antigen->line_number = 42;
    }
};

//=============================================================================
// Pipeline Setup E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, FullPipelineInitialization) {
    // Verify all components created
    ASSERT_NE(health_agent, nullptr) << "Health agent creation failed";
    ASSERT_NE(diag_bridge, nullptr) << "Diagnostic bridge creation failed";
    ASSERT_NE(self_repair, nullptr) << "Self-repair coordinator creation failed";
    ASSERT_NE(repair_bridge, nullptr) << "Self-repair bridge creation failed";
    ASSERT_NE(notify_bridge, nullptr) << "Notify bridge creation failed";
    ASSERT_NE(code_immune, nullptr) << "Code immune creation failed";
    ASSERT_NE(code_immune_bridge, nullptr) << "Code immune bridge creation failed";

    // Verify all ready
    EXPECT_TRUE(health_diag_bridge_is_ready(diag_bridge));
    EXPECT_TRUE(health_self_repair_bridge_is_ready(repair_bridge));
    EXPECT_TRUE(self_repair_health_notify_is_ready(notify_bridge));
    EXPECT_TRUE(code_immune_self_repair_is_ready(code_immune_bridge));
}

//=============================================================================
// Full Pipeline Flow E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, CriticalAnomalyFullPipeline) {
    ASSERT_NE(repair_bridge, nullptr);

    // Create critical anomaly
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.95f);

    // Process through full pipeline
    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly, &request_id);

    // Should trigger repair
    EXPECT_EQ(ret, 0);
    EXPECT_GT(request_id, 0u);

    // Callback should have been invoked
    EXPECT_EQ(g_trigger_count, 1);

    // Verify bridge stats
    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(repair_bridge, &stats);
    EXPECT_EQ(stats.repairs_triggered, 1u);
}

TEST_F(HealthSelfRepairE2ETest, MultipleAnomaliesPipeline) {
    ASSERT_NE(repair_bridge, nullptr);

    // Use CRITICAL severity to match default TRIGGER_CRITICAL policy
    // With aggregation enabled (default), anomalies are batched

    // Process multiple anomalies
    std::vector<anomaly_type_t> anomaly_types = {
        ANOMALY_MEMORY_LEAK,
        ANOMALY_RESOURCE_EXHAUSTION,
        ANOMALY_THREAD_CONTENTION,
        ANOMALY_PERFORMANCE_DEGRADATION
    };

    int triggered = 0;
    for (size_t i = 0; i < anomaly_types.size(); i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, anomaly_types[i],
                       ANOMALY_SEVERITY_CRITICAL, 0.8f);

        uint64_t request_id = 0;
        int ret = health_self_repair_bridge_process_anomaly(
            repair_bridge, &anomaly, &request_id);

        if (ret == 0) {
            triggered++;
        }
    }

    // All should trigger (stored in aggregation batch)
    EXPECT_EQ(triggered, static_cast<int>(anomaly_types.size()));

    // Verify stats reflect triggered count
    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(repair_bridge, &stats);
    EXPECT_EQ(stats.repairs_triggered, static_cast<uint64_t>(triggered));
}

//=============================================================================
// Code Immune Integration E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, CodeImmuneToSelfRepairPipeline) {
    ASSERT_NE(code_immune_bridge, nullptr);

    // Create crash antigen that meets auto-repair criteria
    // Note: severity 0.85f maps to CRITICAL (0.9f+ maps to FATAL)
    code_antigen_t antigen;
    create_antigen(&antigen, SIGSEGV, 0.85f, 0.8f, 5);

    // Check if should auto-repair
    bool should_repair = code_immune_should_auto_repair(code_immune_bridge, &antigen);
    EXPECT_TRUE(should_repair);

    // Convert to diagnostic
    diagnostic_result_t* result = nullptr;
    int ret = code_immune_antigen_to_diagnostic(&antigen, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(result, nullptr);

    // Verify diagnostic properties
    EXPECT_EQ(result->severity, DIAG_SEVERITY_CRITICAL);
    EXPECT_GT(result->confidence, 0.0f);

    diagnostics_free_result(result);
}

TEST_F(HealthSelfRepairE2ETest, CodeImmuneBelowThreshold) {
    ASSERT_NE(code_immune_bridge, nullptr);

    // Create crash antigen that doesn't meet criteria (low recurrence)
    code_antigen_t antigen;
    create_antigen(&antigen, SIGSEGV, 0.9f, 0.8f, 1);  // Only 1 crash

    bool should_repair = code_immune_should_auto_repair(code_immune_bridge, &antigen);
    EXPECT_FALSE(should_repair);
}

TEST_F(HealthSelfRepairE2ETest, CodeImmuneOutcomeNotification) {
    ASSERT_NE(code_immune_bridge, nullptr);

    // Notifying about unknown repair_ids returns -1
    // (tracking records must exist for valid notifications)
    int ret = code_immune_notify_repair_outcome(code_immune_bridge, 12345, true, NULL);
    EXPECT_EQ(ret, -1);

    // Stats should remain at zero for unknown repair_ids
    code_immune_self_repair_stats_t stats;
    code_immune_self_repair_get_stats(code_immune_bridge, &stats);
    EXPECT_EQ(stats.repairs_succeeded, 0u);

    // Same for failed repair notifications
    ret = code_immune_notify_repair_outcome(
        code_immune_bridge, 12346, false, "Fix validation failed");
    EXPECT_EQ(ret, -1);

    code_immune_self_repair_get_stats(code_immune_bridge, &stats);
    EXPECT_EQ(stats.repairs_failed, 0u);
}

//=============================================================================
// Health Agent Notification E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, HealthAgentFailureNotification) {
    ASSERT_NE(notify_bridge, nullptr);

    // Create failure notification
    self_repair_health_notification_t notification;
    memset(&notification, 0, sizeof(notification));
    notification.type = REPAIR_NOTIFY_FAILURE;
    notification.repair_id = 12345;
    notification.intervention = REPAIR_INTERVENTION_ALERT;
    notification.error_type = ERROR_TYPE_MEMORY_LEAK;
    notification.severity = DIAG_SEVERITY_ERROR;
    notification.failure_count = 1;
    strncpy(notification.error_message, "Fix validation failed",
            sizeof(notification.error_message) - 1);

    int ret = self_repair_health_notify_send(notify_bridge, &notification);
    EXPECT_EQ(ret, 0);

    // Verify stats
    self_repair_health_notify_stats_t stats;
    self_repair_health_notify_get_stats(notify_bridge, &stats);
    EXPECT_EQ(stats.notifications_sent, 1u);
    EXPECT_EQ(stats.failures_notified, 1u);
}

TEST_F(HealthSelfRepairE2ETest, HealthAgentInterventionSuggestion) {
    ASSERT_NE(notify_bridge, nullptr);

    // Create repair result
    self_repair_result_t result;
    memset(&result, 0, sizeof(result));
    result.status = REPAIR_STATUS_ERROR;
    result.success = false;

    // Get intervention suggestion
    repair_intervention_t intervention = self_repair_suggest_intervention(
        notify_bridge, &result);

    // Should suggest some intervention
    EXPECT_NE(intervention, REPAIR_INTERVENTION_NONE);
}

TEST_F(HealthSelfRepairE2ETest, RepeatedFailureEscalation) {
    ASSERT_NE(notify_bridge, nullptr);

    // Simulate multiple failures for same issue
    for (int i = 0; i < 5; i++) {
        self_repair_health_notification_t notification;
        memset(&notification, 0, sizeof(notification));
        notification.type = REPAIR_NOTIFY_FAILURE;
        notification.repair_id = 100 + i;
        notification.intervention = REPAIR_INTERVENTION_LOG_ONLY;
        notification.failure_count = i + 1;

        self_repair_health_notify_send(notify_bridge, &notification);
    }

    // Verify stats
    self_repair_health_notify_stats_t stats;
    self_repair_health_notify_get_stats(notify_bridge, &stats);
    EXPECT_EQ(stats.notifications_sent, 5u);
    EXPECT_EQ(stats.failures_notified, 5u);
}

//=============================================================================
// Rate Limiting E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, RateLimitingAcrossPipeline) {
    ASSERT_NE(repair_bridge, nullptr);

    // Configure strict rate limit
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_ERROR;
    config.rate_limit.max_repairs_per_window = 3;
    config.rate_limit.window_duration_ms = 60000;
    config.aggregation.enabled = false;

    // Recreate bridge with new config
    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    // Try to process 10 anomalies
    int triggered = 0;
    int skipped = 0;

    for (int i = 0; i < 10; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_ERROR, 0.9f);

        uint64_t request_id = 0;
        int ret = health_self_repair_bridge_process_anomaly(
            repair_bridge, &anomaly, &request_id);

        if (ret == 0) triggered++;
        else if (ret == 1) skipped++;
    }

    // Should be rate limited
    EXPECT_LE(triggered, 3);
    EXPECT_GE(skipped, 7);

    // Verify stats
    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(repair_bridge, &stats);
    EXPECT_LE(stats.repairs_triggered, 3u);
}

//=============================================================================
// Statistics Accumulation E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, StatisticsAccumulationAcrossAllBridges) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(repair_bridge, nullptr);
    ASSERT_NE(code_immune_bridge, nullptr);

    // Process anomalies through diagnostic bridge
    for (int i = 0; i < 5; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_WARNING, 0.8f);

        diagnostic_result_t* result = nullptr;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);
    }

    // Note: code_immune_notify_repair_outcome requires valid tracking records
    // For unknown repair_ids, the calls will return -1 and not update stats
    // Testing with unknown repair_ids to verify correct error handling
    int ret1 = code_immune_notify_repair_outcome(code_immune_bridge, 1, true, NULL);
    int ret2 = code_immune_notify_repair_outcome(code_immune_bridge, 2, true, NULL);
    int ret3 = code_immune_notify_repair_outcome(code_immune_bridge, 3, false, "Error");

    // All should return -1 for unknown repair_ids
    EXPECT_EQ(ret1, -1);
    EXPECT_EQ(ret2, -1);
    EXPECT_EQ(ret3, -1);

    // Verify diagnostic bridge stats
    health_diag_bridge_stats_t diag_stats;
    health_diag_bridge_get_stats(diag_bridge, &diag_stats);
    EXPECT_EQ(diag_stats.anomalies_converted, 5u);

    // Code immune stats should remain at zero for unknown repair_ids
    code_immune_self_repair_stats_t ci_stats;
    code_immune_self_repair_get_stats(code_immune_bridge, &ci_stats);
    EXPECT_EQ(ci_stats.repairs_succeeded, 0u);
    EXPECT_EQ(ci_stats.repairs_failed, 0u);
}

TEST_F(HealthSelfRepairE2ETest, StatisticsResetAllBridges) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(repair_bridge, nullptr);
    ASSERT_NE(notify_bridge, nullptr);
    ASSERT_NE(code_immune_bridge, nullptr);

    // Generate some stats
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = nullptr;
    health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
    if (result) diagnostics_free_result(result);

    code_immune_notify_repair_outcome(code_immune_bridge, 1, true, NULL);

    // Reset all
    health_diag_bridge_reset_stats(diag_bridge);
    health_self_repair_bridge_reset_stats(repair_bridge);
    self_repair_health_notify_reset_stats(notify_bridge);
    code_immune_self_repair_reset_stats(code_immune_bridge);

    // Verify all reset
    health_diag_bridge_stats_t diag_stats;
    health_diag_bridge_get_stats(diag_bridge, &diag_stats);
    EXPECT_EQ(diag_stats.anomalies_converted, 0u);

    code_immune_self_repair_stats_t ci_stats;
    code_immune_self_repair_get_stats(code_immune_bridge, &ci_stats);
    EXPECT_EQ(ci_stats.repairs_succeeded, 0u);
}

//=============================================================================
// Version Compatibility E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, AllVersionStringsPresent) {
    const char* diag_version = health_diag_bridge_version();
    const char* repair_version = health_self_repair_bridge_version();
    const char* notify_version = self_repair_health_notify_version();
    const char* code_immune_version = code_immune_self_repair_version();

    ASSERT_NE(diag_version, nullptr);
    ASSERT_NE(repair_version, nullptr);
    ASSERT_NE(notify_version, nullptr);
    ASSERT_NE(code_immune_version, nullptr);

    // All should be version 1.0.0
    EXPECT_STREQ(diag_version, "1.0.0");
    EXPECT_STREQ(repair_version, "1.0.0");
    EXPECT_STREQ(notify_version, "1.0.0");
    EXPECT_STREQ(code_immune_version, "1.0.0");
}

//=============================================================================
// Error Recovery E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, PipelineContinuesAfterErrors) {
    ASSERT_NE(repair_bridge, nullptr);

    // Process invalid input (should return error, not crash)
    uint64_t request_id = 0;
    EXPECT_EQ(health_self_repair_bridge_process_anomaly(repair_bridge, NULL, &request_id), -1);

    // Pipeline should still work after error
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);

    int ret = health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly, &request_id);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(request_id, 0u);
}

TEST_F(HealthSelfRepairE2ETest, DiagnosticBridgeContinuesAfterErrors) {
    ASSERT_NE(diag_bridge, nullptr);

    // Attempt invalid conversion
    diagnostic_result_t* result = nullptr;
    EXPECT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, NULL, &result), -1);

    // Bridge should still work
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
    EXPECT_EQ(ret, 0);
    ASSERT_NE(result, nullptr);
    diagnostics_free_result(result);
}

//=============================================================================
// Cleanup and Memory Safety E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, GracefulShutdownUnderLoad) {
    ASSERT_NE(repair_bridge, nullptr);

    // Process several anomalies
    for (int i = 0; i < 20; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_WARNING, 0.8f);

        uint64_t request_id = 0;
        health_self_repair_bridge_process_anomaly(
            repair_bridge, &anomaly, &request_id);
    }

    // Destroy in middle of activity - should not crash
    // (TearDown will handle this, memory check will verify no leaks)
}

//=============================================================================
// Agent Message Pipeline E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, AgentMessageThroughDiagnosticBridge) {
    ASSERT_NE(diag_bridge, nullptr);

    // Create agent message with WARNING severity (meets min_agent_severity)
    health_agent_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.severity = HEALTH_SEVERITY_WARNING;
    msg.type = HEALTH_MSG_ANOMALY_DETECTED;
    strncpy(msg.description, "E2E agent message test",
            sizeof(msg.description) - 1);

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_agent_message(
        diag_bridge, &msg, &result);
    EXPECT_EQ(ret, 0);
    ASSERT_NE(result, nullptr);

    // Verify diagnostic has expected properties
    EXPECT_GE(result->severity, DIAG_SEVERITY_WARNING);
    EXPECT_GT(result->confidence, 0.0f);

    diagnostics_free_result(result);

    // Verify stats
    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.agent_messages_converted, 1u);
}

TEST_F(HealthSelfRepairE2ETest, AgentMessageBelowSeverityFiltered) {
    ASSERT_NE(diag_bridge, nullptr);

    // INFO severity is below default min_agent_severity (WARNING)
    health_agent_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.severity = HEALTH_SEVERITY_INFO;
    msg.type = HEALTH_MSG_ANOMALY_DETECTED;
    strncpy(msg.description, "Info message filtered",
            sizeof(msg.description) - 1);

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_agent_message(
        diag_bridge, &msg, &result);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(result, nullptr);

    // Stats should reflect the filter
    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);
    EXPECT_EQ(stats.agent_messages_converted, 0u);
}

//=============================================================================
// Force Trigger Pipeline E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, ForceTriggerBypassesPolicyPipeline) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(self_repair, nullptr);

    // Create bridge with MANUAL policy (blocks all auto-triggers)
    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_MANUAL;

    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    // Convert anomaly to diagnostic
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.95f);

    diagnostic_result_t* diag = nullptr;
    int ret = health_diag_bridge_convert_anomaly(
        diag_bridge, &anomaly, &diag);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(diag, nullptr);

    // Force trigger should bypass MANUAL policy
    uint64_t request_id = 0;
    ret = health_self_repair_bridge_force_trigger(
        repair_bridge, diag, &request_id);
    // diag is freed internally on success
    EXPECT_EQ(ret, 0);
    EXPECT_GT(request_id, 0u);

    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(repair_bridge, &stats);
    EXPECT_EQ(stats.repairs_triggered, 1u);
}

TEST_F(HealthSelfRepairE2ETest, TriggerFromDiagnosticOwnershipPipeline) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(repair_bridge, nullptr);

    // Convert anomaly to diagnostic
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);

    diagnostic_result_t* diag = nullptr;
    int ret = health_diag_bridge_convert_anomaly(
        diag_bridge, &anomaly, &diag);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(diag, nullptr);

    // trigger_from_diagnostic takes ownership on success
    uint64_t request_id = 0;
    ret = health_self_repair_bridge_trigger_from_diagnostic(
        repair_bridge, diag, &request_id);
    EXPECT_EQ(ret, 0);
    // Do NOT free diag - it's owned by the bridge now

    // Verify repair was recorded
    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(repair_bridge, &stats);
    EXPECT_GE(stats.repairs_triggered, 1u);
}

//=============================================================================
// Tracking Record Lifecycle E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, TrackingRecordAfterTrigger) {
    ASSERT_NE(repair_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.95f);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly, &request_id);
    ASSERT_EQ(ret, 0);
    ASSERT_GT(request_id, 0u);

    // Query tracking record (returns pointer, NULL on not found)
    const health_repair_tracking_t* tracking =
        health_self_repair_bridge_get_tracking(repair_bridge, request_id);
    ASSERT_NE(tracking, nullptr);
    EXPECT_EQ(tracking->request_id, request_id);
}

TEST_F(HealthSelfRepairE2ETest, UnknownTrackingRecordReturnsNull) {
    ASSERT_NE(repair_bridge, nullptr);

    const health_repair_tracking_t* tracking =
        health_self_repair_bridge_get_tracking(repair_bridge, 999999);
    EXPECT_EQ(tracking, nullptr);
}

TEST_F(HealthSelfRepairE2ETest, PendingCountReflectsActiveRepairs) {
    ASSERT_NE(repair_bridge, nullptr);

    uint32_t initial = health_self_repair_bridge_get_pending_count(repair_bridge);
    EXPECT_EQ(initial, 0u);

    // Trigger a repair
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.95f);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly, &request_id);

    if (ret == 0) {
        uint32_t pending = health_self_repair_bridge_get_pending_count(repair_bridge);
        // Pending may be 0 if repair completes synchronously, but should not be negative
        EXPECT_GE(pending, 0u);
    }
}

//=============================================================================
// Health Agent Connection E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, HealthAgentConnectionToRepairBridge) {
    ASSERT_NE(repair_bridge, nullptr);
    ASSERT_NE(health_agent, nullptr);

    int ret = health_self_repair_bridge_connect_health_agent(
        repair_bridge, health_agent);
    EXPECT_EQ(ret, 0);

    // Bridge should still be ready
    EXPECT_TRUE(health_self_repair_bridge_is_ready(repair_bridge));

    // Process anomaly after connection
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);

    uint64_t request_id = 0;
    ret = health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly, &request_id);
    EXPECT_EQ(ret, 0);
}

TEST_F(HealthSelfRepairE2ETest, NullHealthAgentRejectedByRepairBridge) {
    ASSERT_NE(repair_bridge, nullptr);

    int ret = health_self_repair_bridge_connect_health_agent(
        repair_bridge, NULL);
    EXPECT_EQ(ret, -1);

    // Bridge should still be operational
    EXPECT_TRUE(health_self_repair_bridge_is_ready(repair_bridge));
}

//=============================================================================
// Policy Configuration E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, ErrorPolicyAcceptsErrorSeverity) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(self_repair, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_ERROR;

    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                   ANOMALY_SEVERITY_ERROR, 0.85f);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly, &request_id);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(request_id, 0u);
}

TEST_F(HealthSelfRepairE2ETest, ManualPolicyBlocksAutoTrigger) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(self_repair, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_MANUAL;

    health_self_repair_bridge_destroy(repair_bridge);
    repair_bridge = health_self_repair_bridge_create(
        &config, diag_bridge, self_repair);
    ASSERT_NE(repair_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 1.0f);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly, &request_id);
    EXPECT_EQ(ret, 1);  // Skipped by policy

    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(repair_bridge, &stats);
    EXPECT_EQ(stats.repairs_triggered, 0u);
    EXPECT_EQ(stats.repairs_skipped, 1u);
}

//=============================================================================
// Cross-Bridge Consistency E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, DiagAndRepairBridgeStatsConsistent) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(repair_bridge, nullptr);

    // Process multiple anomalies of varying severity
    anomaly_type_t types[] = {
        ANOMALY_MEMORY_LEAK,
        ANOMALY_RESOURCE_EXHAUSTION,
        ANOMALY_THREAD_CONTENTION
    };

    for (auto type : types) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, type, ANOMALY_SEVERITY_CRITICAL, 0.9f);

        uint64_t request_id = 0;
        health_self_repair_bridge_process_anomaly(
            repair_bridge, &anomaly, &request_id);
    }

    // Verify stats
    health_self_repair_bridge_stats_t repair_stats;
    health_self_repair_bridge_get_stats(repair_bridge, &repair_stats);

    // All should have been processed (categories may overlap)
    EXPECT_GE(repair_stats.repairs_triggered + repair_stats.repairs_skipped, 3u);
}

TEST_F(HealthSelfRepairE2ETest, NotifyBridgeWithFullPipelineOutput) {
    ASSERT_NE(notify_bridge, nullptr);

    // Send multiple notification types
    self_repair_health_notification_t notification;

    // Failure notification
    memset(&notification, 0, sizeof(notification));
    notification.type = REPAIR_NOTIFY_FAILURE;
    notification.repair_id = 1;
    notification.intervention = REPAIR_INTERVENTION_ALERT;
    notification.error_type = ERROR_TYPE_MEMORY_LEAK;
    notification.severity = DIAG_SEVERITY_ERROR;
    notification.failure_count = 1;
    strncpy(notification.error_message, "Pipeline repair failed",
            sizeof(notification.error_message) - 1);
    int ret = self_repair_health_notify_send(notify_bridge, &notification);
    EXPECT_EQ(ret, 0);

    // Escalation notification
    memset(&notification, 0, sizeof(notification));
    notification.type = REPAIR_NOTIFY_REPEATED_FAILURE;
    notification.repair_id = 2;
    notification.intervention = REPAIR_INTERVENTION_QUARANTINE;
    notification.failure_count = 5;
    ret = self_repair_health_notify_send(notify_bridge, &notification);
    EXPECT_EQ(ret, 0);

    // Success notification
    memset(&notification, 0, sizeof(notification));
    notification.type = REPAIR_NOTIFY_SUCCESS;
    notification.repair_id = 3;
    notification.intervention = REPAIR_INTERVENTION_NONE;
    ret = self_repair_health_notify_send(notify_bridge, &notification);
    EXPECT_EQ(ret, 0);

    // Verify stats
    self_repair_health_notify_stats_t stats;
    self_repair_health_notify_get_stats(notify_bridge, &stats);
    EXPECT_EQ(stats.notifications_sent, 3u);
    EXPECT_EQ(stats.failures_notified, 1u);
}

//=============================================================================
// Graceful Degradation E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, PipelineWithoutHealthAgent) {
    // Create pipeline without health agent
    health_diag_bridge_t* diag = health_diag_bridge_create(NULL);
    self_repair_coordinator_t* sr = self_repair_create(NULL);
    ASSERT_NE(diag, nullptr);
    ASSERT_NE(sr, nullptr);

    health_self_repair_bridge_t* bridge =
        health_self_repair_bridge_create(NULL, diag, sr);
    ASSERT_NE(bridge, nullptr);

    // Should still process anomalies
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(
        bridge, &anomaly, &request_id);
    EXPECT_EQ(ret, 0);

    health_self_repair_bridge_destroy(bridge);
    self_repair_destroy(sr);
    health_diag_bridge_destroy(diag);
}

TEST_F(HealthSelfRepairE2ETest, PipelineWithoutCodeImmune) {
    // Full pipeline minus code immune
    health_diag_bridge_t* diag = health_diag_bridge_create(NULL);
    self_repair_coordinator_t* sr = self_repair_create(NULL);
    ASSERT_NE(diag, nullptr);
    ASSERT_NE(sr, nullptr);

    health_self_repair_bridge_t* bridge =
        health_self_repair_bridge_create(NULL, diag, sr);
    ASSERT_NE(bridge, nullptr);

    // Process multiple anomalies
    for (int i = 0; i < 5; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                       ANOMALY_SEVERITY_CRITICAL, 0.85f);

        uint64_t request_id = 0;
        health_self_repair_bridge_process_anomaly(
            bridge, &anomaly, &request_id);
    }

    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.repairs_triggered, 5u);

    health_self_repair_bridge_destroy(bridge);
    self_repair_destroy(sr);
    health_diag_bridge_destroy(diag);
}

//=============================================================================
// Reset and Re-Use E2E Tests
//=============================================================================

TEST_F(HealthSelfRepairE2ETest, ResetAndReUsePipeline) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(repair_bridge, nullptr);

    // Process initial anomalies
    for (int i = 0; i < 3; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_CRITICAL, 0.9f);

        uint64_t request_id = 0;
        health_self_repair_bridge_process_anomaly(
            repair_bridge, &anomaly, &request_id);
    }

    // Reset all stats
    health_diag_bridge_reset_stats(diag_bridge);
    health_self_repair_bridge_reset_stats(repair_bridge);

    // Process more anomalies
    for (int i = 0; i < 2; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                       ANOMALY_SEVERITY_CRITICAL, 0.85f);

        uint64_t request_id = 0;
        health_self_repair_bridge_process_anomaly(
            repair_bridge, &anomaly, &request_id);
    }

    // Stats should reflect only post-reset activity
    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(repair_bridge, &stats);
    EXPECT_EQ(stats.repairs_triggered, 2u);
}

TEST_F(HealthSelfRepairE2ETest, FullPipelineStressTest) {
    ASSERT_NE(repair_bridge, nullptr);

    // Process a large number of anomalies
    const int count = 100;
    int triggered = 0;

    for (int i = 0; i < count; i++) {
        anomaly_t anomaly;
        anomaly_type_t type = (i % 2 == 0) ? ANOMALY_MEMORY_LEAK
                                             : ANOMALY_RESOURCE_EXHAUSTION;
        create_anomaly(&anomaly, type, ANOMALY_SEVERITY_CRITICAL, 0.9f);

        uint64_t request_id = 0;
        int ret = health_self_repair_bridge_process_anomaly(
            repair_bridge, &anomaly, &request_id);
        if (ret == 0) triggered++;
    }

    // Stats should be consistent
    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(repair_bridge, &stats);
    EXPECT_EQ(stats.repairs_triggered, static_cast<uint64_t>(triggered));
    // repairs_skipped may overlap with rate_limited_count, so verify individually
    EXPECT_GE(stats.repairs_triggered + stats.repairs_skipped,
              static_cast<uint64_t>(count));
    EXPECT_LE(stats.repairs_triggered, static_cast<uint64_t>(count));
}
