/**
 * @file test_homeostatic_sleep_bridge.cpp
 * @brief Unit tests for Homeostatic-Sleep Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 */

#include <gtest/gtest.h>
#include "plasticity/homeostatic/nimcp_homeostatic_sleep_bridge.h"

/* ============================================================================
 * Helper Function Tests
 * ============================================================================ */

class HomeostaticSleepHelperTest : public ::testing::Test {};

/* Scaling Rate Factor Tests */
TEST_F(HomeostaticSleepHelperTest, ScalingRateAwake) {
    /* No scaling during wake - SHY hypothesis */
    float rate = homeostatic_sleep_scaling_for_state(SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(rate, HOMEO_SLEEP_SCALE_RATE_AWAKE);
    EXPECT_FLOAT_EQ(rate, 0.0f) << "No homeostatic scaling during wake";
}

TEST_F(HomeostaticSleepHelperTest, ScalingRateDrowsy) {
    EXPECT_FLOAT_EQ(homeostatic_sleep_scaling_for_state(SLEEP_STATE_DROWSY),
                    HOMEO_SLEEP_SCALE_RATE_DROWSY);
}

TEST_F(HomeostaticSleepHelperTest, ScalingRateLightNREM) {
    float rate = homeostatic_sleep_scaling_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(rate, HOMEO_SLEEP_SCALE_RATE_LIGHT_NREM);
    EXPECT_GT(rate, 0.5f) << "Active scaling in light NREM";
}

TEST_F(HomeostaticSleepHelperTest, ScalingRateDeepNREM) {
    /* Maximum scaling during deep NREM - core of SHY */
    float rate = homeostatic_sleep_scaling_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(rate, HOMEO_SLEEP_SCALE_RATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(rate, 1.0f) << "Maximum scaling in deep NREM";
}

TEST_F(HomeostaticSleepHelperTest, ScalingRateREM) {
    float rate = homeostatic_sleep_scaling_for_state(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(rate, HOMEO_SLEEP_SCALE_RATE_REM);
    EXPECT_LT(rate, 0.3f) << "Reduced scaling during REM for consolidation";
}

/* Target Rate Modifier Tests */
TEST_F(HomeostaticSleepHelperTest, TargetRateAwake) {
    EXPECT_FLOAT_EQ(homeostatic_sleep_target_for_state(SLEEP_STATE_AWAKE),
                    HOMEO_SLEEP_TARGET_AWAKE);
}

TEST_F(HomeostaticSleepHelperTest, TargetRateDeepNREM) {
    /* Deep NREM should have 20% lower target (downscaling) */
    float target = homeostatic_sleep_target_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(target, HOMEO_SLEEP_TARGET_DEEP_NREM);
    EXPECT_FLOAT_EQ(target, 0.8f) << "20% lower target in deep NREM";
}

/* Pruning Threshold Tests */
TEST_F(HomeostaticSleepHelperTest, PruningAwake) {
    float prune = homeostatic_sleep_pruning_for_state(SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(prune, HOMEO_SLEEP_PRUNE_AWAKE);
    EXPECT_FLOAT_EQ(prune, 0.0f) << "No pruning during wake";
}

TEST_F(HomeostaticSleepHelperTest, PruningDeepNREM) {
    /* Maximum pruning during deep NREM */
    float prune = homeostatic_sleep_pruning_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(prune, HOMEO_SLEEP_PRUNE_DEEP_NREM);
    EXPECT_FLOAT_EQ(prune, 1.0f) << "Maximum pruning in deep NREM";
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(HomeostaticSleepHelperTest, DefaultConfigValid) {
    homeostatic_sleep_config_t config;
    ASSERT_EQ(homeostatic_sleep_default_config(&config), 0);

    EXPECT_TRUE(config.enable_scaling_modulation);
    EXPECT_TRUE(config.enable_target_modulation);
    EXPECT_TRUE(config.enable_pruning_modulation);
    EXPECT_GT(config.modulation_strength, 0.0f);
    EXPECT_GT(config.deep_nrem_scaling_boost, 1.0f) << "Deep NREM should have scaling boost";
}

TEST_F(HomeostaticSleepHelperTest, DefaultConfigNullReturnsError) {
    EXPECT_EQ(homeostatic_sleep_default_config(nullptr), -1);
}

/* ============================================================================
 * Biological Validity Tests (Synaptic Homeostasis Hypothesis - Tononi)
 * ============================================================================ */

TEST_F(HomeostaticSleepHelperTest, SHY_NoScalingDuringWake) {
    /* Core SHY: Homeostatic scaling is OFF during wakefulness */
    float rate = homeostatic_sleep_scaling_for_state(SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(rate, 0.0f) << "SHY: No scaling during wake";
}

TEST_F(HomeostaticSleepHelperTest, SHY_MaxScalingDuringDeepNREM) {
    /* Core SHY: Maximum scaling during deep NREM (slow wave sleep) */
    float rate = homeostatic_sleep_scaling_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(rate, 1.0f) << "SHY: Maximum scaling in deep NREM";
}

TEST_F(HomeostaticSleepHelperTest, SHY_ScalingMonotonicity) {
    /* Scaling should increase as sleep deepens (until REM) */
    float rate_awake = homeostatic_sleep_scaling_for_state(SLEEP_STATE_AWAKE);
    float rate_drowsy = homeostatic_sleep_scaling_for_state(SLEEP_STATE_DROWSY);
    float rate_light = homeostatic_sleep_scaling_for_state(SLEEP_STATE_LIGHT_NREM);
    float rate_deep = homeostatic_sleep_scaling_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_LE(rate_awake, rate_drowsy);
    EXPECT_LE(rate_drowsy, rate_light);
    EXPECT_LE(rate_light, rate_deep);
}

TEST_F(HomeostaticSleepHelperTest, SHY_ReducedScalingInREM) {
    /* REM should have reduced scaling to allow consolidation */
    float rate_deep = homeostatic_sleep_scaling_for_state(SLEEP_STATE_DEEP_NREM);
    float rate_rem = homeostatic_sleep_scaling_for_state(SLEEP_STATE_REM);

    EXPECT_LT(rate_rem, rate_deep) << "REM should reduce scaling vs deep NREM";
}

TEST_F(HomeostaticSleepHelperTest, SHY_TargetReduction) {
    /* Target firing rate should decrease during NREM (downscaling goal) */
    float target_awake = homeostatic_sleep_target_for_state(SLEEP_STATE_AWAKE);
    float target_deep = homeostatic_sleep_target_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_LT(target_deep, target_awake) << "Target should be lower during deep NREM";
}

TEST_F(HomeostaticSleepHelperTest, SHY_PruningDuringNREM) {
    /* Weak synapses should be pruned during NREM */
    float prune_awake = homeostatic_sleep_pruning_for_state(SLEEP_STATE_AWAKE);
    float prune_deep = homeostatic_sleep_pruning_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_GT(prune_deep, prune_awake) << "Pruning should be active during deep NREM";
    EXPECT_FLOAT_EQ(prune_deep, 1.0f) << "Maximum pruning in deep NREM";
}
