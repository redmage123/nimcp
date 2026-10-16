/**
 * @file test_plasticity_sleep_integration.cpp
 * @brief Integration tests for Plasticity-Sleep bridges working together
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Test all plasticity sleep bridges coordinating during sleep cycles
 * WHY:  Verify that STDP, BCM, Homeostatic, and Neuromodulator bridges
 *       correctly interact during sleep state transitions
 * HOW:  Simulate sleep cycles and verify coordinated plasticity changes
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "plasticity/neuromodulators/nimcp_neuromodulators_sleep_bridge.h"
#include "plasticity/stdp/nimcp_stdp_sleep_bridge.h"
#include "plasticity/bcm/nimcp_bcm_sleep_bridge.h"
#include "plasticity/homeostatic/nimcp_homeostatic_sleep_bridge.h"

/* ============================================================================
 * Integration Test Fixture
 * ============================================================================ */

class PlasticitySleepIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Integration tests use helper functions only - no real sleep system */
    }

    void TearDown() override {
    }

    /* Simulate getting effects for a given sleep state */
    void getEffectsForState(sleep_state_t state,
                           float& stdp_lr, float& stdp_ratio,
                           float& bcm_theta, float& bcm_lr,
                           float& homeo_scaling, float& homeo_target,
                           float& neuromod_ach, float& neuromod_ne) {
        stdp_lr = stdp_sleep_get_lr_factor(state);
        stdp_ratio = stdp_sleep_get_ratio_factor(state);
        bcm_theta = bcm_sleep_theta_for_state(state);
        bcm_lr = bcm_sleep_lr_for_state(state);
        homeo_scaling = homeostatic_sleep_scaling_for_state(state);
        homeo_target = homeostatic_sleep_target_for_state(state);
        neuromod_ach = neuromod_sleep_get_ach_factor(state);
        neuromod_ne = neuromod_sleep_get_ne_factor(state);
    }
};

/* ============================================================================
 * Coordinated Sleep State Tests
 * ============================================================================ */

TEST_F(PlasticitySleepIntegrationTest, AwakeState_AllSystemsBaseline) {
    /* WHAT: Verify all systems at baseline during awake state
     * WHY:  Awake is the reference state for all modulations
     * HOW:  Check all factors are at or near 1.0
     */
    float stdp_lr, stdp_ratio, bcm_theta, bcm_lr;
    float homeo_scaling, homeo_target, neuromod_ach, neuromod_ne;

    getEffectsForState(SLEEP_STATE_AWAKE,
                      stdp_lr, stdp_ratio, bcm_theta, bcm_lr,
                      homeo_scaling, homeo_target, neuromod_ach, neuromod_ne);

    /* All learning systems at baseline */
    EXPECT_FLOAT_EQ(stdp_lr, 1.0f) << "STDP LR at baseline";
    EXPECT_FLOAT_EQ(stdp_ratio, 1.0f) << "STDP ratio balanced";
    EXPECT_FLOAT_EQ(bcm_theta, 1.0f) << "BCM theta at baseline";
    EXPECT_FLOAT_EQ(bcm_lr, 1.0f) << "BCM LR at baseline";

    /* No homeostatic scaling during wake */
    EXPECT_FLOAT_EQ(homeo_scaling, 0.0f) << "No scaling during wake (SHY)";
    EXPECT_FLOAT_EQ(homeo_target, 1.0f) << "Target unchanged during wake";

    /* Full neuromodulator activity */
    EXPECT_FLOAT_EQ(neuromod_ach, 1.0f) << "Full ACh during wake";
    EXPECT_FLOAT_EQ(neuromod_ne, 1.0f) << "Full NE during wake";
}

TEST_F(PlasticitySleepIntegrationTest, DeepNREM_SynapticDownscaling) {
    /* WHAT: Verify coordinated downscaling during deep NREM
     * WHY:  Tononi's SHY predicts maximal synaptic downscaling in deep NREM
     * HOW:  Check all systems favor LTD/downscaling
     */
    float stdp_lr, stdp_ratio, bcm_theta, bcm_lr;
    float homeo_scaling, homeo_target, neuromod_ach, neuromod_ne;

    getEffectsForState(SLEEP_STATE_DEEP_NREM,
                      stdp_lr, stdp_ratio, bcm_theta, bcm_lr,
                      homeo_scaling, homeo_target, neuromod_ach, neuromod_ne);

    /* STDP should favor LTD */
    EXPECT_LT(stdp_ratio, 1.0f) << "STDP favors LTD in deep NREM";
    EXPECT_LT(stdp_ratio, 0.6f) << "Strong LTD bias expected";

    /* BCM theta elevated → more LTD */
    EXPECT_GT(bcm_theta, 1.0f) << "BCM theta elevated";
    EXPECT_GT(bcm_theta, 1.4f) << "Significantly elevated for downscaling";

    /* Homeostatic scaling maximal */
    EXPECT_FLOAT_EQ(homeo_scaling, 1.0f) << "Maximum scaling rate";
    EXPECT_LT(homeo_target, 1.0f) << "Reduced target (downscaling goal)";

    /* Neuromodulators suppressed */
    EXPECT_LT(neuromod_ach, 0.2f) << "ACh suppressed in deep NREM";
    EXPECT_LT(neuromod_ne, 0.2f) << "NE suppressed in deep NREM";
}

TEST_F(PlasticitySleepIntegrationTest, REM_ConsolidationMode) {
    /* WHAT: Verify REM favors LTP for memory consolidation
     * WHY:  REM sleep consolidates important memories via LTP
     * HOW:  Check systems favor LTP, reduced downscaling
     */
    float stdp_lr, stdp_ratio, bcm_theta, bcm_lr;
    float homeo_scaling, homeo_target, neuromod_ach, neuromod_ne;

    getEffectsForState(SLEEP_STATE_REM,
                      stdp_lr, stdp_ratio, bcm_theta, bcm_lr,
                      homeo_scaling, homeo_target, neuromod_ach, neuromod_ne);

    /* STDP should favor LTP */
    EXPECT_GT(stdp_ratio, 1.0f) << "STDP favors LTP in REM";
    EXPECT_GT(stdp_ratio, 1.3f) << "Strong LTP bias for consolidation";

    /* BCM theta lowered → easier LTP */
    EXPECT_LT(bcm_theta, 1.0f) << "BCM theta lowered";
    EXPECT_LT(bcm_theta, 0.8f) << "Significantly lowered for LTP";

    /* Reduced homeostatic scaling */
    EXPECT_LT(homeo_scaling, 0.5f) << "Reduced scaling during REM";

    /* REM neuromodulator profile: high ACh, low NE */
    EXPECT_GT(neuromod_ach, 0.8f) << "High ACh in REM (dream state)";
    EXPECT_LT(neuromod_ne, 0.1f) << "NE near zero in REM (LC silent)";
}

TEST_F(PlasticitySleepIntegrationTest, DrowsyTransition_GradualChanges) {
    /* WHAT: Verify drowsy state is intermediate between awake and sleep
     * WHY:  Drowsy is a transition state, should show partial effects
     * HOW:  Check values between awake and NREM
     */
    float stdp_lr_awake, stdp_ratio_awake, bcm_theta_awake, bcm_lr_awake;
    float homeo_scaling_awake, homeo_target_awake, neuromod_ach_awake, neuromod_ne_awake;

    float stdp_lr_drowsy, stdp_ratio_drowsy, bcm_theta_drowsy, bcm_lr_drowsy;
    float homeo_scaling_drowsy, homeo_target_drowsy, neuromod_ach_drowsy, neuromod_ne_drowsy;

    float stdp_lr_nrem, stdp_ratio_nrem, bcm_theta_nrem, bcm_lr_nrem;
    float homeo_scaling_nrem, homeo_target_nrem, neuromod_ach_nrem, neuromod_ne_nrem;

    getEffectsForState(SLEEP_STATE_AWAKE,
                      stdp_lr_awake, stdp_ratio_awake, bcm_theta_awake, bcm_lr_awake,
                      homeo_scaling_awake, homeo_target_awake, neuromod_ach_awake, neuromod_ne_awake);

    getEffectsForState(SLEEP_STATE_DROWSY,
                      stdp_lr_drowsy, stdp_ratio_drowsy, bcm_theta_drowsy, bcm_lr_drowsy,
                      homeo_scaling_drowsy, homeo_target_drowsy, neuromod_ach_drowsy, neuromod_ne_drowsy);

    getEffectsForState(SLEEP_STATE_LIGHT_NREM,
                      stdp_lr_nrem, stdp_ratio_nrem, bcm_theta_nrem, bcm_lr_nrem,
                      homeo_scaling_nrem, homeo_target_nrem, neuromod_ach_nrem, neuromod_ne_nrem);

    /* Drowsy should be between awake and NREM */
    EXPECT_LE(stdp_lr_drowsy, stdp_lr_awake) << "STDP LR reduced from awake";
    EXPECT_GE(stdp_lr_drowsy, stdp_lr_nrem) << "STDP LR higher than NREM";

    EXPECT_GE(bcm_theta_drowsy, bcm_theta_awake) << "BCM theta rising";
    EXPECT_LE(bcm_theta_drowsy, bcm_theta_nrem) << "BCM theta lower than NREM";

    EXPECT_GE(homeo_scaling_drowsy, homeo_scaling_awake) << "Scaling starting";
    EXPECT_LE(homeo_scaling_drowsy, homeo_scaling_nrem) << "Scaling less than NREM";
}

/* ============================================================================
 * Sleep Cycle Simulation Tests
 * ============================================================================ */

TEST_F(PlasticitySleepIntegrationTest, SleepCycle_NetDownscaling) {
    /* WHAT: Simulate a sleep cycle and verify net effect
     * WHY:  A full sleep cycle should result in net synaptic downscaling
     * HOW:  Accumulate LTP/LTD bias across cycle stages
     */
    sleep_state_t cycle[] = {
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_REM,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_REM
    };

    float net_stdp_bias = 0.0f;
    float net_bcm_bias = 0.0f;
    float total_scaling = 0.0f;

    for (auto state : cycle) {
        float stdp_ratio = stdp_sleep_get_ratio_factor(state);
        float bcm_theta = bcm_sleep_theta_for_state(state);
        float scaling = homeostatic_sleep_scaling_for_state(state);

        /* Accumulate biases (ratio < 1 = LTD, theta > 1 = LTD) */
        net_stdp_bias += (stdp_ratio - 1.0f);
        net_bcm_bias += (bcm_theta - 1.0f);
        total_scaling += scaling;
    }

    /* Net effect should favor LTD (negative STDP bias, positive BCM bias) */
    EXPECT_LT(net_stdp_bias, 0.0f) << "Net STDP bias toward LTD";
    EXPECT_GT(net_bcm_bias, 0.0f) << "Net BCM bias toward LTD";
    EXPECT_GT(total_scaling, 0.0f) << "Significant scaling occurred";
}

TEST_F(PlasticitySleepIntegrationTest, NREMvsREM_OppositeEffects) {
    /* WHAT: Verify NREM and REM have opposite plasticity effects
     * WHY:  NREM downscales, REM consolidates - fundamentally different
     * HOW:  Compare STDP ratio and BCM theta
     */
    float stdp_ratio_nrem = stdp_sleep_get_ratio_factor(SLEEP_STATE_DEEP_NREM);
    float stdp_ratio_rem = stdp_sleep_get_ratio_factor(SLEEP_STATE_REM);

    float bcm_theta_nrem = bcm_sleep_theta_for_state(SLEEP_STATE_DEEP_NREM);
    float bcm_theta_rem = bcm_sleep_theta_for_state(SLEEP_STATE_REM);

    /* STDP: NREM favors LTD (<1), REM favors LTP (>1) */
    EXPECT_LT(stdp_ratio_nrem, 1.0f) << "NREM: LTD bias";
    EXPECT_GT(stdp_ratio_rem, 1.0f) << "REM: LTP bias";
    EXPECT_GT(stdp_ratio_rem - stdp_ratio_nrem, 0.5f) << "Significant difference";

    /* BCM: NREM elevates theta (LTD), REM lowers theta (LTP) */
    EXPECT_GT(bcm_theta_nrem, 1.0f) << "NREM: elevated theta";
    EXPECT_LT(bcm_theta_rem, 1.0f) << "REM: lowered theta";
    EXPECT_GT(bcm_theta_nrem - bcm_theta_rem, 0.5f) << "Significant difference";
}

/* ============================================================================
 * Neuromodulator-Plasticity Coordination Tests
 * ============================================================================ */

TEST_F(PlasticitySleepIntegrationTest, LowACh_ReducedEncoding) {
    /* WHAT: Verify low ACh states have reduced learning
     * WHY:  ACh is necessary for encoding new memories
     * HOW:  States with low ACh should have reduced LR
     */
    /* Deep NREM has very low ACh and should have reduced online learning */
    float ach_deep = neuromod_sleep_get_ach_factor(SLEEP_STATE_DEEP_NREM);
    float stdp_lr_deep = stdp_sleep_get_lr_factor(SLEEP_STATE_DEEP_NREM);

    EXPECT_LT(ach_deep, 0.2f) << "Very low ACh in deep NREM";
    EXPECT_LT(stdp_lr_deep, stdp_sleep_get_lr_factor(SLEEP_STATE_AWAKE))
        << "Reduced encoding with low ACh";
}

TEST_F(PlasticitySleepIntegrationTest, LowNE_EnablesDownscaling) {
    /* WHAT: Verify low NE enables homeostatic scaling
     * WHY:  NE suppresses homeostatic mechanisms during wake
     * HOW:  Low NE states should have high scaling rates
     */
    /* Deep NREM has very low NE and maximum scaling */
    float ne_deep = neuromod_sleep_get_ne_factor(SLEEP_STATE_DEEP_NREM);
    float scaling_deep = homeostatic_sleep_scaling_for_state(SLEEP_STATE_DEEP_NREM);

    /* Awake has high NE and no scaling */
    float ne_awake = neuromod_sleep_get_ne_factor(SLEEP_STATE_AWAKE);
    float scaling_awake = homeostatic_sleep_scaling_for_state(SLEEP_STATE_AWAKE);

    EXPECT_GT(ne_awake, ne_deep) << "NE higher when awake";
    EXPECT_GT(scaling_deep, scaling_awake) << "More scaling with low NE";
    EXPECT_FLOAT_EQ(scaling_awake, 0.0f) << "No scaling with full NE";
}

/* ============================================================================
 * STDP Window Modulation Tests
 * ============================================================================ */

TEST_F(PlasticitySleepIntegrationTest, TimingWindow_WiderDuringNREM) {
    /* WHAT: Verify STDP timing windows widen during NREM
     * WHY:  Wider windows facilitate replay-based learning
     * HOW:  Compare tau factors across states
     */
    float tau_awake = stdp_sleep_get_tau_factor(SLEEP_STATE_AWAKE);
    float tau_light = stdp_sleep_get_tau_factor(SLEEP_STATE_LIGHT_NREM);
    float tau_deep = stdp_sleep_get_tau_factor(SLEEP_STATE_DEEP_NREM);
    float tau_rem = stdp_sleep_get_tau_factor(SLEEP_STATE_REM);

    /* NREM should have wider windows than awake */
    EXPECT_GT(tau_light, tau_awake) << "Light NREM widens window";
    EXPECT_GT(tau_deep, tau_light) << "Deep NREM has widest window";

    /* REM returns to normal window */
    EXPECT_LT(tau_rem, tau_deep) << "REM narrows window vs NREM";
}
