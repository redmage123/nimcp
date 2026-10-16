/**
 * @file test_stdp_sleep_bridge.cpp
 * @brief Unit tests for STDP-Sleep Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 */

#include <gtest/gtest.h>
#include "plasticity/stdp/nimcp_stdp_sleep_bridge.h"

/* ============================================================================
 * Helper Function Tests
 * ============================================================================ */

class StdpSleepHelperTest : public ::testing::Test {};

/* Learning Rate Factor Tests */
TEST_F(StdpSleepHelperTest, LrFactorAwake) {
    EXPECT_FLOAT_EQ(stdp_sleep_get_lr_factor(SLEEP_STATE_AWAKE), STDP_SLEEP_LR_AWAKE);
}

TEST_F(StdpSleepHelperTest, LrFactorDrowsy) {
    EXPECT_FLOAT_EQ(stdp_sleep_get_lr_factor(SLEEP_STATE_DROWSY), STDP_SLEEP_LR_DROWSY);
}

TEST_F(StdpSleepHelperTest, LrFactorLightNREM) {
    EXPECT_FLOAT_EQ(stdp_sleep_get_lr_factor(SLEEP_STATE_LIGHT_NREM), STDP_SLEEP_LR_LIGHT_NREM);
}

TEST_F(StdpSleepHelperTest, LrFactorDeepNREM) {
    EXPECT_FLOAT_EQ(stdp_sleep_get_lr_factor(SLEEP_STATE_DEEP_NREM), STDP_SLEEP_LR_DEEP_NREM);
}

TEST_F(StdpSleepHelperTest, LrFactorREM) {
    EXPECT_FLOAT_EQ(stdp_sleep_get_lr_factor(SLEEP_STATE_REM), STDP_SLEEP_LR_REM);
}

/* LTP/LTD Ratio Factor Tests */
TEST_F(StdpSleepHelperTest, RatioFactorAwake) {
    EXPECT_FLOAT_EQ(stdp_sleep_get_ratio_factor(SLEEP_STATE_AWAKE), STDP_SLEEP_RATIO_AWAKE);
}

TEST_F(StdpSleepHelperTest, RatioFactorDrowsy) {
    EXPECT_FLOAT_EQ(stdp_sleep_get_ratio_factor(SLEEP_STATE_DROWSY), STDP_SLEEP_RATIO_DROWSY);
}

TEST_F(StdpSleepHelperTest, RatioFactorLightNREM) {
    EXPECT_FLOAT_EQ(stdp_sleep_get_ratio_factor(SLEEP_STATE_LIGHT_NREM), STDP_SLEEP_RATIO_LIGHT_NREM);
}

TEST_F(StdpSleepHelperTest, RatioFactorDeepNREM) {
    /* Deep NREM should favor LTD (synaptic downscaling) */
    float ratio = stdp_sleep_get_ratio_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(ratio, STDP_SLEEP_RATIO_DEEP_NREM);
    EXPECT_LT(ratio, 1.0f) << "Deep NREM should favor LTD (ratio < 1)";
}

TEST_F(StdpSleepHelperTest, RatioFactorREM) {
    /* REM should favor LTP (consolidation) */
    float ratio = stdp_sleep_get_ratio_factor(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(ratio, STDP_SLEEP_RATIO_REM);
    EXPECT_GT(ratio, 1.0f) << "REM should favor LTP (ratio > 1)";
}

/* Tau (Timing Window) Factor Tests */
TEST_F(StdpSleepHelperTest, TauFactorAwake) {
    EXPECT_FLOAT_EQ(stdp_sleep_get_tau_factor(SLEEP_STATE_AWAKE), STDP_SLEEP_TAU_AWAKE);
}

TEST_F(StdpSleepHelperTest, TauFactorDeepNREM) {
    /* Deep NREM should have widest timing window for replay */
    float tau = stdp_sleep_get_tau_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(tau, STDP_SLEEP_TAU_DEEP_NREM);
    EXPECT_GT(tau, 1.0f) << "Deep NREM should widen timing window";
}

TEST_F(StdpSleepHelperTest, TauFactorREM) {
    EXPECT_FLOAT_EQ(stdp_sleep_get_tau_factor(SLEEP_STATE_REM), STDP_SLEEP_TAU_REM);
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(StdpSleepHelperTest, DefaultConfigValid) {
    stdp_sleep_config_t config;
    ASSERT_EQ(stdp_sleep_default_config(&config), 0);

    EXPECT_TRUE(config.enable_lr_modulation);
    EXPECT_TRUE(config.enable_ratio_modulation);
    EXPECT_TRUE(config.enable_window_modulation);
    EXPECT_GT(config.modulation_strength, 0.0f);
    EXPECT_LE(config.modulation_strength, 1.0f);
}

TEST_F(StdpSleepHelperTest, DefaultConfigNullReturnsError) {
    EXPECT_EQ(stdp_sleep_default_config(nullptr), -1);
}

/* ============================================================================
 * Biological Validity Tests (Synaptic Homeostasis Hypothesis)
 * ============================================================================ */

TEST_F(StdpSleepHelperTest, SynapticDownscalingInDeepNREM) {
    /* Tononi's SHY: Deep NREM should favor LTD for net downscaling */
    float ratio = stdp_sleep_get_ratio_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_LT(ratio, 0.6f) << "Deep NREM should strongly favor LTD (downscaling)";
}

TEST_F(StdpSleepHelperTest, ConsolidationInREM) {
    /* REM sleep should favor LTP for memory consolidation */
    float ratio = stdp_sleep_get_ratio_factor(SLEEP_STATE_REM);
    EXPECT_GT(ratio, 1.3f) << "REM should strongly favor LTP (consolidation)";
}

TEST_F(StdpSleepHelperTest, ReplayFacilitationInNREM) {
    /* NREM should widen timing windows to facilitate replay-based learning */
    float tau_light = stdp_sleep_get_tau_factor(SLEEP_STATE_LIGHT_NREM);
    float tau_deep = stdp_sleep_get_tau_factor(SLEEP_STATE_DEEP_NREM);
    float tau_awake = stdp_sleep_get_tau_factor(SLEEP_STATE_AWAKE);

    EXPECT_GT(tau_light, tau_awake) << "Light NREM should widen timing window";
    EXPECT_GT(tau_deep, tau_light) << "Deep NREM should have widest timing window";
}

TEST_F(StdpSleepHelperTest, ReducedLearningDuringTransition) {
    /* Drowsy state should reduce learning rate */
    float lr_awake = stdp_sleep_get_lr_factor(SLEEP_STATE_AWAKE);
    float lr_drowsy = stdp_sleep_get_lr_factor(SLEEP_STATE_DROWSY);

    EXPECT_LT(lr_drowsy, lr_awake) << "Drowsy should reduce learning rate";
}

TEST_F(StdpSleepHelperTest, LearningRateMonotonicity) {
    /* Learning rate should generally decrease as sleep deepens (except REM) */
    float lr_awake = stdp_sleep_get_lr_factor(SLEEP_STATE_AWAKE);
    float lr_drowsy = stdp_sleep_get_lr_factor(SLEEP_STATE_DROWSY);
    float lr_light = stdp_sleep_get_lr_factor(SLEEP_STATE_LIGHT_NREM);

    EXPECT_GE(lr_awake, lr_drowsy);
    EXPECT_GE(lr_drowsy, lr_light);
}
