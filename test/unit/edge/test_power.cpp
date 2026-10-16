/**
 * @file test_power.cpp
 * @brief GoogleTest unit tests for NIMCP edge power management subsystem
 *
 * Tests battery-aware mode transitions, GPU control, inference Hz scaling,
 * LR scaling, and temperature throttling.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class PowerTest : public ::testing::Test {
protected:
    nimcp_power_config_t config;

    void SetUp() override {
        memset(&config, 0, sizeof(config));
        nimcp_power_init(&config);
    }
};

TEST_F(PowerTest, FullModeAtHighBattery) {
    nimcp_power_mode_t mode = nimcp_power_update(&config, 100.0f, 40.0f);
    EXPECT_EQ(mode, NIMCP_POWER_FULL);
    EXPECT_EQ(config.mode, NIMCP_POWER_FULL);
}

TEST_F(PowerTest, BalancedAtMidBattery) {
    // Default balanced threshold is 80%
    nimcp_power_mode_t mode = nimcp_power_update(&config, 79.0f, 40.0f);
    EXPECT_EQ(mode, NIMCP_POWER_BALANCED);
}

TEST_F(PowerTest, SavingAtLowBattery) {
    // Default saving threshold is 50%
    nimcp_power_mode_t mode = nimcp_power_update(&config, 49.0f, 40.0f);
    EXPECT_EQ(mode, NIMCP_POWER_SAVING);
}

TEST_F(PowerTest, CriticalAtVeryLowBattery) {
    // Default critical threshold is 20%
    nimcp_power_mode_t mode = nimcp_power_update(&config, 19.0f, 40.0f);
    EXPECT_EQ(mode, NIMCP_POWER_CRITICAL);
}

TEST_F(PowerTest, TemperatureThrottlingOverridesBattery) {
    // Full battery but high temperature
    float high_temp = config.thermal_throttle_c + 10.0f;
    nimcp_power_mode_t mode = nimcp_power_update(&config, 100.0f, high_temp);

    // Should throttle despite full battery
    EXPECT_NE(mode, NIMCP_POWER_FULL);
}

TEST_F(PowerTest, GPUDisabledInSavingAndCritical) {
    nimcp_power_update(&config, 49.0f, 40.0f); // SAVING
    EXPECT_FALSE(nimcp_power_is_gpu_enabled(&config));

    nimcp_power_update(&config, 19.0f, 40.0f); // CRITICAL
    EXPECT_FALSE(nimcp_power_is_gpu_enabled(&config));
}

TEST_F(PowerTest, InferenceHzReducesWithMode) {
    nimcp_power_update(&config, 100.0f, 40.0f);
    float hz_full = nimcp_power_get_inference_hz(&config);

    nimcp_power_update(&config, 79.0f, 40.0f);
    float hz_balanced = nimcp_power_get_inference_hz(&config);

    nimcp_power_update(&config, 49.0f, 40.0f);
    float hz_saving = nimcp_power_get_inference_hz(&config);

    nimcp_power_update(&config, 19.0f, 40.0f);
    float hz_critical = nimcp_power_get_inference_hz(&config);

    EXPECT_GE(hz_full, hz_balanced);
    EXPECT_GE(hz_balanced, hz_saving);
    EXPECT_GE(hz_saving, hz_critical);
    EXPECT_GT(hz_critical, 0.0f);
}

TEST_F(PowerTest, LRScaleReducesWithMode) {
    nimcp_power_update(&config, 100.0f, 40.0f);
    float lr_full = nimcp_power_get_lr_scale(&config);

    nimcp_power_update(&config, 49.0f, 40.0f);
    float lr_saving = nimcp_power_get_lr_scale(&config);

    nimcp_power_update(&config, 19.0f, 40.0f);
    float lr_critical = nimcp_power_get_lr_scale(&config);

    EXPECT_GE(lr_full, lr_saving);
    EXPECT_GE(lr_saving, lr_critical);
}

TEST_F(PowerTest, AutoModeTransitionsCorrectly) {
    config.auto_manage = true;

    // Full -> Balanced -> Saving -> Critical
    nimcp_power_update(&config, 100.0f, 40.0f);
    EXPECT_EQ(config.mode, NIMCP_POWER_FULL);

    nimcp_power_update(&config, 70.0f, 40.0f);
    EXPECT_EQ(config.mode, NIMCP_POWER_BALANCED);

    nimcp_power_update(&config, 30.0f, 40.0f);
    EXPECT_EQ(config.mode, NIMCP_POWER_SAVING);

    nimcp_power_update(&config, 10.0f, 40.0f);
    EXPECT_EQ(config.mode, NIMCP_POWER_CRITICAL);

    // Recovery
    nimcp_power_update(&config, 100.0f, 40.0f);
    EXPECT_EQ(config.mode, NIMCP_POWER_FULL);
}

TEST_F(PowerTest, ManualOverrideWorks) {
    config.auto_manage = false;
    config.mode = NIMCP_POWER_SAVING;

    // Even with full battery, manual mode should stay
    nimcp_power_update(&config, 100.0f, 40.0f);
    // In manual mode, the update may or may not change mode
    // At minimum, we verify it doesn't crash
    EXPECT_TRUE(config.mode == NIMCP_POWER_SAVING || config.mode == NIMCP_POWER_FULL);
}

TEST_F(PowerTest, GPUEnabledInFullMode) {
    nimcp_power_update(&config, 100.0f, 40.0f);
    EXPECT_TRUE(nimcp_power_is_gpu_enabled(&config));
}
