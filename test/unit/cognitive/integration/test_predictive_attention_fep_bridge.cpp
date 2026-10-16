/**
 * @file test_predictive_attention_fep_bridge.cpp
 * @brief Unit tests for Predictive-Attention FEP Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for FEP-Predictive-Attention bidirectional integration
 * WHY:  Ensure free energy computation from predictive coding and attention works correctly
 * HOW:  Test lifecycle, registration, metrics, callbacks, and FEP update cycle
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Configuration Validation
 * - FEP Registration/Unregistration
 * - Update Callback Mechanism
 * - Free Energy Computation
 * - Prediction Accuracy Effects
 * - Attention Precision Effects
 * - Error Signal Quality Effects
 * - High Precision Mode Detection
 * - Prediction Error Tracking
 * - Metrics and Statistics
 * - Callback Registration
 * - Null Parameter Handling
 * - Thread Safety
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/integration/nimcp_predictive_attention_fep_bridge.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_high_fe_callback_count{0};
static std::atomic<int> g_surprise_callback_count{0};
static std::atomic<int> g_metrics_callback_count{0};
static float g_last_free_energy = 0.0f;
static float g_last_surprise = 0.0f;
static std::string g_last_surprise_source;
static pa_fep_metrics_t g_last_metrics;

/**
 * Test callback for high free energy events
 */
static void test_high_fe_callback(
    pa_fep_bridge_t* bridge,
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
    pa_fep_bridge_t* bridge,
    float surprise,
    const char* source,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    g_surprise_callback_count++;
    g_last_surprise = surprise;
    if (source) {
        g_last_surprise_source = source;
    }
}

/**
 * Test callback for metrics updates
 */
static void test_metrics_callback(
    pa_fep_bridge_t* bridge,
    const pa_fep_metrics_t* metrics,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    g_metrics_callback_count++;
    if (metrics != nullptr) {
        memcpy(&g_last_metrics, metrics, sizeof(pa_fep_metrics_t));
    }
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PredictiveAttentionFepBridgeTest : public ::testing::Test {
protected:
    pa_fep_bridge_t* bridge = nullptr;
    pa_fep_config_t config;

    void SetUp() override {
        // Reset global state
        g_high_fe_callback_count = 0;
        g_surprise_callback_count = 0;
        g_metrics_callback_count = 0;
        g_last_free_energy = 0.0f;
        g_last_surprise = 0.0f;
        g_last_surprise_source.clear();
        memset(&g_last_metrics, 0, sizeof(g_last_metrics));

        // Get default config and create bridge
        config = pa_fep_config_default();
        bridge = pa_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            pa_fep_bridge_destroy(bridge);
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
TEST_F(PredictiveAttentionFepBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify initial state
    pa_fep_state_t state = pa_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, PA_FEP_STATE_IDLE) << "Initial state should be IDLE";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(PredictiveAttentionFepBridgeTest, BridgeCreationNullConfig) {
    pa_fep_bridge_t* br = pa_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    pa_fep_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(PredictiveAttentionFepBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    pa_fep_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    pa_fep_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(PredictiveAttentionFepBridgeTest, DefaultConfig) {
    pa_fep_config_t default_config = pa_fep_config_default();

    // Verify weights are positive
    EXPECT_GT(default_config.free_energy_weight, 0.0f);
    EXPECT_GT(default_config.prediction_accuracy_weight, 0.0f);
    EXPECT_GT(default_config.attention_precision_weight, 0.0f);
    EXPECT_GT(default_config.error_signal_quality_weight, 0.0f);

    // Verify thresholds are sensible
    EXPECT_GT(default_config.high_free_energy_threshold, 0.0f);
    EXPECT_GT(default_config.prediction_error_threshold, 0.0f);
    EXPECT_GT(default_config.precision_epsilon, 0.0f);

    // Verify normalization parameters
    EXPECT_GE(default_config.baseline_free_energy, 0.0f);
    EXPECT_GT(default_config.max_free_energy, default_config.baseline_free_energy);

    // Verify decay rate is valid
    EXPECT_GT(default_config.error_decay_rate, 0.0f);
    EXPECT_LE(default_config.error_decay_rate, 1.0f);

    // Verify update interval
    EXPECT_GT(default_config.update_interval_ms, 0u);
}

/**
 * Test: ConfigValidation
 * Verify custom configuration is applied correctly
 */
TEST_F(PredictiveAttentionFepBridgeTest, ConfigValidation) {
    pa_fep_config_t custom_config = pa_fep_config_default();
    custom_config.prediction_accuracy_weight = 0.5f;
    custom_config.attention_precision_weight = 0.3f;
    custom_config.error_signal_quality_weight = 0.2f;
    custom_config.high_free_energy_threshold = 1.8f;

    pa_fep_bridge_t* custom_bridge = pa_fep_bridge_create(&custom_config);
    ASSERT_NE(custom_bridge, nullptr);

    pa_fep_config_t retrieved_config;
    int result = pa_fep_bridge_get_config(custom_bridge, &retrieved_config);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(retrieved_config.prediction_accuracy_weight, 0.5f);
    EXPECT_FLOAT_EQ(retrieved_config.attention_precision_weight, 0.3f);
    EXPECT_FLOAT_EQ(retrieved_config.error_signal_quality_weight, 0.2f);
    EXPECT_FLOAT_EQ(retrieved_config.high_free_energy_threshold, 1.8f);

    pa_fep_bridge_destroy(custom_bridge);
}

/**
 * Test: BridgeReset
 * Verify bridge can be reset to initial state
 */
TEST_F(PredictiveAttentionFepBridgeTest, BridgeReset) {
    ASSERT_NE(bridge, nullptr);

    // Force an update to change state
    pa_fep_bridge_update_prediction_accuracy(bridge, 0.2f);
    int result = pa_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Reset bridge
    result = pa_fep_bridge_reset(bridge);
    EXPECT_EQ(result, 0) << "Reset should succeed";

    // Verify state is back to IDLE
    pa_fep_state_t state = pa_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, PA_FEP_STATE_IDLE) << "State should be IDLE after reset";

    // Verify metrics are reset to neutral values
    float pa = pa_fep_bridge_get_prediction_accuracy(bridge);
    EXPECT_FLOAT_EQ(pa, 0.5f) << "Prediction accuracy should be reset to 0.5";
}

/* ============================================================================
 * FEP Registration Tests
 * ============================================================================ */

/**
 * Test: RegisterWithFEP
 * Verify bridge registration state without actual orchestrator
 */
TEST_F(PredictiveAttentionFepBridgeTest, RegisterWithFEP) {
    ASSERT_NE(bridge, nullptr);

    // Before registration, should not be registered
    EXPECT_FALSE(pa_fep_bridge_is_registered(bridge))
        << "Should not be registered initially";

    // Without actual orchestrator, ID should be 0
    uint32_t id = pa_fep_bridge_get_id(bridge);
    EXPECT_EQ(id, 0u) << "ID should be 0 when not registered";
}

/**
 * Test: UnregisterFromFEP
 * Verify bridge can unregister cleanly
 */
TEST_F(PredictiveAttentionFepBridgeTest, UnregisterFromFEP) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should handle gracefully
    int result = pa_fep_bridge_unregister(bridge);
    EXPECT_EQ(result, 0) << "Unregister when not registered should succeed";

    EXPECT_FALSE(pa_fep_bridge_is_registered(bridge))
        << "Should not be registered after unregister";
}

/**
 * Test: RegistrationNullParams
 * Verify registration handles NULL parameters gracefully
 */
TEST_F(PredictiveAttentionFepBridgeTest, RegistrationNullParams) {
    ASSERT_NE(bridge, nullptr);

    uint32_t bridge_id = 0;

    // NULL bridge
    int result = pa_fep_bridge_register(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1) << "Register with NULL bridge should fail";

    // NULL orchestrator
    result = pa_fep_bridge_register(bridge, nullptr, nullptr, &bridge_id);
    EXPECT_EQ(result, -1) << "Register without orchestrator should fail";
}

/* ============================================================================
 * FEP Update Callback Tests
 * ============================================================================ */

/**
 * Test: UpdateCallback
 * Verify FEP update callback behavior
 */
TEST_F(PredictiveAttentionFepBridgeTest, UpdateCallback) {
    ASSERT_NE(bridge, nullptr);

    // Call update callback directly - should fail without registration
    int result = pa_fep_update_callback(bridge);
    EXPECT_EQ(result, -1) << "Update callback should fail without registration";
}

/**
 * Test: ForceUpdate
 * Verify force update triggers FEP computation
 */
TEST_F(PredictiveAttentionFepBridgeTest, ForceUpdate) {
    ASSERT_NE(bridge, nullptr);

    // Get initial metrics
    pa_fep_metrics_t initial_metrics;
    int result = pa_fep_bridge_get_metrics(bridge, &initial_metrics);
    EXPECT_EQ(result, 0);

    // Force update
    result = pa_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Get updated metrics
    pa_fep_metrics_t updated_metrics;
    result = pa_fep_bridge_get_metrics(bridge, &updated_metrics);
    EXPECT_EQ(result, 0);

    // Update count should have increased
    EXPECT_GT(updated_metrics.update_count, initial_metrics.update_count)
        << "Update count should increase after force update";
}

/**
 * Test: ForceUpdateNull
 * Verify force update handles NULL gracefully
 */
TEST_F(PredictiveAttentionFepBridgeTest, ForceUpdateNull) {
    int result = pa_fep_bridge_force_update(nullptr);
    EXPECT_EQ(result, -1) << "Force update with NULL should fail";
}

/* ============================================================================
 * Free Energy Computation Tests
 * ============================================================================ */

/**
 * Test: FreeEnergyContribution
 * Verify free energy is calculated from predictive-attention metrics
 */
TEST_F(PredictiveAttentionFepBridgeTest, FreeEnergyContribution) {
    ASSERT_NE(bridge, nullptr);

    // Get initial free energy
    float initial_fe = pa_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(initial_fe, 0.0f) << "Free energy should be non-negative";
    EXPECT_LE(initial_fe, config.max_free_energy)
        << "Free energy should not exceed maximum";

    // Set low prediction accuracy (high error)
    pa_fep_bridge_update_prediction_accuracy(bridge, 0.2f);
    pa_fep_bridge_force_update(bridge);

    float updated_fe = pa_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(updated_fe, initial_fe)
        << "Lower prediction accuracy should increase free energy";
}

/**
 * Test: LowAccuracyIncreasesFreeEnergy
 * Verify that lower prediction accuracy leads to higher free energy
 */
TEST_F(PredictiveAttentionFepBridgeTest, LowAccuracyIncreasesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // High accuracy
    pa_fep_bridge_update_prediction_accuracy(bridge, 0.9f);
    pa_fep_bridge_force_update(bridge);
    float high_accuracy_fe = pa_fep_bridge_get_free_energy(bridge);

    // Reset and set low accuracy
    pa_fep_bridge_reset(bridge);
    pa_fep_bridge_update_prediction_accuracy(bridge, 0.1f);
    pa_fep_bridge_force_update(bridge);
    float low_accuracy_fe = pa_fep_bridge_get_free_energy(bridge);

    EXPECT_GT(low_accuracy_fe, high_accuracy_fe)
        << "Low prediction accuracy should produce higher free energy than high";
}

/**
 * Test: HighPrecisionReducesFreeEnergy
 * Verify that higher attention precision reduces free energy
 */
TEST_F(PredictiveAttentionFepBridgeTest, HighPrecisionReducesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Low precision
    pa_fep_bridge_update_attention_precision(bridge, 0.1f);
    pa_fep_bridge_force_update(bridge);
    float low_precision_fe = pa_fep_bridge_get_free_energy(bridge);

    // Reset and set high precision
    pa_fep_bridge_reset(bridge);
    pa_fep_bridge_update_attention_precision(bridge, 0.9f);
    pa_fep_bridge_force_update(bridge);
    float high_precision_fe = pa_fep_bridge_get_free_energy(bridge);

    EXPECT_LT(high_precision_fe, low_precision_fe)
        << "High attention precision should produce lower free energy";
}

/**
 * Test: ErrorSignalQualityAffectsFreeEnergy
 * Verify that error signal quality affects free energy
 */
TEST_F(PredictiveAttentionFepBridgeTest, ErrorSignalQualityAffectsFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Poor error signals
    pa_fep_bridge_update_error_signal_quality(bridge, 0.1f);
    pa_fep_bridge_force_update(bridge);
    float poor_quality_fe = pa_fep_bridge_get_free_energy(bridge);

    // Reset and set good error signals
    pa_fep_bridge_reset(bridge);
    pa_fep_bridge_update_error_signal_quality(bridge, 0.9f);
    pa_fep_bridge_force_update(bridge);
    float good_quality_fe = pa_fep_bridge_get_free_energy(bridge);

    EXPECT_LT(good_quality_fe, poor_quality_fe)
        << "Good error signal quality should produce lower free energy";
}

/**
 * Test: FreeEnergyNull
 * Verify get_free_energy handles NULL gracefully
 */
TEST_F(PredictiveAttentionFepBridgeTest, FreeEnergyNull) {
    float fe = pa_fep_bridge_get_free_energy(nullptr);
    EXPECT_LT(fe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: FreeEnergyBaseline
 * Verify baseline free energy is used appropriately
 */
TEST_F(PredictiveAttentionFepBridgeTest, FreeEnergyBaseline) {
    ASSERT_NE(bridge, nullptr);

    // With neutral values, free energy should be moderate
    float fe = pa_fep_bridge_get_free_energy(bridge);
    // Allow some tolerance around moderate free energy
    EXPECT_GE(fe, config.baseline_free_energy * 0.5f);
    EXPECT_LE(fe, config.max_free_energy);
}

/* ============================================================================
 * High Precision Mode Tests
 * ============================================================================ */

/**
 * Test: HighPrecisionModeDetection
 * Verify high precision mode is detected correctly
 */
TEST_F(PredictiveAttentionFepBridgeTest, HighPrecisionModeDetection) {
    ASSERT_NE(bridge, nullptr);

    // Initially should not be in high precision mode
    EXPECT_FALSE(pa_fep_bridge_is_high_precision(bridge))
        << "Should not be in high precision mode initially";

    // Set very high precision (above 1 - epsilon)
    pa_fep_bridge_update_attention_precision(bridge, 0.99f);
    pa_fep_bridge_force_update(bridge);

    EXPECT_TRUE(pa_fep_bridge_is_high_precision(bridge))
        << "Should be in high precision mode when precision > (1 - epsilon)";
}

/**
 * Test: HighPrecisionModeThreshold
 * Verify high precision mode threshold behavior
 */
TEST_F(PredictiveAttentionFepBridgeTest, HighPrecisionModeThreshold) {
    ASSERT_NE(bridge, nullptr);

    // Set precision just below threshold
    pa_fep_bridge_update_attention_precision(bridge, 0.9f);
    pa_fep_bridge_force_update(bridge);

    EXPECT_FALSE(pa_fep_bridge_is_high_precision(bridge))
        << "Should not be in high precision mode just below threshold";

    // Set precision above threshold
    pa_fep_bridge_update_attention_precision(bridge, 0.97f);
    pa_fep_bridge_force_update(bridge);

    EXPECT_TRUE(pa_fep_bridge_is_high_precision(bridge))
        << "Should be in high precision mode above threshold";
}

/* ============================================================================
 * Prediction Error Tests
 * ============================================================================ */

/**
 * Test: PredictionError
 * Verify prediction error tracking
 */
TEST_F(PredictiveAttentionFepBridgeTest, PredictionError) {
    ASSERT_NE(bridge, nullptr);

    // Get initial prediction error
    float initial_pe = pa_fep_bridge_get_prediction_error(bridge);
    EXPECT_GE(initial_pe, 0.0f) << "Prediction error should be non-negative";
    EXPECT_LE(initial_pe, 1.0f) << "Prediction error should be <= 1.0";
}

/**
 * Test: PredictionErrorNull
 * Verify get_prediction_error handles NULL gracefully
 */
TEST_F(PredictiveAttentionFepBridgeTest, PredictionErrorNull) {
    float pe = pa_fep_bridge_get_prediction_error(nullptr);
    EXPECT_LT(pe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: LowAccuracyIncreasesError
 * Verify low prediction accuracy increases prediction error
 */
TEST_F(PredictiveAttentionFepBridgeTest, LowAccuracyIncreasesError) {
    ASSERT_NE(bridge, nullptr);

    // Set low prediction accuracy
    pa_fep_bridge_update_prediction_accuracy(bridge, 0.1f);
    pa_fep_bridge_force_update(bridge);

    float pe = pa_fep_bridge_get_prediction_error(bridge);
    EXPECT_GT(pe, 0.0f) << "Low accuracy should produce prediction error > 0";
}

/* ============================================================================
 * Metrics Tracking Tests
 * ============================================================================ */

/**
 * Test: MetricsTracking
 * Verify metrics are updated correctly during FEP cycles
 */
TEST_F(PredictiveAttentionFepBridgeTest, MetricsTracking) {
    ASSERT_NE(bridge, nullptr);

    pa_fep_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    int result = pa_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(result, 0) << "Get metrics should succeed";

    // Verify core FEP metrics are valid
    EXPECT_GE(metrics.free_energy, 0.0f);
    EXPECT_GE(metrics.prediction_error, 0.0f);
    EXPECT_LE(metrics.prediction_error, 1.0f);
    EXPECT_GE(metrics.entropy, 0.0f);

    // Verify predictive-attention specific metrics are valid
    EXPECT_GE(metrics.prediction_accuracy, 0.0f);
    EXPECT_LE(metrics.prediction_accuracy, 1.0f);
    EXPECT_GE(metrics.attention_precision, 0.0f);
    EXPECT_LE(metrics.attention_precision, 1.0f);
    EXPECT_GE(metrics.error_signal_quality, 0.0f);
    EXPECT_LE(metrics.error_signal_quality, 1.0f);
}

/**
 * Test: MetricsTrackingNull
 * Verify get_metrics handles NULL parameters gracefully
 */
TEST_F(PredictiveAttentionFepBridgeTest, MetricsTrackingNull) {
    pa_fep_metrics_t metrics;

    int result = pa_fep_bridge_get_metrics(nullptr, &metrics);
    EXPECT_EQ(result, -1) << "Get metrics with NULL bridge should fail";

    result = pa_fep_bridge_get_metrics(bridge, nullptr);
    EXPECT_EQ(result, -1) << "Get metrics with NULL output should fail";
}

/**
 * Test: StatisticsTracking
 * Verify statistics are accumulated correctly
 */
TEST_F(PredictiveAttentionFepBridgeTest, StatisticsTracking) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        pa_fep_bridge_force_update(bridge);
    }

    pa_fep_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = pa_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify stats are accumulated
    EXPECT_GE(stats.total_updates, 5u) << "Should have at least 5 updates";
    EXPECT_GE(stats.avg_free_energy, 0.0f);
}

/**
 * Test: StatsReset
 * Verify statistics can be reset
 */
TEST_F(PredictiveAttentionFepBridgeTest, StatsReset) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        pa_fep_bridge_force_update(bridge);
    }

    // Reset stats
    int result = pa_fep_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    pa_fep_stats_t stats;
    result = pa_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    // Total updates should be reset to 0
    EXPECT_EQ(stats.total_updates, 0u) << "Updates should be reset to 0";
}

/* ============================================================================
 * State Query Tests
 * ============================================================================ */

/**
 * Test: GetState
 * Verify bridge state can be queried
 */
TEST_F(PredictiveAttentionFepBridgeTest, GetState) {
    ASSERT_NE(bridge, nullptr);

    pa_fep_state_t state = pa_fep_bridge_get_state(bridge);
    // State should be one of the valid states
    EXPECT_GE((int)state, (int)PA_FEP_STATE_UNINITIALIZED);
    EXPECT_LE((int)state, (int)PA_FEP_STATE_ERROR);
}

/**
 * Test: GetStateNull
 * Verify get_state handles NULL gracefully
 */
TEST_F(PredictiveAttentionFepBridgeTest, GetStateNull) {
    pa_fep_state_t state = pa_fep_bridge_get_state(nullptr);
    EXPECT_EQ(state, PA_FEP_STATE_ERROR)
        << "NULL bridge should return ERROR state";
}

/**
 * Test: IsDegraded
 * Verify degraded mode detection
 */
TEST_F(PredictiveAttentionFepBridgeTest, IsDegraded) {
    ASSERT_NE(bridge, nullptr);

    // Initially should not be degraded
    bool degraded = pa_fep_bridge_is_degraded(bridge);
    EXPECT_FALSE(degraded) << "Should not be degraded initially";
}

/**
 * Test: StateName
 * Verify state name conversion
 */
TEST_F(PredictiveAttentionFepBridgeTest, StateName) {
    const char* name = pa_fep_state_name(PA_FEP_STATE_IDLE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u) << "State name should not be empty";

    name = pa_fep_state_name(PA_FEP_STATE_ACTIVE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = pa_fep_state_name(PA_FEP_STATE_DEGRADED);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = pa_fep_state_name(PA_FEP_STATE_ERROR);
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
TEST_F(PredictiveAttentionFepBridgeTest, HighFECallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = pa_fep_bridge_set_high_fe_callback(
        bridge, test_high_fe_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = pa_fep_bridge_set_high_fe_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: SurpriseCallback
 * Verify surprise event callback registration
 */
TEST_F(PredictiveAttentionFepBridgeTest, SurpriseCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = pa_fep_bridge_set_surprise_callback(
        bridge, test_surprise_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = pa_fep_bridge_set_surprise_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: MetricsCallback
 * Verify metrics update callback registration
 */
TEST_F(PredictiveAttentionFepBridgeTest, MetricsCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = pa_fep_bridge_set_metrics_callback(
        bridge, test_metrics_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Force update to trigger callback
    pa_fep_bridge_force_update(bridge);

    // Metrics callback should have been called
    EXPECT_GE(g_metrics_callback_count.load(), 1)
        << "Metrics callback should be called on update";

    // Clear callback
    result = pa_fep_bridge_set_metrics_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: CallbackNull
 * Verify callback registration handles NULL bridge
 */
TEST_F(PredictiveAttentionFepBridgeTest, CallbackNull) {
    int result = pa_fep_bridge_set_high_fe_callback(
        nullptr, test_high_fe_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = pa_fep_bridge_set_surprise_callback(
        nullptr, test_surprise_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = pa_fep_bridge_set_metrics_callback(
        nullptr, test_metrics_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

/**
 * Test: SetConfig
 * Verify configuration can be updated
 */
TEST_F(PredictiveAttentionFepBridgeTest, SetConfig) {
    ASSERT_NE(bridge, nullptr);

    pa_fep_config_t new_config = pa_fep_config_default();
    new_config.prediction_accuracy_weight = 0.5f;
    new_config.attention_precision_weight = 0.3f;
    new_config.error_signal_quality_weight = 0.2f;

    int result = pa_fep_bridge_set_config(bridge, &new_config);
    EXPECT_EQ(result, 0) << "Set config should succeed";
}

/**
 * Test: SetConfigNull
 * Verify set_config handles NULL parameters
 */
TEST_F(PredictiveAttentionFepBridgeTest, SetConfigNull) {
    pa_fep_config_t config_val = pa_fep_config_default();

    int result = pa_fep_bridge_set_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = pa_fep_bridge_set_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL config should fail";
}

/**
 * Test: GetConfig
 * Verify configuration can be retrieved
 */
TEST_F(PredictiveAttentionFepBridgeTest, GetConfig) {
    ASSERT_NE(bridge, nullptr);

    pa_fep_config_t retrieved_config;
    memset(&retrieved_config, 0, sizeof(retrieved_config));

    int result = pa_fep_bridge_get_config(bridge, &retrieved_config);
    EXPECT_EQ(result, 0) << "Get config should succeed";

    // Verify retrieved config has valid values
    EXPECT_GT(retrieved_config.prediction_accuracy_weight, 0.0f);
    EXPECT_GT(retrieved_config.max_free_energy, 0.0f);
}

/**
 * Test: GetConfigNull
 * Verify get_config handles NULL parameters
 */
TEST_F(PredictiveAttentionFepBridgeTest, GetConfigNull) {
    pa_fep_config_t config_val;

    int result = pa_fep_bridge_get_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = pa_fep_bridge_get_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL output should fail";
}

/* ============================================================================
 * Manual Update Operations Tests
 * ============================================================================ */

/**
 * Test: UpdatePredictionAccuracy
 * Verify manual prediction accuracy update
 */
TEST_F(PredictiveAttentionFepBridgeTest, UpdatePredictionAccuracy) {
    ASSERT_NE(bridge, nullptr);

    int result = pa_fep_bridge_update_prediction_accuracy(bridge, 0.75f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    float pa = pa_fep_bridge_get_prediction_accuracy(bridge);
    EXPECT_FLOAT_EQ(pa, 0.75f) << "Value should be updated";
}

/**
 * Test: UpdateAttentionPrecision
 * Verify manual attention precision update
 */
TEST_F(PredictiveAttentionFepBridgeTest, UpdateAttentionPrecision) {
    ASSERT_NE(bridge, nullptr);

    int result = pa_fep_bridge_update_attention_precision(bridge, 0.65f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    float ap = pa_fep_bridge_get_attention_precision(bridge);
    EXPECT_FLOAT_EQ(ap, 0.65f) << "Value should be updated";
}

/**
 * Test: UpdateErrorSignalQuality
 * Verify manual error signal quality update
 */
TEST_F(PredictiveAttentionFepBridgeTest, UpdateErrorSignalQuality) {
    ASSERT_NE(bridge, nullptr);

    int result = pa_fep_bridge_update_error_signal_quality(bridge, 0.85f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    pa_fep_metrics_t metrics;
    pa_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_FLOAT_EQ(metrics.error_signal_quality, 0.85f) << "Value should be updated";
}

/**
 * Test: UpdateClamps
 * Verify updates are clamped to valid range
 */
TEST_F(PredictiveAttentionFepBridgeTest, UpdateClamps) {
    ASSERT_NE(bridge, nullptr);

    // Test clamping above 1.0
    pa_fep_bridge_update_prediction_accuracy(bridge, 2.0f);
    float pa = pa_fep_bridge_get_prediction_accuracy(bridge);
    EXPECT_LE(pa, 1.0f) << "Should be clamped to max 1.0";

    // Test clamping below 0.0
    pa_fep_bridge_update_prediction_accuracy(bridge, -0.5f);
    pa = pa_fep_bridge_get_prediction_accuracy(bridge);
    EXPECT_GE(pa, 0.0f) << "Should be clamped to min 0.0";
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Comprehensive test for NULL parameter handling across all functions
 */
TEST_F(PredictiveAttentionFepBridgeTest, NullHandling) {
    // Lifecycle
    pa_fep_bridge_destroy(nullptr);

    // Registration
    EXPECT_EQ(pa_fep_bridge_reset(nullptr), -1);
    EXPECT_FALSE(pa_fep_bridge_is_registered(nullptr));
    EXPECT_EQ(pa_fep_bridge_get_id(nullptr), 0u);
    EXPECT_EQ(pa_fep_bridge_unregister(nullptr), -1);

    // Updates
    EXPECT_EQ(pa_fep_update_callback(nullptr), -1);
    EXPECT_EQ(pa_fep_bridge_force_update(nullptr), -1);
    EXPECT_EQ(pa_fep_bridge_update_prediction_accuracy(nullptr, 0.5f), -1);
    EXPECT_EQ(pa_fep_bridge_update_attention_precision(nullptr, 0.5f), -1);
    EXPECT_EQ(pa_fep_bridge_update_error_signal_quality(nullptr, 0.5f), -1);

    // Metrics
    pa_fep_metrics_t metrics;
    EXPECT_EQ(pa_fep_bridge_get_metrics(nullptr, &metrics), -1);
    EXPECT_EQ(pa_fep_bridge_get_metrics(bridge, nullptr), -1);

    pa_fep_stats_t stats;
    EXPECT_EQ(pa_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(pa_fep_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(pa_fep_bridge_reset_stats(nullptr), -1);

    // State queries
    EXPECT_EQ(pa_fep_bridge_get_state(nullptr), PA_FEP_STATE_ERROR);
    EXPECT_LT(pa_fep_bridge_get_free_energy(nullptr), 0.0f);
    EXPECT_LT(pa_fep_bridge_get_prediction_error(nullptr), 0.0f);
    EXPECT_LT(pa_fep_bridge_get_prediction_accuracy(nullptr), 0.0f);
    EXPECT_LT(pa_fep_bridge_get_attention_precision(nullptr), 0.0f);
    EXPECT_FALSE(pa_fep_bridge_is_degraded(nullptr));
    EXPECT_FALSE(pa_fep_bridge_is_high_precision(nullptr));

    // Callbacks
    EXPECT_EQ(pa_fep_bridge_set_high_fe_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(pa_fep_bridge_set_surprise_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(pa_fep_bridge_set_metrics_callback(nullptr, nullptr, nullptr), -1);

    // Config
    pa_fep_config_t cfg;
    EXPECT_EQ(pa_fep_bridge_set_config(nullptr, &cfg), -1);
    EXPECT_EQ(pa_fep_bridge_set_config(bridge, nullptr), -1);
    EXPECT_EQ(pa_fep_bridge_get_config(nullptr, &cfg), -1);
    EXPECT_EQ(pa_fep_bridge_get_config(bridge, nullptr), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/* ============================================================================
 * Error Conditions Tests
 * ============================================================================ */

/**
 * Test: ErrorConditions
 * Verify proper error handling for various edge cases
 */
TEST_F(PredictiveAttentionFepBridgeTest, ErrorConditions) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should succeed (no-op)
    int result = pa_fep_bridge_unregister(bridge);
    EXPECT_EQ(result, 0) << "Unregister when not registered should be no-op";

    // Double destroy should be safe (after first destroy, pointer is invalid
    // but we test with different bridges)
    pa_fep_bridge_t* temp_bridge = pa_fep_bridge_create(nullptr);
    ASSERT_NE(temp_bridge, nullptr);
    pa_fep_bridge_destroy(temp_bridge);
    // temp_bridge is now invalid, but destroy(nullptr) should be safe
    pa_fep_bridge_destroy(nullptr);

    SUCCEED() << "Error condition tests passed";
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Verify basic thread safety with concurrent access
 */
TEST_F(PredictiveAttentionFepBridgeTest, ThreadSafety) {
    ASSERT_NE(bridge, nullptr);

    const int NUM_THREADS = 4;
    const int ITERATIONS = 100;
    std::atomic<int> completed{0};
    std::vector<std::thread> threads;

    // Create threads that concurrently access the bridge
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &completed, ITERATIONS]() {
            for (int i = 0; i < ITERATIONS; i++) {
                // Mix of read and write operations
                pa_fep_bridge_get_free_energy(bridge);
                pa_fep_bridge_get_prediction_error(bridge);
                pa_fep_bridge_get_prediction_accuracy(bridge);
                pa_fep_bridge_get_attention_precision(bridge);
                pa_fep_bridge_get_state(bridge);
                pa_fep_bridge_is_degraded(bridge);
                pa_fep_bridge_is_high_precision(bridge);

                pa_fep_metrics_t metrics;
                pa_fep_bridge_get_metrics(bridge, &metrics);

                pa_fep_stats_t stats;
                pa_fep_bridge_get_stats(bridge, &stats);

                // Write operations
                pa_fep_bridge_update_prediction_accuracy(bridge,
                    (float)(i % 10) / 10.0f);
                pa_fep_bridge_update_attention_precision(bridge,
                    (float)((i + 5) % 10) / 10.0f);
                pa_fep_bridge_force_update(bridge);
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
TEST_F(PredictiveAttentionFepBridgeTest, DestroyCallback) {
    ASSERT_NE(bridge, nullptr);

    // Destroy callback should be safe to call
    pa_fep_destroy_callback(bridge);
    SUCCEED() << "Destroy callback should not crash";

    // Should also handle NULL
    pa_fep_destroy_callback(nullptr);
    SUCCEED() << "Destroy callback with NULL should not crash";
}

/* ============================================================================
 * Integration Scenario Tests
 * ============================================================================ */

/**
 * Test: PredictiveAttentionScenario
 * Simulate a predictive-attention processing scenario
 */
TEST_F(PredictiveAttentionFepBridgeTest, PredictiveAttentionScenario) {
    ASSERT_NE(bridge, nullptr);

    // Register metrics callback to track changes
    pa_fep_bridge_set_metrics_callback(bridge, test_metrics_callback, nullptr);

    // Scenario: Start with poor predictions, improve through attention optimization

    // Phase 1: Initial poor prediction state
    pa_fep_bridge_update_prediction_accuracy(bridge, 0.2f);
    pa_fep_bridge_update_attention_precision(bridge, 0.3f);
    pa_fep_bridge_update_error_signal_quality(bridge, 0.4f);
    pa_fep_bridge_force_update(bridge);

    float initial_fe = pa_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(initial_fe, config.baseline_free_energy)
        << "Poor predictions should produce elevated free energy";

    // Phase 2: Improving attention precision
    pa_fep_bridge_update_attention_precision(bridge, 0.7f);
    pa_fep_bridge_force_update(bridge);

    float improved_precision_fe = pa_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(improved_precision_fe, initial_fe)
        << "Better attention precision should reduce free energy";

    // Phase 3: Prediction accuracy improving
    pa_fep_bridge_update_prediction_accuracy(bridge, 0.6f);
    pa_fep_bridge_force_update(bridge);

    float improved_accuracy_fe = pa_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(improved_accuracy_fe, improved_precision_fe)
        << "Better prediction accuracy should reduce free energy";

    // Phase 4: Error signals become informative
    pa_fep_bridge_update_error_signal_quality(bridge, 0.9f);
    pa_fep_bridge_force_update(bridge);

    float good_errors_fe = pa_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(good_errors_fe, improved_accuracy_fe)
        << "Better error signals should reduce free energy";

    // Phase 5: High precision mode achieved
    pa_fep_bridge_update_prediction_accuracy(bridge, 0.9f);
    pa_fep_bridge_update_attention_precision(bridge, 0.97f);
    pa_fep_bridge_force_update(bridge);

    EXPECT_TRUE(pa_fep_bridge_is_high_precision(bridge))
        << "Should achieve high precision mode";

    float final_fe = pa_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(final_fe, good_errors_fe)
        << "Optimal state should have lowest free energy";

    // Verify callback was called for each update
    EXPECT_GE(g_metrics_callback_count.load(), 5)
        << "Metrics callback should be called for each update";
}

/**
 * Test: DegradedModeTransition
 * Verify transitions into and out of degraded mode
 */
TEST_F(PredictiveAttentionFepBridgeTest, DegradedModeTransition) {
    ASSERT_NE(bridge, nullptr);

    // Set up high FE callback
    pa_fep_bridge_set_high_fe_callback(bridge, test_high_fe_callback, nullptr);

    // Initially not degraded
    EXPECT_FALSE(pa_fep_bridge_is_degraded(bridge));

    // Create high free energy situation
    pa_fep_bridge_update_prediction_accuracy(bridge, 0.05f);
    pa_fep_bridge_update_attention_precision(bridge, 0.05f);
    pa_fep_bridge_update_error_signal_quality(bridge, 0.05f);
    pa_fep_bridge_force_update(bridge);

    // Check if degraded (depends on threshold)
    float fe = pa_fep_bridge_get_free_energy(bridge);
    if (fe > config.high_free_energy_threshold) {
        EXPECT_TRUE(pa_fep_bridge_is_degraded(bridge))
            << "Should be degraded when FE exceeds threshold";
        EXPECT_GE(g_high_fe_callback_count.load(), 1)
            << "High FE callback should have been called";
    }

    // Recover by improving metrics
    pa_fep_bridge_update_prediction_accuracy(bridge, 0.9f);
    pa_fep_bridge_update_attention_precision(bridge, 0.9f);
    pa_fep_bridge_update_error_signal_quality(bridge, 0.9f);
    pa_fep_bridge_force_update(bridge);

    // Should recover from degraded mode
    EXPECT_FALSE(pa_fep_bridge_is_degraded(bridge))
        << "Should recover from degraded mode with good metrics";
}

/**
 * Test: CombinedMetricsEffect
 * Verify all three metrics contribute to free energy
 */
TEST_F(PredictiveAttentionFepBridgeTest, CombinedMetricsEffect) {
    ASSERT_NE(bridge, nullptr);

    // All metrics poor
    pa_fep_bridge_update_prediction_accuracy(bridge, 0.1f);
    pa_fep_bridge_update_attention_precision(bridge, 0.1f);
    pa_fep_bridge_update_error_signal_quality(bridge, 0.1f);
    pa_fep_bridge_force_update(bridge);
    float all_poor_fe = pa_fep_bridge_get_free_energy(bridge);

    // Only prediction good
    pa_fep_bridge_reset(bridge);
    pa_fep_bridge_update_prediction_accuracy(bridge, 0.9f);
    pa_fep_bridge_update_attention_precision(bridge, 0.1f);
    pa_fep_bridge_update_error_signal_quality(bridge, 0.1f);
    pa_fep_bridge_force_update(bridge);
    float pred_good_fe = pa_fep_bridge_get_free_energy(bridge);

    EXPECT_LT(pred_good_fe, all_poor_fe)
        << "Good prediction should reduce FE even with poor other metrics";

    // All metrics good
    pa_fep_bridge_reset(bridge);
    pa_fep_bridge_update_prediction_accuracy(bridge, 0.9f);
    pa_fep_bridge_update_attention_precision(bridge, 0.9f);
    pa_fep_bridge_update_error_signal_quality(bridge, 0.9f);
    pa_fep_bridge_force_update(bridge);
    float all_good_fe = pa_fep_bridge_get_free_energy(bridge);

    EXPECT_LT(all_good_fe, pred_good_fe)
        << "All good metrics should produce lowest FE";
}
