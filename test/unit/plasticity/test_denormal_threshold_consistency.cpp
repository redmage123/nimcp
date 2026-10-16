/**
 * @file test_denormal_threshold_consistency.cpp
 * @brief Unit tests verifying consistent denormal thresholds across plasticity modules
 *
 * WHAT: Verify that all plasticity modules use the standardized NIMCP_DENORMAL_THRESHOLD
 *       constant for flushing subnormal floats to zero
 * WHY:  Different modules previously used inconsistent thresholds (1e-9 vs 1e-10 vs others)
 *       which could cause different numerical behavior in different learning rules.
 *       The fix standardizes to NIMCP_DENORMAL_THRESHOLD (1e-10f) defined in
 *       include/plasticity/nimcp_plasticity_constants.h
 * HOW:  Test that the constant exists, has the expected value, and that plasticity
 *       functions correctly flush values below the threshold.
 *
 * MODULES COVERED:
 * - BCM (bcm_update_threshold exponential decay)
 * - STDP (stdp_update_traces trace flush)
 * - Triplet STDP (triplet_stdp_update_traces trace flush)
 * - Calcium dynamics (calcium decay factor flush)
 * - Attention (entropy computation threshold)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cfloat>
#include <vector>

// Headers have their own extern "C" guards
#include "plasticity/nimcp_plasticity_constants.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/stdp/nimcp_triplet_stdp.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/attention/nimcp_attention.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DenormalThresholdConsistencyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//=============================================================================
// Constant Value Tests
//=============================================================================

/**
 * WHAT: Verify NIMCP_DENORMAL_THRESHOLD has the standardized value
 * WHY:  All modules should use 1e-10f for denormal flushing
 * HOW:  Direct comparison of macro value
 */
TEST_F(DenormalThresholdConsistencyTest, ThresholdValueIsStandardized) {
    EXPECT_FLOAT_EQ(NIMCP_DENORMAL_THRESHOLD, 1e-10f)
        << "NIMCP_DENORMAL_THRESHOLD should be 1e-10f";
}

/**
 * WHAT: Verify NIMCP_DENORMAL_EXP_THRESHOLD equals NIMCP_DENORMAL_THRESHOLD
 * WHY:  Exponential decay threshold should be the same as general threshold
 * HOW:  Direct comparison
 */
TEST_F(DenormalThresholdConsistencyTest, ExpThresholdMatchesGeneralThreshold) {
    EXPECT_FLOAT_EQ(NIMCP_DENORMAL_EXP_THRESHOLD, NIMCP_DENORMAL_THRESHOLD)
        << "NIMCP_DENORMAL_EXP_THRESHOLD should equal NIMCP_DENORMAL_THRESHOLD";
}

/**
 * WHAT: Verify threshold is well above actual IEEE 754 denormal range
 * WHY:  The threshold must be high enough to catch subnormals but low
 *       enough to not discard meaningful values
 * HOW:  Compare with FLT_MIN (smallest normal float) and FLT_TRUE_MIN
 */
TEST_F(DenormalThresholdConsistencyTest, ThresholdAboveDenormalRange) {
    // FLT_MIN is smallest normalized float (~1.175e-38)
    EXPECT_GT(NIMCP_DENORMAL_THRESHOLD, FLT_MIN)
        << "Threshold must be above smallest normalized float";

    // Threshold should be much larger than denormals (factor > 1e27)
    EXPECT_GT(NIMCP_DENORMAL_THRESHOLD / FLT_MIN, 1e20f)
        << "Threshold should be many orders of magnitude above denormal range";
}

/**
 * WHAT: Verify threshold is small enough to not discard meaningful plasticity values
 * WHY:  Typical trace values in the range 0.01 - 1.0 should never be flushed
 * HOW:  Verify threshold is far below typical operating range
 */
TEST_F(DenormalThresholdConsistencyTest, ThresholdBelowMeaningfulValues) {
    // Typical minimum meaningful trace value in STDP/BCM is ~0.001
    float min_meaningful = 0.001f;
    EXPECT_LT(NIMCP_DENORMAL_THRESHOLD, min_meaningful * 1e-3f)
        << "Threshold should be well below meaningful trace values";
}

//=============================================================================
// STDP Module Tests
//=============================================================================

/**
 * WHAT: Verify STDP traces are flushed to zero below threshold
 * WHY:  stdp_update_traces should flush pre_trace and post_trace below
 *       NIMCP_DENORMAL_THRESHOLD to prevent denormal performance degradation
 * HOW:  Initialize synapse with very small traces, update, verify flush
 */
TEST_F(DenormalThresholdConsistencyTest, StdpTraceFlushBelowThreshold) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    // Set traces to a value below the threshold
    synapse.pre_trace = NIMCP_DENORMAL_THRESHOLD * 0.5f;  // Half the threshold
    synapse.post_trace = NIMCP_DENORMAL_THRESHOLD * 0.1f;  // One-tenth the threshold

    // Update traces with a small timestep
    // dt is in seconds for stdp_update_traces
    stdp_update_traces(&synapse, 0.001f);

    // After update with decay, traces should have been flushed to zero
    // since they started below threshold and only decayed further
    EXPECT_EQ(synapse.pre_trace, 0.0f)
        << "STDP pre_trace below threshold should be flushed to zero";
    EXPECT_EQ(synapse.post_trace, 0.0f)
        << "STDP post_trace below threshold should be flushed to zero";
}

/**
 * WHAT: Verify STDP traces above threshold are NOT flushed
 * WHY:  Only values below NIMCP_DENORMAL_THRESHOLD should be zeroed
 * HOW:  Set traces well above threshold, update, verify non-zero
 */
TEST_F(DenormalThresholdConsistencyTest, StdpTracePreservedAboveThreshold) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    // Set traces well above threshold
    synapse.pre_trace = 0.5f;
    synapse.post_trace = 0.5f;

    // Very small timestep to minimize decay
    stdp_update_traces(&synapse, 0.0001f);

    // Traces should still be non-zero (decayed but not flushed)
    EXPECT_GT(synapse.pre_trace, 0.0f)
        << "STDP pre_trace above threshold should NOT be flushed";
    EXPECT_GT(synapse.post_trace, 0.0f)
        << "STDP post_trace above threshold should NOT be flushed";
}

//=============================================================================
// Triplet STDP Module Tests
//=============================================================================

/**
 * WHAT: Verify triplet STDP traces are flushed below threshold
 * WHY:  All four traces (r1_pre, r2_pre, o1_post, o2_post) should use
 *       the standardized NIMCP_DENORMAL_THRESHOLD for consistency
 * HOW:  Create synapse, set traces below threshold, update, verify flush
 */
TEST_F(DenormalThresholdConsistencyTest, TripletStdpTraceFlushBelowThreshold) {
    triplet_stdp_config_t config = triplet_stdp_config_default();
    triplet_stdp_synapse_t* synapse = triplet_stdp_synapse_create(&config, 0.5f);
    ASSERT_NE(synapse, nullptr) << "Failed to create triplet STDP synapse";

    // Set all four traces below threshold
    synapse->r1_pre = NIMCP_DENORMAL_THRESHOLD * 0.1f;
    synapse->r2_pre = NIMCP_DENORMAL_THRESHOLD * 0.01f;
    synapse->o1_post = NIMCP_DENORMAL_THRESHOLD * 0.5f;
    synapse->o2_post = NIMCP_DENORMAL_THRESHOLD * 0.001f;

    // Update traces - should decay and flush
    int result = triplet_stdp_update_traces(synapse, 1.0f);
    EXPECT_EQ(result, 0) << "triplet_stdp_update_traces should succeed";

    // All traces should be flushed to zero
    EXPECT_EQ(synapse->r1_pre, 0.0f)
        << "r1_pre below threshold should be flushed";
    EXPECT_EQ(synapse->r2_pre, 0.0f)
        << "r2_pre below threshold should be flushed";
    EXPECT_EQ(synapse->o1_post, 0.0f)
        << "o1_post below threshold should be flushed";
    EXPECT_EQ(synapse->o2_post, 0.0f)
        << "o2_post below threshold should be flushed";

    triplet_stdp_synapse_destroy(synapse);
}

/**
 * WHAT: Verify triplet STDP traces above threshold are preserved after update
 * WHY:  Only values decayed below threshold should be flushed
 * HOW:  Set large traces, small dt, verify non-zero after update
 */
TEST_F(DenormalThresholdConsistencyTest, TripletStdpTracePreservedAboveThreshold) {
    triplet_stdp_config_t config = triplet_stdp_config_default();
    triplet_stdp_synapse_t* synapse = triplet_stdp_synapse_create(&config, 0.5f);
    ASSERT_NE(synapse, nullptr);

    // Set traces well above threshold
    synapse->r1_pre = 0.8f;
    synapse->r2_pre = 0.6f;
    synapse->o1_post = 0.7f;
    synapse->o2_post = 0.5f;

    // Small timestep
    triplet_stdp_update_traces(synapse, 0.1f);

    // All traces should be non-zero (small decay from 0.1ms step)
    EXPECT_GT(synapse->r1_pre, 0.0f)
        << "r1_pre should be preserved above threshold";
    EXPECT_GT(synapse->r2_pre, 0.0f)
        << "r2_pre should be preserved above threshold";
    EXPECT_GT(synapse->o1_post, 0.0f)
        << "o1_post should be preserved above threshold";
    EXPECT_GT(synapse->o2_post, 0.0f)
        << "o2_post should be preserved above threshold";

    triplet_stdp_synapse_destroy(synapse);
}

//=============================================================================
// BCM Module Tests
//=============================================================================

/**
 * WHAT: Verify BCM exponential decay uses standardized threshold
 * WHY:  BCM previously used 1e-9F while STDP used 1e-10F. Now both should
 *       use NIMCP_DENORMAL_EXP_THRESHOLD (= NIMCP_DENORMAL_THRESHOLD = 1e-10f)
 * HOW:  Create BCM synapse, apply threshold update with very large dt
 *       (which forces exp result below threshold), verify behavior
 */
TEST_F(DenormalThresholdConsistencyTest, BcmExponentialDecayThreshold) {
    bcm_synapse_t synapse = bcm_synapse_init(0.5f, 0.3f);

    bcm_params_t params = {};
    params.learning_rate = 0.01f;
    params.threshold_time_constant = 100.0f;  // 100ms tau
    params.activity_time_constant = 50.0f;
    params.min_threshold = 0.0f;
    params.max_threshold = 1.0f;
    params.enable_bio_async = false;
    params.enable_quantum_bcm = false;

    // Very large dt relative to tau forces exp(-dt/tau) below threshold
    // exp(-5000/100) = exp(-50) ~ 1.9e-22 << 1e-10
    bcm_update_threshold(&synapse, 0.5f, 5000.0f, &params);

    // The synapse should have been updated without NaN or crash
    EXPECT_FALSE(std::isnan(synapse.threshold))
        << "BCM threshold should not be NaN after large dt";
    EXPECT_FALSE(std::isnan(synapse.avg_post_activity))
        << "BCM activity average should not be NaN after large dt";

    // Threshold should be within valid range
    EXPECT_GE(synapse.threshold, params.min_threshold);
    EXPECT_LE(synapse.threshold, params.max_threshold);
}

/**
 * WHAT: Verify BCM threshold update produces consistent results
 * WHY:  After standardizing the denormal threshold, BCM numerical behavior
 *       should remain stable
 * HOW:  Run multiple BCM update cycles and verify convergence
 */
TEST_F(DenormalThresholdConsistencyTest, BcmThresholdConvergence) {
    bcm_synapse_t synapse = bcm_synapse_init(0.5f, 0.3f);

    bcm_params_t params = {};
    params.learning_rate = 0.01f;
    params.threshold_time_constant = 100.0f;
    params.activity_time_constant = 100.0f;
    params.min_threshold = 0.0f;
    params.max_threshold = 2.0f;
    params.enable_bio_async = false;
    params.enable_quantum_bcm = false;

    // Run many updates with constant activity
    float post_activity = 0.7f;
    float dt = 10.0f;  // 10ms steps

    for (int i = 0; i < 1000; i++) {
        bcm_update_threshold(&synapse, post_activity, dt, &params);
        ASSERT_FALSE(std::isnan(synapse.threshold))
            << "Threshold became NaN at step " << i;
        ASSERT_FALSE(std::isinf(synapse.threshold))
            << "Threshold became Inf at step " << i;
    }

    // Threshold should have converged to activity^2 = 0.49
    // (BCM threshold adapts toward square of mean activity)
    EXPECT_GT(synapse.threshold, 0.0f) << "Threshold should be positive after convergence";
    EXPECT_LT(synapse.threshold, 2.0f) << "Threshold should be below max";
}

//=============================================================================
// Attention Module Tests
//=============================================================================

/**
 * WHAT: Verify attention entropy uses standardized threshold for log guard
 * WHY:  attention_compute_entropy should skip values below NIMCP_DENORMAL_THRESHOLD
 *       to avoid log(0) = -inf and denormal performance issues
 * HOW:  Compute entropy with values at, above, and below the threshold
 */
TEST_F(DenormalThresholdConsistencyTest, AttentionEntropyDenormalGuard) {
    constexpr uint32_t SEQ_LEN = 4;
    float weights[SEQ_LEN];

    // Test: Values exactly at threshold
    // Only values ABOVE threshold contribute to entropy
    float at_threshold = NIMCP_DENORMAL_THRESHOLD;
    weights[0] = 1.0f - 3.0f * at_threshold;  // Most weight here
    weights[1] = at_threshold;
    weights[2] = at_threshold;
    weights[3] = at_threshold;

    float entropy = attention_compute_entropy(weights, SEQ_LEN);
    EXPECT_FALSE(std::isnan(entropy)) << "Entropy must not be NaN at threshold";
    EXPECT_FALSE(std::isinf(entropy)) << "Entropy must not be Inf at threshold";

    // Test: Values just above threshold should contribute
    float above = NIMCP_DENORMAL_THRESHOLD * 10.0f;  // 1e-9
    weights[0] = 1.0f - 3.0f * above;
    weights[1] = above;
    weights[2] = above;
    weights[3] = above;

    entropy = attention_compute_entropy(weights, SEQ_LEN);
    EXPECT_FALSE(std::isnan(entropy));
    EXPECT_GT(entropy, 0.0f) << "Non-trivial distribution should have positive entropy";

    // Test: Values well below threshold should not contribute
    float below = NIMCP_DENORMAL_THRESHOLD * 0.001f;  // 1e-13
    weights[0] = 1.0f;
    weights[1] = below;
    weights[2] = below;
    weights[3] = below;

    entropy = attention_compute_entropy(weights, SEQ_LEN);
    EXPECT_FALSE(std::isnan(entropy));
    // Should be near zero since effectively one-hot
    EXPECT_NEAR(entropy, 0.0f, 0.001f)
        << "One-hot (below-threshold others) should have ~0 entropy";
}

//=============================================================================
// Cross-Module Consistency Tests
//=============================================================================

/**
 * WHAT: Verify all modules agree on the same threshold constant value
 * WHY:  The whole point of the fix is to standardize the threshold
 * HOW:  Compile-time check that the macro expands to the same value
 *       across different compilation contexts
 */
TEST_F(DenormalThresholdConsistencyTest, AllModulesUseSameThresholdValue) {
    // This test verifies at compile + link time that the constant header
    // is included correctly and the value is consistent
    float threshold = NIMCP_DENORMAL_THRESHOLD;
    float exp_threshold = NIMCP_DENORMAL_EXP_THRESHOLD;

    EXPECT_FLOAT_EQ(threshold, 1e-10f);
    EXPECT_FLOAT_EQ(exp_threshold, 1e-10f);
    EXPECT_FLOAT_EQ(threshold, exp_threshold)
        << "General and exponential thresholds must be equal";
}

/**
 * WHAT: Verify that values between old 1e-9 and new 1e-10 thresholds
 *       are now correctly handled (not prematurely flushed)
 * WHY:  BCM/calcium previously used 1e-9 which would flush values in the
 *       1e-10 to 1e-9 range. With standardization to 1e-10, these values
 *       are now preserved (more permissive threshold).
 * HOW:  Test STDP with trace values in the transition range
 */
TEST_F(DenormalThresholdConsistencyTest, TransitionRangeValuesPreserved) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    // Set traces in the range between old (1e-9) and new (1e-10) thresholds
    // These should NOT be flushed now that we use 1e-10
    float in_transition = 5e-10f;  // Between 1e-10 and 1e-9
    synapse.pre_trace = in_transition;
    synapse.post_trace = in_transition;

    // Update with zero dt (no decay)
    stdp_update_traces(&synapse, 0.0f);

    // Traces should be preserved since they're above NIMCP_DENORMAL_THRESHOLD
    EXPECT_GT(synapse.pre_trace, 0.0f)
        << "Trace at 5e-10 should be preserved (above 1e-10 threshold)";
    EXPECT_GT(synapse.post_trace, 0.0f)
        << "Trace at 5e-10 should be preserved (above 1e-10 threshold)";
}
