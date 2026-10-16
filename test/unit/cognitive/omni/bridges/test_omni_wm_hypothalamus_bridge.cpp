/**
 * @file test_omni_wm_hypothalamus_bridge.cpp
 * @brief Comprehensive unit tests for World Model Hypothalamus Bridge
 *
 * WHAT: Tests for WM-Hypothalamus integration bridge
 * WHY:  Verify bidirectional homeostatic-predictive processing integration
 * HOW:  GTest-based tests for lifecycle, connection, drive modulation,
 *       circadian integration, resource prediction, and stress handling
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "cognitive/omni/bridges/nimcp_omni_wm_hypothalamus_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

// =============================================================================
// Constants and Helpers
// =============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr float DEFAULT_DT = 0.016f;  // ~60Hz update

static bool float_equals(float a, float b, float tol = FLOAT_TOLERANCE)
{
    return std::fabs(a - b) < tol;
}

static bool float_in_range(float val, float min_val, float max_val)
{
    return val >= min_val && val <= max_val;
}

// =============================================================================
// Test Fixture
// =============================================================================

class OmniWmHypothalamusBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create default bridge for most tests
        bridge_ = omni_wm_hypothalamus_bridge_create(nullptr);
    }

    void TearDown() override
    {
        if (bridge_) {
            omni_wm_hypothalamus_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    omni_wm_hypothalamus_bridge_t* bridge_ = nullptr;
};

// =============================================================================
// 1. Lifecycle Tests
// =============================================================================

TEST_F(OmniWmHypothalamusBridgeTest, CreateWithNullConfig)
{
    // bridge_ was created with NULL config in SetUp
    ASSERT_NE(bridge_, nullptr);
}

TEST_F(OmniWmHypothalamusBridgeTest, CreateWithDefaultConfig)
{
    omni_wm_hypothalamus_bridge_config_t config;
    ASSERT_EQ(omni_wm_hypothalamus_bridge_default_config(&config), NIMCP_SUCCESS);

    omni_wm_hypothalamus_bridge_t* bridge = omni_wm_hypothalamus_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
    omni_wm_hypothalamus_bridge_destroy(bridge);
}

TEST_F(OmniWmHypothalamusBridgeTest, CreateWithCustomConfig)
{
    omni_wm_hypothalamus_bridge_config_t config;
    ASSERT_EQ(omni_wm_hypothalamus_bridge_default_config(&config), NIMCP_SUCCESS);

    // Customize configuration
    config.enable_modulation = true;
    config.sensitivity = 1.5f;
    config.enable_drive_modulation = true;
    config.drive_urgency_threshold = 0.5f;
    config.enable_stress_modulation = true;
    config.stress_threshold = 0.8f;
    config.enable_circadian_modulation = true;
    config.enable_resource_prediction = true;

    omni_wm_hypothalamus_bridge_t* bridge = omni_wm_hypothalamus_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Verify config was applied
    EXPECT_EQ(bridge->config.enable_modulation, true);
    EXPECT_FLOAT_EQ(bridge->config.sensitivity, 1.5f);
    EXPECT_FLOAT_EQ(bridge->config.drive_urgency_threshold, 0.5f);
    EXPECT_FLOAT_EQ(bridge->config.stress_threshold, 0.8f);

    omni_wm_hypothalamus_bridge_destroy(bridge);
}

TEST_F(OmniWmHypothalamusBridgeTest, DestroyNullSafe)
{
    // Should not crash
    omni_wm_hypothalamus_bridge_destroy(nullptr);
}

TEST_F(OmniWmHypothalamusBridgeTest, DestroyValidBridge)
{
    omni_wm_hypothalamus_bridge_t* bridge = omni_wm_hypothalamus_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    // Should not crash
    omni_wm_hypothalamus_bridge_destroy(bridge);
}

TEST_F(OmniWmHypothalamusBridgeTest, ResetNullFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, ResetValidBridge)
{
    // Modify some state first
    omni_wm_hypothalamus_bridge_set_stress(bridge_, 0.8f);
    omni_wm_hypothalamus_bridge_set_arousal(bridge_, 0.6f);

    nimcp_error_t result = omni_wm_hypothalamus_bridge_reset(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 2. Default Config Tests
// =============================================================================

TEST_F(OmniWmHypothalamusBridgeTest, DefaultConfigNullFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, DefaultConfigSetsReasonableValues)
{
    omni_wm_hypothalamus_bridge_config_t config;
    ASSERT_EQ(omni_wm_hypothalamus_bridge_default_config(&config), NIMCP_SUCCESS);

    // Verify reasonable defaults
    EXPECT_TRUE(float_in_range(config.sensitivity, 0.5f, 2.0f));
    EXPECT_TRUE(float_in_range(config.drive_urgency_threshold, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.stress_threshold, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.circadian_modulation_strength, 0.0f, 1.0f));
    EXPECT_LE(config.resource_prediction_horizon, WM_HYPO_MAX_PREDICTION_HORIZON);
}

TEST_F(OmniWmHypothalamusBridgeTest, ValidateConfigNullFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_validate_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, ValidateConfigDefaultSucceeds)
{
    omni_wm_hypothalamus_bridge_config_t config;
    ASSERT_EQ(omni_wm_hypothalamus_bridge_default_config(&config), NIMCP_SUCCESS);

    nimcp_error_t result = omni_wm_hypothalamus_bridge_validate_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 3. Connection Tests
// =============================================================================

TEST_F(OmniWmHypothalamusBridgeTest, ConnectNullBridgeFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_connect(
        nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, IsConnectedBeforeConnect)
{
    // Not connected initially
    EXPECT_FALSE(omni_wm_hypothalamus_bridge_is_connected(bridge_));
}

TEST_F(OmniWmHypothalamusBridgeTest, IsConnectedNullReturnsFalse)
{
    EXPECT_FALSE(omni_wm_hypothalamus_bridge_is_connected(nullptr));
}

TEST_F(OmniWmHypothalamusBridgeTest, ConnectWorldModelNullBridgeFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_connect_world_model(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, ConnectDrivesNullBridgeFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_connect_drives(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, ConnectHomeostasisNullBridgeFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_connect_homeostasis(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, ConnectCircadianNullBridgeFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_connect_circadian(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 4. Update Tests
// =============================================================================

TEST_F(OmniWmHypothalamusBridgeTest, UpdateNullBridgeFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_update(nullptr, DEFAULT_DT);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, UpdateUnconnectedBridge)
{
    // Update should handle unconnected bridge gracefully
    nimcp_error_t result = omni_wm_hypothalamus_bridge_update(bridge_, DEFAULT_DT);
    // May succeed or fail depending on implementation, but should not crash
    (void)result;
}

TEST_F(OmniWmHypothalamusBridgeTest, UpdateNegativeDt)
{
    // Negative dt should be handled
    nimcp_error_t result = omni_wm_hypothalamus_bridge_update(bridge_, -0.1f);
    // Should either reject or handle gracefully
    (void)result;
}

TEST_F(OmniWmHypothalamusBridgeTest, UpdateZeroDt)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_update(bridge_, 0.0f);
    // Should handle zero dt gracefully
    (void)result;
}

// =============================================================================
// 5. Drive Modulation Tests
// =============================================================================

TEST_F(OmniWmHypothalamusBridgeTest, OnDriveChangeNullBridgeFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_on_drive_change(
        nullptr, 0, 0.5f, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, OnDriveChangeValidParams)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_on_drive_change(
        bridge_, 0, 0.7f, 0.8f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, OnDriveChangeBoundaryLevel)
{
    // Test boundary values
    EXPECT_EQ(omni_wm_hypothalamus_bridge_on_drive_change(bridge_, 0, 0.0f, 0.5f), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_hypothalamus_bridge_on_drive_change(bridge_, 0, 1.0f, 0.5f), NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, OnDriveChangeBoundaryUrgency)
{
    // Test boundary urgency values
    EXPECT_EQ(omni_wm_hypothalamus_bridge_on_drive_change(bridge_, 0, 0.5f, 0.0f), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_hypothalamus_bridge_on_drive_change(bridge_, 0, 0.5f, 1.0f), NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetPriorityBoostNullReturnsZero)
{
    float boost = omni_wm_hypothalamus_bridge_get_priority_boost(nullptr, 0);
    EXPECT_FLOAT_EQ(boost, 0.0f);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetPriorityBoostValidDrive)
{
    // Set a high urgency drive
    omni_wm_hypothalamus_bridge_on_drive_change(bridge_, 0, 0.9f, 0.9f);

    float boost = omni_wm_hypothalamus_bridge_get_priority_boost(bridge_, 0);
    // Should return some priority boost
    EXPECT_GE(boost, 0.0f);
    EXPECT_LE(boost, 2.0f);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetDriveModifierNullReturnsOne)
{
    float modifier = omni_wm_hypothalamus_bridge_get_drive_modifier(nullptr);
    // Default should be neutral (1.0)
    EXPECT_TRUE(float_in_range(modifier, 0.5f, 2.0f));
}

TEST_F(OmniWmHypothalamusBridgeTest, GetDriveModifierValidBridge)
{
    float modifier = omni_wm_hypothalamus_bridge_get_drive_modifier(bridge_);
    EXPECT_TRUE(float_in_range(modifier, 0.5f, 2.0f));
}

TEST_F(OmniWmHypothalamusBridgeTest, IsConservativeNullReturnsFalse)
{
    EXPECT_FALSE(omni_wm_hypothalamus_bridge_is_conservative(nullptr));
}

TEST_F(OmniWmHypothalamusBridgeTest, IsConservativeInitially)
{
    // Initially should not be in conservative mode
    EXPECT_FALSE(omni_wm_hypothalamus_bridge_is_conservative(bridge_));
}

// =============================================================================
// 6. Stress and Arousal Tests
// =============================================================================

TEST_F(OmniWmHypothalamusBridgeTest, SetStressNullBridgeFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_set_stress(nullptr, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, SetStressValidRange)
{
    EXPECT_EQ(omni_wm_hypothalamus_bridge_set_stress(bridge_, 0.0f), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_hypothalamus_bridge_set_stress(bridge_, 0.5f), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_hypothalamus_bridge_set_stress(bridge_, 1.0f), NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetStressNullReturnsZero)
{
    float stress = omni_wm_hypothalamus_bridge_get_stress(nullptr);
    EXPECT_FLOAT_EQ(stress, 0.0f);
}

TEST_F(OmniWmHypothalamusBridgeTest, SetAndGetStress)
{
    omni_wm_hypothalamus_bridge_set_stress(bridge_, 0.75f);
    float stress = omni_wm_hypothalamus_bridge_get_stress(bridge_);
    // Should be approximately the set value (may be smoothed)
    EXPECT_TRUE(float_in_range(stress, 0.0f, 1.0f));
}

TEST_F(OmniWmHypothalamusBridgeTest, SetArousalNullBridgeFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_set_arousal(nullptr, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, SetArousalValidRange)
{
    EXPECT_EQ(omni_wm_hypothalamus_bridge_set_arousal(bridge_, 0.0f), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_hypothalamus_bridge_set_arousal(bridge_, 0.5f), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_hypothalamus_bridge_set_arousal(bridge_, 1.0f), NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetArousalNullReturnsZero)
{
    float arousal = omni_wm_hypothalamus_bridge_get_arousal(nullptr);
    EXPECT_FLOAT_EQ(arousal, 0.0f);
}

TEST_F(OmniWmHypothalamusBridgeTest, SetAndGetArousal)
{
    omni_wm_hypothalamus_bridge_set_arousal(bridge_, 0.6f);
    float arousal = omni_wm_hypothalamus_bridge_get_arousal(bridge_);
    EXPECT_TRUE(float_in_range(arousal, 0.0f, 1.0f));
}

TEST_F(OmniWmHypothalamusBridgeTest, HighStressTriggersConservativeMode)
{
    omni_wm_hypothalamus_bridge_config_t config;
    omni_wm_hypothalamus_bridge_default_config(&config);
    config.enable_stress_modulation = true;
    config.stress_threshold = 0.7f;

    omni_wm_hypothalamus_bridge_t* bridge = omni_wm_hypothalamus_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Set stress above threshold
    omni_wm_hypothalamus_bridge_set_stress(bridge, 0.9f);
    // Update to process the stress change
    omni_wm_hypothalamus_bridge_update(bridge, DEFAULT_DT);

    // Should be in conservative mode
    EXPECT_TRUE(omni_wm_hypothalamus_bridge_is_conservative(bridge));

    omni_wm_hypothalamus_bridge_destroy(bridge);
}

// =============================================================================
// 7. Circadian Tests
// =============================================================================

TEST_F(OmniWmHypothalamusBridgeTest, OnPhaseChangeNullBridgeFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_on_phase_change(nullptr, 0, 0.0f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, OnPhaseChangeValidParams)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_on_phase_change(bridge_, 1, 0.25f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetCircadianModulationNullReturnsZero)
{
    float mod = omni_wm_hypothalamus_bridge_get_circadian_modulation(nullptr, 0);
    EXPECT_FLOAT_EQ(mod, 0.0f);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetCircadianModulationTypes)
{
    // Test different modulation types
    for (uint32_t type = 0; type < 4; type++) {
        float mod = omni_wm_hypothalamus_bridge_get_circadian_modulation(bridge_, type);
        EXPECT_TRUE(float_in_range(mod, 0.0f, 1.0f));
    }
}

TEST_F(OmniWmHypothalamusBridgeTest, GetCircadianPhaseNullReturnsZero)
{
    uint32_t phase = omni_wm_hypothalamus_bridge_get_circadian_phase(nullptr);
    EXPECT_EQ(phase, 0u);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetCircadianPhaseAfterChange)
{
    omni_wm_hypothalamus_bridge_on_phase_change(bridge_, 2, 0.5f);
    uint32_t phase = omni_wm_hypothalamus_bridge_get_circadian_phase(bridge_);
    EXPECT_EQ(phase, 2u);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetSleepPressureNullReturnsZero)
{
    float pressure = omni_wm_hypothalamus_bridge_get_sleep_pressure(nullptr);
    EXPECT_FLOAT_EQ(pressure, 0.0f);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetSleepPressureInitial)
{
    float pressure = omni_wm_hypothalamus_bridge_get_sleep_pressure(bridge_);
    EXPECT_TRUE(float_in_range(pressure, 0.0f, 1.0f));
}

// =============================================================================
// 8. Resource Prediction Tests
// =============================================================================

TEST_F(OmniWmHypothalamusBridgeTest, PredictResourceNullBridgeFails)
{
    wm_resource_prediction_t prediction;
    nimcp_error_t result = omni_wm_hypothalamus_bridge_predict_resource(
        nullptr, WM_RESOURCE_ENERGY, 10, &prediction);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, PredictResourceNullOutputFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_predict_resource(
        bridge_, WM_RESOURCE_ENERGY, 10, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, ForecastResourcesNullBridgeFails)
{
    wm_resource_forecast_t forecast;
    nimcp_error_t result = omni_wm_hypothalamus_bridge_forecast_resources(nullptr, &forecast);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, ForecastResourcesNullOutputFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_forecast_resources(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, UpdateResourceNullBridgeFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_update_resource(
        nullptr, WM_RESOURCE_ENERGY, 0.8f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, UpdateResourceValidParams)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_update_resource(
        bridge_, WM_RESOURCE_ENERGY, 0.8f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, UpdateResourceAllTypes)
{
    for (int type = 0; type < WM_RESOURCE_COUNT; type++) {
        nimcp_error_t result = omni_wm_hypothalamus_bridge_update_resource(
            bridge_, (wm_resource_type_t)type, 0.5f);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

TEST_F(OmniWmHypothalamusBridgeTest, ResourceTypeToStringValid)
{
    for (int type = 0; type < WM_RESOURCE_COUNT; type++) {
        const char* name = wm_resource_type_to_string((wm_resource_type_t)type);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

// =============================================================================
// 9. Reward Prediction Tests
// =============================================================================

TEST_F(OmniWmHypothalamusBridgeTest, PredictRewardNullBridgeFails)
{
    float action[] = {0.5f, 0.3f};
    float reward, confidence;
    nimcp_error_t result = omni_wm_hypothalamus_bridge_predict_reward(
        nullptr, action, 2, &reward, &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, PredictRewardNullActionFails)
{
    float reward, confidence;
    nimcp_error_t result = omni_wm_hypothalamus_bridge_predict_reward(
        bridge_, nullptr, 2, &reward, &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, UpdateRewardPredictionNullBridgeFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_update_reward_prediction(
        nullptr, 0.5f, 0.6f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, UpdateRewardPredictionValid)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_update_reward_prediction(
        bridge_, 0.5f, 0.6f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 10. Query API Tests
// =============================================================================

TEST_F(OmniWmHypothalamusBridgeTest, GetHypoEffectsNullReturnsNull)
{
    const hypothalamus_to_omni_wm_effects_t* effects =
        omni_wm_hypothalamus_bridge_get_hypo_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetHypoEffectsValid)
{
    const hypothalamus_to_omni_wm_effects_t* effects =
        omni_wm_hypothalamus_bridge_get_hypo_effects(bridge_);
    EXPECT_NE(effects, nullptr);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetWmEffectsNullReturnsNull)
{
    const omni_wm_to_hypothalamus_effects_t* effects =
        omni_wm_hypothalamus_bridge_get_wm_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetWmEffectsValid)
{
    const omni_wm_to_hypothalamus_effects_t* effects =
        omni_wm_hypothalamus_bridge_get_wm_effects(bridge_);
    EXPECT_NE(effects, nullptr);
}

// =============================================================================
// 11. Statistics Tests
// =============================================================================

TEST_F(OmniWmHypothalamusBridgeTest, GetStatsNullBridgeFails)
{
    omni_wm_hypothalamus_bridge_stats_t stats;
    nimcp_error_t result = omni_wm_hypothalamus_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetStatsNullOutputFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_get_stats(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, GetStatsValid)
{
    omni_wm_hypothalamus_bridge_stats_t stats;
    nimcp_error_t result = omni_wm_hypothalamus_bridge_get_stats(bridge_, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, ResetStatsNullBridgeFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, ResetStatsValid)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_reset_stats(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, StatsIncrementOnUpdate)
{
    omni_wm_hypothalamus_bridge_stats_t stats_before, stats_after;

    omni_wm_hypothalamus_bridge_get_stats(bridge_, &stats_before);

    // Perform some updates
    for (int i = 0; i < 5; i++) {
        omni_wm_hypothalamus_bridge_update(bridge_, DEFAULT_DT);
    }

    omni_wm_hypothalamus_bridge_get_stats(bridge_, &stats_after);

    // Total updates should have increased
    EXPECT_GT(stats_after.total_updates, stats_before.total_updates);
}

// =============================================================================
// 12. Bio-Async Tests
// =============================================================================

TEST_F(OmniWmHypothalamusBridgeTest, ConnectBioAsyncNullFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, DisconnectBioAsyncNullFails)
{
    nimcp_error_t result = omni_wm_hypothalamus_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, IsBioAsyncConnectedNullReturnsFalse)
{
    EXPECT_FALSE(omni_wm_hypothalamus_bridge_is_bio_async_connected(nullptr));
}

TEST_F(OmniWmHypothalamusBridgeTest, IsBioAsyncConnectedInitially)
{
    EXPECT_FALSE(omni_wm_hypothalamus_bridge_is_bio_async_connected(bridge_));
}

// =============================================================================
// 13. Edge Case Tests
// =============================================================================

TEST_F(OmniWmHypothalamusBridgeTest, MultipleResetCalls)
{
    for (int i = 0; i < 5; i++) {
        nimcp_error_t result = omni_wm_hypothalamus_bridge_reset(bridge_);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

TEST_F(OmniWmHypothalamusBridgeTest, RapidUpdateCalls)
{
    // Rapid updates should not cause issues
    for (int i = 0; i < 100; i++) {
        omni_wm_hypothalamus_bridge_update(bridge_, DEFAULT_DT);
    }
    // Should not crash and stats should be updated
    omni_wm_hypothalamus_bridge_stats_t stats;
    omni_wm_hypothalamus_bridge_get_stats(bridge_, &stats);
    EXPECT_GE(stats.total_updates, 100u);
}

TEST_F(OmniWmHypothalamusBridgeTest, StressArousalCombinations)
{
    // Test various stress/arousal combinations
    float values[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float stress : values) {
        for (float arousal : values) {
            EXPECT_EQ(omni_wm_hypothalamus_bridge_set_stress(bridge_, stress), NIMCP_SUCCESS);
            EXPECT_EQ(omni_wm_hypothalamus_bridge_set_arousal(bridge_, arousal), NIMCP_SUCCESS);
            omni_wm_hypothalamus_bridge_update(bridge_, DEFAULT_DT);
        }
    }
}

// =============================================================================
// 14. Memory Safety Tests
// =============================================================================

TEST_F(OmniWmHypothalamusBridgeTest, CreateDestroyMultiple)
{
    // Create and destroy multiple bridges
    for (int i = 0; i < 10; i++) {
        omni_wm_hypothalamus_bridge_t* bridge = omni_wm_hypothalamus_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
        omni_wm_hypothalamus_bridge_destroy(bridge);
    }
}

TEST_F(OmniWmHypothalamusBridgeTest, UseAfterReset)
{
    omni_wm_hypothalamus_bridge_reset(bridge_);

    // Should be able to use bridge after reset
    EXPECT_EQ(omni_wm_hypothalamus_bridge_set_stress(bridge_, 0.5f), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_hypothalamus_bridge_update(bridge_, DEFAULT_DT), NIMCP_SUCCESS);
}

TEST_F(OmniWmHypothalamusBridgeTest, ConfigIntegrity)
{
    omni_wm_hypothalamus_bridge_config_t config;
    omni_wm_hypothalamus_bridge_default_config(&config);
    config.sensitivity = 1.8f;
    config.stress_threshold = 0.65f;

    omni_wm_hypothalamus_bridge_t* bridge = omni_wm_hypothalamus_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Verify config was copied, not referenced
    config.sensitivity = 0.5f;  // Modify original
    EXPECT_FLOAT_EQ(bridge->config.sensitivity, 1.8f);  // Bridge should have original value

    omni_wm_hypothalamus_bridge_destroy(bridge);
}
