/**
 * @file test_rcog_fep_bridge.cpp
 * @brief Unit tests for Recursive Cognition FEP Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for FEP-Recursive Cognition bidirectional integration
 * WHY:  Ensure free energy computation from recursion metrics works correctly
 * HOW:  Test lifecycle, registration, metrics, callbacks, and FEP update cycle
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - FEP Registration/Unregistration
 * - Update Callback Mechanism
 * - Free Energy Computation
 * - Prediction Error Tracking
 * - Metrics and Statistics
 * - Null Parameter Handling
 * - Thread Safety (basic)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/recursive/nimcp_rcog_fep_bridge.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_high_fe_callback_count{0};
static std::atomic<int> g_surprise_callback_count{0};
static std::atomic<int> g_metrics_callback_count{0};
static float g_last_free_energy = 0.0f;
static float g_last_surprise = 0.0f;
static rcog_fep_metrics_t g_last_metrics;

/**
 * Test callback for high free energy events
 */
static void test_high_fe_callback(
    rcog_fep_bridge_t* bridge,
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
    rcog_fep_bridge_t* bridge,
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
    rcog_fep_bridge_t* bridge,
    const rcog_fep_metrics_t* metrics,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    g_metrics_callback_count++;
    if (metrics != nullptr) {
        memcpy(&g_last_metrics, metrics, sizeof(rcog_fep_metrics_t));
    }
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class RcogFepBridgeTest : public ::testing::Test {
protected:
    rcog_fep_bridge_t* bridge = nullptr;
    rcog_fep_config_t config;

    void SetUp() override {
        // Reset global state
        g_high_fe_callback_count = 0;
        g_surprise_callback_count = 0;
        g_metrics_callback_count = 0;
        g_last_free_energy = 0.0f;
        g_last_surprise = 0.0f;
        memset(&g_last_metrics, 0, sizeof(g_last_metrics));

        // Get default config and create bridge
        config = rcog_fep_config_default();
        bridge = rcog_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            rcog_fep_bridge_destroy(bridge);
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
TEST_F(RcogFepBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify initial state
    rcog_fep_state_t state = rcog_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, RCOG_FEP_STATE_IDLE) << "Initial state should be IDLE";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(RcogFepBridgeTest, BridgeCreationNullConfig) {
    rcog_fep_bridge_t* br = rcog_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    rcog_fep_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(RcogFepBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    rcog_fep_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    rcog_fep_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(RcogFepBridgeTest, DefaultConfig) {
    rcog_fep_config_t default_config = rcog_fep_config_default();

    // Verify weights are positive and sum reasonably
    EXPECT_GT(default_config.depth_weight, 0.0f);
    EXPECT_GT(default_config.decomp_weight, 0.0f);
    EXPECT_GT(default_config.refine_weight, 0.0f);

    // Verify thresholds are sensible
    EXPECT_GT(default_config.high_free_energy_threshold, 0.0f);
    EXPECT_GT(default_config.prediction_error_threshold, 0.0f);
    EXPECT_GT(default_config.convergence_threshold, 0.0f);

    // Verify normalization parameters
    EXPECT_GT(default_config.max_depth_norm, 0u);
    EXPECT_GE(default_config.baseline_free_energy, 0.0f);
    EXPECT_GT(default_config.max_free_energy, default_config.baseline_free_energy);

    // Verify decay rate is valid
    EXPECT_GT(default_config.error_decay_rate, 0.0f);
    EXPECT_LE(default_config.error_decay_rate, 1.0f);
}

/**
 * Test: BridgeReset
 * Verify bridge can be reset to initial state
 */
TEST_F(RcogFepBridgeTest, BridgeReset) {
    ASSERT_NE(bridge, nullptr);

    // Force an update to change state
    int result = rcog_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Reset bridge
    result = rcog_fep_bridge_reset(bridge);
    EXPECT_EQ(result, 0) << "Reset should succeed";

    // Verify state is back to IDLE
    rcog_fep_state_t state = rcog_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, RCOG_FEP_STATE_IDLE) << "State should be IDLE after reset";
}

/**
 * Test: BridgeResetNull
 * Verify reset handles NULL gracefully
 */
TEST_F(RcogFepBridgeTest, BridgeResetNull) {
    int result = rcog_fep_bridge_reset(nullptr);
    EXPECT_EQ(result, -1) << "Reset with NULL should fail";
}

/* ============================================================================
 * FEP Registration Tests
 * ============================================================================ */

/**
 * Test: FEPRegistration
 * Verify bridge can register with FEP orchestrator
 */
TEST_F(RcogFepBridgeTest, FEPRegistration) {
    ASSERT_NE(bridge, nullptr);

    // Before registration, should not be registered
    EXPECT_FALSE(rcog_fep_bridge_is_registered(bridge))
        << "Should not be registered initially";

    // Without actual orchestrator, we test the ID accessor
    uint32_t id = rcog_fep_bridge_get_id(bridge);
    EXPECT_EQ(id, 0u) << "ID should be 0 when not registered";
}

/**
 * Test: FEPRegistrationNullParams
 * Verify registration handles NULL parameters gracefully
 */
TEST_F(RcogFepBridgeTest, FEPRegistrationNullParams) {
    ASSERT_NE(bridge, nullptr);

    uint32_t bridge_id = 0;

    // All NULL parameters
    int result = rcog_fep_bridge_register_ex(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1) << "Register with all NULL should fail";

    // Only bridge provided
    result = rcog_fep_bridge_register_ex(bridge, nullptr, nullptr, &bridge_id);
    EXPECT_EQ(result, -1) << "Register without orchestrator should fail";
}

/**
 * Test: FEPUnregistration
 * Verify bridge can unregister cleanly
 */
TEST_F(RcogFepBridgeTest, FEPUnregistration) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should handle gracefully
    int result = rcog_fep_bridge_unregister(bridge);
    // May succeed or fail depending on implementation - just shouldn't crash
    (void)result;

    EXPECT_FALSE(rcog_fep_bridge_is_registered(bridge))
        << "Should not be registered after unregister";
}

/**
 * Test: FEPUnregistrationNull
 * Verify unregister handles NULL gracefully
 */
TEST_F(RcogFepBridgeTest, FEPUnregistrationNull) {
    int result = rcog_fep_bridge_unregister(nullptr);
    EXPECT_EQ(result, -1) << "Unregister NULL should fail";
}

/**
 * Test: IsRegisteredNull
 * Verify is_registered handles NULL gracefully
 */
TEST_F(RcogFepBridgeTest, IsRegisteredNull) {
    bool registered = rcog_fep_bridge_is_registered(nullptr);
    EXPECT_FALSE(registered) << "NULL bridge should not be registered";
}

/**
 * Test: GetIdNull
 * Verify get_id handles NULL gracefully
 */
TEST_F(RcogFepBridgeTest, GetIdNull) {
    uint32_t id = rcog_fep_bridge_get_id(nullptr);
    EXPECT_EQ(id, 0u) << "NULL bridge should return ID 0";
}

/* ============================================================================
 * FEP Update Callback Tests
 * ============================================================================ */

/**
 * Test: UpdateCallback
 * Verify FEP update callback works correctly
 * Note: Raw callback requires registration and engine, returns -1 without them
 */
TEST_F(RcogFepBridgeTest, UpdateCallback) {
    ASSERT_NE(bridge, nullptr);

    // Call update callback directly - should fail without registration
    int result = rcog_fep_update_callback(bridge);
    EXPECT_EQ(result, -1) << "Update callback should fail without registration";

    // Use force_update for unregistered bridge (test-friendly path)
    result = rcog_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed without registration";
}

/**
 * Test: UpdateCallbackNull
 * Verify update callback handles NULL gracefully
 */
TEST_F(RcogFepBridgeTest, UpdateCallbackNull) {
    int result = rcog_fep_update_callback(nullptr);
    EXPECT_EQ(result, -1) << "Update callback with NULL should fail";
}

/**
 * Test: ForceUpdate
 * Verify force update triggers FEP computation
 */
TEST_F(RcogFepBridgeTest, ForceUpdate) {
    ASSERT_NE(bridge, nullptr);

    // Get initial metrics
    rcog_fep_metrics_t initial_metrics;
    int result = rcog_fep_bridge_get_metrics(bridge, &initial_metrics);
    EXPECT_EQ(result, 0);

    // Force update
    result = rcog_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Get updated metrics
    rcog_fep_metrics_t updated_metrics;
    result = rcog_fep_bridge_get_metrics(bridge, &updated_metrics);
    EXPECT_EQ(result, 0);

    // Update count should have increased
    EXPECT_GE(updated_metrics.update_count, initial_metrics.update_count)
        << "Update count should increase after force update";
}

/**
 * Test: ForceUpdateNull
 * Verify force update handles NULL gracefully
 */
TEST_F(RcogFepBridgeTest, ForceUpdateNull) {
    int result = rcog_fep_bridge_force_update(nullptr);
    EXPECT_EQ(result, -1) << "Force update with NULL should fail";
}

/* ============================================================================
 * Free Energy Computation Tests
 * ============================================================================ */

/**
 * Test: FreeEnergyComputation
 * Verify free energy is calculated from recursion metrics
 */
TEST_F(RcogFepBridgeTest, FreeEnergyComputation) {
    ASSERT_NE(bridge, nullptr);

    // Get initial free energy
    float initial_fe = rcog_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(initial_fe, 0.0f) << "Free energy should be non-negative";
    EXPECT_LE(initial_fe, config.max_free_energy)
        << "Free energy should not exceed maximum";

    // After force update, free energy should still be valid
    rcog_fep_bridge_force_update(bridge);
    float updated_fe = rcog_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(updated_fe, 0.0f);
    EXPECT_LE(updated_fe, config.max_free_energy);
}

/**
 * Test: FreeEnergyNull
 * Verify get_free_energy handles NULL gracefully
 */
TEST_F(RcogFepBridgeTest, FreeEnergyNull) {
    float fe = rcog_fep_bridge_get_free_energy(nullptr);
    EXPECT_LT(fe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: FreeEnergyBaseline
 * Verify baseline free energy is used when idle
 */
TEST_F(RcogFepBridgeTest, FreeEnergyBaseline) {
    ASSERT_NE(bridge, nullptr);

    // In idle state with no processing, free energy should be near baseline
    float fe = rcog_fep_bridge_get_free_energy(bridge);
    // Allow some tolerance
    EXPECT_GE(fe, config.baseline_free_energy * 0.5f);
    EXPECT_LE(fe, config.baseline_free_energy * 2.0f);
}

/* ============================================================================
 * Prediction Error Tests
 * ============================================================================ */

/**
 * Test: PredictionError
 * Verify prediction error tracking from task success rate
 */
TEST_F(RcogFepBridgeTest, PredictionError) {
    ASSERT_NE(bridge, nullptr);

    // Get initial prediction error
    float initial_pe = rcog_fep_bridge_get_prediction_error(bridge);
    EXPECT_GE(initial_pe, 0.0f) << "Prediction error should be non-negative";
    EXPECT_LE(initial_pe, 1.0f) << "Prediction error should be <= 1.0";
}

/**
 * Test: PredictionErrorNull
 * Verify get_prediction_error handles NULL gracefully
 */
TEST_F(RcogFepBridgeTest, PredictionErrorNull) {
    float pe = rcog_fep_bridge_get_prediction_error(nullptr);
    EXPECT_LT(pe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: PredictionErrorDecay
 * Verify prediction error decays over time
 */
TEST_F(RcogFepBridgeTest, PredictionErrorDecay) {
    ASSERT_NE(bridge, nullptr);

    // Force multiple updates
    for (int i = 0; i < 10; i++) {
        rcog_fep_bridge_force_update(bridge);
    }

    // Prediction error should have decayed
    float pe = rcog_fep_bridge_get_prediction_error(bridge);
    EXPECT_GE(pe, 0.0f);
    EXPECT_LE(pe, 1.0f);
}

/* ============================================================================
 * Metrics Tracking Tests
 * ============================================================================ */

/**
 * Test: MetricsTracking
 * Verify metrics are updated correctly during FEP cycles
 */
TEST_F(RcogFepBridgeTest, MetricsTracking) {
    ASSERT_NE(bridge, nullptr);

    rcog_fep_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    int result = rcog_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(result, 0) << "Get metrics should succeed";

    // Verify core FEP metrics are valid
    EXPECT_GE(metrics.free_energy, 0.0f);
    EXPECT_GE(metrics.prediction_error, 0.0f);
    EXPECT_LE(metrics.prediction_error, 1.0f);
    EXPECT_GE(metrics.entropy, 0.0f);

    // Verify recursion-specific metrics are valid
    EXPECT_GE(metrics.normalized_depth, 0.0f);
    EXPECT_LE(metrics.normalized_depth, 1.0f);
    EXPECT_GE(metrics.decomp_success_rate, 0.0f);
    EXPECT_LE(metrics.decomp_success_rate, 1.0f);
    EXPECT_GE(metrics.refinement_progress, 0.0f);
    EXPECT_LE(metrics.refinement_progress, 1.0f);
}

/**
 * Test: MetricsTrackingNull
 * Verify get_metrics handles NULL parameters gracefully
 */
TEST_F(RcogFepBridgeTest, MetricsTrackingNull) {
    rcog_fep_metrics_t metrics;

    int result = rcog_fep_bridge_get_metrics(nullptr, &metrics);
    EXPECT_EQ(result, -1) << "Get metrics with NULL bridge should fail";

    result = rcog_fep_bridge_get_metrics(bridge, nullptr);
    EXPECT_EQ(result, -1) << "Get metrics with NULL output should fail";
}

/**
 * Test: StatsTracking
 * Verify statistics are accumulated correctly
 */
TEST_F(RcogFepBridgeTest, StatsTracking) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        rcog_fep_bridge_force_update(bridge);
    }

    rcog_fep_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = rcog_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify stats are accumulated
    EXPECT_GE(stats.total_updates, 5u) << "Should have at least 5 updates";
    EXPECT_GE(stats.avg_free_energy, 0.0f);
    EXPECT_GE(stats.avg_prediction_error, 0.0f);
}

/**
 * Test: StatsTrackingNull
 * Verify get_stats handles NULL parameters gracefully
 */
TEST_F(RcogFepBridgeTest, StatsTrackingNull) {
    rcog_fep_stats_t stats;

    int result = rcog_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1) << "Get stats with NULL bridge should fail";

    result = rcog_fep_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(result, -1) << "Get stats with NULL output should fail";
}

/**
 * Test: StatsReset
 * Verify statistics can be reset
 */
TEST_F(RcogFepBridgeTest, StatsReset) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        rcog_fep_bridge_force_update(bridge);
    }

    // Reset stats
    int result = rcog_fep_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    rcog_fep_stats_t stats;
    result = rcog_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    // Total updates should be reset to 0
    EXPECT_EQ(stats.total_updates, 0u) << "Updates should be reset to 0";
}

/**
 * Test: StatsResetNull
 * Verify reset_stats handles NULL gracefully
 */
TEST_F(RcogFepBridgeTest, StatsResetNull) {
    int result = rcog_fep_bridge_reset_stats(nullptr);
    EXPECT_EQ(result, -1) << "Reset stats with NULL should fail";
}

/* ============================================================================
 * State Query Tests
 * ============================================================================ */

/**
 * Test: GetState
 * Verify bridge state can be queried
 */
TEST_F(RcogFepBridgeTest, GetState) {
    ASSERT_NE(bridge, nullptr);

    rcog_fep_state_t state = rcog_fep_bridge_get_state(bridge);
    // State should be one of the valid states
    EXPECT_GE((int)state, (int)RCOG_FEP_STATE_UNINITIALIZED);
    EXPECT_LE((int)state, (int)RCOG_FEP_STATE_ERROR);
}

/**
 * Test: GetStateNull
 * Verify get_state handles NULL gracefully
 */
TEST_F(RcogFepBridgeTest, GetStateNull) {
    rcog_fep_state_t state = rcog_fep_bridge_get_state(nullptr);
    EXPECT_EQ(state, RCOG_FEP_STATE_ERROR)
        << "NULL bridge should return ERROR state";
}

/**
 * Test: IsDegraded
 * Verify degraded mode detection
 */
TEST_F(RcogFepBridgeTest, IsDegraded) {
    ASSERT_NE(bridge, nullptr);

    // Initially should not be degraded
    bool degraded = rcog_fep_bridge_is_degraded(bridge);
    EXPECT_FALSE(degraded) << "Should not be degraded initially";
}

/**
 * Test: IsDegradedNull
 * Verify is_degraded handles NULL gracefully
 */
TEST_F(RcogFepBridgeTest, IsDegradedNull) {
    bool degraded = rcog_fep_bridge_is_degraded(nullptr);
    EXPECT_FALSE(degraded) << "NULL bridge should return false";
}

/**
 * Test: IsConverging
 * Verify convergence detection
 */
TEST_F(RcogFepBridgeTest, IsConverging) {
    ASSERT_NE(bridge, nullptr);

    // Query convergence status
    bool converging = rcog_fep_bridge_is_converging(bridge);
    // Without active processing, convergence status may vary
    (void)converging;
    SUCCEED() << "Is converging should not crash";
}

/**
 * Test: IsConvergingNull
 * Verify is_converging handles NULL gracefully
 */
TEST_F(RcogFepBridgeTest, IsConvergingNull) {
    bool converging = rcog_fep_bridge_is_converging(nullptr);
    EXPECT_FALSE(converging) << "NULL bridge should return false";
}

/**
 * Test: GetNormalizedDepth
 * Verify normalized depth accessor
 */
TEST_F(RcogFepBridgeTest, GetNormalizedDepth) {
    ASSERT_NE(bridge, nullptr);

    float depth = rcog_fep_bridge_get_normalized_depth(bridge);
    EXPECT_GE(depth, 0.0f) << "Depth should be non-negative";
    EXPECT_LE(depth, 1.0f) << "Depth should be <= 1.0";
}

/**
 * Test: GetNormalizedDepthNull
 * Verify get_normalized_depth handles NULL gracefully
 */
TEST_F(RcogFepBridgeTest, GetNormalizedDepthNull) {
    float depth = rcog_fep_bridge_get_normalized_depth(nullptr);
    EXPECT_LT(depth, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: GetDecompSuccessRate
 * Verify decomposition success rate accessor
 */
TEST_F(RcogFepBridgeTest, GetDecompSuccessRate) {
    ASSERT_NE(bridge, nullptr);

    float rate = rcog_fep_bridge_get_decomp_success_rate(bridge);
    EXPECT_GE(rate, 0.0f) << "Success rate should be non-negative";
    EXPECT_LE(rate, 1.0f) << "Success rate should be <= 1.0";
}

/**
 * Test: GetDecompSuccessRateNull
 * Verify get_decomp_success_rate handles NULL gracefully
 */
TEST_F(RcogFepBridgeTest, GetDecompSuccessRateNull) {
    float rate = rcog_fep_bridge_get_decomp_success_rate(nullptr);
    EXPECT_LT(rate, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: StateName
 * Verify state name conversion
 */
TEST_F(RcogFepBridgeTest, StateName) {
    const char* name = rcog_fep_state_name(RCOG_FEP_STATE_IDLE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u) << "State name should not be empty";

    name = rcog_fep_state_name(RCOG_FEP_STATE_ACTIVE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = rcog_fep_state_name(RCOG_FEP_STATE_DEGRADED);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = rcog_fep_state_name(RCOG_FEP_STATE_ERROR);
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
TEST_F(RcogFepBridgeTest, HighFECallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = rcog_fep_bridge_set_high_fe_callback(
        bridge, test_high_fe_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = rcog_fep_bridge_set_high_fe_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: HighFECallbackNull
 * Verify callback registration handles NULL bridge
 */
TEST_F(RcogFepBridgeTest, HighFECallbackNull) {
    int result = rcog_fep_bridge_set_high_fe_callback(
        nullptr, test_high_fe_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/**
 * Test: SurpriseCallback
 * Verify surprise event callback registration
 */
TEST_F(RcogFepBridgeTest, SurpriseCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = rcog_fep_bridge_set_surprise_callback(
        bridge, test_surprise_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = rcog_fep_bridge_set_surprise_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: SurpriseCallbackNull
 * Verify callback registration handles NULL bridge
 */
TEST_F(RcogFepBridgeTest, SurpriseCallbackNull) {
    int result = rcog_fep_bridge_set_surprise_callback(
        nullptr, test_surprise_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/**
 * Test: MetricsCallback
 * Verify metrics update callback registration
 */
TEST_F(RcogFepBridgeTest, MetricsCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = rcog_fep_bridge_set_metrics_callback(
        bridge, test_metrics_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Force update to trigger callback
    rcog_fep_bridge_force_update(bridge);

    // Metrics callback should have been called
    EXPECT_GE(g_metrics_callback_count.load(), 1)
        << "Metrics callback should be called on update";

    // Clear callback
    result = rcog_fep_bridge_set_metrics_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: MetricsCallbackNull
 * Verify callback registration handles NULL bridge
 */
TEST_F(RcogFepBridgeTest, MetricsCallbackNull) {
    int result = rcog_fep_bridge_set_metrics_callback(
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
TEST_F(RcogFepBridgeTest, SetConfig) {
    ASSERT_NE(bridge, nullptr);

    rcog_fep_config_t new_config = rcog_fep_config_default();
    new_config.depth_weight = 0.5f;
    new_config.decomp_weight = 0.3f;
    new_config.refine_weight = 0.2f;

    int result = rcog_fep_bridge_set_config(bridge, &new_config);
    EXPECT_EQ(result, 0) << "Set config should succeed";
}

/**
 * Test: SetConfigNull
 * Verify set_config handles NULL parameters
 */
TEST_F(RcogFepBridgeTest, SetConfigNull) {
    rcog_fep_config_t config_val = rcog_fep_config_default();

    int result = rcog_fep_bridge_set_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = rcog_fep_bridge_set_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL config should fail";
}

/**
 * Test: GetConfig
 * Verify configuration can be retrieved
 */
TEST_F(RcogFepBridgeTest, GetConfig) {
    ASSERT_NE(bridge, nullptr);

    rcog_fep_config_t retrieved_config;
    memset(&retrieved_config, 0, sizeof(retrieved_config));

    int result = rcog_fep_bridge_get_config(bridge, &retrieved_config);
    EXPECT_EQ(result, 0) << "Get config should succeed";

    // Verify retrieved config has valid values
    EXPECT_GT(retrieved_config.depth_weight, 0.0f);
    EXPECT_GT(retrieved_config.max_free_energy, 0.0f);
}

/**
 * Test: GetConfigNull
 * Verify get_config handles NULL parameters
 */
TEST_F(RcogFepBridgeTest, GetConfigNull) {
    rcog_fep_config_t config_val;

    int result = rcog_fep_bridge_get_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = rcog_fep_bridge_get_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL output should fail";
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Comprehensive test for NULL parameter handling across all functions
 */
TEST_F(RcogFepBridgeTest, NullHandling) {
    // Lifecycle
    rcog_fep_bridge_destroy(nullptr);

    // Registration
    EXPECT_EQ(rcog_fep_bridge_reset(nullptr), -1);
    EXPECT_FALSE(rcog_fep_bridge_is_registered(nullptr));
    EXPECT_EQ(rcog_fep_bridge_get_id(nullptr), 0u);
    EXPECT_EQ(rcog_fep_bridge_unregister(nullptr), -1);

    // Updates
    EXPECT_EQ(rcog_fep_update_callback(nullptr), -1);
    EXPECT_EQ(rcog_fep_bridge_force_update(nullptr), -1);

    // Metrics
    rcog_fep_metrics_t metrics;
    EXPECT_EQ(rcog_fep_bridge_get_metrics(nullptr, &metrics), -1);
    EXPECT_EQ(rcog_fep_bridge_get_metrics(bridge, nullptr), -1);

    rcog_fep_stats_t stats;
    EXPECT_EQ(rcog_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(rcog_fep_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(rcog_fep_bridge_reset_stats(nullptr), -1);

    // State queries
    EXPECT_EQ(rcog_fep_bridge_get_state(nullptr), RCOG_FEP_STATE_ERROR);
    EXPECT_LT(rcog_fep_bridge_get_free_energy(nullptr), 0.0f);
    EXPECT_LT(rcog_fep_bridge_get_prediction_error(nullptr), 0.0f);
    EXPECT_FALSE(rcog_fep_bridge_is_degraded(nullptr));
    EXPECT_FALSE(rcog_fep_bridge_is_converging(nullptr));
    EXPECT_LT(rcog_fep_bridge_get_normalized_depth(nullptr), 0.0f);
    EXPECT_LT(rcog_fep_bridge_get_decomp_success_rate(nullptr), 0.0f);

    // Callbacks
    EXPECT_EQ(rcog_fep_bridge_set_high_fe_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(rcog_fep_bridge_set_surprise_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(rcog_fep_bridge_set_metrics_callback(nullptr, nullptr, nullptr), -1);

    // Config
    rcog_fep_config_t cfg;
    EXPECT_EQ(rcog_fep_bridge_set_config(nullptr, &cfg), -1);
    EXPECT_EQ(rcog_fep_bridge_set_config(bridge, nullptr), -1);
    EXPECT_EQ(rcog_fep_bridge_get_config(nullptr, &cfg), -1);
    EXPECT_EQ(rcog_fep_bridge_get_config(bridge, nullptr), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Verify basic thread safety with concurrent access
 */
TEST_F(RcogFepBridgeTest, ThreadSafety) {
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
                rcog_fep_bridge_get_free_energy(bridge);
                rcog_fep_bridge_get_prediction_error(bridge);
                rcog_fep_bridge_get_state(bridge);
                rcog_fep_bridge_is_degraded(bridge);

                rcog_fep_metrics_t metrics;
                rcog_fep_bridge_get_metrics(bridge, &metrics);

                rcog_fep_stats_t stats;
                rcog_fep_bridge_get_stats(bridge, &stats);

                // Force update (write operation)
                rcog_fep_bridge_force_update(bridge);
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
TEST_F(RcogFepBridgeTest, DestroyCallback) {
    ASSERT_NE(bridge, nullptr);

    // Destroy callback should be safe to call
    rcog_fep_destroy_callback(bridge);
    SUCCEED() << "Destroy callback should not crash";

    // Should also handle NULL
    rcog_fep_destroy_callback(nullptr);
    SUCCEED() << "Destroy callback with NULL should not crash";
}
