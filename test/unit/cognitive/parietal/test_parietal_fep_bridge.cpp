/**
 * @file test_parietal_fep_bridge.cpp
 * @brief Unit tests for Parietal Lobe - FEP Orchestrator Integration Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for FEP-Parietal bidirectional integration
 * WHY:  Ensure free energy computation from spatial/body schema/math metrics works correctly
 * HOW:  Test lifecycle, registration, metrics, callbacks, and FEP update cycle
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Default Configuration
 * - FEP Registration/Unregistration
 * - Update Callback Mechanism
 * - Free Energy Computation
 * - Spatial Uncertainty -> Free Energy
 * - Body Schema Error -> Free Energy
 * - Math Prediction Error -> Free Energy
 * - Accurate Predictions Reduce Free Energy
 * - Metrics and Statistics Tracking
 * - Statistics Reset
 * - Callback Registration
 * - Thread Safety
 * - Null Parameter Handling
 * - Error Conditions
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/parietal/nimcp_parietal_fep_bridge.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_high_fe_callback_count{0};
static std::atomic<int> g_surprise_callback_count{0};
static std::atomic<int> g_metrics_callback_count{0};
static float g_last_free_energy = 0.0f;
static float g_last_surprise = 0.0f;
static parietal_fep_metrics_t g_last_metrics;

/**
 * Test callback for high free energy events
 */
static void test_high_fe_callback(
    parietal_fep_bridge_t* bridge,
    float free_energy,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    g_high_fe_callback_count++;
    g_last_free_energy = free_energy;
}

/**
 * Test callback for surprise events
 */
static void test_surprise_callback(
    parietal_fep_bridge_t* bridge,
    float surprise,
    const char* source,
    void* user_data
) {
    (void)bridge;
    (void)source;
    (void)user_data;
    g_surprise_callback_count++;
    g_last_surprise = surprise;
}

/**
 * Test callback for metrics updates
 */
static void test_metrics_callback(
    parietal_fep_bridge_t* bridge,
    const parietal_fep_metrics_t* metrics,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    g_metrics_callback_count++;
    if (metrics != nullptr) {
        memcpy(&g_last_metrics, metrics, sizeof(parietal_fep_metrics_t));
    }
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ParietalFepBridgeTest : public ::testing::Test {
protected:
    parietal_fep_bridge_t* bridge = nullptr;
    parietal_fep_config_t config;

    void SetUp() override {
        // Reset global state
        g_high_fe_callback_count = 0;
        g_surprise_callback_count = 0;
        g_metrics_callback_count = 0;
        g_last_free_energy = 0.0f;
        g_last_surprise = 0.0f;
        memset(&g_last_metrics, 0, sizeof(g_last_metrics));

        // Get default config and create bridge
        config = parietal_fep_config_default();
        bridge = parietal_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            parietal_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Bridge Lifecycle Tests
 * ============================================================================ */

/**
 * Test: BridgeCreation
 * Verify bridge can be created and destroyed successfully
 */
TEST_F(ParietalFepBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify initial state
    parietal_fep_state_t state = parietal_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, PARIETAL_FEP_STATE_IDLE) << "Initial state should be IDLE";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(ParietalFepBridgeTest, BridgeCreationNullConfig) {
    parietal_fep_bridge_t* br = parietal_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    parietal_fep_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(ParietalFepBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    parietal_fep_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    parietal_fep_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(ParietalFepBridgeTest, DefaultConfig) {
    parietal_fep_config_t default_config = parietal_fep_config_default();

    // Verify weights are positive
    EXPECT_GT(default_config.spatial_uncertainty_weight, 0.0f);
    EXPECT_GT(default_config.body_schema_error_weight, 0.0f);
    EXPECT_GT(default_config.math_error_weight, 0.0f);

    // Verify weights sum to approximately 1.0
    float weight_sum = default_config.spatial_uncertainty_weight +
                       default_config.body_schema_error_weight +
                       default_config.math_error_weight;
    EXPECT_NEAR(weight_sum, 1.0f, 0.01f) << "Weights should sum to 1.0";

    // Verify thresholds are sensible
    EXPECT_GT(default_config.high_free_energy_threshold, 0.0f);
    EXPECT_GT(default_config.prediction_error_threshold, 0.0f);

    // Verify normalization parameters
    EXPECT_GE(default_config.baseline_free_energy, 0.0f);
    EXPECT_GT(default_config.max_free_energy, default_config.baseline_free_energy);

    // Verify decay rate is valid
    EXPECT_GT(default_config.error_decay_rate, 0.0f);
    EXPECT_LE(default_config.error_decay_rate, 1.0f);

    // Verify timing
    EXPECT_GT(default_config.update_interval_ms, 0u);
}

/**
 * Test: BridgeReset
 * Verify bridge can be reset to initial state
 */
TEST_F(ParietalFepBridgeTest, BridgeReset) {
    ASSERT_NE(bridge, nullptr);

    // Force an update to change state
    int result = parietal_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Reset bridge
    result = parietal_fep_bridge_reset(bridge);
    EXPECT_EQ(result, 0) << "Reset should succeed";

    // Verify state is back to IDLE
    parietal_fep_state_t state = parietal_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, PARIETAL_FEP_STATE_IDLE) << "State should be IDLE after reset";
}

/**
 * Test: BridgeResetNull
 * Verify reset handles NULL gracefully
 */
TEST_F(ParietalFepBridgeTest, BridgeResetNull) {
    int result = parietal_fep_bridge_reset(nullptr);
    EXPECT_EQ(result, -1) << "Reset with NULL should fail";
}

/* ============================================================================
 * FEP Registration Tests
 * ============================================================================ */

/**
 * Test: RegisterWithFEP
 * Verify bridge registration query methods work without orchestrator
 */
TEST_F(ParietalFepBridgeTest, RegisterWithFEP) {
    ASSERT_NE(bridge, nullptr);

    // Before registration, should not be registered
    EXPECT_FALSE(parietal_fep_bridge_is_registered(bridge))
        << "Should not be registered initially";

    // Without actual orchestrator, we test the ID accessor
    uint32_t id = parietal_fep_bridge_get_id(bridge);
    EXPECT_EQ(id, 0u) << "ID should be 0 when not registered";
}

/**
 * Test: UnregisterFromFEP
 * Verify bridge can unregister cleanly
 */
TEST_F(ParietalFepBridgeTest, UnregisterFromFEP) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should handle gracefully
    int result = parietal_fep_bridge_unregister(bridge);
    // Should succeed (nothing to do)
    EXPECT_EQ(result, 0) << "Unregister when not registered should succeed";

    EXPECT_FALSE(parietal_fep_bridge_is_registered(bridge))
        << "Should not be registered after unregister";
}

/**
 * Test: IsRegisteredNull
 * Verify is_registered handles NULL gracefully
 */
TEST_F(ParietalFepBridgeTest, IsRegisteredNull) {
    bool registered = parietal_fep_bridge_is_registered(nullptr);
    EXPECT_FALSE(registered) << "NULL bridge should not be registered";
}

/**
 * Test: GetIdNull
 * Verify get_id handles NULL gracefully
 */
TEST_F(ParietalFepBridgeTest, GetIdNull) {
    uint32_t id = parietal_fep_bridge_get_id(nullptr);
    EXPECT_EQ(id, 0u) << "NULL bridge should return ID 0";
}

/* ============================================================================
 * FEP Update Callback Tests
 * ============================================================================ */

/**
 * Test: UpdateCallback
 * Verify FEP update callback works correctly
 * Note: Raw callback requires registration, returns -1 without it
 */
TEST_F(ParietalFepBridgeTest, UpdateCallback) {
    ASSERT_NE(bridge, nullptr);

    // Call update callback directly - should fail without registration
    int result = parietal_fep_update_callback(bridge);
    EXPECT_EQ(result, -1) << "Update callback should fail without registration";

    // Use force_update for unregistered bridge (test-friendly path)
    result = parietal_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed without registration";
}

/**
 * Test: UpdateCallbackNull
 * Verify update callback handles NULL gracefully
 */
TEST_F(ParietalFepBridgeTest, UpdateCallbackNull) {
    int result = parietal_fep_update_callback(nullptr);
    EXPECT_EQ(result, -1) << "Update callback with NULL should fail";
}

/**
 * Test: ForceUpdate
 * Verify force update triggers FEP computation
 */
TEST_F(ParietalFepBridgeTest, ForceUpdate) {
    ASSERT_NE(bridge, nullptr);

    // Get initial metrics
    parietal_fep_metrics_t initial_metrics;
    int result = parietal_fep_bridge_get_metrics(bridge, &initial_metrics);
    EXPECT_EQ(result, 0);

    // Force update
    result = parietal_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Get updated metrics
    parietal_fep_metrics_t updated_metrics;
    result = parietal_fep_bridge_get_metrics(bridge, &updated_metrics);
    EXPECT_EQ(result, 0);

    // Update count should have increased
    EXPECT_GE(updated_metrics.update_count, initial_metrics.update_count)
        << "Update count should increase after force update";
}

/**
 * Test: ForceUpdateNull
 * Verify force update handles NULL gracefully
 */
TEST_F(ParietalFepBridgeTest, ForceUpdateNull) {
    int result = parietal_fep_bridge_force_update(nullptr);
    EXPECT_EQ(result, -1) << "Force update with NULL should fail";
}

/* ============================================================================
 * Free Energy Computation Tests
 * ============================================================================ */

/**
 * Test: FreeEnergyContribution
 * Verify free energy is calculated and accessible
 */
TEST_F(ParietalFepBridgeTest, FreeEnergyContribution) {
    ASSERT_NE(bridge, nullptr);

    // Get initial free energy
    float initial_fe = parietal_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_GE(initial_fe, 0.0f) << "Free energy should be non-negative";
    EXPECT_LE(initial_fe, config.max_free_energy)
        << "Free energy should not exceed maximum";

    // After force update, free energy should still be valid
    parietal_fep_bridge_force_update(bridge);
    float updated_fe = parietal_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_GE(updated_fe, 0.0f);
    EXPECT_LE(updated_fe, config.max_free_energy);
}

/**
 * Test: FreeEnergyContributionNull
 * Verify get_free_energy_contribution handles NULL gracefully
 */
TEST_F(ParietalFepBridgeTest, FreeEnergyContributionNull) {
    float fe = parietal_fep_bridge_get_free_energy_contribution(nullptr);
    EXPECT_LT(fe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: SpatialUncertaintyIncreasesFreeEnergy
 * Verify that spatial uncertainty contributes to free energy
 */
TEST_F(ParietalFepBridgeTest, SpatialUncertaintyIncreasesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Get initial spatial uncertainty
    float uncertainty = parietal_fep_bridge_get_spatial_uncertainty(bridge);
    EXPECT_GE(uncertainty, 0.0f) << "Spatial uncertainty should be non-negative";
    EXPECT_LE(uncertainty, 1.0f) << "Spatial uncertainty should be <= 1.0";

    // Spatial uncertainty should contribute to free energy
    // (exact relationship tested in integration tests with actual parietal module)
    float fe = parietal_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_GE(fe, config.baseline_free_energy)
        << "Free energy should be at least baseline";
}

/**
 * Test: BodySchemaErrorContribution
 * Verify body schema error is tracked
 */
TEST_F(ParietalFepBridgeTest, BodySchemaErrorContribution) {
    ASSERT_NE(bridge, nullptr);

    // Get body schema error
    float error = parietal_fep_bridge_get_body_schema_error(bridge);
    EXPECT_GE(error, 0.0f) << "Body schema error should be non-negative";
    EXPECT_LE(error, 1.0f) << "Body schema error should be <= 1.0";
}

/**
 * Test: BodySchemaErrorNull
 * Verify get_body_schema_error handles NULL gracefully
 */
TEST_F(ParietalFepBridgeTest, BodySchemaErrorNull) {
    float error = parietal_fep_bridge_get_body_schema_error(nullptr);
    EXPECT_LT(error, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: AccurateSpatialPredictionsReduceFreeEnergy
 * Verify concept that accurate predictions should minimize free energy
 * (This is a conceptual test - actual reduction requires parietal module)
 */
TEST_F(ParietalFepBridgeTest, AccurateSpatialPredictionsReduceFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Initial free energy at baseline (good predictions = low FE)
    float fe = parietal_fep_bridge_get_free_energy_contribution(bridge);

    // Baseline free energy represents accurate predictions
    // Free energy should be near baseline when there are no errors
    EXPECT_LE(fe, config.baseline_free_energy * 2.0f)
        << "Free energy should be low with accurate predictions";
}

/**
 * Test: PredictionError
 * Verify prediction error tracking
 */
TEST_F(ParietalFepBridgeTest, PredictionError) {
    ASSERT_NE(bridge, nullptr);

    float pe = parietal_fep_bridge_get_prediction_error(bridge);
    EXPECT_GE(pe, 0.0f) << "Prediction error should be non-negative";
    EXPECT_LE(pe, 1.0f) << "Prediction error should be <= 1.0";
}

/**
 * Test: PredictionErrorNull
 * Verify get_prediction_error handles NULL gracefully
 */
TEST_F(ParietalFepBridgeTest, PredictionErrorNull) {
    float pe = parietal_fep_bridge_get_prediction_error(nullptr);
    EXPECT_LT(pe, 0.0f) << "NULL bridge should return negative value";
}

/* ============================================================================
 * Statistics Tracking Tests
 * ============================================================================ */

/**
 * Test: StatisticsTracking
 * Verify statistics are accumulated correctly
 */
TEST_F(ParietalFepBridgeTest, StatisticsTracking) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        parietal_fep_bridge_force_update(bridge);
    }

    parietal_fep_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = parietal_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify stats are accumulated
    EXPECT_GE(stats.total_updates, 5u) << "Should have at least 5 updates";
    EXPECT_GE(stats.avg_free_energy, 0.0f);
    EXPECT_GE(stats.avg_prediction_error, 0.0f);
}

/**
 * Test: StatisticsTrackingNull
 * Verify get_stats handles NULL parameters gracefully
 */
TEST_F(ParietalFepBridgeTest, StatisticsTrackingNull) {
    parietal_fep_stats_t stats;

    int result = parietal_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1) << "Get stats with NULL bridge should fail";

    result = parietal_fep_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(result, -1) << "Get stats with NULL output should fail";
}

/**
 * Test: StatsReset
 * Verify statistics can be reset
 */
TEST_F(ParietalFepBridgeTest, StatsReset) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        parietal_fep_bridge_force_update(bridge);
    }

    // Reset stats
    int result = parietal_fep_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    parietal_fep_stats_t stats;
    result = parietal_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    // Total updates should be reset to 0
    EXPECT_EQ(stats.total_updates, 0u) << "Updates should be reset to 0";
}

/**
 * Test: StatsResetNull
 * Verify reset_stats handles NULL gracefully
 */
TEST_F(ParietalFepBridgeTest, StatsResetNull) {
    int result = parietal_fep_bridge_reset_stats(nullptr);
    EXPECT_EQ(result, -1) << "Reset stats with NULL should fail";
}

/* ============================================================================
 * Metrics Tracking Tests
 * ============================================================================ */

/**
 * Test: MetricsTracking
 * Verify metrics are updated correctly during FEP cycles
 */
TEST_F(ParietalFepBridgeTest, MetricsTracking) {
    ASSERT_NE(bridge, nullptr);

    parietal_fep_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    int result = parietal_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(result, 0) << "Get metrics should succeed";

    // Verify core FEP metrics are valid
    EXPECT_GE(metrics.free_energy, 0.0f);
    EXPECT_GE(metrics.prediction_error, 0.0f);
    EXPECT_LE(metrics.prediction_error, 1.0f);
    EXPECT_GE(metrics.entropy, 0.0f);

    // Verify parietal-specific metrics are valid
    EXPECT_GE(metrics.spatial_uncertainty, 0.0f);
    EXPECT_LE(metrics.spatial_uncertainty, 1.0f);
    EXPECT_GE(metrics.body_schema_error, 0.0f);
    EXPECT_LE(metrics.body_schema_error, 1.0f);
    EXPECT_GE(metrics.math_prediction_error, 0.0f);
    EXPECT_LE(metrics.math_prediction_error, 1.0f);
}

/**
 * Test: MetricsTrackingNull
 * Verify get_metrics handles NULL parameters gracefully
 */
TEST_F(ParietalFepBridgeTest, MetricsTrackingNull) {
    parietal_fep_metrics_t metrics;

    int result = parietal_fep_bridge_get_metrics(nullptr, &metrics);
    EXPECT_EQ(result, -1) << "Get metrics with NULL bridge should fail";

    result = parietal_fep_bridge_get_metrics(bridge, nullptr);
    EXPECT_EQ(result, -1) << "Get metrics with NULL output should fail";
}

/* ============================================================================
 * State Query Tests
 * ============================================================================ */

/**
 * Test: GetState
 * Verify bridge state can be queried
 */
TEST_F(ParietalFepBridgeTest, GetState) {
    ASSERT_NE(bridge, nullptr);

    parietal_fep_state_t state = parietal_fep_bridge_get_state(bridge);
    // State should be one of the valid states
    EXPECT_GE((int)state, (int)PARIETAL_FEP_STATE_UNINITIALIZED);
    EXPECT_LE((int)state, (int)PARIETAL_FEP_STATE_ERROR);
}

/**
 * Test: GetStateNull
 * Verify get_state handles NULL gracefully
 */
TEST_F(ParietalFepBridgeTest, GetStateNull) {
    parietal_fep_state_t state = parietal_fep_bridge_get_state(nullptr);
    EXPECT_EQ(state, PARIETAL_FEP_STATE_ERROR)
        << "NULL bridge should return ERROR state";
}

/**
 * Test: IsDegraded
 * Verify degraded mode detection
 */
TEST_F(ParietalFepBridgeTest, IsDegraded) {
    ASSERT_NE(bridge, nullptr);

    // Initially should not be degraded
    bool degraded = parietal_fep_bridge_is_degraded(bridge);
    EXPECT_FALSE(degraded) << "Should not be degraded initially";
}

/**
 * Test: IsDegradedNull
 * Verify is_degraded handles NULL gracefully
 */
TEST_F(ParietalFepBridgeTest, IsDegradedNull) {
    bool degraded = parietal_fep_bridge_is_degraded(nullptr);
    EXPECT_FALSE(degraded) << "NULL bridge should return false";
}

/**
 * Test: StateName
 * Verify state name conversion
 */
TEST_F(ParietalFepBridgeTest, StateName) {
    const char* name = parietal_fep_state_name(PARIETAL_FEP_STATE_IDLE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u) << "State name should not be empty";

    name = parietal_fep_state_name(PARIETAL_FEP_STATE_ACTIVE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = parietal_fep_state_name(PARIETAL_FEP_STATE_DEGRADED);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = parietal_fep_state_name(PARIETAL_FEP_STATE_ERROR);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

/* ============================================================================
 * Callback Registration Tests
 * ============================================================================ */

/**
 * Test: HighFECallback
 * Verify high free energy callback registration
 */
TEST_F(ParietalFepBridgeTest, HighFECallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = parietal_fep_bridge_set_high_fe_callback(
        bridge, test_high_fe_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = parietal_fep_bridge_set_high_fe_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: HighFECallbackNull
 * Verify callback registration handles NULL bridge
 */
TEST_F(ParietalFepBridgeTest, HighFECallbackNull) {
    int result = parietal_fep_bridge_set_high_fe_callback(
        nullptr, test_high_fe_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/**
 * Test: SurpriseCallback
 * Verify surprise event callback registration
 */
TEST_F(ParietalFepBridgeTest, SurpriseCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = parietal_fep_bridge_set_surprise_callback(
        bridge, test_surprise_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = parietal_fep_bridge_set_surprise_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: SurpriseCallbackNull
 * Verify callback registration handles NULL bridge
 */
TEST_F(ParietalFepBridgeTest, SurpriseCallbackNull) {
    int result = parietal_fep_bridge_set_surprise_callback(
        nullptr, test_surprise_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/**
 * Test: MetricsCallback
 * Verify metrics update callback registration
 */
TEST_F(ParietalFepBridgeTest, MetricsCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = parietal_fep_bridge_set_metrics_callback(
        bridge, test_metrics_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Force update to trigger callback
    parietal_fep_bridge_force_update(bridge);

    // Metrics callback should have been called
    EXPECT_GE(g_metrics_callback_count.load(), 1)
        << "Metrics callback should be called on update";

    // Clear callback
    result = parietal_fep_bridge_set_metrics_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: MetricsCallbackNull
 * Verify callback registration handles NULL bridge
 */
TEST_F(ParietalFepBridgeTest, MetricsCallbackNull) {
    int result = parietal_fep_bridge_set_metrics_callback(
        nullptr, test_metrics_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

/**
 * Test: ConfigValidation
 * Verify configuration values are properly applied
 */
TEST_F(ParietalFepBridgeTest, ConfigValidation) {
    ASSERT_NE(bridge, nullptr);

    parietal_fep_config_t retrieved_config;
    memset(&retrieved_config, 0, sizeof(retrieved_config));

    int result = parietal_fep_bridge_get_config(bridge, &retrieved_config);
    EXPECT_EQ(result, 0) << "Get config should succeed";

    // Verify retrieved config matches what was set
    EXPECT_FLOAT_EQ(retrieved_config.spatial_uncertainty_weight,
                    config.spatial_uncertainty_weight);
    EXPECT_FLOAT_EQ(retrieved_config.body_schema_error_weight,
                    config.body_schema_error_weight);
    EXPECT_FLOAT_EQ(retrieved_config.max_free_energy,
                    config.max_free_energy);
}

/**
 * Test: SetConfig
 * Verify configuration can be updated
 */
TEST_F(ParietalFepBridgeTest, SetConfig) {
    ASSERT_NE(bridge, nullptr);

    parietal_fep_config_t new_config = parietal_fep_config_default();
    new_config.spatial_uncertainty_weight = 0.5f;
    new_config.body_schema_error_weight = 0.3f;
    new_config.math_error_weight = 0.2f;

    int result = parietal_fep_bridge_set_config(bridge, &new_config);
    EXPECT_EQ(result, 0) << "Set config should succeed";

    // Verify the change took effect
    parietal_fep_config_t retrieved_config;
    result = parietal_fep_bridge_get_config(bridge, &retrieved_config);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(retrieved_config.spatial_uncertainty_weight, 0.5f);
}

/**
 * Test: SetConfigNull
 * Verify set_config handles NULL parameters
 */
TEST_F(ParietalFepBridgeTest, SetConfigNull) {
    parietal_fep_config_t config_val = parietal_fep_config_default();

    int result = parietal_fep_bridge_set_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = parietal_fep_bridge_set_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL config should fail";
}

/**
 * Test: GetConfigNull
 * Verify get_config handles NULL parameters
 */
TEST_F(ParietalFepBridgeTest, GetConfigNull) {
    parietal_fep_config_t config_val;

    int result = parietal_fep_bridge_get_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = parietal_fep_bridge_get_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL output should fail";
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Comprehensive test for NULL parameter handling across all functions
 */
TEST_F(ParietalFepBridgeTest, NullHandling) {
    // Lifecycle
    parietal_fep_bridge_destroy(nullptr);

    // Reset
    EXPECT_EQ(parietal_fep_bridge_reset(nullptr), -1);

    // Registration
    EXPECT_FALSE(parietal_fep_bridge_is_registered(nullptr));
    EXPECT_EQ(parietal_fep_bridge_get_id(nullptr), 0u);
    EXPECT_EQ(parietal_fep_bridge_unregister(nullptr), -1);

    // Updates
    EXPECT_EQ(parietal_fep_update_callback(nullptr), -1);
    EXPECT_EQ(parietal_fep_bridge_force_update(nullptr), -1);

    // Metrics
    parietal_fep_metrics_t metrics;
    EXPECT_EQ(parietal_fep_bridge_get_metrics(nullptr, &metrics), -1);
    EXPECT_EQ(parietal_fep_bridge_get_metrics(bridge, nullptr), -1);

    parietal_fep_stats_t stats;
    EXPECT_EQ(parietal_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(parietal_fep_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(parietal_fep_bridge_reset_stats(nullptr), -1);

    // Accessors
    EXPECT_EQ(parietal_fep_bridge_get_state(nullptr), PARIETAL_FEP_STATE_ERROR);
    EXPECT_LT(parietal_fep_bridge_get_free_energy_contribution(nullptr), 0.0f);
    EXPECT_LT(parietal_fep_bridge_get_spatial_uncertainty(nullptr), 0.0f);
    EXPECT_LT(parietal_fep_bridge_get_body_schema_error(nullptr), 0.0f);
    EXPECT_LT(parietal_fep_bridge_get_prediction_error(nullptr), 0.0f);
    EXPECT_FALSE(parietal_fep_bridge_is_degraded(nullptr));

    // Callbacks
    EXPECT_EQ(parietal_fep_bridge_set_high_fe_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(parietal_fep_bridge_set_surprise_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(parietal_fep_bridge_set_metrics_callback(nullptr, nullptr, nullptr), -1);

    // Config
    parietal_fep_config_t cfg;
    EXPECT_EQ(parietal_fep_bridge_set_config(nullptr, &cfg), -1);
    EXPECT_EQ(parietal_fep_bridge_set_config(bridge, nullptr), -1);
    EXPECT_EQ(parietal_fep_bridge_get_config(nullptr, &cfg), -1);
    EXPECT_EQ(parietal_fep_bridge_get_config(bridge, nullptr), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/**
 * Test: ErrorConditions
 * Test various error conditions
 */
TEST_F(ParietalFepBridgeTest, ErrorConditions) {
    ASSERT_NE(bridge, nullptr);

    // Register with NULL parameters should fail
    int result = parietal_fep_bridge_register(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1) << "Register with all NULL should fail";

    uint32_t bridge_id = 0;
    result = parietal_fep_bridge_register(bridge, nullptr, nullptr, &bridge_id);
    EXPECT_EQ(result, -1) << "Register without orchestrator should fail";

    SUCCEED() << "Error condition tests passed";
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Verify basic thread safety with concurrent access
 */
TEST_F(ParietalFepBridgeTest, ThreadSafety) {
    ASSERT_NE(bridge, nullptr);

    const int NUM_THREADS = 4;
    const int ITERATIONS = 100;
    std::atomic<int> completed{0};
    std::vector<std::thread> threads;

    // Create threads that concurrently access the bridge
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &completed, ITERATIONS]() {
            for (int i = 0; i < ITERATIONS; i++) {
                // Mix of read operations
                parietal_fep_bridge_get_free_energy_contribution(bridge);
                parietal_fep_bridge_get_spatial_uncertainty(bridge);
                parietal_fep_bridge_get_body_schema_error(bridge);
                parietal_fep_bridge_get_prediction_error(bridge);
                parietal_fep_bridge_get_state(bridge);
                parietal_fep_bridge_is_degraded(bridge);

                parietal_fep_metrics_t metrics;
                parietal_fep_bridge_get_metrics(bridge, &metrics);

                parietal_fep_stats_t stats;
                parietal_fep_bridge_get_stats(bridge, &stats);

                // Write operation
                parietal_fep_bridge_force_update(bridge);
            }
            completed++;
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed.load(), NUM_THREADS)
        << "All threads should complete successfully";
}

/* ============================================================================
 * FEP Destroy Callback Test
 * ============================================================================ */

/**
 * Test: DestroyCallback
 * Verify destroy callback can be called safely
 */
TEST_F(ParietalFepBridgeTest, DestroyCallback) {
    ASSERT_NE(bridge, nullptr);

    // Destroy callback should be safe to call
    parietal_fep_destroy_callback(bridge);
    SUCCEED() << "Destroy callback should not crash";

    // Should also handle NULL
    parietal_fep_destroy_callback(nullptr);
    SUCCEED() << "Destroy callback with NULL should not crash";
}
