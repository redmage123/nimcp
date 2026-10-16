/**
 * @file test_game_theory_fep_bridge.cpp
 * @brief Unit tests for Game Theory FEP Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for FEP-Game Theory bidirectional integration
 * WHY:  Ensure free energy computation from strategic reasoning works correctly
 * HOW:  Test lifecycle, registration, metrics, callbacks, and FEP update cycle
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Configuration Validation
 * - FEP Registration/Unregistration
 * - Update Callback Mechanism
 * - Free Energy Computation
 * - Strategy Uncertainty Effects
 * - Nash Equilibrium Detection
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

#include "cognitive/game_theory/nimcp_game_theory_fep_bridge.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_high_fe_callback_count{0};
static std::atomic<int> g_surprise_callback_count{0};
static std::atomic<int> g_metrics_callback_count{0};
static float g_last_free_energy = 0.0f;
static float g_last_surprise = 0.0f;
static std::string g_last_surprise_source;
static gt_fep_metrics_t g_last_metrics;

/**
 * Test callback for high free energy events
 */
static void test_high_fe_callback(
    gt_fep_bridge_t* bridge,
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
    gt_fep_bridge_t* bridge,
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
    gt_fep_bridge_t* bridge,
    const gt_fep_metrics_t* metrics,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    g_metrics_callback_count++;
    if (metrics != nullptr) {
        memcpy(&g_last_metrics, metrics, sizeof(gt_fep_metrics_t));
    }
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class GameTheoryFepBridgeTest : public ::testing::Test {
protected:
    gt_fep_bridge_t* bridge = nullptr;
    gt_fep_config_t config;

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
        config = gt_fep_config_default();
        bridge = gt_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            gt_fep_bridge_destroy(bridge);
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
TEST_F(GameTheoryFepBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify initial state
    gt_fep_state_t state = gt_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, GT_FEP_STATE_IDLE) << "Initial state should be IDLE";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(GameTheoryFepBridgeTest, BridgeCreationNullConfig) {
    gt_fep_bridge_t* br = gt_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    gt_fep_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(GameTheoryFepBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    gt_fep_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    gt_fep_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(GameTheoryFepBridgeTest, DefaultConfig) {
    gt_fep_config_t default_config = gt_fep_config_default();

    // Verify weights are positive
    EXPECT_GT(default_config.free_energy_weight, 0.0f);
    EXPECT_GT(default_config.strategy_uncertainty_weight, 0.0f);
    EXPECT_GT(default_config.opponent_modeling_weight, 0.0f);
    EXPECT_GT(default_config.nash_convergence_weight, 0.0f);

    // Verify thresholds are sensible
    EXPECT_GT(default_config.high_free_energy_threshold, 0.0f);
    EXPECT_GT(default_config.prediction_error_threshold, 0.0f);
    EXPECT_GT(default_config.nash_epsilon, 0.0f);

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
TEST_F(GameTheoryFepBridgeTest, ConfigValidation) {
    gt_fep_config_t custom_config = gt_fep_config_default();
    custom_config.strategy_uncertainty_weight = 0.5f;
    custom_config.opponent_modeling_weight = 0.3f;
    custom_config.nash_convergence_weight = 0.2f;
    custom_config.high_free_energy_threshold = 1.8f;

    gt_fep_bridge_t* custom_bridge = gt_fep_bridge_create(&custom_config);
    ASSERT_NE(custom_bridge, nullptr);

    gt_fep_config_t retrieved_config;
    int result = gt_fep_bridge_get_config(custom_bridge, &retrieved_config);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(retrieved_config.strategy_uncertainty_weight, 0.5f);
    EXPECT_FLOAT_EQ(retrieved_config.opponent_modeling_weight, 0.3f);
    EXPECT_FLOAT_EQ(retrieved_config.nash_convergence_weight, 0.2f);
    EXPECT_FLOAT_EQ(retrieved_config.high_free_energy_threshold, 1.8f);

    gt_fep_bridge_destroy(custom_bridge);
}

/**
 * Test: BridgeReset
 * Verify bridge can be reset to initial state
 */
TEST_F(GameTheoryFepBridgeTest, BridgeReset) {
    ASSERT_NE(bridge, nullptr);

    // Force an update to change state
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.8f);
    int result = gt_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Reset bridge
    result = gt_fep_bridge_reset(bridge);
    EXPECT_EQ(result, 0) << "Reset should succeed";

    // Verify state is back to IDLE
    gt_fep_state_t state = gt_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, GT_FEP_STATE_IDLE) << "State should be IDLE after reset";

    // Verify metrics are reset
    float su = gt_fep_bridge_get_strategy_uncertainty(bridge);
    EXPECT_FLOAT_EQ(su, 0.0f) << "Strategy uncertainty should be reset";
}

/* ============================================================================
 * FEP Registration Tests
 * ============================================================================ */

/**
 * Test: RegisterWithFEP
 * Verify bridge registration state without actual orchestrator
 */
TEST_F(GameTheoryFepBridgeTest, RegisterWithFEP) {
    ASSERT_NE(bridge, nullptr);

    // Before registration, should not be registered
    EXPECT_FALSE(gt_fep_bridge_is_registered(bridge))
        << "Should not be registered initially";

    // Without actual orchestrator, ID should be 0
    uint32_t id = gt_fep_bridge_get_id(bridge);
    EXPECT_EQ(id, 0u) << "ID should be 0 when not registered";
}

/**
 * Test: UnregisterFromFEP
 * Verify bridge can unregister cleanly
 */
TEST_F(GameTheoryFepBridgeTest, UnregisterFromFEP) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should handle gracefully
    int result = gt_fep_bridge_unregister(bridge);
    EXPECT_EQ(result, 0) << "Unregister when not registered should succeed";

    EXPECT_FALSE(gt_fep_bridge_is_registered(bridge))
        << "Should not be registered after unregister";
}

/**
 * Test: RegistrationNullParams
 * Verify registration handles NULL parameters gracefully
 */
TEST_F(GameTheoryFepBridgeTest, RegistrationNullParams) {
    ASSERT_NE(bridge, nullptr);

    uint32_t bridge_id = 0;

    // NULL bridge
    int result = gt_fep_bridge_register(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1) << "Register with NULL bridge should fail";

    // NULL orchestrator
    result = gt_fep_bridge_register(bridge, nullptr, nullptr, &bridge_id);
    EXPECT_EQ(result, -1) << "Register without orchestrator should fail";
}

/* ============================================================================
 * FEP Update Callback Tests
 * ============================================================================ */

/**
 * Test: UpdateCallback
 * Verify FEP update callback behavior
 */
TEST_F(GameTheoryFepBridgeTest, UpdateCallback) {
    ASSERT_NE(bridge, nullptr);

    // Call update callback directly - should fail without registration
    int result = gt_fep_update_callback(bridge);
    EXPECT_EQ(result, -1) << "Update callback should fail without registration";
}

/**
 * Test: ForceUpdate
 * Verify force update triggers FEP computation
 */
TEST_F(GameTheoryFepBridgeTest, ForceUpdate) {
    ASSERT_NE(bridge, nullptr);

    // Get initial metrics
    gt_fep_metrics_t initial_metrics;
    int result = gt_fep_bridge_get_metrics(bridge, &initial_metrics);
    EXPECT_EQ(result, 0);

    // Force update
    result = gt_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Get updated metrics
    gt_fep_metrics_t updated_metrics;
    result = gt_fep_bridge_get_metrics(bridge, &updated_metrics);
    EXPECT_EQ(result, 0);

    // Update count should have increased
    EXPECT_GT(updated_metrics.update_count, initial_metrics.update_count)
        << "Update count should increase after force update";
}

/**
 * Test: ForceUpdateNull
 * Verify force update handles NULL gracefully
 */
TEST_F(GameTheoryFepBridgeTest, ForceUpdateNull) {
    int result = gt_fep_bridge_force_update(nullptr);
    EXPECT_EQ(result, -1) << "Force update with NULL should fail";
}

/* ============================================================================
 * Free Energy Computation Tests
 * ============================================================================ */

/**
 * Test: FreeEnergyContribution
 * Verify free energy is calculated from game theory metrics
 */
TEST_F(GameTheoryFepBridgeTest, FreeEnergyContribution) {
    ASSERT_NE(bridge, nullptr);

    // Get initial free energy
    float initial_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(initial_fe, 0.0f) << "Free energy should be non-negative";
    EXPECT_LE(initial_fe, config.max_free_energy)
        << "Free energy should not exceed maximum";

    // Set high strategy uncertainty
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.8f);
    gt_fep_bridge_force_update(bridge);

    float updated_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(updated_fe, initial_fe)
        << "Higher strategy uncertainty should increase free energy";
}

/**
 * Test: StrategyUncertaintyIncreasesFreeEnergy
 * Verify that higher strategy uncertainty leads to higher free energy
 */
TEST_F(GameTheoryFepBridgeTest, StrategyUncertaintyIncreasesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Low uncertainty
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.1f);
    gt_fep_bridge_force_update(bridge);
    float low_fe = gt_fep_bridge_get_free_energy(bridge);

    // Reset and set high uncertainty
    gt_fep_bridge_reset(bridge);
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.9f);
    gt_fep_bridge_force_update(bridge);
    float high_fe = gt_fep_bridge_get_free_energy(bridge);

    EXPECT_GT(high_fe, low_fe)
        << "High strategy uncertainty should produce higher free energy than low";
}

/**
 * Test: NashEquilibriumReducesFreeEnergy
 * Verify that approaching Nash equilibrium reduces free energy
 */
TEST_F(GameTheoryFepBridgeTest, NashEquilibriumReducesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Far from Nash equilibrium
    gt_fep_bridge_update_nash_distance(bridge, 0.9f);
    gt_fep_bridge_force_update(bridge);
    float far_fe = gt_fep_bridge_get_free_energy(bridge);

    // Reset and set close to Nash equilibrium
    gt_fep_bridge_reset(bridge);
    gt_fep_bridge_update_nash_distance(bridge, 0.1f);
    gt_fep_bridge_force_update(bridge);
    float close_fe = gt_fep_bridge_get_free_energy(bridge);

    EXPECT_LT(close_fe, far_fe)
        << "Being close to Nash equilibrium should produce lower free energy";
}

/**
 * Test: FreeEnergyNull
 * Verify get_free_energy handles NULL gracefully
 */
TEST_F(GameTheoryFepBridgeTest, FreeEnergyNull) {
    float fe = gt_fep_bridge_get_free_energy(nullptr);
    EXPECT_LT(fe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: FreeEnergyBaseline
 * Verify baseline free energy is used when idle
 */
TEST_F(GameTheoryFepBridgeTest, FreeEnergyBaseline) {
    ASSERT_NE(bridge, nullptr);

    // In idle state with no uncertainty, free energy should be near baseline
    float fe = gt_fep_bridge_get_free_energy(bridge);
    // Allow some tolerance
    EXPECT_GE(fe, config.baseline_free_energy * 0.5f);
    EXPECT_LE(fe, config.baseline_free_energy * 3.0f);
}

/* ============================================================================
 * Nash Equilibrium Tests
 * ============================================================================ */

/**
 * Test: NashEquilibriumDetection
 * Verify Nash equilibrium state is detected correctly
 */
TEST_F(GameTheoryFepBridgeTest, NashEquilibriumDetection) {
    ASSERT_NE(bridge, nullptr);

    // Initially not at Nash
    EXPECT_FALSE(gt_fep_bridge_is_at_nash(bridge))
        << "Should not be at Nash equilibrium initially";

    // Set very close to Nash (below epsilon)
    gt_fep_bridge_update_nash_distance(bridge, 0.001f);
    gt_fep_bridge_force_update(bridge);

    EXPECT_TRUE(gt_fep_bridge_is_at_nash(bridge))
        << "Should be at Nash equilibrium when distance < epsilon";
}

/* ============================================================================
 * Prediction Error Tests
 * ============================================================================ */

/**
 * Test: PredictionError
 * Verify prediction error tracking
 */
TEST_F(GameTheoryFepBridgeTest, PredictionError) {
    ASSERT_NE(bridge, nullptr);

    // Get initial prediction error
    float initial_pe = gt_fep_bridge_get_prediction_error(bridge);
    EXPECT_GE(initial_pe, 0.0f) << "Prediction error should be non-negative";
    EXPECT_LE(initial_pe, 1.0f) << "Prediction error should be <= 1.0";
}

/**
 * Test: PredictionErrorNull
 * Verify get_prediction_error handles NULL gracefully
 */
TEST_F(GameTheoryFepBridgeTest, PredictionErrorNull) {
    float pe = gt_fep_bridge_get_prediction_error(nullptr);
    EXPECT_LT(pe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: OpponentErrorAffectsPrediction
 * Verify opponent modeling error contributes to prediction error
 */
TEST_F(GameTheoryFepBridgeTest, OpponentErrorAffectsPrediction) {
    ASSERT_NE(bridge, nullptr);

    // Set high opponent error
    gt_fep_bridge_update_opponent_error(bridge, 0.9f);
    gt_fep_bridge_force_update(bridge);

    float pe = gt_fep_bridge_get_prediction_error(bridge);
    EXPECT_GT(pe, 0.0f) << "High opponent error should produce prediction error";
}

/* ============================================================================
 * Metrics Tracking Tests
 * ============================================================================ */

/**
 * Test: MetricsTracking
 * Verify metrics are updated correctly during FEP cycles
 */
TEST_F(GameTheoryFepBridgeTest, MetricsTracking) {
    ASSERT_NE(bridge, nullptr);

    gt_fep_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    int result = gt_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(result, 0) << "Get metrics should succeed";

    // Verify core FEP metrics are valid
    EXPECT_GE(metrics.free_energy, 0.0f);
    EXPECT_GE(metrics.prediction_error, 0.0f);
    EXPECT_LE(metrics.prediction_error, 1.0f);
    EXPECT_GE(metrics.entropy, 0.0f);

    // Verify game theory specific metrics are valid
    EXPECT_GE(metrics.strategy_uncertainty, 0.0f);
    EXPECT_LE(metrics.strategy_uncertainty, 1.0f);
    EXPECT_GE(metrics.opponent_prediction_error, 0.0f);
    EXPECT_LE(metrics.opponent_prediction_error, 1.0f);
    EXPECT_GE(metrics.nash_distance, 0.0f);
    EXPECT_LE(metrics.nash_distance, 1.0f);
}

/**
 * Test: MetricsTrackingNull
 * Verify get_metrics handles NULL parameters gracefully
 */
TEST_F(GameTheoryFepBridgeTest, MetricsTrackingNull) {
    gt_fep_metrics_t metrics;

    int result = gt_fep_bridge_get_metrics(nullptr, &metrics);
    EXPECT_EQ(result, -1) << "Get metrics with NULL bridge should fail";

    result = gt_fep_bridge_get_metrics(bridge, nullptr);
    EXPECT_EQ(result, -1) << "Get metrics with NULL output should fail";
}

/**
 * Test: StatisticsTracking
 * Verify statistics are accumulated correctly
 */
TEST_F(GameTheoryFepBridgeTest, StatisticsTracking) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        gt_fep_bridge_force_update(bridge);
    }

    gt_fep_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = gt_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify stats are accumulated
    EXPECT_GE(stats.total_updates, 5u) << "Should have at least 5 updates";
    EXPECT_GE(stats.avg_free_energy, 0.0f);
}

/**
 * Test: StatsReset
 * Verify statistics can be reset
 */
TEST_F(GameTheoryFepBridgeTest, StatsReset) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        gt_fep_bridge_force_update(bridge);
    }

    // Reset stats
    int result = gt_fep_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    gt_fep_stats_t stats;
    result = gt_fep_bridge_get_stats(bridge, &stats);
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
TEST_F(GameTheoryFepBridgeTest, GetState) {
    ASSERT_NE(bridge, nullptr);

    gt_fep_state_t state = gt_fep_bridge_get_state(bridge);
    // State should be one of the valid states
    EXPECT_GE((int)state, (int)GT_FEP_STATE_UNINITIALIZED);
    EXPECT_LE((int)state, (int)GT_FEP_STATE_ERROR);
}

/**
 * Test: GetStateNull
 * Verify get_state handles NULL gracefully
 */
TEST_F(GameTheoryFepBridgeTest, GetStateNull) {
    gt_fep_state_t state = gt_fep_bridge_get_state(nullptr);
    EXPECT_EQ(state, GT_FEP_STATE_ERROR)
        << "NULL bridge should return ERROR state";
}

/**
 * Test: IsDegraded
 * Verify degraded mode detection
 */
TEST_F(GameTheoryFepBridgeTest, IsDegraded) {
    ASSERT_NE(bridge, nullptr);

    // Initially should not be degraded
    bool degraded = gt_fep_bridge_is_degraded(bridge);
    EXPECT_FALSE(degraded) << "Should not be degraded initially";
}

/**
 * Test: StateName
 * Verify state name conversion
 */
TEST_F(GameTheoryFepBridgeTest, StateName) {
    const char* name = gt_fep_state_name(GT_FEP_STATE_IDLE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u) << "State name should not be empty";

    name = gt_fep_state_name(GT_FEP_STATE_ACTIVE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = gt_fep_state_name(GT_FEP_STATE_DEGRADED);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = gt_fep_state_name(GT_FEP_STATE_ERROR);
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
TEST_F(GameTheoryFepBridgeTest, HighFECallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = gt_fep_bridge_set_high_fe_callback(
        bridge, test_high_fe_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = gt_fep_bridge_set_high_fe_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: SurpriseCallback
 * Verify surprise event callback registration
 */
TEST_F(GameTheoryFepBridgeTest, SurpriseCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = gt_fep_bridge_set_surprise_callback(
        bridge, test_surprise_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = gt_fep_bridge_set_surprise_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: MetricsCallback
 * Verify metrics update callback registration
 */
TEST_F(GameTheoryFepBridgeTest, MetricsCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = gt_fep_bridge_set_metrics_callback(
        bridge, test_metrics_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Force update to trigger callback
    gt_fep_bridge_force_update(bridge);

    // Metrics callback should have been called
    EXPECT_GE(g_metrics_callback_count.load(), 1)
        << "Metrics callback should be called on update";

    // Clear callback
    result = gt_fep_bridge_set_metrics_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: CallbackNull
 * Verify callback registration handles NULL bridge
 */
TEST_F(GameTheoryFepBridgeTest, CallbackNull) {
    int result = gt_fep_bridge_set_high_fe_callback(
        nullptr, test_high_fe_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = gt_fep_bridge_set_surprise_callback(
        nullptr, test_surprise_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = gt_fep_bridge_set_metrics_callback(
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
TEST_F(GameTheoryFepBridgeTest, SetConfig) {
    ASSERT_NE(bridge, nullptr);

    gt_fep_config_t new_config = gt_fep_config_default();
    new_config.strategy_uncertainty_weight = 0.5f;
    new_config.opponent_modeling_weight = 0.3f;
    new_config.nash_convergence_weight = 0.2f;

    int result = gt_fep_bridge_set_config(bridge, &new_config);
    EXPECT_EQ(result, 0) << "Set config should succeed";
}

/**
 * Test: SetConfigNull
 * Verify set_config handles NULL parameters
 */
TEST_F(GameTheoryFepBridgeTest, SetConfigNull) {
    gt_fep_config_t config_val = gt_fep_config_default();

    int result = gt_fep_bridge_set_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = gt_fep_bridge_set_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL config should fail";
}

/**
 * Test: GetConfig
 * Verify configuration can be retrieved
 */
TEST_F(GameTheoryFepBridgeTest, GetConfig) {
    ASSERT_NE(bridge, nullptr);

    gt_fep_config_t retrieved_config;
    memset(&retrieved_config, 0, sizeof(retrieved_config));

    int result = gt_fep_bridge_get_config(bridge, &retrieved_config);
    EXPECT_EQ(result, 0) << "Get config should succeed";

    // Verify retrieved config has valid values
    EXPECT_GT(retrieved_config.strategy_uncertainty_weight, 0.0f);
    EXPECT_GT(retrieved_config.max_free_energy, 0.0f);
}

/**
 * Test: GetConfigNull
 * Verify get_config handles NULL parameters
 */
TEST_F(GameTheoryFepBridgeTest, GetConfigNull) {
    gt_fep_config_t config_val;

    int result = gt_fep_bridge_get_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = gt_fep_bridge_get_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL output should fail";
}

/* ============================================================================
 * Manual Update Operations Tests
 * ============================================================================ */

/**
 * Test: UpdateStrategyUncertainty
 * Verify manual strategy uncertainty update
 */
TEST_F(GameTheoryFepBridgeTest, UpdateStrategyUncertainty) {
    ASSERT_NE(bridge, nullptr);

    int result = gt_fep_bridge_update_strategy_uncertainty(bridge, 0.75f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    float su = gt_fep_bridge_get_strategy_uncertainty(bridge);
    EXPECT_FLOAT_EQ(su, 0.75f) << "Value should be updated";
}

/**
 * Test: UpdateOpponentError
 * Verify manual opponent error update
 */
TEST_F(GameTheoryFepBridgeTest, UpdateOpponentError) {
    ASSERT_NE(bridge, nullptr);

    int result = gt_fep_bridge_update_opponent_error(bridge, 0.65f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    gt_fep_metrics_t metrics;
    gt_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_FLOAT_EQ(metrics.opponent_prediction_error, 0.65f)
        << "Value should be updated";
}

/**
 * Test: UpdateNashDistance
 * Verify manual Nash distance update
 */
TEST_F(GameTheoryFepBridgeTest, UpdateNashDistance) {
    ASSERT_NE(bridge, nullptr);

    int result = gt_fep_bridge_update_nash_distance(bridge, 0.25f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    gt_fep_metrics_t metrics;
    gt_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_FLOAT_EQ(metrics.nash_distance, 0.25f) << "Value should be updated";
}

/**
 * Test: UpdateClamps
 * Verify updates are clamped to valid range
 */
TEST_F(GameTheoryFepBridgeTest, UpdateClamps) {
    ASSERT_NE(bridge, nullptr);

    // Test clamping above 1.0
    gt_fep_bridge_update_strategy_uncertainty(bridge, 2.0f);
    float su = gt_fep_bridge_get_strategy_uncertainty(bridge);
    EXPECT_LE(su, 1.0f) << "Should be clamped to max 1.0";

    // Test clamping below 0.0
    gt_fep_bridge_update_strategy_uncertainty(bridge, -0.5f);
    su = gt_fep_bridge_get_strategy_uncertainty(bridge);
    EXPECT_GE(su, 0.0f) << "Should be clamped to min 0.0";
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Comprehensive test for NULL parameter handling across all functions
 */
TEST_F(GameTheoryFepBridgeTest, NullHandling) {
    // Lifecycle
    gt_fep_bridge_destroy(nullptr);

    // Registration
    EXPECT_EQ(gt_fep_bridge_reset(nullptr), -1);
    EXPECT_FALSE(gt_fep_bridge_is_registered(nullptr));
    EXPECT_EQ(gt_fep_bridge_get_id(nullptr), 0u);
    EXPECT_EQ(gt_fep_bridge_unregister(nullptr), -1);

    // Updates
    EXPECT_EQ(gt_fep_update_callback(nullptr), -1);
    EXPECT_EQ(gt_fep_bridge_force_update(nullptr), -1);
    EXPECT_EQ(gt_fep_bridge_update_strategy_uncertainty(nullptr, 0.5f), -1);
    EXPECT_EQ(gt_fep_bridge_update_opponent_error(nullptr, 0.5f), -1);
    EXPECT_EQ(gt_fep_bridge_update_nash_distance(nullptr, 0.5f), -1);

    // Metrics
    gt_fep_metrics_t metrics;
    EXPECT_EQ(gt_fep_bridge_get_metrics(nullptr, &metrics), -1);
    EXPECT_EQ(gt_fep_bridge_get_metrics(bridge, nullptr), -1);

    gt_fep_stats_t stats;
    EXPECT_EQ(gt_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(gt_fep_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(gt_fep_bridge_reset_stats(nullptr), -1);

    // State queries
    EXPECT_EQ(gt_fep_bridge_get_state(nullptr), GT_FEP_STATE_ERROR);
    EXPECT_LT(gt_fep_bridge_get_free_energy(nullptr), 0.0f);
    EXPECT_LT(gt_fep_bridge_get_prediction_error(nullptr), 0.0f);
    EXPECT_LT(gt_fep_bridge_get_strategy_uncertainty(nullptr), 0.0f);
    EXPECT_FALSE(gt_fep_bridge_is_degraded(nullptr));
    EXPECT_FALSE(gt_fep_bridge_is_at_nash(nullptr));

    // Callbacks
    EXPECT_EQ(gt_fep_bridge_set_high_fe_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(gt_fep_bridge_set_surprise_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(gt_fep_bridge_set_metrics_callback(nullptr, nullptr, nullptr), -1);

    // Config
    gt_fep_config_t cfg;
    EXPECT_EQ(gt_fep_bridge_set_config(nullptr, &cfg), -1);
    EXPECT_EQ(gt_fep_bridge_set_config(bridge, nullptr), -1);
    EXPECT_EQ(gt_fep_bridge_get_config(nullptr, &cfg), -1);
    EXPECT_EQ(gt_fep_bridge_get_config(bridge, nullptr), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/* ============================================================================
 * Error Conditions Tests
 * ============================================================================ */

/**
 * Test: ErrorConditions
 * Verify proper error handling for various edge cases
 */
TEST_F(GameTheoryFepBridgeTest, ErrorConditions) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should succeed (no-op)
    int result = gt_fep_bridge_unregister(bridge);
    EXPECT_EQ(result, 0) << "Unregister when not registered should be no-op";

    // Double destroy should be safe (after first destroy, pointer is invalid
    // but we test with different bridges)
    gt_fep_bridge_t* temp_bridge = gt_fep_bridge_create(nullptr);
    ASSERT_NE(temp_bridge, nullptr);
    gt_fep_bridge_destroy(temp_bridge);
    // temp_bridge is now invalid, but destroy(nullptr) should be safe
    gt_fep_bridge_destroy(nullptr);

    SUCCEED() << "Error condition tests passed";
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Verify basic thread safety with concurrent access
 */
TEST_F(GameTheoryFepBridgeTest, ThreadSafety) {
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
                gt_fep_bridge_get_free_energy(bridge);
                gt_fep_bridge_get_prediction_error(bridge);
                gt_fep_bridge_get_strategy_uncertainty(bridge);
                gt_fep_bridge_get_state(bridge);
                gt_fep_bridge_is_degraded(bridge);
                gt_fep_bridge_is_at_nash(bridge);

                gt_fep_metrics_t metrics;
                gt_fep_bridge_get_metrics(bridge, &metrics);

                gt_fep_stats_t stats;
                gt_fep_bridge_get_stats(bridge, &stats);

                // Write operations
                gt_fep_bridge_update_strategy_uncertainty(bridge,
                    (float)(i % 10) / 10.0f);
                gt_fep_bridge_force_update(bridge);
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
TEST_F(GameTheoryFepBridgeTest, DestroyCallback) {
    ASSERT_NE(bridge, nullptr);

    // Destroy callback should be safe to call
    gt_fep_destroy_callback(bridge);
    SUCCEED() << "Destroy callback should not crash";

    // Should also handle NULL
    gt_fep_destroy_callback(nullptr);
    SUCCEED() << "Destroy callback with NULL should not crash";
}

/* ============================================================================
 * Integration Scenario Tests
 * ============================================================================ */

/**
 * Test: GameTheoryScenario
 * Simulate a game theory reasoning scenario
 */
TEST_F(GameTheoryFepBridgeTest, GameTheoryScenario) {
    ASSERT_NE(bridge, nullptr);

    // Register metrics callback to track changes
    gt_fep_bridge_set_metrics_callback(bridge, test_metrics_callback, nullptr);

    // Scenario: Start with high uncertainty, gradually converge to Nash

    // Phase 1: Initial high uncertainty
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.9f);
    gt_fep_bridge_update_opponent_error(bridge, 0.8f);
    gt_fep_bridge_update_nash_distance(bridge, 0.95f);
    gt_fep_bridge_force_update(bridge);

    float initial_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(initial_fe, config.baseline_free_energy)
        << "High uncertainty should produce elevated free energy";

    // Phase 2: Learning about opponent
    gt_fep_bridge_update_opponent_error(bridge, 0.4f);
    gt_fep_bridge_force_update(bridge);

    float learning_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(learning_fe, initial_fe)
        << "Better opponent model should reduce free energy";

    // Phase 3: Strategy refinement
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.3f);
    gt_fep_bridge_force_update(bridge);

    float refined_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(refined_fe, learning_fe)
        << "Strategy refinement should reduce free energy";

    // Phase 4: Nash equilibrium reached
    gt_fep_bridge_update_nash_distance(bridge, 0.005f);  // Below epsilon
    gt_fep_bridge_force_update(bridge);

    EXPECT_TRUE(gt_fep_bridge_is_at_nash(bridge))
        << "Should detect Nash equilibrium";

    float final_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(final_fe, refined_fe)
        << "Nash equilibrium should have lowest free energy";

    // Verify callback was called for each update
    EXPECT_GE(g_metrics_callback_count.load(), 4)
        << "Metrics callback should be called for each update";
}
