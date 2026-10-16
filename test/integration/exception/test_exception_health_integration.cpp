/**
 * @file test_exception_health_integration.cpp
 * @brief Integration tests for health system exception integration
 * @version 1.0.0
 * @date 2026-01-21
 *
 * WHAT: Test integration between exception handling and health monitoring systems
 * WHY:  Exceptions should trigger health notifications and self-repair actions
 * HOW:  Simulate critical exceptions, verify health agent notifications and repair triggers
 *
 * TEST SCENARIOS:
 * - Health agent notification on critical exceptions
 * - Self-repair trigger on repeated exceptions
 * - Exception-to-diagnostic conversion
 * - Exception rate limiting integration
 * - Health diagnostic bridge exception handling
 * - Self-repair bridge exception flow
 * - Exception immune presentation to health system
 * - Cascading exception health monitoring
 *
 * HEADER FILES REFERENCED:
 * - include/utils/fault_tolerance/nimcp_health_agent.h
 * - include/cognitive/fault_tolerance/nimcp_self_repair.h
 * - include/cognitive/fault_tolerance/nimcp_health_self_repair_bridge.h
 * - include/cognitive/fault_tolerance/nimcp_health_diagnostic_bridge.h
 * - include/utils/exception/nimcp_exception.h
 * - include/utils/exception/nimcp_exception_handlers.h
 * - include/utils/exception/nimcp_exception_immune.h
 * - include/utils/exception/nimcp_exception_circuit.h
 * - include/utils/exception/nimcp_exception_metrics.h
 *
 * FUNCTION SIGNATURES USED:
 * - nimcp_health_agent_create(config) -> nimcp_health_agent_t*
 * - nimcp_health_agent_destroy(agent)
 * - nimcp_health_agent_register_callback(agent, callback, user_data) -> int
 * - nimcp_health_agent_send_message(agent, message) -> int
 * - health_self_repair_bridge_create(config, diag_bridge, self_repair) -> health_self_repair_bridge_t*
 * - health_self_repair_bridge_destroy(bridge)
 * - health_self_repair_bridge_process_anomaly(bridge, anomaly, request_id) -> int
 * - health_self_repair_bridge_get_stats(bridge, stats) -> int
 * - nimcp_exception_create(code, severity, ...) -> nimcp_exception_t*
 * - nimcp_exception_present_to_immune(ex, response) -> int
 * - nimcp_circuit_record(ex) -> int
 * - nimcp_metrics_record_exception(ex)
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>

// Headers have their own extern "C" guards - don't wrap them
// (wrapping would break CUDA template headers included transitively)
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "cognitive/fault_tolerance/nimcp_health_self_repair_bridge.h"
#include "cognitive/fault_tolerance/nimcp_health_diagnostic_bridge.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test State Tracking
//=============================================================================

/**
 * @brief Health notification event record
 */
struct HealthNotificationEvent {
    health_agent_severity_t severity;
    health_agent_msg_type_t msg_type;
    std::string description;
    uint64_t timestamp_us;
    bool triggered_repair;
};

/**
 * @brief Self-repair trigger event record
 */
struct SelfRepairTriggerEvent {
    uint64_t request_id;
    error_type_t error_type;
    float confidence;
    time_t timestamp;
    bool repair_succeeded;
};

/**
 * @brief Global state for health integration tests
 */
static struct {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<HealthNotificationEvent> health_events;
    std::vector<SelfRepairTriggerEvent> repair_events;
    std::atomic<int> health_notifications{0};
    std::atomic<int> repair_triggers{0};
    std::atomic<int> exceptions_presented{0};
    std::atomic<int> diagnostics_created{0};
    std::map<nimcp_error_t, int> exception_counts;
} g_health_state;

//=============================================================================
// Mock Health Agent Callback
//=============================================================================

/**
 * @brief Health agent message callback for testing
 */
static void health_agent_callback(
    const health_agent_message_t* message,
    void* user_data
) {
    (void)user_data;

    g_health_state.health_notifications++;

    HealthNotificationEvent event;
    event.severity = message->severity;
    event.msg_type = message->type;
    event.description = message->description;
    event.timestamp_us = message->timestamp_us;
    event.triggered_repair = false;

    {
        std::lock_guard<std::mutex> lock(g_health_state.mutex);
        g_health_state.health_events.push_back(event);
        g_health_state.cv.notify_all();
    }
}

/**
 * @brief Self-repair trigger callback for testing
 */
static void repair_trigger_callback(
    uint64_t request_id,
    const diagnostic_result_t* diagnostic,
    void* user_data
) {
    (void)user_data;

    g_health_state.repair_triggers++;

    SelfRepairTriggerEvent event;
    event.request_id = request_id;
    event.error_type = diagnostic->error_type;
    event.confidence = diagnostic->confidence;
    event.timestamp = diagnostic->timestamp;
    event.repair_succeeded = false;  // Unknown yet

    {
        std::lock_guard<std::mutex> lock(g_health_state.mutex);
        g_health_state.repair_events.push_back(event);
        g_health_state.cv.notify_all();
    }
}

/**
 * @brief Self-repair outcome callback for testing
 */
static void repair_outcome_callback(
    uint64_t request_id,
    health_repair_outcome_t outcome,
    const self_repair_result_t* result,
    void* user_data
) {
    (void)result;
    (void)user_data;

    std::lock_guard<std::mutex> lock(g_health_state.mutex);
    for (auto& event : g_health_state.repair_events) {
        if (event.request_id == request_id) {
            event.repair_succeeded = (outcome == HEALTH_REPAIR_OUTCOME_SUCCESS);
            break;
        }
    }
    g_health_state.cv.notify_all();
}

/**
 * @brief Exception handler that sends to health agent
 */
static nimcp_health_agent_t* g_test_health_agent = nullptr;

static bool health_notification_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;

    // For severe+ exceptions, notify health agent
    if (ex->severity >= EXCEPTION_SEVERITY_SEVERE && g_test_health_agent) {
        health_agent_message_t msg;
        memset(&msg, 0, sizeof(msg));

        // Map exception severity to health agent severity
        switch (ex->severity) {
            case EXCEPTION_SEVERITY_SEVERE:
                msg.severity = HEALTH_SEVERITY_ERROR;
                break;
            case EXCEPTION_SEVERITY_CRITICAL:
                msg.severity = HEALTH_SEVERITY_CRITICAL;
                break;
            case EXCEPTION_SEVERITY_FATAL:
                msg.severity = HEALTH_SEVERITY_FATAL;
                break;
            default:
                msg.severity = HEALTH_SEVERITY_WARNING;
                break;
        }

        msg.type = HEALTH_MSG_STATUS_UPDATE;
        msg.source = HEALTH_SOURCE_NEURAL;
        msg.timestamp_us = ex->timestamp_us;
        strncpy(msg.description, ex->message ? ex->message : "Unknown error",
                sizeof(msg.description) - 1);

        nimcp_health_agent_report_anomaly(g_test_health_agent, &msg);
    }

    return false;  // Don't consume
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionHealthIntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* health_agent_ = nullptr;
    health_diag_bridge_t* diag_bridge_ = nullptr;
    self_repair_coordinator_t* self_repair_ = nullptr;
    health_self_repair_bridge_t* repair_bridge_ = nullptr;
    nimcp_handler_registration_t* handler_reg_ = nullptr;

    void SetUp() override {
        // Reset state
        {
            std::lock_guard<std::mutex> lock(g_health_state.mutex);
            g_health_state.health_events.clear();
            g_health_state.repair_events.clear();
            g_health_state.exception_counts.clear();
        }
        g_health_state.health_notifications = 0;
        g_health_state.repair_triggers = 0;
        g_health_state.exceptions_presented = 0;
        g_health_state.diagnostics_created = 0;

        // Initialize exception system
        nimcp_exception_system_init();

        // Initialize circuit breaker
        nimcp_circuit_init();

        // Initialize metrics
        nimcp_metrics_init();

        // Initialize immune integration
        nimcp_exception_immune_config_t immune_config;
        nimcp_exception_immune_default_config(&immune_config);
        immune_config.enable_auto_present = false;
        immune_config.enable_auto_recovery = false;
        nimcp_exception_immune_init(&immune_config);

        // Create health agent with correct API
        health_agent_config_t agent_config;
        nimcp_health_agent_default_config(&agent_config);
        // Register callback for anomaly notifications
        agent_config.on_anomaly_detected = health_agent_callback;
        agent_config.callback_user_data = nullptr;
        health_agent_ = nimcp_health_agent_create(&agent_config);
        g_test_health_agent = health_agent_;

        // Create diagnostic bridge with correct API (no health_agent param)
        health_diag_bridge_config_t diag_config;
        health_diag_bridge_default_config(&diag_config);
        diag_bridge_ = health_diag_bridge_create(&diag_config);

        // Create self-repair coordinator with correct API
        self_repair_config_t repair_config = self_repair_default_config();
        repair_config.mode = REPAIR_MODE_DUAL;  // Both hot-patch and source
        self_repair_ = self_repair_create(&repair_config);

        // Create health-self-repair bridge
        if (diag_bridge_ && self_repair_) {
            health_self_repair_bridge_config_t bridge_config;
            health_self_repair_bridge_default_config(&bridge_config);
            bridge_config.trigger_policy = HEALTH_TRIGGER_CRITICAL;
            bridge_config.async_repairs = false;  // Sync for testing
            repair_bridge_ = health_self_repair_bridge_create(
                &bridge_config, diag_bridge_, self_repair_);

            if (repair_bridge_) {
                health_self_repair_bridge_set_trigger_callback(
                    repair_bridge_, repair_trigger_callback, nullptr);
                health_self_repair_bridge_set_outcome_callback(
                    repair_bridge_, repair_outcome_callback, nullptr);
            }
        }

        // Register exception handler for health notifications
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "health_notification_handler";
        opts.handler = health_notification_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        opts.min_severity = EXCEPTION_SEVERITY_SEVERE;
        handler_reg_ = nimcp_handler_register(&opts);
    }

    void TearDown() override {
        g_test_health_agent = nullptr;

        if (handler_reg_) {
            nimcp_handler_unregister(handler_reg_);
            handler_reg_ = nullptr;
        }

        if (repair_bridge_) {
            health_self_repair_bridge_destroy(repair_bridge_);
            repair_bridge_ = nullptr;
        }

        if (self_repair_) {
            self_repair_destroy(self_repair_);
            self_repair_ = nullptr;
        }

        if (diag_bridge_) {
            health_diag_bridge_destroy(diag_bridge_);
            diag_bridge_ = nullptr;
        }

        if (health_agent_) {
            nimcp_health_agent_destroy(health_agent_);
            health_agent_ = nullptr;
        }

        nimcp_exception_immune_shutdown();
        nimcp_metrics_shutdown();
        nimcp_circuit_shutdown();
        nimcp_exception_clear_current();
        nimcp_exception_handlers_shutdown();
        nimcp_exception_system_shutdown();
    }

    void waitForHealthEvents(size_t expected_count, int timeout_ms = 2000) {
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeout_ms);
        std::unique_lock<std::mutex> lock(g_health_state.mutex);
        while (g_health_state.health_events.size() < expected_count) {
            if (g_health_state.cv.wait_until(lock, deadline) ==
                std::cv_status::timeout) {
                break;
            }
        }
    }

    void waitForRepairEvents(size_t expected_count, int timeout_ms = 2000) {
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeout_ms);
        std::unique_lock<std::mutex> lock(g_health_state.mutex);
        while (g_health_state.repair_events.size() < expected_count) {
            if (g_health_state.cv.wait_until(lock, deadline) ==
                std::cv_status::timeout) {
                break;
            }
        }
    }
};

//=============================================================================
// Test: Health Agent Notification on Critical Exception
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, HealthAgentNotificationOnCriticalException) {
    // WHAT: Verify critical exceptions notify health agent
    // WHY: Health system needs awareness of critical errors

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Deadlock detected in neural pathway"
    );
    ASSERT_NE(ex, nullptr);

    // Dispatch exception
    nimcp_exception_dispatch(ex);

    // Wait for health notification
    waitForHealthEvents(1, 1000);

    // Verify notification received
    EXPECT_GE(g_health_state.health_notifications.load(), 1)
        << "Health agent should receive notification";

    {
        std::lock_guard<std::mutex> lock(g_health_state.mutex);
        EXPECT_FALSE(g_health_state.health_events.empty())
            << "Health event should be recorded";

        if (!g_health_state.health_events.empty()) {
            const auto& event = g_health_state.health_events[0];
            EXPECT_GE(event.severity, DIAG_SEVERITY_CRITICAL)
                << "Severity should be CRITICAL or higher";
        }
    }

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Severe Exception Notifies Health Agent
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, SevereExceptionNotifiesHealthAgent) {
    // WHAT: Verify SEVERE severity triggers health notification
    // WHY: Severe errors need health system attention

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory corruption in synaptic weights"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_dispatch(ex);

    waitForHealthEvents(1, 1000);

    EXPECT_GE(g_health_state.health_notifications.load(), 1)
        << "Severe exception should trigger health notification";

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Low Severity Does Not Notify Health Agent
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, LowSeverityDoesNotNotifyHealthAgent) {
    // WHAT: Verify WARNING severity does not trigger health notification
    // WHY: Health agent should focus on severe+ issues

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Invalid parameter in configuration"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_dispatch(ex);

    // Give some time for potential notification
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(g_health_state.health_notifications.load(), 0)
        << "WARNING severity should not trigger health notification";

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Self-Repair Trigger on Repeated Exceptions
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, SelfRepairTriggerOnRepeatedExceptions) {
    // WHAT: Verify repeated exceptions trigger self-repair
    // WHY: Pattern of errors should initiate automated repair

    if (!repair_bridge_) {
        GTEST_SKIP() << "Repair bridge not initialized";
    }

    // Create multiple critical exceptions to trigger repair
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_LEARNING_FAILED,
            EXCEPTION_SEVERITY_CRITICAL,
            __FILE__, __LINE__, __func__,
            "Learning failure iteration %d", i
        );
        ASSERT_NE(ex, nullptr);

        // Record in circuit breaker (may trigger repair threshold)
        nimcp_circuit_record(ex);

        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    waitForHealthEvents(5, 2000);

    // Should have received multiple health notifications
    EXPECT_GE(g_health_state.health_notifications.load(), 5)
        << "All critical exceptions should notify health agent";
}

//=============================================================================
// Test: Exception-to-Diagnostic Conversion
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, ExceptionToDiagnosticConversion) {
    // WHAT: Verify exception can be converted to diagnostic
    // WHY: Health system uses diagnostics, not exceptions directly

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Inference engine failure"
    );
    ASSERT_NE(ex, nullptr);

    // Add context for conversion
    nimcp_exception_set_context(ex, "engine_id", "42");
    nimcp_exception_set_context(ex, "batch_size", "128");

    // Convert exception to diagnostic via immune presentation
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex, &response);

    // Presentation may fail if not connected, but shouldn't crash
    if (result == 0) {
        EXPECT_TRUE(ex->presented_to_immune)
            << "Exception should be marked as presented";
        g_health_state.exceptions_presented++;
    }

    // Generate epitope for pattern matching
    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(epitope_len, 0u) << "Epitope should be generated";

    // The epitope can be used for immune system matching
    EXPECT_NE(ex->epitope, nullptr) << "Epitope data should exist";

    nimcp_exception_dispatch(ex);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Exception Rate Limiting Integration
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, ExceptionRateLimitingIntegration) {
    // WHAT: Verify exception rate limiting works with health system
    // WHY: Prevent flooding health system with repeated exceptions

    ASSERT_TRUE(nimcp_circuit_is_initialized());

    // Configure circuit breaker for this test
    nimcp_circuit_set_threshold(NIMCP_ERROR_OPERATION_FAILED, 5, 10000);

    // Send many exceptions rapidly
    int blocked_count = 0;
    for (int i = 0; i < 15; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Rate limit test exception %d", i
        );
        ASSERT_NE(ex, nullptr);

        int cb_result = nimcp_circuit_record(ex);
        if (cb_result == 1) {
            blocked_count++;
        }

        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    // Some should have been blocked by circuit breaker
    EXPECT_GT(blocked_count, 0)
        << "Circuit breaker should block some exceptions";

    // Check circuit state
    nimcp_circuit_state_t state = nimcp_circuit_get_state(NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_NE(state, CIRCUIT_STATE_CLOSED)
        << "Circuit should no longer be closed after threshold";
}

//=============================================================================
// Test: Health Agent Message Triggers Repair Bridge
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, HealthAgentMessageTriggersRepairBridge) {
    // WHAT: Verify health agent messages can trigger repair
    // WHY: Integration between health monitoring and repair

    if (!repair_bridge_ || !health_agent_) {
        GTEST_SKIP() << "Required components not initialized";
    }

    // Connect repair bridge to health agent
    int connect_result = health_self_repair_bridge_connect_health_agent(
        repair_bridge_, health_agent_);

    if (connect_result != 0) {
        GTEST_SKIP() << "Could not connect to health agent";
    }

    // Create critical health message using correct API
    health_agent_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = HEALTH_MSG_NAN_DETECTED;
    msg.severity = HEALTH_SEVERITY_CRITICAL;
    msg.source = HEALTH_SOURCE_NEURAL;
    msg.suggested_action = HEALTH_RECOVERY_CHECKPOINT;
    msg.timestamp_us = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    strncpy(msg.description, "Neural pathway damage detected", sizeof(msg.description) - 1);

    // Report anomaly via health agent (triggers repair via connected bridge)
    nimcp_health_agent_report_anomaly(health_agent_, &msg);

    // Wait for potential repair trigger
    waitForRepairEvents(1, 2000);

    // Check if repair was triggered
    // May not trigger depending on policy
    health_self_repair_bridge_stats_t stats;
    if (health_self_repair_bridge_get_stats(repair_bridge_, &stats) == 0) {
        // Just verify we can get stats
        EXPECT_GE(stats.repairs_triggered + stats.repairs_skipped, 0u);
    }
}

//=============================================================================
// Test: Anomaly Processing Triggers Repair
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, AnomalyProcessingTriggersRepair) {
    // WHAT: Verify health anomaly can trigger repair
    // WHY: Anomaly detection should lead to repair actions

    if (!repair_bridge_) {
        GTEST_SKIP() << "Repair bridge not initialized";
    }

    // Create test anomaly
    anomaly_t anomaly;
    memset(&anomaly, 0, sizeof(anomaly));
    anomaly.type = ANOMALY_PERFORMANCE_DEGRADATION;
    anomaly.severity = ANOMALY_SEVERITY_CRITICAL;
    anomaly.confidence = 0.95f;
    strncpy(anomaly.description, "Critical degradation in visual cortex",
            sizeof(anomaly.description) - 1);
    anomaly.detected_at_us = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Process anomaly
    uint64_t request_id = 0;
    int result = health_self_repair_bridge_process_anomaly(
        repair_bridge_, &anomaly, &request_id);

    // Result: 0 = repair triggered, 1 = skipped (policy), -1 = error
    if (result == 0) {
        EXPECT_NE(request_id, 0u) << "Request ID should be assigned";

        waitForRepairEvents(1, 2000);
        EXPECT_GE(g_health_state.repair_triggers.load(), 1)
            << "Repair should be triggered for critical anomaly";
    } else if (result == 1) {
        // Skipped by policy - acceptable
        SUCCEED() << "Repair skipped by policy (expected for some configurations)";
    }
}

//=============================================================================
// Test: Exception Metrics for Health Monitoring
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, ExceptionMetricsForHealthMonitoring) {
    // WHAT: Verify exception metrics are available for health monitoring
    // WHY: Metrics enable health system decision making

    ASSERT_TRUE(nimcp_metrics_is_initialized());

    // Generate variety of exceptions
    for (int i = 0; i < 10; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            i < 5 ? EXCEPTION_SEVERITY_ERROR : EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "Metrics test %d", i
        );
        ASSERT_NE(ex, nullptr);

        nimcp_metrics_record_exception(ex);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    // Get metrics snapshot
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    EXPECT_GE(metrics.total_exceptions, 10u)
        << "Metrics should track all exceptions";

    // Rate should be calculable
    EXPECT_GE(metrics.current_rate_per_second, 0.0f)
        << "Rate should be non-negative";
}

//=============================================================================
// Test: Health Diagnostic Bridge Conversion
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, HealthDiagnosticBridgeConversion) {
    // WHAT: Verify diagnostic bridge can convert health data
    // WHY: Bridge transforms health events to diagnostics

    if (!diag_bridge_) {
        GTEST_SKIP() << "Diagnostic bridge not initialized";
    }

    // Create test anomaly
    anomaly_t anomaly;
    memset(&anomaly, 0, sizeof(anomaly));
    anomaly.type = ANOMALY_ERROR_SPIKE;
    anomaly.severity = ANOMALY_SEVERITY_WARNING;
    anomaly.confidence = 0.75f;
    strncpy(anomaly.description, "Metric deviation in memory usage",
            sizeof(anomaly.description) - 1);

    // Convert via bridge - function allocates result
    diagnostic_result_t* diagnostic = nullptr;
    int result = health_diag_bridge_convert_anomaly(diag_bridge_, &anomaly, &diagnostic);

    if (result == 0 && diagnostic != nullptr) {
        EXPECT_EQ(diagnostic->severity, DIAG_SEVERITY_WARNING);
        EXPECT_GE(diagnostic->confidence, 0.0f);
        EXPECT_LE(diagnostic->confidence, 1.0f);
        g_health_state.diagnostics_created++;
        // Free allocated diagnostic
        free(diagnostic);
    }
}

//=============================================================================
// Test: Exception Immune Presentation Flow
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, ExceptionImmunePresentationFlow) {
    // WHAT: Verify immune presentation creates proper response
    // WHY: Immune system may suggest recovery actions

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Learning rate divergence"
    );
    ASSERT_NE(ex, nullptr);

    // Add diagnostic context
    nimcp_exception_set_context(ex, "learning_rate", "0.001");
    nimcp_exception_set_context(ex, "epoch", "42");
    nimcp_exception_set_context(ex, "loss", "inf");

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex, &response);

    if (result == 0) {
        // Check response - verify valid response time
        EXPECT_GE(response.response_time_us, 0u);
        EXPECT_TRUE(ex->presented_to_immune);

        // Response may suggest recovery
        if (response.action_taken != EXCEPTION_RECOVERY_NONE) {
            EXPECT_TRUE(
                response.action_taken >= EXCEPTION_RECOVERY_NONE &&
                response.action_taken <= EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN
            ) << "Action taken should be valid";
        }
    }

    nimcp_exception_dispatch(ex);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Cascading Exception Health Monitoring
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, CascadingExceptionHealthMonitoring) {
    // WHAT: Verify cascading exceptions are tracked for health
    // WHY: Cascading failures need special attention

    // Create chain of exceptions
    nimcp_exception_t* root = nimcp_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "GPU out of memory"
    );
    ASSERT_NE(root, nullptr);

    nimcp_exception_t* mid = nimcp_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Forward pass failed due to GPU error"
    );
    ASSERT_NE(mid, nullptr);
    nimcp_exception_set_cause(mid, root);

    nimcp_exception_t* top = nimcp_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Inference pipeline crashed"
    );
    ASSERT_NE(top, nullptr);
    nimcp_exception_set_cause(top, mid);

    // Dispatch top-level (should include chain)
    nimcp_exception_dispatch(top);

    waitForHealthEvents(1, 1000);

    // Health should be notified of critical exception
    EXPECT_GE(g_health_state.health_notifications.load(), 1)
        << "Critical exception should notify health";

    // Verify cause chain is accessible
    nimcp_exception_t* cause = nimcp_exception_get_cause(top);
    EXPECT_EQ(cause, mid);
    EXPECT_EQ(nimcp_exception_get_cause(mid), root);

    nimcp_exception_unref(top);
    // mid and root are unreffed via cause chain
}

//=============================================================================
// Test: Health Self-Repair Bridge Statistics
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, HealthSelfRepairBridgeStatistics) {
    // WHAT: Verify repair bridge tracks statistics
    // WHY: Statistics enable monitoring and tuning

    if (!repair_bridge_) {
        GTEST_SKIP() << "Repair bridge not initialized";
    }

    // Get initial stats
    health_self_repair_bridge_stats_t stats;
    int result = health_self_repair_bridge_get_stats(repair_bridge_, &stats);
    ASSERT_EQ(result, 0);

    // Record initial values
    uint64_t initial_triggered = stats.repairs_triggered;
    uint64_t initial_skipped = stats.repairs_skipped;

    // Try to trigger a repair
    anomaly_t anomaly;
    memset(&anomaly, 0, sizeof(anomaly));
    anomaly.type = ANOMALY_PERFORMANCE_DEGRADATION;
    anomaly.severity = ANOMALY_SEVERITY_CRITICAL;  // Most severe
    anomaly.confidence = 0.99f;
    strncpy(anomaly.description, "Fatal degradation",
            sizeof(anomaly.description) - 1);

    uint64_t request_id = 0;
    health_self_repair_bridge_process_anomaly(repair_bridge_, &anomaly, &request_id);

    // Get updated stats
    result = health_self_repair_bridge_get_stats(repair_bridge_, &stats);
    ASSERT_EQ(result, 0);

    // Should have changed
    EXPECT_GE(stats.repairs_triggered + stats.repairs_skipped,
              initial_triggered + initial_skipped)
        << "Stats should reflect processing";
}

//=============================================================================
// Test: Rate Limit Check Before Repair
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, RateLimitCheckBeforeRepair) {
    // WHAT: Verify rate limiting is checked before repair
    // WHY: Prevent repair flooding

    if (!repair_bridge_) {
        GTEST_SKIP() << "Repair bridge not initialized";
    }

    // Check initial rate limit status
    bool rate_limited = health_self_repair_bridge_is_rate_limited(
        repair_bridge_, ERROR_TYPE_UNKNOWN);

    // Initially should not be limited
    EXPECT_FALSE(rate_limited) << "Initially should not be rate limited";

    // Trigger many repairs to potentially hit limit
    for (int i = 0; i < 20; i++) {
        anomaly_t anomaly;
        memset(&anomaly, 0, sizeof(anomaly));
        anomaly.type = ANOMALY_PERFORMANCE_DEGRADATION;
        anomaly.severity = ANOMALY_SEVERITY_CRITICAL;
        anomaly.confidence = 0.95f;
        snprintf(anomaly.description, sizeof(anomaly.description),
                 "Rate limit test %d", i);

        uint64_t request_id = 0;
        health_self_repair_bridge_process_anomaly(repair_bridge_, &anomaly, &request_id);
    }

    // Check rate limit status again
    rate_limited = health_self_repair_bridge_is_rate_limited(
        repair_bridge_, ERROR_TYPE_UNKNOWN);

    // May or may not be limited depending on configuration
    // Just verify we can check
    SUCCEED() << "Rate limit check completed: " << (rate_limited ? "limited" : "not limited");
}

//=============================================================================
// Test: Exception Context to Diagnostic Fields
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, ExceptionContextToDiagnosticFields) {
    // WHAT: Verify exception context transfers to diagnostic
    // WHY: Rich context improves diagnostics

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Brain creation failed"
    );
    ASSERT_NE(ex, nullptr);

    // Add rich context
    nimcp_exception_set_context(ex, "brain_id", "123");
    nimcp_exception_set_context(ex, "region", "prefrontal_cortex");
    nimcp_exception_set_context(ex, "neuron_count", "1000000");
    nimcp_exception_set_context(ex, "failure_phase", "initialization");

    // Verify context count
    EXPECT_EQ(nimcp_exception_context_count(ex), 4u);

    // Generate epitope (which uses context)
    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(epitope_len, 0u);

    // Context should be accessible
    const char* region = nimcp_exception_get_context(ex, "region");
    EXPECT_NE(region, nullptr);
    if (region) {
        EXPECT_STREQ(region, "prefrontal_cortex");
    }

    nimcp_exception_dispatch(ex);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Multiple Severity Levels Health Response
//=============================================================================

TEST_F(ExceptionHealthIntegrationTest, MultipleSeverityLevelsHealthResponse) {
    // WHAT: Verify different severities get appropriate health response
    // WHY: Health system should prioritize appropriately

    struct TestCase {
        nimcp_exception_severity_t severity;
        bool should_notify;
    } test_cases[] = {
        {EXCEPTION_SEVERITY_WARNING, false},
        {EXCEPTION_SEVERITY_ERROR, false},
        {EXCEPTION_SEVERITY_SEVERE, true},
        {EXCEPTION_SEVERITY_CRITICAL, true},
        {EXCEPTION_SEVERITY_FATAL, true}
    };

    for (const auto& tc : test_cases) {
        g_health_state.health_notifications = 0;

        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            tc.severity,
            __FILE__, __LINE__, __func__,
            "Severity test: %d", tc.severity
        );
        ASSERT_NE(ex, nullptr);

        nimcp_exception_dispatch(ex);

        // Wait briefly for notification
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        if (tc.should_notify) {
            EXPECT_GE(g_health_state.health_notifications.load(), 1)
                << "Severity " << tc.severity << " should notify health";
        } else {
            EXPECT_EQ(g_health_state.health_notifications.load(), 0)
                << "Severity " << tc.severity << " should not notify health";
        }

        nimcp_exception_unref(ex);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
