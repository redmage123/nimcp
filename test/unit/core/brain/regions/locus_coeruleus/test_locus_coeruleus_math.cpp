/**
 * @file test_locus_coeruleus_math.cpp
 * @brief Unit tests for Locus Coeruleus math fix (approach_target delta computation)
 * @date 2026-02-15
 *
 * WHAT: Verify that approach_target correctly computes delta = (target - current)
 * WHY:  A copy-paste bug had (target - target) which always yields 0,
 *       making the NE regulation system completely inert
 * HOW:  Test that NE concentration and firing rate actually change when
 *       targets differ from current values, using the public API
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "core/brain/regions/locus_coeruleus/nimcp_locus_coeruleus.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LCMathTest : public ::testing::Test {
protected:
    nimcp_lc_system_t lc;

    void SetUp() override {
        memset(&lc, 0, sizeof(lc));
        nimcp_lc_error_t err = nimcp_lc_init(&lc, nullptr);
        ASSERT_EQ(err, LC_OK);
        ASSERT_TRUE(lc.initialized);
    }

    void TearDown() override {
        nimcp_lc_shutdown(&lc);
    }

    /**
     * Run the system for N steps with given dt.
     * Returns LC_OK if all steps succeed.
     */
    nimcp_lc_error_t run_steps(int steps, float dt) {
        for (int i = 0; i < steps; i++) {
            nimcp_lc_error_t err = nimcp_lc_update(&lc, dt);
            if (err != LC_OK) return err;
        }
        return LC_OK;
    }
};

//=============================================================================
// Core Delta/Approach Tests
//=============================================================================

/**
 * The approach_target function is used internally in update_neuron_pool
 * to smooth firing rate changes. If delta is always 0, the firing rate
 * never actually moves toward the target rate.
 *
 * We test this by setting phasic mode (high target rate) and verifying
 * the actual firing rate changes toward it.
 */
TEST_F(LCMathTest, FiringRateApproachesTargetInPhasicMode) {
    /* Re-init with autoreceptors disabled to isolate approach_target behavior.
     * With autoreceptors enabled, the NE-driven negative feedback from high
     * phasic firing overwhelms the target rate (net_input goes very negative,
     * causing target_rate to clamp to 0). */
    nimcp_lc_shutdown(&lc);
    memset(&lc, 0, sizeof(lc));
    nimcp_lc_config_t cfg = nimcp_lc_default_config();
    cfg.enable_autoreceptors = false;
    nimcp_lc_error_t err = nimcp_lc_init(&lc, &cfg);
    ASSERT_EQ(err, LC_OK);

    /* Record initial tonic firing rate */
    float initial_rate;
    err = nimcp_lc_get_firing_rate(&lc, &initial_rate);
    ASSERT_EQ(err, LC_OK);

    /* Use trigger_attention_reset which sets novelty_signal=1.0 to keep
     * phasic mode alive. trigger_burst alone doesn't set novelty_signal,
     * so update_mode_state exits phasic immediately (novelty < 0.1). */
    err = nimcp_lc_trigger_attention_reset(&lc);
    ASSERT_EQ(err, LC_OK);
    EXPECT_EQ(lc.mode, LC_MODE_PHASIC);

    /* Run several update steps to allow approach_target to work.
     * novelty_signal decays with tau=200ms, so phasic persists for ~90 steps
     * at dt=5ms before novelty drops below 0.1. */
    err = run_steps(50, 5.0f);
    ASSERT_EQ(err, LC_OK);

    /* The firing rate should have moved toward the phasic target.
     * With the bug (target - target = 0), it would stay at initial_rate.
     * With the fix (target - current), it should increase significantly. */
    float final_rate;
    err = nimcp_lc_get_firing_rate(&lc, &final_rate);
    ASSERT_EQ(err, LC_OK);

    /* Phasic target is config.phasic_rate_hz = 20 Hz.
     * Without autoreceptor feedback, the rate should approach the target. */
    EXPECT_GT(final_rate, initial_rate * 1.5f)
        << "Firing rate should increase significantly in phasic mode. "
        << "initial=" << initial_rate << " final=" << final_rate
        << " (if final ~= initial, approach_target delta is broken)";
}

TEST_F(LCMathTest, FiringRateDecreasesWhenTargetIsLower) {
    /* First increase the firing rate with excitation */
    for (int i = 0; i < 50; i++) {
        nimcp_lc_apply_excitation(&lc, 0.8f);
        nimcp_lc_update(&lc, 2.0f);
    }

    float elevated_rate;
    nimcp_lc_get_firing_rate(&lc, &elevated_rate);

    /* Now switch to quiescent mode - target rate drops to 0.1 Hz */
    nimcp_lc_error_t err = nimcp_lc_set_mode(&lc, LC_MODE_QUIESCENT);
    ASSERT_EQ(err, LC_OK);

    /* Run without excitation - rate should decrease toward 0.1 Hz */
    err = run_steps(200, 5.0f);
    ASSERT_EQ(err, LC_OK);

    float final_rate;
    nimcp_lc_get_firing_rate(&lc, &final_rate);

    /* Rate should have decreased substantially */
    EXPECT_LT(final_rate, elevated_rate * 0.5f)
        << "Firing rate should decrease in quiescent mode. "
        << "elevated=" << elevated_rate << " final=" << final_rate;
}

TEST_F(LCMathTest, NEConcentrationRespondsToFiringRateChanges) {
    /* Record baseline NE */
    float baseline_ne = lc.ne_concentration;

    /* Trigger high-intensity burst */
    nimcp_lc_error_t err = nimcp_lc_trigger_burst(&lc, 1.0f, 500.0f);
    ASSERT_EQ(err, LC_OK);

    /* Run updates - higher firing rate should produce more NE */
    err = run_steps(200, 5.0f);
    ASSERT_EQ(err, LC_OK);

    float burst_ne = lc.ne_concentration;

    /* NE should have risen above baseline due to increased firing.
     * If approach_target was broken, firing rate wouldn't change,
     * and NE dynamics would be flat. */
    EXPECT_GT(burst_ne, baseline_ne)
        << "NE concentration should increase during phasic burst. "
        << "baseline=" << baseline_ne << " burst=" << burst_ne;
}

//=============================================================================
// Arousal/Membrane Potential Approach Tests
//=============================================================================

TEST_F(LCMathTest, MembranePotentialChangesWithInput) {
    /* Record initial membrane potential */
    float initial_vm = lc.neurons.membrane_potential;

    /* Apply strong excitation and update */
    for (int i = 0; i < 50; i++) {
        nimcp_lc_apply_excitation(&lc, 1.0f);
        nimcp_lc_update(&lc, 5.0f);
    }

    /* Membrane potential should have moved toward a more depolarized target.
     * Note: membrane potential uses its own inline approach calculation,
     * not the approach_target helper, but we verify it works too. */
    float final_vm = lc.neurons.membrane_potential;
    EXPECT_NE(final_vm, initial_vm)
        << "Membrane potential should change with excitatory input";
}

TEST_F(LCMathTest, ArousalLevelChangesWithNE) {
    /* Record initial arousal */
    float initial_arousal = lc.arousal_level;

    /* Trigger burst to increase NE and thus arousal */
    nimcp_lc_trigger_burst(&lc, 1.0f, 500.0f);

    /* Apply excitation to sustain high firing */
    for (int i = 0; i < 100; i++) {
        nimcp_lc_apply_excitation(&lc, 0.5f);
        nimcp_lc_update(&lc, 10.0f);
    }

    float final_arousal = lc.arousal_level;

    /* Arousal uses its own alpha*(target-current) inline.
     * The change should be non-zero. */
    EXPECT_NE(final_arousal, initial_arousal)
        << "Arousal level should change in response to NE changes. "
        << "initial=" << initial_arousal << " final=" << final_arousal;
}

//=============================================================================
// Convergence Tests
//=============================================================================

TEST_F(LCMathTest, FiringRateConvergesTowardTarget) {
    /* Test convergence in quiescent mode (target=0.1 Hz), which is stable
     * because it doesn't require novelty_signal to stay active.
     * Starting from tonic rate ~2.0 Hz, the rate should converge toward 0.1 Hz. */
    float initial_rate;
    nimcp_lc_get_firing_rate(&lc, &initial_rate);
    ASSERT_GT(initial_rate, 0.5f);  /* Should be ~2.0 Hz */

    float quiescent_target = 0.1f;
    float initial_distance = std::fabs(quiescent_target - initial_rate);

    /* Switch to quiescent mode */
    nimcp_lc_set_mode(&lc, LC_MODE_QUIESCENT);

    /* Run many steps */
    run_steps(500, 2.0f);

    float final_rate;
    nimcp_lc_get_firing_rate(&lc, &final_rate);
    float final_distance = std::fabs(quiescent_target - final_rate);

    /* The distance to the quiescent target should decrease over time */
    EXPECT_LT(final_distance, initial_distance)
        << "Firing rate should converge toward quiescent target. "
        << "target=" << quiescent_target
        << " initial_rate=" << initial_rate
        << " final_rate=" << final_rate;
}

TEST_F(LCMathTest, StableAfterManyUpdatesWithDeltaFix) {
    /* With the bug, everything was trivially "stable" because nothing moved.
     * With the fix, we need to verify the system doesn't diverge. */
    nimcp_lc_trigger_burst(&lc, 0.7f, 200.0f);

    for (int i = 0; i < 2000; i++) {
        if (i % 100 == 0) {
            nimcp_lc_apply_excitation(&lc, 0.3f);
        }
        nimcp_lc_error_t err = nimcp_lc_update(&lc, 1.0f);
        ASSERT_EQ(err, LC_OK);
    }

    /* Verify no NaN/Inf in key values */
    EXPECT_FALSE(std::isnan(lc.ne_concentration));
    EXPECT_FALSE(std::isinf(lc.ne_concentration));
    EXPECT_FALSE(std::isnan(lc.neurons.firing_rate));
    EXPECT_FALSE(std::isinf(lc.neurons.firing_rate));
    EXPECT_FALSE(std::isnan(lc.arousal_level));
    EXPECT_FALSE(std::isinf(lc.arousal_level));

    /* Values should be within valid ranges */
    EXPECT_GE(lc.ne_concentration, 0.0f);
    EXPECT_LE(lc.ne_concentration, lc.config.ne_max_nm);
    EXPECT_GE(lc.neurons.firing_rate, 0.0f);
    EXPECT_LE(lc.neurons.firing_rate, LC_PHASIC_MAX_HZ);
    EXPECT_GE(lc.arousal_level, 0.0f);
    EXPECT_LE(lc.arousal_level, 1.0f);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(LCMathTest, ApproachTargetWithZeroTau) {
    /* When dt is very large relative to tau, alpha approaches 1.0, so the
     * firing rate should jump quickly to the target. We test this by
     * switching to quiescent mode (target = 0.1 Hz) and verifying the rate
     * drops rapidly with large dt.
     *
     * Note: We use quiescent mode rather than phasic because phasic mode
     * requires novelty_signal > 0.1 to persist, and update_mode_state would
     * exit phasic on the first update if novelty is not set. */
    float initial_rate;
    nimcp_lc_get_firing_rate(&lc, &initial_rate);
    ASSERT_GT(initial_rate, 0.5f);  /* Should be ~2.0 Hz */

    nimcp_lc_set_mode(&lc, LC_MODE_QUIESCENT);

    /* Large dt relative to rate_tau (50ms) should cause rapid convergence */
    for (int i = 0; i < 3; i++) {
        nimcp_lc_update(&lc, 500.0f);  /* Very large timestep */
    }

    float rate;
    nimcp_lc_get_firing_rate(&lc, &rate);

    /* With large dt, alpha is close to 1.0, so rate should have jumped
     * quickly toward the quiescent target of 0.1 Hz. */
    EXPECT_LT(rate, initial_rate * 0.5f)
        << "With large dt, firing rate should converge quickly toward quiescent target. "
        << "initial=" << initial_rate << " rate=" << rate;
}

TEST_F(LCMathTest, ApproachTargetWhenCurrentEqualsTarget) {
    /* When current == target, delta should be 0 and nothing changes.
     * This is correct behavior even after the fix. */
    float initial_rate;
    nimcp_lc_get_firing_rate(&lc, &initial_rate);

    /* In tonic mode with no input, target ~= current (both at tonic baseline).
     * One step should produce minimal change. */
    nimcp_lc_update(&lc, 0.001f);  /* Tiny timestep */

    float final_rate;
    nimcp_lc_get_firing_rate(&lc, &final_rate);

    /* Rate shouldn't change much when already at target */
    float delta = std::fabs(final_rate - initial_rate);
    EXPECT_LT(delta, 0.1f)
        << "When at target, firing rate should barely change. "
        << "delta=" << delta;
}

//=============================================================================
// Modulate Arousal Integration
//=============================================================================

TEST_F(LCMathTest, ModulateArousalAffectsFiringRate) {
    /* nimcp_lc_modulate_arousal adjusts tonic_firing_rate to achieve
     * a target NE level. We verify the tonic_firing_rate parameter changes
     * and that the firing rate differs between high and low arousal targets.
     *
     * Note: Autoreceptor feedback creates strong negative feedback that can
     * overwhelm moderate tonic rate increases. We disable autoreceptors to
     * isolate the arousal modulation effect on firing rate. */

    /* Re-init with autoreceptors disabled to isolate the test */
    nimcp_lc_shutdown(&lc);
    memset(&lc, 0, sizeof(lc));
    nimcp_lc_config_t cfg = nimcp_lc_default_config();
    cfg.enable_autoreceptors = false;
    nimcp_lc_error_t err = nimcp_lc_init(&lc, &cfg);
    ASSERT_EQ(err, LC_OK);

    /* Set low arousal and let system settle */
    err = nimcp_lc_modulate_arousal(&lc, 0.1f);
    ASSERT_EQ(err, LC_OK);
    float low_tonic = lc.tonic_firing_rate;

    err = run_steps(200, 5.0f);
    ASSERT_EQ(err, LC_OK);

    float low_rate;
    nimcp_lc_get_firing_rate(&lc, &low_rate);

    /* Reset and set high arousal */
    nimcp_lc_reset(&lc);
    err = nimcp_lc_modulate_arousal(&lc, 0.95f);
    ASSERT_EQ(err, LC_OK);
    float high_tonic = lc.tonic_firing_rate;

    /* The tonic_firing_rate should be higher with high arousal */
    EXPECT_GT(high_tonic, low_tonic)
        << "Higher arousal should set higher tonic_firing_rate. "
        << "low=" << low_tonic << " high=" << high_tonic;

    err = run_steps(200, 5.0f);
    ASSERT_EQ(err, LC_OK);

    float high_rate;
    nimcp_lc_get_firing_rate(&lc, &high_rate);

    EXPECT_GT(high_rate, low_rate)
        << "Higher arousal target should produce higher firing rate. "
        << "low_rate=" << low_rate << " high_rate=" << high_rate;
}

TEST_F(LCMathTest, LowArousalTargetDecreasesFiringRate) {
    /* First elevate */
    nimcp_lc_modulate_arousal(&lc, 0.9f);
    run_steps(100, 5.0f);

    float elevated_rate;
    nimcp_lc_get_firing_rate(&lc, &elevated_rate);

    /* Now set very low arousal */
    nimcp_lc_modulate_arousal(&lc, 0.05f);

    /* The tonic_firing_rate should now be lower */
    EXPECT_LT(lc.tonic_firing_rate, lc.config.tonic_rate_hz);

    /* After updates, firing rate should decrease */
    run_steps(200, 5.0f);

    float final_rate;
    nimcp_lc_get_firing_rate(&lc, &final_rate);

    EXPECT_LT(final_rate, elevated_rate)
        << "Lower arousal target should decrease firing rate. "
        << "elevated=" << elevated_rate << " final=" << final_rate;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
