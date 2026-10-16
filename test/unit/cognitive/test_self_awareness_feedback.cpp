/**
 * @file test_self_awareness_feedback.cpp
 * @brief Unit tests for Self-Awareness Feedback System
 *
 * WHAT: Comprehensive unit tests for feedback loop management system
 * WHY:  Ensure correct lifecycle, policy management, and transfer functions
 * HOW:  Test init/cleanup, policy functions, transfer operations, analysis
 *
 * Function signatures tested (from include/cognitive/nimcp_self_awareness_feedback.h):
 *   int feedback_system_init(feedback_system_t* system);
 *   void feedback_system_cleanup(feedback_system_t* system);
 *   int feedback_default_policy(feedback_policy_t* policy);
 *   int feedback_conservative_policy(feedback_policy_t* policy);
 *   int feedback_aggressive_policy(feedback_policy_t* policy);
 *   int feedback_gated_policy(feedback_policy_t* policy, float gate_threshold);
 *   int feedback_set_policy(feedback_system_t*, feedback_loop_type_t, const feedback_policy_t*);
 *   int feedback_get_policy(const feedback_system_t*, feedback_loop_type_t, feedback_policy_t*);
 *   float feedback_apply_transfer(float value, transfer_function_t func, float gate_threshold, bool gate_open);
 *   int feedback_record_transfer(feedback_system_t*, ...);
 *   int feedback_compute_value(feedback_system_t*, feedback_loop_type_t, float, float*);
 *   int feedback_analyze_loop(feedback_system_t*, feedback_loop_type_t, feedback_analysis_t*);
 *   int feedback_analyze_all(feedback_system_t* system);
 *   feedback_health_t feedback_get_health(const feedback_system_t*, feedback_loop_type_t);
 *   feedback_trend_t feedback_get_trend(const feedback_system_t*, feedback_loop_type_t);
 *   bool feedback_has_unhealthy_loops(const feedback_system_t* system);
 *   int feedback_open_gate(feedback_system_t*, feedback_loop_type_t);
 *   int feedback_close_gate(feedback_system_t*, feedback_loop_type_t);
 *   bool feedback_is_gate_open(const feedback_system_t*, feedback_loop_type_t);
 *   const char* feedback_transfer_name(transfer_function_t func);
 *   const char* feedback_trend_name(feedback_trend_t trend);
 *   const char* feedback_health_name(feedback_health_t health);
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/nimcp_self_awareness_feedback.h"
}

#include "utils/nimcp_test_base.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SelfAwarenessFeedbackTest : public NimcpTestBase {
protected:
    feedback_system_t system;
    bool system_initialized = false;

    void SetUp() override {
        NimcpTestBase::SetUp();
        memset(&system, 0, sizeof(system));
    }

    void TearDown() override {
        if (system_initialized) {
            feedback_system_cleanup(&system);
            system_initialized = false;
        }
        NimcpTestBase::TearDown();
    }

    void InitSystem() {
        int result = feedback_system_init(&system);
        ASSERT_EQ(result, 0) << "Failed to initialize feedback system";
        system_initialized = true;
    }
};

/* ============================================================================
 * System Lifecycle Tests
 * ============================================================================ */

TEST_F(SelfAwarenessFeedbackTest, SystemInit_ValidParams) {
    // WHAT: Initialize feedback system
    // WHY:  Basic lifecycle validation
    // HOW:  Call init and verify

    int result = feedback_system_init(&system);
    EXPECT_EQ(result, 0);

    if (result == 0) {
        EXPECT_TRUE(system.initialized);
        system_initialized = true;
    }
}

TEST_F(SelfAwarenessFeedbackTest, SystemInit_NullParam) {
    // WHAT: Test NULL safety for init
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int result = feedback_system_init(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(SelfAwarenessFeedbackTest, SystemCleanup_ValidSystem) {
    // WHAT: Cleanup initialized system
    // WHY:  Verify proper cleanup
    // HOW:  Init then cleanup

    InitSystem();
    feedback_system_cleanup(&system);
    system_initialized = false;

    EXPECT_FALSE(system.initialized);
}

TEST_F(SelfAwarenessFeedbackTest, SystemCleanup_NullParam) {
    // WHAT: Test NULL safety for cleanup
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    feedback_system_cleanup(nullptr);
    SUCCEED() << "feedback_system_cleanup(NULL) did not crash";
}

TEST_F(SelfAwarenessFeedbackTest, SystemCleanup_DoubleCleanup) {
    // WHAT: Test double cleanup safety
    // WHY:  Defensive programming
    // HOW:  Cleanup twice

    InitSystem();
    feedback_system_cleanup(&system);
    system_initialized = false;

    // Second cleanup should be safe
    feedback_system_cleanup(&system);
    SUCCEED() << "Double cleanup did not crash";
}

/* ============================================================================
 * Policy Tests
 * ============================================================================ */

TEST_F(SelfAwarenessFeedbackTest, DefaultPolicy_ValidParam) {
    // WHAT: Get default feedback policy
    // WHY:  Test policy initialization
    // HOW:  Call and verify values

    feedback_policy_t policy;
    memset(&policy, 0, sizeof(policy));

    int result = feedback_default_policy(&policy);
    EXPECT_EQ(result, 0);

    // Default should be linear transfer
    EXPECT_EQ(policy.transfer_func, TRANSFER_LINEAR);

    // Learning rate should be positive and reasonable
    EXPECT_GT(policy.learning_rate, 0.0f);
    EXPECT_LE(policy.learning_rate, 1.0f);
}

TEST_F(SelfAwarenessFeedbackTest, DefaultPolicy_NullParam) {
    // WHAT: Test NULL safety for default policy
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int result = feedback_default_policy(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(SelfAwarenessFeedbackTest, ConservativePolicy_ValidParam) {
    // WHAT: Get conservative feedback policy
    // WHY:  Test conservative settings
    // HOW:  Call and verify values

    feedback_policy_t policy;
    memset(&policy, 0, sizeof(policy));

    int result = feedback_conservative_policy(&policy);
    EXPECT_EQ(result, 0);

    // Conservative should have lower learning rate
    EXPECT_GT(policy.learning_rate, 0.0f);
    EXPECT_LE(policy.learning_rate, 0.5f); // Lower than default

    // Should use sigmoid for smooth transfer
    EXPECT_EQ(policy.transfer_func, TRANSFER_SIGMOID);
}

TEST_F(SelfAwarenessFeedbackTest, ConservativePolicy_NullParam) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int result = feedback_conservative_policy(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(SelfAwarenessFeedbackTest, AggressivePolicy_ValidParam) {
    // WHAT: Get aggressive feedback policy
    // WHY:  Test aggressive settings
    // HOW:  Call and verify values

    feedback_policy_t policy;
    memset(&policy, 0, sizeof(policy));

    int result = feedback_aggressive_policy(&policy);
    EXPECT_EQ(result, 0);

    // Aggressive should have higher learning rate
    EXPECT_GT(policy.learning_rate, 0.3f);

    // Should use linear for direct transfer
    EXPECT_EQ(policy.transfer_func, TRANSFER_LINEAR);
}

TEST_F(SelfAwarenessFeedbackTest, AggressivePolicy_NullParam) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int result = feedback_aggressive_policy(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(SelfAwarenessFeedbackTest, GatedPolicy_ValidParams) {
    // WHAT: Get gated feedback policy
    // WHY:  Test gated transfer setup
    // HOW:  Call with threshold and verify

    feedback_policy_t policy;
    memset(&policy, 0, sizeof(policy));

    int result = feedback_gated_policy(&policy, 0.5f);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(policy.transfer_func, TRANSFER_GATED);
    EXPECT_FLOAT_EQ(policy.gate_threshold, 0.5f);
}

TEST_F(SelfAwarenessFeedbackTest, GatedPolicy_NullParam) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int result = feedback_gated_policy(nullptr, 0.5f);
    EXPECT_LT(result, 0);
}

TEST_F(SelfAwarenessFeedbackTest, SetAndGetPolicy) {
    // WHAT: Set and retrieve policy for a loop
    // WHY:  Test policy management
    // HOW:  Set policy, then get it back

    InitSystem();

    feedback_policy_t set_policy, get_policy;
    memset(&set_policy, 0, sizeof(set_policy));
    memset(&get_policy, 0, sizeof(get_policy));

    feedback_aggressive_policy(&set_policy);

    int result = feedback_set_policy(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &set_policy);
    EXPECT_EQ(result, 0);

    result = feedback_get_policy(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &get_policy);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(get_policy.transfer_func, set_policy.transfer_func);
    EXPECT_FLOAT_EQ(get_policy.learning_rate, set_policy.learning_rate);
}

TEST_F(SelfAwarenessFeedbackTest, SetPolicy_NullSystem) {
    // WHAT: Test NULL safety for set policy
    // WHY:  Defensive programming
    // HOW:  Call with NULL system

    feedback_policy_t policy;
    feedback_default_policy(&policy);

    int result = feedback_set_policy(nullptr, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &policy);
    EXPECT_LT(result, 0);
}

TEST_F(SelfAwarenessFeedbackTest, SetPolicy_NullPolicy) {
    // WHAT: Test NULL safety for set policy
    // WHY:  Defensive programming
    // HOW:  Call with NULL policy

    InitSystem();

    int result = feedback_set_policy(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(SelfAwarenessFeedbackTest, GetPolicy_NullParams) {
    // WHAT: Test NULL safety for get policy
    // WHY:  Defensive programming
    // HOW:  Call with NULL params

    InitSystem();
    feedback_policy_t policy;

    int result = feedback_get_policy(nullptr, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &policy);
    EXPECT_LT(result, 0);

    result = feedback_get_policy(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, nullptr);
    EXPECT_LT(result, 0);
}

/* ============================================================================
 * Transfer Function Tests
 * ============================================================================ */

TEST_F(SelfAwarenessFeedbackTest, ApplyTransfer_Linear) {
    // WHAT: Test linear transfer function
    // WHY:  Verify linear pass-through
    // HOW:  Apply linear transfer

    float result = feedback_apply_transfer(0.5f, TRANSFER_LINEAR, 0.0f, false);
    EXPECT_FLOAT_EQ(result, 0.5f);

    result = feedback_apply_transfer(-0.3f, TRANSFER_LINEAR, 0.0f, false);
    EXPECT_FLOAT_EQ(result, -0.3f);
}

TEST_F(SelfAwarenessFeedbackTest, ApplyTransfer_Sigmoid) {
    // WHAT: Test sigmoid transfer function
    // WHY:  Verify smooth 0-1 mapping
    // HOW:  Apply sigmoid at various points

    float result = feedback_apply_transfer(0.0f, TRANSFER_SIGMOID, 0.0f, false);
    EXPECT_NEAR(result, 0.5f, 0.01f); // sigmoid(0) = 0.5

    // Large positive should approach 1
    result = feedback_apply_transfer(5.0f, TRANSFER_SIGMOID, 0.0f, false);
    EXPECT_GT(result, 0.9f);

    // Large negative should approach 0
    result = feedback_apply_transfer(-5.0f, TRANSFER_SIGMOID, 0.0f, false);
    EXPECT_LT(result, 0.1f);
}

TEST_F(SelfAwarenessFeedbackTest, ApplyTransfer_Tanh) {
    // WHAT: Test tanh transfer function
    // WHY:  Verify -1 to 1 mapping
    // HOW:  Apply tanh at various points

    float result = feedback_apply_transfer(0.0f, TRANSFER_TANH, 0.0f, false);
    EXPECT_NEAR(result, 0.0f, 0.01f); // tanh(0) = 0

    // Large positive should approach 1
    result = feedback_apply_transfer(5.0f, TRANSFER_TANH, 0.0f, false);
    EXPECT_GT(result, 0.99f);

    // Large negative should approach -1
    result = feedback_apply_transfer(-5.0f, TRANSFER_TANH, 0.0f, false);
    EXPECT_LT(result, -0.99f);
}

TEST_F(SelfAwarenessFeedbackTest, ApplyTransfer_Step) {
    // WHAT: Test step transfer function
    // WHY:  Verify binary threshold behavior
    // HOW:  Apply step at threshold boundary

    // Value at 0 should give one result (depends on implementation)
    float result = feedback_apply_transfer(0.5f, TRANSFER_STEP, 0.0f, false);
    // Step should return 1 for positive, 0 for negative
    EXPECT_GE(result, 0.0f);
    EXPECT_LE(result, 1.0f);
}

TEST_F(SelfAwarenessFeedbackTest, ApplyTransfer_Gated_Open) {
    // WHAT: Test gated transfer when gate is open
    // WHY:  Verify pass-through when gate open
    // HOW:  Apply gated with gate_open=true

    float result = feedback_apply_transfer(0.7f, TRANSFER_GATED, 0.5f, true);
    // Gate open should pass through
    EXPECT_FLOAT_EQ(result, 0.7f);
}

TEST_F(SelfAwarenessFeedbackTest, ApplyTransfer_Gated_Closed) {
    // WHAT: Test gated transfer when gate is closed
    // WHY:  Verify blocking when gate closed
    // HOW:  Apply gated with gate_open=false

    float result = feedback_apply_transfer(0.7f, TRANSFER_GATED, 0.5f, false);
    // Gate closed should return 0
    EXPECT_FLOAT_EQ(result, 0.0f);
}

/* ============================================================================
 * Transfer Record Tests
 * ============================================================================ */

TEST_F(SelfAwarenessFeedbackTest, RecordTransfer_ValidParams) {
    // WHAT: Record a feedback transfer
    // WHY:  Test history recording
    // HOW:  Record transfer and verify

    InitSystem();

    int result = feedback_record_transfer(
        &system,
        FEEDBACK_INTROSPECTION_TO_SELF_MODEL,
        0.5f,   // source_value
        0.45f,  // transferred_value
        0.1f,   // target_delta
        1.5f,   // latency_ms
        true,   // successful
        nullptr // no error
    );
    EXPECT_EQ(result, 0);

    // Total transfers should have incremented
    EXPECT_GE(system.total_transfers, 1u);
}

TEST_F(SelfAwarenessFeedbackTest, RecordTransfer_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL system

    int result = feedback_record_transfer(
        nullptr,
        FEEDBACK_INTROSPECTION_TO_SELF_MODEL,
        0.5f, 0.45f, 0.1f, 1.5f, true, nullptr
    );
    EXPECT_LT(result, 0);
}

TEST_F(SelfAwarenessFeedbackTest, RecordTransfer_Failed) {
    // WHAT: Record a failed transfer
    // WHY:  Test failure recording
    // HOW:  Record with successful=false

    InitSystem();

    int result = feedback_record_transfer(
        &system,
        FEEDBACK_INTROSPECTION_TO_SELF_MODEL,
        0.5f,
        0.0f,
        0.0f,
        5.0f,
        false,
        "Timeout"
    );
    EXPECT_EQ(result, 0);

    EXPECT_GE(system.total_failures, 1u);
}

/* ============================================================================
 * Value Computation Tests
 * ============================================================================ */

TEST_F(SelfAwarenessFeedbackTest, ComputeValue_ValidParams) {
    // WHAT: Compute feedback value with policy
    // WHY:  Test policy-adjusted computation
    // HOW:  Set policy, compute value

    InitSystem();

    float output = 0.0f;
    int result = feedback_compute_value(
        &system,
        FEEDBACK_INTROSPECTION_TO_SELF_MODEL,
        0.5f,
        &output
    );
    EXPECT_EQ(result, 0);

    // Output should be in valid range
    EXPECT_GE(output, -1.0f);
    EXPECT_LE(output, 1.0f);
}

TEST_F(SelfAwarenessFeedbackTest, ComputeValue_NullParams) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL params

    InitSystem();
    float output = 0.0f;

    int result = feedback_compute_value(nullptr, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, 0.5f, &output);
    EXPECT_LT(result, 0);

    result = feedback_compute_value(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, 0.5f, nullptr);
    EXPECT_LT(result, 0);
}

/* ============================================================================
 * Analysis Tests
 * ============================================================================ */

TEST_F(SelfAwarenessFeedbackTest, AnalyzeLoop_ValidParams) {
    // WHAT: Analyze a feedback loop
    // WHY:  Test analysis functions
    // HOW:  Record some transfers, then analyze

    InitSystem();

    // Record some transfers
    for (int i = 0; i < 10; i++) {
        feedback_record_transfer(
            &system,
            FEEDBACK_INTROSPECTION_TO_SELF_MODEL,
            (float)i / 10.0f,
            (float)i / 10.0f,
            0.01f,
            1.0f,
            true,
            nullptr
        );
    }

    feedback_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));

    int result = feedback_analyze_loop(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &analysis);
    EXPECT_EQ(result, 0);

    // Should have 100% success rate
    EXPECT_FLOAT_EQ(analysis.success_rate, 1.0f);
}

TEST_F(SelfAwarenessFeedbackTest, AnalyzeLoop_NullParams) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL params

    InitSystem();
    feedback_analysis_t analysis;

    int result = feedback_analyze_loop(nullptr, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &analysis);
    EXPECT_LT(result, 0);

    result = feedback_analyze_loop(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(SelfAwarenessFeedbackTest, AnalyzeAll_ValidSystem) {
    // WHAT: Analyze all feedback loops
    // WHY:  Test bulk analysis
    // HOW:  Call analyze_all

    InitSystem();

    int result = feedback_analyze_all(&system);
    EXPECT_EQ(result, 0);
}

TEST_F(SelfAwarenessFeedbackTest, AnalyzeAll_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int result = feedback_analyze_all(nullptr);
    EXPECT_LT(result, 0);
}

/* ============================================================================
 * Health and Trend Tests
 * ============================================================================ */

TEST_F(SelfAwarenessFeedbackTest, GetHealth_ValidSystem) {
    // WHAT: Get loop health status
    // WHY:  Test health reporting
    // HOW:  Get health for a loop

    InitSystem();

    feedback_health_t health = feedback_get_health(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL);

    // Should be one of valid health values
    EXPECT_GE(static_cast<int>(health), static_cast<int>(FEEDBACK_HEALTH_OPTIMAL));
    EXPECT_LE(static_cast<int>(health), static_cast<int>(FEEDBACK_HEALTH_DEAD));
}

TEST_F(SelfAwarenessFeedbackTest, GetTrend_ValidSystem) {
    // WHAT: Get loop trend
    // WHY:  Test trend reporting
    // HOW:  Get trend for a loop

    InitSystem();

    feedback_trend_t trend = feedback_get_trend(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL);

    // Should be one of valid trend values
    EXPECT_GE(static_cast<int>(trend), static_cast<int>(TREND_STABLE));
    EXPECT_LE(static_cast<int>(trend), static_cast<int>(TREND_DIVERGING));
}

TEST_F(SelfAwarenessFeedbackTest, HasUnhealthyLoops_HealthySystem) {
    // WHAT: Check for unhealthy loops
    // WHY:  Test system health check
    // HOW:  Check freshly initialized system

    InitSystem();

    bool has_unhealthy = feedback_has_unhealthy_loops(&system);
    // Fresh system should be healthy
    EXPECT_FALSE(has_unhealthy);
}

TEST_F(SelfAwarenessFeedbackTest, HasUnhealthyLoops_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool has_unhealthy = feedback_has_unhealthy_loops(nullptr);
    // NULL should return true (unsafe)
    EXPECT_TRUE(has_unhealthy);
}

/* ============================================================================
 * Gate Tests
 * ============================================================================ */

TEST_F(SelfAwarenessFeedbackTest, GateOperations) {
    // WHAT: Test gate open/close operations
    // WHY:  Verify gate control
    // HOW:  Open, check, close, check

    InitSystem();

    // Set gated policy
    feedback_policy_t policy;
    feedback_gated_policy(&policy, 0.5f);
    feedback_set_policy(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &policy);

    // Open gate
    int result = feedback_open_gate(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(feedback_is_gate_open(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL));

    // Close gate
    result = feedback_close_gate(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(feedback_is_gate_open(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL));
}

TEST_F(SelfAwarenessFeedbackTest, GateOperations_NullSystem) {
    // WHAT: Test NULL safety for gate ops
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int result = feedback_open_gate(nullptr, FEEDBACK_INTROSPECTION_TO_SELF_MODEL);
    EXPECT_LT(result, 0);

    result = feedback_close_gate(nullptr, FEEDBACK_INTROSPECTION_TO_SELF_MODEL);
    EXPECT_LT(result, 0);

    bool is_open = feedback_is_gate_open(nullptr, FEEDBACK_INTROSPECTION_TO_SELF_MODEL);
    EXPECT_FALSE(is_open);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(SelfAwarenessFeedbackTest, TransferName_AllTypes) {
    // WHAT: Get names for all transfer functions
    // WHY:  Test string conversion
    // HOW:  Call for each type

    const char* name = feedback_transfer_name(TRANSFER_LINEAR);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = feedback_transfer_name(TRANSFER_SIGMOID);
    EXPECT_NE(name, nullptr);

    name = feedback_transfer_name(TRANSFER_TANH);
    EXPECT_NE(name, nullptr);

    name = feedback_transfer_name(TRANSFER_EXPONENTIAL);
    EXPECT_NE(name, nullptr);

    name = feedback_transfer_name(TRANSFER_LOGARITHMIC);
    EXPECT_NE(name, nullptr);

    name = feedback_transfer_name(TRANSFER_STEP);
    EXPECT_NE(name, nullptr);

    name = feedback_transfer_name(TRANSFER_GATED);
    EXPECT_NE(name, nullptr);
}

TEST_F(SelfAwarenessFeedbackTest, TrendName_AllTypes) {
    // WHAT: Get names for all trends
    // WHY:  Test string conversion
    // HOW:  Call for each type

    const char* name = feedback_trend_name(TREND_STABLE);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = feedback_trend_name(TREND_INCREASING);
    EXPECT_NE(name, nullptr);

    name = feedback_trend_name(TREND_DECREASING);
    EXPECT_NE(name, nullptr);

    name = feedback_trend_name(TREND_OSCILLATING);
    EXPECT_NE(name, nullptr);

    name = feedback_trend_name(TREND_DIVERGING);
    EXPECT_NE(name, nullptr);
}

TEST_F(SelfAwarenessFeedbackTest, HealthName_AllTypes) {
    // WHAT: Get names for all health statuses
    // WHY:  Test string conversion
    // HOW:  Call for each type

    const char* name = feedback_health_name(FEEDBACK_HEALTH_OPTIMAL);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = feedback_health_name(FEEDBACK_HEALTH_DEGRADED);
    EXPECT_NE(name, nullptr);

    name = feedback_health_name(FEEDBACK_HEALTH_FAILING);
    EXPECT_NE(name, nullptr);

    name = feedback_health_name(FEEDBACK_HEALTH_DEAD);
    EXPECT_NE(name, nullptr);
}
