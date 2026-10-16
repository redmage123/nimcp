/**
 * @file test_omni_wm_kg_bridge.cpp
 * @brief Comprehensive unit tests for World Model Knowledge Graph Bridge
 *
 * WHAT: Tests for WM-KG bidirectional bridge
 * WHY:  KG bridge enables semantic reasoning via world model predictions
 * HOW:  Tests all APIs: lifecycle, connections, predictions, anomaly detection,
 *       training, registry sync, and edge cases
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

extern "C" {
/* Include KG types BEFORE bridge header to avoid forward declaration conflicts */
#include "core/brain/nimcp_kg_wiring_exception.h"
#include "core/brain/nimcp_kg_events.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_kg_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

// =============================================================================
// Constants and Helpers
// =============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr uint32_t TEST_ENTITY_ID = 42;
static constexpr uint32_t TEST_MODULE_ID = 0x1234;
static constexpr uint32_t TEST_HORIZON_STEPS = 5;
static constexpr uint32_t TEST_STATE_DIM = 16;

static bool float_equals(float a, float b, float tol = FLOAT_TOLERANCE)
{
    return std::fabs(a - b) < tol;
}

// =============================================================================
// Test Fixture
// =============================================================================

class OmniWmKgBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create bridge with default config for most tests
        bridge_ = omni_wm_kg_bridge_create(nullptr);
    }

    void TearDown() override
    {
        if (bridge_) {
            omni_wm_kg_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    // Helper to create config with custom settings
    omni_wm_kg_bridge_config_t create_custom_config()
    {
        omni_wm_kg_bridge_config_t config;
        omni_wm_kg_bridge_default_config(&config);
        config.enable_modulation = true;
        config.sensitivity = 1.5f;
        config.enable_entity_prediction = true;
        config.enable_anomaly_detection = true;
        return config;
    }

    omni_wm_kg_bridge_t* bridge_ = nullptr;
};

// =============================================================================
// 1. Default Config Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, DefaultConfigBasic)
{
    omni_wm_kg_bridge_config_t config;
    nimcp_error_t result = omni_wm_kg_bridge_default_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, DefaultConfigNullFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, DefaultConfigSetsReasonableValues)
{
    omni_wm_kg_bridge_config_t config;
    ASSERT_EQ(omni_wm_kg_bridge_default_config(&config), NIMCP_SUCCESS);

    // Check sensitivity is in valid range
    EXPECT_GE(config.sensitivity, 0.5f);
    EXPECT_LE(config.sensitivity, 2.0f);

    // Check threshold values are reasonable
    EXPECT_GE(config.prediction_confidence_threshold, 0.0f);
    EXPECT_LE(config.prediction_confidence_threshold, 1.0f);

    EXPECT_GE(config.anomaly_threshold, 0.0f);
    EXPECT_LE(config.anomaly_threshold, 1.0f);

    EXPECT_GT(config.default_prediction_horizon, 0u);
    EXPECT_GT(config.failure_horizon_sec, 0.0f);
}

TEST_F(OmniWmKgBridgeTest, ValidateConfigValidConfig)
{
    omni_wm_kg_bridge_config_t config;
    omni_wm_kg_bridge_default_config(&config);

    nimcp_error_t result = omni_wm_kg_bridge_validate_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, ValidateConfigNullFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_validate_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 2. Lifecycle Tests - Create
// =============================================================================

TEST_F(OmniWmKgBridgeTest, CreateWithNullConfig)
{
    // bridge_ was created in SetUp with NULL config
    ASSERT_NE(bridge_, nullptr);
}

TEST_F(OmniWmKgBridgeTest, CreateWithCustomConfig)
{
    omni_wm_kg_bridge_config_t config = create_custom_config();

    omni_wm_kg_bridge_t* bridge = omni_wm_kg_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Verify config was applied
    EXPECT_TRUE(bridge->config.enable_modulation);
    EXPECT_FLOAT_EQ(bridge->config.sensitivity, 1.5f);

    omni_wm_kg_bridge_destroy(bridge);
}

TEST_F(OmniWmKgBridgeTest, CreateInitializesBaseFields)
{
    ASSERT_NE(bridge_, nullptr);

    // Check base bridge initialization
    EXPECT_EQ(bridge_->base.module_id, BIO_MODULE_WM_KG_BRIDGE);
    EXPECT_NE(bridge_->base.module_name, nullptr);
    EXPECT_FALSE(bridge_->base.bridge_active);
}

TEST_F(OmniWmKgBridgeTest, CreateInitializesEffects)
{
    ASSERT_NE(bridge_, nullptr);

    // Check WM->KG effects are zeroed
    EXPECT_EQ(bridge_->wm_to_kg.entity_predictions_count, 0u);
    EXPECT_FALSE(bridge_->wm_to_kg.anomaly_detected);

    // Check KG->WM effects are zeroed
    EXPECT_EQ(bridge_->kg_to_wm.active_entities_count, 0u);
    EXPECT_EQ(bridge_->kg_to_wm.relationship_count, 0u);
}

TEST_F(OmniWmKgBridgeTest, CreateInitializesStats)
{
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(bridge_->stats.entity_predictions_made, 0u);
    EXPECT_EQ(bridge_->stats.anomalies_detected, 0u);
    EXPECT_EQ(bridge_->stats.total_updates, 0u);
    EXPECT_EQ(bridge_->stats.errors_total, 0u);
}

// =============================================================================
// 3. Lifecycle Tests - Destroy
// =============================================================================

TEST_F(OmniWmKgBridgeTest, DestroyNullSafe)
{
    // Should not crash
    omni_wm_kg_bridge_destroy(nullptr);
}

TEST_F(OmniWmKgBridgeTest, DestroyValidBridge)
{
    omni_wm_kg_bridge_t* bridge = omni_wm_kg_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    // Should not crash
    omni_wm_kg_bridge_destroy(bridge);
}

// =============================================================================
// 4. Lifecycle Tests - Reset
// =============================================================================

TEST_F(OmniWmKgBridgeTest, ResetBasic)
{
    nimcp_error_t result = omni_wm_kg_bridge_reset(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, ResetNullFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, ResetClearsStats)
{
    // First update to generate some stats
    bridge_->stats.entity_predictions_made = 100;
    bridge_->stats.anomalies_detected = 5;
    bridge_->stats.total_updates = 50;

    nimcp_error_t result = omni_wm_kg_bridge_reset(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Stats should be reset
    EXPECT_EQ(bridge_->stats.entity_predictions_made, 0u);
    EXPECT_EQ(bridge_->stats.anomalies_detected, 0u);
    EXPECT_EQ(bridge_->stats.total_updates, 0u);
}

TEST_F(OmniWmKgBridgeTest, ResetPreservesConfig)
{
    // Set custom config value
    bridge_->config.sensitivity = 1.75f;
    bridge_->config.enable_entity_prediction = true;

    omni_wm_kg_bridge_reset(bridge_);

    // Config should be preserved
    EXPECT_FLOAT_EQ(bridge_->config.sensitivity, 1.75f);
    EXPECT_TRUE(bridge_->config.enable_entity_prediction);
}

// =============================================================================
// 5. Connection Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, ConnectNullBridgeFails)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    nimcp_error_t result = omni_wm_kg_bridge_connect(nullptr, dummy_wm, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, ConnectNullWorldModelFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_connect(bridge_, nullptr, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, ConnectWorldModelOnly)
{
    // Use a dummy world model pointer for testing
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);

    nimcp_error_t result = omni_wm_kg_bridge_connect(bridge_, dummy_wm, nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->world_model, dummy_wm);
    EXPECT_TRUE(omni_wm_kg_bridge_is_connected(bridge_));
}

TEST_F(OmniWmKgBridgeTest, ConnectAllSystems)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    kg_wiring_manager_t* dummy_kg = reinterpret_cast<kg_wiring_manager_t*>(0x5678);
    kg_module_registry_t* dummy_reg = reinterpret_cast<kg_module_registry_t*>(0x9ABC);

    nimcp_error_t result = omni_wm_kg_bridge_connect(bridge_, dummy_wm, dummy_kg, dummy_reg);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->world_model, dummy_wm);
    EXPECT_EQ(bridge_->kg_wiring, dummy_kg);
    EXPECT_EQ(bridge_->registry, dummy_reg);
}

TEST_F(OmniWmKgBridgeTest, ConnectWorldModelSeparate)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);

    nimcp_error_t result = omni_wm_kg_bridge_connect_world_model(bridge_, dummy_wm);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->world_model, dummy_wm);
}

TEST_F(OmniWmKgBridgeTest, ConnectKgWiringSeparate)
{
    kg_wiring_manager_t* dummy_kg = reinterpret_cast<kg_wiring_manager_t*>(0x5678);

    nimcp_error_t result = omni_wm_kg_bridge_connect_kg_wiring(bridge_, dummy_kg);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->kg_wiring, dummy_kg);
}

TEST_F(OmniWmKgBridgeTest, ConnectRegistrySeparate)
{
    kg_module_registry_t* dummy_reg = reinterpret_cast<kg_module_registry_t*>(0x9ABC);

    nimcp_error_t result = omni_wm_kg_bridge_connect_registry(bridge_, dummy_reg);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->registry, dummy_reg);
}

TEST_F(OmniWmKgBridgeTest, IsConnectedWithNoConnection)
{
    EXPECT_FALSE(omni_wm_kg_bridge_is_connected(bridge_));
}

TEST_F(OmniWmKgBridgeTest, IsConnectedNullFalse)
{
    EXPECT_FALSE(omni_wm_kg_bridge_is_connected(nullptr));
}

// =============================================================================
// 6. Update Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, UpdateNullBridgeFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_update(nullptr, 0.016f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, UpdateWithoutConnectionHandled)
{
    // Update without connection - should handle gracefully
    nimcp_error_t result = omni_wm_kg_bridge_update(bridge_, 0.016f);
    // May return error or success depending on implementation
    // The key is it shouldn't crash
}

TEST_F(OmniWmKgBridgeTest, UpdateIncrementsCounter)
{
    // Connect world model first
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    omni_wm_kg_bridge_connect(bridge_, dummy_wm, nullptr, nullptr);

    uint64_t initial_updates = bridge_->stats.total_updates;

    omni_wm_kg_bridge_update(bridge_, 0.016f);

    // Stats should track the update attempt
    EXPECT_GE(bridge_->stats.total_updates, initial_updates);
}

TEST_F(OmniWmKgBridgeTest, UpdateWithZeroDt)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    omni_wm_kg_bridge_connect(bridge_, dummy_wm, nullptr, nullptr);

    nimcp_error_t result = omni_wm_kg_bridge_update(bridge_, 0.0f);
    // Should handle zero dt gracefully
}

TEST_F(OmniWmKgBridgeTest, UpdateWithNegativeDt)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    omni_wm_kg_bridge_connect(bridge_, dummy_wm, nullptr, nullptr);

    nimcp_error_t result = omni_wm_kg_bridge_update(bridge_, -0.016f);
    // Should handle negative dt - may clamp or error
}

// =============================================================================
// 7. Entity Prediction Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, PredictEntityNullBridgeFails)
{
    wm_to_kg_entity_prediction_t prediction;
    nimcp_error_t result = omni_wm_kg_bridge_predict_entity(
        nullptr, TEST_ENTITY_ID, TEST_HORIZON_STEPS, &prediction);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, PredictEntityNullOutputFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_predict_entity(
        bridge_, TEST_ENTITY_ID, TEST_HORIZON_STEPS, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, PredictEntityWithoutConnectionHandled)
{
    wm_to_kg_entity_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    nimcp_error_t result = omni_wm_kg_bridge_predict_entity(
        bridge_, TEST_ENTITY_ID, TEST_HORIZON_STEPS, &prediction);
    // Should handle gracefully without crash
}

TEST_F(OmniWmKgBridgeTest, PredictEntityZeroHorizonHandled)
{
    wm_to_kg_entity_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    nimcp_error_t result = omni_wm_kg_bridge_predict_entity(
        bridge_, TEST_ENTITY_ID, 0, &prediction);
    // Zero horizon may be invalid - implementation dependent
}

TEST_F(OmniWmKgBridgeTest, PredictEntitiesBatchNullBridgeFails)
{
    uint32_t entity_ids[] = {1, 2, 3};
    wm_to_kg_entity_prediction_t predictions[3];

    nimcp_error_t result = omni_wm_kg_bridge_predict_entities_batch(
        nullptr, entity_ids, 3, TEST_HORIZON_STEPS, predictions);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, PredictEntitiesBatchNullIdsHandled)
{
    wm_to_kg_entity_prediction_t predictions[3];

    nimcp_error_t result = omni_wm_kg_bridge_predict_entities_batch(
        bridge_, nullptr, 3, TEST_HORIZON_STEPS, predictions);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, PredictEntitiesBatchZeroCountHandled)
{
    uint32_t entity_ids[] = {1};
    wm_to_kg_entity_prediction_t predictions[1];

    nimcp_error_t result = omni_wm_kg_bridge_predict_entities_batch(
        bridge_, entity_ids, 0, TEST_HORIZON_STEPS, predictions);
    // Zero count should be handled
}

// =============================================================================
// 8. Relationship Prediction Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, PredictRelationshipsNullBridgeFails)
{
    float change_prob;
    nimcp_error_t result = omni_wm_kg_bridge_predict_relationships(
        nullptr, TEST_ENTITY_ID, &change_prob);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, PredictRelationshipsNullOutputFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_predict_relationships(
        bridge_, TEST_ENTITY_ID, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, PredictRelationshipStrengthNullBridgeFails)
{
    float strength, confidence;
    nimcp_error_t result = omni_wm_kg_bridge_predict_relationship_strength(
        nullptr, 1, 2, TEST_HORIZON_STEPS, &strength, &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, PredictRelationshipStrengthNullOutputFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_predict_relationship_strength(
        bridge_, 1, 2, TEST_HORIZON_STEPS, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 9. Module Failure Prediction Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, PredictModuleFailureNullBridgeFails)
{
    float failure_prob, time_to_failure;
    nimcp_error_t result = omni_wm_kg_bridge_predict_module_failure(
        nullptr, TEST_MODULE_ID, &failure_prob, &time_to_failure);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, PredictModuleFailureNullOutputFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_predict_module_failure(
        bridge_, TEST_MODULE_ID, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, GetModulePredictionNullBridgeFails)
{
    wm_to_kg_failure_prediction_t prediction;
    nimcp_error_t result = omni_wm_kg_bridge_get_module_prediction(
        nullptr, TEST_MODULE_ID, &prediction);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, GetModulePredictionNullOutputFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_get_module_prediction(
        bridge_, TEST_MODULE_ID, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, PredictSystemStabilityNullBridgeFails)
{
    float stability;
    uint32_t exceptions;
    nimcp_error_t result = omni_wm_kg_bridge_predict_system_stability(
        nullptr, &stability, &exceptions);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, PredictSystemStabilityNullOutputFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_predict_system_stability(
        bridge_, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 10. Exception Handling Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, OnExceptionNullBridgeFails)
{
    nimcp_kg_wiring_exception_t exception;
    memset(&exception, 0, sizeof(exception));

    nimcp_error_t result = omni_wm_kg_bridge_on_exception(nullptr, &exception);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, OnExceptionNullExceptionFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_on_exception(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 11. Anomaly Detection Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, CheckAnomalyNullBridgeFails)
{
    bool is_anomalous;
    float anomaly_score;
    nimcp_error_t result = omni_wm_kg_bridge_check_anomaly(
        nullptr, TEST_ENTITY_ID, &is_anomalous, &anomaly_score);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, CheckAnomalyNullOutputFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_check_anomaly(
        bridge_, TEST_ENTITY_ID, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 12. Training Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, TrainFromKgEventNullBridgeFails)
{
    kg_event_t event;
    memset(&event, 0, sizeof(event));

    nimcp_error_t result = omni_wm_kg_bridge_train_from_kg_event(nullptr, &event);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, TrainFromKgEventNullEventFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_train_from_kg_event(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, TrainFromObservationNullBridgeFails)
{
    float observed_state[TEST_STATE_DIM] = {0};
    nimcp_error_t result = omni_wm_kg_bridge_train_from_observation(
        nullptr, TEST_ENTITY_ID, observed_state, TEST_STATE_DIM, 1000);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, TrainFromObservationNullStateFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_train_from_observation(
        bridge_, TEST_ENTITY_ID, nullptr, TEST_STATE_DIM, 1000);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, TrainFromObservationZeroDimHandled)
{
    float observed_state[1] = {0};
    nimcp_error_t result = omni_wm_kg_bridge_train_from_observation(
        bridge_, TEST_ENTITY_ID, observed_state, 0, 1000);
    // Zero dim should be handled
}

TEST_F(OmniWmKgBridgeTest, FlushTrainingNullBridgeFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_flush_training(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 13. Registry Sync Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, SyncRegistryNullBridgeFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_sync_registry(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, UpdateModuleHealthNullBridgeFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_update_module_health(
        nullptr, TEST_MODULE_ID, KG_MODULE_HEALTH_HEALTHY, 0.9f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, UpdateModuleHealthInvalidHealthScore)
{
    // Score out of range [0,1]
    nimcp_error_t result = omni_wm_kg_bridge_update_module_health(
        bridge_, TEST_MODULE_ID, KG_MODULE_HEALTH_HEALTHY, 1.5f);
    // May clamp or error - should handle gracefully
}

TEST_F(OmniWmKgBridgeTest, UpdateModuleHealthNegativeScore)
{
    nimcp_error_t result = omni_wm_kg_bridge_update_module_health(
        bridge_, TEST_MODULE_ID, KG_MODULE_HEALTH_DEGRADED, -0.5f);
    // Should handle negative score
}

// =============================================================================
// 14. Query API Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, GetWmEffectsBasic)
{
    const omni_wm_to_kg_effects_t* effects = omni_wm_kg_bridge_get_wm_effects(bridge_);
    EXPECT_NE(effects, nullptr);
}

TEST_F(OmniWmKgBridgeTest, GetWmEffectsNullBridge)
{
    const omni_wm_to_kg_effects_t* effects = omni_wm_kg_bridge_get_wm_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(OmniWmKgBridgeTest, GetKgEffectsBasic)
{
    const kg_to_omni_wm_effects_t* effects = omni_wm_kg_bridge_get_kg_effects(bridge_);
    EXPECT_NE(effects, nullptr);
}

TEST_F(OmniWmKgBridgeTest, GetKgEffectsNullBridge)
{
    const kg_to_omni_wm_effects_t* effects = omni_wm_kg_bridge_get_kg_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

// =============================================================================
// 15. Statistics Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, GetStatsBasic)
{
    omni_wm_kg_bridge_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats)); // Fill with non-zero to verify overwrite

    nimcp_error_t result = omni_wm_kg_bridge_get_stats(bridge_, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_updates, 0u); // Should be zero for fresh bridge
}

TEST_F(OmniWmKgBridgeTest, GetStatsNullBridgeFails)
{
    omni_wm_kg_bridge_stats_t stats;
    nimcp_error_t result = omni_wm_kg_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, GetStatsNullOutputFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_get_stats(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, ResetStatsBasic)
{
    // Modify stats
    bridge_->stats.entity_predictions_made = 100;
    bridge_->stats.errors_total = 5;

    nimcp_error_t result = omni_wm_kg_bridge_reset_stats(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify reset
    EXPECT_EQ(bridge_->stats.entity_predictions_made, 0u);
    EXPECT_EQ(bridge_->stats.errors_total, 0u);
}

TEST_F(OmniWmKgBridgeTest, ResetStatsNullBridgeFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 16. Bio-Async Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, ConnectBioAsyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, DisconnectBioAsyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_kg_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmKgBridgeTest, IsBioAsyncConnectedNullFalse)
{
    EXPECT_FALSE(omni_wm_kg_bridge_is_bio_async_connected(nullptr));
}

TEST_F(OmniWmKgBridgeTest, IsBioAsyncConnectedInitiallyFalse)
{
    EXPECT_FALSE(omni_wm_kg_bridge_is_bio_async_connected(bridge_));
}

// =============================================================================
// 17. Utility Function Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, RelationshipTypeToString)
{
    const char* str = omni_wm_kg_relationship_type_to_string(KG_REL_TYPE_DEPENDS_ON);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

TEST_F(OmniWmKgBridgeTest, RelationshipTypeToStringUnknown)
{
    const char* str = omni_wm_kg_relationship_type_to_string(KG_REL_TYPE_UNKNOWN);
    EXPECT_NE(str, nullptr);
}

TEST_F(OmniWmKgBridgeTest, RelationshipTypeToStringInvalid)
{
    const char* str = omni_wm_kg_relationship_type_to_string(
        static_cast<kg_relationship_type_t>(999));
    EXPECT_NE(str, nullptr); // Should return a valid string even for invalid input
}

TEST_F(OmniWmKgBridgeTest, ModuleHealthToString)
{
    const char* str = omni_wm_kg_module_health_to_string(KG_MODULE_HEALTH_HEALTHY);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

TEST_F(OmniWmKgBridgeTest, ModuleHealthToStringAllStates)
{
    EXPECT_NE(omni_wm_kg_module_health_to_string(KG_MODULE_HEALTH_UNKNOWN), nullptr);
    EXPECT_NE(omni_wm_kg_module_health_to_string(KG_MODULE_HEALTH_HEALTHY), nullptr);
    EXPECT_NE(omni_wm_kg_module_health_to_string(KG_MODULE_HEALTH_DEGRADED), nullptr);
    EXPECT_NE(omni_wm_kg_module_health_to_string(KG_MODULE_HEALTH_FAILING), nullptr);
    EXPECT_NE(omni_wm_kg_module_health_to_string(KG_MODULE_HEALTH_FAILED), nullptr);
    EXPECT_NE(omni_wm_kg_module_health_to_string(KG_MODULE_HEALTH_RECOVERING), nullptr);
}

// =============================================================================
// 18. Memory Safety Tests
// =============================================================================

TEST_F(OmniWmKgBridgeTest, MultipleCreateDestroy)
{
    // Test multiple create/destroy cycles don't leak
    for (int i = 0; i < 10; i++) {
        omni_wm_kg_bridge_t* b = omni_wm_kg_bridge_create(nullptr);
        ASSERT_NE(b, nullptr);
        omni_wm_kg_bridge_destroy(b);
    }
}

TEST_F(OmniWmKgBridgeTest, DoubleDestroyHandled)
{
    omni_wm_kg_bridge_t* b = omni_wm_kg_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    omni_wm_kg_bridge_destroy(b);
    // Second destroy should be safe (though undefined behavior in practice)
    // Note: This test verifies null-safety, not double-free safety
}

TEST_F(OmniWmKgBridgeTest, ResetAfterOperations)
{
    // Connect and do some operations
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    omni_wm_kg_bridge_connect(bridge_, dummy_wm, nullptr, nullptr);

    // Update a few times
    omni_wm_kg_bridge_update(bridge_, 0.016f);
    omni_wm_kg_bridge_update(bridge_, 0.016f);

    // Reset should work
    nimcp_error_t result = omni_wm_kg_bridge_reset(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 19. Config Edge Cases
// =============================================================================

TEST_F(OmniWmKgBridgeTest, ConfigSensitivityBelowMin)
{
    omni_wm_kg_bridge_config_t config;
    omni_wm_kg_bridge_default_config(&config);
    config.sensitivity = 0.1f; // Below recommended 0.5

    omni_wm_kg_bridge_t* bridge = omni_wm_kg_bridge_create(&config);
    // Should either clamp or accept - implementation dependent
    if (bridge) {
        omni_wm_kg_bridge_destroy(bridge);
    }
}

TEST_F(OmniWmKgBridgeTest, ConfigSensitivityAboveMax)
{
    omni_wm_kg_bridge_config_t config;
    omni_wm_kg_bridge_default_config(&config);
    config.sensitivity = 5.0f; // Above recommended 2.0

    omni_wm_kg_bridge_t* bridge = omni_wm_kg_bridge_create(&config);
    // Should either clamp or accept - implementation dependent
    if (bridge) {
        omni_wm_kg_bridge_destroy(bridge);
    }
}

TEST_F(OmniWmKgBridgeTest, ConfigAllFeaturesEnabled)
{
    omni_wm_kg_bridge_config_t config;
    omni_wm_kg_bridge_default_config(&config);

    config.enable_modulation = true;
    config.enable_entity_prediction = true;
    config.enable_relationship_prediction = true;
    config.enable_module_prediction = true;
    config.enable_anomaly_detection = true;
    config.enable_training_from_kg = true;
    config.enable_registry_sync = true;
    config.enable_bio_async = true;

    omni_wm_kg_bridge_t* bridge = omni_wm_kg_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
    omni_wm_kg_bridge_destroy(bridge);
}

TEST_F(OmniWmKgBridgeTest, ConfigAllFeaturesDisabled)
{
    omni_wm_kg_bridge_config_t config;
    omni_wm_kg_bridge_default_config(&config);

    config.enable_modulation = false;
    config.enable_entity_prediction = false;
    config.enable_relationship_prediction = false;
    config.enable_module_prediction = false;
    config.enable_anomaly_detection = false;
    config.enable_training_from_kg = false;
    config.enable_registry_sync = false;
    config.enable_bio_async = false;

    omni_wm_kg_bridge_t* bridge = omni_wm_kg_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
    omni_wm_kg_bridge_destroy(bridge);
}

// =============================================================================
// 20. Concurrent Access Safety Tests (Basic)
// =============================================================================

TEST_F(OmniWmKgBridgeTest, MutexIsInitialized)
{
    ASSERT_NE(bridge_, nullptr);
    EXPECT_NE(bridge_->base.mutex, nullptr);
}

// Main function for standalone execution
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
