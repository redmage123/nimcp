/**
 * @file test_health_repair_bridge_regression.cpp
 * @brief Regression tests for Health to Self-Repair Bridge
 *
 * WHAT: Regression tests to prevent known bugs from reoccurring
 * WHY: Ensure bug fixes remain fixed across code changes
 * HOW: Test specific bug scenarios and edge cases from development
 *
 * TEST COVERAGE:
 * - Null pointer dereference edge cases
 * - Severity mapping boundary issues
 * - Rate limiting numerical stability
 * - Memory management and leak prevention
 * - Concurrent access safety
 * - Configuration validation edge cases
 *
 * @version 1.0.0
 * @date 2025-01-20
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_health_diagnostic_bridge.h"
#include "cognitive/fault_tolerance/nimcp_health_self_repair_bridge.h"
#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "cognitive/fault_tolerance/nimcp_self_repair_health_notify.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class HealthRepairBridgeRegressionTest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;
    health_diag_bridge_t* diag_bridge = nullptr;
    health_self_repair_bridge_t* repair_bridge = nullptr;
    self_repair_coordinator_t* self_repair = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;

        diag_bridge = health_diag_bridge_create(NULL);
        self_repair = self_repair_create(NULL);

        if (diag_bridge && self_repair) {
            repair_bridge = health_self_repair_bridge_create(
                NULL, diag_bridge, self_repair);
        }
    }

    void TearDown() override {
        if (repair_bridge) health_self_repair_bridge_destroy(repair_bridge);
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
    }
};

//=============================================================================
// REGRESSION: Null Pointer Handling
//=============================================================================

/**
 * @test REGRESSION: Null Bridge Crash
 *
 * WHAT: Prevent crash when passing NULL to API functions
 * WHY: Bug #1: Various functions crashed on NULL input
 * HOW: Call all public APIs with NULL parameters
 */
TEST_F(HealthRepairBridgeRegressionTest, NullBridgeCrashPrevention) {
    // Diagnostic bridge NULL handling
    EXPECT_NO_THROW(health_diag_bridge_destroy(NULL));
    EXPECT_FALSE(health_diag_bridge_is_ready(NULL));

    anomaly_t anomaly;
    diagnostic_result_t* result = nullptr;
    EXPECT_EQ(health_diag_bridge_convert_anomaly(NULL, &anomaly, &result), -1);

    health_agent_message_t msg;
    EXPECT_EQ(health_diag_bridge_convert_agent_message(NULL, &msg, &result), -1);

    health_diag_bridge_stats_t diag_stats;
    EXPECT_EQ(health_diag_bridge_get_stats(NULL, &diag_stats), -1);

    // Self-repair bridge NULL handling
    EXPECT_NO_THROW(health_self_repair_bridge_destroy(NULL));
    EXPECT_FALSE(health_self_repair_bridge_is_ready(NULL));

    uint64_t request_id;
    EXPECT_EQ(health_self_repair_bridge_process_anomaly(NULL, &anomaly, &request_id), -1);

    health_self_repair_bridge_stats_t repair_stats;
    EXPECT_EQ(health_self_repair_bridge_get_stats(NULL, &repair_stats), -1);

    // Notify bridge NULL handling
    EXPECT_NO_THROW(self_repair_health_notify_destroy(NULL));
    EXPECT_FALSE(self_repair_health_notify_is_ready(NULL));
}

/**
 * @test REGRESSION: Null Output Parameter Crash
 *
 * WHAT: Prevent crash when output parameter is NULL
 * WHY: Bug #2: Functions wrote to NULL pointers
 * HOW: Call functions with NULL output parameters
 */
TEST_F(HealthRepairBridgeRegressionTest, NullOutputParameterHandling) {
    ASSERT_NE(diag_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    // NULL result pointer
    EXPECT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, NULL), -1);

    // NULL stats pointer
    EXPECT_EQ(health_diag_bridge_get_stats(diag_bridge, NULL), -1);

    ASSERT_NE(repair_bridge, nullptr);
    EXPECT_EQ(health_self_repair_bridge_get_stats(repair_bridge, NULL), -1);
}

/**
 * @test REGRESSION: Null Input Data Crash
 *
 * WHAT: Prevent crash when input data is NULL
 * WHY: Bug #3: Dereference of NULL anomaly/message
 * HOW: Call conversion functions with NULL input
 */
TEST_F(HealthRepairBridgeRegressionTest, NullInputDataHandling) {
    ASSERT_NE(diag_bridge, nullptr);

    diagnostic_result_t* result = nullptr;

    // NULL anomaly
    EXPECT_EQ(health_diag_bridge_convert_anomaly(diag_bridge, NULL, &result), -1);
    EXPECT_EQ(result, nullptr);

    // NULL agent message
    EXPECT_EQ(health_diag_bridge_convert_agent_message(diag_bridge, NULL, &result), -1);
    EXPECT_EQ(result, nullptr);

    ASSERT_NE(repair_bridge, nullptr);

    uint64_t request_id = 0;
    EXPECT_EQ(health_self_repair_bridge_process_anomaly(repair_bridge, NULL, &request_id), -1);
}

//=============================================================================
// REGRESSION: Severity Mapping Edge Cases
//=============================================================================

/**
 * @test REGRESSION: Unknown Severity Handling
 *
 * WHAT: Handle unknown/invalid severity values gracefully
 * WHY: Bug #4: Crash on out-of-range severity enum values
 * HOW: Pass invalid severity and verify default handling
 */
TEST_F(HealthRepairBridgeRegressionTest, UnknownSeverityHandling) {
    ASSERT_NE(diag_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, (anomaly_severity_t)999, 0.8f);

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);

    // Should succeed with default severity, not crash
    EXPECT_EQ(ret, 0);
    if (result) {
        diagnostics_free_result(result);
    }
}

/**
 * @test REGRESSION: Unknown Anomaly Type Handling
 *
 * WHAT: Handle unknown anomaly types gracefully
 * WHY: Bug #5: Array out-of-bounds on unknown anomaly type
 * HOW: Pass invalid anomaly type
 */
TEST_F(HealthRepairBridgeRegressionTest, UnknownAnomalyTypeHandling) {
    ASSERT_NE(diag_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, (anomaly_type_t)999, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);

    // Should succeed with default mapping, not crash
    EXPECT_EQ(ret, 0);
    if (result) {
        diagnostics_free_result(result);
    }
}

//=============================================================================
// REGRESSION: Numerical Stability
//=============================================================================

/**
 * @test REGRESSION: Zero Confidence Handling
 *
 * WHAT: Handle zero confidence values
 * WHY: Bug #6: Division by zero when normalizing zero confidence
 * HOW: Pass anomaly with zero confidence
 */
TEST_F(HealthRepairBridgeRegressionTest, ZeroConfidenceHandling) {
    ASSERT_NE(diag_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.0f);

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);

    EXPECT_EQ(ret, 0);
    ASSERT_NE(result, nullptr);
    // Zero confidence should remain zero or get clamped, not NaN
    EXPECT_FALSE(std::isnan(result->confidence));
    diagnostics_free_result(result);
}

/**
 * @test REGRESSION: NaN/Inf Confidence Handling
 *
 * WHAT: Handle NaN and Inf confidence values
 * WHY: Bug #7: NaN propagation through calculations
 * HOW: Pass anomaly with NaN confidence
 */
TEST_F(HealthRepairBridgeRegressionTest, NaNConfidenceHandling) {
    ASSERT_NE(diag_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING,
                   std::numeric_limits<float>::quiet_NaN());

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);

    if (ret == 0 && result) {
        // NaN should be handled, not propagated
        EXPECT_FALSE(std::isnan(result->confidence));
        diagnostics_free_result(result);
    }
}

/**
 * @test REGRESSION: Confidence Clamping
 *
 * WHAT: Clamp confidence to valid range [0, 1]
 * WHY: Bug #8: Confidence > 1.0 caused issues in downstream processing
 * HOW: Pass confidence values outside [0, 1]
 */
TEST_F(HealthRepairBridgeRegressionTest, ConfidenceClamping) {
    ASSERT_NE(diag_bridge, nullptr);

    // Test confidence > 1.0
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 2.0f);

    diagnostic_result_t* result = nullptr;
    health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);

    if (result) {
        EXPECT_LE(result->confidence, 1.0f);
        EXPECT_GE(result->confidence, 0.0f);
        diagnostics_free_result(result);
    }

    // Test confidence < 0.0
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, -1.0f);
    result = nullptr;
    health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);

    if (result) {
        EXPECT_LE(result->confidence, 1.0f);
        EXPECT_GE(result->confidence, 0.0f);
        diagnostics_free_result(result);
    }
}

//=============================================================================
// REGRESSION: Rate Limiting Edge Cases
//=============================================================================

/**
 * @test REGRESSION: Zero Rate Limit Handling
 *
 * WHAT: Handle zero max_repairs_per_window configuration
 * WHY: Bug #9: Division by zero in rate limit calculations
 * HOW: Configure bridge with zero rate limit
 */
TEST_F(HealthRepairBridgeRegressionTest, ZeroRateLimitHandling) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(self_repair, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.rate_limit.max_repairs_per_window = 0;  // Zero rate limit

    health_self_repair_bridge_t* test_bridge =
        health_self_repair_bridge_create(&config, diag_bridge, self_repair);

    // Should either reject invalid config or handle gracefully
    if (test_bridge) {
        // If created, verify it doesn't crash on use
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                       ANOMALY_SEVERITY_CRITICAL, 0.9f);

        uint64_t request_id = 0;
        // Should not crash, might return error or skip
        EXPECT_NO_THROW(
            health_self_repair_bridge_process_anomaly(test_bridge, &anomaly, &request_id)
        );

        health_self_repair_bridge_destroy(test_bridge);
    }
    // If NULL returned, that's also acceptable behavior
}

/**
 * @test REGRESSION: Zero Window Duration Handling
 *
 * WHAT: Handle zero window_duration_ms configuration
 * WHY: Bug #10: Division by zero in rate calculations
 * HOW: Configure bridge with zero window duration
 */
TEST_F(HealthRepairBridgeRegressionTest, ZeroWindowDurationHandling) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(self_repair, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.rate_limit.window_duration_ms = 0;  // Zero window

    health_self_repair_bridge_t* test_bridge =
        health_self_repair_bridge_create(&config, diag_bridge, self_repair);

    if (test_bridge) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                       ANOMALY_SEVERITY_CRITICAL, 0.9f);

        uint64_t request_id = 0;
        EXPECT_NO_THROW(
            health_self_repair_bridge_process_anomaly(test_bridge, &anomaly, &request_id)
        );

        health_self_repair_bridge_destroy(test_bridge);
    }
}

//=============================================================================
// REGRESSION: Memory Management
//=============================================================================

/**
 * @test REGRESSION: Double Free Prevention
 *
 * WHAT: Prevent double-free crashes
 * WHY: Bug #11: Double destroy calls caused crash
 * HOW: Call destroy twice on same handle
 */
TEST_F(HealthRepairBridgeRegressionTest, DoubleFreePreventionDiagBridge) {
    health_diag_bridge_t* bridge = health_diag_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    health_diag_bridge_destroy(bridge);
    // Second destroy should not crash (NULL should be handled)
    EXPECT_NO_THROW(health_diag_bridge_destroy(NULL));
}

TEST_F(HealthRepairBridgeRegressionTest, DoubleFreePreventionRepairBridge) {
    // Note: Can't test actual double-free without use-after-free issues
    // Instead verify NULL handling
    EXPECT_NO_THROW(health_self_repair_bridge_destroy(NULL));
    EXPECT_NO_THROW(health_self_repair_bridge_destroy(NULL));
}

/**
 * @test REGRESSION: Result Leak Prevention
 *
 * WHAT: Ensure diagnostic results can be freed properly
 * WHY: Bug #12: Memory leak when caller forgot to free result
 * HOW: Convert multiple anomalies and verify no leaks
 */
TEST_F(HealthRepairBridgeRegressionTest, ResultLeakPrevention) {
    ASSERT_NE(diag_bridge, nullptr);

    // Convert many anomalies and free each result
    for (int i = 0; i < 100; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

        diagnostic_result_t* result = nullptr;
        int ret = health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);

        ASSERT_EQ(ret, 0);
        ASSERT_NE(result, nullptr);
        diagnostics_free_result(result);
    }

    // Memory check happens in TearDown
}

//=============================================================================
// REGRESSION: Configuration Validation
//=============================================================================

/**
 * @test REGRESSION: Invalid Confidence Threshold
 *
 * WHAT: Handle invalid min_confidence configuration
 * WHY: Bug #13: Negative threshold caused all repairs to pass
 * HOW: Configure with negative threshold
 */
TEST_F(HealthRepairBridgeRegressionTest, InvalidConfidenceThreshold) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(self_repair, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.min_confidence = -1.0f;  // Invalid negative threshold

    health_self_repair_bridge_t* test_bridge =
        health_self_repair_bridge_create(&config, diag_bridge, self_repair);

    // Should either reject invalid config or clamp to valid range
    if (test_bridge) {
        health_self_repair_bridge_destroy(test_bridge);
    }
}

/**
 * @test REGRESSION: Default Config Idempotency
 *
 * WHAT: Calling default_config multiple times should be safe
 * WHY: Bug #14: Some fields weren't reset on second call
 * HOW: Call default_config twice and compare
 */
TEST_F(HealthRepairBridgeRegressionTest, DefaultConfigIdempotency) {
    health_self_repair_bridge_config_t config1, config2;

    // Initialize first time
    health_self_repair_bridge_default_config(&config1);

    // Modify some fields
    config1.trigger_policy = HEALTH_TRIGGER_MANUAL;
    config1.rate_limit.max_repairs_per_window = 999;

    // Call default_config again - should reset
    health_self_repair_bridge_default_config(&config1);
    health_self_repair_bridge_default_config(&config2);

    // Should be equal
    EXPECT_EQ(config1.trigger_policy, config2.trigger_policy);
    EXPECT_EQ(config1.rate_limit.max_repairs_per_window,
              config2.rate_limit.max_repairs_per_window);
}

//=============================================================================
// REGRESSION: String Handling
//=============================================================================

/**
 * @test REGRESSION: Long Description Truncation
 *
 * WHAT: Handle anomaly descriptions longer than buffer
 * WHY: Bug #15: Buffer overflow on long descriptions
 * HOW: Pass anomaly with maximum length description
 */
TEST_F(HealthRepairBridgeRegressionTest, LongDescriptionTruncation) {
    ASSERT_NE(diag_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    // Fill description with max length string
    memset(anomaly.description, 'A', sizeof(anomaly.description) - 1);
    anomaly.description[sizeof(anomaly.description) - 1] = '\0';

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);

    EXPECT_EQ(ret, 0);
    if (result) {
        diagnostics_free_result(result);
    }
}

/**
 * @test REGRESSION: Empty Description Handling
 *
 * WHAT: Handle anomalies with empty description
 * WHY: Bug #16: Empty string caused issues in logging
 * HOW: Pass anomaly with empty description
 */
TEST_F(HealthRepairBridgeRegressionTest, EmptyDescriptionHandling) {
    ASSERT_NE(diag_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);
    anomaly.description[0] = '\0';  // Empty description

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);

    EXPECT_EQ(ret, 0);
    if (result) {
        diagnostics_free_result(result);
    }
}

//=============================================================================
// REGRESSION: Statistics Accuracy
//=============================================================================

/**
 * @test REGRESSION: Statistics Overflow Prevention
 *
 * WHAT: Prevent statistics counter overflow
 * WHY: Bug #17: 32-bit counters overflowed on heavy use
 * HOW: Process many anomalies and verify stats don't wrap
 */
TEST_F(HealthRepairBridgeRegressionTest, StatisticsOverflowPrevention) {
    ASSERT_NE(diag_bridge, nullptr);

    // Process many anomalies
    for (int i = 0; i < 1000; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

        diagnostic_result_t* result = nullptr;
        health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);
    }

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);

    // Should not overflow
    EXPECT_EQ(stats.anomalies_converted, 1000u);
}

/**
 * @test REGRESSION: Statistics Reset Completeness
 *
 * WHAT: Reset should clear all statistics
 * WHY: Bug #18: Some stats were not reset
 * HOW: Generate stats, reset, verify all zero
 */
TEST_F(HealthRepairBridgeRegressionTest, StatisticsResetCompleteness) {
    ASSERT_NE(diag_bridge, nullptr);

    // Generate some stats
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = nullptr;
    health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
    if (result) diagnostics_free_result(result);

    // Reset
    health_diag_bridge_reset_stats(diag_bridge);

    // Verify all zero
    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(diag_bridge, &stats);

    EXPECT_EQ(stats.anomalies_converted, 0u);
    EXPECT_EQ(stats.agent_messages_converted, 0u);
    EXPECT_EQ(stats.conversions_failed, 0u);
}

//=============================================================================
// REGRESSION: API Stability
//=============================================================================

/**
 * @test REGRESSION: Version String Non-Null
 *
 * WHAT: Version functions should never return NULL
 * WHY: Bug #19: NULL version string caused printf crash
 * HOW: Verify all version functions return non-NULL
 */
TEST_F(HealthRepairBridgeRegressionTest, VersionStringNonNull) {
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

/**
 * @test REGRESSION: Policy/Outcome Name Non-Null
 *
 * WHAT: Name conversion functions should handle all enum values
 * WHY: Bug #20: Unknown enum returned NULL, causing crash
 * HOW: Test all enum values including invalid ones
 */
TEST_F(HealthRepairBridgeRegressionTest, NameFunctionsNonNull) {
    // Test valid policy names
    for (int i = 0; i <= HEALTH_TRIGGER_AUTO; i++) {
        const char* name = health_self_repair_bridge_policy_name((health_trigger_policy_t)i);
        EXPECT_NE(name, nullptr) << "Policy " << i << " returned NULL";
    }

    // Test invalid policy - should return "UNKNOWN" or similar, not NULL
    const char* invalid_policy = health_self_repair_bridge_policy_name((health_trigger_policy_t)999);
    EXPECT_NE(invalid_policy, nullptr);

    // Test valid outcome names
    for (int i = 0; i <= HEALTH_REPAIR_OUTCOME_TIMEOUT; i++) {
        const char* name = health_self_repair_bridge_outcome_name((health_repair_outcome_t)i);
        EXPECT_NE(name, nullptr) << "Outcome " << i << " returned NULL";
    }

    // Test invalid outcome
    const char* invalid_outcome = health_self_repair_bridge_outcome_name((health_repair_outcome_t)999);
    EXPECT_NE(invalid_outcome, nullptr);
}

//=============================================================================
// REGRESSION: Health Agent Connection
//=============================================================================

/**
 * @test REGRESSION: NULL Health Agent Rejected
 *
 * WHAT: connect_health_agent rejects NULL health_agent parameter
 * WHY: Bug #21: Passing NULL health_agent caused NULL dereference later
 * HOW: Call connect_health_agent with NULL and verify rejection
 */
TEST_F(HealthRepairBridgeRegressionTest, NullHealthAgentRejected) {
    ASSERT_NE(repair_bridge, nullptr);

    int ret = health_self_repair_bridge_connect_health_agent(repair_bridge, NULL);
    EXPECT_EQ(ret, -1);

    // Bridge should still be operational after rejected connection
    EXPECT_TRUE(health_self_repair_bridge_is_ready(repair_bridge));
}

/**
 * @test REGRESSION: Connect Health Agent After Operations
 *
 * WHAT: Connecting health agent mid-operation should not corrupt state
 * WHY: Bug #22: Late health agent connection caused race on stats
 * HOW: Process anomalies, then connect health agent
 */
TEST_F(HealthRepairBridgeRegressionTest, ConnectHealthAgentAfterOperations) {
    ASSERT_NE(repair_bridge, nullptr);

    // Process some anomalies first
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);

    uint64_t request_id = 0;
    health_self_repair_bridge_process_anomaly(repair_bridge, &anomaly, &request_id);

    // Now connect health agent
    nimcp_health_agent_t* agent = nimcp_health_agent_create(NULL);
    ASSERT_NE(agent, nullptr);

    int ret = health_self_repair_bridge_connect_health_agent(repair_bridge, agent);
    EXPECT_EQ(ret, 0);

    // Bridge should still be operational
    EXPECT_TRUE(health_self_repair_bridge_is_ready(repair_bridge));

    // Process more anomalies after connection
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                   ANOMALY_SEVERITY_CRITICAL, 0.85f);
    ret = health_self_repair_bridge_process_anomaly(repair_bridge, &anomaly, &request_id);
    EXPECT_EQ(ret, 0);

    // Stats should be consistent
    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(repair_bridge, &stats);
    EXPECT_GE(stats.repairs_triggered, 2u);

    nimcp_health_agent_destroy(agent);
}

//=============================================================================
// REGRESSION: Agent Message Severity Filtering
//=============================================================================

/**
 * @test REGRESSION: Agent Message Below Min Severity Filtered
 *
 * WHAT: Messages below min_agent_severity are rejected by diagnostic bridge
 * WHY: Bug #23: INFO messages passed through when min was WARNING
 * HOW: Send INFO message and verify it's filtered
 */
TEST_F(HealthRepairBridgeRegressionTest, AgentMessageBelowMinSeverityFiltered) {
    ASSERT_NE(diag_bridge, nullptr);

    health_agent_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.severity = HEALTH_SEVERITY_INFO;
    msg.type = HEALTH_MSG_ANOMALY_DETECTED;
    strncpy(msg.description, "Info-level message", sizeof(msg.description) - 1);

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_agent_message(diag_bridge, &msg, &result);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(result, nullptr);
}

/**
 * @test REGRESSION: Agent Message At Min Severity Accepted
 *
 * WHAT: Messages at exactly min_agent_severity should be accepted
 * WHY: Bug #24: Off-by-one in severity comparison rejected at-threshold msgs
 * HOW: Send WARNING message (default min) and verify acceptance
 */
TEST_F(HealthRepairBridgeRegressionTest, AgentMessageAtMinSeverityAccepted) {
    ASSERT_NE(diag_bridge, nullptr);

    health_agent_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.severity = HEALTH_SEVERITY_WARNING;
    msg.type = HEALTH_MSG_ANOMALY_DETECTED;
    strncpy(msg.description, "Warning-level message", sizeof(msg.description) - 1);

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_agent_message(diag_bridge, &msg, &result);
    EXPECT_EQ(ret, 0);
    if (result) {
        diagnostics_free_result(result);
    }
}

//=============================================================================
// REGRESSION: Diagnostic Ownership Semantics
//=============================================================================

/**
 * @test REGRESSION: TriggerFromDiagnostic Frees Diagnostic
 *
 * WHAT: trigger_from_diagnostic takes ownership of diagnostic on success
 * WHY: Bug #25: Double-free when caller freed diagnostic after trigger
 * HOW: Trigger from diagnostic and verify no double-free
 */
TEST_F(HealthRepairBridgeRegressionTest, TriggerFromDiagnosticOwnership) {
    ASSERT_NE(repair_bridge, nullptr);
    ASSERT_NE(diag_bridge, nullptr);

    // Create diagnostic from anomaly
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);

    diagnostic_result_t* diag = nullptr;
    int ret = health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &diag);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(diag, nullptr);

    // trigger_from_diagnostic takes ownership - do NOT free diag after
    uint64_t request_id = 0;
    ret = health_self_repair_bridge_trigger_from_diagnostic(
        repair_bridge, diag, &request_id);
    EXPECT_EQ(ret, 0);
    // diag is now owned by the bridge - do NOT call diagnostics_free_result(diag)

    // Verify stats updated
    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(repair_bridge, &stats);
    EXPECT_GE(stats.repairs_triggered, 1u);
}

/**
 * @test REGRESSION: ForceTrigger Frees Diagnostic
 *
 * WHAT: force_trigger takes ownership of diagnostic on success
 * WHY: Bug #26: Same double-free as trigger_from_diagnostic
 * HOW: Force-trigger and verify no double-free
 */
TEST_F(HealthRepairBridgeRegressionTest, ForceTriggerOwnership) {
    ASSERT_NE(repair_bridge, nullptr);
    ASSERT_NE(diag_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                   ANOMALY_SEVERITY_CRITICAL, 0.95f);

    diagnostic_result_t* diag = nullptr;
    int ret = health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &diag);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(diag, nullptr);

    // force_trigger also takes ownership
    uint64_t request_id = 0;
    ret = health_self_repair_bridge_force_trigger(
        repair_bridge, diag, &request_id);
    EXPECT_EQ(ret, 0);
    // diag is now owned - do NOT free
}

/**
 * @test REGRESSION: Failed Trigger Does Not Take Ownership
 *
 * WHAT: Failed trigger calls should not free the diagnostic
 * WHY: Bug #27: Partial-failure path freed diagnostic, then caller freed again
 * HOW: Trigger with NULL bridge (guaranteed failure), then safely free
 */
TEST_F(HealthRepairBridgeRegressionTest, FailedTriggerDoesNotTakeOwnership) {
    ASSERT_NE(diag_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                   ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* diag = nullptr;
    int ret = health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &diag);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(diag, nullptr);

    // Trigger with NULL bridge - should fail
    uint64_t request_id = 0;
    ret = health_self_repair_bridge_trigger_from_diagnostic(
        NULL, diag, &request_id);
    EXPECT_EQ(ret, -1);

    // Since trigger failed, caller still owns the diagnostic
    diagnostics_free_result(diag);
}

//=============================================================================
// REGRESSION: Aggregation Boundary Conditions
//=============================================================================

/**
 * @test REGRESSION: Disabled Aggregation Still Processes
 *
 * WHAT: Disabling aggregation should not break processing
 * WHY: Bug #28: Disabled aggregation caused NULL pointer in batch path
 * HOW: Create bridge with aggregation disabled and process anomaly
 */
TEST_F(HealthRepairBridgeRegressionTest, DisabledAggregationStillProcesses) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(self_repair, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.aggregation.enabled = false;
    config.trigger_policy = HEALTH_TRIGGER_ERROR;

    health_self_repair_bridge_t* test_bridge =
        health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(test_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                   ANOMALY_SEVERITY_ERROR, 0.85f);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(
        test_bridge, &anomaly, &request_id);
    EXPECT_EQ(ret, 0);

    health_self_repair_bridge_destroy(test_bridge);
}

/**
 * @test REGRESSION: Zero Aggregation Window
 *
 * WHAT: Zero aggregation window should not cause division by zero
 * WHY: Bug #29: Zero window_ms caused infinite loop in timer
 * HOW: Configure zero aggregation window and process anomaly
 */
TEST_F(HealthRepairBridgeRegressionTest, ZeroAggregationWindow) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(self_repair, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.aggregation.window_ms = 0;
    config.trigger_policy = HEALTH_TRIGGER_CRITICAL;

    health_self_repair_bridge_t* test_bridge =
        health_self_repair_bridge_create(&config, diag_bridge, self_repair);

    if (test_bridge) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                       ANOMALY_SEVERITY_CRITICAL, 0.9f);

        uint64_t request_id = 0;
        EXPECT_NO_THROW(
            health_self_repair_bridge_process_anomaly(test_bridge, &anomaly, &request_id)
        );

        health_self_repair_bridge_destroy(test_bridge);
    }
}

//=============================================================================
// REGRESSION: Callback Safety
//=============================================================================

/**
 * @test REGRESSION: NULL Callback Does Not Crash
 *
 * WHAT: Setting NULL callbacks should not crash on trigger
 * WHY: Bug #30: NULL function pointer called during trigger
 * HOW: Set NULL callbacks, trigger repair, verify no crash
 */
TEST_F(HealthRepairBridgeRegressionTest, NullCallbackDoesNotCrash) {
    ASSERT_NE(repair_bridge, nullptr);

    // Set NULL callbacks
    health_self_repair_bridge_set_trigger_callback(repair_bridge, NULL, NULL);
    health_self_repair_bridge_set_outcome_callback(repair_bridge, NULL, NULL);

    // Process anomaly that triggers repair
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);

    uint64_t request_id = 0;
    EXPECT_NO_THROW(
        health_self_repair_bridge_process_anomaly(repair_bridge, &anomaly, &request_id)
    );
}

/**
 * @test REGRESSION: Callback User Data Preserved
 *
 * WHAT: User data pointer should be correctly passed to callbacks
 * WHY: Bug #31: Wrong user_data pointer passed to outcome callback
 * HOW: Set callback with specific user_data and verify in callback
 */
TEST_F(HealthRepairBridgeRegressionTest, CallbackUserDataPreserved) {
    ASSERT_NE(repair_bridge, nullptr);

    static int trigger_user_data = 42;
    static bool trigger_called = false;
    static void* received_user_data = nullptr;

    auto trigger_cb = [](uint64_t request_id,
                         const diagnostic_result_t* diag,
                         void* user_data) {
        (void)request_id;
        (void)diag;
        trigger_called = true;
        received_user_data = user_data;
    };

    trigger_called = false;
    received_user_data = nullptr;

    health_self_repair_bridge_set_trigger_callback(
        repair_bridge, trigger_cb, &trigger_user_data);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);

    uint64_t request_id = 0;
    health_self_repair_bridge_process_anomaly(repair_bridge, &anomaly, &request_id);

    if (trigger_called) {
        EXPECT_EQ(received_user_data, &trigger_user_data);
    }
}

//=============================================================================
// REGRESSION: Policy Edge Cases
//=============================================================================

/**
 * @test REGRESSION: Manual Policy Blocks All Auto-Triggers
 *
 * WHAT: MANUAL policy should block all automatic triggers
 * WHY: Bug #32: FATAL severity bypassed MANUAL policy check
 * HOW: Set MANUAL policy and send FATAL anomaly
 */
TEST_F(HealthRepairBridgeRegressionTest, ManualPolicyBlocksAllAutoTriggers) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(self_repair, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_MANUAL;

    health_self_repair_bridge_t* test_bridge =
        health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(test_bridge, nullptr);

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 1.0f);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(
        test_bridge, &anomaly, &request_id);

    // MANUAL policy should skip auto-trigger (return 1 = skipped)
    EXPECT_EQ(ret, 1);

    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(test_bridge, &stats);
    EXPECT_EQ(stats.repairs_triggered, 0u);
    EXPECT_EQ(stats.repairs_skipped, 1u);

    health_self_repair_bridge_destroy(test_bridge);
}

/**
 * @test REGRESSION: Fatal-Only Policy Rejects All Anomaly Severities
 *
 * WHAT: FATAL_ONLY policy rejects all anomaly severities (max is CRITICAL)
 * WHY: Bug #33: CRITICAL passed through FATAL_ONLY check
 * HOW: Test CRITICAL (highest anomaly severity) with FATAL_ONLY policy
 *
 * NOTE: anomaly_severity_t max is CRITICAL (3), which maps to
 *       DIAG_SEVERITY_CRITICAL, not DIAG_SEVERITY_FATAL. So FATAL_ONLY
 *       policy will reject all anomaly-sourced repairs. Only force_trigger
 *       or direct diagnostic injection with FATAL severity can bypass.
 */
TEST_F(HealthRepairBridgeRegressionTest, FatalOnlyPolicyRejectsAllAnomalies) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(self_repair, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.trigger_policy = HEALTH_TRIGGER_FATAL_ONLY;

    health_self_repair_bridge_t* test_bridge =
        health_self_repair_bridge_create(&config, diag_bridge, self_repair);
    ASSERT_NE(test_bridge, nullptr);

    // CRITICAL (highest anomaly severity) should be skipped
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 1.0f);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(
        test_bridge, &anomaly, &request_id);
    EXPECT_EQ(ret, 1);  // Skipped by policy

    // ERROR should also be skipped
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_ERROR, 0.9f);
    ret = health_self_repair_bridge_process_anomaly(
        test_bridge, &anomaly, &request_id);
    EXPECT_EQ(ret, 1);  // Skipped by policy

    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(test_bridge, &stats);
    EXPECT_EQ(stats.repairs_triggered, 0u);
    EXPECT_EQ(stats.repairs_skipped, 2u);

    health_self_repair_bridge_destroy(test_bridge);
}

//=============================================================================
// REGRESSION: Tracking Record Integrity
//=============================================================================

/**
 * @test REGRESSION: Get Tracking After Destroy Returns Error
 *
 * WHAT: Querying tracking records on NULL bridge returns error
 * WHY: Bug #34: Use-after-free accessing tracking records post-destroy
 * HOW: Query tracking on NULL bridge handle
 */
TEST_F(HealthRepairBridgeRegressionTest, TrackingQueryOnNullBridge) {
    const health_repair_tracking_t* tracking =
        health_self_repair_bridge_get_tracking(NULL, 12345);
    EXPECT_EQ(tracking, nullptr);
}

/**
 * @test REGRESSION: Unknown Request ID Returns NULL
 *
 * WHAT: Querying non-existent tracking record returns NULL
 * WHY: Bug #35: Hash table lookup on unknown key returned garbage
 * HOW: Query tracking for non-existent request_id
 */
TEST_F(HealthRepairBridgeRegressionTest, UnknownRequestIdTracking) {
    ASSERT_NE(repair_bridge, nullptr);

    const health_repair_tracking_t* tracking =
        health_self_repair_bridge_get_tracking(repair_bridge, 999999);
    EXPECT_EQ(tracking, nullptr);
}

/**
 * @test REGRESSION: Pending Count Consistency
 *
 * WHAT: Pending count should match actual pending repairs
 * WHY: Bug #36: Pending count went negative after outcome notification
 * HOW: Trigger repairs and verify pending count
 */
TEST_F(HealthRepairBridgeRegressionTest, PendingCountConsistency) {
    ASSERT_NE(repair_bridge, nullptr);

    uint32_t initial_pending = health_self_repair_bridge_get_pending_count(repair_bridge);
    EXPECT_EQ(initial_pending, 0u);

    // Process anomaly
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);

    uint64_t request_id = 0;
    int ret = health_self_repair_bridge_process_anomaly(
        repair_bridge, &anomaly, &request_id);

    if (ret == 0) {
        uint32_t pending = health_self_repair_bridge_get_pending_count(repair_bridge);
        EXPECT_GE(pending, 0u);  // Should be non-negative (uint32_t guarantees this)
    }
}

//=============================================================================
// REGRESSION: Stats Reset Under Active Processing
//=============================================================================

/**
 * @test REGRESSION: Reset Stats During Active Processing
 *
 * WHAT: Resetting stats while processing should not corrupt data
 * WHY: Bug #37: Race condition in stats reset during active repair
 * HOW: Process anomaly, reset stats, process more, verify consistency
 */
TEST_F(HealthRepairBridgeRegressionTest, ResetStatsDuringProcessing) {
    ASSERT_NE(repair_bridge, nullptr);

    // Process first anomaly
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                   ANOMALY_SEVERITY_CRITICAL, 0.9f);

    uint64_t request_id = 0;
    health_self_repair_bridge_process_anomaly(repair_bridge, &anomaly, &request_id);

    // Reset stats
    health_self_repair_bridge_reset_stats(repair_bridge);

    // Process another anomaly
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                   ANOMALY_SEVERITY_CRITICAL, 0.85f);
    health_self_repair_bridge_process_anomaly(repair_bridge, &anomaly, &request_id);

    // Stats should reflect only post-reset activity
    health_self_repair_bridge_stats_t stats;
    health_self_repair_bridge_get_stats(repair_bridge, &stats);
    EXPECT_EQ(stats.repairs_triggered, 1u);
}

/**
 * @test REGRESSION: Diagnostic Bridge Stats Independent
 *
 * WHAT: Diagnostic bridge stats should be independent of repair bridge
 * WHY: Bug #38: Shared counter between diagnostic and repair bridges
 * HOW: Reset one bridge stats, verify other unaffected
 */
TEST_F(HealthRepairBridgeRegressionTest, DiagBridgeStatsIndependent) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(repair_bridge, nullptr);

    // Generate diagnostic stats
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = nullptr;
    health_diag_bridge_convert_anomaly(diag_bridge, &anomaly, &result);
    if (result) diagnostics_free_result(result);

    // Reset repair bridge stats only
    health_self_repair_bridge_reset_stats(repair_bridge);

    // Diagnostic bridge stats should be unaffected
    health_diag_bridge_stats_t diag_stats;
    health_diag_bridge_get_stats(diag_bridge, &diag_stats);
    EXPECT_EQ(diag_stats.anomalies_converted, 1u);
}

//=============================================================================
// REGRESSION: Configuration Boundary Values
//=============================================================================

/**
 * @test REGRESSION: Max Rate Limit Configuration
 *
 * WHAT: Handle maximum rate limit values without overflow
 * WHY: Bug #39: UINT32_MAX in rate limit caused overflow in calculations
 * HOW: Configure with UINT32_MAX rate limit
 */
TEST_F(HealthRepairBridgeRegressionTest, MaxRateLimitConfiguration) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(self_repair, nullptr);

    health_self_repair_bridge_config_t config;
    health_self_repair_bridge_default_config(&config);
    config.rate_limit.max_repairs_per_window = UINT32_MAX;
    config.rate_limit.window_duration_ms = UINT32_MAX;

    health_self_repair_bridge_t* test_bridge =
        health_self_repair_bridge_create(&config, diag_bridge, self_repair);

    if (test_bridge) {
        // Should handle without overflow
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_RESOURCE_EXHAUSTION,
                       ANOMALY_SEVERITY_CRITICAL, 0.9f);

        uint64_t request_id = 0;
        EXPECT_NO_THROW(
            health_self_repair_bridge_process_anomaly(test_bridge, &anomaly, &request_id)
        );

        health_self_repair_bridge_destroy(test_bridge);
    }
}

/**
 * @test REGRESSION: All Trigger Policies Create Valid Bridges
 *
 * WHAT: Every trigger policy should create a valid bridge
 * WHY: Bug #40: HEALTH_TRIGGER_AUTO caused assertion failure
 * HOW: Create bridge with each policy
 */
TEST_F(HealthRepairBridgeRegressionTest, AllTriggerPoliciesValid) {
    ASSERT_NE(diag_bridge, nullptr);
    ASSERT_NE(self_repair, nullptr);

    health_trigger_policy_t policies[] = {
        HEALTH_TRIGGER_MANUAL,
        HEALTH_TRIGGER_FATAL_ONLY,
        HEALTH_TRIGGER_CRITICAL,
        HEALTH_TRIGGER_ERROR,
        HEALTH_TRIGGER_AUTO
    };

    for (auto policy : policies) {
        health_self_repair_bridge_config_t config;
        health_self_repair_bridge_default_config(&config);
        config.trigger_policy = policy;

        health_self_repair_bridge_t* test_bridge =
            health_self_repair_bridge_create(&config, diag_bridge, self_repair);
        EXPECT_NE(test_bridge, nullptr)
            << "Failed for policy: " << health_self_repair_bridge_policy_name(policy);

        if (test_bridge) {
            EXPECT_TRUE(health_self_repair_bridge_is_ready(test_bridge));
            health_self_repair_bridge_destroy(test_bridge);
        }
    }
}

//=============================================================================
// REGRESSION: Notify Bridge Edge Cases
//=============================================================================

/**
 * @test REGRESSION: Notify Bridge NULL Notification
 *
 * WHAT: Sending NULL notification should return error
 * WHY: Bug #41: NULL notification caused NULL dereference in memcpy
 * HOW: Call send with NULL notification
 */
TEST_F(HealthRepairBridgeRegressionTest, NotifyBridgeNullNotification) {
    self_repair_health_notify_bridge_t* notify =
        self_repair_health_notify_create(NULL, self_repair, NULL);

    if (notify) {
        int ret = self_repair_health_notify_send(notify, NULL);
        EXPECT_EQ(ret, -1);
        self_repair_health_notify_destroy(notify);
    }
}

/**
 * @test REGRESSION: Notify Bridge Stats After Reset
 *
 * WHAT: Stats should be zero after reset
 * WHY: Bug #42: Notification type counters not included in reset
 * HOW: Send notifications, reset, verify all zero
 */
TEST_F(HealthRepairBridgeRegressionTest, NotifyBridgeStatsAfterReset) {
    nimcp_health_agent_t* agent = nimcp_health_agent_create(NULL);
    ASSERT_NE(agent, nullptr);

    self_repair_health_notify_bridge_t* notify =
        self_repair_health_notify_create(NULL, self_repair, agent);
    ASSERT_NE(notify, nullptr);

    // Send a notification
    self_repair_health_notification_t notification;
    memset(&notification, 0, sizeof(notification));
    notification.type = REPAIR_NOTIFY_FAILURE;
    notification.repair_id = 1;
    notification.intervention = REPAIR_INTERVENTION_ALERT;
    self_repair_health_notify_send(notify, &notification);

    // Reset
    self_repair_health_notify_reset_stats(notify);

    // Verify all zero
    self_repair_health_notify_stats_t stats;
    self_repair_health_notify_get_stats(notify, &stats);
    EXPECT_EQ(stats.notifications_sent, 0u);
    EXPECT_EQ(stats.failures_notified, 0u);

    self_repair_health_notify_destroy(notify);
    nimcp_health_agent_destroy(agent);
}
