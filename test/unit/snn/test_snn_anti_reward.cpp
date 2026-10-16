/**
 * @file test_snn_anti_reward.cpp
 * @brief Unit tests for the anti-reward branch of snn_compute_intrinsic_reward.
 *
 * WHAT: Verify that intrinsic reward goes NEGATIVE when a population's
 *       firing rate exceeds threshold_ratio × target, so R-STDP can push
 *       saturated pathways back down.
 * WHY:  Without signed reward, saturation collapses the gaussian to 0 and
 *       R-STDP weight updates vanish.
 * HOW:  Build a minimal 2-population SNN, manually set firing_rate_ema +
 *       rate_samples to bypass warmup, call snn_compute_intrinsic_reward,
 *       and assert the expected values across the gaussian/anti-reward
 *       regimes. Also verify setter range clamps and the input-pop target
 *       interaction.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers carry their own extern "C" guards.
#include "snn/nimcp_snn_training.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"

//=============================================================================
// Fixture
//=============================================================================

class SNNAntiRewardTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    snn_config_t config;

    // Saved tunable state so tests don't leak global defaults to each other.
    float saved_enabled = 0.0f;
    float saved_thr_ratio = 0.0f;
    float saved_gain = 0.0f;
    float saved_target = 0.0f;
    float saved_target_input = 0.0f;

    void SetUp() override {
        memset(&config, 0, sizeof(snn_config_t));
        // 4 inputs, 0 hidden, 2 outputs → populations = {input, output}. The
        // feedforward builder creates exactly what we need: a 2-pop network
        // where pop[0] is named "input" and pop[1] is "output".
        snn_config_feedforward(&config, 4, 0, 2);
        network = snn_network_create(&config);
        ASSERT_NE(network, nullptr);
        ASSERT_GE(network->n_populations, 2u);

        saved_enabled       = snn_tune_get_anti_reward_enabled();
        saved_thr_ratio     = snn_tune_get_anti_reward_threshold_ratio();
        saved_gain          = snn_tune_get_anti_reward_gain();
        saved_target        = snn_tune_get_target_rate();
        saved_target_input  = snn_tune_get_target_rate_input();

        // Canonical defaults for deterministic math across tests.
        snn_tune_set_anti_reward_enabled(1.0f);
        snn_tune_set_anti_reward_threshold_ratio(2.0f);
        snn_tune_set_anti_reward_gain(0.5f);
    }

    void TearDown() override {
        snn_tune_set_anti_reward_enabled(saved_enabled);
        snn_tune_set_anti_reward_threshold_ratio(saved_thr_ratio);
        snn_tune_set_anti_reward_gain(saved_gain);
        snn_tune_set_target_rate(saved_target);
        snn_tune_set_target_rate_input(saved_target_input);
        if (network) {
            snn_network_destroy(network);
            network = nullptr;
        }
        snn_config_destroy(&config);
    }

    // Set every pop's firing_rate_ema to the given ratio of its own target,
    // and bypass warmup. Pop targets come from _target_rate_for_pop which
    // returns 0.05 for "input*" pops and g_homeo_target_rate otherwise.
    void SetAllPopsToRatioOfTarget(float ratio) {
        for (uint32_t p = 0; p < network->n_populations; p++) {
            snn_population_t* pop = network->populations[p];
            ASSERT_NE(pop, nullptr);
            float target;
            if (strncmp(pop->name, "input", 5) == 0) {
                target = snn_tune_get_target_rate_input();
            } else {
                target = snn_tune_get_target_rate();
            }
            pop->firing_rate_ema = ratio * target;
            pop->rate_samples = 20;  // bypass the rate_samples < 10 warmup
        }
    }
};

//=============================================================================
// Gaussian-only regime (below or at anti-reward threshold)
//=============================================================================

TEST_F(SNNAntiRewardTest, AtTargetRewardIsOne) {
    // All pops exactly at target → per-pop reward = exp(0) = 1.
    SetAllPopsToRatioOfTarget(1.0f);
    float r = snn_compute_intrinsic_reward(network);
    EXPECT_NEAR(r, 1.0f, 0.001f);
}

TEST_F(SNNAntiRewardTest, AtZeroRateRewardSmallPositive) {
    // rate=0, target=t → err = -t, sigma = 0.5t, so exponent = -(t^2)/(2*(0.5t)^2)
    // = -2, giving exp(-2) ≈ 0.1353. Anti-reward threshold (rate > 2t) is NOT
    // triggered, so the gaussian alone rules.
    SetAllPopsToRatioOfTarget(0.0f);
    float r = snn_compute_intrinsic_reward(network);
    EXPECT_GT(r, 0.0f);
    EXPECT_NEAR(r, expf(-2.0f), 0.001f);
}

TEST_F(SNNAntiRewardTest, AtTwoTimesTargetStillPositiveGaussianOnly) {
    // rate = 2×target: err = target, (err/sigma)^2 = (2)^2 = 4, /2 = 2,
    // → exp(-2) ≈ 0.1353. Anti-reward threshold is STRICTLY rate > thr×target,
    // so at exactly 2×target the penalty does not apply.
    SetAllPopsToRatioOfTarget(2.0f);
    float r = snn_compute_intrinsic_reward(network);
    EXPECT_GT(r, 0.0f);
    EXPECT_NEAR(r, expf(-2.0f), 0.001f);
}

//=============================================================================
// Anti-reward (negative) regime
//=============================================================================

TEST_F(SNNAntiRewardTest, AtThreeTimesTargetRewardNegative) {
    // rate = 3×target: gaussian exp(-(2)^2/(2*0.5^2 * target^2)... wait, err=2t,
    // err^2/two_sigma_sq = (4t^2)/(2*(0.5t)^2) = 4t^2 / (0.5 t^2) = 8, so
    // gaussian = exp(-8) ≈ 3.35e-4. Anti-reward subtracts 0.5 * (3t - 2t)/t
    // = 0.5. Net per-pop reward ≈ -0.4997.
    SetAllPopsToRatioOfTarget(3.0f);
    float r = snn_compute_intrinsic_reward(network);
    EXPECT_LT(r, 0.0f);
    EXPECT_NEAR(r, expf(-8.0f) - 0.5f, 0.001f);
}

TEST_F(SNNAntiRewardTest, DisabledFlagTurnsOffNegativeBranch) {
    // Same 3×target situation, but anti-reward disabled: reward must fall
    // back to small positive gaussian tail (≈ exp(-8)).
    snn_tune_set_anti_reward_enabled(0.0f);
    SetAllPopsToRatioOfTarget(3.0f);
    float r = snn_compute_intrinsic_reward(network);
    EXPECT_GT(r, 0.0f);
    EXPECT_NEAR(r, expf(-8.0f), 0.001f);
}

TEST_F(SNNAntiRewardTest, ZeroGainDisablesNegativeContribution) {
    // gain=0 with enabled=1: penalty coefficient is zero so reward collapses
    // back to the gaussian tail, same as disabling entirely.
    snn_tune_set_anti_reward_gain(0.0f);
    SetAllPopsToRatioOfTarget(3.0f);
    float r = snn_compute_intrinsic_reward(network);
    EXPECT_GT(r, 0.0f);
    EXPECT_NEAR(r, expf(-8.0f), 0.001f);
}

//=============================================================================
// Setter range clamps — values outside the valid range must NOT update.
//=============================================================================

TEST_F(SNNAntiRewardTest, SetterClampsThresholdRatio) {
    const float before = snn_tune_get_anti_reward_threshold_ratio();

    snn_tune_set_anti_reward_threshold_ratio(0.5f);   // <= 1.0 → rejected
    EXPECT_FLOAT_EQ(snn_tune_get_anti_reward_threshold_ratio(), before);

    snn_tune_set_anti_reward_threshold_ratio(1.0f);   // boundary excluded
    EXPECT_FLOAT_EQ(snn_tune_get_anti_reward_threshold_ratio(), before);

    snn_tune_set_anti_reward_threshold_ratio(10.0f);  // boundary excluded
    EXPECT_FLOAT_EQ(snn_tune_get_anti_reward_threshold_ratio(), before);

    snn_tune_set_anti_reward_threshold_ratio(50.0f);  // too large → rejected
    EXPECT_FLOAT_EQ(snn_tune_get_anti_reward_threshold_ratio(), before);

    snn_tune_set_anti_reward_threshold_ratio(3.5f);   // valid → accepted
    EXPECT_FLOAT_EQ(snn_tune_get_anti_reward_threshold_ratio(), 3.5f);
}

TEST_F(SNNAntiRewardTest, SetterClampsGain) {
    snn_tune_set_anti_reward_gain(1.25f);
    const float baseline = snn_tune_get_anti_reward_gain();
    EXPECT_FLOAT_EQ(baseline, 1.25f);

    snn_tune_set_anti_reward_gain(-0.1f);             // < 0 → rejected
    EXPECT_FLOAT_EQ(snn_tune_get_anti_reward_gain(), baseline);

    snn_tune_set_anti_reward_gain(10.0f);             // boundary excluded
    EXPECT_FLOAT_EQ(snn_tune_get_anti_reward_gain(), baseline);

    snn_tune_set_anti_reward_gain(100.0f);            // too large → rejected
    EXPECT_FLOAT_EQ(snn_tune_get_anti_reward_gain(), baseline);

    snn_tune_set_anti_reward_gain(0.0f);              // 0 is valid
    EXPECT_FLOAT_EQ(snn_tune_get_anti_reward_gain(), 0.0f);
}

TEST_F(SNNAntiRewardTest, SetterEnabledAcceptsAnyNonzero) {
    snn_tune_set_anti_reward_enabled(0.0f);
    EXPECT_FLOAT_EQ(snn_tune_get_anti_reward_enabled(), 0.0f);

    snn_tune_set_anti_reward_enabled(1.0f);
    EXPECT_NE(snn_tune_get_anti_reward_enabled(), 0.0f);

    snn_tune_set_anti_reward_enabled(-1.0f);          // any nonzero → on
    EXPECT_NE(snn_tune_get_anti_reward_enabled(), 0.0f);

    snn_tune_set_anti_reward_enabled(42.0f);
    EXPECT_NE(snn_tune_get_anti_reward_enabled(), 0.0f);
}

//=============================================================================
// Per-layer target interaction: input-named pop uses g_target_rate_input.
//=============================================================================

TEST_F(SNNAntiRewardTest, InputPopUsesInputTargetRate) {
    // Rename pop 0 to "input0" — still prefix-matches "input". Set its rate
    // EMA to exactly the input target (5%). Set pop 1 ("output") to the
    // default target (3%). Both pops should be at target → reward ≈ 1.0.
    // If the code used g_homeo_target_rate (3%) for the input pop, 5% / 3%
    // = 1.67× would still be within threshold but would drop the gaussian
    // to exp(-(0.02)^2/(2*0.015^2)) ≈ exp(-0.89) ≈ 0.41, far from 1.0.
    ASSERT_GE(network->n_populations, 2u);
    snn_population_t* in_pop  = network->populations[0];
    snn_population_t* out_pop = network->populations[network->n_populations - 1];
    ASSERT_NE(in_pop, nullptr);
    ASSERT_NE(out_pop, nullptr);

    snprintf(in_pop->name, sizeof(in_pop->name), "input0");
    snprintf(out_pop->name, sizeof(out_pop->name), "output");

    in_pop->firing_rate_ema  = snn_tune_get_target_rate_input();   // 0.05
    in_pop->rate_samples     = 20;
    out_pop->firing_rate_ema = snn_tune_get_target_rate();         // 0.03
    out_pop->rate_samples    = 20;

    // Silence any hidden or auxiliary pops that might exist — any such pop
    // with no samples is excluded from the average (rate_samples < 10).
    for (uint32_t p = 1; p < network->n_populations - 1; p++) {
        if (network->populations[p]) {
            network->populations[p]->rate_samples = 0;
        }
    }

    float r = snn_compute_intrinsic_reward(network);
    EXPECT_NEAR(r, 1.0f, 0.001f);

    // Now push the input pop into anti-reward territory using its 5% target:
    // 3 × 0.05 = 0.15. With output pop still at 0.03 (=target → reward 1.0),
    // the average is (exp(-8) - 0.5 + 1.0) / 2 ≈ 0.25. If the input pop
    // were instead using the default 3% target, 0.15 would be 5× that, and
    // the penalty would be 0.5 × (0.15 - 2*0.03)/0.03 = 0.5 × 3 = 1.5,
    // giving a much more negative average.
    in_pop->firing_rate_ema = 3.0f * snn_tune_get_target_rate_input();
    float r2 = snn_compute_intrinsic_reward(network);
    float expected = (expf(-8.0f) - 0.5f + 1.0f) / 2.0f;
    EXPECT_NEAR(r2, expected, 0.005f);
}
