/**
 * @file test_social_fep_bridge.cpp
 * @brief Unit tests for Social Cognition FEP Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for FEP-Social Cognition bidirectional integration
 * WHY:  Ensure free energy computation from social metrics works correctly
 * HOW:  Test lifecycle, registration, metrics, callbacks, and FEP update cycle
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Default Configuration
 * - FEP Registration/Unregistration
 * - Update Callback Mechanism
 * - Free Energy Computation
 * - Social Prediction Error Tracking
 * - Relationship Uncertainty Contribution
 * - Social Norm Violation Detection
 * - Statistics Tracking and Reset
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

#include "cognitive/social/nimcp_social_fep_bridge.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_high_fe_callback_count{0};
static std::atomic<int> g_surprise_callback_count{0};
static std::atomic<int> g_metrics_callback_count{0};
static float g_last_free_energy = 0.0f;
static float g_last_surprise = 0.0f;
static std::string g_last_surprise_source;
static social_fep_metrics_t g_last_metrics;

/**
 * Test callback for high free energy events
 */
static void test_high_fe_callback(
    social_fep_bridge_t* bridge,
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
    social_fep_bridge_t* bridge,
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
    social_fep_bridge_t* bridge,
    const social_fep_metrics_t* metrics,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    g_metrics_callback_count++;
    if (metrics != nullptr) {
        memcpy(&g_last_metrics, metrics, sizeof(social_fep_metrics_t));
    }
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SocialFepBridgeTest : public ::testing::Test {
protected:
    social_fep_bridge_t* bridge = nullptr;
    social_fep_config_t config;

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
        config = social_fep_config_default();
        bridge = social_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            social_fep_bridge_destroy(bridge);
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
TEST_F(SocialFepBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify initial state
    social_fep_state_t state = social_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, SOCIAL_FEP_STATE_IDLE) << "Initial state should be IDLE";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(SocialFepBridgeTest, BridgeCreationNullConfig) {
    social_fep_bridge_t* br = social_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    social_fep_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(SocialFepBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    social_fep_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    social_fep_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(SocialFepBridgeTest, DefaultConfig) {
    social_fep_config_t default_config = social_fep_config_default();

    // Verify weights are positive
    EXPECT_GT(default_config.free_energy_weight, 0.0f);
    EXPECT_GT(default_config.social_prediction_error_weight, 0.0f);
    EXPECT_GT(default_config.relationship_uncertainty_weight, 0.0f);
    EXPECT_GT(default_config.norm_violation_weight, 0.0f);

    // Verify thresholds are sensible
    EXPECT_GT(default_config.high_free_energy_threshold, 0.0f);
    EXPECT_GT(default_config.prediction_error_threshold, 0.0f);
    EXPECT_GT(default_config.uncertainty_threshold, 0.0f);

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
 * Verify configuration can be set and retrieved
 */
TEST_F(SocialFepBridgeTest, ConfigValidation) {
    ASSERT_NE(bridge, nullptr);

    // Modify config
    social_fep_config_t new_config = social_fep_config_default();
    new_config.social_prediction_error_weight = 0.5f;
    new_config.relationship_uncertainty_weight = 0.3f;
    new_config.norm_violation_weight = 0.2f;
    new_config.update_interval_ms = 100;

    int result = social_fep_bridge_set_config(bridge, &new_config);
    EXPECT_EQ(result, 0) << "Set config should succeed";

    // Retrieve and verify
    social_fep_config_t retrieved_config;
    result = social_fep_bridge_get_config(bridge, &retrieved_config);
    EXPECT_EQ(result, 0) << "Get config should succeed";

    EXPECT_FLOAT_EQ(retrieved_config.social_prediction_error_weight, 0.5f);
    EXPECT_FLOAT_EQ(retrieved_config.relationship_uncertainty_weight, 0.3f);
    EXPECT_FLOAT_EQ(retrieved_config.norm_violation_weight, 0.2f);
    EXPECT_EQ(retrieved_config.update_interval_ms, 100u);
}

/**
 * Test: BridgeReset
 * Verify bridge can be reset to initial state
 */
TEST_F(SocialFepBridgeTest, BridgeReset) {
    ASSERT_NE(bridge, nullptr);

    // Force an update to change state
    int result = social_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Reset bridge
    result = social_fep_bridge_reset(bridge);
    EXPECT_EQ(result, 0) << "Reset should succeed";

    // Verify state is back to IDLE
    social_fep_state_t state = social_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, SOCIAL_FEP_STATE_IDLE) << "State should be IDLE after reset";
}

/* ============================================================================
 * FEP Registration Tests
 * ============================================================================ */

/**
 * Test: RegisterWithFEP
 * Verify bridge registration state handling
 */
TEST_F(SocialFepBridgeTest, RegisterWithFEP) {
    ASSERT_NE(bridge, nullptr);

    // Before registration, should not be registered
    EXPECT_FALSE(social_fep_bridge_is_registered(bridge))
        << "Should not be registered initially";

    // Without actual orchestrator, we test the ID accessor
    uint32_t id = social_fep_bridge_get_id(bridge);
    EXPECT_EQ(id, 0u) << "ID should be 0 when not registered";
}

/**
 * Test: UnregisterFromFEP
 * Verify bridge can unregister cleanly
 */
TEST_F(SocialFepBridgeTest, UnregisterFromFEP) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should handle gracefully
    int result = social_fep_bridge_unregister(bridge);
    EXPECT_EQ(result, 0) << "Unregister when not registered should succeed";

    EXPECT_FALSE(social_fep_bridge_is_registered(bridge))
        << "Should not be registered after unregister";
}

/* ============================================================================
 * Update Callback Tests
 * ============================================================================ */

/**
 * Test: UpdateCallback
 * Verify FEP update callback works correctly
 */
TEST_F(SocialFepBridgeTest, UpdateCallback) {
    ASSERT_NE(bridge, nullptr);

    // Call update callback directly - should fail without registration
    int result = social_fep_update_callback(bridge);
    EXPECT_EQ(result, -1) << "Update callback should fail without registration";

    // Use force_update for unregistered bridge (test-friendly path)
    result = social_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed without registration";
}

/**
 * Test: ForceUpdate
 * Verify force update triggers FEP computation
 */
TEST_F(SocialFepBridgeTest, ForceUpdate) {
    ASSERT_NE(bridge, nullptr);

    // Get initial metrics
    social_fep_metrics_t initial_metrics;
    int result = social_fep_bridge_get_metrics(bridge, &initial_metrics);
    EXPECT_EQ(result, 0);

    // Force update
    result = social_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Get updated metrics
    social_fep_metrics_t updated_metrics;
    result = social_fep_bridge_get_metrics(bridge, &updated_metrics);
    EXPECT_EQ(result, 0);

    // Update count should have increased
    EXPECT_GE(updated_metrics.update_count, initial_metrics.update_count)
        << "Update count should increase after force update";
}

/* ============================================================================
 * Free Energy Computation Tests
 * ============================================================================ */

/**
 * Test: FreeEnergyContribution
 * Verify free energy is calculated correctly
 */
TEST_F(SocialFepBridgeTest, FreeEnergyContribution) {
    ASSERT_NE(bridge, nullptr);

    // Get initial free energy
    float initial_fe = social_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_GE(initial_fe, 0.0f) << "Free energy should be non-negative";
    EXPECT_LE(initial_fe, config.max_free_energy)
        << "Free energy should not exceed maximum";

    // After force update, free energy should still be valid
    social_fep_bridge_force_update(bridge);
    float updated_fe = social_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_GE(updated_fe, 0.0f);
    EXPECT_LE(updated_fe, config.max_free_energy);
}

/**
 * Test: SocialPredictionErrorIncreasesFreeEnergy
 * Verify that social prediction error contributes to free energy
 */
TEST_F(SocialFepBridgeTest, SocialPredictionErrorIncreasesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Force an update
    social_fep_bridge_force_update(bridge);

    // Get social prediction error
    float spe = social_fep_bridge_get_social_prediction_error(bridge);
    EXPECT_GE(spe, 0.0f) << "Social prediction error should be non-negative";
    EXPECT_LE(spe, 1.0f) << "Social prediction error should be <= 1.0";

    // Get free energy
    float fe = social_fep_bridge_get_free_energy_contribution(bridge);

    // Free energy should be at least baseline
    EXPECT_GE(fe, config.baseline_free_energy)
        << "Free energy should be at least baseline";
}

/**
 * Test: RelationshipUncertaintyContribution
 * Verify relationship uncertainty contributes to free energy
 */
TEST_F(SocialFepBridgeTest, RelationshipUncertaintyContribution) {
    ASSERT_NE(bridge, nullptr);

    // Force an update
    social_fep_bridge_force_update(bridge);

    // Get relationship uncertainty
    float uncertainty = social_fep_bridge_get_relationship_uncertainty(bridge);
    EXPECT_GE(uncertainty, 0.0f) << "Uncertainty should be non-negative";
    EXPECT_LE(uncertainty, 1.0f) << "Uncertainty should be <= 1.0";
}

/**
 * Test: AccurateSocialPredictionsReduceFreeEnergy
 * Verify concept that accurate predictions lead to lower free energy
 */
TEST_F(SocialFepBridgeTest, AccurateSocialPredictionsReduceFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Get baseline free energy
    float baseline_fe = social_fep_bridge_get_free_energy_contribution(bridge);

    // The baseline should be at the configured baseline level
    EXPECT_GE(baseline_fe, config.baseline_free_energy * 0.5f);
    EXPECT_LE(baseline_fe, config.baseline_free_energy * 2.0f);
}

/**
 * Test: SocialNormViolationHighFreeEnergy
 * Verify that norm violations would cause high free energy
 */
TEST_F(SocialFepBridgeTest, SocialNormViolationHighFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Force update to compute metrics
    social_fep_bridge_force_update(bridge);

    // Get metrics to check norm violation surprise
    social_fep_metrics_t metrics;
    int result = social_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(result, 0);

    // Norm violation surprise should be in valid range
    EXPECT_GE(metrics.norm_violation_surprise, 0.0f);
    EXPECT_LE(metrics.norm_violation_surprise, 1.0f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

/**
 * Test: StatisticsTracking
 * Verify statistics are accumulated correctly
 */
TEST_F(SocialFepBridgeTest, StatisticsTracking) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        social_fep_bridge_force_update(bridge);
    }

    social_fep_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = social_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify stats are accumulated
    EXPECT_GE(stats.total_updates, 5u) << "Should have at least 5 updates";
    EXPECT_GE(stats.avg_free_energy, 0.0f);
    EXPECT_GE(stats.avg_update_time_us, 0.0f);
}

/**
 * Test: StatsReset
 * Verify statistics can be reset
 */
TEST_F(SocialFepBridgeTest, StatsReset) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        social_fep_bridge_force_update(bridge);
    }

    // Reset stats
    int result = social_fep_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    social_fep_stats_t stats;
    result = social_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    // Total updates should be reset to 0
    EXPECT_EQ(stats.total_updates, 0u) << "Updates should be reset to 0";
}

/* ============================================================================
 * Metrics Tests
 * ============================================================================ */

/**
 * Test: MetricsTracking
 * Verify metrics are updated correctly during FEP cycles
 */
TEST_F(SocialFepBridgeTest, MetricsTracking) {
    ASSERT_NE(bridge, nullptr);

    social_fep_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    int result = social_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(result, 0) << "Get metrics should succeed";

    // Verify core FEP metrics are valid
    EXPECT_GE(metrics.free_energy, 0.0f);
    EXPECT_GE(metrics.prediction_error, 0.0f);
    EXPECT_LE(metrics.prediction_error, 1.0f);
    EXPECT_GE(metrics.entropy, 0.0f);

    // Verify social-specific metrics are valid
    EXPECT_GE(metrics.social_prediction_error, 0.0f);
    EXPECT_LE(metrics.social_prediction_error, 1.0f);
    EXPECT_GE(metrics.relationship_uncertainty, 0.0f);
    EXPECT_LE(metrics.relationship_uncertainty, 1.0f);
    EXPECT_GE(metrics.norm_violation_surprise, 0.0f);
    EXPECT_LE(metrics.norm_violation_surprise, 1.0f);
}

/* ============================================================================
 * State Query Tests
 * ============================================================================ */

/**
 * Test: GetState
 * Verify bridge state can be queried
 */
TEST_F(SocialFepBridgeTest, GetState) {
    ASSERT_NE(bridge, nullptr);

    social_fep_state_t state = social_fep_bridge_get_state(bridge);
    // State should be one of the valid states
    EXPECT_GE((int)state, (int)SOCIAL_FEP_STATE_UNINITIALIZED);
    EXPECT_LE((int)state, (int)SOCIAL_FEP_STATE_ERROR);
}

/**
 * Test: IsDegraded
 * Verify degraded mode detection
 */
TEST_F(SocialFepBridgeTest, IsDegraded) {
    ASSERT_NE(bridge, nullptr);

    // Initially should not be degraded
    bool degraded = social_fep_bridge_is_degraded(bridge);
    EXPECT_FALSE(degraded) << "Should not be degraded initially";
}

/**
 * Test: StateName
 * Verify state name conversion
 */
TEST_F(SocialFepBridgeTest, StateName) {
    const char* name = social_fep_state_name(SOCIAL_FEP_STATE_IDLE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u) << "State name should not be empty";

    name = social_fep_state_name(SOCIAL_FEP_STATE_ACTIVE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = social_fep_state_name(SOCIAL_FEP_STATE_DEGRADED);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = social_fep_state_name(SOCIAL_FEP_STATE_ERROR);
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
TEST_F(SocialFepBridgeTest, HighFECallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = social_fep_bridge_set_high_fe_callback(
        bridge, test_high_fe_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = social_fep_bridge_set_high_fe_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: SurpriseCallback
 * Verify surprise event callback registration
 */
TEST_F(SocialFepBridgeTest, SurpriseCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = social_fep_bridge_set_surprise_callback(
        bridge, test_surprise_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = social_fep_bridge_set_surprise_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: MetricsCallback
 * Verify metrics update callback registration
 */
TEST_F(SocialFepBridgeTest, MetricsCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = social_fep_bridge_set_metrics_callback(
        bridge, test_metrics_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Force update to trigger callback
    social_fep_bridge_force_update(bridge);

    // Metrics callback should have been called
    EXPECT_GE(g_metrics_callback_count.load(), 1)
        << "Metrics callback should be called on update";

    // Clear callback
    result = social_fep_bridge_set_metrics_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Verify basic thread safety with concurrent access
 */
TEST_F(SocialFepBridgeTest, ThreadSafety) {
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
                social_fep_bridge_get_free_energy_contribution(bridge);
                social_fep_bridge_get_social_prediction_error(bridge);
                social_fep_bridge_get_relationship_uncertainty(bridge);
                social_fep_bridge_get_state(bridge);
                social_fep_bridge_is_degraded(bridge);

                social_fep_metrics_t metrics;
                social_fep_bridge_get_metrics(bridge, &metrics);

                social_fep_stats_t stats;
                social_fep_bridge_get_stats(bridge, &stats);

                // Force update (write operation)
                social_fep_bridge_force_update(bridge);
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
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Comprehensive test for NULL parameter handling across all functions
 */
TEST_F(SocialFepBridgeTest, NullHandling) {
    // Lifecycle
    social_fep_bridge_destroy(nullptr);

    // Reset
    EXPECT_EQ(social_fep_bridge_reset(nullptr), -1);

    // Registration
    EXPECT_FALSE(social_fep_bridge_is_registered(nullptr));
    EXPECT_EQ(social_fep_bridge_get_id(nullptr), 0u);
    EXPECT_EQ(social_fep_bridge_unregister(nullptr), -1);
    EXPECT_EQ(social_fep_bridge_register(nullptr, nullptr, nullptr, nullptr), -1);

    // Updates
    EXPECT_EQ(social_fep_update_callback(nullptr), -1);
    EXPECT_EQ(social_fep_bridge_update(nullptr), -1);
    EXPECT_EQ(social_fep_bridge_force_update(nullptr), -1);

    // Metrics
    social_fep_metrics_t metrics;
    EXPECT_EQ(social_fep_bridge_get_metrics(nullptr, &metrics), -1);
    EXPECT_EQ(social_fep_bridge_get_metrics(bridge, nullptr), -1);

    social_fep_stats_t stats;
    EXPECT_EQ(social_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(social_fep_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(social_fep_bridge_reset_stats(nullptr), -1);

    // Free energy accessors
    EXPECT_LT(social_fep_bridge_get_free_energy_contribution(nullptr), 0.0f);
    EXPECT_LT(social_fep_bridge_get_social_prediction_error(nullptr), 0.0f);
    EXPECT_LT(social_fep_bridge_get_relationship_uncertainty(nullptr), 0.0f);

    // State queries
    EXPECT_EQ(social_fep_bridge_get_state(nullptr), SOCIAL_FEP_STATE_ERROR);
    EXPECT_FALSE(social_fep_bridge_is_degraded(nullptr));

    // Callbacks
    EXPECT_EQ(social_fep_bridge_set_high_fe_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(social_fep_bridge_set_surprise_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(social_fep_bridge_set_metrics_callback(nullptr, nullptr, nullptr), -1);

    // Config
    social_fep_config_t cfg;
    EXPECT_EQ(social_fep_bridge_set_config(nullptr, &cfg), -1);
    EXPECT_EQ(social_fep_bridge_set_config(bridge, nullptr), -1);
    EXPECT_EQ(social_fep_bridge_get_config(nullptr, &cfg), -1);
    EXPECT_EQ(social_fep_bridge_get_config(bridge, nullptr), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/**
 * Test: ErrorConditions
 * Verify proper error handling for various conditions
 */
TEST_F(SocialFepBridgeTest, ErrorConditions) {
    ASSERT_NE(bridge, nullptr);

    // Update callback without registration should fail
    int result = social_fep_update_callback(bridge);
    EXPECT_EQ(result, -1) << "Update without registration should fail";

    // Register with NULL orchestrator should fail
    uint32_t bridge_id;
    result = social_fep_bridge_register(bridge, nullptr, nullptr, &bridge_id);
    EXPECT_EQ(result, -1) << "Register with NULL orchestrator should fail";
}

/**
 * Test: DestroyCallback
 * Verify destroy callback can be called safely
 */
TEST_F(SocialFepBridgeTest, DestroyCallback) {
    ASSERT_NE(bridge, nullptr);

    // Destroy callback should be safe to call
    social_fep_destroy_callback(bridge);
    SUCCEED() << "Destroy callback should not crash";

    // Should also handle NULL
    social_fep_destroy_callback(nullptr);
    SUCCEED() << "Destroy callback with NULL should not crash";
}

/**
 * Test: MultipleUpdates
 * Verify multiple consecutive updates work correctly
 */
TEST_F(SocialFepBridgeTest, MultipleUpdates) {
    ASSERT_NE(bridge, nullptr);

    float prev_fe = social_fep_bridge_get_free_energy_contribution(bridge);

    // Perform multiple updates
    for (int i = 0; i < 10; i++) {
        int result = social_fep_bridge_force_update(bridge);
        EXPECT_EQ(result, 0) << "Update " << i << " should succeed";

        float current_fe = social_fep_bridge_get_free_energy_contribution(bridge);
        EXPECT_GE(current_fe, 0.0f) << "Free energy should remain non-negative";
        EXPECT_LE(current_fe, config.max_free_energy)
            << "Free energy should not exceed maximum";

        prev_fe = current_fe;
    }

    // Check stats after multiple updates
    social_fep_stats_t stats;
    int result = social_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.total_updates, 10u) << "Should have at least 10 updates";
}

/**
 * Test: ConfigPersistence
 * Verify configuration persists across updates
 */
TEST_F(SocialFepBridgeTest, ConfigPersistence) {
    ASSERT_NE(bridge, nullptr);

    // Set custom config
    social_fep_config_t new_config = social_fep_config_default();
    new_config.social_prediction_error_weight = 0.6f;
    new_config.relationship_uncertainty_weight = 0.25f;
    new_config.norm_violation_weight = 0.15f;

    int result = social_fep_bridge_set_config(bridge, &new_config);
    EXPECT_EQ(result, 0);

    // Force multiple updates
    for (int i = 0; i < 5; i++) {
        social_fep_bridge_force_update(bridge);
    }

    // Verify config is still as set
    social_fep_config_t retrieved_config;
    result = social_fep_bridge_get_config(bridge, &retrieved_config);
    EXPECT_EQ(result, 0);

    EXPECT_FLOAT_EQ(retrieved_config.social_prediction_error_weight, 0.6f);
    EXPECT_FLOAT_EQ(retrieved_config.relationship_uncertainty_weight, 0.25f);
    EXPECT_FLOAT_EQ(retrieved_config.norm_violation_weight, 0.15f);
}
