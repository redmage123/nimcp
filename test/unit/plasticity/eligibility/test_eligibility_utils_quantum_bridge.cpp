//=============================================================================
// test_eligibility_utils_quantum_bridge.cpp - Unit Tests for Bidirectional Bridge
//=============================================================================
/**
 * @file test_eligibility_utils_quantum_bridge.cpp
 * @brief Comprehensive tests for eligibility Utils-Quantum bidirectional bridge
 *
 * Tests: Configuration, lifecycle, forward triggers, backward feedback,
 *        feedback loop, coherence tracking, state/statistics
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

extern "C" {
#include "plasticity/eligibility/nimcp_eligibility_utils_quantum_bridge.h"
}

//=============================================================================
// Configuration Tests
//=============================================================================

class EligUQConfigTest : public ::testing::Test {};

TEST_F(EligUQConfigTest, DefaultConfigValues) {
    elig_uq_bridge_config_t config = elig_uq_bridge_default_config();

    // Forward triggers enabled
    EXPECT_TRUE(config.enable_metric_triggers);
    EXPECT_FLOAT_EQ(config.ltp_ltd_ratio_min, ELIG_UQ_LTP_LTD_RATIO_MIN);
    EXPECT_FLOAT_EQ(config.ltp_ltd_ratio_max, ELIG_UQ_LTP_LTD_RATIO_MAX);
    EXPECT_FLOAT_EQ(config.pool_exhaustion_threshold, ELIG_UQ_POOL_EXHAUSTION_THRESHOLD);
    EXPECT_FLOAT_EQ(config.bottleneck_escalation_threshold, ELIG_UQ_BOTTLENECK_ESCALATION);

    // Backward feedback enabled
    EXPECT_TRUE(config.enable_credit_feedback);
    EXPECT_TRUE(config.enable_param_feedback);
    EXPECT_TRUE(config.enable_diffusion_feedback);
    EXPECT_TRUE(config.enable_step_feedback);
    EXPECT_FLOAT_EQ(config.param_integration_rate, ELIG_UQ_PARAM_INTEGRATION_RATE);

    // Feedback loop enabled
    EXPECT_TRUE(config.enable_auto_feedback_loop);
    EXPECT_EQ(config.feedback_loop_interval_ms, 100u);
}

TEST_F(EligUQConfigTest, ValidateValidConfig) {
    elig_uq_bridge_config_t config = elig_uq_bridge_default_config();
    EXPECT_TRUE(elig_uq_bridge_validate_config(&config));
}

TEST_F(EligUQConfigTest, ValidateNullConfig) {
    EXPECT_FALSE(elig_uq_bridge_validate_config(nullptr));
}

TEST_F(EligUQConfigTest, ValidateInvalidRatioBounds) {
    elig_uq_bridge_config_t config = elig_uq_bridge_default_config();

    // Invalid: min > max
    config.ltp_ltd_ratio_min = 5.0f;
    config.ltp_ltd_ratio_max = 3.0f;
    EXPECT_FALSE(elig_uq_bridge_validate_config(&config));
}

TEST_F(EligUQConfigTest, ValidateInvalidPoolThreshold) {
    elig_uq_bridge_config_t config = elig_uq_bridge_default_config();

    config.pool_exhaustion_threshold = 1.5f;  // > 1.0
    EXPECT_FALSE(elig_uq_bridge_validate_config(&config));

    config.pool_exhaustion_threshold = -0.1f;  // < 0.0
    EXPECT_FALSE(elig_uq_bridge_validate_config(&config));
}

TEST_F(EligUQConfigTest, ValidateInvalidIntegrationRate) {
    elig_uq_bridge_config_t config = elig_uq_bridge_default_config();

    config.param_integration_rate = 1.5f;  // > 1.0
    EXPECT_FALSE(elig_uq_bridge_validate_config(&config));
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

class EligUQLifecycleTest : public ::testing::Test {
protected:
    elig_uq_bridge_t bridge = nullptr;

    void TearDown() override {
        if (bridge) {
            elig_uq_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(EligUQLifecycleTest, CreateWithDefaultConfig) {
    bridge = elig_uq_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(EligUQLifecycleTest, CreateWithCustomConfig) {
    elig_uq_bridge_config_t config = elig_uq_bridge_default_config();
    config.enable_auto_feedback_loop = false;
    config.param_integration_rate = 0.2f;

    bridge = elig_uq_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(EligUQLifecycleTest, CreateWithInvalidConfig) {
    elig_uq_bridge_config_t config = elig_uq_bridge_default_config();
    config.ltp_ltd_ratio_min = 10.0f;  // Invalid
    config.ltp_ltd_ratio_max = 5.0f;

    bridge = elig_uq_bridge_create(&config);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(EligUQLifecycleTest, DestroyNull) {
    elig_uq_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(EligUQLifecycleTest, IsConnectedInitially) {
    bridge = elig_uq_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Not connected initially
    EXPECT_FALSE(elig_uq_bridge_is_connected(bridge));
}

TEST_F(EligUQLifecycleTest, AttachContexts) {
    bridge = elig_uq_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Create mock contexts
    eligibility_utils_ctx_t utils_ctx = eligibility_utils_create(nullptr);
    eligibility_quantum_ctx_t quantum_ctx = elig_quantum_create(nullptr);

    ASSERT_NE(utils_ctx, nullptr);
    ASSERT_NE(quantum_ctx, nullptr);

    // Attach contexts
    EXPECT_EQ(elig_uq_bridge_attach_utils(bridge, utils_ctx), 0);
    EXPECT_EQ(elig_uq_bridge_attach_quantum(bridge, quantum_ctx), 0);

    // Now connected
    EXPECT_TRUE(elig_uq_bridge_is_connected(bridge));

    // Cleanup
    eligibility_utils_destroy(utils_ctx);
    elig_quantum_destroy(quantum_ctx);
}

TEST_F(EligUQLifecycleTest, AttachNullContext) {
    bridge = elig_uq_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(elig_uq_bridge_attach_utils(bridge, nullptr), -1);
    EXPECT_EQ(elig_uq_bridge_attach_quantum(bridge, nullptr), -1);
}

TEST_F(EligUQLifecycleTest, Reset) {
    bridge = elig_uq_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    elig_uq_bridge_reset(bridge);  // Should not crash

    // Coherence should be reset to 1.0
    EXPECT_FLOAT_EQ(elig_uq_get_coherence(bridge), 1.0f);
}

//=============================================================================
// Forward Direction Tests (Utils -> Quantum)
//=============================================================================

class EligUQForwardTest : public ::testing::Test {
protected:
    elig_uq_bridge_t bridge = nullptr;

    void SetUp() override {
        bridge = elig_uq_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) {
            elig_uq_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(EligUQForwardTest, EvaluateMetricsNoContext) {
    // Should work without attached contexts
    elig_uq_forward_effect_t effect;
    int result = elig_uq_evaluate_metrics(bridge, &effect);
    EXPECT_EQ(result, 0);

    // Timestamp should be set
    EXPECT_GT(effect.timestamp_ms, 0u);
}

TEST_F(EligUQForwardTest, EvaluateMetricsNullEffect) {
    int result = elig_uq_evaluate_metrics(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(EligUQForwardTest, NotifyLtpLtdImbalanceLow) {
    elig_uq_forward_effect_t effect;

    // LTP/LTD ratio = 0.1 (below min of 0.3)
    int result = elig_uq_notify_ltp_ltd_imbalance(bridge, 1, 10, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.trigger_type, ELIG_UQ_TRIGGER_LTP_LTD_IMBALANCE);
    EXPECT_TRUE(effect.request_annealing);
    EXPECT_NEAR(effect.ltp_ltd_ratio, 0.1f, 0.01f);
}

TEST_F(EligUQForwardTest, NotifyLtpLtdImbalanceHigh) {
    elig_uq_forward_effect_t effect;

    // LTP/LTD ratio = 5.0 (above max of 3.0)
    int result = elig_uq_notify_ltp_ltd_imbalance(bridge, 50, 10, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.trigger_type, ELIG_UQ_TRIGGER_LTP_LTD_IMBALANCE);
    EXPECT_TRUE(effect.request_annealing);
    EXPECT_NEAR(effect.ltp_ltd_ratio, 5.0f, 0.01f);
}

TEST_F(EligUQForwardTest, NotifyLtpLtdBalanced) {
    elig_uq_forward_effect_t effect;

    // LTP/LTD ratio = 1.0 (within range)
    int result = elig_uq_notify_ltp_ltd_imbalance(bridge, 10, 10, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.trigger_type, ELIG_UQ_TRIGGER_NONE);
    EXPECT_FALSE(effect.request_annealing);
}

TEST_F(EligUQForwardTest, NotifyPoolPressureHigh) {
    elig_uq_forward_effect_t effect;

    // 95% utilization (above 90% threshold)
    int result = elig_uq_notify_pool_pressure(bridge, 0.95f, 50, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.trigger_type, ELIG_UQ_TRIGGER_POOL_PRESSURE);
    EXPECT_TRUE(effect.request_quantum_walk);
    EXPECT_FLOAT_EQ(effect.pool_utilization, 0.95f);
}

TEST_F(EligUQForwardTest, NotifyPoolPressureNormal) {
    elig_uq_forward_effect_t effect;

    // 50% utilization (below threshold)
    int result = elig_uq_notify_pool_pressure(bridge, 0.50f, 500, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.trigger_type, ELIG_UQ_TRIGGER_NONE);
    EXPECT_FALSE(effect.request_quantum_walk);
}

TEST_F(EligUQForwardTest, EscalateBottleneckHigh) {
    std::vector<eligibility_bottleneck_t> bottlenecks(3);
    for (int i = 0; i < 3; i++) {
        bottlenecks[i].synapse_id = i;
        bottlenecks[i].information_deficit = 0.8f;  // Above 0.6 threshold
        bottlenecks[i].current_trace = 0.5f;
    }

    elig_uq_forward_effect_t effect;
    int result = elig_uq_escalate_bottleneck(bridge, bottlenecks.data(), 3, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.trigger_type, ELIG_UQ_TRIGGER_BOTTLENECK);
    EXPECT_TRUE(effect.request_quantum_shannon);
    EXPECT_EQ(effect.bottleneck_count, 3u);
}

TEST_F(EligUQForwardTest, EscalateBottleneckLow) {
    std::vector<eligibility_bottleneck_t> bottlenecks(3);
    for (int i = 0; i < 3; i++) {
        bottlenecks[i].synapse_id = i;
        bottlenecks[i].information_deficit = 0.3f;  // Below 0.6 threshold
        bottlenecks[i].current_trace = 0.5f;
    }

    elig_uq_forward_effect_t effect;
    int result = elig_uq_escalate_bottleneck(bridge, bottlenecks.data(), 3, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.trigger_type, ELIG_UQ_TRIGGER_NONE);
    EXPECT_FALSE(effect.request_quantum_shannon);
}

TEST_F(EligUQForwardTest, ProvideHistorySufficient) {
    std::vector<eligibility_trace_t> traces(150);
    for (size_t i = 0; i < traces.size(); i++) {
        traces[i].trace = 0.5f + 0.1f * sinf((float)i * 0.1f);
        traces[i].last_update = i;
    }

    elig_uq_forward_effect_t effect;
    int result = elig_uq_provide_history(bridge, traces.data(), 150, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.trigger_type, ELIG_UQ_TRIGGER_HISTORY_READY);
    EXPECT_TRUE(effect.request_quantum_walk);
    EXPECT_EQ(effect.history_samples, 150u);
    EXPECT_GT(effect.history_variance, 0.0f);
}

TEST_F(EligUQForwardTest, ProvideHistoryInsufficient) {
    std::vector<eligibility_trace_t> traces(50);  // Below 100 threshold
    for (size_t i = 0; i < traces.size(); i++) {
        traces[i].trace = 0.5f;
        traces[i].last_update = i;
    }

    elig_uq_forward_effect_t effect;
    int result = elig_uq_provide_history(bridge, traces.data(), 50, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.trigger_type, ELIG_UQ_TRIGGER_NONE);
    EXPECT_FALSE(effect.request_quantum_walk);
}

TEST_F(EligUQForwardTest, RequestOptimization) {
    elig_quantum_params_t params = {
        .tau_fast = 10.0f,
        .tau_slow = 100.0f,
        .learning_rate = 0.001f,
        .dopamine_sensitivity = 1.0f,
        .burst_threshold = 0.5f,
        .consolidation_threshold = 0.3f,
        .energy = 5.0f,
        .amplitude = 1.0f
    };

    elig_uq_forward_effect_t effect;
    int result = elig_uq_request_optimization(bridge, &params, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.trigger_type, ELIG_UQ_TRIGGER_METRIC_ANOMALY);
    EXPECT_TRUE(effect.request_annealing);
}

//=============================================================================
// Backward Direction Tests (Quantum -> Utils)
//=============================================================================

class EligUQBackwardTest : public ::testing::Test {
protected:
    elig_uq_bridge_t bridge = nullptr;

    void SetUp() override {
        bridge = elig_uq_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) {
            elig_uq_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(EligUQBackwardTest, ApplyCreditFeedback) {
    std::vector<elig_quantum_credit_t> credits(5);
    for (int i = 0; i < 5; i++) {
        credits[i].synapse_id = i;
        credits[i].credit_fraction = 0.2f;  // Equal distribution
        credits[i].confidence = 0.8f;
        credits[i].temporal_weight = 1.0f;
        credits[i].causal_strength = 0.9f;
    }

    elig_uq_backward_effect_t effect;
    int result = elig_uq_apply_credit_feedback(bridge, credits.data(), 5, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.feedback_type, ELIG_UQ_FEEDBACK_CREDIT_ASSIGNMENT);
    EXPECT_NEAR(effect.avg_credit_fraction, 0.2f, 0.01f);
    EXPECT_NEAR(effect.credit_confidence, 0.8f, 0.01f);
    EXPECT_GT(effect.credit_entropy, 0.0f);  // Non-zero entropy for non-uniform
}

TEST_F(EligUQBackwardTest, ApplyCreditFeedbackNull) {
    elig_uq_backward_effect_t effect;
    int result = elig_uq_apply_credit_feedback(bridge, nullptr, 0, &effect);
    EXPECT_EQ(result, 0);  // Should handle gracefully
}

TEST_F(EligUQBackwardTest, ApplyParamFeedback) {
    elig_quantum_params_t optimized = {
        .tau_fast = 8.0f,
        .tau_slow = 80.0f,
        .learning_rate = 0.0008f,
        .dopamine_sensitivity = 1.2f,
        .burst_threshold = 0.4f,
        .consolidation_threshold = 0.25f,
        .energy = 2.5f,
        .amplitude = 1.1f
    };

    elig_uq_backward_effect_t effect;
    int result = elig_uq_apply_param_feedback(bridge, &optimized, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.feedback_type, ELIG_UQ_FEEDBACK_PARAM_OPTIMIZATION);
    EXPECT_FLOAT_EQ(effect.optimized_tau_fast, 8.0f);
    EXPECT_FLOAT_EQ(effect.optimized_learning_rate, 0.0008f);
    EXPECT_GT(effect.recommended_dt, 0.0f);
    EXPECT_GT(effect.recommended_tolerance, 0.0f);
}

TEST_F(EligUQBackwardTest, ApplyParamFeedbackImproveCoherence) {
    // First feedback with high energy
    elig_quantum_params_t params1 = {
        .tau_fast = 10.0f, .tau_slow = 100.0f, .learning_rate = 0.001f,
        .dopamine_sensitivity = 1.0f, .burst_threshold = 0.5f,
        .consolidation_threshold = 0.3f, .energy = 10.0f, .amplitude = 1.0f
    };

    elig_uq_backward_effect_t effect;
    elig_uq_apply_param_feedback(bridge, &params1, &effect);
    float coherence_after_first = elig_uq_get_coherence(bridge);

    // Second feedback with lower energy (improvement)
    elig_quantum_params_t params2 = params1;
    params2.energy = 5.0f;  // Better optimization

    elig_uq_apply_param_feedback(bridge, &params2, &effect);
    float coherence_after_second = elig_uq_get_coherence(bridge);

    // Coherence should increase after improvement
    EXPECT_GE(coherence_after_second, coherence_after_first);
}

TEST_F(EligUQBackwardTest, ApplyDiffusionFeedback) {
    std::vector<float> diffused(100);
    for (size_t i = 0; i < diffused.size(); i++) {
        diffused[i] = 0.01f + 0.05f * expf(-0.05f * (float)i);
    }

    elig_uq_backward_effect_t effect;
    int result = elig_uq_apply_diffusion_feedback(bridge, diffused.data(), 100, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.feedback_type, ELIG_UQ_FEEDBACK_DIFFUSION_RESULT);
    EXPECT_GT(effect.max_diffused_priority, 0.0f);
    EXPECT_GT(effect.mean_diffused_priority, 0.0f);
    EXPECT_NEAR(effect.diffusion_speedup, 10.0f, 0.5f);  // sqrt(100) = 10
}

TEST_F(EligUQBackwardTest, ApplyStepFeedback) {
    elig_quantum_anneal_state_t anneal = {
        .temperature = 5.0f,
        .tunneling_probability = 0.05f,
        .iteration = 100,
        .tunneling_events = 5,
        .best_energy = 2.0f
    };

    elig_uq_backward_effect_t effect;
    int result = elig_uq_apply_step_feedback(bridge, &anneal, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.feedback_type, ELIG_UQ_FEEDBACK_STEP_SIZE);
    EXPECT_GT(effect.recommended_dt, 0.0f);
    EXPECT_GT(effect.recommended_tolerance, 0.0f);
}

TEST_F(EligUQBackwardTest, GetIntegrationParams) {
    // Apply some param feedback first
    elig_quantum_params_t optimized = {
        .tau_fast = 5.0f, .tau_slow = 50.0f, .learning_rate = 0.001f,
        .dopamine_sensitivity = 1.0f, .burst_threshold = 0.5f,
        .consolidation_threshold = 0.3f, .energy = 3.0f, .amplitude = 1.0f
    };

    elig_uq_backward_effect_t effect;
    elig_uq_apply_param_feedback(bridge, &optimized, &effect);

    float dt, tolerance;
    int result = elig_uq_get_integration_params(bridge, &dt, &tolerance);
    EXPECT_EQ(result, 0);

    EXPECT_GT(dt, 0.0f);
    EXPECT_GT(tolerance, 0.0f);
}

//=============================================================================
// Feedback Loop Tests
//=============================================================================

class EligUQFeedbackLoopTest : public ::testing::Test {
protected:
    elig_uq_bridge_t bridge = nullptr;
    eligibility_utils_ctx_t utils_ctx = nullptr;
    eligibility_quantum_ctx_t quantum_ctx = nullptr;
    std::vector<eligibility_trace_t> traces;
    std::vector<float> weights;

    void SetUp() override {
        bridge = elig_uq_bridge_create(nullptr);
        utils_ctx = eligibility_utils_create(nullptr);
        quantum_ctx = elig_quantum_create(nullptr);

        elig_uq_bridge_attach_utils(bridge, utils_ctx);
        elig_uq_bridge_attach_quantum(bridge, quantum_ctx);

        // Initialize test data
        traces.resize(100);
        weights.resize(100);
        for (size_t i = 0; i < traces.size(); i++) {
            traces[i].trace = 0.3f + 0.4f * (float)i / traces.size();
            traces[i].last_update = 0;
            weights[i] = 0.5f;
        }
    }

    void TearDown() override {
        if (bridge) elig_uq_bridge_destroy(bridge);
        if (utils_ctx) eligibility_utils_destroy(utils_ctx);
        if (quantum_ctx) elig_quantum_destroy(quantum_ctx);
    }
};

TEST_F(EligUQFeedbackLoopTest, SingleTick) {
    elig_uq_forward_effect_t fwd;
    elig_uq_backward_effect_t bwd;

    int result = elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                            traces.size(), &fwd, &bwd);
    EXPECT_EQ(result, 0);

    // Forward effect should have timestamp
    EXPECT_GT(fwd.timestamp_ms, 0u);
}

TEST_F(EligUQFeedbackLoopTest, MultipleTicks) {
    elig_uq_forward_effect_t fwd;
    elig_uq_backward_effect_t bwd;

    for (int i = 0; i < 10; i++) {
        int result = elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                                traces.size(), &fwd, &bwd);
        EXPECT_EQ(result, 0);
    }

    // Check statistics
    elig_uq_bridge_stats_t stats;
    elig_uq_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.feedback_loop_iterations, 10u);
}

TEST_F(EligUQFeedbackLoopTest, CoherenceDecay) {
    float initial_coherence = elig_uq_get_coherence(bridge);
    EXPECT_FLOAT_EQ(initial_coherence, 1.0f);

    // Run multiple ticks without improvements
    for (int i = 0; i < 50; i++) {
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   traces.size(), nullptr, nullptr);
    }

    float final_coherence = elig_uq_get_coherence(bridge);
    EXPECT_LT(final_coherence, initial_coherence);
    EXPECT_GE(final_coherence, ELIG_UQ_MIN_COHERENCE);
}

TEST_F(EligUQFeedbackLoopTest, StabilityComputation) {
    // Run enough ticks to fill stability window
    for (int i = 0; i < ELIG_UQ_STABILITY_WINDOW + 10; i++) {
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   traces.size(), nullptr, nullptr);
    }

    float stability = elig_uq_get_stability(bridge);
    EXPECT_GE(stability, 0.0f);
    EXPECT_LE(stability, 1.0f);
}

TEST_F(EligUQFeedbackLoopTest, AutoFeedbackToggle) {
    EXPECT_TRUE(elig_uq_is_auto_feedback_enabled(bridge));

    elig_uq_set_auto_feedback(bridge, false);
    EXPECT_FALSE(elig_uq_is_auto_feedback_enabled(bridge));

    elig_uq_set_auto_feedback(bridge, true);
    EXPECT_TRUE(elig_uq_is_auto_feedback_enabled(bridge));
}

TEST_F(EligUQFeedbackLoopTest, NullEffects) {
    // Should work with null output effects
    int result = elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                            traces.size(), nullptr, nullptr);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// State and Statistics Tests
//=============================================================================

class EligUQStateStatsTest : public ::testing::Test {
protected:
    elig_uq_bridge_t bridge = nullptr;

    void SetUp() override {
        bridge = elig_uq_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) {
            elig_uq_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(EligUQStateStatsTest, GetInitialState) {
    elig_uq_bridge_state_t state;
    int result = elig_uq_bridge_get_state(bridge, &state);
    EXPECT_EQ(result, 0);

    EXPECT_FLOAT_EQ(state.utils_quantum_coherence, 1.0f);
    EXPECT_FLOAT_EQ(state.stability_metric, 1.0f);
    EXPECT_GT(state.current_optimized_dt, 0.0f);
}

TEST_F(EligUQStateStatsTest, GetInitialStats) {
    elig_uq_bridge_stats_t stats;
    int result = elig_uq_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(stats.total_forward_events, 0u);
    EXPECT_EQ(stats.total_backward_events, 0u);
    EXPECT_EQ(stats.feedback_loop_iterations, 0u);
}

TEST_F(EligUQStateStatsTest, StatsAccumulate) {
    // Generate some events
    elig_uq_forward_effect_t fwd;
    elig_uq_notify_ltp_ltd_imbalance(bridge, 1, 10, &fwd);  // Triggers
    elig_uq_notify_pool_pressure(bridge, 0.95f, 50, &fwd);

    elig_uq_backward_effect_t bwd;
    elig_quantum_params_t params = {
        .tau_fast = 10.0f, .tau_slow = 100.0f, .learning_rate = 0.001f,
        .dopamine_sensitivity = 1.0f, .burst_threshold = 0.5f,
        .consolidation_threshold = 0.3f, .energy = 5.0f, .amplitude = 1.0f
    };
    elig_uq_apply_param_feedback(bridge, &params, &bwd);

    elig_uq_bridge_stats_t stats;
    elig_uq_bridge_get_stats(bridge, &stats);

    EXPECT_GT(stats.total_forward_events, 0u);
    EXPECT_GT(stats.total_backward_events, 0u);
    EXPECT_GT(stats.ltp_ltd_triggers, 0u);
    EXPECT_GT(stats.pool_triggers, 0u);
    EXPECT_GT(stats.param_feedbacks, 0u);
}

TEST_F(EligUQStateStatsTest, ResetStats) {
    // Generate events
    elig_uq_forward_effect_t fwd;
    elig_uq_notify_ltp_ltd_imbalance(bridge, 1, 10, &fwd);

    // Reset
    int result = elig_uq_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    // Stats should be cleared
    elig_uq_bridge_stats_t stats;
    elig_uq_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_forward_events, 0u);
    EXPECT_EQ(stats.ltp_ltd_triggers, 0u);
}

TEST_F(EligUQStateStatsTest, UpdateDecaysCoherence) {
    float initial = elig_uq_get_coherence(bridge);

    // Simulate 100ms of time
    elig_uq_bridge_update(bridge, 100.0f);

    float after = elig_uq_get_coherence(bridge);
    EXPECT_LT(after, initial);
}

TEST_F(EligUQStateStatsTest, CoherenceStatistics) {
    // Generate some activity to vary coherence
    std::vector<eligibility_trace_t> traces(100);
    std::vector<float> weights(100);
    for (size_t i = 0; i < traces.size(); i++) {
        traces[i].trace = 0.5f;
        traces[i].last_update = 0;
        weights[i] = 0.5f;
    }

    eligibility_utils_ctx_t utils_ctx = eligibility_utils_create(nullptr);
    eligibility_quantum_ctx_t quantum_ctx = elig_quantum_create(nullptr);
    elig_uq_bridge_attach_utils(bridge, utils_ctx);
    elig_uq_bridge_attach_quantum(bridge, quantum_ctx);

    for (int i = 0; i < 20; i++) {
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   traces.size(), nullptr, nullptr);
    }

    elig_uq_bridge_stats_t stats;
    elig_uq_bridge_get_stats(bridge, &stats);

    EXPECT_LE(stats.min_coherence, stats.avg_coherence);
    EXPECT_LE(stats.avg_coherence, stats.max_coherence);

    eligibility_utils_destroy(utils_ctx);
    elig_quantum_destroy(quantum_ctx);
}

//=============================================================================
// Diagnostic Tests
//=============================================================================

class EligUQDiagnosticTest : public ::testing::Test {
protected:
    elig_uq_bridge_t bridge = nullptr;

    void SetUp() override {
        bridge = elig_uq_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) {
            elig_uq_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(EligUQDiagnosticTest, VerifyValidBridge) {
    EXPECT_TRUE(elig_uq_bridge_verify(bridge));
}

TEST_F(EligUQDiagnosticTest, VerifyNullBridge) {
    EXPECT_FALSE(elig_uq_bridge_verify(nullptr));
}

TEST_F(EligUQDiagnosticTest, PrintSummary) {
    // Should not crash
    elig_uq_bridge_print_summary(bridge);
    elig_uq_bridge_print_summary(nullptr);
}

TEST_F(EligUQDiagnosticTest, ExportCSV) {
    const char* filename = "/tmp/elig_uq_bridge_test.csv";

    bool result = elig_uq_bridge_export_csv(bridge, filename);
    EXPECT_TRUE(result);

    // Verify file exists
    FILE* f = fopen(filename, "r");
    EXPECT_NE(f, nullptr);
    if (f) fclose(f);

    // Cleanup
    remove(filename);
}

TEST_F(EligUQDiagnosticTest, ExportCSVNullFilename) {
    EXPECT_FALSE(elig_uq_bridge_export_csv(bridge, nullptr));
}

//=============================================================================
// Integration Tests
//=============================================================================

class EligUQIntegrationTest : public ::testing::Test {
protected:
    elig_uq_bridge_t bridge = nullptr;
    eligibility_utils_ctx_t utils_ctx = nullptr;
    eligibility_quantum_ctx_t quantum_ctx = nullptr;

    void SetUp() override {
        bridge = elig_uq_bridge_create(nullptr);
        utils_ctx = eligibility_utils_create(nullptr);
        quantum_ctx = elig_quantum_create(nullptr);

        elig_uq_bridge_attach_utils(bridge, utils_ctx);
        elig_uq_bridge_attach_quantum(bridge, quantum_ctx);
    }

    void TearDown() override {
        if (bridge) elig_uq_bridge_destroy(bridge);
        if (utils_ctx) eligibility_utils_destroy(utils_ctx);
        if (quantum_ctx) elig_quantum_destroy(quantum_ctx);
    }
};

TEST_F(EligUQIntegrationTest, FullFeedbackCycle) {
    std::vector<eligibility_trace_t> traces(50);
    std::vector<float> weights(50);
    for (size_t i = 0; i < traces.size(); i++) {
        traces[i].trace = 0.2f + 0.6f * (float)i / traces.size();
        traces[i].last_update = 0;
        weights[i] = 0.5f;
    }

    // Run full feedback cycle
    elig_uq_forward_effect_t fwd;
    elig_uq_backward_effect_t bwd;

    int result = elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                            traces.size(), &fwd, &bwd);
    EXPECT_EQ(result, 0);

    // Verify state was updated
    elig_uq_bridge_state_t state;
    elig_uq_bridge_get_state(bridge, &state);
    EXPECT_GT(state.last_feedback_loop_ms, 0u);
}

TEST_F(EligUQIntegrationTest, ConvergenceTest) {
    std::vector<eligibility_trace_t> traces(100);
    std::vector<float> weights(100);
    for (size_t i = 0; i < traces.size(); i++) {
        traces[i].trace = 0.5f;
        traces[i].last_update = 0;
        weights[i] = 0.5f;
    }

    // Run many iterations
    for (int i = 0; i < 100; i++) {
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   traces.size(), nullptr, nullptr);
    }

    // System should have some stability (not zero)
    float stability = elig_uq_get_stability(bridge);
    EXPECT_GT(stability, 0.0f);  // Should have non-zero stability

    // Coherence should not have dropped too low
    float coherence = elig_uq_get_coherence(bridge);
    EXPECT_GE(coherence, ELIG_UQ_MIN_COHERENCE);
}

TEST_F(EligUQIntegrationTest, RoundTripLatency) {
    std::vector<eligibility_trace_t> traces(50);
    std::vector<float> weights(50);
    for (size_t i = 0; i < traces.size(); i++) {
        traces[i].trace = 0.5f;
        traces[i].last_update = 0;
        weights[i] = 0.5f;
    }

    // Run several iterations
    for (int i = 0; i < 20; i++) {
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   traces.size(), nullptr, nullptr);
    }

    // Check latency statistics
    elig_uq_bridge_stats_t stats;
    elig_uq_bridge_get_stats(bridge, &stats);

    EXPECT_GT(stats.avg_round_trip_us, 0.0);
    EXPECT_LT(stats.avg_round_trip_us, 10000.0);  // Should be under 10ms
}
