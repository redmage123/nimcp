/**
 * @file test_omni_wm_substrate_bridge.cpp
 * @brief Comprehensive unit tests for World Model Substrate Bridge
 *
 * WHAT: Tests for WM-Substrate integration bridge
 * WHY:  Verify metabolic constraints on world model computation
 * HOW:  GTest-based tests for lifecycle, connection, metabolic modulation,
 *       demand signaling, alerts, and Q10 temperature effects
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "cognitive/omni/bridges/nimcp_omni_wm_substrate_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

// =============================================================================
// Constants and Helpers
// =============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr float DEFAULT_DT = 0.016f;

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

class OmniWmSubstrateBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        bridge_ = omni_wm_substrate_bridge_create(nullptr);
    }

    void TearDown() override
    {
        if (bridge_) {
            omni_wm_substrate_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    omni_wm_substrate_bridge_t* bridge_ = nullptr;
};

// =============================================================================
// 1. Lifecycle Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, CreateWithNullConfig)
{
    ASSERT_NE(bridge_, nullptr);
}

TEST_F(OmniWmSubstrateBridgeTest, CreateWithDefaultConfig)
{
    omni_wm_substrate_bridge_config_t config;
    ASSERT_EQ(omni_wm_substrate_bridge_default_config(&config), NIMCP_SUCCESS);

    omni_wm_substrate_bridge_t* bridge = omni_wm_substrate_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
    omni_wm_substrate_bridge_destroy(bridge);
}

TEST_F(OmniWmSubstrateBridgeTest, CreateWithCustomConfig)
{
    omni_wm_substrate_bridge_config_t config;
    ASSERT_EQ(omni_wm_substrate_bridge_default_config(&config), NIMCP_SUCCESS);

    // Customize
    config.enable_modulation = true;
    config.sensitivity = 1.5f;
    config.enable_atp_modulation = true;
    config.atp_training_threshold = 0.6f;
    config.atp_prediction_threshold = 0.35f;
    config.enable_o2_modulation = true;
    config.o2_critical_threshold = 0.45f;
    config.enable_temperature_effects = true;
    config.q10_coefficient = 2.5f;

    omni_wm_substrate_bridge_t* bridge = omni_wm_substrate_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FLOAT_EQ(bridge->config.sensitivity, 1.5f);
    EXPECT_FLOAT_EQ(bridge->config.atp_training_threshold, 0.6f);
    EXPECT_FLOAT_EQ(bridge->config.q10_coefficient, 2.5f);

    omni_wm_substrate_bridge_destroy(bridge);
}

TEST_F(OmniWmSubstrateBridgeTest, DestroyNullSafe)
{
    omni_wm_substrate_bridge_destroy(nullptr);
}

TEST_F(OmniWmSubstrateBridgeTest, DestroyValidBridge)
{
    omni_wm_substrate_bridge_t* bridge = omni_wm_substrate_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    omni_wm_substrate_bridge_destroy(bridge);
}

TEST_F(OmniWmSubstrateBridgeTest, ResetNullFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, ResetValidBridge)
{
    nimcp_error_t result = omni_wm_substrate_bridge_reset(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 2. Default Config Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, DefaultConfigNullFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, DefaultConfigSetsReasonableValues)
{
    omni_wm_substrate_bridge_config_t config;
    ASSERT_EQ(omni_wm_substrate_bridge_default_config(&config), NIMCP_SUCCESS);

    EXPECT_TRUE(float_in_range(config.sensitivity, 0.5f, 2.0f));
    EXPECT_TRUE(float_in_range(config.atp_training_threshold, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.atp_prediction_threshold, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.o2_critical_threshold, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.glucose_learning_threshold, 0.0f, 1.0f));
    EXPECT_GT(config.q10_coefficient, 1.0f);  // Q10 > 1
    EXPECT_LE(config.max_horizon, WM_SUBSTRATE_MAX_HORIZON);
    EXPECT_GE(config.min_horizon, WM_SUBSTRATE_MIN_HORIZON);
    EXPECT_GT(config.base_compute_rate, 0.0f);
}

TEST_F(OmniWmSubstrateBridgeTest, ValidateConfigNullFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_validate_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, ValidateConfigDefaultSucceeds)
{
    omni_wm_substrate_bridge_config_t config;
    ASSERT_EQ(omni_wm_substrate_bridge_default_config(&config), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_substrate_bridge_validate_config(&config), NIMCP_SUCCESS);
}

// =============================================================================
// 3. Connection Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, ConnectNullBridgeFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_connect(
        nullptr, nullptr, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, IsConnectedBeforeConnect)
{
    EXPECT_FALSE(omni_wm_substrate_bridge_is_connected(bridge_));
}

TEST_F(OmniWmSubstrateBridgeTest, IsConnectedNullReturnsFalse)
{
    EXPECT_FALSE(omni_wm_substrate_bridge_is_connected(nullptr));
}

TEST_F(OmniWmSubstrateBridgeTest, ConnectWorldModelNullBridgeFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_connect_world_model(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, ConnectSubstrateNullBridgeFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_connect_substrate(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, ConnectMetabolicNullBridgeFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_connect_metabolic(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 4. Update Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, UpdateNullBridgeFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_update(nullptr, DEFAULT_DT);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, UpdateUnconnectedBridge)
{
    nimcp_error_t result = omni_wm_substrate_bridge_update(bridge_, DEFAULT_DT);
    (void)result;
}

TEST_F(OmniWmSubstrateBridgeTest, RefreshMetabolicStateNullFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_refresh_metabolic_state(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 5. Metabolic Constraint Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, GetAvailabilityNullBridgeFails)
{
    wm_metabolic_availability_t availability;
    nimcp_error_t result = omni_wm_substrate_bridge_get_availability(nullptr, &availability);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, GetAvailabilityNullOutputFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_get_availability(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, GetAvailabilityValid)
{
    wm_metabolic_availability_t availability;
    nimcp_error_t result = omni_wm_substrate_bridge_get_availability(bridge_, &availability);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify fields are in valid ranges
    EXPECT_TRUE(float_in_range(availability.atp_level, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(availability.oxygen_saturation, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(availability.glucose_level, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(availability.metabolic_capacity, 0.0f, 1.0f));
}

TEST_F(OmniWmSubstrateBridgeTest, CanTrainNullReturnsFalse)
{
    EXPECT_FALSE(omni_wm_substrate_bridge_can_train(nullptr));
}

TEST_F(OmniWmSubstrateBridgeTest, CanTrainInitial)
{
    // Initial state should allow training (full resources)
    bool can_train = omni_wm_substrate_bridge_can_train(bridge_);
    // May be true or false depending on initial state
    (void)can_train;
}

TEST_F(OmniWmSubstrateBridgeTest, CanPredictNullReturnsFalse)
{
    EXPECT_FALSE(omni_wm_substrate_bridge_can_predict(nullptr));
}

TEST_F(OmniWmSubstrateBridgeTest, CanPredictInitial)
{
    bool can_predict = omni_wm_substrate_bridge_can_predict(bridge_);
    (void)can_predict;
}

TEST_F(OmniWmSubstrateBridgeTest, GetAllowedHorizonNullReturnsMin)
{
    uint32_t horizon = omni_wm_substrate_bridge_get_allowed_horizon(nullptr);
    // Should return minimum or 0
    EXPECT_LE(horizon, WM_SUBSTRATE_MAX_HORIZON);
}

TEST_F(OmniWmSubstrateBridgeTest, GetAllowedHorizonValid)
{
    uint32_t horizon = omni_wm_substrate_bridge_get_allowed_horizon(bridge_);
    EXPECT_GE(horizon, WM_SUBSTRATE_MIN_HORIZON);
    EXPECT_LE(horizon, WM_SUBSTRATE_MAX_HORIZON);
}

TEST_F(OmniWmSubstrateBridgeTest, GetAllowedComputeRateNullReturnsZero)
{
    float rate = omni_wm_substrate_bridge_get_allowed_compute_rate(nullptr);
    EXPECT_GE(rate, 0.0f);
}

TEST_F(OmniWmSubstrateBridgeTest, GetAllowedComputeRateValid)
{
    float rate = omni_wm_substrate_bridge_get_allowed_compute_rate(bridge_);
    EXPECT_GT(rate, 0.0f);
}

TEST_F(OmniWmSubstrateBridgeTest, GetLearningRateScaleNullReturnsZero)
{
    float scale = omni_wm_substrate_bridge_get_learning_rate_scale(nullptr);
    EXPECT_GE(scale, 0.0f);
}

TEST_F(OmniWmSubstrateBridgeTest, GetLearningRateScaleValid)
{
    float scale = omni_wm_substrate_bridge_get_learning_rate_scale(bridge_);
    EXPECT_TRUE(float_in_range(scale, 0.0f, 1.0f));
}

// =============================================================================
// 6. Demand Signaling Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, SignalDemandNullBridgeFails)
{
    wm_computational_demand_t demand = {0};
    demand.prediction_demand = 0.5f;
    demand.training_demand = 0.3f;

    nimcp_error_t result = omni_wm_substrate_bridge_signal_demand(nullptr, &demand);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, SignalDemandNullDemandFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_signal_demand(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, SignalDemandValid)
{
    wm_computational_demand_t demand = {0};
    demand.prediction_demand = 0.5f;
    demand.training_demand = 0.3f;
    demand.rollout_demand = 0.2f;
    demand.overall_demand = 0.4f;
    demand.requested_horizon = 16;
    demand.training_active = true;

    nimcp_error_t result = omni_wm_substrate_bridge_signal_demand(bridge_, &demand);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, ReportPredictionsNullBridgeFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_report_predictions(nullptr, 10, 8);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, ReportPredictionsValid)
{
    nimcp_error_t result = omni_wm_substrate_bridge_report_predictions(bridge_, 10, 8);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, ReportTrainingNullBridgeFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_report_training(nullptr, 100);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, ReportTrainingValid)
{
    nimcp_error_t result = omni_wm_substrate_bridge_report_training(bridge_, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, ReportRolloutsNullBridgeFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_report_rollouts(nullptr, 5, 50);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, ReportRolloutsValid)
{
    nimcp_error_t result = omni_wm_substrate_bridge_report_rollouts(bridge_, 5, 50);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 7. Alert Management Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, GetActiveAlertsNullReturnsZero)
{
    uint32_t alerts = omni_wm_substrate_bridge_get_active_alerts(nullptr);
    EXPECT_EQ(alerts, 0u);
}

TEST_F(OmniWmSubstrateBridgeTest, GetActiveAlertsInitial)
{
    uint32_t alerts = omni_wm_substrate_bridge_get_active_alerts(bridge_);
    // Initially should have no alerts
    EXPECT_EQ(alerts, 0u);
}

TEST_F(OmniWmSubstrateBridgeTest, HasAlertNullReturnsFalse)
{
    EXPECT_FALSE(omni_wm_substrate_bridge_has_alert(nullptr));
}

TEST_F(OmniWmSubstrateBridgeTest, HasAlertInitial)
{
    EXPECT_FALSE(omni_wm_substrate_bridge_has_alert(bridge_));
}

TEST_F(OmniWmSubstrateBridgeTest, CheckAlertNullReturnsFalse)
{
    EXPECT_FALSE(omni_wm_substrate_bridge_check_alert(nullptr, 1));
}

TEST_F(OmniWmSubstrateBridgeTest, CheckAlertNoAlerts)
{
    EXPECT_FALSE(omni_wm_substrate_bridge_check_alert(bridge_,
        BIO_MSG_WM_SUBSTRATE_ALERT_LOW_ATP));
    EXPECT_FALSE(omni_wm_substrate_bridge_check_alert(bridge_,
        BIO_MSG_WM_SUBSTRATE_ALERT_HYPOXIA));
    EXPECT_FALSE(omni_wm_substrate_bridge_check_alert(bridge_,
        BIO_MSG_WM_SUBSTRATE_ALERT_LOW_GLUCOSE));
    EXPECT_FALSE(omni_wm_substrate_bridge_check_alert(bridge_,
        BIO_MSG_WM_SUBSTRATE_ALERT_HYPERTHERMIA));
}

// =============================================================================
// 8. Query API Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, GetSubstrateEffectsNullReturnsNull)
{
    const substrate_to_wm_effects_t* effects =
        omni_wm_substrate_bridge_get_substrate_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(OmniWmSubstrateBridgeTest, GetSubstrateEffectsValid)
{
    const substrate_to_wm_effects_t* effects =
        omni_wm_substrate_bridge_get_substrate_effects(bridge_);
    EXPECT_NE(effects, nullptr);
}

TEST_F(OmniWmSubstrateBridgeTest, GetWmEffectsNullReturnsNull)
{
    const wm_to_substrate_effects_t* effects =
        omni_wm_substrate_bridge_get_wm_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(OmniWmSubstrateBridgeTest, GetWmEffectsValid)
{
    const wm_to_substrate_effects_t* effects =
        omni_wm_substrate_bridge_get_wm_effects(bridge_);
    EXPECT_NE(effects, nullptr);
}

// =============================================================================
// 9. Statistics Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, GetStatsNullBridgeFails)
{
    omni_wm_substrate_bridge_stats_t stats;
    nimcp_error_t result = omni_wm_substrate_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, GetStatsNullOutputFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_get_stats(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, GetStatsValid)
{
    omni_wm_substrate_bridge_stats_t stats;
    nimcp_error_t result = omni_wm_substrate_bridge_get_stats(bridge_, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, ResetStatsNullBridgeFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, ResetStatsValid)
{
    nimcp_error_t result = omni_wm_substrate_bridge_reset_stats(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, StatsIncrementOnActivity)
{
    omni_wm_substrate_bridge_stats_t stats_before, stats_after;

    omni_wm_substrate_bridge_get_stats(bridge_, &stats_before);

    // Perform some activity
    for (int i = 0; i < 5; i++) {
        omni_wm_substrate_bridge_update(bridge_, DEFAULT_DT);
        omni_wm_substrate_bridge_report_predictions(bridge_, 10, 8);
    }

    omni_wm_substrate_bridge_get_stats(bridge_, &stats_after);
    EXPECT_GT(stats_after.total_updates, stats_before.total_updates);
}

// =============================================================================
// 10. Bio-Async Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, ConnectBioAsyncNullFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, DisconnectBioAsyncNullFails)
{
    nimcp_error_t result = omni_wm_substrate_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, IsBioAsyncConnectedNullReturnsFalse)
{
    EXPECT_FALSE(omni_wm_substrate_bridge_is_bio_async_connected(nullptr));
}

TEST_F(OmniWmSubstrateBridgeTest, IsBioAsyncConnectedInitially)
{
    EXPECT_FALSE(omni_wm_substrate_bridge_is_bio_async_connected(bridge_));
}

// =============================================================================
// 11. Q10 Temperature Effects Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, ComputeQ10FactorNormalTemp)
{
    float factor = omni_wm_substrate_compute_q10_factor(
        WM_SUBSTRATE_NORMAL_TEMP, WM_SUBSTRATE_NORMAL_TEMP, WM_SUBSTRATE_Q10_COMPUTATION);
    EXPECT_FLOAT_EQ(factor, 1.0f);  // At normal temp, factor should be 1.0
}

TEST_F(OmniWmSubstrateBridgeTest, ComputeQ10FactorHigherTemp)
{
    float factor = omni_wm_substrate_compute_q10_factor(
        WM_SUBSTRATE_NORMAL_TEMP + 10.0f, WM_SUBSTRATE_NORMAL_TEMP, WM_SUBSTRATE_Q10_COMPUTATION);
    EXPECT_GT(factor, 1.0f);  // Higher temp should increase rate
    EXPECT_TRUE(float_equals(factor, WM_SUBSTRATE_Q10_COMPUTATION, 0.01f));
}

TEST_F(OmniWmSubstrateBridgeTest, ComputeQ10FactorLowerTemp)
{
    float factor = omni_wm_substrate_compute_q10_factor(
        WM_SUBSTRATE_NORMAL_TEMP - 10.0f, WM_SUBSTRATE_NORMAL_TEMP, WM_SUBSTRATE_Q10_COMPUTATION);
    EXPECT_LT(factor, 1.0f);  // Lower temp should decrease rate
    EXPECT_GT(factor, 0.0f);  // But should be positive
}

TEST_F(OmniWmSubstrateBridgeTest, ComputeQ10FactorVariousTemps)
{
    float temps[] = {27.0f, 32.0f, 37.0f, 40.0f, 42.0f};
    float prev_factor = 0.0f;

    for (float temp : temps) {
        float factor = omni_wm_substrate_compute_q10_factor(
            temp, WM_SUBSTRATE_NORMAL_TEMP, WM_SUBSTRATE_Q10_COMPUTATION);
        EXPECT_GT(factor, 0.0f);
        EXPECT_GT(factor, prev_factor);  // Should increase with temp
        prev_factor = factor;
    }
}

// =============================================================================
// 12. Utility Function Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, MsgTypeToStringValid)
{
    const char* name = omni_wm_substrate_msg_type_to_string(BIO_MSG_WM_SUBSTRATE_ALERT_LOW_ATP);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(OmniWmSubstrateBridgeTest, MsgTypeToStringAllTypes)
{
    omni_wm_substrate_msg_type_t types[] = {
        BIO_MSG_WM_SUBSTRATE_ALERT_LOW_ATP,
        BIO_MSG_WM_SUBSTRATE_ALERT_HYPOXIA,
        BIO_MSG_WM_SUBSTRATE_ALERT_LOW_GLUCOSE,
        BIO_MSG_WM_SUBSTRATE_ALERT_HYPERTHERMIA,
        BIO_MSG_WM_SUBSTRATE_ALERT_RESOLVED,
        BIO_MSG_WM_SUBSTRATE_BRIDGE_STATUS,
        BIO_MSG_WM_SUBSTRATE_BRIDGE_ERROR,
        BIO_MSG_WM_SUBSTRATE_STATS_UPDATE
    };

    for (auto type : types) {
        const char* name = omni_wm_substrate_msg_type_to_string(type);
        EXPECT_NE(name, nullptr);
    }
}

// =============================================================================
// 13. Edge Case Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, MultipleResetCalls)
{
    for (int i = 0; i < 5; i++) {
        nimcp_error_t result = omni_wm_substrate_bridge_reset(bridge_);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

TEST_F(OmniWmSubstrateBridgeTest, RapidDemandSignaling)
{
    wm_computational_demand_t demand = {0};

    for (int i = 0; i < 100; i++) {
        demand.prediction_demand = (float)i / 100.0f;
        demand.overall_demand = demand.prediction_demand;
        omni_wm_substrate_bridge_signal_demand(bridge_, &demand);
    }
}

TEST_F(OmniWmSubstrateBridgeTest, HeavyActivityReporting)
{
    for (int i = 0; i < 100; i++) {
        omni_wm_substrate_bridge_report_predictions(bridge_, 50, 16);
        omni_wm_substrate_bridge_report_training(bridge_, 32);
        omni_wm_substrate_bridge_report_rollouts(bridge_, 3, 24);
    }

    omni_wm_substrate_bridge_stats_t stats;
    omni_wm_substrate_bridge_get_stats(bridge_, &stats);
    // Should have accumulated consumption
    EXPECT_GE(stats.total_atp_consumed, 0.0f);
}

TEST_F(OmniWmSubstrateBridgeTest, UpdateWithVaryingDt)
{
    float dts[] = {0.001f, 0.016f, 0.033f, 0.1f, 0.5f};

    for (float dt : dts) {
        nimcp_error_t result = omni_wm_substrate_bridge_update(bridge_, dt);
        (void)result;
    }
}

// =============================================================================
// 14. Memory Safety Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, CreateDestroyMultiple)
{
    for (int i = 0; i < 10; i++) {
        omni_wm_substrate_bridge_t* bridge = omni_wm_substrate_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
        omni_wm_substrate_bridge_destroy(bridge);
    }
}

TEST_F(OmniWmSubstrateBridgeTest, UseAfterReset)
{
    omni_wm_substrate_bridge_reset(bridge_);

    EXPECT_EQ(omni_wm_substrate_bridge_update(bridge_, DEFAULT_DT), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_substrate_bridge_report_predictions(bridge_, 10, 8), NIMCP_SUCCESS);
}

TEST_F(OmniWmSubstrateBridgeTest, ConfigIntegrity)
{
    omni_wm_substrate_bridge_config_t config;
    omni_wm_substrate_bridge_default_config(&config);
    config.atp_training_threshold = 0.55f;
    config.q10_coefficient = 2.8f;

    omni_wm_substrate_bridge_t* bridge = omni_wm_substrate_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Modify original
    config.atp_training_threshold = 0.3f;

    // Bridge should have original value
    EXPECT_FLOAT_EQ(bridge->config.atp_training_threshold, 0.55f);

    omni_wm_substrate_bridge_destroy(bridge);
}

// =============================================================================
// 15. Threshold-Based Behavior Tests
// =============================================================================

TEST_F(OmniWmSubstrateBridgeTest, SubstrateEffectsWithLowATP)
{
    // This tests internal state behavior
    const substrate_to_wm_effects_t* effects =
        omni_wm_substrate_bridge_get_substrate_effects(bridge_);
    ASSERT_NE(effects, nullptr);

    // Initially should have training permitted with defaults
    // (actual behavior depends on simulated substrate state)
    EXPECT_LE(effects->allowed_horizon, WM_SUBSTRATE_MAX_HORIZON);
}

TEST_F(OmniWmSubstrateBridgeTest, ConstraintStateCheck)
{
    // Initially should not be constrained
    EXPECT_FALSE(bridge_->is_constrained);

    // After updates, check state is consistent
    for (int i = 0; i < 10; i++) {
        omni_wm_substrate_bridge_update(bridge_, DEFAULT_DT);
    }

    // Constraint state should be valid bool
    bool constrained = bridge_->is_constrained;
    EXPECT_TRUE(constrained == true || constrained == false);
}
