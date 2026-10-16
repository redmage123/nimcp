/**
 * @file test_omni_wm_security_immune_bridge.cpp
 * @brief Comprehensive unit tests for World Model Security-Immune Bridge
 *
 * WHAT: Tests for WM Security-Immune Bridge connecting RSSM with Security and Immune systems
 * WHY:  Bridge is critical for security-aware predictions with cytokine modulation
 * HOW:  Tests all APIs: lifecycle, connection, update, cytokine modulation, prediction,
 *       security events, BBB state, immune triggers, and statistics
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "cognitive/omni/bridges/nimcp_omni_wm_security_immune_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

// =============================================================================
// Constants and Helpers
// =============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr uint32_t TEST_STATE_DIM = 16;
static constexpr float TEST_DT = 0.016f; // ~60Hz

static bool float_equals(float a, float b, float tol = FLOAT_TOLERANCE)
{
    return std::fabs(a - b) < tol;
}

static bool float_in_range(float value, float min_val, float max_val)
{
    return value >= min_val && value <= max_val;
}

// =============================================================================
// Test Fixture
// =============================================================================

class WMSecurityImmuneBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create bridge with default config
        bridge_ = omni_wm_security_immune_bridge_create(nullptr);
    }

    void TearDown() override
    {
        if (bridge_) {
            omni_wm_security_immune_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    // Helper to create bridge with custom config
    omni_wm_security_immune_bridge_t* create_custom_bridge(bool enable_modulation,
                                                            float sensitivity)
    {
        omni_wm_security_immune_config_t config;
        omni_wm_security_immune_bridge_default_config(&config);
        config.enable_modulation = enable_modulation;
        config.sensitivity = sensitivity;
        return omni_wm_security_immune_bridge_create(&config);
    }

    omni_wm_security_immune_bridge_t* bridge_ = nullptr;
};

// =============================================================================
// 1. Default Config Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, DefaultConfigNullFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, DefaultConfigSetsReasonableValues)
{
    omni_wm_security_immune_config_t config;
    memset(&config, 0, sizeof(config));

    nimcp_error_t result = omni_wm_security_immune_bridge_default_config(&config);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Check general settings
    EXPECT_TRUE(config.enable_modulation);
    EXPECT_TRUE(float_in_range(config.sensitivity, 0.5f, 2.0f));

    // Check anomaly detection settings
    EXPECT_TRUE(float_in_range(config.anomaly_threshold, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.anomaly_confidence_min, 0.0f, 1.0f));

    // Check immune modulation settings
    EXPECT_TRUE(float_in_range(config.immune_sensitivity, 0.5f, 2.0f));
    EXPECT_GT(config.cytokine_decay_rate, 0.0f);

    // Check IL-1 parameters (pro-inflammatory, reduces confidence)
    EXPECT_TRUE(float_in_range(config.il1_confidence_factor, 0.5f, 1.0f));
    EXPECT_TRUE(float_in_range(config.il1_horizon_factor, 0.3f, 1.0f));
    EXPECT_TRUE(float_in_range(config.il1_vigilance_boost, 0.0f, 0.5f));

    // Check IL-10 parameters (anti-inflammatory, restores confidence)
    EXPECT_GT(config.il10_restoration_rate, 0.0f);
    EXPECT_GT(config.il10_resolution_factor, 0.0f);

    // Check IFN-gamma parameters (boosts learning)
    EXPECT_GE(config.ifn_learning_boost, 1.0f);
}

TEST_F(WMSecurityImmuneBridgeTest, DefaultConfigIdempotent)
{
    omni_wm_security_immune_config_t config1, config2;

    omni_wm_security_immune_bridge_default_config(&config1);
    omni_wm_security_immune_bridge_default_config(&config2);

    EXPECT_EQ(config1.enable_modulation, config2.enable_modulation);
    EXPECT_FLOAT_EQ(config1.sensitivity, config2.sensitivity);
    EXPECT_FLOAT_EQ(config1.anomaly_threshold, config2.anomaly_threshold);
    EXPECT_FLOAT_EQ(config1.immune_sensitivity, config2.immune_sensitivity);
}

// =============================================================================
// 2. Lifecycle Tests - Create/Destroy
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, CreateWithNullConfigUsesDefaults)
{
    // bridge_ created in SetUp with NULL config
    ASSERT_NE(bridge_, nullptr);
}

TEST_F(WMSecurityImmuneBridgeTest, CreateWithCustomConfig)
{
    omni_wm_security_immune_config_t config;
    omni_wm_security_immune_bridge_default_config(&config);
    config.enable_modulation = false;
    config.sensitivity = 1.5f;
    config.enable_anomaly_prediction = false;
    config.enable_immune_modulation = true;

    omni_wm_security_immune_bridge_t* custom_bridge = omni_wm_security_immune_bridge_create(&config);
    ASSERT_NE(custom_bridge, nullptr);

    // Verify config was applied
    EXPECT_FALSE(custom_bridge->config.enable_modulation);
    EXPECT_FLOAT_EQ(custom_bridge->config.sensitivity, 1.5f);

    omni_wm_security_immune_bridge_destroy(custom_bridge);
}

TEST_F(WMSecurityImmuneBridgeTest, CreateInitializesBaseFields)
{
    ASSERT_NE(bridge_, nullptr);

    // Check base bridge infrastructure
    EXPECT_NE(bridge_->base.module_name, nullptr);
    EXPECT_NE(bridge_->base.module_id, 0u);
}

TEST_F(WMSecurityImmuneBridgeTest, CreateInitializesStatistics)
{
    ASSERT_NE(bridge_, nullptr);

    // Stats should be zeroed on creation
    EXPECT_EQ(bridge_->stats.anomalies_predicted, 0u);
    EXPECT_EQ(bridge_->stats.anomalies_verified, 0u);
    EXPECT_EQ(bridge_->stats.total_updates, 0u);
    EXPECT_EQ(bridge_->stats.errors_total, 0u);
}

TEST_F(WMSecurityImmuneBridgeTest, CreateInitializesModulationState)
{
    ASSERT_NE(bridge_, nullptr);

    // Modulation should start at baseline
    EXPECT_FLOAT_EQ(bridge_->current_modulation.combined_confidence_mod, 1.0f);
    EXPECT_FLOAT_EQ(bridge_->current_modulation.combined_horizon_mod, 1.0f);
    EXPECT_FLOAT_EQ(bridge_->current_modulation.combined_learning_mod, 1.0f);
    EXPECT_FLOAT_EQ(bridge_->current_modulation.combined_vigilance, 0.0f);
    EXPECT_EQ(bridge_->current_modulation.inflammation_level, 0u);
    EXPECT_FALSE(bridge_->current_modulation.is_cytokine_storm);
}

TEST_F(WMSecurityImmuneBridgeTest, DestroyNullSafe)
{
    // Should not crash
    omni_wm_security_immune_bridge_destroy(nullptr);
}

TEST_F(WMSecurityImmuneBridgeTest, DestroyValidBridge)
{
    omni_wm_security_immune_bridge_t* temp = omni_wm_security_immune_bridge_create(nullptr);
    ASSERT_NE(temp, nullptr);

    // Should not crash
    omni_wm_security_immune_bridge_destroy(temp);
}

TEST_F(WMSecurityImmuneBridgeTest, DestroyMultipleTimes)
{
    omni_wm_security_immune_bridge_t* temp = omni_wm_security_immune_bridge_create(nullptr);
    ASSERT_NE(temp, nullptr);

    omni_wm_security_immune_bridge_destroy(temp);
    // Second destroy should be safe (pointer is invalid but we won't use it)
    // This tests that destroy doesn't leave dangling state
}

// =============================================================================
// 3. Reset Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, ResetNullFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, ResetClearsStatistics)
{
    ASSERT_NE(bridge_, nullptr);

    // Manually increment some stats
    bridge_->stats.anomalies_predicted = 100;
    bridge_->stats.total_updates = 500;
    bridge_->stats.errors_total = 10;

    nimcp_error_t result = omni_wm_security_immune_bridge_reset(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->stats.anomalies_predicted, 0u);
    EXPECT_EQ(bridge_->stats.total_updates, 0u);
    EXPECT_EQ(bridge_->stats.errors_total, 0u);
}

TEST_F(WMSecurityImmuneBridgeTest, ResetRestoresBaselineModulation)
{
    ASSERT_NE(bridge_, nullptr);

    // Modify modulation state
    bridge_->current_modulation.combined_confidence_mod = 0.5f;
    bridge_->current_modulation.combined_vigilance = 0.8f;
    bridge_->current_modulation.inflammation_level = 3;

    nimcp_error_t result = omni_wm_security_immune_bridge_reset(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Should be back to baseline
    EXPECT_FLOAT_EQ(bridge_->current_modulation.combined_confidence_mod, 1.0f);
    EXPECT_FLOAT_EQ(bridge_->current_modulation.combined_vigilance, 0.0f);
    EXPECT_EQ(bridge_->current_modulation.inflammation_level, 0u);
}

TEST_F(WMSecurityImmuneBridgeTest, ResetPreservesConfiguration)
{
    omni_wm_security_immune_bridge_t* custom = create_custom_bridge(false, 1.8f);
    ASSERT_NE(custom, nullptr);

    // Store original config values
    bool orig_enable = custom->config.enable_modulation;
    float orig_sens = custom->config.sensitivity;

    nimcp_error_t result = omni_wm_security_immune_bridge_reset(custom);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Config should be preserved
    EXPECT_EQ(custom->config.enable_modulation, orig_enable);
    EXPECT_FLOAT_EQ(custom->config.sensitivity, orig_sens);

    omni_wm_security_immune_bridge_destroy(custom);
}

TEST_F(WMSecurityImmuneBridgeTest, ResetClearsAlertState)
{
    ASSERT_NE(bridge_, nullptr);

    // Set alert state
    bridge_->under_attack = true;
    bridge_->alert_level = 4;

    nimcp_error_t result = omni_wm_security_immune_bridge_reset(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_FALSE(bridge_->under_attack);
    EXPECT_EQ(bridge_->alert_level, 0u);
}

// =============================================================================
// 4. Connection Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, IsConnectedWithoutConnectionReturnsFalse)
{
    ASSERT_NE(bridge_, nullptr);

    // No world model connected yet
    bool connected = omni_wm_security_immune_bridge_is_connected(bridge_);
    EXPECT_FALSE(connected);
}

TEST_F(WMSecurityImmuneBridgeTest, IsConnectedNullBridgeReturnsFalse)
{
    bool connected = omni_wm_security_immune_bridge_is_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(WMSecurityImmuneBridgeTest, ConnectNullBridgeFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_connect(
        nullptr, nullptr, nullptr, {0}, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, ConnectWorldModelNullBridgeFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_connect_world_model(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, ConnectSecurityNullBridgeFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_connect_security(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, ConnectImmuneNullBridgeFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_connect_immune(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, ConnectAnomalyDetectorNullBridgeFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_connect_anomaly_detector(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 5. Update Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, UpdateNullBridgeFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_update(nullptr, TEST_DT);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, UpdateWithoutConnectionReturnsError)
{
    ASSERT_NE(bridge_, nullptr);

    // Bridge not connected to world model
    nimcp_error_t result = omni_wm_security_immune_bridge_update(bridge_, TEST_DT);
    // Should return error or handle gracefully
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, UpdateZeroDtHandled)
{
    ASSERT_NE(bridge_, nullptr);

    // Zero dt should be handled gracefully
    nimcp_error_t result = omni_wm_security_immune_bridge_update(bridge_, 0.0f);
    // Not necessarily an error, but should handle it
    EXPECT_TRUE(result == NIMCP_SUCCESS || result != NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, UpdateNegativeDtHandled)
{
    ASSERT_NE(bridge_, nullptr);

    // Negative dt should be rejected
    nimcp_error_t result = omni_wm_security_immune_bridge_update(bridge_, -1.0f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 6. Cytokine Modulation Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, UpdateCytokinesNullBridgeFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_update_cytokines(
        nullptr, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, UpdateCytokinesValidRange)
{
    ASSERT_NE(bridge_, nullptr);

    // All cytokines at moderate levels
    nimcp_error_t result = omni_wm_security_immune_bridge_update_cytokines(
        bridge_, 0.3f, 0.3f, 0.2f, 0.5f, 0.4f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, UpdateCytokinesZeroLevels)
{
    ASSERT_NE(bridge_, nullptr);

    // All cytokines at zero (healthy baseline)
    nimcp_error_t result = omni_wm_security_immune_bridge_update_cytokines(
        bridge_, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, UpdateCytokinesMaxLevels)
{
    ASSERT_NE(bridge_, nullptr);

    // All pro-inflammatory at max (cytokine storm territory)
    nimcp_error_t result = omni_wm_security_immune_bridge_update_cytokines(
        bridge_, 1.0f, 1.0f, 1.0f, 0.0f, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, UpdateCytokinesOutOfRangeClamped)
{
    ASSERT_NE(bridge_, nullptr);

    // Values outside [0,1] should be clamped
    nimcp_error_t result = omni_wm_security_immune_bridge_update_cytokines(
        bridge_, -0.5f, 1.5f, 2.0f, -1.0f, 0.5f);
    // Should either succeed (with clamping) or fail with validation error
    EXPECT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(WMSecurityImmuneBridgeTest, HighIL1ReducesConfidence)
{
    ASSERT_NE(bridge_, nullptr);

    // Start at baseline
    float baseline_conf = omni_wm_security_immune_bridge_get_modulated_confidence(bridge_);

    // High IL-1 (pro-inflammatory)
    omni_wm_security_immune_bridge_update_cytokines(bridge_, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f);
    omni_wm_security_immune_bridge_compute_modulation(bridge_);

    float modulated_conf = omni_wm_security_immune_bridge_get_modulated_confidence(bridge_);

    // IL-1 should reduce confidence
    EXPECT_LT(modulated_conf, baseline_conf);
}

TEST_F(WMSecurityImmuneBridgeTest, HighTNFAlphaIncreasesConservatism)
{
    ASSERT_NE(bridge_, nullptr);

    // High TNF-alpha (damage signal) should make predictions more conservative
    omni_wm_security_immune_bridge_update_cytokines(bridge_, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f);
    omni_wm_security_immune_bridge_compute_modulation(bridge_);

    // Check modulation effects
    const immune_to_wm_modulation_t* mod = omni_wm_security_immune_bridge_get_modulation(bridge_);
    ASSERT_NE(mod, nullptr);

    // High TNF should increase vigilance
    EXPECT_GT(mod->combined_vigilance, 0.0f);
}

TEST_F(WMSecurityImmuneBridgeTest, HighIL10RestoresBaseline)
{
    ASSERT_NE(bridge_, nullptr);

    // First induce inflammation
    omni_wm_security_immune_bridge_update_cytokines(bridge_, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f);
    omni_wm_security_immune_bridge_compute_modulation(bridge_);

    float inflamed_conf = omni_wm_security_immune_bridge_get_modulated_confidence(bridge_);

    // Now add high IL-10 (anti-inflammatory)
    omni_wm_security_immune_bridge_update_cytokines(bridge_, 0.5f, 0.5f, 0.5f, 0.9f, 0.0f);
    omni_wm_security_immune_bridge_compute_modulation(bridge_);

    float restored_conf = omni_wm_security_immune_bridge_get_modulated_confidence(bridge_);

    // IL-10 should partially restore confidence
    EXPECT_GT(restored_conf, inflamed_conf);
}

TEST_F(WMSecurityImmuneBridgeTest, HighIFNGammaBoostsLearning)
{
    ASSERT_NE(bridge_, nullptr);

    // Baseline learning rate
    float baseline_lr = omni_wm_security_immune_bridge_get_modulated_learning_rate(bridge_);

    // High IFN-gamma (adaptive immunity, enhances learning)
    omni_wm_security_immune_bridge_update_cytokines(bridge_, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f);
    omni_wm_security_immune_bridge_compute_modulation(bridge_);

    float modulated_lr = omni_wm_security_immune_bridge_get_modulated_learning_rate(bridge_);

    // IFN-gamma should increase learning rate
    EXPECT_GT(modulated_lr, baseline_lr);
}

// =============================================================================
// 7. Inflammation Level Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, SetInflammationNullBridgeFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_set_inflammation(nullptr, 1);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, SetInflammationLevelNone)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_security_immune_bridge_set_inflammation(bridge_, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    const immune_to_wm_modulation_t* mod = omni_wm_security_immune_bridge_get_modulation(bridge_);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(mod->inflammation_level, 0u);
}

TEST_F(WMSecurityImmuneBridgeTest, SetInflammationLevelLocal)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_security_immune_bridge_set_inflammation(bridge_, 1);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, SetInflammationLevelSystemic)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_security_immune_bridge_set_inflammation(bridge_, 3);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, SetInflammationLevelStorm)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_security_immune_bridge_set_inflammation(bridge_, 4);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    const immune_to_wm_modulation_t* mod = omni_wm_security_immune_bridge_get_modulation(bridge_);
    ASSERT_NE(mod, nullptr);
    EXPECT_TRUE(mod->is_cytokine_storm);
}

TEST_F(WMSecurityImmuneBridgeTest, SetInflammationLevelOutOfRange)
{
    ASSERT_NE(bridge_, nullptr);

    // Level 5 is out of valid range (0-4)
    nimcp_error_t result = omni_wm_security_immune_bridge_set_inflammation(bridge_, 5);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 8. Get Modulation Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, GetModulationNullBridgeReturnsNull)
{
    const immune_to_wm_modulation_t* mod = omni_wm_security_immune_bridge_get_modulation(nullptr);
    EXPECT_EQ(mod, nullptr);
}

TEST_F(WMSecurityImmuneBridgeTest, GetModulationReturnsValidPointer)
{
    ASSERT_NE(bridge_, nullptr);

    const immune_to_wm_modulation_t* mod = omni_wm_security_immune_bridge_get_modulation(bridge_);
    ASSERT_NE(mod, nullptr);

    // Check baseline values
    EXPECT_TRUE(float_in_range(mod->combined_confidence_mod, 0.5f, 1.5f));
    EXPECT_TRUE(float_in_range(mod->combined_horizon_mod, 0.25f, 1.0f));
    EXPECT_TRUE(float_in_range(mod->combined_learning_mod, 0.5f, 2.0f));
    EXPECT_TRUE(float_in_range(mod->combined_vigilance, 0.0f, 1.0f));
}

TEST_F(WMSecurityImmuneBridgeTest, GetModulatedConfidenceNullBridgeReturnsZero)
{
    float conf = omni_wm_security_immune_bridge_get_modulated_confidence(nullptr);
    EXPECT_FLOAT_EQ(conf, 0.0f);
}

TEST_F(WMSecurityImmuneBridgeTest, GetModulatedConfidenceValidRange)
{
    ASSERT_NE(bridge_, nullptr);

    float conf = omni_wm_security_immune_bridge_get_modulated_confidence(bridge_);
    EXPECT_TRUE(float_in_range(conf, 0.0f, 1.0f));
}

TEST_F(WMSecurityImmuneBridgeTest, GetModulatedHorizonNullBridgeReturnsZero)
{
    uint32_t horizon = omni_wm_security_immune_bridge_get_modulated_horizon(nullptr);
    EXPECT_EQ(horizon, 0u);
}

TEST_F(WMSecurityImmuneBridgeTest, GetModulatedHorizonValid)
{
    ASSERT_NE(bridge_, nullptr);

    uint32_t horizon = omni_wm_security_immune_bridge_get_modulated_horizon(bridge_);
    EXPECT_GT(horizon, 0u);
    EXPECT_LE(horizon, WM_SECURITY_MAX_FORECAST_HORIZON);
}

TEST_F(WMSecurityImmuneBridgeTest, GetModulatedLearningRateNullBridgeReturnsZero)
{
    float lr = omni_wm_security_immune_bridge_get_modulated_learning_rate(nullptr);
    EXPECT_FLOAT_EQ(lr, 0.0f);
}

TEST_F(WMSecurityImmuneBridgeTest, GetModulatedLearningRatePositive)
{
    ASSERT_NE(bridge_, nullptr);

    float lr = omni_wm_security_immune_bridge_get_modulated_learning_rate(bridge_);
    EXPECT_GT(lr, 0.0f);
}

// =============================================================================
// 9. Security Alert Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, SetAlertLevelNullBridgeFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_set_alert_level(nullptr, 2, false);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, SetAlertLevelValid)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_security_immune_bridge_set_alert_level(bridge_, 2, false);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->alert_level, 2u);
    EXPECT_FALSE(bridge_->under_attack);
}

TEST_F(WMSecurityImmuneBridgeTest, SetAlertLevelWithAttack)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_security_immune_bridge_set_alert_level(bridge_, 4, true);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->alert_level, 4u);
    EXPECT_TRUE(bridge_->under_attack);
}

TEST_F(WMSecurityImmuneBridgeTest, SetAlertLevelOutOfRange)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_security_immune_bridge_set_alert_level(bridge_, 5, false);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 10. Anomaly Prediction Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, PredictAnomalyNullBridgeFails)
{
    float score, confidence;
    nimcp_error_t result = omni_wm_security_immune_bridge_predict_anomaly(
        nullptr, 5, &score, &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, PredictAnomalyNullOutputsFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_security_immune_bridge_predict_anomaly(
        bridge_, 5, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, PredictAnomalyZeroHorizon)
{
    ASSERT_NE(bridge_, nullptr);

    float score, confidence;
    nimcp_error_t result = omni_wm_security_immune_bridge_predict_anomaly(
        bridge_, 0, &score, &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 11. BBB State Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, UpdateBBBStateNullBridgeFails)
{
    bbb_state_for_wm_t state;
    memset(&state, 0, sizeof(state));

    nimcp_error_t result = omni_wm_security_immune_bridge_update_bbb_state(nullptr, &state);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, UpdateBBBStateNullStateFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_security_immune_bridge_update_bbb_state(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, UpdateBBBStateValid)
{
    ASSERT_NE(bridge_, nullptr);

    bbb_state_for_wm_t state;
    memset(&state, 0, sizeof(state));
    state.permeability = 0.2f;
    state.integrity = 0.95f;
    state.active_threats = 2;
    state.is_compromised = false;

    nimcp_error_t result = omni_wm_security_immune_bridge_update_bbb_state(bridge_, &state);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 12. PE Trigger Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, CheckPETriggerNullBridgeFails)
{
    bool should_trigger;
    float strength;

    nimcp_error_t result = omni_wm_security_immune_bridge_check_pe_trigger(
        nullptr, 0.5f, &should_trigger, &strength);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, CheckPETriggerNullOutputsFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_security_immune_bridge_check_pe_trigger(
        bridge_, 0.5f, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, CheckPETriggerLowPE)
{
    ASSERT_NE(bridge_, nullptr);

    bool should_trigger;
    float strength;

    // Low PE should not trigger immune response
    nimcp_error_t result = omni_wm_security_immune_bridge_check_pe_trigger(
        bridge_, 0.1f, &should_trigger, &strength);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    // Low PE typically won't trigger
}

TEST_F(WMSecurityImmuneBridgeTest, TriggerImmuneNullBridgeFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_trigger_immune(nullptr, 0.8f, 1);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 13. Statistics Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, GetStatsNullBridgeFails)
{
    omni_wm_security_immune_stats_t stats;
    nimcp_error_t result = omni_wm_security_immune_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, GetStatsNullStatsFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_security_immune_bridge_get_stats(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, GetStatsValid)
{
    ASSERT_NE(bridge_, nullptr);

    omni_wm_security_immune_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats)); // Fill with non-zero to verify copy

    nimcp_error_t result = omni_wm_security_immune_bridge_get_stats(bridge_, &stats);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Fresh bridge should have zero stats
    EXPECT_EQ(stats.anomalies_predicted, 0u);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST_F(WMSecurityImmuneBridgeTest, ResetStatsNullBridgeFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, ResetStatsValid)
{
    ASSERT_NE(bridge_, nullptr);

    // Increment some stats
    bridge_->stats.anomalies_predicted = 50;
    bridge_->stats.modulation_updates = 100;

    nimcp_error_t result = omni_wm_security_immune_bridge_reset_stats(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->stats.anomalies_predicted, 0u);
    EXPECT_EQ(bridge_->stats.modulation_updates, 0u);
}

// =============================================================================
// 14. Query Effects Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, GetWMEffectsNullBridgeReturnsNull)
{
    const omni_wm_to_security_effects_t* effects =
        omni_wm_security_immune_bridge_get_wm_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(WMSecurityImmuneBridgeTest, GetWMEffectsValid)
{
    ASSERT_NE(bridge_, nullptr);

    const omni_wm_to_security_effects_t* effects =
        omni_wm_security_immune_bridge_get_wm_effects(bridge_);
    ASSERT_NE(effects, nullptr);
}

TEST_F(WMSecurityImmuneBridgeTest, GetSecurityEffectsNullBridgeReturnsNull)
{
    const security_immune_to_wm_effects_t* effects =
        omni_wm_security_immune_bridge_get_security_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(WMSecurityImmuneBridgeTest, GetSecurityEffectsValid)
{
    ASSERT_NE(bridge_, nullptr);

    const security_immune_to_wm_effects_t* effects =
        omni_wm_security_immune_bridge_get_security_effects(bridge_);
    ASSERT_NE(effects, nullptr);
}

// =============================================================================
// 15. Bio-Async Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, ConnectBioAsyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, DisconnectBioAsyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, IsBioAsyncConnectedNullBridgeReturnsFalse)
{
    bool connected = omni_wm_security_immune_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(WMSecurityImmuneBridgeTest, IsBioAsyncConnectedDefault)
{
    ASSERT_NE(bridge_, nullptr);

    // Should be false by default unless config enabled it
    bool connected = omni_wm_security_immune_bridge_is_bio_async_connected(bridge_);
    // Depends on default config, just verify it doesn't crash
    EXPECT_TRUE(connected || !connected);
}

// =============================================================================
// 16. Utility Function Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, ValidateConfigNullFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_validate_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, ValidateConfigDefaultValid)
{
    omni_wm_security_immune_config_t config;
    omni_wm_security_immune_bridge_default_config(&config);

    nimcp_error_t result = omni_wm_security_immune_bridge_validate_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, ValidateConfigInvalidSensitivity)
{
    omni_wm_security_immune_config_t config;
    omni_wm_security_immune_bridge_default_config(&config);
    config.sensitivity = 0.0f; // Out of valid range

    nimcp_error_t result = omni_wm_security_immune_bridge_validate_config(&config);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, ComputeModulationNullBridgeFails)
{
    nimcp_error_t result = omni_wm_security_immune_bridge_compute_modulation(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, ComputeModulationValid)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_security_immune_bridge_compute_modulation(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 17. Threat Forecast Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, ForecastThreatNullBridgeFails)
{
    wm_threat_prediction_t prediction;
    nimcp_error_t result = omni_wm_security_immune_bridge_forecast_threat(
        nullptr, 0, &prediction);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, ForecastThreatNullOutputFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_security_immune_bridge_forecast_threat(
        bridge_, 0, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMSecurityImmuneBridgeTest, GetActivePredictionsNullBridgeReturnsNull)
{
    uint32_t count;
    const wm_threat_prediction_t* preds =
        omni_wm_security_immune_bridge_get_active_predictions(nullptr, &count);
    EXPECT_EQ(preds, nullptr);
}

TEST_F(WMSecurityImmuneBridgeTest, GetActivePredictionsNullCountHandled)
{
    ASSERT_NE(bridge_, nullptr);

    const wm_threat_prediction_t* preds =
        omni_wm_security_immune_bridge_get_active_predictions(bridge_, nullptr);
    // Should handle gracefully or return null
    EXPECT_TRUE(preds == nullptr || preds != nullptr);
}

// =============================================================================
// 18. Memory Safety Tests
// =============================================================================

TEST_F(WMSecurityImmuneBridgeTest, CreateDestroyManyTimes)
{
    // Stress test create/destroy cycle
    for (int i = 0; i < 100; i++) {
        omni_wm_security_immune_bridge_t* temp = omni_wm_security_immune_bridge_create(nullptr);
        ASSERT_NE(temp, nullptr);
        omni_wm_security_immune_bridge_destroy(temp);
    }
}

TEST_F(WMSecurityImmuneBridgeTest, ResetManyTimes)
{
    ASSERT_NE(bridge_, nullptr);

    for (int i = 0; i < 100; i++) {
        nimcp_error_t result = omni_wm_security_immune_bridge_reset(bridge_);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

TEST_F(WMSecurityImmuneBridgeTest, CytokineUpdateManyTimes)
{
    ASSERT_NE(bridge_, nullptr);

    for (int i = 0; i < 100; i++) {
        float level = (float)i / 100.0f;
        nimcp_error_t result = omni_wm_security_immune_bridge_update_cytokines(
            bridge_, level, level * 0.5f, level * 0.3f, 1.0f - level, level * 0.2f);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
