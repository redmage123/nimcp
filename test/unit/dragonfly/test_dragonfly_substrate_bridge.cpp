/**
 * @file test_dragonfly_substrate_bridge.cpp
 * @brief Unit tests for Dragonfly-to-Neural Substrate Bridge
 *
 * Tests metabolic cost modeling, fatigue effects, energy consumption,
 * and performance modulation based on substrate state.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_substrate_bridge.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class DragonflySubstrateBridgeTest : public ::testing::Test {
protected:
    dragonfly_substrate_bridge_t* bridge = nullptr;
    dragonfly_substrate_config_t config;

    void SetUp() override {
        ASSERT_EQ(0, dragonfly_substrate_bridge_default_config(&config));
    }

    void TearDown() override {
        if (bridge) {
            dragonfly_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    void CreateBridge() {
        bridge = dragonfly_substrate_bridge_create(nullptr, nullptr, &config);
        ASSERT_NE(nullptr, bridge);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(DragonflySubstrateBridgeTest, DefaultConfigValid) {
    dragonfly_substrate_config_t cfg;
    EXPECT_EQ(0, dragonfly_substrate_bridge_default_config(&cfg));

    /* Check energy costs are set */
    EXPECT_GT(cfg.costs.tsdn_update, 0.0f);
    EXPECT_GT(cfg.costs.tracking_step, 0.0f);
    EXPECT_GT(cfg.costs.prediction_step, 0.0f);
    EXPECT_GT(cfg.costs.intercept_calc, 0.0f);
    EXPECT_GT(cfg.costs.mode_switch, 0.0f);
    EXPECT_GT(cfg.costs.pursuit_flight, 0.0f);
    EXPECT_GE(cfg.costs.idle_baseline, 0.0f);

    /* Check fatigue thresholds are ordered */
    EXPECT_LT(cfg.severe_fatigue_threshold, cfg.moderate_fatigue_threshold);
    EXPECT_LT(cfg.moderate_fatigue_threshold, cfg.mild_fatigue_threshold);
}

TEST_F(DragonflySubstrateBridgeTest, DefaultConfigNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_substrate_bridge_default_config(nullptr));
}

TEST_F(DragonflySubstrateBridgeTest, ValidateConfigSuccess) {
    EXPECT_EQ(0, dragonfly_substrate_bridge_validate_config(&config));
}

TEST_F(DragonflySubstrateBridgeTest, ValidateConfigNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_substrate_bridge_validate_config(nullptr));
}

TEST_F(DragonflySubstrateBridgeTest, ValidateConfigNegativeCostsAccepted) {
    /* Implementation allows negative costs (treated as 0) */
    config.costs.tsdn_update = -1.0f;
    EXPECT_EQ(0, dragonfly_substrate_bridge_validate_config(&config));
}

TEST_F(DragonflySubstrateBridgeTest, ValidateConfigInvalidThresholdsInvalid) {
    /* Thresholds must be ordered: severe < moderate < mild */
    config.severe_fatigue_threshold = 0.8f;
    config.moderate_fatigue_threshold = 0.5f;  /* Should be > severe */
    EXPECT_EQ(-1, dragonfly_substrate_bridge_validate_config(&config));
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DragonflySubstrateBridgeTest, CreateWithDefaultConfig) {
    bridge = dragonfly_substrate_bridge_create(nullptr, nullptr, &config);
    EXPECT_NE(nullptr, bridge);
}

TEST_F(DragonflySubstrateBridgeTest, CreateWithNullConfigUsesDefaults) {
    bridge = dragonfly_substrate_bridge_create(nullptr, nullptr, nullptr);
    EXPECT_NE(nullptr, bridge);
}

TEST_F(DragonflySubstrateBridgeTest, DestroyNullSafe) {
    dragonfly_substrate_bridge_destroy(nullptr);
    /* No crash = success */
}

TEST_F(DragonflySubstrateBridgeTest, ResetRestoresInitialState) {
    CreateBridge();

    /* Consume some energy */
    for (int i = 0; i < 100; ++i) {
        dragonfly_substrate_record_pursuit(bridge, 1.0f);
    }
    float after_consumption = dragonfly_substrate_get_energy(bridge);
    EXPECT_LT(after_consumption, 1.0f);

    /* Reset */
    EXPECT_EQ(0, dragonfly_substrate_bridge_reset(bridge));
    float after_reset = dragonfly_substrate_get_energy(bridge);
    EXPECT_NEAR(after_reset, 1.0f, 0.01f);
}

TEST_F(DragonflySubstrateBridgeTest, ResetNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_substrate_bridge_reset(nullptr));
}

//=============================================================================
// Energy Consumption Tests
//=============================================================================

TEST_F(DragonflySubstrateBridgeTest, RecordTsdnUpdateConsumesEnergy) {
    CreateBridge();
    float initial = dragonfly_substrate_get_energy(bridge);

    EXPECT_EQ(0, dragonfly_substrate_record_tsdn_update(bridge, 16));

    float after = dragonfly_substrate_get_energy(bridge);
    EXPECT_LT(after, initial);
}

TEST_F(DragonflySubstrateBridgeTest, RecordTrackingConsumesEnergy) {
    CreateBridge();
    float initial = dragonfly_substrate_get_energy(bridge);

    EXPECT_EQ(0, dragonfly_substrate_record_tracking(bridge, 3));

    float after = dragonfly_substrate_get_energy(bridge);
    EXPECT_LT(after, initial);
}

TEST_F(DragonflySubstrateBridgeTest, RecordPredictionConsumesEnergy) {
    CreateBridge();
    float initial = dragonfly_substrate_get_energy(bridge);

    EXPECT_EQ(0, dragonfly_substrate_record_prediction(bridge, 0.5f));

    float after = dragonfly_substrate_get_energy(bridge);
    EXPECT_LT(after, initial);
}

TEST_F(DragonflySubstrateBridgeTest, RecordInterceptCalcConsumesEnergy) {
    CreateBridge();
    float initial = dragonfly_substrate_get_energy(bridge);

    EXPECT_EQ(0, dragonfly_substrate_record_intercept_calc(bridge, 0.8f));

    float after = dragonfly_substrate_get_energy(bridge);
    EXPECT_LT(after, initial);
}

TEST_F(DragonflySubstrateBridgeTest, RecordModeSwitchConsumesEnergy) {
    CreateBridge();
    float initial = dragonfly_substrate_get_energy(bridge);

    EXPECT_EQ(0, dragonfly_substrate_record_mode_switch(bridge));

    float after = dragonfly_substrate_get_energy(bridge);
    EXPECT_LT(after, initial);
}

TEST_F(DragonflySubstrateBridgeTest, RecordPursuitConsumesEnergy) {
    CreateBridge();
    float initial = dragonfly_substrate_get_energy(bridge);

    EXPECT_EQ(0, dragonfly_substrate_record_pursuit(bridge, 1.0f));

    float after = dragonfly_substrate_get_energy(bridge);
    EXPECT_LT(after, initial);
}

TEST_F(DragonflySubstrateBridgeTest, HigherIntensityConsumesMoreEnergy) {
    CreateBridge();
    float initial = dragonfly_substrate_get_energy(bridge);

    dragonfly_substrate_record_pursuit(bridge, 0.2f);
    float after_low = dragonfly_substrate_get_energy(bridge);
    float low_consumption = initial - after_low;

    dragonfly_substrate_bridge_reset(bridge);
    initial = dragonfly_substrate_get_energy(bridge);

    dragonfly_substrate_record_pursuit(bridge, 1.0f);
    float after_high = dragonfly_substrate_get_energy(bridge);
    float high_consumption = initial - after_high;

    EXPECT_GT(high_consumption, low_consumption);
}

TEST_F(DragonflySubstrateBridgeTest, EnergyConsumptionNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_substrate_record_tsdn_update(nullptr, 16));
    EXPECT_EQ(-1, dragonfly_substrate_record_tracking(nullptr, 1));
    EXPECT_EQ(-1, dragonfly_substrate_record_prediction(nullptr, 0.5f));
    EXPECT_EQ(-1, dragonfly_substrate_record_intercept_calc(nullptr, 0.5f));
    EXPECT_EQ(-1, dragonfly_substrate_record_mode_switch(nullptr));
    EXPECT_EQ(-1, dragonfly_substrate_record_pursuit(nullptr, 0.5f));
}

TEST_F(DragonflySubstrateBridgeTest, EnergyNeverGoesBelowZero) {
    CreateBridge();

    /* Consume massive amounts of energy */
    for (int i = 0; i < 10000; ++i) {
        dragonfly_substrate_record_pursuit(bridge, 1.0f);
    }

    float energy = dragonfly_substrate_get_energy(bridge);
    EXPECT_GE(energy, 0.0f);
}

//=============================================================================
// Performance Modulation Tests
//=============================================================================

TEST_F(DragonflySubstrateBridgeTest, GetModulationSuccess) {
    CreateBridge();
    dragonfly_substrate_modulation_t mod;

    EXPECT_EQ(0, dragonfly_substrate_get_modulation(bridge, &mod));
    EXPECT_GE(mod.tracking_accuracy, 0.0f);
    EXPECT_LE(mod.tracking_accuracy, 1.0f);
    EXPECT_GE(mod.overall_performance, 0.0f);
    EXPECT_LE(mod.overall_performance, 1.0f);
}

TEST_F(DragonflySubstrateBridgeTest, GetModulationNullReturnsError) {
    CreateBridge();
    dragonfly_substrate_modulation_t mod;

    EXPECT_EQ(-1, dragonfly_substrate_get_modulation(nullptr, &mod));
    EXPECT_EQ(-1, dragonfly_substrate_get_modulation(bridge, nullptr));
}

TEST_F(DragonflySubstrateBridgeTest, FullEnergyGivesFullPerformance) {
    CreateBridge();

    float tracking = dragonfly_substrate_get_tracking_accuracy(bridge);
    float speed = dragonfly_substrate_get_pursuit_speed(bridge);
    float reaction = dragonfly_substrate_get_reaction_factor(bridge);

    EXPECT_NEAR(tracking, 1.0f, 0.01f);
    EXPECT_NEAR(speed, 1.0f, 0.01f);
    EXPECT_NEAR(reaction, 1.0f, 0.01f);
}

TEST_F(DragonflySubstrateBridgeTest, LowEnergyDegradesPerfomance) {
    CreateBridge();
    config.enable_fatigue_modeling = true;
    dragonfly_substrate_bridge_destroy(bridge);
    bridge = dragonfly_substrate_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(nullptr, bridge);

    /* Deplete energy significantly */
    for (int i = 0; i < 1000; ++i) {
        dragonfly_substrate_record_pursuit(bridge, 1.0f);
    }

    float tracking = dragonfly_substrate_get_tracking_accuracy(bridge);
    float speed = dragonfly_substrate_get_pursuit_speed(bridge);

    /* Performance should be degraded */
    EXPECT_LT(tracking, 1.0f);
    EXPECT_LT(speed, 1.0f);
}

TEST_F(DragonflySubstrateBridgeTest, FatigueModelingDisabledNoPerformanceImpact) {
    config.enable_fatigue_modeling = false;
    CreateBridge();

    /* Deplete energy significantly */
    for (int i = 0; i < 1000; ++i) {
        dragonfly_substrate_record_pursuit(bridge, 1.0f);
    }

    /* With fatigue modeling disabled, performance stays at 1.0 */
    float tracking = dragonfly_substrate_get_tracking_accuracy(bridge);
    EXPECT_NEAR(tracking, 1.0f, 0.01f);
}

TEST_F(DragonflySubstrateBridgeTest, GetPerformanceFactorsNullReturnDefault) {
    EXPECT_EQ(1.0f, dragonfly_substrate_get_tracking_accuracy(nullptr));
    EXPECT_EQ(1.0f, dragonfly_substrate_get_pursuit_speed(nullptr));
    EXPECT_EQ(1.0f, dragonfly_substrate_get_reaction_factor(nullptr));
}

//=============================================================================
// Impact Level Tests
//=============================================================================

TEST_F(DragonflySubstrateBridgeTest, InitialImpactNone) {
    CreateBridge();

    substrate_perf_impact_t impact = dragonfly_substrate_get_impact(bridge);
    EXPECT_EQ(PERF_IMPACT_NONE, impact);
}

TEST_F(DragonflySubstrateBridgeTest, ImpactIncreasesWithFatigue) {
    config.enable_fatigue_modeling = true;
    CreateBridge();

    /* Consume energy until we see impact */
    substrate_perf_impact_t initial = dragonfly_substrate_get_impact(bridge);
    EXPECT_EQ(PERF_IMPACT_NONE, initial);

    for (int i = 0; i < 2000; ++i) {
        dragonfly_substrate_record_pursuit(bridge, 1.0f);
    }

    substrate_perf_impact_t after = dragonfly_substrate_get_impact(bridge);
    EXPECT_GT(after, PERF_IMPACT_NONE);
}

TEST_F(DragonflySubstrateBridgeTest, GetImpactNullReturnsNone) {
    EXPECT_EQ(PERF_IMPACT_NONE, dragonfly_substrate_get_impact(nullptr));
}

TEST_F(DragonflySubstrateBridgeTest, IsFatiguedInitiallyFalse) {
    CreateBridge();
    EXPECT_FALSE(dragonfly_substrate_is_fatigued(bridge));
}

TEST_F(DragonflySubstrateBridgeTest, IsFatiguedAfterHeavyConsumption) {
    config.enable_fatigue_modeling = true;
    CreateBridge();

    for (int i = 0; i < 2000; ++i) {
        dragonfly_substrate_record_pursuit(bridge, 1.0f);
    }

    EXPECT_TRUE(dragonfly_substrate_is_fatigued(bridge));
}

TEST_F(DragonflySubstrateBridgeTest, IsFatiguedNullReturnsFalse) {
    EXPECT_FALSE(dragonfly_substrate_is_fatigued(nullptr));
}

//=============================================================================
// Activity Tracking Tests
//=============================================================================

TEST_F(DragonflySubstrateBridgeTest, SetActivitySuccess) {
    CreateBridge();

    EXPECT_EQ(0, dragonfly_substrate_set_activity(bridge, SUBSTRATE_ACTIVITY_TRACKING));
    EXPECT_EQ(SUBSTRATE_ACTIVITY_TRACKING, dragonfly_substrate_get_activity(bridge));

    EXPECT_EQ(0, dragonfly_substrate_set_activity(bridge, SUBSTRATE_ACTIVITY_PURSUIT));
    EXPECT_EQ(SUBSTRATE_ACTIVITY_PURSUIT, dragonfly_substrate_get_activity(bridge));
}

TEST_F(DragonflySubstrateBridgeTest, SetActivityNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_substrate_set_activity(nullptr, SUBSTRATE_ACTIVITY_IDLE));
}

TEST_F(DragonflySubstrateBridgeTest, GetActivityNullReturnsIdle) {
    EXPECT_EQ(SUBSTRATE_ACTIVITY_IDLE, dragonfly_substrate_get_activity(nullptr));
}

TEST_F(DragonflySubstrateBridgeTest, GetEnergyInitiallyFull) {
    CreateBridge();
    EXPECT_NEAR(dragonfly_substrate_get_energy(bridge), 1.0f, 0.01f);
}

TEST_F(DragonflySubstrateBridgeTest, GetEnergyNullReturnsFullEnergy) {
    /* Null bridge returns full energy (1.0) as safe default */
    EXPECT_EQ(1.0f, dragonfly_substrate_get_energy(nullptr));
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(DragonflySubstrateBridgeTest, ConnectDragonflySuccess) {
    CreateBridge();

    /* We don't have a real dragonfly, but API should accept non-null */
    int dummy_dragonfly;
    EXPECT_EQ(0, dragonfly_substrate_connect_dragonfly(
        bridge, (dragonfly_system_t*)&dummy_dragonfly));
    EXPECT_TRUE(dragonfly_substrate_has_dragonfly(bridge));
}

TEST_F(DragonflySubstrateBridgeTest, ConnectSubstrateSuccess) {
    CreateBridge();

    int dummy_substrate;
    EXPECT_EQ(0, dragonfly_substrate_connect_substrate(bridge, &dummy_substrate));
    EXPECT_TRUE(dragonfly_substrate_has_substrate(bridge));
}

TEST_F(DragonflySubstrateBridgeTest, ConnectNullReturnsError) {
    CreateBridge();

    EXPECT_EQ(-1, dragonfly_substrate_connect_dragonfly(nullptr, nullptr));
    EXPECT_EQ(-1, dragonfly_substrate_connect_substrate(nullptr, nullptr));
}

TEST_F(DragonflySubstrateBridgeTest, HasDragonflyInitiallyFalse) {
    CreateBridge();
    EXPECT_FALSE(dragonfly_substrate_has_dragonfly(bridge));
}

TEST_F(DragonflySubstrateBridgeTest, HasSubstrateInitiallyFalse) {
    CreateBridge();
    EXPECT_FALSE(dragonfly_substrate_has_substrate(bridge));
}

TEST_F(DragonflySubstrateBridgeTest, HasDragonflyNullReturnsFalse) {
    EXPECT_FALSE(dragonfly_substrate_has_dragonfly(nullptr));
}

TEST_F(DragonflySubstrateBridgeTest, HasSubstrateNullReturnsFalse) {
    EXPECT_FALSE(dragonfly_substrate_has_substrate(nullptr));
}

//=============================================================================
// Update/Step Tests
//=============================================================================

TEST_F(DragonflySubstrateBridgeTest, UpdateSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_substrate_update(bridge));
}

TEST_F(DragonflySubstrateBridgeTest, UpdateNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_substrate_update(nullptr));
}

TEST_F(DragonflySubstrateBridgeTest, StepSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_substrate_step(bridge, 16.0f));
}

TEST_F(DragonflySubstrateBridgeTest, StepNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_substrate_step(nullptr, 16.0f));
}

TEST_F(DragonflySubstrateBridgeTest, StepRecoveryIncreasesEnergy) {
    config.enable_recovery = true;
    CreateBridge();

    /* Deplete some energy */
    for (int i = 0; i < 500; ++i) {
        dragonfly_substrate_record_pursuit(bridge, 1.0f);
    }
    float after_depletion = dragonfly_substrate_get_energy(bridge);

    /* Set to idle for recovery */
    dragonfly_substrate_set_activity(bridge, SUBSTRATE_ACTIVITY_IDLE);

    /* Step multiple times to allow recovery */
    for (int i = 0; i < 100; ++i) {
        dragonfly_substrate_step(bridge, 100.0f);
    }
    float after_recovery = dragonfly_substrate_get_energy(bridge);

    EXPECT_GT(after_recovery, after_depletion);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DragonflySubstrateBridgeTest, GetStatsSuccess) {
    CreateBridge();
    substrate_bridge_stats_t stats;

    EXPECT_EQ(0, dragonfly_substrate_bridge_get_stats(bridge, &stats));
}

TEST_F(DragonflySubstrateBridgeTest, GetStatsNullReturnsError) {
    CreateBridge();
    substrate_bridge_stats_t stats;

    EXPECT_EQ(-1, dragonfly_substrate_bridge_get_stats(nullptr, &stats));
    EXPECT_EQ(-1, dragonfly_substrate_bridge_get_stats(bridge, nullptr));
}

TEST_F(DragonflySubstrateBridgeTest, StatsTracksOperations) {
    CreateBridge();

    /* Perform various operations */
    for (int i = 0; i < 10; ++i) {
        dragonfly_substrate_record_tsdn_update(bridge, 16);
    }
    for (int i = 0; i < 20; ++i) {
        dragonfly_substrate_record_tracking(bridge, 1);
    }
    for (int i = 0; i < 5; ++i) {
        dragonfly_substrate_record_mode_switch(bridge);
    }

    substrate_bridge_stats_t stats;
    EXPECT_EQ(0, dragonfly_substrate_bridge_get_stats(bridge, &stats));

    EXPECT_EQ(10u, stats.tsdn_updates);
    EXPECT_EQ(20u, stats.tracking_steps);
    EXPECT_EQ(5u, stats.mode_switches);
    EXPECT_GT(stats.total_energy_consumed, 0.0f);
}

TEST_F(DragonflySubstrateBridgeTest, ResetStatsSuccess) {
    CreateBridge();

    /* Perform some operations */
    for (int i = 0; i < 10; ++i) {
        dragonfly_substrate_record_pursuit(bridge, 1.0f);
    }

    EXPECT_EQ(0, dragonfly_substrate_bridge_reset_stats(bridge));

    substrate_bridge_stats_t stats;
    dragonfly_substrate_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0u, stats.pursuit_steps);
    EXPECT_EQ(0.0f, stats.total_energy_consumed);
}

TEST_F(DragonflySubstrateBridgeTest, ResetStatsNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_substrate_bridge_reset_stats(nullptr));
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(DragonflySubstrateBridgeTest, ActivityNameValid) {
    EXPECT_STREQ("idle", dragonfly_substrate_activity_name(SUBSTRATE_ACTIVITY_IDLE));
    EXPECT_STREQ("scanning", dragonfly_substrate_activity_name(SUBSTRATE_ACTIVITY_SCANNING));
    EXPECT_STREQ("tracking", dragonfly_substrate_activity_name(SUBSTRATE_ACTIVITY_TRACKING));
    EXPECT_STREQ("pursuit", dragonfly_substrate_activity_name(SUBSTRATE_ACTIVITY_PURSUIT));
    EXPECT_STREQ("intercept", dragonfly_substrate_activity_name(SUBSTRATE_ACTIVITY_INTERCEPT));
}

TEST_F(DragonflySubstrateBridgeTest, ActivityNameInvalidReturnsUnknown) {
    EXPECT_STREQ("unknown", dragonfly_substrate_activity_name((substrate_activity_level_t)99));
}

TEST_F(DragonflySubstrateBridgeTest, ImpactNameValid) {
    EXPECT_STREQ("none", dragonfly_substrate_impact_name(PERF_IMPACT_NONE));
    EXPECT_STREQ("mild", dragonfly_substrate_impact_name(PERF_IMPACT_MILD));
    EXPECT_STREQ("moderate", dragonfly_substrate_impact_name(PERF_IMPACT_MODERATE));
    EXPECT_STREQ("severe", dragonfly_substrate_impact_name(PERF_IMPACT_SEVERE));
    EXPECT_STREQ("critical", dragonfly_substrate_impact_name(PERF_IMPACT_CRITICAL));
}

TEST_F(DragonflySubstrateBridgeTest, ImpactNameInvalidReturnsUnknown) {
    EXPECT_STREQ("unknown", dragonfly_substrate_impact_name((substrate_perf_impact_t)99));
}

//=============================================================================
// Comprehensive Integration Test
//=============================================================================

TEST_F(DragonflySubstrateBridgeTest, FullHuntingScenario) {
    config.enable_fatigue_modeling = true;
    config.enable_recovery = true;
    CreateBridge();

    /* Phase 1: Scanning */
    dragonfly_substrate_set_activity(bridge, SUBSTRATE_ACTIVITY_SCANNING);
    for (int i = 0; i < 50; ++i) {
        dragonfly_substrate_record_tsdn_update(bridge, 16);
        dragonfly_substrate_step(bridge, 10.0f);
    }
    float after_scanning = dragonfly_substrate_get_energy(bridge);
    EXPECT_LT(after_scanning, 1.0f);

    /* Phase 2: Target detected - tracking */
    dragonfly_substrate_set_activity(bridge, SUBSTRATE_ACTIVITY_TRACKING);
    dragonfly_substrate_record_mode_switch(bridge);
    for (int i = 0; i < 100; ++i) {
        dragonfly_substrate_record_tracking(bridge, 1);
        dragonfly_substrate_record_prediction(bridge, 0.5f);
        dragonfly_substrate_step(bridge, 5.0f);
    }
    float after_tracking = dragonfly_substrate_get_energy(bridge);
    EXPECT_LT(after_tracking, after_scanning);

    /* Phase 3: Pursuit */
    dragonfly_substrate_set_activity(bridge, SUBSTRATE_ACTIVITY_PURSUIT);
    dragonfly_substrate_record_mode_switch(bridge);
    for (int i = 0; i < 50; ++i) {
        dragonfly_substrate_record_pursuit(bridge, 0.8f);
        dragonfly_substrate_record_intercept_calc(bridge, 0.7f);
        dragonfly_substrate_step(bridge, 2.0f);
    }
    float after_pursuit = dragonfly_substrate_get_energy(bridge);
    EXPECT_LT(after_pursuit, after_tracking);

    /* Phase 4: Recovery (return to idle) */
    dragonfly_substrate_set_activity(bridge, SUBSTRATE_ACTIVITY_IDLE);
    for (int i = 0; i < 200; ++i) {
        dragonfly_substrate_step(bridge, 50.0f);
    }
    float after_recovery = dragonfly_substrate_get_energy(bridge);
    EXPECT_GT(after_recovery, after_pursuit);

    /* Check stats */
    substrate_bridge_stats_t stats;
    dragonfly_substrate_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(50u, stats.tsdn_updates);
    EXPECT_EQ(100u, stats.tracking_steps);
    EXPECT_EQ(50u, stats.pursuit_steps);
    EXPECT_EQ(2u, stats.mode_switches);
    EXPECT_GT(stats.total_energy_consumed, 0.0f);
}
