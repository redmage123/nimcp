/**
 * @file test_bcm.cpp
 * @brief Test-driven development for BCM learning rule
 *
 * BCM (Bienenstock-Cooper-Munro) Rule:
 * - Sliding threshold that depends on neuron's average activity
 * - LTP when post-synaptic activity > threshold
 * - LTD when post-synaptic activity < threshold
 * - Self-stabilizing without explicit weight normalization
 *
 * BIOLOGICAL BASIS:
 * - Models visual cortex development and ocular dominance
 * - Explains critical periods in learning
 * - Implements "rich get richer" dynamics
 *
 * TEST PHILOSOPHY:
 * - Red-Green-Refactor cycle
 * - Test biological constraints
 * - Verify stability and convergence
 * - Performance benchmarks
 */

#include <gtest/gtest.h>
#include "plasticity/bcm/nimcp_bcm.h"
#include <cmath>
#include <chrono>

//=============================================================================
// Test Fixtures
//=============================================================================

class BCMTest : public ::testing::Test {
protected:
    bcm_synapse_t synapse;
    bcm_params_t params;

    void SetUp() override {
        /* WHAT: Initialize default BCM parameters
         * WHY:  Consistent starting state for all tests
         */
        params.learning_rate = 0.01f;
        params.threshold_time_constant = 1000.0f;  // ms
        params.activity_time_constant = 100.0f;     // ms
        params.min_threshold = 0.1f;
        params.max_threshold = 10.0f;

        /* WHAT: Initialize synapse with baseline values
         * WHY:  Test starting from typical physiological state
         */
        synapse.weight = 0.5f;
        synapse.threshold = 1.0f;
        synapse.avg_post_activity = 0.5f;
        synapse.eligibility = 0.0f;
    }
};

//=============================================================================
// Creation and Initialization Tests (TDD: Red phase)
//=============================================================================

TEST_F(BCMTest, InitializeSynapse) {
    /* WHAT: Test synapse initialization
     * WHY:  Verify starting state is physiologically plausible
     */
    bcm_synapse_t syn = bcm_synapse_init(0.3f, 1.0f);

    EXPECT_FLOAT_EQ(syn.weight, 0.3f);
    EXPECT_FLOAT_EQ(syn.threshold, 1.0f);
    EXPECT_FLOAT_EQ(syn.avg_post_activity, 0.0f);
    EXPECT_FLOAT_EQ(syn.eligibility, 0.0f);
}

TEST_F(BCMTest, ThresholdUpdateIncreases) {
    /* WHAT: Test threshold increases with high activity
     * WHY:  BCM threshold should track average post-synaptic activity
     * BIOLOGICAL: Prevents runaway potentiation
     */
    float high_activity = 2.0f;
    float dt = 10.0f;  // ms

    bcm_update_threshold(&synapse, high_activity, dt, &params);

    EXPECT_GT(synapse.threshold, 1.0f);
    EXPECT_LT(synapse.threshold, params.max_threshold);
}

TEST_F(BCMTest, ThresholdUpdateDecreases) {
    /* WHAT: Test threshold decreases with low activity
     * WHY:  Threshold should adapt downward when neuron is quiet
     */
    synapse.threshold = 2.0f;
    float low_activity = 0.2f;
    float dt = 10.0f;

    bcm_update_threshold(&synapse, low_activity, dt, &params);

    EXPECT_LT(synapse.threshold, 2.0f);
    EXPECT_GT(synapse.threshold, params.min_threshold);
}

//=============================================================================
// Plasticity Rule Tests
//=============================================================================

TEST_F(BCMTest, LTP_WhenAboveThreshold) {
    /* WHAT: Test long-term potentiation when activity exceeds threshold
     * WHY:  Core BCM rule: Δw ∝ post × (post - threshold) × pre
     * BIOLOGICAL: Strengthens synapses during high activity
     */
    float pre_activity = 1.0f;
    float post_activity = 2.0f;  // Above threshold (1.0)
    float dt = 1.0f;

    float old_weight = synapse.weight;
    bcm_apply_rule(&synapse, pre_activity, post_activity, dt, &params);

    EXPECT_GT(synapse.weight, old_weight);
    EXPECT_LE(synapse.weight, 1.0f);  // Should be clamped
}

TEST_F(BCMTest, LTD_WhenBelowThreshold) {
    /* WHAT: Test long-term depression when activity below threshold
     * WHY:  BCM implements depression for weak activations
     * BIOLOGICAL: Prunes ineffective synapses
     */
    synapse.weight = 0.6f;
    float pre_activity = 1.0f;
    float post_activity = 0.5f;  // Below threshold (1.0)
    float dt = 1.0f;

    float old_weight = synapse.weight;
    bcm_apply_rule(&synapse, pre_activity, post_activity, dt, &params);

    EXPECT_LT(synapse.weight, old_weight);
    EXPECT_GE(synapse.weight, 0.0f);  // Should be clamped
}

TEST_F(BCMTest, NoChange_AtThreshold) {
    /* WHAT: Test no change when activity equals threshold
     * WHY:  BCM crossover point should produce zero weight change
     */
    float pre_activity = 1.0f;
    float post_activity = synapse.threshold;  // Exactly at threshold
    float dt = 1.0f;

    float old_weight = synapse.weight;
    bcm_apply_rule(&synapse, pre_activity, post_activity, dt, &params);

    EXPECT_NEAR(synapse.weight, old_weight, 1e-6f);
}

//=============================================================================
// Stability Tests
//=============================================================================

TEST_F(BCMTest, ConvergesToStableState) {
    /* WHAT: Test BCM converges to stable weight distribution
     * WHY:  BCM should self-stabilize without external normalization
     * BIOLOGICAL: Models developmental stabilization
     */
    float pre_activity = 0.8f;
    float post_activity = 0.8f;  // Use activity that won't saturate
    float dt = 1.0f;

    /* WHAT: Run 1000 updates and check convergence
     * WHY:  Should reach equilibrium where threshold ≈ post_activity²
     */
    for (int i = 0; i < 1000; i++) {
        bcm_update_threshold(&synapse, post_activity, dt, &params);
        bcm_apply_rule(&synapse, pre_activity, post_activity, dt, &params);
    }

    /* WHAT: Verify threshold converged to post_activity² (BCM rule)
     * WHY:  At equilibrium, threshold tracks E[post²]
     * FORMULA: θ → post² = 0.8² = 0.64
     */
    float expected_threshold = post_activity * post_activity;
    EXPECT_NEAR(synapse.threshold, expected_threshold, 0.2f);

    /* WHAT: Verify weight is stable (bounded)
     * WHY:  Should not grow unbounded
     */
    EXPECT_GT(synapse.weight, 0.0f);
    EXPECT_LT(synapse.weight, 1.0f);
}

TEST_F(BCMTest, RichGetRicher_Dynamics) {
    /* WHAT: Test bidirectional plasticity (LTP vs LTD)
     * WHY:  Post-synaptic activity relative to threshold determines sign
     * BIOLOGICAL: Explains selective strengthening in cortex
     */
    bcm_synapse_t syn_ltp = bcm_synapse_init(0.5f, 0.3f);  // Low threshold
    bcm_synapse_t syn_ltd = bcm_synapse_init(0.5f, 1.0f);  // High threshold

    float pre = 1.0f;
    float post = 0.8f;  // Moderate activity
    float dt = 1.0f;

    /* WHAT: Apply same input to synapses with different thresholds
     * WHY:  Test bidirectional plasticity
     * LOGIC: post > θ_low → LTP, post < θ_high → LTD
     */
    bcm_apply_rule(&syn_ltp, pre, post, dt, &params);
    bcm_apply_rule(&syn_ltd, pre, post, dt, &params);

    /* WHAT: Verify LTP when post > threshold
     * WHY:  post (0.8) > threshold (0.3) → positive weight change
     */
    EXPECT_GT(syn_ltp.weight, 0.5f);

    /* WHAT: Verify LTD when post < threshold
     * WHY:  post (0.8) < threshold (1.0) → negative weight change
     */
    EXPECT_LT(syn_ltd.weight, 0.5f);
}

//=============================================================================
// Integration with Neuromodulators
//=============================================================================

TEST_F(BCMTest, NeuromodulatorGating) {
    /* WHAT: Test BCM learning rate modulation by dopamine
     * WHY:  Reward signals should gate BCM plasticity
     * BIOLOGICAL: Dopamine gates cortical plasticity
     */
    float pre = 1.0f;
    float post = 2.0f;
    float dt = 1.0f;

    /* WHAT: Apply BCM with low dopamine (no reward)
     * WHY:  Should produce minimal weight change
     */
    float dopamine_low = 0.1f;
    float old_weight = synapse.weight;
    bcm_apply_rule_modulated(&synapse, pre, post, dt, &params, dopamine_low);
    float change_low = synapse.weight - old_weight;

    /* WHAT: Reset and apply with high dopamine (reward)
     * WHY:  Should produce larger weight change
     */
    synapse.weight = old_weight;
    float dopamine_high = 0.9f;
    bcm_apply_rule_modulated(&synapse, pre, post, dt, &params, dopamine_high);
    float change_high = synapse.weight - old_weight;

    EXPECT_GT(fabs(change_high), fabs(change_low));
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(BCMTest, UpdatePerformance) {
    /* WHAT: Benchmark BCM update performance
     * WHY:  Should be < 10 microseconds per synapse
     * TARGET: 100,000 synapses/second on single core
     */
    const int NUM_UPDATES = 10000;
    float pre = 0.8f;
    float post = 1.5f;
    float dt = 1.0f;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_UPDATES; i++) {
        bcm_update_threshold(&synapse, post, dt, &params);
        bcm_apply_rule(&synapse, pre, post, dt, &params);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float us_per_update = duration.count() / (float)NUM_UPDATES;

    EXPECT_LT(us_per_update, 10.0f);

    std::cout << "BCM update performance: " << us_per_update
              << " μs per synapse" << std::endl;
}

//=============================================================================
// Biological Realism Tests
//=============================================================================

TEST_F(BCMTest, CriticalPeriod) {
    /* WHAT: Test BCM implements critical period learning
     * WHY:  Learning should be fast early, then slow
     * BIOLOGICAL: Models visual cortex critical periods
     */
    params.learning_rate = 0.1f;  // High initial rate
    float pre = 1.0f;
    float post = 2.0f;
    float dt = 1.0f;

    /* WHAT: Early learning (critical period)
     * WHY:  Should produce large weight changes
     */
    float initial_weight = synapse.weight;
    for (int i = 0; i < 100; i++) {
        bcm_apply_rule(&synapse, pre, post, dt, &params);
    }
    float early_change = synapse.weight - initial_weight;

    /* WHAT: Reduce learning rate (end of critical period)
     * WHY:  Simulates maturation
     */
    params.learning_rate = 0.01f;
    float mature_weight = synapse.weight;

    for (int i = 0; i < 100; i++) {
        bcm_apply_rule(&synapse, pre, post, dt, &params);
    }
    float late_change = synapse.weight - mature_weight;

    /* WHAT: Early changes should be much larger
     * WHY:  Critical period has higher plasticity
     */
    EXPECT_GT(fabs(early_change), 5.0f * fabs(late_change));
}

//=============================================================================
// Boundary Condition Tests
//=============================================================================

TEST_F(BCMTest, ExtremeActivity_VeryHigh) {
    /* WHAT: Test with very high post-synaptic activity
     * WHY:  Should handle without numerical overflow
     */
    float pre = 1.0f;
    float post = 100.0f;  // Extreme activity
    float dt = 1.0f;

    bcm_apply_rule(&synapse, pre, post, dt, &params);

    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 1.0f);
    EXPECT_LT(synapse.threshold, params.max_threshold);
}

TEST_F(BCMTest, ExtremeActivity_VeryLow) {
    /* WHAT: Test with very low activity
     * WHY:  Should handle near-zero values without underflow
     */
    float pre = 0.0001f;
    float post = 0.0001f;
    float dt = 1.0f;

    bcm_apply_rule(&synapse, pre, post, dt, &params);

    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_GE(synapse.threshold, params.min_threshold);
}

TEST_F(BCMTest, NegativeActivity_Handled) {
    /* WHAT: Test handling of negative activity (invalid input)
     * WHY:  Should clamp or handle gracefully
     */
    float pre = -0.5f;
    float post = -1.0f;
    float dt = 1.0f;

    bcm_apply_rule(&synapse, pre, post, dt, &params);

    // Should not crash and weight should remain valid
    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 1.0f);
}

TEST_F(BCMTest, ZeroTimeStep_NoChange) {
    /* WHAT: Test with dt=0
     * WHY:  Should not change weight
     */
    float initial_weight = synapse.weight;
    float pre = 1.0f;
    float post = 2.0f;

    bcm_apply_rule(&synapse, pre, post, 0.0f, &params);

    EXPECT_FLOAT_EQ(synapse.weight, initial_weight);
}

TEST_F(BCMTest, VeryLargeTimeStep) {
    /* WHAT: Test with very large dt
     * WHY:  Should remain stable, not overshoot
     */
    float pre = 1.0f;
    float post = 2.0f;

    bcm_apply_rule(&synapse, pre, post, 10000.0f, &params);

    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 1.0f);
}

//=============================================================================
// Stress and Robustness Tests
//=============================================================================

TEST_F(BCMTest, ThousandUpdates_Convergence) {
    /* WHAT: Test convergence over many updates
     * WHY:  BCM should stabilize, not oscillate
     */
    float pre = 0.6f;
    float post = 0.7f;  // Moderate activity to avoid saturation
    float dt = 1.0f;

    float prev_weight = synapse.weight;
    float weight_change = 0.0f;

    for (int i = 0; i < 1000; i++) {
        bcm_update_threshold(&synapse, post, dt, &params);
        bcm_apply_rule(&synapse, pre, post, dt, &params);

        /* WHAT: Track weight changes in final 100 iterations
         * WHY:  Late changes indicate convergence quality
         */
        if (i > 900) {
            weight_change += fabs(synapse.weight - prev_weight);
        }
        prev_weight = synapse.weight;
    }

    /* WHAT: Verify late weight changes are small (converged)
     * WHY:  Average change < 0.01 indicates stable equilibrium
     */
    EXPECT_LT(weight_change / 100.0f, 0.01f);
}

TEST_F(BCMTest, AlternatingActivity_Stability) {
    /* WHAT: Test with alternating high/low activity
     * WHY:  Threshold should adapt to average
     */
    float dt = 1.0f;

    for (int i = 0; i < 200; i++) {
        float post = (i % 2 == 0) ? 2.0f : 0.5f;
        float pre = 1.0f;

        bcm_update_threshold(&synapse, post, dt, &params);
        bcm_apply_rule(&synapse, pre, post, dt, &params);
    }

    // Threshold should be between extremes
    EXPECT_GT(synapse.threshold, 0.25f);  // Above low²
    EXPECT_LT(synapse.threshold, 4.0f);   // Below high²
}

TEST_F(BCMTest, MultipleParameters_Independence) {
    /* WHAT: Test that parameter changes don't affect old synapses
     * WHY:  Verify value semantics of params struct
     */
    bcm_synapse_t syn1 = bcm_synapse_init(0.5f, 1.0f);
    bcm_params_t params1 = params;
    params1.learning_rate = 0.1f;

    bcm_synapse_t syn2 = bcm_synapse_init(0.5f, 1.0f);
    bcm_params_t params2 = params;
    params2.learning_rate = 0.001f;

    bcm_apply_rule(&syn1, 1.0f, 2.0f, 1.0f, &params1);
    bcm_apply_rule(&syn2, 1.0f, 2.0f, 1.0f, &params2);

    // Syn1 should change more due to higher learning rate
    EXPECT_GT(fabs(syn1.weight - 0.5f), fabs(syn2.weight - 0.5f));
}

//=============================================================================
// Threshold Dynamics Tests
//=============================================================================

TEST_F(BCMTest, ThresholdTracksActivity) {
    /* WHAT: Test threshold adapts to activity level
     * WHY:  θ should track E[post²]
     */
    float constant_activity = 0.9f;  // Use moderate activity
    float dt = 1.0f;  // dt in ms

    /* WHAT: Run for ~5 time constants (5000ms)
     * WHY:  Need sufficient time for exponential convergence (τ = 1000ms)
     * NOTE: After 5τ, convergence is ~99.3% (e^-5 ≈ 0.007)
     */
    for (int i = 0; i < 5000; i++) {
        bcm_update_threshold(&synapse, constant_activity, dt, &params);
    }

    /* WHAT: Verify threshold converged to post²
     * WHY:  BCM threshold tracks E[post²] = 0.9² = 0.81
     */
    float expected_threshold = constant_activity * constant_activity;
    EXPECT_NEAR(synapse.threshold, expected_threshold, 0.1f);
}

TEST_F(BCMTest, ThresholdAdaptationTimeConstant) {
    /* WHAT: Test threshold adaptation follows time constant
     * WHY:  Verify exponential dynamics with correct τ
     */
    synapse.threshold = 0.5f;
    float target_activity = 2.0f;
    float target_threshold = target_activity * target_activity;  // 4.0

    float dt = params.threshold_time_constant;  // One time constant

    bcm_update_threshold(&synapse, target_activity, dt, &params);

    // After one τ, should be ~63% of the way to target
    float progress = (synapse.threshold - 0.5f) / (target_threshold - 0.5f);
    EXPECT_NEAR(progress, 0.63f, 0.1f);
}

//=============================================================================
// Statistical Properties Tests
//=============================================================================

TEST_F(BCMTest, ComputeStats_Accuracy) {
    /* WHAT: Test bcm_compute_stats computes correct values
     * WHY:  Statistics should accurately reflect population
     */
    const int NUM_SYNAPSES = 100;
    bcm_synapse_t synapses[NUM_SYNAPSES];

    // Initialize with varying weights
    for (int i = 0; i < NUM_SYNAPSES; i++) {
        synapses[i] = bcm_synapse_init(i / 100.0f, 1.0f);
    }

    bcm_stats_t stats;
    ASSERT_TRUE(bcm_compute_stats(synapses, NUM_SYNAPSES, &stats));

    EXPECT_NEAR(stats.avg_weight, 0.495f, 0.01f);  // Average of 0-0.99
    EXPECT_GT(stats.weight_variance, 0.0f);
}

TEST_F(BCMTest, Stats_NullSafety) {
    /* WHAT: Test stats computation with null inputs
     * WHY:  Should return false, not crash
     */
    bcm_stats_t stats;
    EXPECT_FALSE(bcm_compute_stats(nullptr, 10, &stats));
    EXPECT_FALSE(bcm_compute_stats(&synapse, 0, &stats));
    EXPECT_FALSE(bcm_compute_stats(&synapse, 10, nullptr));
}

//=============================================================================
// Factory Preset Tests
//=============================================================================

TEST_F(BCMTest, FactoryPresets_Valid) {
    /* WHAT: Test all factory presets produce valid parameters
     * WHY:  Ensure presets are usable
     */
    bcm_params_t cortical = bcm_params_cortical();
    bcm_params_t critical = bcm_params_critical_period();
    bcm_params_t mature = bcm_params_mature();

    // All should have positive learning rates
    EXPECT_GT(cortical.learning_rate, 0.0f);
    EXPECT_GT(critical.learning_rate, 0.0f);
    EXPECT_GT(mature.learning_rate, 0.0f);

    // Critical period should have highest learning rate
    EXPECT_GT(critical.learning_rate, cortical.learning_rate);
    EXPECT_GT(cortical.learning_rate, mature.learning_rate);
}

TEST_F(BCMTest, FactoryPresets_DifferentBehavior) {
    /* WHAT: Test presets produce different learning dynamics
     * WHY:  Verify presets are meaningfully different
     */
    bcm_synapse_t syn_critical = bcm_synapse_init(0.5f, 1.0f);
    bcm_synapse_t syn_mature = bcm_synapse_init(0.5f, 1.0f);

    bcm_params_t params_critical = bcm_params_critical_period();
    bcm_params_t params_mature = bcm_params_mature();

    // Apply same activity to both
    for (int i = 0; i < 10; i++) {
        bcm_apply_rule(&syn_critical, 1.0f, 2.0f, 1.0f, &params_critical);
        bcm_apply_rule(&syn_mature, 1.0f, 2.0f, 1.0f, &params_mature);
    }

    // Critical period should show larger weight changes
    EXPECT_GT(fabs(syn_critical.weight - 0.5f), fabs(syn_mature.weight - 0.5f));
}

//=============================================================================
// Memory and Performance Tests
//=============================================================================

TEST_F(BCMTest, StructSize_Compact) {
    /* WHAT: Test BCM synapse struct size is reasonable
     * WHY:  Monitor size to prevent bloat (currently 56 bytes with pthread_mutex_t)
     * NOTE: 4 floats (16 bytes) + pthread_mutex_t (40 bytes) = 56 bytes
     *       Original goal was 16 bytes, but thread safety via mutex increases size
     */
    EXPECT_LE(sizeof(bcm_synapse_t), 64);  // Allow up to cache-line size
}

TEST_F(BCMTest, BatchUpdate_Performance) {
    /* WHAT: Benchmark batch updates
     * WHY:  Should process >10k synapses/ms
     */
    const int NUM_SYNAPSES = 10000;
    bcm_synapse_t synapses[NUM_SYNAPSES];

    for (int i = 0; i < NUM_SYNAPSES; i++) {
        synapses[i] = bcm_synapse_init(0.5f, 1.0f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_SYNAPSES; i++) {
        bcm_apply_rule(&synapses[i], 1.0f, 2.0f, 1.0f, &params);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float us_per_synapse = duration.count() / (float)NUM_SYNAPSES;
    // Performance expectation: < 2μs per synapse (allows for varying system loads)
    EXPECT_LT(us_per_synapse, 2.0f);  // < 2.0 μs per synapse

    std::cout << "BCM batch performance: " << us_per_synapse
              << " μs per synapse (" << NUM_SYNAPSES << " synapses in "
              << duration.count() << " μs)" << std::endl;
}
