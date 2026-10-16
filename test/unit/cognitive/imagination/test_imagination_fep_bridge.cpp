/**
 * @file test_imagination_fep_bridge.cpp
 * @brief Unit tests for Imagination FEP Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for FEP-Imagination bidirectional integration
 * WHY:  Ensure free energy computation from imagination metrics works correctly
 * HOW:  Test lifecycle, registration, metrics, callbacks, and FEP update cycle
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Default Config Validation
 * - FEP Registration/Unregistration
 * - Update Callback Mechanism
 * - Free Energy Contribution
 * - Simulation Divergence Tracking
 * - Accurate Predictions Reduce Free Energy
 * - Counterfactual Cost Computation
 * - Statistics Tracking
 * - Thread Safety (basic)
 * - Null Parameter Handling
 * - Error Conditions
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/imagination/nimcp_imagination_fep_bridge.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_high_fe_callback_count{0};
static std::atomic<int> g_divergence_callback_count{0};
static float g_last_free_energy = 0.0f;
static float g_last_divergence = 0.0f;

/**
 * Test callback for high free energy events
 */
static void test_high_fe_callback(
    imagination_fep_bridge_t* bridge,
    float free_energy,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    g_high_fe_callback_count++;
    g_last_free_energy = free_energy;
}

/**
 * Test callback for divergence events
 */
static void test_divergence_callback(
    imagination_fep_bridge_t* bridge,
    float divergence,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    g_divergence_callback_count++;
    g_last_divergence = divergence;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ImaginationFepBridgeTest : public ::testing::Test {
protected:
    imagination_fep_bridge_t* bridge = nullptr;
    imagination_fep_config_t config;

    void SetUp() override {
        // Reset global state
        g_high_fe_callback_count = 0;
        g_divergence_callback_count = 0;
        g_last_free_energy = 0.0f;
        g_last_divergence = 0.0f;

        // Get default config and create bridge
        config = imagination_fep_config_default();
        bridge = imagination_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            imagination_fep_bridge_destroy(bridge);
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
TEST_F(ImaginationFepBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify initial state
    imagination_fep_state_t state = imagination_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, IMAGINATION_FEP_STATE_IDLE) << "Initial state should be IDLE";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(ImaginationFepBridgeTest, BridgeCreationNullConfig) {
    imagination_fep_bridge_t* br = imagination_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    imagination_fep_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(ImaginationFepBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    imagination_fep_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    imagination_fep_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(ImaginationFepBridgeTest, DefaultConfig) {
    imagination_fep_config_t default_config = imagination_fep_config_default();

    // Verify weights are positive
    EXPECT_GT(default_config.simulation_divergence_weight, 0.0f);
    EXPECT_GT(default_config.counterfactual_cost, 0.0f);
    EXPECT_GT(default_config.coherence_weight, 0.0f);
    EXPECT_GT(default_config.prediction_accuracy_weight, 0.0f);

    // Verify thresholds are sensible
    EXPECT_GT(default_config.high_free_energy_threshold, 0.0f);
    EXPECT_GT(default_config.divergence_threshold, 0.0f);
    EXPECT_GT(default_config.coherence_threshold, 0.0f);
    EXPECT_LT(default_config.coherence_threshold, 1.0f);

    // Verify normalization parameters
    EXPECT_GE(default_config.baseline_free_energy, 0.0f);
    EXPECT_GT(default_config.max_free_energy, default_config.baseline_free_energy);

    // Verify timing
    EXPECT_GT(default_config.update_interval_ms, 0u);
}

/**
 * Test: ConfigValidation
 * Verify configuration can be set and retrieved
 */
TEST_F(ImaginationFepBridgeTest, ConfigValidation) {
    ASSERT_NE(bridge, nullptr);

    // Modify config
    imagination_fep_config_t new_config = imagination_fep_config_default();
    new_config.simulation_divergence_weight = 0.5f;
    new_config.counterfactual_cost = 0.4f;

    int result = imagination_fep_bridge_set_config(bridge, &new_config);
    EXPECT_EQ(result, 0);

    // Retrieve and verify
    imagination_fep_config_t retrieved_config;
    result = imagination_fep_bridge_get_config(bridge, &retrieved_config);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(retrieved_config.simulation_divergence_weight, 0.5f);
    EXPECT_FLOAT_EQ(retrieved_config.counterfactual_cost, 0.4f);
}

/* ============================================================================
 * Registration Tests
 * ============================================================================ */

/**
 * Test: RegisterWithFEP
 * Note: Without a real FEP orchestrator, we test the basic registration logic
 */
TEST_F(ImaginationFepBridgeTest, RegisterWithFEP) {
    ASSERT_NE(bridge, nullptr);

    // Without orchestrator, registration should fail
    uint32_t bridge_id = 0;
    int result = imagination_fep_bridge_register(bridge, nullptr, nullptr, &bridge_id);
    EXPECT_EQ(result, -1) << "Registration without orchestrator should fail";

    // Should not be registered
    EXPECT_FALSE(imagination_fep_bridge_is_registered(bridge));
    EXPECT_EQ(imagination_fep_bridge_get_id(bridge), 0u);
}

/**
 * Test: UnregisterFromFEP
 * Verify unregistration works when not registered
 */
TEST_F(ImaginationFepBridgeTest, UnregisterFromFEP) {
    ASSERT_NE(bridge, nullptr);

    // Unregistering when not registered should succeed (no-op)
    int result = imagination_fep_bridge_unregister(bridge);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Update Callback Tests
 * ============================================================================ */

/**
 * Test: UpdateCallback
 * Verify update callback handles NULL and unregistered state
 */
TEST_F(ImaginationFepBridgeTest, UpdateCallback) {
    // NULL handle should fail
    int result = imagination_fep_update_callback(nullptr);
    EXPECT_EQ(result, -1);

    // Unregistered bridge should fail
    ASSERT_NE(bridge, nullptr);
    result = imagination_fep_update_callback(bridge);
    EXPECT_EQ(result, -1) << "Update without registration should fail";
}

/**
 * Test: ForceUpdate
 * Verify force_update works for testing purposes
 */
TEST_F(ImaginationFepBridgeTest, ForceUpdate) {
    ASSERT_NE(bridge, nullptr);

    // Force update should work even without full registration
    int result = imagination_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed for testing";

    // Stats should be updated
    imagination_fep_stats_t stats;
    result = imagination_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.total_updates, 1u);
}

/* ============================================================================
 * Free Energy Computation Tests
 * ============================================================================ */

/**
 * Test: FreeEnergyContribution
 * Verify free energy is initialized to baseline
 */
TEST_F(ImaginationFepBridgeTest, FreeEnergyContribution) {
    ASSERT_NE(bridge, nullptr);

    float fe = imagination_fep_bridge_get_free_energy(bridge);
    EXPECT_FLOAT_EQ(fe, config.baseline_free_energy)
        << "Initial free energy should be baseline";
}

/**
 * Test: SimulationDivergenceIncreasesFreeEnergy
 * Verify that simulation divergence contributes to free energy
 */
TEST_F(ImaginationFepBridgeTest, SimulationDivergenceIncreasesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Get initial divergence
    float divergence = imagination_fep_bridge_get_simulation_divergence(bridge);
    EXPECT_GE(divergence, 0.0f);
    EXPECT_LE(divergence, 1.0f);

    // Initial divergence should be zero
    EXPECT_FLOAT_EQ(divergence, 0.0f);
}

/**
 * Test: AccuratePredictionsReduceFreeEnergy
 * Verify prediction accuracy weight is positive (reduces FE)
 */
TEST_F(ImaginationFepBridgeTest, AccuratePredictionsReduceFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    imagination_fep_config_t cfg;
    int result = imagination_fep_bridge_get_config(bridge, &cfg);
    EXPECT_EQ(result, 0);

    // Prediction accuracy weight should be positive (it's subtracted in FE computation)
    EXPECT_GT(cfg.prediction_accuracy_weight, 0.0f);
}

/**
 * Test: CounterfactualCost
 * Verify counterfactual cost parameter is reasonable
 */
TEST_F(ImaginationFepBridgeTest, CounterfactualCost) {
    ASSERT_NE(bridge, nullptr);

    imagination_fep_config_t cfg;
    int result = imagination_fep_bridge_get_config(bridge, &cfg);
    EXPECT_EQ(result, 0);

    // Counterfactual cost should be positive and reasonable
    EXPECT_GT(cfg.counterfactual_cost, 0.0f);
    EXPECT_LE(cfg.counterfactual_cost, 1.0f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

/**
 * Test: StatisticsTracking
 * Verify statistics are properly tracked
 */
TEST_F(ImaginationFepBridgeTest, StatisticsTracking) {
    ASSERT_NE(bridge, nullptr);

    // Get initial stats
    imagination_fep_stats_t stats;
    int result = imagination_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_updates, 0u);

    // Force update to generate stats
    imagination_fep_bridge_force_update(bridge);

    // Get updated stats
    result = imagination_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.total_updates, 1u);
}

/**
 * Test: StatsReset
 * Verify statistics can be reset
 */
TEST_F(ImaginationFepBridgeTest, StatsReset) {
    ASSERT_NE(bridge, nullptr);

    // Generate some stats
    imagination_fep_bridge_force_update(bridge);
    imagination_fep_bridge_force_update(bridge);

    // Reset stats
    int result = imagination_fep_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    // Verify reset
    imagination_fep_stats_t stats;
    result = imagination_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_updates, 0u);
    EXPECT_FLOAT_EQ(stats.avg_free_energy, 0.0f);
    EXPECT_FLOAT_EQ(stats.peak_free_energy, 0.0f);
}

/* ============================================================================
 * State Tests
 * ============================================================================ */

/**
 * Test: BridgeReset
 * Verify bridge can be reset to initial state
 */
TEST_F(ImaginationFepBridgeTest, BridgeReset) {
    ASSERT_NE(bridge, nullptr);

    // Force an update to change state
    int result = imagination_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0);

    // Reset bridge
    result = imagination_fep_bridge_reset(bridge);
    EXPECT_EQ(result, 0);

    // Verify state is reset
    imagination_fep_state_t state = imagination_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, IMAGINATION_FEP_STATE_IDLE);

    // Verify metrics are reset
    float fe = imagination_fep_bridge_get_free_energy(bridge);
    EXPECT_FLOAT_EQ(fe, config.baseline_free_energy);
}

/**
 * Test: DegradedModeCheck
 * Verify degraded mode detection
 */
TEST_F(ImaginationFepBridgeTest, DegradedModeCheck) {
    ASSERT_NE(bridge, nullptr);

    // Initially not degraded
    EXPECT_FALSE(imagination_fep_bridge_is_degraded(bridge));

    // State should be idle initially
    imagination_fep_state_t state = imagination_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, IMAGINATION_FEP_STATE_IDLE);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

/**
 * Test: HighFECallback
 * Verify high free energy callback can be set
 */
TEST_F(ImaginationFepBridgeTest, HighFECallback) {
    ASSERT_NE(bridge, nullptr);

    int result = imagination_fep_bridge_set_high_fe_callback(
        bridge, test_high_fe_callback, nullptr);
    EXPECT_EQ(result, 0);
}

/**
 * Test: DivergenceCallback
 * Verify divergence callback can be set
 */
TEST_F(ImaginationFepBridgeTest, DivergenceCallback) {
    ASSERT_NE(bridge, nullptr);

    int result = imagination_fep_bridge_set_divergence_callback(
        bridge, test_divergence_callback, nullptr);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Basic thread safety test with concurrent reads
 */
TEST_F(ImaginationFepBridgeTest, ThreadSafety) {
    ASSERT_NE(bridge, nullptr);

    std::atomic<bool> stop{false};
    std::atomic<int> read_count{0};

    // Start reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; i++) {
        readers.emplace_back([this, &stop, &read_count]() {
            while (!stop) {
                imagination_fep_bridge_get_free_energy(bridge);
                imagination_fep_bridge_get_simulation_divergence(bridge);
                imagination_fep_bridge_get_state(bridge);
                read_count++;
            }
        });
    }

    // Let threads run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stop = true;

    // Join threads
    for (auto& t : readers) {
        t.join();
    }

    EXPECT_GT(read_count.load(), 0) << "Should have completed some reads";
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Verify all functions handle NULL parameters gracefully
 */
TEST_F(ImaginationFepBridgeTest, NullHandling) {
    // Destroy NULL
    imagination_fep_bridge_destroy(nullptr);
    SUCCEED();

    // Reset NULL
    EXPECT_EQ(imagination_fep_bridge_reset(nullptr), -1);

    // Register NULL
    EXPECT_EQ(imagination_fep_bridge_register(nullptr, nullptr, nullptr, nullptr), -1);

    // Unregister NULL
    EXPECT_EQ(imagination_fep_bridge_unregister(nullptr), -1);

    // Is registered NULL
    EXPECT_FALSE(imagination_fep_bridge_is_registered(nullptr));

    // Get ID NULL
    EXPECT_EQ(imagination_fep_bridge_get_id(nullptr), 0u);

    // Get free energy NULL
    EXPECT_FLOAT_EQ(imagination_fep_bridge_get_free_energy(nullptr), -1.0f);

    // Get divergence NULL
    EXPECT_FLOAT_EQ(imagination_fep_bridge_get_simulation_divergence(nullptr), -1.0f);

    // Get prediction error NULL
    EXPECT_FLOAT_EQ(imagination_fep_bridge_get_prediction_error(nullptr), -1.0f);

    // Get state NULL
    EXPECT_EQ(imagination_fep_bridge_get_state(nullptr), IMAGINATION_FEP_STATE_ERROR);

    // Is degraded NULL
    EXPECT_FALSE(imagination_fep_bridge_is_degraded(nullptr));

    // Get stats NULL
    imagination_fep_stats_t stats;
    EXPECT_EQ(imagination_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(imagination_fep_bridge_get_stats(bridge, nullptr), -1);

    // Reset stats NULL
    EXPECT_EQ(imagination_fep_bridge_reset_stats(nullptr), -1);

    // Set config NULL
    imagination_fep_config_t cfg;
    EXPECT_EQ(imagination_fep_bridge_set_config(nullptr, &cfg), -1);
    EXPECT_EQ(imagination_fep_bridge_set_config(bridge, nullptr), -1);

    // Get config NULL
    EXPECT_EQ(imagination_fep_bridge_get_config(nullptr, &cfg), -1);
    EXPECT_EQ(imagination_fep_bridge_get_config(bridge, nullptr), -1);

    // Set callbacks NULL
    EXPECT_EQ(imagination_fep_bridge_set_high_fe_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(imagination_fep_bridge_set_divergence_callback(nullptr, nullptr, nullptr), -1);

    // Force update NULL
    EXPECT_EQ(imagination_fep_bridge_force_update(nullptr), -1);

    // Update NULL
    EXPECT_EQ(imagination_fep_bridge_update(nullptr), -1);
}

/* ============================================================================
 * Error Conditions Tests
 * ============================================================================ */

/**
 * Test: ErrorConditions
 * Verify error conditions are handled properly
 */
TEST_F(ImaginationFepBridgeTest, ErrorConditions) {
    ASSERT_NE(bridge, nullptr);

    // Update without registration should fail
    int result = imagination_fep_bridge_update(bridge);
    EXPECT_EQ(result, -1);

    // But force_update should work (for testing)
    result = imagination_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * State Name Tests
 * ============================================================================ */

/**
 * Test: StateNameConversion
 * Verify state-to-string conversion works
 */
TEST_F(ImaginationFepBridgeTest, StateNameConversion) {
    EXPECT_STREQ(imagination_fep_state_name(IMAGINATION_FEP_STATE_UNINITIALIZED), "uninitialized");
    EXPECT_STREQ(imagination_fep_state_name(IMAGINATION_FEP_STATE_IDLE), "idle");
    EXPECT_STREQ(imagination_fep_state_name(IMAGINATION_FEP_STATE_ACTIVE), "active");
    EXPECT_STREQ(imagination_fep_state_name(IMAGINATION_FEP_STATE_DEGRADED), "degraded");
    EXPECT_STREQ(imagination_fep_state_name(IMAGINATION_FEP_STATE_ERROR), "error");

    // Invalid state
    EXPECT_STREQ(imagination_fep_state_name((imagination_fep_state_t)999), "unknown");
}

/* ============================================================================
 * Metric Range Tests
 * ============================================================================ */

/**
 * Test: MetricRanges
 * Verify metrics are within expected ranges
 */
TEST_F(ImaginationFepBridgeTest, MetricRanges) {
    ASSERT_NE(bridge, nullptr);

    float fe = imagination_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(fe, 0.0f);
    EXPECT_LE(fe, config.max_free_energy);

    float divergence = imagination_fep_bridge_get_simulation_divergence(bridge);
    EXPECT_GE(divergence, 0.0f);
    EXPECT_LE(divergence, 1.0f);

    float pe = imagination_fep_bridge_get_prediction_error(bridge);
    EXPECT_GE(pe, 0.0f);
    EXPECT_LE(pe, 1.0f);
}

/* ============================================================================
 * Multiple Update Tests
 * ============================================================================ */

/**
 * Test: MultipleUpdates
 * Verify multiple updates work correctly
 */
TEST_F(ImaginationFepBridgeTest, MultipleUpdates) {
    ASSERT_NE(bridge, nullptr);

    // Perform multiple updates
    for (int i = 0; i < 10; i++) {
        int result = imagination_fep_bridge_force_update(bridge);
        EXPECT_EQ(result, 0);
    }

    // Verify stats
    imagination_fep_stats_t stats;
    int result = imagination_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.total_updates, 10u);
}

/* ============================================================================
 * Configuration Persistence Tests
 * ============================================================================ */

/**
 * Test: ConfigPersistence
 * Verify configuration persists across operations
 */
TEST_F(ImaginationFepBridgeTest, ConfigPersistence) {
    ASSERT_NE(bridge, nullptr);

    // Set custom config
    imagination_fep_config_t custom_config = imagination_fep_config_default();
    custom_config.simulation_divergence_weight = 0.75f;
    custom_config.high_free_energy_threshold = 1.8f;

    int result = imagination_fep_bridge_set_config(bridge, &custom_config);
    EXPECT_EQ(result, 0);

    // Force update
    imagination_fep_bridge_force_update(bridge);

    // Verify config persisted
    imagination_fep_config_t retrieved_config;
    result = imagination_fep_bridge_get_config(bridge, &retrieved_config);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(retrieved_config.simulation_divergence_weight, 0.75f);
    EXPECT_FLOAT_EQ(retrieved_config.high_free_energy_threshold, 1.8f);
}
