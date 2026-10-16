/**
 * @file test_eligibility_utils_quantum_regression.cpp
 * @brief Regression tests for Eligibility Utils-Quantum Bidirectional Bridge
 *
 * WHAT: Validates stability, edge cases, and potential regressions
 * WHY:  Ensure bridge behavior doesn't regress under stress/edge conditions
 * HOW:  Test edge cases, high load, rapid cycles, memory safety
 *
 * @version 1.0.0
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>

extern "C" {
#include "plasticity/eligibility/nimcp_eligibility_utils_quantum_bridge.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
}

//=============================================================================
// Regression Test Fixture
//=============================================================================

class EligibilityUtilsQuantumRegressionTest : public ::testing::Test {
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

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(EligibilityUtilsQuantumRegressionTest, EmptyTraceArray) {
    // Empty array should not crash
    elig_uq_forward_effect_t fwd;
    elig_uq_backward_effect_t bwd;

    int result = elig_uq_feedback_loop_tick(bridge, nullptr, nullptr, 0, &fwd, &bwd);
    // Should handle gracefully
    EXPECT_EQ(result, 0);
}

TEST_F(EligibilityUtilsQuantumRegressionTest, SingleSynapse) {
    // Single synapse edge case
    eligibility_trace_t trace;
    eligibility_trace_init(&trace, 0);
    trace.trace = 0.5f;
    float weight = 0.5f;

    elig_uq_forward_effect_t fwd;
    elig_uq_backward_effect_t bwd;

    int result = elig_uq_feedback_loop_tick(bridge, &trace, &weight, 1, &fwd, &bwd);
    EXPECT_EQ(result, 0);

    // Should not crash on diffusion with single synapse
    float diffused = 0.0f;
    elig_uq_apply_diffusion_feedback(bridge, &diffused, 1, &bwd);
}

TEST_F(EligibilityUtilsQuantumRegressionTest, ZeroTraces) {
    // All traces are zero
    std::vector<eligibility_trace_t> traces(100);
    std::vector<float> weights(100);
    for (size_t i = 0; i < traces.size(); i++) {
        eligibility_trace_init(&traces[i], 0);
        traces[i].trace = 0.0f;
        weights[i] = 0.5f;
    }

    elig_uq_forward_effect_t fwd;
    elig_uq_backward_effect_t bwd;

    int result = elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                            traces.size(), &fwd, &bwd);
    EXPECT_EQ(result, 0);

    // Coherence should still be valid
    float coherence = elig_uq_get_coherence(bridge);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(EligibilityUtilsQuantumRegressionTest, MaxTraces) {
    // All traces at maximum
    std::vector<eligibility_trace_t> traces(100);
    std::vector<float> weights(100);
    for (size_t i = 0; i < traces.size(); i++) {
        eligibility_trace_init(&traces[i], 0);
        traces[i].trace = 1.0f;
        weights[i] = 1.0f;
    }

    elig_uq_forward_effect_t fwd;
    elig_uq_backward_effect_t bwd;

    int result = elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                            traces.size(), &fwd, &bwd);
    EXPECT_EQ(result, 0);
}

TEST_F(EligibilityUtilsQuantumRegressionTest, ZeroLTDCount) {
    // Zero LTD events (divide by zero edge case)
    elig_uq_forward_effect_t effect;
    int result = elig_uq_notify_ltp_ltd_imbalance(bridge, 100, 0, &effect);
    EXPECT_EQ(result, 0);

    // Ratio should be high but finite
    EXPECT_GT(effect.ltp_ltd_ratio, 0.0f);
    EXPECT_TRUE(std::isfinite(effect.ltp_ltd_ratio));
}

TEST_F(EligibilityUtilsQuantumRegressionTest, ZeroPoolUtilization) {
    elig_uq_forward_effect_t effect;
    int result = elig_uq_notify_pool_pressure(bridge, 0.0f, 1000, &effect);
    EXPECT_EQ(result, 0);

    // Should not trigger pool pressure
    EXPECT_NE(effect.trigger_type, ELIG_UQ_TRIGGER_POOL_PRESSURE);
}

TEST_F(EligibilityUtilsQuantumRegressionTest, FullPoolUtilization) {
    elig_uq_forward_effect_t effect;
    int result = elig_uq_notify_pool_pressure(bridge, 1.0f, 0, &effect);
    EXPECT_EQ(result, 0);

    // Should trigger pool pressure
    EXPECT_EQ(effect.trigger_type, ELIG_UQ_TRIGGER_POOL_PRESSURE);
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(EligibilityUtilsQuantumRegressionTest, VerySmallEnergy) {
    elig_quantum_params_t params = {
        .tau_fast = 10.0f,
        .tau_slow = 100.0f,
        .learning_rate = 0.001f,
        .dopamine_sensitivity = 1.0f,
        .burst_threshold = 0.5f,
        .consolidation_threshold = 0.3f,
        .energy = 1e-10f,  // Very small
        .amplitude = 1.0f
    };

    elig_uq_backward_effect_t effect;
    int result = elig_uq_apply_param_feedback(bridge, &params, &effect);
    EXPECT_EQ(result, 0);

    // Recommended dt should be finite and positive
    EXPECT_GT(effect.recommended_dt, 0.0f);
    EXPECT_TRUE(std::isfinite(effect.recommended_dt));
}

TEST_F(EligibilityUtilsQuantumRegressionTest, VerySmallTau) {
    elig_quantum_params_t params = {
        .tau_fast = 1e-8f,  // Very small
        .tau_slow = 1e-8f,
        .learning_rate = 0.001f,
        .dopamine_sensitivity = 1.0f,
        .burst_threshold = 0.5f,
        .consolidation_threshold = 0.3f,
        .energy = 5.0f,
        .amplitude = 1.0f
    };

    elig_uq_backward_effect_t effect;
    int result = elig_uq_apply_param_feedback(bridge, &params, &effect);
    EXPECT_EQ(result, 0);

    // Should handle gracefully
    EXPECT_GT(effect.recommended_dt, 0.0f);
    EXPECT_TRUE(std::isfinite(effect.recommended_dt));
}

TEST_F(EligibilityUtilsQuantumRegressionTest, InfiniteEnergy) {
    elig_quantum_params_t params = {
        .tau_fast = 10.0f,
        .tau_slow = 100.0f,
        .learning_rate = 0.001f,
        .dopamine_sensitivity = 1.0f,
        .burst_threshold = 0.5f,
        .consolidation_threshold = 0.3f,
        .energy = INFINITY,
        .amplitude = 1.0f
    };

    elig_uq_backward_effect_t effect;
    int result = elig_uq_apply_param_feedback(bridge, &params, &effect);
    EXPECT_EQ(result, 0);

    // Should handle gracefully
    EXPECT_TRUE(std::isfinite(effect.recommended_dt));
}

TEST_F(EligibilityUtilsQuantumRegressionTest, NaNProtection) {
    // Create traces with potential NaN-inducing values
    std::vector<eligibility_trace_t> traces(10);
    std::vector<float> weights(10);
    for (size_t i = 0; i < traces.size(); i++) {
        eligibility_trace_init(&traces[i], 0);
        traces[i].trace = 0.0f;  // Could cause divide by zero in normalization
        weights[i] = 0.0f;
    }

    elig_uq_forward_effect_t fwd;
    elig_uq_backward_effect_t bwd;

    int result = elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                            traces.size(), &fwd, &bwd);
    EXPECT_EQ(result, 0);

    // All values should be finite
    EXPECT_TRUE(std::isfinite(elig_uq_get_coherence(bridge)));
    EXPECT_TRUE(std::isfinite(elig_uq_get_stability(bridge)));
}

//=============================================================================
// High Load Tests
//=============================================================================

TEST_F(EligibilityUtilsQuantumRegressionTest, HighSynapseCount) {
    const uint32_t num_synapses = 10000;
    std::vector<eligibility_trace_t> traces(num_synapses);
    std::vector<float> weights(num_synapses);

    for (uint32_t i = 0; i < num_synapses; i++) {
        eligibility_trace_init(&traces[i], 0);
        traces[i].trace = 0.5f * (1.0f + sinf((float)i * 0.01f));
        weights[i] = 0.5f;
    }

    elig_uq_forward_effect_t fwd;
    elig_uq_backward_effect_t bwd;

    int result = elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                            num_synapses, &fwd, &bwd);
    EXPECT_EQ(result, 0);

    // Should complete successfully
    EXPECT_TRUE(elig_uq_bridge_verify(bridge));
}

TEST_F(EligibilityUtilsQuantumRegressionTest, RapidFeedbackCycles) {
    std::vector<eligibility_trace_t> traces(100);
    std::vector<float> weights(100);
    for (size_t i = 0; i < traces.size(); i++) {
        eligibility_trace_init(&traces[i], 0);
        traces[i].trace = 0.5f;
        weights[i] = 0.5f;
    }

    // Run 1000 rapid cycles
    for (int i = 0; i < 1000; i++) {
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   traces.size(), nullptr, nullptr);
    }

    // Should still be stable
    EXPECT_TRUE(elig_uq_bridge_verify(bridge));

    float coherence = elig_uq_get_coherence(bridge);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(EligibilityUtilsQuantumRegressionTest, RepeatedResetCycles) {
    std::vector<eligibility_trace_t> traces(50);
    std::vector<float> weights(50);
    for (size_t i = 0; i < traces.size(); i++) {
        eligibility_trace_init(&traces[i], 0);
        traces[i].trace = 0.5f;
        weights[i] = 0.5f;
    }

    for (int cycle = 0; cycle < 100; cycle++) {
        // Run some iterations
        for (int i = 0; i < 10; i++) {
            elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                       traces.size(), nullptr, nullptr);
        }

        // Reset
        elig_uq_bridge_reset(bridge);

        // Verify reset worked
        EXPECT_FLOAT_EQ(elig_uq_get_coherence(bridge), 1.0f);
    }

    EXPECT_TRUE(elig_uq_bridge_verify(bridge));
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(EligibilityUtilsQuantumRegressionTest, CreateDestroyRepeated) {
    for (int i = 0; i < 100; i++) {
        elig_uq_bridge_t b = elig_uq_bridge_create(nullptr);
        ASSERT_NE(b, nullptr);
        elig_uq_bridge_destroy(b);
    }
}

TEST_F(EligibilityUtilsQuantumRegressionTest, AttachDetachRepeated) {
    for (int i = 0; i < 50; i++) {
        eligibility_utils_ctx_t u = eligibility_utils_create(nullptr);
        eligibility_quantum_ctx_t q = elig_quantum_create(nullptr);

        elig_uq_bridge_t b = elig_uq_bridge_create(nullptr);

        elig_uq_bridge_attach_utils(b, u);
        elig_uq_bridge_attach_quantum(b, q);

        EXPECT_TRUE(elig_uq_bridge_is_connected(b));

        elig_uq_bridge_destroy(b);
        eligibility_utils_destroy(u);
        elig_quantum_destroy(q);
    }
}

TEST_F(EligibilityUtilsQuantumRegressionTest, OperationsAfterReset) {
    std::vector<eligibility_trace_t> traces(50);
    std::vector<float> weights(50);
    for (size_t i = 0; i < traces.size(); i++) {
        eligibility_trace_init(&traces[i], 0);
        traces[i].trace = 0.5f;
        weights[i] = 0.5f;
    }

    // Run operations
    for (int i = 0; i < 10; i++) {
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   traces.size(), nullptr, nullptr);
    }

    // Reset
    elig_uq_bridge_reset(bridge);

    // Operations should still work
    elig_uq_forward_effect_t fwd;
    elig_uq_backward_effect_t bwd;
    int result = elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                            traces.size(), &fwd, &bwd);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// State Consistency Tests
//=============================================================================

TEST_F(EligibilityUtilsQuantumRegressionTest, CoherenceBounds) {
    std::vector<eligibility_trace_t> traces(100);
    std::vector<float> weights(100);
    for (size_t i = 0; i < traces.size(); i++) {
        eligibility_trace_init(&traces[i], 0);
        traces[i].trace = 0.5f;
        weights[i] = 0.5f;
    }

    // Run many iterations checking coherence bounds
    for (int i = 0; i < 500; i++) {
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   traces.size(), nullptr, nullptr);

        float coherence = elig_uq_get_coherence(bridge);
        EXPECT_GE(coherence, 0.0f) << "Iteration " << i;
        EXPECT_LE(coherence, 1.0f) << "Iteration " << i;
    }
}

TEST_F(EligibilityUtilsQuantumRegressionTest, StabilityBounds) {
    std::vector<eligibility_trace_t> traces(100);
    std::vector<float> weights(100);
    for (size_t i = 0; i < traces.size(); i++) {
        eligibility_trace_init(&traces[i], 0);
        traces[i].trace = 0.5f;
        weights[i] = 0.5f;
    }

    // Fill stability window
    for (int i = 0; i < ELIG_UQ_STABILITY_WINDOW + 50; i++) {
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   traces.size(), nullptr, nullptr);

        float stability = elig_uq_get_stability(bridge);
        EXPECT_GE(stability, 0.0f) << "Iteration " << i;
        EXPECT_LE(stability, 1.0f) << "Iteration " << i;
    }
}

TEST_F(EligibilityUtilsQuantumRegressionTest, IntegrationParamsBounds) {
    std::vector<eligibility_trace_t> traces(50);
    std::vector<float> weights(50);
    for (size_t i = 0; i < traces.size(); i++) {
        eligibility_trace_init(&traces[i], 0);
        traces[i].trace = 0.5f;
        weights[i] = 0.5f;
    }

    for (int i = 0; i < 200; i++) {
        // Apply random param feedback
        elig_quantum_params_t params = {
            .tau_fast = 1.0f + (float)(rand() % 100),
            .tau_slow = 10.0f + (float)(rand() % 1000),
            .learning_rate = 0.0001f + 0.01f * (float)(rand() % 100) / 100.0f,
            .dopamine_sensitivity = 1.0f,
            .burst_threshold = 0.5f,
            .consolidation_threshold = 0.3f,
            .energy = 0.1f + (float)(rand() % 100),
            .amplitude = 1.0f
        };

        elig_uq_backward_effect_t effect;
        elig_uq_apply_param_feedback(bridge, &params, &effect);

        float dt, tolerance;
        elig_uq_get_integration_params(bridge, &dt, &tolerance);

        EXPECT_GT(dt, 0.0f) << "Iteration " << i;
        EXPECT_GT(tolerance, 0.0f) << "Iteration " << i;
        EXPECT_TRUE(std::isfinite(dt)) << "Iteration " << i;
        EXPECT_TRUE(std::isfinite(tolerance)) << "Iteration " << i;
    }
}

//=============================================================================
// Stress Test
//=============================================================================

TEST_F(EligibilityUtilsQuantumRegressionTest, LongRunningStability) {
    const int duration_seconds = 2;  // Run for 2 seconds
    const int iterations_per_check = 100;

    std::vector<eligibility_trace_t> traces(200);
    std::vector<float> weights(200);
    for (size_t i = 0; i < traces.size(); i++) {
        eligibility_trace_init(&traces[i], 0);
        traces[i].trace = 0.5f;
        weights[i] = 0.5f;
    }

    auto start = std::chrono::steady_clock::now();
    int total_iterations = 0;

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start);
        if (elapsed.count() >= duration_seconds) break;

        for (int i = 0; i < iterations_per_check; i++) {
            // Vary traces slightly
            for (size_t j = 0; j < traces.size(); j++) {
                traces[j].trace = 0.5f + 0.3f * sinf((float)(total_iterations + j) * 0.01f);
            }

            elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                       traces.size(), nullptr, nullptr);
            total_iterations++;
        }

        // Periodic verification
        EXPECT_TRUE(elig_uq_bridge_verify(bridge));
    }

    // Final verification
    EXPECT_TRUE(elig_uq_bridge_verify(bridge));

    elig_uq_bridge_stats_t stats;
    elig_uq_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.feedback_loop_iterations, 0u);
}
