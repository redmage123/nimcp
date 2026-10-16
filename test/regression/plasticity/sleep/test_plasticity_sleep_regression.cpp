/**
 * @file test_plasticity_sleep_regression.cpp
 * @brief Regression tests for Plasticity-Sleep bridge stability
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Ensure plasticity sleep bridges maintain consistent behavior
 * WHY:  Prevent regressions in sleep-dependent plasticity modulation
 * HOW:  Test exact values, boundary conditions, and edge cases
 */

#include <gtest/gtest.h>
#include <cmath>
#include <limits>

// Headers have their own extern "C" guards
#include "plasticity/neuromodulators/nimcp_neuromodulators_sleep_bridge.h"
#include "plasticity/stdp/nimcp_stdp_sleep_bridge.h"
#include "plasticity/bcm/nimcp_bcm_sleep_bridge.h"
#include "plasticity/homeostatic/nimcp_homeostatic_sleep_bridge.h"

/* ============================================================================
 * Regression Test Fixture
 * ============================================================================ */

class PlasticitySleepRegressionTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-6f;
};

/* ============================================================================
 * Exact Value Regression Tests - STDP
 * ============================================================================ */

TEST_F(PlasticitySleepRegressionTest, STDP_LR_ExactValues) {
    /* Regression: Ensure LR factors haven't changed */
    EXPECT_FLOAT_EQ(stdp_sleep_get_lr_factor(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(stdp_sleep_get_lr_factor(SLEEP_STATE_DROWSY), 0.5f);
    EXPECT_FLOAT_EQ(stdp_sleep_get_lr_factor(SLEEP_STATE_LIGHT_NREM), 0.3f);
    EXPECT_FLOAT_EQ(stdp_sleep_get_lr_factor(SLEEP_STATE_DEEP_NREM), 0.4f);
    EXPECT_FLOAT_EQ(stdp_sleep_get_lr_factor(SLEEP_STATE_REM), 0.6f);
}

TEST_F(PlasticitySleepRegressionTest, STDP_Ratio_ExactValues) {
    /* Regression: Ensure LTP/LTD ratios haven't changed */
    EXPECT_FLOAT_EQ(stdp_sleep_get_ratio_factor(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(stdp_sleep_get_ratio_factor(SLEEP_STATE_DROWSY), 0.9f);
    EXPECT_FLOAT_EQ(stdp_sleep_get_ratio_factor(SLEEP_STATE_LIGHT_NREM), 0.8f);
    EXPECT_FLOAT_EQ(stdp_sleep_get_ratio_factor(SLEEP_STATE_DEEP_NREM), 0.5f);
    EXPECT_FLOAT_EQ(stdp_sleep_get_ratio_factor(SLEEP_STATE_REM), 1.5f);
}

TEST_F(PlasticitySleepRegressionTest, STDP_Tau_ExactValues) {
    /* Regression: Ensure tau factors haven't changed */
    EXPECT_FLOAT_EQ(stdp_sleep_get_tau_factor(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(stdp_sleep_get_tau_factor(SLEEP_STATE_DROWSY), 1.0f);
    EXPECT_FLOAT_EQ(stdp_sleep_get_tau_factor(SLEEP_STATE_LIGHT_NREM), 1.3f);
    EXPECT_FLOAT_EQ(stdp_sleep_get_tau_factor(SLEEP_STATE_DEEP_NREM), 1.5f);
    EXPECT_FLOAT_EQ(stdp_sleep_get_tau_factor(SLEEP_STATE_REM), 1.0f);
}

/* ============================================================================
 * Exact Value Regression Tests - BCM
 * ============================================================================ */

TEST_F(PlasticitySleepRegressionTest, BCM_Theta_ExactValues) {
    /* Regression: Ensure theta factors haven't changed */
    EXPECT_FLOAT_EQ(bcm_sleep_theta_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(bcm_sleep_theta_for_state(SLEEP_STATE_DROWSY), 1.1f);
    EXPECT_FLOAT_EQ(bcm_sleep_theta_for_state(SLEEP_STATE_LIGHT_NREM), 1.3f);
    EXPECT_FLOAT_EQ(bcm_sleep_theta_for_state(SLEEP_STATE_DEEP_NREM), 1.5f);
    EXPECT_FLOAT_EQ(bcm_sleep_theta_for_state(SLEEP_STATE_REM), 0.7f);
}

TEST_F(PlasticitySleepRegressionTest, BCM_LR_ExactValues) {
    /* Regression: Ensure BCM LR factors haven't changed */
    EXPECT_FLOAT_EQ(bcm_sleep_lr_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(bcm_sleep_lr_for_state(SLEEP_STATE_DROWSY), 0.6f);
    EXPECT_FLOAT_EQ(bcm_sleep_lr_for_state(SLEEP_STATE_LIGHT_NREM), 0.4f);
    EXPECT_FLOAT_EQ(bcm_sleep_lr_for_state(SLEEP_STATE_DEEP_NREM), 0.5f);
    EXPECT_FLOAT_EQ(bcm_sleep_lr_for_state(SLEEP_STATE_REM), 0.7f);
}

/* ============================================================================
 * Exact Value Regression Tests - Homeostatic
 * ============================================================================ */

TEST_F(PlasticitySleepRegressionTest, Homeostatic_Scaling_ExactValues) {
    /* Regression: Ensure scaling rates haven't changed */
    EXPECT_FLOAT_EQ(homeostatic_sleep_scaling_for_state(SLEEP_STATE_AWAKE), 0.0f);
    EXPECT_FLOAT_EQ(homeostatic_sleep_scaling_for_state(SLEEP_STATE_DROWSY), 0.2f);
    EXPECT_FLOAT_EQ(homeostatic_sleep_scaling_for_state(SLEEP_STATE_LIGHT_NREM), 0.6f);
    EXPECT_FLOAT_EQ(homeostatic_sleep_scaling_for_state(SLEEP_STATE_DEEP_NREM), 1.0f);
    EXPECT_FLOAT_EQ(homeostatic_sleep_scaling_for_state(SLEEP_STATE_REM), 0.2f);
}

TEST_F(PlasticitySleepRegressionTest, Homeostatic_Target_ExactValues) {
    /* Regression: Ensure target rates haven't changed */
    EXPECT_FLOAT_EQ(homeostatic_sleep_target_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(homeostatic_sleep_target_for_state(SLEEP_STATE_DROWSY), 1.0f);
    EXPECT_FLOAT_EQ(homeostatic_sleep_target_for_state(SLEEP_STATE_LIGHT_NREM), 0.9f);
    EXPECT_FLOAT_EQ(homeostatic_sleep_target_for_state(SLEEP_STATE_DEEP_NREM), 0.8f);
    EXPECT_FLOAT_EQ(homeostatic_sleep_target_for_state(SLEEP_STATE_REM), 1.0f);
}

/* ============================================================================
 * Exact Value Regression Tests - Neuromodulators
 * ============================================================================ */

TEST_F(PlasticitySleepRegressionTest, Neuromod_ACh_ExactValues) {
    /* Regression: Ensure ACh factors haven't changed */
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ach_factor(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ach_factor(SLEEP_STATE_DROWSY), 0.7f);
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ach_factor(SLEEP_STATE_LIGHT_NREM), 0.3f);
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ach_factor(SLEEP_STATE_DEEP_NREM), 0.1f);
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ach_factor(SLEEP_STATE_REM), 0.9f);
}

TEST_F(PlasticitySleepRegressionTest, Neuromod_NE_ExactValues) {
    /* Regression: Ensure NE factors haven't changed */
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ne_factor(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ne_factor(SLEEP_STATE_DROWSY), 0.6f);
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ne_factor(SLEEP_STATE_LIGHT_NREM), 0.3f);
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ne_factor(SLEEP_STATE_DEEP_NREM), 0.1f);
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ne_factor(SLEEP_STATE_REM), 0.05f);
}

/* ============================================================================
 * Boundary Condition Tests
 * ============================================================================ */

TEST_F(PlasticitySleepRegressionTest, InvalidState_DefaultsToAwake) {
    /* Regression: Invalid states should return awake defaults */
    sleep_state_t invalid = static_cast<sleep_state_t>(99);

    EXPECT_FLOAT_EQ(stdp_sleep_get_lr_factor(invalid), STDP_SLEEP_LR_AWAKE);
    EXPECT_FLOAT_EQ(bcm_sleep_theta_for_state(invalid), BCM_SLEEP_THETA_AWAKE);
    EXPECT_FLOAT_EQ(homeostatic_sleep_scaling_for_state(invalid), HOMEO_SLEEP_SCALE_RATE_AWAKE);
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ach_factor(invalid), SLEEP_NEUROMOD_ACH_AWAKE);
}

TEST_F(PlasticitySleepRegressionTest, DefaultConfig_NullCheck) {
    /* Regression: NULL config should return error */
    EXPECT_EQ(stdp_sleep_default_config(nullptr), -1);
    EXPECT_EQ(bcm_sleep_default_config(nullptr), -1);
    EXPECT_EQ(homeostatic_sleep_default_config(nullptr), -1);
    EXPECT_EQ(neuromod_sleep_default_config(nullptr), -1);
}

TEST_F(PlasticitySleepRegressionTest, DefaultConfig_ValidValues) {
    /* Regression: Default configs should have valid values */
    stdp_sleep_config_t stdp_cfg;
    bcm_sleep_config_t bcm_cfg;
    homeostatic_sleep_config_t homeo_cfg;
    neuromodulators_sleep_config_t neuromod_cfg;

    ASSERT_EQ(stdp_sleep_default_config(&stdp_cfg), 0);
    ASSERT_EQ(bcm_sleep_default_config(&bcm_cfg), 0);
    ASSERT_EQ(homeostatic_sleep_default_config(&homeo_cfg), 0);
    ASSERT_EQ(neuromod_sleep_default_config(&neuromod_cfg), 0);

    /* All modulation strengths should be in [0, 1] */
    EXPECT_GE(stdp_cfg.modulation_strength, 0.0f);
    EXPECT_LE(stdp_cfg.modulation_strength, 1.0f);
    EXPECT_GE(bcm_cfg.modulation_strength, 0.0f);
    EXPECT_LE(bcm_cfg.modulation_strength, 1.0f);
    EXPECT_GE(homeo_cfg.modulation_strength, 0.0f);
    EXPECT_LE(homeo_cfg.modulation_strength, 1.0f);
}

/* ============================================================================
 * Value Range Tests
 * ============================================================================ */

TEST_F(PlasticitySleepRegressionTest, AllFactors_InValidRange) {
    /* Regression: All factors should be in valid ranges */
    for (int state = SLEEP_STATE_AWAKE; state <= SLEEP_STATE_REM; state++) {
        sleep_state_t s = static_cast<sleep_state_t>(state);

        /* LR factors should be positive */
        EXPECT_GT(stdp_sleep_get_lr_factor(s), 0.0f);
        EXPECT_GT(bcm_sleep_lr_for_state(s), 0.0f);

        /* Ratio can be > 1 (LTP bias) but should be positive */
        EXPECT_GT(stdp_sleep_get_ratio_factor(s), 0.0f);

        /* Theta should be positive */
        EXPECT_GT(bcm_sleep_theta_for_state(s), 0.0f);

        /* Scaling rate should be in [0, 1] */
        EXPECT_GE(homeostatic_sleep_scaling_for_state(s), 0.0f);
        EXPECT_LE(homeostatic_sleep_scaling_for_state(s), 1.0f);

        /* Neuromodulator factors should be in [0, 1] */
        EXPECT_GE(neuromod_sleep_get_ach_factor(s), 0.0f);
        EXPECT_LE(neuromod_sleep_get_ach_factor(s), 1.0f);
        EXPECT_GE(neuromod_sleep_get_ne_factor(s), 0.0f);
        EXPECT_LE(neuromod_sleep_get_ne_factor(s), 1.0f);
    }
}

/* ============================================================================
 * Monotonicity Tests
 * ============================================================================ */

TEST_F(PlasticitySleepRegressionTest, HomeostaticScaling_Monotonic) {
    /* Regression: Scaling should increase from awake to deep NREM */
    float scale_awake = homeostatic_sleep_scaling_for_state(SLEEP_STATE_AWAKE);
    float scale_drowsy = homeostatic_sleep_scaling_for_state(SLEEP_STATE_DROWSY);
    float scale_light = homeostatic_sleep_scaling_for_state(SLEEP_STATE_LIGHT_NREM);
    float scale_deep = homeostatic_sleep_scaling_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_LE(scale_awake, scale_drowsy);
    EXPECT_LE(scale_drowsy, scale_light);
    EXPECT_LE(scale_light, scale_deep);
}

TEST_F(PlasticitySleepRegressionTest, BCMTheta_NREMElevation) {
    /* Regression: BCM theta should be elevated in NREM */
    float theta_awake = bcm_sleep_theta_for_state(SLEEP_STATE_AWAKE);
    float theta_light = bcm_sleep_theta_for_state(SLEEP_STATE_LIGHT_NREM);
    float theta_deep = bcm_sleep_theta_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_LT(theta_awake, theta_light);
    EXPECT_LT(theta_light, theta_deep);
}
