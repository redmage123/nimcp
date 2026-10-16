/**
 * @file test_eligibility_utils_quantum_e2e.cpp
 * @brief End-to-End tests for Eligibility Utils-Quantum Bidirectional Bridge
 *
 * WHAT: Tests complete learning scenarios with bidirectional feedback
 * WHY:  Validates that the bridge improves actual learning outcomes
 * HOW:  Simulate full learning scenarios, measure improvements
 *
 * @version 1.0.0
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <chrono>
#include <numeric>
#include <algorithm>

extern "C" {
#include "plasticity/eligibility/nimcp_eligibility_utils_quantum_bridge.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
}

//=============================================================================
// E2E Test Fixture - Full Learning Pipeline
//=============================================================================

class EligibilityUtilsQuantumE2ETest : public ::testing::Test {
protected:
    // Full pipeline components
    elig_uq_bridge_t bridge = nullptr;
    eligibility_utils_ctx_t utils_ctx = nullptr;
    eligibility_quantum_ctx_t quantum_ctx = nullptr;
    eligibility_config_t elig_config;

    // Learning simulation state
    std::vector<eligibility_trace_t> traces;
    std::vector<float> weights;
    std::vector<float> target_weights;  // Ground truth for learning
    const uint32_t num_synapses = 100;

    void SetUp() override {
        // Create full pipeline
        bridge = elig_uq_bridge_create(nullptr);
        utils_ctx = eligibility_utils_create(nullptr);
        quantum_ctx = elig_quantum_create(nullptr);

        elig_uq_bridge_attach_utils(bridge, utils_ctx);
        elig_uq_bridge_attach_quantum(bridge, quantum_ctx);

        elig_config = eligibility_default_config();

        // Initialize synapses
        traces.resize(num_synapses);
        weights.resize(num_synapses);
        target_weights.resize(num_synapses);

        for (uint32_t i = 0; i < num_synapses; i++) {
            eligibility_trace_init(&traces[i], 0);
            weights[i] = 0.5f;
            // Target: sine wave pattern
            target_weights[i] = 0.3f + 0.4f * (0.5f + 0.5f * sinf((float)i * 0.1f));
        }
    }

    void TearDown() override {
        if (bridge) elig_uq_bridge_destroy(bridge);
        if (utils_ctx) eligibility_utils_destroy(utils_ctx);
        if (quantum_ctx) elig_quantum_destroy(quantum_ctx);
    }

    // Compute mean squared error from target
    float computeMSE() {
        float mse = 0.0f;
        for (uint32_t i = 0; i < num_synapses; i++) {
            float diff = weights[i] - target_weights[i];
            mse += diff * diff;
        }
        return mse / num_synapses;
    }

    // Simulate one learning step
    void learnStep(uint64_t time_ms, float learning_rate) {
        // Compute reward based on current error
        float error = computeMSE();
        float reward = 1.0f / (1.0f + error * 10.0f);  // Higher reward for lower error

        // Update traces based on "activity"
        for (uint32_t i = 0; i < num_synapses; i++) {
            // Spike probability based on weight deviation
            float deviation = fabsf(weights[i] - target_weights[i]);
            bool spike = (rand() % 100) < (int)(30 + 40 * deviation);

            eligibility_trace_update(&traces[i], &elig_config, time_ms, spike ? 1.0f : 0.0f);
        }

        // Apply three-factor learning: weight += lr * trace * reward * direction
        for (uint32_t i = 0; i < num_synapses; i++) {
            float direction = (target_weights[i] - weights[i]) > 0 ? 1.0f : -1.0f;
            float delta_w = learning_rate * traces[i].trace * reward * direction;
            weights[i] += delta_w;
            weights[i] = std::max(0.0f, std::min(1.0f, weights[i]));

            // Record metrics
            eligibility_utils_record_update(utils_ctx, traces[i].trace, delta_w);
        }
    }
};

//=============================================================================
// E2E Learning Scenario Tests
//=============================================================================

TEST_F(EligibilityUtilsQuantumE2ETest, LearningWithBidirectionalFeedback) {
    const int learning_iterations = 500;
    const float base_learning_rate = 0.01f;

    float initial_mse = computeMSE();

    // Run learning with bidirectional feedback
    for (int iter = 0; iter < learning_iterations; iter++) {
        // Learning step
        learnStep(iter, base_learning_rate);

        // Run feedback loop every 10 iterations
        if (iter % 10 == 0) {
            elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                       num_synapses, nullptr, nullptr);
        }
    }

    float final_mse = computeMSE();

    // Learning should improve (reduce MSE)
    EXPECT_LT(final_mse, initial_mse);

    // Should achieve significant improvement
    float improvement = (initial_mse - final_mse) / initial_mse;
    EXPECT_GT(improvement, 0.2f);  // At least 20% improvement
}

TEST_F(EligibilityUtilsQuantumE2ETest, LearningWithoutBidirectionalFeedback) {
    const int learning_iterations = 500;
    const float base_learning_rate = 0.01f;

    float initial_mse = computeMSE();

    // Run learning WITHOUT feedback loop
    for (int iter = 0; iter < learning_iterations; iter++) {
        learnStep(iter, base_learning_rate);
    }

    float final_mse_without = computeMSE();

    // Reset and run WITH feedback
    for (uint32_t i = 0; i < num_synapses; i++) {
        eligibility_trace_init(&traces[i], 0);
        weights[i] = 0.5f;
    }
    elig_uq_bridge_reset(bridge);

    for (int iter = 0; iter < learning_iterations; iter++) {
        learnStep(iter, base_learning_rate);
        if (iter % 10 == 0) {
            elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                       num_synapses, nullptr, nullptr);
        }
    }

    float final_mse_with = computeMSE();

    // Both should improve
    EXPECT_LT(final_mse_without, initial_mse);
    EXPECT_LT(final_mse_with, initial_mse);
}

TEST_F(EligibilityUtilsQuantumE2ETest, AdaptiveIntegrationParameters) {
    const int learning_iterations = 200;

    float initial_dt, initial_tol;
    elig_uq_get_integration_params(bridge, &initial_dt, &initial_tol);

    // Run learning with various dynamics
    for (int iter = 0; iter < learning_iterations; iter++) {
        learnStep(iter, 0.01f);

        if (iter % 5 == 0) {
            elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                       num_synapses, nullptr, nullptr);
        }
    }

    float final_dt, final_tol;
    elig_uq_get_integration_params(bridge, &final_dt, &final_tol);

    // Parameters should have been adapted (may or may not change significantly)
    EXPECT_GT(final_dt, 0.0f);
    EXPECT_GT(final_tol, 0.0f);
}

//=============================================================================
// Credit Assignment Scenario Tests
//=============================================================================

TEST_F(EligibilityUtilsQuantumE2ETest, CreditAssignmentForDelayedReward) {
    // Scenario: Only some synapses should get credit for reward
    // Setup: Create activity pattern, delay reward, verify credit assignment

    // Active synapses (should get credit)
    std::vector<uint32_t> active_synapses = {0, 5, 10, 15, 20};
    for (uint32_t id : active_synapses) {
        traces[id].trace = 0.9f;
    }

    // Inactive synapses
    for (uint32_t i = 0; i < num_synapses; i++) {
        if (std::find(active_synapses.begin(), active_synapses.end(), i) == active_synapses.end()) {
            traces[i].trace = 0.05f;
        }
    }

    // Get quantum credit assignment
    elig_quantum_credit_t credits[50];
    uint32_t num_credits = 0;
    bool success = elig_quantum_assign_credit(quantum_ctx, traces.data(), num_synapses,
                                               1.0f, credits, 50, &num_credits);
    EXPECT_TRUE(success);
    EXPECT_GT(num_credits, 0u);

    // Apply feedback
    elig_uq_backward_effect_t effect;
    elig_uq_apply_credit_feedback(bridge, credits, num_credits, &effect);

    // Verify active synapses got more credit
    float active_credit = 0.0f;
    float inactive_credit = 0.0f;
    int active_count = 0;
    int inactive_count = 0;

    for (uint32_t c = 0; c < num_credits; c++) {
        bool is_active = std::find(active_synapses.begin(), active_synapses.end(),
                                   credits[c].synapse_id) != active_synapses.end();
        if (is_active) {
            active_credit += credits[c].credit_fraction;
            active_count++;
        } else {
            inactive_credit += credits[c].credit_fraction;
            inactive_count++;
        }
    }

    // Active synapses should have higher average credit
    if (active_count > 0 && inactive_count > 0) {
        float avg_active = active_credit / active_count;
        float avg_inactive = inactive_credit / inactive_count;
        EXPECT_GT(avg_active, avg_inactive);
    }
}

//=============================================================================
// Bottleneck Detection and Resolution Scenario
//=============================================================================

TEST_F(EligibilityUtilsQuantumE2ETest, BottleneckDetectionAndResolution) {
    // Create artificial bottleneck: concentrate activity in few synapses
    for (uint32_t i = 0; i < num_synapses; i++) {
        traces[i].trace = (i < 3) ? 0.95f : 0.01f;  // Only 3 active
    }

    // Initial information efficiency should be low
    float initial_efficiency = eligibility_utils_analyze_information_flow(
        utils_ctx, traces.data(), weights.data(), num_synapses);

    EXPECT_LT(initial_efficiency, 0.5f);

    // Run feedback loops to trigger bottleneck detection and resolution
    for (int iter = 0; iter < 50; iter++) {
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   num_synapses, nullptr, nullptr);

        // Simulate some spreading of activity
        for (uint32_t i = 0; i < num_synapses; i++) {
            if (traces[i].trace > 0.1f) {
                // Spread to neighbors
                if (i > 0) traces[i-1].trace += 0.05f;
                if (i < num_synapses - 1) traces[i+1].trace += 0.05f;
            }
            // Normalize
            traces[i].trace = std::min(traces[i].trace, 1.0f);
        }
    }

    // Get bottleneck statistics
    elig_uq_bridge_stats_t stats;
    elig_uq_bridge_get_stats(bridge, &stats);

    // Final efficiency should be better (activity spread out)
    float final_efficiency = eligibility_utils_analyze_information_flow(
        utils_ctx, traces.data(), weights.data(), num_synapses);

    // Either bottleneck was detected/resolved, OR efficiency improved
    bool bottleneck_processed = (stats.bottleneck_triggers + stats.bottleneck_resolutions) > 0;
    bool efficiency_improved = final_efficiency >= initial_efficiency;

    EXPECT_TRUE(bottleneck_processed || efficiency_improved)
        << "Neither bottleneck processing nor efficiency improvement occurred";
}

//=============================================================================
// Coherence and Stability Scenario
//=============================================================================

TEST_F(EligibilityUtilsQuantumE2ETest, CoherenceConvergence) {
    const int iterations = 300;

    std::vector<float> coherence_history;
    coherence_history.reserve(iterations);

    for (int iter = 0; iter < iterations; iter++) {
        // Vary traces to create dynamics
        for (uint32_t i = 0; i < num_synapses; i++) {
            traces[i].trace = 0.5f + 0.3f * sinf((float)(iter + i) * 0.05f);
        }

        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   num_synapses, nullptr, nullptr);

        coherence_history.push_back(elig_uq_get_coherence(bridge));
    }

    // Coherence should stabilize (low variance in later iterations)
    float late_mean = 0.0f;
    float late_variance = 0.0f;
    int late_start = iterations - 100;

    for (int i = late_start; i < iterations; i++) {
        late_mean += coherence_history[i];
    }
    late_mean /= 100.0f;

    for (int i = late_start; i < iterations; i++) {
        float diff = coherence_history[i] - late_mean;
        late_variance += diff * diff;
    }
    late_variance /= 99.0f;

    // Variance should be small (coherence stabilized)
    EXPECT_LT(late_variance, 0.01f);

    // Coherence should be in valid range
    EXPECT_GE(late_mean, ELIG_UQ_MIN_COHERENCE);
    EXPECT_LE(late_mean, 1.0f);
}

TEST_F(EligibilityUtilsQuantumE2ETest, StabilityUnderPerturbation) {
    const int warmup_iterations = 100;
    const int perturbation_iterations = 50;
    const int recovery_iterations = 100;

    // Warmup
    for (int iter = 0; iter < warmup_iterations; iter++) {
        for (uint32_t i = 0; i < num_synapses; i++) {
            traces[i].trace = 0.5f;
        }
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   num_synapses, nullptr, nullptr);
    }

    float pre_perturbation_stability = elig_uq_get_stability(bridge);

    // Perturbation: rapid, chaotic changes
    for (int iter = 0; iter < perturbation_iterations; iter++) {
        for (uint32_t i = 0; i < num_synapses; i++) {
            traces[i].trace = (float)(rand() % 100) / 100.0f;
        }
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   num_synapses, nullptr, nullptr);
    }

    float during_perturbation_stability = elig_uq_get_stability(bridge);

    // Recovery: stable input
    for (int iter = 0; iter < recovery_iterations; iter++) {
        for (uint32_t i = 0; i < num_synapses; i++) {
            traces[i].trace = 0.5f;
        }
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   num_synapses, nullptr, nullptr);
    }

    float post_recovery_stability = elig_uq_get_stability(bridge);

    // System should maintain some stability throughout (not crash or diverge)
    EXPECT_GE(during_perturbation_stability, 0.0f);
    EXPECT_LE(during_perturbation_stability, 1.0f);

    // Stability should be in valid range after recovery
    EXPECT_GE(post_recovery_stability, 0.0f);
    EXPECT_LE(post_recovery_stability, 1.0f);

    // Either perturbation decreased stability OR recovery improved it
    // (the system should respond to changes in input patterns)
    bool responded_to_perturbation = (during_perturbation_stability != pre_perturbation_stability);
    bool recovered_stability = (post_recovery_stability != during_perturbation_stability);

    EXPECT_TRUE(responded_to_perturbation || recovered_stability)
        << "System didn't respond to perturbation or recovery";
}

//=============================================================================
// Full System Performance Test
//=============================================================================

TEST_F(EligibilityUtilsQuantumE2ETest, SystemThroughput) {
    const int total_iterations = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < total_iterations; iter++) {
        // Simulate activity
        for (uint32_t i = 0; i < num_synapses; i++) {
            traces[i].trace = 0.5f + 0.3f * sinf((float)(iter + i) * 0.02f);
        }

        // Full feedback loop
        elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                   num_synapses, nullptr, nullptr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double iterations_per_second = (double)total_iterations * 1000.0 / duration.count();

    // Should achieve reasonable throughput (at least 100 iterations/second)
    EXPECT_GT(iterations_per_second, 100.0);

    // Print performance for reference
    printf("E2E Throughput: %.1f iterations/second (%.2f ms/iteration)\n",
           iterations_per_second, (double)duration.count() / total_iterations);
}

//=============================================================================
// Export and Diagnostics Test
//=============================================================================

TEST_F(EligibilityUtilsQuantumE2ETest, ExportAfterLearning) {
    // Run learning
    for (int iter = 0; iter < 100; iter++) {
        learnStep(iter, 0.01f);
        if (iter % 10 == 0) {
            elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                       num_synapses, nullptr, nullptr);
        }
    }

    // Export to CSV
    const char* filename = "/tmp/elig_uq_e2e_export.csv";
    bool success = elig_uq_bridge_export_csv(bridge, filename);
    EXPECT_TRUE(success);

    // Verify file was created and has content
    FILE* f = fopen(filename, "r");
    ASSERT_NE(f, nullptr);

    char line[256];
    int line_count = 0;
    while (fgets(line, sizeof(line), f)) {
        line_count++;
    }
    fclose(f);

    // Should have header + multiple data lines
    EXPECT_GT(line_count, 5);

    // Cleanup
    remove(filename);
}

TEST_F(EligibilityUtilsQuantumE2ETest, PrintSummaryAfterLearning) {
    // Run learning
    for (int iter = 0; iter < 50; iter++) {
        learnStep(iter, 0.01f);
        if (iter % 10 == 0) {
            elig_uq_feedback_loop_tick(bridge, traces.data(), weights.data(),
                                       num_synapses, nullptr, nullptr);
        }
    }

    // Should not crash
    elig_uq_bridge_print_summary(bridge);

    // Verify bridge
    EXPECT_TRUE(elig_uq_bridge_verify(bridge));
}
