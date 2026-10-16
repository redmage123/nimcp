/**
 * @file test_salience_attention_fep_bridge.cpp
 * @brief Unit tests for Salience-Attention FEP Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for FEP-Salience-Attention bidirectional integration
 * WHY:  Ensure free energy computation from attention allocation works correctly
 * HOW:  Test lifecycle, registration, metrics, callbacks, and FEP update cycle
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Configuration Validation
 * - FEP Registration/Unregistration
 * - Update Callback Mechanism
 * - Free Energy Computation
 * - Salience Prediction Error Effects
 * - Attention Efficiency Detection
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

#include "cognitive/integration/nimcp_salience_attention_fep_bridge.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_high_fe_callback_count{0};
static std::atomic<int> g_surprise_callback_count{0};
static std::atomic<int> g_metrics_callback_count{0};
static float g_last_free_energy = 0.0f;
static float g_last_surprise = 0.0f;
static std::string g_last_surprise_source;
static sa_fep_metrics_t g_last_metrics;

/**
 * Test callback for high free energy events
 */
static void test_high_fe_callback(
    sa_fep_bridge_t* bridge,
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
    sa_fep_bridge_t* bridge,
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
    sa_fep_bridge_t* bridge,
    const sa_fep_metrics_t* metrics,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    g_metrics_callback_count++;
    if (metrics != nullptr) {
        memcpy(&g_last_metrics, metrics, sizeof(sa_fep_metrics_t));
    }
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SalienceAttentionFepBridgeTest : public ::testing::Test {
protected:
    sa_fep_bridge_t* bridge = nullptr;
    sa_fep_config_t config;

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
        config = sa_fep_config_default();
        bridge = sa_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            sa_fep_bridge_destroy(bridge);
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
TEST_F(SalienceAttentionFepBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify initial state
    sa_fep_state_t state = sa_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, SA_FEP_STATE_IDLE) << "Initial state should be IDLE";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(SalienceAttentionFepBridgeTest, BridgeCreationNullConfig) {
    sa_fep_bridge_t* br = sa_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    sa_fep_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(SalienceAttentionFepBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    sa_fep_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    sa_fep_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(SalienceAttentionFepBridgeTest, DefaultConfig) {
    sa_fep_config_t default_config = sa_fep_config_default();

    // Verify weights are positive
    EXPECT_GT(default_config.free_energy_weight, 0.0f);
    EXPECT_GT(default_config.salience_prediction_weight, 0.0f);
    EXPECT_GT(default_config.attention_allocation_weight, 0.0f);
    EXPECT_GT(default_config.priority_estimation_weight, 0.0f);

    // Verify thresholds are sensible
    EXPECT_GT(default_config.high_free_energy_threshold, 0.0f);
    EXPECT_GT(default_config.prediction_error_threshold, 0.0f);
    EXPECT_GT(default_config.attention_efficiency_threshold, 0.0f);

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
TEST_F(SalienceAttentionFepBridgeTest, ConfigValidation) {
    sa_fep_config_t custom_config = sa_fep_config_default();
    custom_config.salience_prediction_weight = 0.5f;
    custom_config.attention_allocation_weight = 0.3f;
    custom_config.priority_estimation_weight = 0.2f;
    custom_config.high_free_energy_threshold = 1.8f;

    sa_fep_bridge_t* custom_bridge = sa_fep_bridge_create(&custom_config);
    ASSERT_NE(custom_bridge, nullptr);

    sa_fep_config_t retrieved_config;
    int result = sa_fep_bridge_get_config(custom_bridge, &retrieved_config);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(retrieved_config.salience_prediction_weight, 0.5f);
    EXPECT_FLOAT_EQ(retrieved_config.attention_allocation_weight, 0.3f);
    EXPECT_FLOAT_EQ(retrieved_config.priority_estimation_weight, 0.2f);
    EXPECT_FLOAT_EQ(retrieved_config.high_free_energy_threshold, 1.8f);

    sa_fep_bridge_destroy(custom_bridge);
}

/**
 * Test: BridgeReset
 * Verify bridge can be reset to initial state
 */
TEST_F(SalienceAttentionFepBridgeTest, BridgeReset) {
    ASSERT_NE(bridge, nullptr);

    // Force an update to change state
    sa_fep_bridge_update_salience_error(bridge, 0.8f);
    int result = sa_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Reset bridge
    result = sa_fep_bridge_reset(bridge);
    EXPECT_EQ(result, 0) << "Reset should succeed";

    // Verify state is back to IDLE
    sa_fep_state_t state = sa_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, SA_FEP_STATE_IDLE) << "State should be IDLE after reset";

    // Verify metrics are reset
    float se = sa_fep_bridge_get_salience_error(bridge);
    EXPECT_FLOAT_EQ(se, 0.0f) << "Salience error should be reset";
}

/* ============================================================================
 * FEP Registration Tests
 * ============================================================================ */

/**
 * Test: RegisterWithFEP
 * Verify bridge registration state without actual orchestrator
 */
TEST_F(SalienceAttentionFepBridgeTest, RegisterWithFEP) {
    ASSERT_NE(bridge, nullptr);

    // Before registration, should not be registered
    EXPECT_FALSE(sa_fep_bridge_is_registered(bridge))
        << "Should not be registered initially";

    // Without actual orchestrator, ID should be 0
    uint32_t id = sa_fep_bridge_get_id(bridge);
    EXPECT_EQ(id, 0u) << "ID should be 0 when not registered";
}

/**
 * Test: UnregisterFromFEP
 * Verify bridge can unregister cleanly
 */
TEST_F(SalienceAttentionFepBridgeTest, UnregisterFromFEP) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should handle gracefully
    int result = sa_fep_bridge_unregister(bridge);
    EXPECT_EQ(result, 0) << "Unregister when not registered should succeed";

    EXPECT_FALSE(sa_fep_bridge_is_registered(bridge))
        << "Should not be registered after unregister";
}

/**
 * Test: RegistrationNullParams
 * Verify registration handles NULL parameters gracefully
 */
TEST_F(SalienceAttentionFepBridgeTest, RegistrationNullParams) {
    ASSERT_NE(bridge, nullptr);

    uint32_t bridge_id = 0;

    // NULL bridge
    int result = sa_fep_bridge_register(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1) << "Register with NULL bridge should fail";

    // NULL orchestrator
    result = sa_fep_bridge_register(bridge, nullptr, nullptr, &bridge_id);
    EXPECT_EQ(result, -1) << "Register without orchestrator should fail";
}

/* ============================================================================
 * FEP Update Callback Tests
 * ============================================================================ */

/**
 * Test: UpdateCallback
 * Verify FEP update callback behavior
 */
TEST_F(SalienceAttentionFepBridgeTest, UpdateCallback) {
    ASSERT_NE(bridge, nullptr);

    // Call update callback directly - should fail without registration
    int result = sa_fep_update_callback(bridge);
    EXPECT_EQ(result, -1) << "Update callback should fail without registration";
}

/**
 * Test: ForceUpdate
 * Verify force update triggers FEP computation
 */
TEST_F(SalienceAttentionFepBridgeTest, ForceUpdate) {
    ASSERT_NE(bridge, nullptr);

    // Get initial metrics
    sa_fep_metrics_t initial_metrics;
    int result = sa_fep_bridge_get_metrics(bridge, &initial_metrics);
    EXPECT_EQ(result, 0);

    // Force update
    result = sa_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Get updated metrics
    sa_fep_metrics_t updated_metrics;
    result = sa_fep_bridge_get_metrics(bridge, &updated_metrics);
    EXPECT_EQ(result, 0);

    // Update count should have increased
    EXPECT_GT(updated_metrics.update_count, initial_metrics.update_count)
        << "Update count should increase after force update";
}

/**
 * Test: ForceUpdateNull
 * Verify force update handles NULL gracefully
 */
TEST_F(SalienceAttentionFepBridgeTest, ForceUpdateNull) {
    int result = sa_fep_bridge_force_update(nullptr);
    EXPECT_EQ(result, -1) << "Force update with NULL should fail";
}

/* ============================================================================
 * Free Energy Computation Tests
 * ============================================================================ */

/**
 * Test: FreeEnergyContribution
 * Verify free energy is calculated from salience-attention metrics
 */
TEST_F(SalienceAttentionFepBridgeTest, FreeEnergyContribution) {
    ASSERT_NE(bridge, nullptr);

    // Get initial free energy
    float initial_fe = sa_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(initial_fe, 0.0f) << "Free energy should be non-negative";
    EXPECT_LE(initial_fe, config.max_free_energy)
        << "Free energy should not exceed maximum";

    // Set high salience prediction error
    sa_fep_bridge_update_salience_error(bridge, 0.8f);
    sa_fep_bridge_force_update(bridge);

    float updated_fe = sa_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(updated_fe, initial_fe)
        << "Higher salience error should increase free energy";
}

/**
 * Test: SalienceErrorIncreasesFreeEnergy
 * Verify that higher salience prediction error leads to higher free energy
 */
TEST_F(SalienceAttentionFepBridgeTest, SalienceErrorIncreasesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Low error
    sa_fep_bridge_update_salience_error(bridge, 0.1f);
    sa_fep_bridge_force_update(bridge);
    float low_fe = sa_fep_bridge_get_free_energy(bridge);

    // Reset and set high error
    sa_fep_bridge_reset(bridge);
    sa_fep_bridge_update_salience_error(bridge, 0.9f);
    sa_fep_bridge_force_update(bridge);
    float high_fe = sa_fep_bridge_get_free_energy(bridge);

    EXPECT_GT(high_fe, low_fe)
        << "High salience error should produce higher free energy than low";
}

/**
 * Test: AttentionEfficiencyReducesFreeEnergy
 * Verify that efficient attention reduces free energy
 */
TEST_F(SalienceAttentionFepBridgeTest, AttentionEfficiencyReducesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // High attention allocation error (inefficient)
    sa_fep_bridge_update_attention_error(bridge, 0.9f);
    sa_fep_bridge_force_update(bridge);
    float inefficient_fe = sa_fep_bridge_get_free_energy(bridge);

    // Reset and set low error (efficient)
    sa_fep_bridge_reset(bridge);
    sa_fep_bridge_update_attention_error(bridge, 0.1f);
    sa_fep_bridge_force_update(bridge);
    float efficient_fe = sa_fep_bridge_get_free_energy(bridge);

    EXPECT_LT(efficient_fe, inefficient_fe)
        << "Efficient attention allocation should produce lower free energy";
}

/**
 * Test: PriorityErrorIncreasesFreeEnergy
 * Verify that priority estimation error increases free energy
 */
TEST_F(SalienceAttentionFepBridgeTest, PriorityErrorIncreasesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Low priority error
    sa_fep_bridge_update_priority_error(bridge, 0.1f);
    sa_fep_bridge_force_update(bridge);
    float low_fe = sa_fep_bridge_get_free_energy(bridge);

    // Reset and set high error
    sa_fep_bridge_reset(bridge);
    sa_fep_bridge_update_priority_error(bridge, 0.9f);
    sa_fep_bridge_force_update(bridge);
    float high_fe = sa_fep_bridge_get_free_energy(bridge);

    EXPECT_GT(high_fe, low_fe)
        << "High priority error should produce higher free energy";
}

/**
 * Test: FreeEnergyNull
 * Verify get_free_energy handles NULL gracefully
 */
TEST_F(SalienceAttentionFepBridgeTest, FreeEnergyNull) {
    float fe = sa_fep_bridge_get_free_energy(nullptr);
    EXPECT_LT(fe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: FreeEnergyBaseline
 * Verify baseline free energy is used when idle
 */
TEST_F(SalienceAttentionFepBridgeTest, FreeEnergyBaseline) {
    ASSERT_NE(bridge, nullptr);

    // In idle state with no errors, free energy should be near baseline
    sa_fep_bridge_update_salience_error(bridge, 0.0f);
    sa_fep_bridge_update_attention_error(bridge, 0.0f);
    sa_fep_bridge_update_priority_error(bridge, 0.0f);
    sa_fep_bridge_force_update(bridge);

    float fe = sa_fep_bridge_get_free_energy(bridge);
    // Allow some tolerance
    EXPECT_GE(fe, config.baseline_free_energy * 0.5f);
    EXPECT_LE(fe, config.baseline_free_energy * 3.0f);
}

/* ============================================================================
 * Attention Efficiency Tests
 * ============================================================================ */

/**
 * Test: AttentionEfficiencyDetection
 * Verify attention efficiency state is detected correctly
 */
TEST_F(SalienceAttentionFepBridgeTest, AttentionEfficiencyDetection) {
    ASSERT_NE(bridge, nullptr);

    // Initially at neutral efficiency
    float initial_eff = sa_fep_bridge_get_attention_efficiency(bridge);
    EXPECT_GE(initial_eff, 0.0f);
    EXPECT_LE(initial_eff, 1.0f);

    // Set high efficiency
    sa_fep_bridge_update_attention_efficiency(bridge, 0.95f);
    sa_fep_bridge_force_update(bridge);

    EXPECT_TRUE(sa_fep_bridge_is_efficient(bridge))
        << "Should be efficient when efficiency > threshold";

    // Set low efficiency
    sa_fep_bridge_update_attention_efficiency(bridge, 0.3f);
    sa_fep_bridge_force_update(bridge);

    EXPECT_FALSE(sa_fep_bridge_is_efficient(bridge))
        << "Should not be efficient when efficiency < threshold";
}

/* ============================================================================
 * Prediction Error Tests
 * ============================================================================ */

/**
 * Test: PredictionError
 * Verify prediction error tracking
 */
TEST_F(SalienceAttentionFepBridgeTest, PredictionError) {
    ASSERT_NE(bridge, nullptr);

    // Get initial prediction error
    float initial_pe = sa_fep_bridge_get_prediction_error(bridge);
    EXPECT_GE(initial_pe, 0.0f) << "Prediction error should be non-negative";
    EXPECT_LE(initial_pe, 1.0f) << "Prediction error should be <= 1.0";
}

/**
 * Test: PredictionErrorNull
 * Verify get_prediction_error handles NULL gracefully
 */
TEST_F(SalienceAttentionFepBridgeTest, PredictionErrorNull) {
    float pe = sa_fep_bridge_get_prediction_error(nullptr);
    EXPECT_LT(pe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: SalienceErrorAffectsPrediction
 * Verify salience prediction error contributes to prediction error
 */
TEST_F(SalienceAttentionFepBridgeTest, SalienceErrorAffectsPrediction) {
    ASSERT_NE(bridge, nullptr);

    // Set high salience error
    sa_fep_bridge_update_salience_error(bridge, 0.9f);
    sa_fep_bridge_force_update(bridge);

    float pe = sa_fep_bridge_get_prediction_error(bridge);
    EXPECT_GT(pe, 0.0f) << "High salience error should produce prediction error";
}

/* ============================================================================
 * Metrics Tracking Tests
 * ============================================================================ */

/**
 * Test: MetricsTracking
 * Verify metrics are updated correctly during FEP cycles
 */
TEST_F(SalienceAttentionFepBridgeTest, MetricsTracking) {
    ASSERT_NE(bridge, nullptr);

    sa_fep_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    int result = sa_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(result, 0) << "Get metrics should succeed";

    // Verify core FEP metrics are valid
    EXPECT_GE(metrics.free_energy, 0.0f);
    EXPECT_GE(metrics.prediction_error, 0.0f);
    EXPECT_LE(metrics.prediction_error, 1.0f);
    EXPECT_GE(metrics.entropy, 0.0f);

    // Verify salience-attention specific metrics are valid
    EXPECT_GE(metrics.salience_prediction_error, 0.0f);
    EXPECT_LE(metrics.salience_prediction_error, 1.0f);
    EXPECT_GE(metrics.attention_allocation_error, 0.0f);
    EXPECT_LE(metrics.attention_allocation_error, 1.0f);
    EXPECT_GE(metrics.priority_estimation_error, 0.0f);
    EXPECT_LE(metrics.priority_estimation_error, 1.0f);
    EXPECT_GE(metrics.attention_efficiency, 0.0f);
    EXPECT_LE(metrics.attention_efficiency, 1.0f);
}

/**
 * Test: MetricsTrackingNull
 * Verify get_metrics handles NULL parameters gracefully
 */
TEST_F(SalienceAttentionFepBridgeTest, MetricsTrackingNull) {
    sa_fep_metrics_t metrics;

    int result = sa_fep_bridge_get_metrics(nullptr, &metrics);
    EXPECT_EQ(result, -1) << "Get metrics with NULL bridge should fail";

    result = sa_fep_bridge_get_metrics(bridge, nullptr);
    EXPECT_EQ(result, -1) << "Get metrics with NULL output should fail";
}

/**
 * Test: StatisticsTracking
 * Verify statistics are accumulated correctly
 */
TEST_F(SalienceAttentionFepBridgeTest, StatisticsTracking) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        sa_fep_bridge_force_update(bridge);
    }

    sa_fep_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = sa_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify stats are accumulated
    EXPECT_GE(stats.total_updates, 5u) << "Should have at least 5 updates";
    EXPECT_GE(stats.avg_free_energy, 0.0f);
}

/**
 * Test: StatsReset
 * Verify statistics can be reset
 */
TEST_F(SalienceAttentionFepBridgeTest, StatsReset) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        sa_fep_bridge_force_update(bridge);
    }

    // Reset stats
    int result = sa_fep_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    sa_fep_stats_t stats;
    result = sa_fep_bridge_get_stats(bridge, &stats);
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
TEST_F(SalienceAttentionFepBridgeTest, GetState) {
    ASSERT_NE(bridge, nullptr);

    sa_fep_state_t state = sa_fep_bridge_get_state(bridge);
    // State should be one of the valid states
    EXPECT_GE((int)state, (int)SA_FEP_STATE_UNINITIALIZED);
    EXPECT_LE((int)state, (int)SA_FEP_STATE_ERROR);
}

/**
 * Test: GetStateNull
 * Verify get_state handles NULL gracefully
 */
TEST_F(SalienceAttentionFepBridgeTest, GetStateNull) {
    sa_fep_state_t state = sa_fep_bridge_get_state(nullptr);
    EXPECT_EQ(state, SA_FEP_STATE_ERROR)
        << "NULL bridge should return ERROR state";
}

/**
 * Test: IsDegraded
 * Verify degraded mode detection
 */
TEST_F(SalienceAttentionFepBridgeTest, IsDegraded) {
    ASSERT_NE(bridge, nullptr);

    // Initially should not be degraded
    bool degraded = sa_fep_bridge_is_degraded(bridge);
    EXPECT_FALSE(degraded) << "Should not be degraded initially";
}

/**
 * Test: StateName
 * Verify state name conversion
 */
TEST_F(SalienceAttentionFepBridgeTest, StateName) {
    const char* name = sa_fep_state_name(SA_FEP_STATE_IDLE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u) << "State name should not be empty";

    name = sa_fep_state_name(SA_FEP_STATE_ACTIVE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = sa_fep_state_name(SA_FEP_STATE_DEGRADED);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = sa_fep_state_name(SA_FEP_STATE_ERROR);
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
TEST_F(SalienceAttentionFepBridgeTest, HighFECallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = sa_fep_bridge_set_high_fe_callback(
        bridge, test_high_fe_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = sa_fep_bridge_set_high_fe_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: SurpriseCallback
 * Verify surprise event callback registration
 */
TEST_F(SalienceAttentionFepBridgeTest, SurpriseCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = sa_fep_bridge_set_surprise_callback(
        bridge, test_surprise_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = sa_fep_bridge_set_surprise_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: MetricsCallback
 * Verify metrics update callback registration
 */
TEST_F(SalienceAttentionFepBridgeTest, MetricsCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = sa_fep_bridge_set_metrics_callback(
        bridge, test_metrics_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Force update to trigger callback
    sa_fep_bridge_force_update(bridge);

    // Metrics callback should have been called
    EXPECT_GE(g_metrics_callback_count.load(), 1)
        << "Metrics callback should be called on update";

    // Clear callback
    result = sa_fep_bridge_set_metrics_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: CallbackNull
 * Verify callback registration handles NULL bridge
 */
TEST_F(SalienceAttentionFepBridgeTest, CallbackNull) {
    int result = sa_fep_bridge_set_high_fe_callback(
        nullptr, test_high_fe_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = sa_fep_bridge_set_surprise_callback(
        nullptr, test_surprise_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = sa_fep_bridge_set_metrics_callback(
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
TEST_F(SalienceAttentionFepBridgeTest, SetConfig) {
    ASSERT_NE(bridge, nullptr);

    sa_fep_config_t new_config = sa_fep_config_default();
    new_config.salience_prediction_weight = 0.5f;
    new_config.attention_allocation_weight = 0.3f;
    new_config.priority_estimation_weight = 0.2f;

    int result = sa_fep_bridge_set_config(bridge, &new_config);
    EXPECT_EQ(result, 0) << "Set config should succeed";
}

/**
 * Test: SetConfigNull
 * Verify set_config handles NULL parameters
 */
TEST_F(SalienceAttentionFepBridgeTest, SetConfigNull) {
    sa_fep_config_t config_val = sa_fep_config_default();

    int result = sa_fep_bridge_set_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = sa_fep_bridge_set_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL config should fail";
}

/**
 * Test: GetConfig
 * Verify configuration can be retrieved
 */
TEST_F(SalienceAttentionFepBridgeTest, GetConfig) {
    ASSERT_NE(bridge, nullptr);

    sa_fep_config_t retrieved_config;
    memset(&retrieved_config, 0, sizeof(retrieved_config));

    int result = sa_fep_bridge_get_config(bridge, &retrieved_config);
    EXPECT_EQ(result, 0) << "Get config should succeed";

    // Verify retrieved config has valid values
    EXPECT_GT(retrieved_config.salience_prediction_weight, 0.0f);
    EXPECT_GT(retrieved_config.max_free_energy, 0.0f);
}

/**
 * Test: GetConfigNull
 * Verify get_config handles NULL parameters
 */
TEST_F(SalienceAttentionFepBridgeTest, GetConfigNull) {
    sa_fep_config_t config_val;

    int result = sa_fep_bridge_get_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = sa_fep_bridge_get_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL output should fail";
}

/* ============================================================================
 * Manual Update Operations Tests
 * ============================================================================ */

/**
 * Test: UpdateSalienceError
 * Verify manual salience error update
 */
TEST_F(SalienceAttentionFepBridgeTest, UpdateSalienceError) {
    ASSERT_NE(bridge, nullptr);

    int result = sa_fep_bridge_update_salience_error(bridge, 0.75f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    float se = sa_fep_bridge_get_salience_error(bridge);
    EXPECT_FLOAT_EQ(se, 0.75f) << "Value should be updated";
}

/**
 * Test: UpdateAttentionError
 * Verify manual attention error update
 */
TEST_F(SalienceAttentionFepBridgeTest, UpdateAttentionError) {
    ASSERT_NE(bridge, nullptr);

    int result = sa_fep_bridge_update_attention_error(bridge, 0.65f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    sa_fep_metrics_t metrics;
    sa_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_FLOAT_EQ(metrics.attention_allocation_error, 0.65f)
        << "Value should be updated";
}

/**
 * Test: UpdatePriorityError
 * Verify manual priority error update
 */
TEST_F(SalienceAttentionFepBridgeTest, UpdatePriorityError) {
    ASSERT_NE(bridge, nullptr);

    int result = sa_fep_bridge_update_priority_error(bridge, 0.25f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    sa_fep_metrics_t metrics;
    sa_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_FLOAT_EQ(metrics.priority_estimation_error, 0.25f)
        << "Value should be updated";
}

/**
 * Test: UpdateAttentionEfficiency
 * Verify manual attention efficiency update
 */
TEST_F(SalienceAttentionFepBridgeTest, UpdateAttentionEfficiency) {
    ASSERT_NE(bridge, nullptr);

    int result = sa_fep_bridge_update_attention_efficiency(bridge, 0.85f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    float eff = sa_fep_bridge_get_attention_efficiency(bridge);
    EXPECT_FLOAT_EQ(eff, 0.85f) << "Value should be updated";
}

/**
 * Test: UpdateClamps
 * Verify updates are clamped to valid range
 */
TEST_F(SalienceAttentionFepBridgeTest, UpdateClamps) {
    ASSERT_NE(bridge, nullptr);

    // Test clamping above 1.0
    sa_fep_bridge_update_salience_error(bridge, 2.0f);
    float se = sa_fep_bridge_get_salience_error(bridge);
    EXPECT_LE(se, 1.0f) << "Should be clamped to max 1.0";

    // Test clamping below 0.0
    sa_fep_bridge_update_salience_error(bridge, -0.5f);
    se = sa_fep_bridge_get_salience_error(bridge);
    EXPECT_GE(se, 0.0f) << "Should be clamped to min 0.0";

    // Test efficiency clamping
    sa_fep_bridge_update_attention_efficiency(bridge, 1.5f);
    float eff = sa_fep_bridge_get_attention_efficiency(bridge);
    EXPECT_LE(eff, 1.0f) << "Efficiency should be clamped to max 1.0";
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Comprehensive test for NULL parameter handling across all functions
 */
TEST_F(SalienceAttentionFepBridgeTest, NullHandling) {
    // Lifecycle
    sa_fep_bridge_destroy(nullptr);

    // Registration
    EXPECT_EQ(sa_fep_bridge_reset(nullptr), -1);
    EXPECT_FALSE(sa_fep_bridge_is_registered(nullptr));
    EXPECT_EQ(sa_fep_bridge_get_id(nullptr), 0u);
    EXPECT_EQ(sa_fep_bridge_unregister(nullptr), -1);

    // Updates
    EXPECT_EQ(sa_fep_update_callback(nullptr), -1);
    EXPECT_EQ(sa_fep_bridge_force_update(nullptr), -1);
    EXPECT_EQ(sa_fep_bridge_update_salience_error(nullptr, 0.5f), -1);
    EXPECT_EQ(sa_fep_bridge_update_attention_error(nullptr, 0.5f), -1);
    EXPECT_EQ(sa_fep_bridge_update_priority_error(nullptr, 0.5f), -1);
    EXPECT_EQ(sa_fep_bridge_update_attention_efficiency(nullptr, 0.5f), -1);

    // Metrics
    sa_fep_metrics_t metrics;
    EXPECT_EQ(sa_fep_bridge_get_metrics(nullptr, &metrics), -1);
    EXPECT_EQ(sa_fep_bridge_get_metrics(bridge, nullptr), -1);

    sa_fep_stats_t stats;
    EXPECT_EQ(sa_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(sa_fep_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(sa_fep_bridge_reset_stats(nullptr), -1);

    // State queries
    EXPECT_EQ(sa_fep_bridge_get_state(nullptr), SA_FEP_STATE_ERROR);
    EXPECT_LT(sa_fep_bridge_get_free_energy(nullptr), 0.0f);
    EXPECT_LT(sa_fep_bridge_get_prediction_error(nullptr), 0.0f);
    EXPECT_LT(sa_fep_bridge_get_salience_error(nullptr), 0.0f);
    EXPECT_LT(sa_fep_bridge_get_attention_efficiency(nullptr), 0.0f);
    EXPECT_FALSE(sa_fep_bridge_is_degraded(nullptr));
    EXPECT_FALSE(sa_fep_bridge_is_efficient(nullptr));

    // Callbacks
    EXPECT_EQ(sa_fep_bridge_set_high_fe_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(sa_fep_bridge_set_surprise_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(sa_fep_bridge_set_metrics_callback(nullptr, nullptr, nullptr), -1);

    // Config
    sa_fep_config_t cfg;
    EXPECT_EQ(sa_fep_bridge_set_config(nullptr, &cfg), -1);
    EXPECT_EQ(sa_fep_bridge_set_config(bridge, nullptr), -1);
    EXPECT_EQ(sa_fep_bridge_get_config(nullptr, &cfg), -1);
    EXPECT_EQ(sa_fep_bridge_get_config(bridge, nullptr), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/* ============================================================================
 * Error Conditions Tests
 * ============================================================================ */

/**
 * Test: ErrorConditions
 * Verify proper error handling for various edge cases
 */
TEST_F(SalienceAttentionFepBridgeTest, ErrorConditions) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should succeed (no-op)
    int result = sa_fep_bridge_unregister(bridge);
    EXPECT_EQ(result, 0) << "Unregister when not registered should be no-op";

    // Double destroy should be safe (after first destroy, pointer is invalid
    // but we test with different bridges)
    sa_fep_bridge_t* temp_bridge = sa_fep_bridge_create(nullptr);
    ASSERT_NE(temp_bridge, nullptr);
    sa_fep_bridge_destroy(temp_bridge);
    // temp_bridge is now invalid, but destroy(nullptr) should be safe
    sa_fep_bridge_destroy(nullptr);

    SUCCEED() << "Error condition tests passed";
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Verify basic thread safety with concurrent access
 */
TEST_F(SalienceAttentionFepBridgeTest, ThreadSafety) {
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
                sa_fep_bridge_get_free_energy(bridge);
                sa_fep_bridge_get_prediction_error(bridge);
                sa_fep_bridge_get_salience_error(bridge);
                sa_fep_bridge_get_attention_efficiency(bridge);
                sa_fep_bridge_get_state(bridge);
                sa_fep_bridge_is_degraded(bridge);
                sa_fep_bridge_is_efficient(bridge);

                sa_fep_metrics_t metrics;
                sa_fep_bridge_get_metrics(bridge, &metrics);

                sa_fep_stats_t stats;
                sa_fep_bridge_get_stats(bridge, &stats);

                // Write operations
                sa_fep_bridge_update_salience_error(bridge,
                    (float)(i % 10) / 10.0f);
                sa_fep_bridge_update_attention_efficiency(bridge,
                    (float)((i + 5) % 10) / 10.0f);
                sa_fep_bridge_force_update(bridge);
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
TEST_F(SalienceAttentionFepBridgeTest, DestroyCallback) {
    ASSERT_NE(bridge, nullptr);

    // Destroy callback should be safe to call
    sa_fep_destroy_callback(bridge);
    SUCCEED() << "Destroy callback should not crash";

    // Should also handle NULL
    sa_fep_destroy_callback(nullptr);
    SUCCEED() << "Destroy callback with NULL should not crash";
}

/* ============================================================================
 * Integration Scenario Tests
 * ============================================================================ */

/**
 * Test: AttentionAllocationScenario
 * Simulate an attention allocation scenario
 */
TEST_F(SalienceAttentionFepBridgeTest, AttentionAllocationScenario) {
    ASSERT_NE(bridge, nullptr);

    // Register metrics callback to track changes
    sa_fep_bridge_set_metrics_callback(bridge, test_metrics_callback, nullptr);

    // Scenario: Start with poor attention, gradually improve allocation

    // Phase 1: Initial high error state
    sa_fep_bridge_update_salience_error(bridge, 0.9f);
    sa_fep_bridge_update_attention_error(bridge, 0.8f);
    sa_fep_bridge_update_priority_error(bridge, 0.7f);
    sa_fep_bridge_update_attention_efficiency(bridge, 0.3f);
    sa_fep_bridge_force_update(bridge);

    float initial_fe = sa_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(initial_fe, config.baseline_free_energy)
        << "High errors should produce elevated free energy";

    // Phase 2: Improving salience prediction
    sa_fep_bridge_update_salience_error(bridge, 0.4f);
    sa_fep_bridge_force_update(bridge);

    float learning_fe = sa_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(learning_fe, initial_fe)
        << "Better salience prediction should reduce free energy";

    // Phase 3: Attention allocation improvement
    sa_fep_bridge_update_attention_error(bridge, 0.3f);
    sa_fep_bridge_update_attention_efficiency(bridge, 0.7f);
    sa_fep_bridge_force_update(bridge);

    float improved_fe = sa_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(improved_fe, learning_fe)
        << "Better attention allocation should reduce free energy";

    // Phase 4: Priority estimation improvement
    sa_fep_bridge_update_priority_error(bridge, 0.1f);
    sa_fep_bridge_force_update(bridge);

    float final_fe = sa_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(final_fe, improved_fe)
        << "Better priority estimation should have lowest free energy";

    // Phase 5: Efficient state achieved
    sa_fep_bridge_update_attention_efficiency(bridge, 0.95f);
    sa_fep_bridge_force_update(bridge);

    EXPECT_TRUE(sa_fep_bridge_is_efficient(bridge))
        << "Should detect efficient attention allocation";

    // Verify callback was called for each update
    EXPECT_GE(g_metrics_callback_count.load(), 5)
        << "Metrics callback should be called for each update";
}

/**
 * Test: HighLoadScenario
 * Simulate a high cognitive load scenario with degradation
 */
TEST_F(SalienceAttentionFepBridgeTest, HighLoadScenario) {
    ASSERT_NE(bridge, nullptr);

    // Register high FE callback
    sa_fep_bridge_set_high_fe_callback(bridge, test_high_fe_callback, nullptr);

    // Set very high errors to trigger degraded mode
    sa_fep_bridge_update_salience_error(bridge, 0.95f);
    sa_fep_bridge_update_attention_error(bridge, 0.95f);
    sa_fep_bridge_update_priority_error(bridge, 0.95f);
    sa_fep_bridge_force_update(bridge);

    float high_fe = sa_fep_bridge_get_free_energy(bridge);

    // Check if we entered degraded mode (depends on threshold)
    if (high_fe > config.high_free_energy_threshold) {
        EXPECT_TRUE(sa_fep_bridge_is_degraded(bridge))
            << "Should be in degraded mode with high errors";
        EXPECT_GE(g_high_fe_callback_count.load(), 1)
            << "High FE callback should be called";
    }

    // Recovery: reduce errors
    sa_fep_bridge_update_salience_error(bridge, 0.2f);
    sa_fep_bridge_update_attention_error(bridge, 0.2f);
    sa_fep_bridge_update_priority_error(bridge, 0.2f);
    sa_fep_bridge_force_update(bridge);

    float recovered_fe = sa_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(recovered_fe, high_fe)
        << "Free energy should decrease after recovery";

    if (recovered_fe < config.high_free_energy_threshold) {
        EXPECT_FALSE(sa_fep_bridge_is_degraded(bridge))
            << "Should exit degraded mode after recovery";
    }
}

/**
 * Test: SurpriseEventScenario
 * Test surprise events from sudden changes
 */
TEST_F(SalienceAttentionFepBridgeTest, SurpriseEventScenario) {
    ASSERT_NE(bridge, nullptr);

    // Register surprise callback
    sa_fep_bridge_set_surprise_callback(bridge, test_surprise_callback, nullptr);

    // Start at low error state
    sa_fep_bridge_update_salience_error(bridge, 0.1f);
    sa_fep_bridge_update_attention_error(bridge, 0.1f);
    sa_fep_bridge_force_update(bridge);

    // Sudden spike in salience error (unexpected salient event)
    sa_fep_bridge_update_salience_error(bridge, 0.95f);
    sa_fep_bridge_force_update(bridge);

    // Check that surprise was registered
    sa_fep_metrics_t metrics;
    sa_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_GT(metrics.surprise, 0.0f)
        << "Sudden change should produce surprise";
}
