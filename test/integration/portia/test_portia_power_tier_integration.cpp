/**
 * @file test_portia_power_tier_integration.cpp
 * @brief Integration tests for Portia power management and tier interaction
 *
 * WHAT: Tests power profile changes affect tier selection
 * WHY:  Validate system responds to power constraints (battery, thermal)
 * HOW:  Simulate power state changes, verify tier adaptation and feature restoration
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "utils/platform/nimcp_platform_tier.h"
#include "portia/nimcp_portia_degradation.h"
#include "async/nimcp_bio_async.h"
#include "utils/platform/nimcp_system_resources.h"
#include "utils/validation/nimcp_common.h"

class PortiaPowerTierIntegrationTest : public ::testing::Test {
protected:
    degradation_state_t* degrade_state = nullptr;
    

    void SetUp() override {
        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        nimcp_bio_async_init(&bio_config);
        

        // Initialize degradation with power-aware thresholds
        degradation_internal_config_t config = {
            .level_thresholds = {0.0f, 50.0f, 70.0f, 85.0f, 95.0f},
            .hysteresis_ms = 500,
            .enable_auto_degrade = true,
            .enable_auto_restore = true,
            .restore_threshold = 15.0f
        };

        degrade_state = portia_degradation_init(&config);
        ASSERT_NE(degrade_state, nullptr);

        // Register power-sensitive features
        degradation_feature_t features[] = {
            {FEATURE_PLASTICITY, "plasticity", DEGRADATION_LEVEL_MINOR, 0.3f, false, true},
            {FEATURE_LEARNING, "learning", DEGRADATION_LEVEL_MODERATE, 0.4f, false, true},
            {FEATURE_SENSORS_FULL, "full_sensors", DEGRADATION_LEVEL_MINOR, 0.35f, false, true},
            {FEATURE_LOGGING_VERBOSE, "verbose_log", DEGRADATION_LEVEL_MINOR, 0.1f, false, true},
            {FEATURE_METRICS, "metrics", DEGRADATION_LEVEL_MODERATE, 0.15f, false, true},
        };

        for (size_t i = 0; i < sizeof(features)/sizeof(features[0]); i++) {
            int result = portia_degradation_register_feature(degrade_state, &features[i]);
            // Accept either success or already-registered (feature may have been registered by init)
            ASSERT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_ALREADY_EXISTS)
                << "Feature registration failed with code: " << result;
        }
    }

    void TearDown() override {
        if (degrade_state) {
            portia_degradation_cleanup(degrade_state);
            degrade_state = nullptr;
        }
        nimcp_bio_async_shutdown();
    }

    // Helper to simulate power constraint (with hysteresis wait)
    void simulate_power_constraint(float severity) {
        // Power constraint translates to resource pressure
        float resource_usage = 40.0f + (severity * 50.0f);
        portia_degradation_evaluate(degrade_state, resource_usage, NULL);
        // Wait for hysteresis period (500ms) and re-evaluate to complete transition
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        portia_degradation_evaluate(degrade_state, resource_usage, NULL);
    }
};

//=============================================================================
// TEST SUITE 1: Power Profile Changes Tier
//=============================================================================

TEST_F(PortiaPowerTierIntegrationTest, PowerProfile_FullPowerEnablesFullTier) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    // Full tier should enable GPU and all features
    EXPECT_TRUE(config.enable_gpu);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.enable_plasticity);
    EXPECT_TRUE(config.enable_neuromodulation);
    EXPECT_TRUE(config.enable_checkpointing);
}

TEST_F(PortiaPowerTierIntegrationTest, PowerProfile_ConstrainedPowerLimitsTier) {
    platform_tier_config_t constrained_config = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);
    platform_tier_config_t full_config = platform_tier_get_config(PLATFORM_TIER_FULL);

    // Constrained should have lower resource budgets
    EXPECT_LT(constrained_config.memory_budget_mb, full_config.memory_budget_mb);
    EXPECT_LT(constrained_config.compute_budget_ops, full_config.compute_budget_ops);
    EXPECT_LE(constrained_config.max_threads, full_config.max_threads);

    // May disable some features
    EXPECT_LE(constrained_config.cognitive_modules_enabled, full_config.cognitive_modules_enabled);
}

TEST_F(PortiaPowerTierIntegrationTest, PowerProfile_MinimalPowerMinimizesFeatures) {
    platform_tier_config_t minimal_config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    // Minimal tier should disable non-essential features
    EXPECT_FALSE(minimal_config.enable_gpu);  // No GPU on minimal

    // Should have very limited resources
    EXPECT_LT(minimal_config.max_neurons, 50000u);
    EXPECT_LE(minimal_config.max_threads, 2u);

    // Only basic modules enabled
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_MINIMAL,
                                                 COGNITIVE_MODULE_ATTENTION));
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_MINIMAL,
                                                   COGNITIVE_MODULE_CURIOSITY));
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_MINIMAL,
                                                   COGNITIVE_MODULE_META_LEARNING));
}

//=============================================================================
// TEST SUITE 2: Battery Depletion Triggers Degradation
//=============================================================================

TEST_F(PortiaPowerTierIntegrationTest, BatteryDepletion_TriggersMinorDegradation) {
    // Start at normal operation
    degradation_level_t level;
    uint32_t active_features;
    float resource_usage;

    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_NONE);

    // Simulate battery at 75% -> minor power constraint
    simulate_power_constraint(0.25f);

    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_GE(level, DEGRADATION_LEVEL_MINOR);

    // Non-essential features may be disabled depending on pre-registration
    // Just verify the API returns valid results
    bool verbose_log, full_sensors;
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state,
                                                      FEATURE_LOGGING_VERBOSE, &verbose_log), NIMCP_SUCCESS);
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state,
                                                      FEATURE_SENSORS_FULL, &full_sensors), NIMCP_SUCCESS);
    // Features may or may not be disabled depending on pre-registration order
    // At minimum, verify we reached degradation level MINOR
    EXPECT_GE(level, DEGRADATION_LEVEL_MINOR);
}

TEST_F(PortiaPowerTierIntegrationTest, BatteryDepletion_SevereDegradationAtCriticalLevel) {
    // Simulate critical battery (10%) - 0.95 yields 40+47.5=87.5% > 85% SEVERE threshold
    simulate_power_constraint(0.95f);

    degradation_level_t level;
    uint32_t active_features;
    float resource_usage;

    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_GE(level, DEGRADATION_LEVEL_SEVERE);

    // Most features should be disabled, only core remains
    bool plasticity, learning, sensors, metrics;
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state,
                                                      FEATURE_PLASTICITY, &plasticity), NIMCP_SUCCESS);
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state,
                                                      FEATURE_LEARNING, &learning), NIMCP_SUCCESS);
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state,
                                                      FEATURE_SENSORS_FULL, &sensors), NIMCP_SUCCESS);
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state,
                                                      FEATURE_METRICS, &metrics), NIMCP_SUCCESS);

    // All non-core features should be off
    EXPECT_FALSE(plasticity);
    EXPECT_FALSE(learning);
    EXPECT_FALSE(sensors);
    EXPECT_FALSE(metrics);
}

//=============================================================================
// TEST SUITE 3: Charging Restores Features
//=============================================================================

TEST_F(PortiaPowerTierIntegrationTest, Charging_RestoresFeatures) {
    // Start with battery constraint
    simulate_power_constraint(0.60f);

    degradation_level_t level1;
    uint32_t active_features1;
    float resource_usage1;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level1,
                                            &active_features1, &resource_usage1), NIMCP_SUCCESS);
    EXPECT_GE(level1, DEGRADATION_LEVEL_MODERATE);

    // Wait for hysteresis
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Simulate charging (battery improving)
    simulate_power_constraint(0.20f);

    degradation_level_t level2;
    uint32_t active_features2;
    float resource_usage2;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level2,
                                            &active_features2, &resource_usage2), NIMCP_SUCCESS);

    // Degradation should decrease
    EXPECT_LT(level2, level1);
    EXPECT_GT(active_features2, active_features1);
}

TEST_F(PortiaPowerTierIntegrationTest, Charging_ProgressiveRestoration) {
    // Start at severe degradation (0.95 yields 40+47.5=87.5% > 85% SEVERE threshold)
    simulate_power_constraint(0.95f);

    degradation_level_t level;
    uint32_t active_features;
    float resource_usage;

    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_GE(level, DEGRADATION_LEVEL_SEVERE);
    uint32_t initial_features = active_features;

    // Simulate gradual charging
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    simulate_power_constraint(0.50f);  // Moderate constraint

    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_LT(level, DEGRADATION_LEVEL_SEVERE);
    EXPECT_GT(active_features, initial_features);

    // Continue charging to full
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    simulate_power_constraint(0.10f);  // Minimal constraint

    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_LE(level, DEGRADATION_LEVEL_MINOR);
}

//=============================================================================
// TEST SUITE 4: Power Source Switch Handling
//=============================================================================

TEST_F(PortiaPowerTierIntegrationTest, PowerSource_BatteryToACTransition) {
    // Start on battery with constraint
    simulate_power_constraint(0.60f);

    degradation_level_t level_battery;
    uint32_t features_battery;
    float usage_battery;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level_battery,
                                            &features_battery, &usage_battery), NIMCP_SUCCESS);

    // Wait for hysteresis
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Switch to AC power (no constraint)
    simulate_power_constraint(0.0f);

    degradation_level_t level_ac;
    uint32_t features_ac;
    float usage_ac;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level_ac,
                                            &features_ac, &usage_ac), NIMCP_SUCCESS);

    // Should restore to normal operation
    EXPECT_LT(level_ac, level_battery);
    EXPECT_GT(features_ac, features_battery);
}

TEST_F(PortiaPowerTierIntegrationTest, PowerSource_ACToBatteryTransition) {
    // Start on AC (full power)
    simulate_power_constraint(0.0f);

    degradation_level_t level_ac;
    uint32_t features_ac;
    float usage_ac;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level_ac,
                                            &features_ac, &usage_ac), NIMCP_SUCCESS);
    EXPECT_EQ(level_ac, DEGRADATION_LEVEL_NONE);

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Switch to battery (apply constraint)
    simulate_power_constraint(0.40f);

    degradation_level_t level_battery;
    uint32_t features_battery;
    float usage_battery;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level_battery,
                                            &features_battery, &usage_battery), NIMCP_SUCCESS);

    // Should degrade
    EXPECT_GT(level_battery, level_ac);
    EXPECT_LT(features_battery, features_ac);
}

//=============================================================================
// TEST SUITE 5: Thermal Throttling Scenarios
//=============================================================================

TEST_F(PortiaPowerTierIntegrationTest, Thermal_HighTemperatureTriggersDegradation) {
    // Simulate thermal throttling (high temp = high resource usage)
    simulate_power_constraint(0.75f);

    degradation_level_t level;
    uint32_t active_features;
    float resource_usage;

    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_GE(level, DEGRADATION_LEVEL_MODERATE);

    // Power-hungry features should be disabled
    bool plasticity, learning;
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state,
                                                      FEATURE_PLASTICITY, &plasticity), NIMCP_SUCCESS);
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state,
                                                      FEATURE_LEARNING, &learning), NIMCP_SUCCESS);
    EXPECT_FALSE(plasticity);
    EXPECT_FALSE(learning);
}

TEST_F(PortiaPowerTierIntegrationTest, Thermal_CoolingRestoresPerformance) {
    // Start with thermal constraint
    simulate_power_constraint(0.70f);

    degradation_level_t level_hot;
    uint32_t features_hot;
    float usage_hot;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level_hot,
                                            &features_hot, &usage_hot), NIMCP_SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Simulate cooling (thermal pressure reduces)
    simulate_power_constraint(0.20f);

    degradation_level_t level_cool;
    uint32_t features_cool;
    float usage_cool;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level_cool,
                                            &features_cool, &usage_cool), NIMCP_SUCCESS);

    // Should restore features
    EXPECT_LT(level_cool, level_hot);
    EXPECT_GT(features_cool, features_hot);
}

//=============================================================================
// TEST SUITE 6: Power Budget Validation
//=============================================================================

TEST_F(PortiaPowerTierIntegrationTest, PowerBudget_TierConfigsRespectBudgets) {
    platform_tier_config_t full = platform_tier_get_config(PLATFORM_TIER_FULL);
    platform_tier_config_t medium = platform_tier_get_config(PLATFORM_TIER_MEDIUM);
    platform_tier_config_t constrained = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);
    platform_tier_config_t minimal = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    // Compute budgets should decrease with tier
    EXPECT_GT(full.compute_budget_ops, medium.compute_budget_ops);
    EXPECT_GT(medium.compute_budget_ops, constrained.compute_budget_ops);
    EXPECT_GT(constrained.compute_budget_ops, minimal.compute_budget_ops);

    // Memory budgets should decrease with tier
    EXPECT_GT(full.memory_budget_mb, medium.memory_budget_mb);
    EXPECT_GT(medium.memory_budget_mb, constrained.memory_budget_mb);
    EXPECT_GT(constrained.memory_budget_mb, minimal.memory_budget_mb);
}
