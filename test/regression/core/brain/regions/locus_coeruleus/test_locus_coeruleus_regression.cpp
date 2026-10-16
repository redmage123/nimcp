/**
 * @file test_locus_coeruleus_regression.cpp
 * @brief Regression tests for Locus Coeruleus approach_target math bug
 * @date 2026-02-15
 *
 * WHAT: Prevent recurrence of the (target - target) bug in approach_target
 * WHY:  A copy-paste error produced delta=0 always, making NE regulation dead
 * HOW:  Verify observable behavior that would fail if the bug returned:
 *       - Firing rate must change when target differs from current
 *       - NE concentration must respond to mode changes
 *       - System must not be "stuck" at initial values
 *
 * BUG DESCRIPTION:
 *   In approach_target(), the computation was:
 *     return current + alpha * (target - target);  // BUG: always 0
 *   Correct:
 *     return current + alpha * (target - current);
 *   This caused the neuron pool firing rate to never smoothly transition
 *   to the target rate, making the entire NE regulation pathway inert.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "core/brain/regions/locus_coeruleus/nimcp_locus_coeruleus.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LCRegressionTest : public ::testing::Test {
protected:
    nimcp_lc_system_t lc;

    void SetUp() override {
        memset(&lc, 0, sizeof(lc));
        nimcp_lc_error_t err = nimcp_lc_init(&lc, nullptr);
        ASSERT_EQ(err, LC_OK);
    }

    void TearDown() override {
        nimcp_lc_shutdown(&lc);
    }
};

//=============================================================================
// Regression: approach_target delta must be non-zero when target != current
//=============================================================================

/**
 * REGRESSION: approach_target(current, target, tau, dt) must produce
 * a result != current when target != current.
 *
 * This is the core regression test. If the bug returns, the firing rate
 * in phasic mode will remain at the tonic rate despite the higher target.
 */
TEST_F(LCRegressionTest, FiringRateChangesInPhasicMode_DeltaBugRegression) {
    /* Re-init with autoreceptors disabled to isolate approach_target behavior.
     * With autoreceptors enabled, the NE-driven negative feedback from high
     * phasic firing overwhelms the target rate (net_input goes very negative,
     * causing effective target_rate to clamp to 0). */
    nimcp_lc_shutdown(&lc);
    memset(&lc, 0, sizeof(lc));
    nimcp_lc_config_t cfg = nimcp_lc_default_config();
    cfg.enable_autoreceptors = false;
    nimcp_lc_error_t err = nimcp_lc_init(&lc, &cfg);
    ASSERT_EQ(err, LC_OK);

    /* Capture tonic baseline rate */
    float tonic_rate;
    nimcp_lc_get_firing_rate(&lc, &tonic_rate);

    /* Use trigger_attention_reset which sets novelty_signal=1.0 to keep
     * phasic mode alive. set_mode(PHASIC) alone doesn't set novelty_signal,
     * so update_mode_state exits phasic immediately (novelty < 0.1). */
    err = nimcp_lc_trigger_attention_reset(&lc);
    ASSERT_EQ(err, LC_OK);
    EXPECT_EQ(lc.mode, LC_MODE_PHASIC);

    /* The phasic_firing_rate (target for the neuron pool) should be set */
    ASSERT_GT(lc.phasic_firing_rate, 0.0f);

    /* Run updates - approach_target should move firing rate toward phasic target.
     * Use only 50 steps so novelty hasn't decayed below 0.1 yet
     * (novelty decays with tau=200ms; at dt=5ms, stays above 0.1 for ~90 steps). */
    for (int i = 0; i < 50; i++) {
        err = nimcp_lc_update(&lc, 5.0f);
        ASSERT_EQ(err, LC_OK);
    }

    float phasic_rate;
    nimcp_lc_get_firing_rate(&lc, &phasic_rate);

    /* KEY ASSERTION: If the bug (target-target=0) returns, phasic_rate
     * will remain approximately equal to tonic_rate because approach_target
     * never moves the value. With the fix, it must be significantly higher. */
    EXPECT_GT(phasic_rate, tonic_rate * 1.2f)
        << "REGRESSION: Firing rate did not increase in phasic mode. "
        << "This indicates approach_target delta may be broken (target-target=0). "
        << "tonic=" << tonic_rate << " phasic=" << phasic_rate;
}

/**
 * REGRESSION: Verify that returning from phasic to tonic mode causes
 * the firing rate to decrease back down. Both directions of approach
 * must work (increasing and decreasing).
 */
TEST_F(LCRegressionTest, FiringRateDecreasesWhenReturningToTonic_DeltaBugRegression) {
    /* Enter phasic and run to increase rate */
    nimcp_lc_trigger_burst(&lc, 1.0f, 500.0f);
    for (int i = 0; i < 100; i++) {
        nimcp_lc_update(&lc, 5.0f);
    }

    float phasic_rate;
    nimcp_lc_get_firing_rate(&lc, &phasic_rate);

    /* Return to tonic mode */
    nimcp_lc_set_mode(&lc, LC_MODE_TONIC);

    /* Run updates - rate should decrease toward tonic target */
    for (int i = 0; i < 200; i++) {
        nimcp_lc_update(&lc, 5.0f);
    }

    float tonic_rate;
    nimcp_lc_get_firing_rate(&lc, &tonic_rate);

    /* Rate should have decreased from phasic peak */
    EXPECT_LT(tonic_rate, phasic_rate)
        << "REGRESSION: Firing rate did not decrease when returning to tonic. "
        << "phasic=" << phasic_rate << " tonic=" << tonic_rate;
}

/**
 * REGRESSION: NE concentration must change in response to sustained
 * phasic firing. If approach_target is broken, firing rate stays at
 * tonic levels, and NE production stays at baseline.
 */
TEST_F(LCRegressionTest, NEConcentrationIncreasesInPhasicMode_DeltaBugRegression) {
    /* Record baseline NE at init (before any updates) */
    float baseline_ne = lc.ne_concentration;

    /* Use trigger_attention_reset which sets novelty_signal=1.0,
     * keeping phasic mode alive during updates */
    nimcp_lc_trigger_attention_reset(&lc);

    /* Run phasic updates - higher firing rate should produce more NE */
    for (int i = 0; i < 100; i++) {
        nimcp_lc_update(&lc, 5.0f);
    }

    float phasic_ne = lc.ne_concentration;

    /* NE should be elevated above the initial baseline.
     * With the bug, firing rate stays low, NE stays near baseline. */
    EXPECT_GT(phasic_ne, baseline_ne)
        << "REGRESSION: NE concentration didn't increase during phasic burst. "
        << "baseline_ne=" << baseline_ne << " phasic_ne=" << phasic_ne;
}

/**
 * REGRESSION: The quiescent mode target rate is 0.1 Hz. If approach_target
 * works, firing rate should drop well below tonic baseline.
 */
TEST_F(LCRegressionTest, FiringRateDropsInQuiescentMode_DeltaBugRegression) {
    /* Get tonic baseline */
    float tonic_rate;
    nimcp_lc_get_firing_rate(&lc, &tonic_rate);
    ASSERT_GT(tonic_rate, 0.5f);  /* Should be ~2 Hz */

    /* Switch to quiescent */
    nimcp_lc_set_mode(&lc, LC_MODE_QUIESCENT);

    /* Run many steps */
    for (int i = 0; i < 500; i++) {
        nimcp_lc_update(&lc, 5.0f);
    }

    float quiescent_rate;
    nimcp_lc_get_firing_rate(&lc, &quiescent_rate);

    /* Should have dropped substantially toward 0.1 Hz target */
    EXPECT_LT(quiescent_rate, tonic_rate * 0.5f)
        << "REGRESSION: Firing rate didn't drop in quiescent mode. "
        << "tonic=" << tonic_rate << " quiescent=" << quiescent_rate;
}

//=============================================================================
// Stability Regression (system must remain stable WITH the fix active)
//=============================================================================

TEST_F(LCRegressionTest, NoNanOrInfAfterExtendedSimulation) {
    /* With the bug, everything was trivially stable (nothing moved).
     * Verify the fixed code is also stable. */

    /* Run a realistic scenario: alternating tonic and phasic */
    for (int cycle = 0; cycle < 5; cycle++) {
        /* Tonic phase */
        nimcp_lc_set_mode(&lc, LC_MODE_TONIC);
        for (int i = 0; i < 200; i++) {
            nimcp_lc_apply_excitation(&lc, 0.2f);
            nimcp_lc_update(&lc, 5.0f);
        }

        /* Phasic burst */
        nimcp_lc_trigger_burst(&lc, 0.8f, 200.0f);
        for (int i = 0; i < 50; i++) {
            nimcp_lc_update(&lc, 5.0f);
        }
    }

    /* Check stability */
    EXPECT_FALSE(std::isnan(lc.ne_concentration));
    EXPECT_FALSE(std::isinf(lc.ne_concentration));
    EXPECT_FALSE(std::isnan(lc.neurons.firing_rate));
    EXPECT_FALSE(std::isinf(lc.neurons.firing_rate));
    EXPECT_FALSE(std::isnan(lc.arousal_level));
    EXPECT_FALSE(std::isinf(lc.arousal_level));
    EXPECT_FALSE(std::isnan(lc.alertness));
    EXPECT_FALSE(std::isinf(lc.alertness));
    EXPECT_FALSE(std::isnan(lc.neurons.membrane_potential));
    EXPECT_FALSE(std::isinf(lc.neurons.membrane_potential));

    /* All values in valid ranges */
    EXPECT_GE(lc.ne_concentration, 0.0f);
    EXPECT_LE(lc.ne_concentration, lc.config.ne_max_nm);
    EXPECT_GE(lc.neurons.firing_rate, 0.0f);
    EXPECT_LE(lc.neurons.firing_rate, LC_PHASIC_MAX_HZ * 2.0f);
    EXPECT_GE(lc.arousal_level, 0.0f);
    EXPECT_LE(lc.arousal_level, 1.0f);
}

TEST_F(LCRegressionTest, MetricsReflectActualActivity) {
    /* With the bug, mean_firing_rate in metrics would stay at tonic baseline
     * even during phasic mode. With the fix, metrics should show variation.
     *
     * We compare quiescent vs tonic to avoid the phasic/novelty dependency.
     * Tonic rate ~2 Hz should produce higher mean than quiescent (~0.1 Hz). */

    /* Run quiescent phase first */
    nimcp_lc_set_mode(&lc, LC_MODE_QUIESCENT);
    nimcp_lc_reset_metrics(&lc);

    for (int i = 0; i < 200; i++) {
        nimcp_lc_update(&lc, 5.0f);
    }

    nimcp_lc_metrics_t quiescent_metrics;
    nimcp_lc_get_metrics(&lc, &quiescent_metrics);
    float quiescent_mean_rate = quiescent_metrics.mean_firing_rate;

    /* Reset to tonic mode and clear metrics */
    nimcp_lc_reset(&lc);
    nimcp_lc_reset_metrics(&lc);

    /* Run tonic phase */
    for (int i = 0; i < 200; i++) {
        nimcp_lc_update(&lc, 5.0f);
    }

    nimcp_lc_metrics_t tonic_metrics;
    nimcp_lc_get_metrics(&lc, &tonic_metrics);
    float tonic_mean_rate = tonic_metrics.mean_firing_rate;

    /* Tonic mean rate should be higher than quiescent mean rate */
    EXPECT_GT(tonic_mean_rate, quiescent_mean_rate)
        << "REGRESSION: Tonic mean firing rate not higher than quiescent. "
        << "quiescent_mean=" << quiescent_mean_rate
        << " tonic_mean=" << tonic_mean_rate;
}

//=============================================================================
// Projection Regression (NE at targets must reflect firing changes)
//=============================================================================

TEST_F(LCRegressionTest, ProjectionNEReflectsFiringChanges) {
    /* Add a projection */
    uint32_t proj_id;
    nimcp_lc_error_t err = nimcp_lc_add_projection(&lc, LC_TARGET_CORTEX,
                                                     "Cortex", 0.8f, &proj_id);
    ASSERT_EQ(err, LC_OK);

    /* Record NE at target before any updates */
    float initial_ne_at_target;
    nimcp_lc_get_ne_at_target(&lc, LC_TARGET_CORTEX, &initial_ne_at_target);

    /* Use trigger_attention_reset to enter phasic with novelty_signal=1.0.
     * This keeps phasic mode alive during updates. */
    err = nimcp_lc_trigger_attention_reset(&lc);
    ASSERT_EQ(err, LC_OK);

    /* Run phasic updates - higher firing rate should push more NE to target */
    for (int i = 0; i < 100; i++) {
        nimcp_lc_update(&lc, 5.0f);
    }

    float phasic_ne_at_target;
    nimcp_lc_get_ne_at_target(&lc, LC_TARGET_CORTEX, &phasic_ne_at_target);

    /* NE at the cortex projection should be higher after phasic burst */
    EXPECT_GT(phasic_ne_at_target, initial_ne_at_target)
        << "REGRESSION: NE at cortex projection didn't increase during burst. "
        << "initial=" << initial_ne_at_target << " phasic=" << phasic_ne_at_target;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
