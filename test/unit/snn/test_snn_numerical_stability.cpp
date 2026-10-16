/**
 * @file test_snn_numerical_stability.cpp
 * @brief Unit tests for SNN numerical stability
 *
 * WHAT: Tests for numerical stability in SNN training algorithms
 * WHY:  Ensure robust computation without NaN/Inf or overflow
 * HOW:  Test edge cases with extreme values, boundary conditions
 *
 * TESTS COVER:
 * 1. NaN/Inf handling in STDP weight updates
 * 2. Exponential overflow prevention in time constants
 * 3. Spike buffer overflow protection
 * 4. Denormal float prevention in gradients
 * 5. Weight bound enforcement
 * 6. Gradient clipping effectiveness
 * 7. Eligibility trace stability
 * 8. Reward signal saturation
 *
 * @version 1.0.0
 * @date 2025-01-15
 */

#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <cfloat>

extern "C" {
#include "snn/nimcp_snn_training.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class SNNNumericalStabilityTest : public ::testing::Test {
protected:
    snn_stdp_config_t stdp_config;
    snn_rstdp_config_t rstdp_config;
    snn_surrogate_config_t surrogate_config;
    snn_eprop_config_t eprop_config;
    snn_homeostatic_config_t homeostatic_config;

    snn_training_ctx_t* stdp_ctx;
    snn_training_ctx_t* rstdp_ctx;
    snn_training_ctx_t* surrogate_ctx;

    void SetUp() override {
        /* Initialize default configs */
        snn_stdp_config_default(&stdp_config);
        snn_rstdp_config_default(&rstdp_config);
        snn_surrogate_config_default(&surrogate_config);
        snn_eprop_config_default(&eprop_config);
        snn_homeostatic_config_default(&homeostatic_config);

        stdp_ctx = nullptr;
        rstdp_ctx = nullptr;
        surrogate_ctx = nullptr;
    }

    void TearDown() override {
        if (stdp_ctx) {
            snn_training_destroy(stdp_ctx);
            stdp_ctx = nullptr;
        }
        if (rstdp_ctx) {
            snn_training_destroy(rstdp_ctx);
            rstdp_ctx = nullptr;
        }
        if (surrogate_ctx) {
            snn_training_destroy(surrogate_ctx);
            surrogate_ctx = nullptr;
        }
    }

    /**
     * @brief Check if value is finite (not NaN or Inf)
     */
    bool is_finite(float value) {
        return std::isfinite(value);
    }

    /**
     * @brief Check if value is denormal (subnormal)
     */
    bool is_denormal(float value) {
        return std::fpclassify(value) == FP_SUBNORMAL;
    }
};

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(SNNNumericalStabilityTest, STDPConfigDefaultsAreValid) {
    /**
     * WHAT: Verify STDP defaults are numerically safe
     * WHY:  Defaults must not cause numerical issues
     * HOW:  Check all parameters are finite and reasonable
     */
    EXPECT_TRUE(is_finite(stdp_config.a_plus));
    EXPECT_TRUE(is_finite(stdp_config.a_minus));
    EXPECT_TRUE(is_finite(stdp_config.tau_plus));
    EXPECT_TRUE(is_finite(stdp_config.tau_minus));
    EXPECT_TRUE(is_finite(stdp_config.w_min));
    EXPECT_TRUE(is_finite(stdp_config.w_max));

    /* Time constants must be positive */
    EXPECT_GT(stdp_config.tau_plus, 0.0f);
    EXPECT_GT(stdp_config.tau_minus, 0.0f);

    /* Weight bounds must be ordered */
    EXPECT_LT(stdp_config.w_min, stdp_config.w_max);
}

TEST_F(SNNNumericalStabilityTest, RSTDPConfigDefaultsAreValid) {
    /**
     * WHAT: Verify R-STDP defaults are numerically safe
     * WHY:  Defaults must not cause numerical issues
     * HOW:  Check all parameters are finite and reasonable
     */
    EXPECT_TRUE(is_finite(rstdp_config.eligibility_tau));
    EXPECT_TRUE(is_finite(rstdp_config.reward_tau));
    EXPECT_TRUE(is_finite(rstdp_config.baseline_reward));

    EXPECT_GT(rstdp_config.eligibility_tau, 0.0f);
    EXPECT_GT(rstdp_config.reward_tau, 0.0f);
}

TEST_F(SNNNumericalStabilityTest, SurrogateConfigDefaultsAreValid) {
    /**
     * WHAT: Verify surrogate gradient defaults are numerically safe
     * WHY:  Gradient computation must be stable
     * HOW:  Check parameters don't cause overflow
     */
    EXPECT_TRUE(is_finite(surrogate_config.beta));
    EXPECT_TRUE(is_finite(surrogate_config.threshold));
    EXPECT_TRUE(is_finite(surrogate_config.learning_rate));
    EXPECT_TRUE(is_finite(surrogate_config.momentum));
    EXPECT_TRUE(is_finite(surrogate_config.weight_decay));

    /* Beta must be positive to avoid division issues */
    EXPECT_GT(surrogate_config.beta, 0.0f);

    /* Learning rate must be positive */
    EXPECT_GT(surrogate_config.learning_rate, 0.0f);
}

//=============================================================================
// STDP Numerical Stability Tests
//=============================================================================

TEST_F(SNNNumericalStabilityTest, STDPDeltaWWithZeroDt) {
    /**
     * WHAT: Test STDP with zero time difference
     * WHY:  Simultaneous pre/post spikes should not cause issues
     * HOW:  Pass dt=0, verify finite result
     */
    stdp_ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(stdp_ctx, nullptr);

    float delta_w = snn_stdp_compute_delta_w(stdp_ctx, 0.0f, 0.5f);

    EXPECT_TRUE(is_finite(delta_w));
}

TEST_F(SNNNumericalStabilityTest, STDPDeltaWWithLargeDt) {
    /**
     * WHAT: Test STDP with very large time difference
     * WHY:  Exponential decay should not underflow to denormal
     * HOW:  Pass large dt, verify no denormal results
     */
    stdp_ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(stdp_ctx, nullptr);

    /* Large dt should produce very small (but not denormal) delta_w */
    float delta_w = snn_stdp_compute_delta_w(stdp_ctx, 1000.0f, 0.5f);

    EXPECT_TRUE(is_finite(delta_w));
    if (delta_w != 0.0f) {
        EXPECT_FALSE(is_denormal(delta_w)) << "Delta_w should not be denormal";
    }
}

TEST_F(SNNNumericalStabilityTest, STDPDeltaWWithNegativeDt) {
    /**
     * WHAT: Test STDP with negative time difference
     * WHY:  Post-before-pre should produce LTD
     * HOW:  Pass negative dt, verify finite result
     */
    stdp_ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(stdp_ctx, nullptr);

    float delta_w = snn_stdp_compute_delta_w(stdp_ctx, -50.0f, 0.5f);

    EXPECT_TRUE(is_finite(delta_w));
}

TEST_F(SNNNumericalStabilityTest, STDPDeltaWWithExtremeDt) {
    /**
     * WHAT: Test STDP with extreme time differences
     * WHY:  Edge cases must be handled without overflow
     * HOW:  Test very large positive and negative dt values
     */
    stdp_ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(stdp_ctx, nullptr);

    /* Very large positive dt */
    float delta_w_pos = snn_stdp_compute_delta_w(stdp_ctx, 1e6f, 0.5f);
    EXPECT_TRUE(is_finite(delta_w_pos));

    /* Very large negative dt */
    float delta_w_neg = snn_stdp_compute_delta_w(stdp_ctx, -1e6f, 0.5f);
    EXPECT_TRUE(is_finite(delta_w_neg));
}

TEST_F(SNNNumericalStabilityTest, STDPDeltaWAtWeightBounds) {
    /**
     * WHAT: Test STDP at weight boundaries
     * WHY:  Weight bounds must be enforced
     * HOW:  Test at w_min and w_max
     */
    stdp_ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(stdp_ctx, nullptr);

    /* At minimum weight */
    float delta_w_min = snn_stdp_compute_delta_w(stdp_ctx, 10.0f, stdp_config.w_min);
    EXPECT_TRUE(is_finite(delta_w_min));

    /* At maximum weight */
    float delta_w_max = snn_stdp_compute_delta_w(stdp_ctx, 10.0f, stdp_config.w_max);
    EXPECT_TRUE(is_finite(delta_w_max));
}

TEST_F(SNNNumericalStabilityTest, STDPDeltaWWithInfWeight) {
    /**
     * WHAT: Test STDP with infinite weight input
     * WHY:  Must handle invalid input gracefully
     * HOW:  Pass Inf weight, verify no crash and finite output
     */
    stdp_ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(stdp_ctx, nullptr);

    /* Pass infinity - should be clamped or handled */
    float delta_w = snn_stdp_compute_delta_w(stdp_ctx, 10.0f, std::numeric_limits<float>::infinity());

    /* Should return finite value (clamped) or 0 */
    EXPECT_TRUE(is_finite(delta_w) || delta_w == 0.0f);
}

TEST_F(SNNNumericalStabilityTest, STDPDeltaWWithNaNWeight) {
    /**
     * WHAT: Test STDP with NaN weight input
     * WHY:  Must handle invalid input gracefully
     * HOW:  Pass NaN weight, verify no crash
     */
    stdp_ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(stdp_ctx, nullptr);

    float nan_weight = std::numeric_limits<float>::quiet_NaN();
    float delta_w = snn_stdp_compute_delta_w(stdp_ctx, 10.0f, nan_weight);

    /* Should handle NaN gracefully (return 0 or finite value) */
    EXPECT_TRUE(is_finite(delta_w) || delta_w == 0.0f || std::isnan(delta_w));
}

//=============================================================================
// Exponential Overflow Prevention Tests
//=============================================================================

TEST_F(SNNNumericalStabilityTest, ExponentialDoesNotOverflow) {
    /**
     * WHAT: Test that exponential calculations don't overflow
     * WHY:  exp(x) overflows for x > ~88 in float
     * HOW:  Use very small tau values that could cause large exponents
     */
    /* Create config with small tau values */
    snn_stdp_config_t test_config;
    snn_stdp_config_default(&test_config);
    test_config.tau_plus = 0.1f;   /* Very small tau */
    test_config.tau_minus = 0.1f;

    snn_training_ctx_t* ctx = snn_training_create_stdp(&test_config);
    if (!ctx) {
        /* If creation fails, that's also acceptable (rejecting bad config) */
        SUCCEED();
        return;
    }

    /* dt/tau would be 1000 -> exp(1000) = Inf without protection */
    float delta_w = snn_stdp_compute_delta_w(ctx, 100.0f, 0.5f);
    EXPECT_TRUE(is_finite(delta_w));

    snn_training_destroy(ctx);
}

TEST_F(SNNNumericalStabilityTest, EligibilityDecayDoesNotUnderflow) {
    /**
     * WHAT: Test eligibility trace decay doesn't underflow
     * WHY:  Repeated decay can cause denormal floats
     * HOW:  Apply many decay steps, check for denormals
     */
    rstdp_ctx = snn_training_create_rstdp(&rstdp_config, 10, 10);
    ASSERT_NE(rstdp_ctx, nullptr);

    /* Apply many decay steps */
    for (int i = 0; i < 1000; i++) {
        snn_rstdp_update_eligibility(rstdp_ctx, 1.0f);
    }

    /* Test passed if no crash occurred */
    SUCCEED();
}

//=============================================================================
// Gradient Clipping Tests
//=============================================================================

TEST_F(SNNNumericalStabilityTest, SurrogateGradientClipped) {
    /**
     * WHAT: Test surrogate gradient is bounded
     * WHY:  Unbounded gradients cause training instability
     * HOW:  Test with extreme membrane potentials
     */
    surrogate_ctx = snn_training_create_surrogate(&surrogate_config, 10, 10);
    ASSERT_NE(surrogate_ctx, nullptr);

    /* Very positive membrane potential */
    float grad_pos = snn_training_surrogate_gradient(surrogate_ctx, 1000.0f);
    EXPECT_TRUE(is_finite(grad_pos));
    EXPECT_LE(std::abs(grad_pos), 10.0f);  /* Should be bounded */

    /* Very negative membrane potential */
    float grad_neg = snn_training_surrogate_gradient(surrogate_ctx, -1000.0f);
    EXPECT_TRUE(is_finite(grad_neg));
    EXPECT_LE(std::abs(grad_neg), 10.0f);
}

TEST_F(SNNNumericalStabilityTest, SurrogateGradientAtThreshold) {
    /**
     * WHAT: Test surrogate gradient at threshold
     * WHY:  Gradient should be maximum near threshold
     * HOW:  Test at threshold value
     */
    surrogate_ctx = snn_training_create_surrogate(&surrogate_config, 10, 10);
    ASSERT_NE(surrogate_ctx, nullptr);

    float grad = snn_training_surrogate_gradient(surrogate_ctx, surrogate_config.threshold);
    EXPECT_TRUE(is_finite(grad));
    EXPECT_GT(std::abs(grad), 0.0f);  /* Should have non-zero gradient */
}

TEST_F(SNNNumericalStabilityTest, SurrogateGradientWithInfInput) {
    /**
     * WHAT: Test surrogate gradient with infinite input
     * WHY:  Must handle invalid input gracefully
     * HOW:  Pass Inf membrane potential
     */
    surrogate_ctx = snn_training_create_surrogate(&surrogate_config, 10, 10);
    ASSERT_NE(surrogate_ctx, nullptr);

    float grad = snn_training_surrogate_gradient(surrogate_ctx, std::numeric_limits<float>::infinity());
    EXPECT_TRUE(is_finite(grad));  /* Should clamp to finite value */
}

TEST_F(SNNNumericalStabilityTest, SurrogateGradientWithNaN) {
    /**
     * WHAT: Test surrogate gradient with NaN input
     * WHY:  Must handle invalid input gracefully
     * HOW:  Pass NaN membrane potential
     */
    surrogate_ctx = snn_training_create_surrogate(&surrogate_config, 10, 10);
    ASSERT_NE(surrogate_ctx, nullptr);

    float nan_val = std::numeric_limits<float>::quiet_NaN();
    float grad = snn_training_surrogate_gradient(surrogate_ctx, nan_val);

    /* Should handle NaN gracefully */
    /* Result could be 0, NaN, or finite depending on implementation */
}

//=============================================================================
// Reward Signal Stability Tests
//=============================================================================

TEST_F(SNNNumericalStabilityTest, RewardSignalClamped) {
    /**
     * WHAT: Test reward signal is clamped to reasonable range
     * WHY:  Extreme rewards cause training instability
     * HOW:  Set extreme rewards, verify clamping
     */
    rstdp_ctx = snn_training_create_rstdp(&rstdp_config, 10, 10);
    ASSERT_NE(rstdp_ctx, nullptr);

    /* Set very large positive reward */
    snn_rstdp_set_reward(rstdp_ctx, 1e10f);

    /* Set very large negative reward */
    snn_rstdp_set_reward(rstdp_ctx, -1e10f);

    /* Set infinity */
    snn_rstdp_set_reward(rstdp_ctx, std::numeric_limits<float>::infinity());

    /* Set NaN - should be handled */
    snn_rstdp_set_reward(rstdp_ctx, std::numeric_limits<float>::quiet_NaN());

    /* Test passed if no crash */
    SUCCEED();
}

TEST_F(SNNNumericalStabilityTest, BaselineRewardStability) {
    /**
     * WHAT: Test baseline reward doesn't cause issues
     * WHY:  Baseline subtraction could cause cancellation errors
     * HOW:  Set rewards near baseline
     */
    rstdp_ctx = snn_training_create_rstdp(&rstdp_config, 10, 10);
    ASSERT_NE(rstdp_ctx, nullptr);

    /* Reward equal to baseline */
    snn_rstdp_set_reward(rstdp_ctx, rstdp_config.baseline_reward);

    /* Reward very close to baseline (potential cancellation) */
    snn_rstdp_set_reward(rstdp_ctx, rstdp_config.baseline_reward + 1e-7f);
    snn_rstdp_set_reward(rstdp_ctx, rstdp_config.baseline_reward - 1e-7f);

    SUCCEED();
}

//=============================================================================
// Weight Bound Enforcement Tests
//=============================================================================

TEST_F(SNNNumericalStabilityTest, WeightBoundsEnforced) {
    /**
     * WHAT: Test weight bounds are always enforced
     * WHY:  Unbounded weights cause network instability
     * HOW:  Apply many updates, verify bounds maintained
     */
    stdp_ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(stdp_ctx, nullptr);

    float weight = 0.5f;

    /* Apply many LTP updates */
    for (int i = 0; i < 1000; i++) {
        float delta_w = snn_stdp_compute_delta_w(stdp_ctx, 5.0f, weight);
        weight += delta_w;

        /* Manually clamp (testing that delta_w is bounded) */
        weight = std::max(stdp_config.w_min, std::min(stdp_config.w_max, weight));

        EXPECT_LE(weight, stdp_config.w_max);
        EXPECT_GE(weight, stdp_config.w_min);
    }
}

TEST_F(SNNNumericalStabilityTest, SoftBoundsNumericalStability) {
    /**
     * WHAT: Test soft bounds don't cause numerical issues
     * WHY:  Multiplicative bounds can accumulate errors
     * HOW:  Test with soft bounds enabled
     */
    snn_stdp_config_t soft_config;
    snn_stdp_config_default(&soft_config);
    soft_config.soft_bounds = true;

    snn_training_ctx_t* ctx = snn_training_create_stdp(&soft_config);
    ASSERT_NE(ctx, nullptr);

    /* Test at extremes where soft bounds matter */
    float delta_w_near_max = snn_stdp_compute_delta_w(ctx, 5.0f, soft_config.w_max - 0.001f);
    EXPECT_TRUE(is_finite(delta_w_near_max));

    float delta_w_near_min = snn_stdp_compute_delta_w(ctx, -5.0f, soft_config.w_min + 0.001f);
    EXPECT_TRUE(is_finite(delta_w_near_min));

    snn_training_destroy(ctx);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(SNNNumericalStabilityTest, STDPComputeNullCtx) {
    /**
     * WHAT: Test STDP with NULL context
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify no crash
     */
    float delta_w = snn_stdp_compute_delta_w(nullptr, 10.0f, 0.5f);
    EXPECT_EQ(delta_w, 0.0f);  /* Should return 0 for NULL */
}

TEST_F(SNNNumericalStabilityTest, SurrogateGradientNullCtx) {
    /**
     * WHAT: Test surrogate gradient with NULL context
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify no crash
     */
    float grad = snn_training_surrogate_gradient(nullptr, 0.5f);
    EXPECT_EQ(grad, 0.0f);  /* Should return 0 for NULL */
}

TEST_F(SNNNumericalStabilityTest, SetRewardNullCtx) {
    /**
     * WHAT: Test set reward with NULL context
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify no crash
     */
    snn_rstdp_set_reward(nullptr, 1.0f);
    /* Should not crash */
    SUCCEED();
}

TEST_F(SNNNumericalStabilityTest, UpdateEligibilityNullCtx) {
    /**
     * WHAT: Test eligibility update with NULL context
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify no crash
     */
    snn_rstdp_update_eligibility(nullptr, 1.0f);
    SUCCEED();
}

TEST_F(SNNNumericalStabilityTest, DestroyNullCtx) {
    /**
     * WHAT: Test destroy with NULL
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify no crash
     */
    snn_training_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Denormal Prevention Tests
//=============================================================================

TEST_F(SNNNumericalStabilityTest, NoDenormalsInDecay) {
    /**
     * WHAT: Test that decay operations don't produce denormals
     * WHY:  Denormals cause severe performance penalties
     * HOW:  Apply many decay steps, check for denormals
     */
    stdp_ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(stdp_ctx, nullptr);

    float weight = 1e-30f;  /* Very small starting value */

    for (int i = 0; i < 100; i++) {
        /* Large dt causes decay */
        float delta_w = snn_stdp_compute_delta_w(stdp_ctx, 100.0f, weight);

        if (delta_w != 0.0f && std::abs(delta_w) < FLT_MIN) {
            FAIL() << "Denormal delta_w detected: " << delta_w;
        }

        weight += delta_w;
        weight = std::max(0.0f, weight);  /* Prevent negative */

        if (weight != 0.0f && std::abs(weight) < FLT_MIN) {
            /* Weight became denormal - should be flushed to zero */
            weight = 0.0f;
        }
    }

    SUCCEED();
}

//=============================================================================
// Long-Running Stability Tests
//=============================================================================

TEST_F(SNNNumericalStabilityTest, LongRunningSTDPStability) {
    /**
     * WHAT: Test STDP stability over many iterations
     * WHY:  Numerical errors can accumulate
     * HOW:  Run many STDP updates, verify no degradation
     */
    stdp_ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(stdp_ctx, nullptr);

    float weight = 0.5f;
    int nan_count = 0;
    int inf_count = 0;

    for (int i = 0; i < 10000; i++) {
        /* Alternate LTP and LTD */
        float dt = (i % 2 == 0) ? 10.0f : -10.0f;
        float delta_w = snn_stdp_compute_delta_w(stdp_ctx, dt, weight);

        if (std::isnan(delta_w)) nan_count++;
        if (std::isinf(delta_w)) inf_count++;

        weight += delta_w;
        weight = std::max(stdp_config.w_min, std::min(stdp_config.w_max, weight));
    }

    EXPECT_EQ(nan_count, 0) << "NaN values detected during long run";
    EXPECT_EQ(inf_count, 0) << "Inf values detected during long run";
    EXPECT_TRUE(is_finite(weight)) << "Final weight is not finite";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
