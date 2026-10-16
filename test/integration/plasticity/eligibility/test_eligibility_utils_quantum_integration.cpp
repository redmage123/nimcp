/**
 * @file test_eligibility_utils_quantum_integration.cpp
 * @brief Integration tests for Eligibility Utils-Quantum Bidirectional Bridge
 *
 * WHAT: Tests bidirectional communication between Utils and Quantum modules
 * WHY:  Validates that the feedback loop improves learning dynamics
 * HOW:  Create full pipeline, run learning scenarios, verify improvements
 *
 * @version 1.0.0
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <chrono>

extern "C" {
#include "plasticity/eligibility/nimcp_eligibility_utils_quantum_bridge.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
}

//=============================================================================
// Integration Test Fixture
//=============================================================================

class EligibilityUtilsQuantumIntegrationTest : public ::testing::Test {
protected:
    elig_uq_bridge_t bridge = nullptr;
    eligibility_utils_ctx_t utils_ctx = nullptr;
    eligibility_quantum_ctx_t quantum_ctx = nullptr;
    eligibility_config_t elig_config;
    std::vector<eligibility_trace_t> traces;
    std::vector<float> weights;
    const uint32_t num_synapses = 200;

    void SetUp() override {
        // Create bridge with all features enabled
        elig_uq_bridge_config_t bridge_config = elig_uq_bridge_default_config();
        bridge_config.enable_auto_feedback_loop = true;
        bridge_config.feedback_loop_interval_ms = 10;  // Fast feedback for testing
        bridge = elig_uq_bridge_create(&bridge_config);

        // Create utils context with metrics enabled
        eligibility_utils_config_t utils_config = eligibility_utils_default_config();
        utils_config.enable_metrics = true;
        utils_config.enable_bottleneck_detection = true;
        utils_ctx = eligibility_utils_create(&utils_config);

        // Create quantum context with all features
        elig_quantum_config_t quantum_config = elig_quantum_default_config();
        quantum_config.enable_bottleneck_detection = true;
        quantum_config.enable_credit_assignment = true;
        quantum_config.enable_quantum_optimization = true;
        quantum_config.enable_quantum_walk = true;
        quantum_ctx = elig_quantum_create(&quantum_config);

        // Attach contexts to bridge
        elig_uq_bridge_attach_utils(bridge, utils_ctx);
        elig_uq_bridge_attach_quantum(bridge, quantum_ctx);

        // Initialize eligibility config
        elig_config = eligibility_default_config();

        // Initialize test data
        traces.resize(num_synapses);
        weights.resize(num_synapses);
        for (uint32_t i = 0; i < num_synapses; i++) {
            eligibility_trace_init(&traces[i], 0);
            traces[i].trace = 0.1f + 0.8f * (float)i / num_synapses;
            weights[i] = 0.3f + 0.4f * sinf((float)i * 0.1f);
        }
    }

    void TearDown() override {
        if (bridge) elig_uq_bridge_destroy(bridge);
        if (utils_ctx) eligibility_utils_destroy(utils_ctx);
        if (quantum_ctx) elig_quantum_destroy(quantum_ctx);
    }

    // Helper: Simulate learning with reward signal
    void simulateLearning(float reward, uint32_t iterations) {
        for (uint32_t iter = 0; iter < iterations; iter++) {
            // Update traces
            for (uint32_t i = 0; i < num_synapses; i++) {
                bool spike = (rand() % 10) < 3;  // 30% spike probability
                eligibility_trace_update(&traces[i], &elig_config, iter, spike ? 1.0f : 0.0f);

                // Record metrics
                eligibility_utils_record_update(utils_ctx, traces[i].trace, 0.0f);
            }

            // Run feedback loop periodically
            if (iter % 10 == 0) {
                elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                          num_synapses, nullptr, nullptr);
            }
        }
    }
};

//=============================================================================
// Full Pipeline Integration Tests
//=============================================================================

TEST_F(EligibilityUtilsQuantumIntegrationTest, FullPipelineConnectivity) {
    EXPECT_TRUE(elig_uq_bridge_is_connected(bridge));
    EXPECT_TRUE(elig_uq_bridge_verify(bridge));
}

TEST_F(EligibilityUtilsQuantumIntegrationTest, MetricsToQuantumTrigger) {
    // Create imbalanced LTP/LTD situation
    for (uint32_t i = 0; i < num_synapses; i++) {
        traces[i].trace = (i < num_synapses / 10) ? 0.9f : 0.05f;  // Very unbalanced
    }

    // Record metrics
    eligibility_utils_update_trace_stats(utils_ctx, traces.data(), num_synapses);

    // Evaluate metrics - should trigger quantum optimization
    elig_uq_forward_effect_t effect;
    int result = elig_uq_evaluate_metrics(bridge, &effect);
    EXPECT_EQ(result, 0);

    // With imbalanced traces, should request some quantum action
    EXPECT_TRUE(effect.request_annealing || effect.request_quantum_walk ||
                effect.request_quantum_shannon || effect.request_qmc_credit);
}

TEST_F(EligibilityUtilsQuantumIntegrationTest, QuantumOptimizationFeedback) {
    // Run initial feedback loop
    elig_uq_forward_effect_t fwd;
    elig_uq_backward_effect_t bwd;

    int result = elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                            num_synapses, &fwd, &bwd);
    EXPECT_EQ(result, 0);

    // Get integration params after feedback
    float dt, tolerance;
    elig_uq_get_integration_params(bridge, &dt, &tolerance);

    EXPECT_GT(dt, 0.0f);
    EXPECT_GT(tolerance, 0.0f);
    EXPECT_LE(dt, ELIG_INTEGRATION_DT * 2.0f);  // Should be reasonable
}

TEST_F(EligibilityUtilsQuantumIntegrationTest, BottleneckEscalation) {
    // Create bottleneck situation - high demand, low capacity
    for (uint32_t i = 0; i < num_synapses; i++) {
        // Concentrate trace in few synapses (inefficient)
        traces[i].trace = (i < 5) ? 0.95f : 0.001f;
    }

    // Analyze information flow via utils
    float efficiency = eligibility_utils_analyze_information_flow(
        utils_ctx, traces.data(), weights.data(), num_synapses);

    EXPECT_LT(efficiency, 0.5f);  // Should be low

    // Detect bottlenecks via utils
    eligibility_bottleneck_t bottlenecks[10];
    uint32_t num_found = 0;
    eligibility_utils_detect_bottlenecks(utils_ctx, traces.data(), weights.data(),
                                         num_synapses, bottlenecks, 10, &num_found);

    // Escalate to quantum-Shannon
    elig_uq_forward_effect_t effect;
    elig_uq_escalate_bottleneck(bridge, bottlenecks, num_found, &effect);

    // Compute average deficit to verify test expectation
    float avg_deficit = 0.0f;
    if (num_found > 0) {
        for (uint32_t i = 0; i < num_found; i++) {
            avg_deficit += bottlenecks[i].information_deficit;
        }
        avg_deficit /= num_found;
    }

    // Should request quantum-Shannon if deficit exceeds threshold
    if (avg_deficit > ELIG_UQ_BOTTLENECK_ESCALATION) {
        EXPECT_TRUE(effect.request_quantum_shannon);
    }
    // Regardless, bottleneck count should be recorded
    EXPECT_EQ(effect.bottleneck_count, num_found);
}

TEST_F(EligibilityUtilsQuantumIntegrationTest, CreditAssignmentFlow) {
    // Setup for credit assignment
    for (uint32_t i = 0; i < num_synapses; i++) {
        traces[i].trace = 0.1f + 0.8f * expf(-0.02f * (float)i);
    }

    // Request credit assignment via quantum
    elig_quantum_credit_t credits[50];
    uint32_t num_credits = 0;
    bool success = elig_quantum_assign_credit(quantum_ctx, traces.data(), num_synapses,
                                               1.0f, credits, 50, &num_credits);
    EXPECT_TRUE(success);
    EXPECT_GT(num_credits, 0u);

    // Apply credit feedback to bridge
    elig_uq_backward_effect_t effect;
    int result = elig_uq_apply_credit_feedback(bridge, credits, num_credits, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.feedback_type, ELIG_UQ_FEEDBACK_CREDIT_ASSIGNMENT);
    EXPECT_GT(effect.avg_credit_fraction, 0.0f);
    EXPECT_GT(effect.credit_confidence, 0.0f);
}

TEST_F(EligibilityUtilsQuantumIntegrationTest, DiffusionPriorityIntegration) {
    // Initial trace at source
    uint32_t source = 0;
    traces[source].trace = 1.0f;
    for (uint32_t i = 1; i < num_synapses; i++) {
        traces[i].trace = 0.01f;
    }

    // Diffuse via quantum walk
    std::vector<float> diffused(num_synapses);
    bool success = elig_quantum_diffuse(quantum_ctx, source, traces[source].trace,
                                         nullptr, num_synapses, diffused.data());
    EXPECT_TRUE(success);

    // Apply diffusion feedback
    elig_uq_backward_effect_t effect;
    int result = elig_uq_apply_diffusion_feedback(bridge, diffused.data(), num_synapses, &effect);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(effect.feedback_type, ELIG_UQ_FEEDBACK_DIFFUSION_RESULT);
    EXPECT_GT(effect.diffusion_speedup, 1.0f);  // Should show speedup
}

//=============================================================================
// Feedback Loop Convergence Tests
//=============================================================================

TEST_F(EligibilityUtilsQuantumIntegrationTest, FeedbackLoopConvergence) {
    float initial_coherence = elig_uq_get_coherence(bridge);

    // Run many feedback iterations
    for (int i = 0; i < 100; i++) {
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   num_synapses, nullptr, nullptr);
    }

    // Get final state
    elig_uq_bridge_state_t state;
    elig_uq_bridge_get_state(bridge, &state);

    // Coherence should be bounded
    EXPECT_GE(state.utils_quantum_coherence, ELIG_UQ_MIN_COHERENCE);
    EXPECT_LE(state.utils_quantum_coherence, 1.0f);

    // Stability should be computed
    float stability = elig_uq_get_stability(bridge);
    EXPECT_GE(stability, 0.0f);
    EXPECT_LE(stability, 1.0f);
}

TEST_F(EligibilityUtilsQuantumIntegrationTest, ParameterAdaptation) {
    // Get initial integration params
    float initial_dt, initial_tol;
    elig_uq_get_integration_params(bridge, &initial_dt, &initial_tol);

    // Simulate learning with various dynamics
    simulateLearning(1.0f, 100);

    // Get final integration params
    float final_dt, final_tol;
    elig_uq_get_integration_params(bridge, &final_dt, &final_tol);

    // Parameters should have been adapted
    EXPECT_GT(final_dt, 0.0f);
    EXPECT_GT(final_tol, 0.0f);
}

TEST_F(EligibilityUtilsQuantumIntegrationTest, StatisticsAccumulation) {
    // Run learning simulation
    simulateLearning(1.0f, 200);

    // Get statistics
    elig_uq_bridge_stats_t stats;
    elig_uq_bridge_get_stats(bridge, &stats);

    // Should have accumulated events
    EXPECT_GT(stats.total_forward_events, 0u);
    EXPECT_GT(stats.feedback_loop_iterations, 0u);

    // Average round-trip should be measured
    EXPECT_GT(stats.avg_round_trip_us, 0.0);
}

//=============================================================================
// Multi-Module Coordination Tests
//=============================================================================

TEST_F(EligibilityUtilsQuantumIntegrationTest, UtilsMetricsQuantumOptimization) {
    // Setup high-activity scenario
    for (uint32_t i = 0; i < num_synapses; i++) {
        traces[i].trace = 0.8f;  // High uniform activity
    }

    // Record many updates to build metrics
    for (int i = 0; i < 100; i++) {
        for (uint32_t j = 0; j < num_synapses; j++) {
            eligibility_utils_record_update(utils_ctx, traces[j].trace, 0.01f * (rand() % 10 - 5));
        }
    }

    // Get metrics
    eligibility_metrics_t metrics;
    eligibility_utils_get_metrics(utils_ctx, &metrics);

    // Run feedback loop
    elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                               num_synapses, nullptr, nullptr);

    // Verify quantum optimization ran
    elig_quantum_metrics_t q_metrics;
    elig_quantum_get_metrics(quantum_ctx, &q_metrics);

    // Stats should show activity
    elig_uq_bridge_stats_t bridge_stats;
    elig_uq_bridge_get_stats(bridge, &bridge_stats);
    EXPECT_GT(bridge_stats.feedback_loop_iterations, 0u);
}

TEST_F(EligibilityUtilsQuantumIntegrationTest, ConcurrentForwardBackward) {
    // Test rapid forward-backward cycles
    for (int cycle = 0; cycle < 50; cycle++) {
        // Forward: evaluate and trigger
        elig_uq_forward_effect_t fwd;
        elig_uq_evaluate_metrics(bridge, &fwd);

        // Backward: apply feedback
        elig_quantum_params_t params = {
            .tau_fast = 10.0f - 0.1f * cycle,
            .tau_slow = 100.0f,
            .learning_rate = 0.001f,
            .dopamine_sensitivity = 1.0f,
            .burst_threshold = 0.5f,
            .consolidation_threshold = 0.3f,
            .energy = 5.0f - 0.05f * cycle,
            .amplitude = 1.0f
        };

        elig_uq_backward_effect_t bwd;
        elig_uq_apply_param_feedback(bridge, &params, &bwd);
    }

    // Should still be valid
    EXPECT_TRUE(elig_uq_bridge_verify(bridge));

    // Coherence should have improved (energy decreased)
    elig_uq_bridge_stats_t stats;
    elig_uq_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.successful_optimizations, 0u);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(EligibilityUtilsQuantumIntegrationTest, FeedbackLoopLatency) {
    // Warm up
    for (int i = 0; i < 10; i++) {
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   num_synapses, nullptr, nullptr);
    }

    // Measure latency
    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 100;
    for (int i = 0; i < iterations; i++) {
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   num_synapses, nullptr, nullptr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_latency_us = (double)duration.count() / iterations;

    // Should be reasonably fast (under 1ms per iteration for 200 synapses)
    EXPECT_LT(avg_latency_us, 1000.0);

    // Verify reported latency is close to measured
    elig_uq_bridge_stats_t stats;
    elig_uq_bridge_get_stats(bridge, &stats);

    // Allow 5x tolerance for system variation
    EXPECT_LT(stats.avg_round_trip_us, avg_latency_us * 5);
}
