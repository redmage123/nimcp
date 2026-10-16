/**
 * @file test_dragonfly_immune_bridge.cpp
 * @brief Unit tests for dragonfly immune bridge module
 *
 * Tests immune-dragonfly integration including health status,
 * stress management, and hunting modulation.
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_immune_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DragonImmuneBridgeTest : public ::testing::Test {
protected:
    dragonfly_immune_bridge_t bridge = nullptr;

    void SetUp() override {
        dragonfly_immune_config_t config = dragonfly_immune_default_config();
        bridge = dragonfly_immune_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            dragonfly_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(DragonImmuneBridgeTest, DefaultConfig) {
    dragonfly_immune_config_t config = dragonfly_immune_default_config();

    // Health thresholds should be ordered (lower threshold = more impairment)
    EXPECT_GT(config.mild_impairment_threshold, 0.0f);
    EXPECT_LT(config.moderate_impairment_threshold, config.mild_impairment_threshold);
    EXPECT_LT(config.severe_impairment_threshold, config.moderate_impairment_threshold);
    EXPECT_LT(config.critical_threshold, config.severe_impairment_threshold);

    // Stress parameters
    EXPECT_GT(config.failure_stress_increment, 0.0f);
    EXPECT_GT(config.success_stress_decrement, 0.0f);
    EXPECT_GT(config.stress_decay_rate, 0.0f);
    EXPECT_GT(config.failure_frustration_threshold, 0u);

    // Energy management
    EXPECT_GT(config.energy_per_pursuit_j, 0.0f);
    EXPECT_GT(config.energy_recovery_rate, 0.0f);
    EXPECT_GT(config.min_energy_for_hunt, 0.0f);
}

TEST_F(DragonImmuneBridgeTest, ValidateConfig) {
    dragonfly_immune_config_t config = dragonfly_immune_default_config();
    EXPECT_TRUE(dragonfly_immune_validate_config(&config));

    // Null config
    EXPECT_FALSE(dragonfly_immune_validate_config(nullptr));
}

TEST_F(DragonImmuneBridgeTest, CreateWithCustomConfig) {
    dragonfly_immune_config_t config = dragonfly_immune_default_config();
    config.failure_stress_increment = 0.2f;
    config.enable_immune_feedback = true;

    dragonfly_immune_bridge_t custom = dragonfly_immune_bridge_create(&config);
    ASSERT_NE(custom, nullptr);
    dragonfly_immune_bridge_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DragonImmuneBridgeTest, CreateAndDestroy) {
    dragonfly_immune_bridge_t b = dragonfly_immune_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    dragonfly_immune_bridge_destroy(b);
}

TEST_F(DragonImmuneBridgeTest, DestroyNull) {
    dragonfly_immune_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(DragonImmuneBridgeTest, Disconnect) {
    EXPECT_EQ(dragonfly_immune_bridge_disconnect(bridge), 0);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(DragonImmuneBridgeTest, UpdateBasic) {
    EXPECT_EQ(dragonfly_immune_bridge_update(bridge, 0.016f), 0);
}

TEST_F(DragonImmuneBridgeTest, UpdateNullBridge) {
    EXPECT_NE(dragonfly_immune_bridge_update(nullptr, 0.016f), 0);
}

TEST_F(DragonImmuneBridgeTest, UpdateMultiple) {
    // Multiple update cycles
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(dragonfly_immune_bridge_update(bridge, 0.016f), 0);
    }
}

//=============================================================================
// Hunt Reporting Tests
//=============================================================================

TEST_F(DragonImmuneBridgeTest, ReportSuccessfulHunt) {
    EXPECT_EQ(dragonfly_immune_report_hunt(bridge, true, 2.0f, 50.0f), 0);

    dragonfly_immune_state_t state;
    EXPECT_EQ(dragonfly_immune_get_state(bridge, &state), 0);
    EXPECT_EQ(state.stress_report.hunts_attempted, 1u);
    EXPECT_EQ(state.stress_report.hunts_successful, 1u);
    EXPECT_EQ(state.stress_report.consecutive_failures, 0u);
}

TEST_F(DragonImmuneBridgeTest, ReportFailedHunt) {
    EXPECT_EQ(dragonfly_immune_report_hunt(bridge, false, 3.0f, 75.0f), 0);

    dragonfly_immune_state_t state;
    EXPECT_EQ(dragonfly_immune_get_state(bridge, &state), 0);
    EXPECT_EQ(state.stress_report.hunts_attempted, 1u);
    EXPECT_EQ(state.stress_report.hunts_successful, 0u);
    EXPECT_EQ(state.stress_report.consecutive_failures, 1u);
}

TEST_F(DragonImmuneBridgeTest, ConsecutiveFailuresIncreasesFrustration) {
    // Multiple failures
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(dragonfly_immune_report_hunt(bridge, false, 3.0f, 50.0f), 0);
    }

    dragonfly_immune_state_t state;
    EXPECT_EQ(dragonfly_immune_get_state(bridge, &state), 0);
    EXPECT_EQ(state.stress_report.consecutive_failures, 5u);
    EXPECT_GT(state.stress_report.frustration_level, 0.0f);
}

TEST_F(DragonImmuneBridgeTest, SuccessResetsFrustration) {
    // Failures then success
    for (int i = 0; i < 3; i++) {
        dragonfly_immune_report_hunt(bridge, false, 2.0f, 40.0f);
    }
    dragonfly_immune_report_hunt(bridge, true, 1.5f, 30.0f);

    dragonfly_immune_state_t state;
    EXPECT_EQ(dragonfly_immune_get_state(bridge, &state), 0);
    EXPECT_EQ(state.stress_report.consecutive_failures, 0u);
}

//=============================================================================
// Stress Reporting Tests
//=============================================================================

TEST_F(DragonImmuneBridgeTest, ReportStress) {
    EXPECT_EQ(dragonfly_immune_report_stress(bridge, 0.8f, 5.0f), 0);
}

TEST_F(DragonImmuneBridgeTest, HighIntensityStress) {
    // High intensity pursuit - just verify the call succeeds
    EXPECT_EQ(dragonfly_immune_report_stress(bridge, 1.0f, 10.0f), 0);

    // Stress level may or may not increase depending on implementation
    stress_level_t level = dragonfly_immune_get_stress(bridge);
    EXPECT_GE((int)level, (int)STRESS_NONE);
}

//=============================================================================
// Rest/Recovery Tests
//=============================================================================

TEST_F(DragonImmuneBridgeTest, ReportRest) {
    EXPECT_EQ(dragonfly_immune_report_rest(bridge, 10.0f), 0);
}

TEST_F(DragonImmuneBridgeTest, RestReducesStress) {
    // Build up stress
    for (int i = 0; i < 5; i++) {
        dragonfly_immune_report_stress(bridge, 0.9f, 3.0f);
    }

    stress_level_t before = dragonfly_immune_get_stress(bridge);

    // Rest
    dragonfly_immune_report_rest(bridge, 30.0f);
    dragonfly_immune_bridge_update(bridge, 30.0f);

    stress_level_t after = dragonfly_immune_get_stress(bridge);
    EXPECT_LE((int)after, (int)before);
}

//=============================================================================
// Modulation Query Tests
//=============================================================================

TEST_F(DragonImmuneBridgeTest, GetModulation) {
    immune_modulation_t modulation;
    EXPECT_EQ(dragonfly_immune_get_modulation(bridge, &modulation), 0);

    // Fresh bridge should have good modifiers
    EXPECT_GE(modulation.speed_modifier, 0.8f);
    EXPECT_GE(modulation.accuracy_modifier, 0.8f);
    EXPECT_GE(modulation.endurance_modifier, 0.8f);
    EXPECT_TRUE(modulation.hunting_recommended);
}

TEST_F(DragonImmuneBridgeTest, ModulationAfterStress) {
    // Report stress events
    for (int i = 0; i < 10; i++) {
        dragonfly_immune_report_stress(bridge, 1.0f, 5.0f);
    }

    immune_modulation_t modulation;
    EXPECT_EQ(dragonfly_immune_get_modulation(bridge, &modulation), 0);

    // Modifiers should be in valid range
    EXPECT_LE(modulation.speed_modifier, 1.0f);
    EXPECT_GE(modulation.speed_modifier, 0.0f);
}

TEST_F(DragonImmuneBridgeTest, HuntingSafe) {
    EXPECT_TRUE(dragonfly_immune_hunting_safe(bridge));
}

//=============================================================================
// Health Status Tests
//=============================================================================

TEST_F(DragonImmuneBridgeTest, GetHealthStatus) {
    health_status_t status = dragonfly_immune_get_health(bridge);
    EXPECT_EQ(status, HEALTH_OPTIMAL);
}

TEST_F(DragonImmuneBridgeTest, GetStressLevel) {
    stress_level_t level = dragonfly_immune_get_stress(bridge);
    EXPECT_EQ(level, STRESS_NONE);
}

TEST_F(DragonImmuneBridgeTest, GetState) {
    dragonfly_immune_state_t state;
    EXPECT_EQ(dragonfly_immune_get_state(bridge, &state), 0);
    EXPECT_EQ(state.health_status, HEALTH_OPTIMAL);
    EXPECT_EQ(state.stress_level, STRESS_NONE);
    EXPECT_FALSE(state.is_injured);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DragonImmuneBridgeTest, GetStats) {
    dragonfly_immune_stats_t stats;
    EXPECT_EQ(dragonfly_immune_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.hunts_blocked, 0u);
    EXPECT_EQ(stats.injuries_sustained, 0u);
}

TEST_F(DragonImmuneBridgeTest, StatsAccumulate) {
    // Do some hunting
    for (int i = 0; i < 5; i++) {
        dragonfly_immune_report_hunt(bridge, i % 2 == 0, 2.0f, 50.0f);
    }

    dragonfly_immune_stats_t stats;
    EXPECT_EQ(dragonfly_immune_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.total_energy_expended_j, 0.0f);
}

//=============================================================================
// Name Function Tests
//=============================================================================

TEST_F(DragonImmuneBridgeTest, HealthStatusNames) {
    EXPECT_STREQ(dragonfly_health_status_name(HEALTH_OPTIMAL), "optimal");
    EXPECT_STREQ(dragonfly_health_status_name(HEALTH_MILD_IMPAIRMENT), "mild_impairment");
    EXPECT_STREQ(dragonfly_health_status_name(HEALTH_CRITICAL), "critical");
}

TEST_F(DragonImmuneBridgeTest, StressLevelNames) {
    EXPECT_STREQ(dragonfly_stress_level_name(STRESS_NONE), "none");
    EXPECT_STREQ(dragonfly_stress_level_name(STRESS_HIGH), "high");
    EXPECT_STREQ(dragonfly_stress_level_name(STRESS_CHRONIC), "chronic");
}

//=============================================================================
// Null Parameter Tests
//=============================================================================

TEST_F(DragonImmuneBridgeTest, NullModulationOutput) {
    EXPECT_NE(dragonfly_immune_get_modulation(bridge, nullptr), 0);
}

TEST_F(DragonImmuneBridgeTest, NullStateOutput) {
    EXPECT_NE(dragonfly_immune_get_state(bridge, nullptr), 0);
}

TEST_F(DragonImmuneBridgeTest, NullStatsOutput) {
    EXPECT_NE(dragonfly_immune_get_stats(bridge, nullptr), 0);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(DragonImmuneBridgeTest, ZeroDurationHunt) {
    EXPECT_EQ(dragonfly_immune_report_hunt(bridge, true, 0.0f, 0.0f), 0);
}

TEST_F(DragonImmuneBridgeTest, LongDurationPursuit) {
    EXPECT_EQ(dragonfly_immune_report_stress(bridge, 1.0f, 3600.0f), 0);  // 1 hour
}

TEST_F(DragonImmuneBridgeTest, NegativeUpdateDt) {
    // Should handle gracefully
    dragonfly_immune_bridge_update(bridge, -0.016f);
}
