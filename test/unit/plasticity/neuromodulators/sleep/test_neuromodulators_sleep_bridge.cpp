/**
 * @file test_neuromodulators_sleep_bridge.cpp
 * @brief Unit tests for Neuromodulators-Sleep Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 */

#include <gtest/gtest.h>
#include "plasticity/neuromodulators/nimcp_neuromodulators_sleep_bridge.h"

/* ============================================================================
 * Helper Function Tests (Pure functions, no fixtures needed)
 * ============================================================================ */

class NeuromodSleepHelperTest : public ::testing::Test {};

TEST_F(NeuromodSleepHelperTest, AchFactorAwake) {
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ach_factor(SLEEP_STATE_AWAKE), SLEEP_NEUROMOD_ACH_AWAKE);
}

TEST_F(NeuromodSleepHelperTest, AchFactorDrowsy) {
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ach_factor(SLEEP_STATE_DROWSY), SLEEP_NEUROMOD_ACH_DROWSY);
}

TEST_F(NeuromodSleepHelperTest, AchFactorLightNREM) {
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ach_factor(SLEEP_STATE_LIGHT_NREM), SLEEP_NEUROMOD_ACH_LIGHT_NREM);
}

TEST_F(NeuromodSleepHelperTest, AchFactorDeepNREM) {
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ach_factor(SLEEP_STATE_DEEP_NREM), SLEEP_NEUROMOD_ACH_DEEP_NREM);
}

TEST_F(NeuromodSleepHelperTest, AchFactorREM) {
    /* ACh high in REM for dream state reactivation */
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ach_factor(SLEEP_STATE_REM), SLEEP_NEUROMOD_ACH_REM);
    EXPECT_GT(neuromod_sleep_get_ach_factor(SLEEP_STATE_REM), 0.8f);
}

TEST_F(NeuromodSleepHelperTest, NeFactorAwake) {
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ne_factor(SLEEP_STATE_AWAKE), SLEEP_NEUROMOD_NE_AWAKE);
}

TEST_F(NeuromodSleepHelperTest, NeFactorDrowsy) {
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ne_factor(SLEEP_STATE_DROWSY), SLEEP_NEUROMOD_NE_DROWSY);
}

TEST_F(NeuromodSleepHelperTest, NeFactorLightNREM) {
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ne_factor(SLEEP_STATE_LIGHT_NREM), SLEEP_NEUROMOD_NE_LIGHT_NREM);
}

TEST_F(NeuromodSleepHelperTest, NeFactorDeepNREM) {
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ne_factor(SLEEP_STATE_DEEP_NREM), SLEEP_NEUROMOD_NE_DEEP_NREM);
}

TEST_F(NeuromodSleepHelperTest, NeFactorREM) {
    /* NE near zero in REM - locus coeruleus silent */
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ne_factor(SLEEP_STATE_REM), SLEEP_NEUROMOD_NE_REM);
    EXPECT_LT(neuromod_sleep_get_ne_factor(SLEEP_STATE_REM), 0.1f);
}

TEST_F(NeuromodSleepHelperTest, DaFactorAwake) {
    EXPECT_FLOAT_EQ(neuromod_sleep_get_da_factor(SLEEP_STATE_AWAKE), SLEEP_NEUROMOD_DA_AWAKE);
}

TEST_F(NeuromodSleepHelperTest, DaFactorDeepNREM) {
    EXPECT_FLOAT_EQ(neuromod_sleep_get_da_factor(SLEEP_STATE_DEEP_NREM), SLEEP_NEUROMOD_DA_DEEP_NREM);
}

TEST_F(NeuromodSleepHelperTest, SerotoninFactorAwake) {
    EXPECT_FLOAT_EQ(neuromod_sleep_get_serotonin_factor(SLEEP_STATE_AWAKE), SLEEP_NEUROMOD_5HT_AWAKE);
}

TEST_F(NeuromodSleepHelperTest, SerotoninFactorREM) {
    /* 5-HT near zero in REM - raphe silent */
    EXPECT_FLOAT_EQ(neuromod_sleep_get_serotonin_factor(SLEEP_STATE_REM), SLEEP_NEUROMOD_5HT_REM);
    EXPECT_LT(neuromod_sleep_get_serotonin_factor(SLEEP_STATE_REM), 0.2f);
}

/* ============================================================================
 * Release Sensitivity Computation Tests
 * ============================================================================ */

TEST_F(NeuromodSleepHelperTest, ReleaseSensitivityBelowThreshold) {
    /* Below threshold = full sensitivity */
    float sens = neuromod_sleep_compute_release_sensitivity(
        0.5f,  /* pressure below threshold */
        SLEEP_PRESSURE_THRESHOLD,
        SLEEP_PRESSURE_NE_SUPPRESSION
    );
    EXPECT_FLOAT_EQ(sens, 1.0f);
}

TEST_F(NeuromodSleepHelperTest, ReleaseSensitivityAtThreshold) {
    float sens = neuromod_sleep_compute_release_sensitivity(
        SLEEP_PRESSURE_THRESHOLD,
        SLEEP_PRESSURE_THRESHOLD,
        SLEEP_PRESSURE_NE_SUPPRESSION
    );
    EXPECT_FLOAT_EQ(sens, 1.0f);
}

TEST_F(NeuromodSleepHelperTest, ReleaseSensitivityAboveThreshold) {
    /* Above threshold = reduced sensitivity */
    float sens = neuromod_sleep_compute_release_sensitivity(
        1.0f,  /* max pressure */
        SLEEP_PRESSURE_THRESHOLD,
        SLEEP_PRESSURE_NE_SUPPRESSION
    );
    EXPECT_LT(sens, 1.0f);
    EXPECT_GT(sens, 0.0f);
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(NeuromodSleepHelperTest, DefaultConfigValid) {
    neuromodulators_sleep_config_t config;
    ASSERT_EQ(neuromod_sleep_default_config(&config), 0);

    EXPECT_TRUE(config.enable_sleep_state_modulation);
    EXPECT_TRUE(config.enable_pressure_effects);
    EXPECT_GT(config.ach_modulation_strength, 0.0f);
    EXPECT_GT(config.ne_modulation_strength, 0.0f);
    EXPECT_GT(config.da_modulation_strength, 0.0f);
    EXPECT_GT(config.serotonin_modulation_strength, 0.0f);
}

TEST_F(NeuromodSleepHelperTest, DefaultConfigNullReturnsError) {
    EXPECT_EQ(neuromod_sleep_default_config(nullptr), -1);
}

/* ============================================================================
 * Biological Validity Tests
 * ============================================================================ */

TEST_F(NeuromodSleepHelperTest, REMCharacteristics) {
    /* REM sleep has characteristic neuromodulator profile:
     * - High ACh (dream imagery)
     * - Near-zero NE (LC silent)
     * - Near-zero 5-HT (raphe silent)
     */
    float ach = neuromod_sleep_get_ach_factor(SLEEP_STATE_REM);
    float ne = neuromod_sleep_get_ne_factor(SLEEP_STATE_REM);
    float serotonin = neuromod_sleep_get_serotonin_factor(SLEEP_STATE_REM);

    EXPECT_GT(ach, 0.8f) << "ACh should be high in REM";
    EXPECT_LT(ne, 0.1f) << "NE should be near zero in REM";
    EXPECT_LT(serotonin, 0.2f) << "5-HT should be low in REM";
}

TEST_F(NeuromodSleepHelperTest, DeepNREMCharacteristics) {
    /* Deep NREM has low neuromodulator activity across the board */
    float ach = neuromod_sleep_get_ach_factor(SLEEP_STATE_DEEP_NREM);
    float ne = neuromod_sleep_get_ne_factor(SLEEP_STATE_DEEP_NREM);
    float da = neuromod_sleep_get_da_factor(SLEEP_STATE_DEEP_NREM);
    float serotonin = neuromod_sleep_get_serotonin_factor(SLEEP_STATE_DEEP_NREM);

    EXPECT_LT(ach, 0.2f) << "ACh should be very low in deep NREM";
    EXPECT_LT(ne, 0.2f) << "NE should be very low in deep NREM";
    EXPECT_LT(da, 0.5f) << "DA should be reduced in deep NREM";
    EXPECT_LT(serotonin, 0.5f) << "5-HT should be low in deep NREM";
}

TEST_F(NeuromodSleepHelperTest, AwakeIsBaseline) {
    /* Awake state should be baseline (1.0) for all neuromodulators */
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ach_factor(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(neuromod_sleep_get_ne_factor(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(neuromod_sleep_get_da_factor(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(neuromod_sleep_get_serotonin_factor(SLEEP_STATE_AWAKE), 1.0f);
}
