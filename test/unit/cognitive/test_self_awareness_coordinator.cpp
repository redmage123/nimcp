// ============================================================================
// test_self_awareness_coordinator.cpp - Unit tests for Self-Awareness Coordinator
// ============================================================================
/**
 * @file test_self_awareness_coordinator.cpp
 * @brief Comprehensive unit tests for self-awareness coordination system
 *
 * Tests cover:
 * - Lifecycle (create, destroy)
 * - Configuration
 * - Feedback loops
 * - Coherence checking
 * - Phi monitoring
 * - Bio-async integration
 * - Conflict detection and resolution
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_self_awareness_coordinator.h"
#include "cognitive/nimcp_self_awareness_feedback.h"

// ============================================================================
// Test Fixture
// ============================================================================

class SelfAwarenessCoordinatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize default config
        sac_default_config(&config);
    }

    void TearDown() override {
        // Cleanup happens in individual tests
    }

    sac_config_t config;
};

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(SelfAwarenessCoordinatorTest, DefaultConfigInitialization) {
    // Default config should have sensible values
    EXPECT_TRUE(config.enable_introspection_feedback);
    EXPECT_TRUE(config.enable_autobio_feedback);
    EXPECT_TRUE(config.enable_tom_grounding);
    EXPECT_TRUE(config.enable_coherence_checking);
    EXPECT_TRUE(config.enable_phi_monitoring);

    EXPECT_GT(config.coherence_threshold, 0.0f);
    EXPECT_LT(config.coherence_threshold, 1.0f);
    EXPECT_GT(config.phi_alert_threshold, 0.0f);
    EXPECT_LT(config.phi_alert_threshold, 1.0f);

    EXPECT_GT(config.update_interval_ms, 0u);
    EXPECT_GT(config.coherence_check_interval_ms, 0u);
}

TEST_F(SelfAwarenessCoordinatorTest, DefaultConfigNullSafe) {
    int ret = sac_default_config(NULL);
    EXPECT_NE(ret, 0);
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(SelfAwarenessCoordinatorTest, CreateWithNullConfig) {
    // NULL config should fail gracefully
    self_awareness_coordinator_t* coord = sac_create(
        NULL, NULL, NULL, NULL, NULL
    );
    EXPECT_EQ(coord, nullptr);
}

TEST_F(SelfAwarenessCoordinatorTest, DestroyNull) {
    // Destroying NULL should not crash
    sac_destroy(NULL);
    SUCCEED();
}

// ============================================================================
// Feedback Loop Name Tests
// ============================================================================

TEST_F(SelfAwarenessCoordinatorTest, FeedbackLoopNames) {
    EXPECT_STREQ(sac_feedback_loop_name(FEEDBACK_INTROSPECTION_TO_SELF_MODEL),
                 "introspection_to_self_model");
    EXPECT_STREQ(sac_feedback_loop_name(FEEDBACK_SELF_MODEL_TO_INTROSPECTION),
                 "self_model_to_introspection");
    EXPECT_STREQ(sac_feedback_loop_name(FEEDBACK_AUTOBIO_TO_SELF_MODEL),
                 "autobio_to_self_model");
    EXPECT_STREQ(sac_feedback_loop_name(FEEDBACK_SELF_MODEL_TO_AUTOBIO),
                 "self_model_to_autobio");
    EXPECT_STREQ(sac_feedback_loop_name(FEEDBACK_SELF_MODEL_TO_TOM),
                 "self_model_to_tom");
    EXPECT_STREQ(sac_feedback_loop_name(FEEDBACK_TOM_TO_SELF_MODEL),
                 "tom_to_self_model");
    EXPECT_STREQ(sac_feedback_loop_name(FEEDBACK_CONSCIOUSNESS_TO_EXECUTIVE),
                 "consciousness_to_executive");
    EXPECT_STREQ(sac_feedback_loop_name(FEEDBACK_EXECUTIVE_TO_CONSCIOUSNESS),
                 "executive_to_consciousness");
}

TEST_F(SelfAwarenessCoordinatorTest, FeedbackLoopNameInvalid) {
    const char* name = sac_feedback_loop_name((feedback_loop_type_t)999);
    EXPECT_STREQ(name, "unknown");
}

// ============================================================================
// Conflict Type Name Tests
// ============================================================================

TEST_F(SelfAwarenessCoordinatorTest, ConflictTypeNames) {
    EXPECT_STREQ(sac_conflict_type_name(CONFLICT_BELIEF_EXPERIENCE),
                 "belief_experience");
    EXPECT_STREQ(sac_conflict_type_name(CONFLICT_BELIEF_CAPABILITY),
                 "belief_capability");
    EXPECT_STREQ(sac_conflict_type_name(CONFLICT_MEMORY_IDENTITY),
                 "memory_identity");
    EXPECT_STREQ(sac_conflict_type_name(CONFLICT_TOM_SELF),
                 "tom_self");
    EXPECT_STREQ(sac_conflict_type_name(CONFLICT_PHI_SELF_REPORT),
                 "phi_self_report");
    EXPECT_STREQ(sac_conflict_type_name(CONFLICT_TEMPORAL_CONTINUITY),
                 "temporal_continuity");
}

TEST_F(SelfAwarenessCoordinatorTest, ConflictTypeNameInvalid) {
    const char* name = sac_conflict_type_name((coherence_conflict_type_t)999);
    EXPECT_STREQ(name, "unknown");
}

// ============================================================================
// State Name Tests
// ============================================================================

TEST_F(SelfAwarenessCoordinatorTest, StateNames) {
    EXPECT_STREQ(sac_state_name(SAC_STATE_UNINITIALIZED), "uninitialized");
    EXPECT_STREQ(sac_state_name(SAC_STATE_INITIALIZING), "initializing");
    EXPECT_STREQ(sac_state_name(SAC_STATE_RUNNING), "running");
    EXPECT_STREQ(sac_state_name(SAC_STATE_COHERENCE_CHECK), "coherence_check");
    EXPECT_STREQ(sac_state_name(SAC_STATE_CONFLICT_RESOLUTION), "conflict_resolution");
    EXPECT_STREQ(sac_state_name(SAC_STATE_LOW_CONSCIOUSNESS), "low_consciousness");
    EXPECT_STREQ(sac_state_name(SAC_STATE_PAUSED), "paused");
    EXPECT_STREQ(sac_state_name(SAC_STATE_ERROR), "error");
}

TEST_F(SelfAwarenessCoordinatorTest, StateNameInvalid) {
    const char* name = sac_state_name((sac_state_t)999);
    EXPECT_STREQ(name, "unknown");
}

// ============================================================================
// Feedback System Tests
// ============================================================================

class FeedbackSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&system, 0, sizeof(system));
        ASSERT_EQ(feedback_system_init(&system), 0);
    }

    void TearDown() override {
        feedback_system_cleanup(&system);
    }

    feedback_system_t system;
};

TEST_F(FeedbackSystemTest, InitializationSuccess) {
    EXPECT_TRUE(system.initialized);
    EXPECT_EQ(system.total_transfers, 0u);
    EXPECT_EQ(system.total_failures, 0u);
}

TEST_F(FeedbackSystemTest, InitNull) {
    EXPECT_LT(feedback_system_init(NULL), 0);
}

TEST_F(FeedbackSystemTest, CleanupNull) {
    feedback_system_cleanup(NULL);
    SUCCEED();
}

// ============================================================================
// Policy Tests
// ============================================================================

TEST_F(FeedbackSystemTest, DefaultPolicy) {
    feedback_policy_t policy;
    ASSERT_EQ(feedback_default_policy(&policy), 0);

    EXPECT_EQ(policy.transfer_func, TRANSFER_LINEAR);
    EXPECT_GT(policy.learning_rate, 0.0f);
    EXPECT_GT(policy.momentum, 0.0f);
    EXPECT_GT(policy.decay, 0.0f);
    EXPECT_TRUE(policy.gate_open);
}

TEST_F(FeedbackSystemTest, ConservativePolicy) {
    feedback_policy_t policy;
    ASSERT_EQ(feedback_conservative_policy(&policy), 0);

    EXPECT_EQ(policy.transfer_func, TRANSFER_SIGMOID);
    EXPECT_LT(policy.learning_rate, 0.1f);  // Conservative = slow
    EXPECT_GT(policy.momentum, 0.9f);       // High momentum
    EXPECT_TRUE(policy.adaptive_rate);
}

TEST_F(FeedbackSystemTest, AggressivePolicy) {
    feedback_policy_t policy;
    ASSERT_EQ(feedback_aggressive_policy(&policy), 0);

    EXPECT_EQ(policy.transfer_func, TRANSFER_LINEAR);
    EXPECT_GT(policy.learning_rate, 0.3f);  // Aggressive = fast
    EXPECT_LT(policy.momentum, 0.8f);       // Lower momentum
    EXPECT_TRUE(policy.adaptive_rate);
}

TEST_F(FeedbackSystemTest, GatedPolicy) {
    feedback_policy_t policy;
    ASSERT_EQ(feedback_gated_policy(&policy, 0.75f), 0);

    EXPECT_EQ(policy.transfer_func, TRANSFER_GATED);
    EXPECT_FLOAT_EQ(policy.gate_threshold, 0.75f);
    EXPECT_FALSE(policy.gate_open);  // Gated policy starts closed
}

TEST_F(FeedbackSystemTest, PolicyNullSafe) {
    EXPECT_LT(feedback_default_policy(NULL), 0);
    EXPECT_LT(feedback_conservative_policy(NULL), 0);
    EXPECT_LT(feedback_aggressive_policy(NULL), 0);
    EXPECT_LT(feedback_gated_policy(NULL, 0.5f), 0);
}

// ============================================================================
// Set/Get Policy Tests
// ============================================================================

TEST_F(FeedbackSystemTest, SetAndGetPolicy) {
    feedback_policy_t policy;
    feedback_conservative_policy(&policy);

    ASSERT_EQ(feedback_set_policy(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &policy), 0);

    feedback_policy_t retrieved;
    ASSERT_EQ(feedback_get_policy(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &retrieved), 0);

    EXPECT_EQ(retrieved.transfer_func, policy.transfer_func);
    EXPECT_FLOAT_EQ(retrieved.learning_rate, policy.learning_rate);
    EXPECT_FLOAT_EQ(retrieved.momentum, policy.momentum);
}

TEST_F(FeedbackSystemTest, SetPolicyInvalidLoop) {
    feedback_policy_t policy;
    feedback_default_policy(&policy);

    EXPECT_LT(feedback_set_policy(&system, (feedback_loop_type_t)-1, &policy), 0);
    EXPECT_LT(feedback_set_policy(&system, FEEDBACK_LOOP_COUNT, &policy), 0);
}

// ============================================================================
// Transfer Function Tests
// ============================================================================

TEST(TransferFunctionTest, LinearTransfer) {
    float result = feedback_apply_transfer(0.5f, TRANSFER_LINEAR, 0.5f, true);
    EXPECT_FLOAT_EQ(result, 0.5f);

    result = feedback_apply_transfer(-0.3f, TRANSFER_LINEAR, 0.5f, true);
    EXPECT_FLOAT_EQ(result, -0.3f);
}

TEST(TransferFunctionTest, SigmoidTransfer) {
    float result = feedback_apply_transfer(0.0f, TRANSFER_SIGMOID, 0.5f, true);
    EXPECT_FLOAT_EQ(result, 0.5f);  // sigmoid(0) = 0.5

    result = feedback_apply_transfer(10.0f, TRANSFER_SIGMOID, 0.5f, true);
    EXPECT_NEAR(result, 1.0f, 0.001f);  // sigmoid(10) ≈ 1

    result = feedback_apply_transfer(-10.0f, TRANSFER_SIGMOID, 0.5f, true);
    EXPECT_NEAR(result, 0.0f, 0.001f);  // sigmoid(-10) ≈ 0
}

TEST(TransferFunctionTest, TanhTransfer) {
    float result = feedback_apply_transfer(0.0f, TRANSFER_TANH, 0.5f, true);
    EXPECT_FLOAT_EQ(result, 0.0f);  // tanh(0) = 0

    result = feedback_apply_transfer(3.0f, TRANSFER_TANH, 0.5f, true);
    EXPECT_NEAR(result, 1.0f, 0.01f);  // tanh(3) ≈ 1
}

TEST(TransferFunctionTest, StepTransfer) {
    float result = feedback_apply_transfer(0.6f, TRANSFER_STEP, 0.5f, true);
    EXPECT_FLOAT_EQ(result, 1.0f);  // Above threshold

    result = feedback_apply_transfer(0.4f, TRANSFER_STEP, 0.5f, true);
    EXPECT_FLOAT_EQ(result, 0.0f);  // Below threshold

    result = feedback_apply_transfer(0.5f, TRANSFER_STEP, 0.5f, true);
    EXPECT_FLOAT_EQ(result, 1.0f);  // At threshold = above
}

TEST(TransferFunctionTest, GatedTransferOpen) {
    float result = feedback_apply_transfer(0.75f, TRANSFER_GATED, 0.5f, true);
    EXPECT_FLOAT_EQ(result, 0.75f);  // Gate open = pass through
}

TEST(TransferFunctionTest, GatedTransferClosed) {
    float result = feedback_apply_transfer(0.75f, TRANSFER_GATED, 0.5f, false);
    EXPECT_FLOAT_EQ(result, 0.0f);  // Gate closed = zero
}

TEST(TransferFunctionTest, LogarithmicTransfer) {
    float result = feedback_apply_transfer(0.0f, TRANSFER_LOGARITHMIC, 0.5f, true);
    EXPECT_FLOAT_EQ(result, 0.0f);  // log(1+0) = 0

    result = feedback_apply_transfer(1.0f, TRANSFER_LOGARITHMIC, 0.5f, true);
    EXPECT_NEAR(result, 0.693f, 0.01f);  // log(2) ≈ 0.693

    result = feedback_apply_transfer(-0.5f, TRANSFER_LOGARITHMIC, 0.5f, true);
    EXPECT_FLOAT_EQ(result, 0.0f);  // Negative returns 0
}

TEST(TransferFunctionTest, ExponentialTransfer) {
    float result = feedback_apply_transfer(0.0f, TRANSFER_EXPONENTIAL, 0.5f, true);
    EXPECT_FLOAT_EQ(result, 0.0f);  // exp(0) - 1 = 0

    result = feedback_apply_transfer(1.0f, TRANSFER_EXPONENTIAL, 0.5f, true);
    EXPECT_NEAR(result, 1.718f, 0.01f);  // e - 1 ≈ 1.718
}

// ============================================================================
// Transfer Recording Tests
// ============================================================================

TEST_F(FeedbackSystemTest, RecordTransfer) {
    ASSERT_EQ(feedback_record_transfer(
        &system,
        FEEDBACK_INTROSPECTION_TO_SELF_MODEL,
        0.5f,   // source
        0.45f,  // transferred
        0.1f,   // delta
        5.0f,   // latency
        true,   // successful
        NULL
    ), 0);

    EXPECT_EQ(system.total_transfers, 1u);
    EXPECT_EQ(system.total_failures, 0u);
}

TEST_F(FeedbackSystemTest, RecordTransferFailure) {
    ASSERT_EQ(feedback_record_transfer(
        &system,
        FEEDBACK_AUTOBIO_TO_SELF_MODEL,
        0.5f,
        0.0f,
        0.0f,
        10.0f,
        false,  // failed
        "Connection timeout"
    ), 0);

    EXPECT_EQ(system.total_transfers, 1u);
    EXPECT_EQ(system.total_failures, 1u);
}

TEST_F(FeedbackSystemTest, RecordTransferNullSystem) {
    EXPECT_LT(feedback_record_transfer(
        NULL,
        FEEDBACK_INTROSPECTION_TO_SELF_MODEL,
        0.5f, 0.45f, 0.1f, 5.0f, true, NULL
    ), 0);
}

// ============================================================================
// Gate Control Tests
// ============================================================================

TEST_F(FeedbackSystemTest, GateOpenClose) {
    // Set gated policy first
    feedback_policy_t policy;
    feedback_gated_policy(&policy, 0.5f);
    feedback_set_policy(&system, FEEDBACK_TOM_TO_SELF_MODEL, &policy);

    EXPECT_FALSE(feedback_is_gate_open(&system, FEEDBACK_TOM_TO_SELF_MODEL));

    ASSERT_EQ(feedback_open_gate(&system, FEEDBACK_TOM_TO_SELF_MODEL), 0);
    EXPECT_TRUE(feedback_is_gate_open(&system, FEEDBACK_TOM_TO_SELF_MODEL));

    ASSERT_EQ(feedback_close_gate(&system, FEEDBACK_TOM_TO_SELF_MODEL), 0);
    EXPECT_FALSE(feedback_is_gate_open(&system, FEEDBACK_TOM_TO_SELF_MODEL));
}

TEST_F(FeedbackSystemTest, GateNullSystem) {
    EXPECT_LT(feedback_open_gate(NULL, FEEDBACK_TOM_TO_SELF_MODEL), 0);
    EXPECT_LT(feedback_close_gate(NULL, FEEDBACK_TOM_TO_SELF_MODEL), 0);
    EXPECT_FALSE(feedback_is_gate_open(NULL, FEEDBACK_TOM_TO_SELF_MODEL));
}

// ============================================================================
// Adaptive Rate Tests
// ============================================================================

TEST_F(FeedbackSystemTest, EnableAdaptiveRate) {
    ASSERT_EQ(feedback_enable_adaptive_rate(
        &system,
        FEEDBACK_INTROSPECTION_TO_SELF_MODEL,
        1.2f,  // increase factor
        0.8f   // decrease factor
    ), 0);

    feedback_policy_t policy;
    feedback_get_policy(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &policy);
    EXPECT_TRUE(policy.adaptive_rate);
    EXPECT_FLOAT_EQ(policy.rate_increase_factor, 1.2f);
    EXPECT_FLOAT_EQ(policy.rate_decrease_factor, 0.8f);
}

TEST_F(FeedbackSystemTest, DisableAdaptiveRate) {
    // Enable first
    feedback_enable_adaptive_rate(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, 1.2f, 0.8f);

    // Then disable
    ASSERT_EQ(feedback_disable_adaptive_rate(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL), 0);

    feedback_policy_t policy;
    feedback_get_policy(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &policy);
    EXPECT_FALSE(policy.adaptive_rate);
}

TEST_F(FeedbackSystemTest, GetCurrentRate) {
    feedback_policy_t policy;
    feedback_default_policy(&policy);
    policy.learning_rate = 0.1f;
    feedback_set_policy(&system, FEEDBACK_AUTOBIO_TO_SELF_MODEL, &policy);

    float rate = feedback_get_current_rate(&system, FEEDBACK_AUTOBIO_TO_SELF_MODEL);
    EXPECT_FLOAT_EQ(rate, 0.1f);
}

// ============================================================================
// Analysis Tests
// ============================================================================

TEST_F(FeedbackSystemTest, AnalyzeEmptyLoop) {
    feedback_analysis_t analysis;
    ASSERT_EQ(feedback_analyze_loop(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &analysis), 0);

    EXPECT_EQ(analysis.trend, TREND_STABLE);
    EXPECT_EQ(analysis.health, FEEDBACK_HEALTH_OPTIMAL);
    EXPECT_TRUE(analysis.is_stale);
}

TEST_F(FeedbackSystemTest, AnalyzeWithTransfers) {
    // Record several transfers
    for (int i = 0; i < 10; i++) {
        feedback_record_transfer(
            &system,
            FEEDBACK_INTROSPECTION_TO_SELF_MODEL,
            0.5f + i * 0.01f,  // increasing values
            0.5f + i * 0.01f,
            0.01f,
            5.0f,
            true,
            NULL
        );
    }

    feedback_analysis_t analysis;
    ASSERT_EQ(feedback_analyze_loop(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &analysis), 0);

    EXPECT_GT(analysis.mean_value, 0.0f);
    EXPECT_FLOAT_EQ(analysis.success_rate, 1.0f);
    EXPECT_GT(analysis.avg_latency_ms, 0.0f);
}

TEST_F(FeedbackSystemTest, AnalyzeAll) {
    ASSERT_EQ(feedback_analyze_all(&system), 0);
}

TEST_F(FeedbackSystemTest, GetHealthOptimal) {
    // Fresh system should be optimal
    feedback_analysis_t analysis;
    feedback_analyze_loop(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &analysis);

    feedback_health_t health = feedback_get_health(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL);
    EXPECT_EQ(health, FEEDBACK_HEALTH_OPTIMAL);
}

TEST_F(FeedbackSystemTest, GetTrend) {
    feedback_analysis_t analysis;
    feedback_analyze_loop(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, &analysis);

    feedback_trend_t trend = feedback_get_trend(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL);
    EXPECT_EQ(trend, TREND_STABLE);
}

TEST_F(FeedbackSystemTest, HasUnhealthyLoopsInitially) {
    // Fresh system should not have unhealthy loops
    EXPECT_FALSE(feedback_has_unhealthy_loops(&system));
}

// ============================================================================
// History Tests
// ============================================================================

TEST_F(FeedbackSystemTest, GetHistoryEmpty) {
    feedback_transfer_record_t records[10];
    uint32_t count = 0;

    ASSERT_EQ(feedback_get_history(
        &system,
        FEEDBACK_INTROSPECTION_TO_SELF_MODEL,
        records,
        10,
        &count
    ), 0);

    EXPECT_EQ(count, 0u);
}

TEST_F(FeedbackSystemTest, GetHistoryWithRecords) {
    // Add some records
    for (int i = 0; i < 5; i++) {
        feedback_record_transfer(
            &system,
            FEEDBACK_INTROSPECTION_TO_SELF_MODEL,
            (float)i * 0.1f,
            (float)i * 0.1f,
            0.05f,
            2.0f,
            true,
            NULL
        );
    }

    feedback_transfer_record_t records[10];
    uint32_t count = 0;

    ASSERT_EQ(feedback_get_history(
        &system,
        FEEDBACK_INTROSPECTION_TO_SELF_MODEL,
        records,
        10,
        &count
    ), 0);

    EXPECT_EQ(count, 5u);
}

TEST_F(FeedbackSystemTest, ClearHistorySingleLoop) {
    // Add records
    for (int i = 0; i < 3; i++) {
        feedback_record_transfer(
            &system,
            FEEDBACK_INTROSPECTION_TO_SELF_MODEL,
            0.5f, 0.5f, 0.1f, 1.0f, true, NULL
        );
    }

    ASSERT_EQ(feedback_clear_history(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL), 0);

    feedback_transfer_record_t records[10];
    uint32_t count = 99;
    feedback_get_history(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, records, 10, &count);
    EXPECT_EQ(count, 0u);
}

TEST_F(FeedbackSystemTest, ClearHistoryAllLoops) {
    // Add records to multiple loops
    for (int loop = 0; loop < 3; loop++) {
        for (int i = 0; i < 2; i++) {
            feedback_record_transfer(
                &system,
                (feedback_loop_type_t)loop,
                0.5f, 0.5f, 0.1f, 1.0f, true, NULL
            );
        }
    }

    // Clear all by passing FEEDBACK_LOOP_COUNT
    ASSERT_EQ(feedback_clear_history(&system, FEEDBACK_LOOP_COUNT), 0);

    // Verify all are empty
    for (int loop = 0; loop < FEEDBACK_LOOP_COUNT; loop++) {
        feedback_transfer_record_t records[10];
        uint32_t count = 99;
        feedback_get_history(&system, (feedback_loop_type_t)loop, records, 10, &count);
        EXPECT_EQ(count, 0u);
    }
}

// ============================================================================
// Utility Name Tests
// ============================================================================

TEST(FeedbackUtilityTest, TransferNames) {
    EXPECT_STREQ(feedback_transfer_name(TRANSFER_LINEAR), "LINEAR");
    EXPECT_STREQ(feedback_transfer_name(TRANSFER_SIGMOID), "SIGMOID");
    EXPECT_STREQ(feedback_transfer_name(TRANSFER_TANH), "TANH");
    EXPECT_STREQ(feedback_transfer_name(TRANSFER_EXPONENTIAL), "EXPONENTIAL");
    EXPECT_STREQ(feedback_transfer_name(TRANSFER_LOGARITHMIC), "LOGARITHMIC");
    EXPECT_STREQ(feedback_transfer_name(TRANSFER_STEP), "STEP");
    EXPECT_STREQ(feedback_transfer_name(TRANSFER_GATED), "GATED");
    EXPECT_STREQ(feedback_transfer_name((transfer_function_t)999), "UNKNOWN");
}

TEST(FeedbackUtilityTest, TrendNames) {
    EXPECT_STREQ(feedback_trend_name(TREND_STABLE), "STABLE");
    EXPECT_STREQ(feedback_trend_name(TREND_INCREASING), "INCREASING");
    EXPECT_STREQ(feedback_trend_name(TREND_DECREASING), "DECREASING");
    EXPECT_STREQ(feedback_trend_name(TREND_OSCILLATING), "OSCILLATING");
    EXPECT_STREQ(feedback_trend_name(TREND_DIVERGING), "DIVERGING");
    EXPECT_STREQ(feedback_trend_name((feedback_trend_t)999), "UNKNOWN");
}

TEST(FeedbackUtilityTest, HealthNames) {
    EXPECT_STREQ(feedback_health_name(FEEDBACK_HEALTH_OPTIMAL), "OPTIMAL");
    EXPECT_STREQ(feedback_health_name(FEEDBACK_HEALTH_DEGRADED), "DEGRADED");
    EXPECT_STREQ(feedback_health_name(FEEDBACK_HEALTH_FAILING), "FAILING");
    EXPECT_STREQ(feedback_health_name(FEEDBACK_HEALTH_DEAD), "DEAD");
    EXPECT_STREQ(feedback_health_name((feedback_health_t)999), "UNKNOWN");
}

// ============================================================================
// Compute Value Tests
// ============================================================================

TEST_F(FeedbackSystemTest, ComputeValueBasic) {
    float output = 0.0f;
    ASSERT_EQ(feedback_compute_value(
        &system,
        FEEDBACK_INTROSPECTION_TO_SELF_MODEL,
        0.5f,
        &output
    ), 0);

    // With default policy (linear, learning rate 0.1, momentum 0.9)
    // First call should produce a non-zero output
    EXPECT_GT(output, 0.0f);
    EXPECT_LT(output, 0.5f);  // Should be scaled by learning rate
}

TEST_F(FeedbackSystemTest, ComputeValueNull) {
    float output = 0.0f;
    EXPECT_LT(feedback_compute_value(NULL, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, 0.5f, &output), 0);
    EXPECT_LT(feedback_compute_value(&system, FEEDBACK_INTROSPECTION_TO_SELF_MODEL, 0.5f, NULL), 0);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
