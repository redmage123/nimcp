/**
 * @file test_denormal_threshold_regression.cpp
 * @brief Regression tests for denormal threshold standardization
 *
 * WHAT: Ensure that the standardized NIMCP_DENORMAL_THRESHOLD does not
 *       cause regressions in plasticity learning behavior
 * WHY:  Changing BCM/calcium from 1e-9 to 1e-10 is more permissive
 *       (preserves smaller values). This could theoretically affect
 *       learning dynamics if previously-flushed values now contribute.
 *       These tests verify that learning remains stable and correct.
 * HOW:  Run extended learning simulations with each plasticity rule
 *       and verify convergence, numerical stability, and weight bounds.
 *
 * REGRESSION PROTECTS AGAINST:
 * - BCM threshold convergence changes (1e-9 -> 1e-10 transition)
 * - STDP weight drift from trace accumulation at new threshold
 * - Triplet STDP frequency-dependent behavior changes
 * - Numerical instability from preserved near-zero values
 * - Denormal performance degradation (values should still be flushed)
 *
 * @date 2026-02-15
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cfloat>
#include <vector>
#include <numeric>
#include <chrono>

// Headers have their own extern "C" guards
#include "plasticity/nimcp_plasticity_constants.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/stdp/nimcp_triplet_stdp.h"
#include "plasticity/bcm/nimcp_bcm.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DenormalThresholdRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//=============================================================================
// STDP Extended Simulation Tests
//=============================================================================

/**
 * WHAT: Verify STDP learning is stable over many iterations with new threshold
 * WHY:  Extended simulation catches accumulation errors from threshold change
 * HOW:  Run 10000 STDP update cycles, verify weight stays bounded and stable
 */
TEST_F(DenormalThresholdRegressionTest, StdpExtendedStability) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    synapse.weight = 0.5f;
    synapse.w_max = 1.0f;
    synapse.w_min = 0.0f;
    synapse.learning_rate = 0.01f;

    float dt = 0.001f;  // 1ms timestep in seconds

    for (int i = 0; i < 10000; i++) {
        stdp_update_traces(&synapse, dt);

        // Occasionally inject spikes
        if (i % 50 == 0) {
            float time_ms = static_cast<float>(i) * 1.0f;
            stdp_pre_spike(&synapse, time_ms);
        }
        if (i % 53 == 0) {  // Slight offset for STDP timing
            float time_ms = static_cast<float>(i) * 1.0f;
            stdp_post_spike(&synapse, time_ms);
        }

        // Verify numerical stability
        ASSERT_FALSE(std::isnan(synapse.weight))
            << "Weight became NaN at step " << i;
        ASSERT_FALSE(std::isinf(synapse.weight))
            << "Weight became Inf at step " << i;
        ASSERT_GE(synapse.weight, synapse.w_min)
            << "Weight below min at step " << i;
        ASSERT_LE(synapse.weight, synapse.w_max)
            << "Weight above max at step " << i;

        // Traces should never be NaN
        ASSERT_FALSE(std::isnan(synapse.pre_trace))
            << "Pre trace NaN at step " << i;
        ASSERT_FALSE(std::isnan(synapse.post_trace))
            << "Post trace NaN at step " << i;
    }
}

/**
 * WHAT: Verify STDP trace decay properly reaches zero
 * WHY:  With the new threshold at 1e-10, traces should eventually reach zero
 *       via flushing, not asymptotically approach it
 * HOW:  Set a trace, let it decay for many steps without new spikes
 */
TEST_F(DenormalThresholdRegressionTest, StdpTraceDecayToZero) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    // Inject a single pre-spike to set trace
    synapse.pre_trace = 1.0f;
    synapse.post_trace = 1.0f;

    float dt = 0.001f;  // 1ms
    int steps_to_zero_pre = -1;
    int steps_to_zero_post = -1;

    for (int i = 0; i < 100000; i++) {
        stdp_update_traces(&synapse, dt);

        if (synapse.pre_trace == 0.0f && steps_to_zero_pre < 0) {
            steps_to_zero_pre = i;
        }
        if (synapse.post_trace == 0.0f && steps_to_zero_post < 0) {
            steps_to_zero_post = i;
        }

        if (steps_to_zero_pre >= 0 && steps_to_zero_post >= 0) break;
    }

    // Traces must eventually reach exactly zero (flushed)
    EXPECT_GE(steps_to_zero_pre, 0)
        << "Pre trace should reach zero via denormal flush";
    EXPECT_GE(steps_to_zero_post, 0)
        << "Post trace should reach zero via denormal flush";
    EXPECT_EQ(synapse.pre_trace, 0.0f)
        << "Pre trace should be exactly zero";
    EXPECT_EQ(synapse.post_trace, 0.0f)
        << "Post trace should be exactly zero";
}

//=============================================================================
// Triplet STDP Extended Simulation Tests
//=============================================================================

/**
 * WHAT: Verify triplet STDP learning is stable over many iterations
 * WHY:  Extended simulation catches numerical issues from threshold change
 * HOW:  Run 10000 triplet STDP update cycles with periodic spikes
 */
TEST_F(DenormalThresholdRegressionTest, TripletStdpExtendedStability) {
    triplet_stdp_config_t config = triplet_stdp_config_default();
    triplet_stdp_synapse_t* synapse = triplet_stdp_synapse_create(&config, 0.5f);
    ASSERT_NE(synapse, nullptr);

    for (int i = 0; i < 10000; i++) {
        float dt = 1.0f;  // 1ms
        triplet_stdp_update_traces(synapse, dt);

        // Periodic spikes
        float time_ms = static_cast<float>(i);
        if (i % 50 == 0) {
            triplet_stdp_pre_spike(synapse, time_ms);
        }
        if (i % 55 == 0) {
            triplet_stdp_post_spike(synapse, time_ms);
        }

        // Verify stability
        ASSERT_FALSE(std::isnan(synapse->weight))
            << "Weight NaN at step " << i;
        ASSERT_GE(synapse->weight, synapse->w_min)
            << "Weight below min at step " << i;
        ASSERT_LE(synapse->weight, synapse->w_max)
            << "Weight above max at step " << i;

        // All four traces should be valid
        ASSERT_FALSE(std::isnan(synapse->r1_pre));
        ASSERT_FALSE(std::isnan(synapse->r2_pre));
        ASSERT_FALSE(std::isnan(synapse->o1_post));
        ASSERT_FALSE(std::isnan(synapse->o2_post));
        ASSERT_GE(synapse->r1_pre, 0.0f);
        ASSERT_GE(synapse->r2_pre, 0.0f);
        ASSERT_GE(synapse->o1_post, 0.0f);
        ASSERT_GE(synapse->o2_post, 0.0f);
    }

    triplet_stdp_synapse_destroy(synapse);
}

/**
 * WHAT: Verify all four triplet STDP traces decay to exactly zero
 * WHY:  With NIMCP_DENORMAL_THRESHOLD flush, traces should not asymptotically
 *       approach zero but be explicitly zeroed when below threshold
 * HOW:  Set all traces to 1.0, decay without new spikes, verify exact zero
 */
TEST_F(DenormalThresholdRegressionTest, TripletStdpAllTracesDecayToZero) {
    triplet_stdp_config_t config = triplet_stdp_config_default();
    triplet_stdp_synapse_t* synapse = triplet_stdp_synapse_create(&config, 0.5f);
    ASSERT_NE(synapse, nullptr);

    // Set all traces to 1.0
    synapse->r1_pre = 1.0f;
    synapse->r2_pre = 1.0f;
    synapse->o1_post = 1.0f;
    synapse->o2_post = 1.0f;

    // Decay for a long time
    for (int i = 0; i < 100000; i++) {
        triplet_stdp_update_traces(synapse, 1.0f);  // 1ms steps

        // Check if all reached zero
        if (synapse->r1_pre == 0.0f && synapse->r2_pre == 0.0f &&
            synapse->o1_post == 0.0f && synapse->o2_post == 0.0f) {
            break;
        }
    }

    EXPECT_EQ(synapse->r1_pre, 0.0f)
        << "r1_pre should reach exactly zero";
    EXPECT_EQ(synapse->r2_pre, 0.0f)
        << "r2_pre should reach exactly zero";
    EXPECT_EQ(synapse->o1_post, 0.0f)
        << "o1_post should reach exactly zero";
    EXPECT_EQ(synapse->o2_post, 0.0f)
        << "o2_post should reach exactly zero";

    triplet_stdp_synapse_destroy(synapse);
}

//=============================================================================
// BCM Extended Simulation Tests
//=============================================================================

/**
 * WHAT: Verify BCM threshold convergence is stable after threshold change
 * WHY:  BCM previously flushed exp results below 1e-9, now 1e-10.
 *       This more permissive threshold should not affect convergence.
 * HOW:  Run 10000 BCM update cycles, verify threshold converges
 */
TEST_F(DenormalThresholdRegressionTest, BcmThresholdConvergenceStable) {
    bcm_synapse_t synapse = bcm_synapse_init(0.5f, 0.3f);

    bcm_params_t params = {};
    params.learning_rate = 0.01f;
    params.threshold_time_constant = 100.0f;
    params.activity_time_constant = 100.0f;
    params.min_threshold = 0.0f;
    params.max_threshold = 2.0f;
    params.enable_bio_async = false;
    params.enable_quantum_bcm = false;

    float post_activity = 0.7f;
    float dt = 1.0f;  // 1ms

    std::vector<float> threshold_history;
    threshold_history.reserve(10000);

    for (int i = 0; i < 10000; i++) {
        bcm_update_threshold(&synapse, post_activity, dt, &params);
        threshold_history.push_back(synapse.threshold);

        ASSERT_FALSE(std::isnan(synapse.threshold))
            << "Threshold NaN at step " << i;
        ASSERT_FALSE(std::isinf(synapse.threshold))
            << "Threshold Inf at step " << i;
        ASSERT_GE(synapse.threshold, params.min_threshold);
        ASSERT_LE(synapse.threshold, params.max_threshold);
    }

    // Verify convergence: last 100 values should be stable
    if (threshold_history.size() >= 200) {
        float last_100_mean = 0.0f;
        for (size_t i = threshold_history.size() - 100; i < threshold_history.size(); i++) {
            last_100_mean += threshold_history[i];
        }
        last_100_mean /= 100.0f;

        float last_100_var = 0.0f;
        for (size_t i = threshold_history.size() - 100; i < threshold_history.size(); i++) {
            float diff = threshold_history[i] - last_100_mean;
            last_100_var += diff * diff;
        }
        last_100_var /= 100.0f;

        // Variance should be very small (converged)
        EXPECT_LT(last_100_var, 0.001f)
            << "BCM threshold should converge (low variance in last 100 steps)";
    }
}

/**
 * WHAT: Verify BCM handles extreme dt values (denormal edge case)
 * WHY:  Very large dt/tau ratios produce exp results far below threshold.
 *       The flush should prevent denormal propagation.
 * HOW:  Test with dt values that produce exp(-x) in different ranges
 */
TEST_F(DenormalThresholdRegressionTest, BcmExtremeDtValues) {
    bcm_params_t params = {};
    params.learning_rate = 0.01f;
    params.threshold_time_constant = 10.0f;   // Small tau
    params.activity_time_constant = 10.0f;
    params.min_threshold = 0.0f;
    params.max_threshold = 2.0f;
    params.enable_bio_async = false;
    params.enable_quantum_bcm = false;

    float post_activity = 0.5f;

    // Test a range of dt values
    float dt_values[] = {0.001f, 0.1f, 1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f};

    for (float dt : dt_values) {
        bcm_synapse_t synapse = bcm_synapse_init(0.5f, 0.3f);

        bcm_update_threshold(&synapse, post_activity, dt, &params);

        EXPECT_FALSE(std::isnan(synapse.threshold))
            << "Threshold NaN for dt=" << dt;
        EXPECT_FALSE(std::isinf(synapse.threshold))
            << "Threshold Inf for dt=" << dt;
        EXPECT_GE(synapse.threshold, params.min_threshold)
            << "Threshold below min for dt=" << dt;
        EXPECT_LE(synapse.threshold, params.max_threshold)
            << "Threshold above max for dt=" << dt;
    }
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

/**
 * WHAT: Verify no performance regression from denormal threshold change
 * WHY:  The whole purpose of denormal flushing is performance. Changing
 *       the threshold should not introduce denormal performance issues.
 * HOW:  Run many trace updates and measure time. Denormal-affected code
 *       is typically 10-100x slower, so we check for gross slowdowns.
 */
TEST_F(DenormalThresholdRegressionTest, NoPerformanceDegradation) {
    constexpr int NUM_SYNAPSES = 100;
    constexpr int NUM_ITERATIONS = 10000;

    // Create synapses
    std::vector<stdp_synapse_t> synapses(NUM_SYNAPSES);
    for (auto& s : synapses) {
        stdp_synapse_init(&s);
        s.pre_trace = 1.0f;
        s.post_trace = 1.0f;
    }

    float dt = 0.001f;

    auto start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        for (auto& s : synapses) {
            stdp_update_traces(&s, dt);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Sanity check: 100 synapses * 10000 iterations should complete in < 5 seconds
    // even on slow hardware. Denormal-affected code would be 10-100x slower.
    EXPECT_LT(duration_us, 5000000)  // 5 seconds
        << "STDP trace update performance regression detected";

    // Verify all traces decayed to zero (flushed, not denormal)
    for (const auto& s : synapses) {
        EXPECT_EQ(s.pre_trace, 0.0f)
            << "All traces should be exactly zero (flushed) after long decay";
        EXPECT_EQ(s.post_trace, 0.0f);
    }
}

/**
 * WHAT: Verify BCM + STDP combined learning is numerically stable
 * WHY:  In real usage, BCM and STDP operate on the same synapses.
 *       The standardized threshold should work correctly for both.
 * HOW:  Simulate a simplified plasticity pipeline with both rules
 */
TEST_F(DenormalThresholdRegressionTest, CombinedBcmStdpStability) {
    // BCM synapse for threshold
    bcm_synapse_t bcm_syn = bcm_synapse_init(0.5f, 0.3f);
    bcm_params_t bcm_params = {};
    bcm_params.learning_rate = 0.01f;
    bcm_params.threshold_time_constant = 100.0f;
    bcm_params.activity_time_constant = 100.0f;
    bcm_params.min_threshold = 0.0f;
    bcm_params.max_threshold = 2.0f;
    bcm_params.enable_bio_async = false;
    bcm_params.enable_quantum_bcm = false;

    // STDP synapse for timing
    stdp_synapse_t stdp_syn;
    stdp_synapse_init(&stdp_syn);

    float dt = 1.0f;  // 1ms

    for (int i = 0; i < 5000; i++) {
        // BCM threshold update
        float post_activity = 0.3f + 0.4f * sinf(static_cast<float>(i) * 0.01f);
        bcm_update_threshold(&bcm_syn, post_activity, dt, &bcm_params);

        // STDP trace update
        stdp_update_traces(&stdp_syn, dt / 1000.0f);  // dt in seconds for STDP

        // Periodic spikes
        if (i % 100 == 0) {
            stdp_pre_spike(&stdp_syn, static_cast<float>(i));
        }
        if (i % 110 == 0) {
            stdp_post_spike(&stdp_syn, static_cast<float>(i));
        }

        // Both should remain stable
        ASSERT_FALSE(std::isnan(bcm_syn.threshold));
        ASSERT_FALSE(std::isnan(stdp_syn.weight));
        ASSERT_FALSE(std::isnan(stdp_syn.pre_trace));
        ASSERT_FALSE(std::isnan(stdp_syn.post_trace));
    }

    // Final state should be valid
    EXPECT_GE(bcm_syn.threshold, 0.0f);
    EXPECT_LE(bcm_syn.threshold, 2.0f);
    EXPECT_GE(stdp_syn.weight, 0.0f);
    EXPECT_LE(stdp_syn.weight, 1.0f);
}
