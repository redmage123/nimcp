/**
 * @file test_bcm_sleep_bridge.cpp
 * @brief Unit tests for BCM-Sleep Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 */

#include <gtest/gtest.h>
#include "plasticity/bcm/nimcp_bcm_sleep_bridge.h"

/* ============================================================================
 * Helper Function Tests
 * ============================================================================ */

class BcmSleepHelperTest : public ::testing::Test {};

/* Theta (Sliding Threshold) Factor Tests */
TEST_F(BcmSleepHelperTest, ThetaFactorAwake) {
    EXPECT_FLOAT_EQ(bcm_sleep_theta_for_state(SLEEP_STATE_AWAKE), BCM_SLEEP_THETA_AWAKE);
}

TEST_F(BcmSleepHelperTest, ThetaFactorDrowsy) {
    EXPECT_FLOAT_EQ(bcm_sleep_theta_for_state(SLEEP_STATE_DROWSY), BCM_SLEEP_THETA_DROWSY);
}

TEST_F(BcmSleepHelperTest, ThetaFactorLightNREM) {
    EXPECT_FLOAT_EQ(bcm_sleep_theta_for_state(SLEEP_STATE_LIGHT_NREM), BCM_SLEEP_THETA_LIGHT_NREM);
}

TEST_F(BcmSleepHelperTest, ThetaFactorDeepNREM) {
    /* Deep NREM should have highest theta (favors LTD) */
    float theta = bcm_sleep_theta_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(theta, BCM_SLEEP_THETA_DEEP_NREM);
    EXPECT_GT(theta, 1.0f) << "Deep NREM should elevate theta for downscaling";
}

TEST_F(BcmSleepHelperTest, ThetaFactorREM) {
    /* REM should have lowest theta (favors LTP) */
    float theta = bcm_sleep_theta_for_state(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(theta, BCM_SLEEP_THETA_REM);
    EXPECT_LT(theta, 1.0f) << "REM should lower theta for consolidation";
}

/* Learning Rate Factor Tests */
TEST_F(BcmSleepHelperTest, LrFactorAwake) {
    EXPECT_FLOAT_EQ(bcm_sleep_lr_for_state(SLEEP_STATE_AWAKE), BCM_SLEEP_LR_AWAKE);
}

TEST_F(BcmSleepHelperTest, LrFactorDrowsy) {
    EXPECT_FLOAT_EQ(bcm_sleep_lr_for_state(SLEEP_STATE_DROWSY), BCM_SLEEP_LR_DROWSY);
}

TEST_F(BcmSleepHelperTest, LrFactorLightNREM) {
    EXPECT_FLOAT_EQ(bcm_sleep_lr_for_state(SLEEP_STATE_LIGHT_NREM), BCM_SLEEP_LR_LIGHT_NREM);
}

TEST_F(BcmSleepHelperTest, LrFactorDeepNREM) {
    EXPECT_FLOAT_EQ(bcm_sleep_lr_for_state(SLEEP_STATE_DEEP_NREM), BCM_SLEEP_LR_DEEP_NREM);
}

TEST_F(BcmSleepHelperTest, LrFactorREM) {
    EXPECT_FLOAT_EQ(bcm_sleep_lr_for_state(SLEEP_STATE_REM), BCM_SLEEP_LR_REM);
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(BcmSleepHelperTest, DefaultConfigValid) {
    bcm_sleep_config_t config;
    ASSERT_EQ(bcm_sleep_default_config(&config), 0);

    EXPECT_TRUE(config.enable_theta_modulation);
    EXPECT_TRUE(config.enable_lr_modulation);
    EXPECT_GT(config.modulation_strength, 0.0f);
    EXPECT_LE(config.modulation_strength, 1.0f);
}

TEST_F(BcmSleepHelperTest, DefaultConfigNullReturnsError) {
    EXPECT_EQ(bcm_sleep_default_config(nullptr), -1);
}

/* ============================================================================
 * Biological Validity Tests
 * ============================================================================ */

TEST_F(BcmSleepHelperTest, ThetaElevationDuringNREM) {
    /* BCM theta should be elevated during NREM to favor LTD (synaptic downscaling) */
    float theta_awake = bcm_sleep_theta_for_state(SLEEP_STATE_AWAKE);
    float theta_light = bcm_sleep_theta_for_state(SLEEP_STATE_LIGHT_NREM);
    float theta_deep = bcm_sleep_theta_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_GT(theta_light, theta_awake) << "Light NREM should elevate theta";
    EXPECT_GT(theta_deep, theta_light) << "Deep NREM should have highest theta";
}

TEST_F(BcmSleepHelperTest, ThetaDepressionDuringREM) {
    /* BCM theta should be depressed during REM to favor LTP (consolidation) */
    float theta_awake = bcm_sleep_theta_for_state(SLEEP_STATE_AWAKE);
    float theta_rem = bcm_sleep_theta_for_state(SLEEP_STATE_REM);

    EXPECT_LT(theta_rem, theta_awake) << "REM should lower theta for consolidation";
}

TEST_F(BcmSleepHelperTest, AwakeIsBaseline) {
    /* Awake state should be baseline (1.0) */
    EXPECT_FLOAT_EQ(bcm_sleep_theta_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(bcm_sleep_lr_for_state(SLEEP_STATE_AWAKE), 1.0f);
}

TEST_F(BcmSleepHelperTest, LrReducedDuringTransitions) {
    /* Learning rate should be reduced during sleep transitions */
    float lr_awake = bcm_sleep_lr_for_state(SLEEP_STATE_AWAKE);
    float lr_drowsy = bcm_sleep_lr_for_state(SLEEP_STATE_DROWSY);
    float lr_light = bcm_sleep_lr_for_state(SLEEP_STATE_LIGHT_NREM);

    EXPECT_LT(lr_drowsy, lr_awake);
    EXPECT_LT(lr_light, lr_drowsy);
}

TEST_F(BcmSleepHelperTest, DownscalingRegime) {
    /* Deep NREM: high theta + moderate LR = downscaling */
    float theta = bcm_sleep_theta_for_state(SLEEP_STATE_DEEP_NREM);
    float lr = bcm_sleep_lr_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_GT(theta, 1.3f) << "High theta needed for effective downscaling";
    EXPECT_GT(lr, 0.3f) << "LR must be non-zero for downscaling to occur";
}

TEST_F(BcmSleepHelperTest, ConsolidationRegime) {
    /* REM: low theta + moderate LR = consolidation */
    float theta = bcm_sleep_theta_for_state(SLEEP_STATE_REM);
    float lr = bcm_sleep_lr_for_state(SLEEP_STATE_REM);

    EXPECT_LT(theta, 0.8f) << "Low theta needed for consolidation";
    EXPECT_GT(lr, 0.5f) << "LR should be elevated for consolidation";
}
