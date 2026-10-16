/**
 * @file test_mirror_empathy_fep_bridge.cpp
 * @brief Unit tests for Mirror-Empathy FEP Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for FEP-Mirror-Empathy bidirectional integration
 * WHY:  Ensure free energy computation from social cognition works correctly
 * HOW:  Test lifecycle, registration, metrics, callbacks, and FEP update cycle
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Configuration Validation
 * - FEP Registration/Unregistration
 * - Update Callback Mechanism
 * - Free Energy Computation
 * - Mirroring Error Effects
 * - High Resonance Detection
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

#include "cognitive/integration/nimcp_mirror_empathy_fep_bridge.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_high_fe_callback_count{0};
static std::atomic<int> g_surprise_callback_count{0};
static std::atomic<int> g_metrics_callback_count{0};
static float g_last_free_energy = 0.0f;
static float g_last_surprise = 0.0f;
static std::string g_last_surprise_source;
static me_fep_metrics_t g_last_metrics;

/**
 * Test callback for high free energy events
 */
static void test_high_fe_callback(
    me_fep_bridge_t* bridge,
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
    me_fep_bridge_t* bridge,
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
    me_fep_bridge_t* bridge,
    const me_fep_metrics_t* metrics,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    g_metrics_callback_count++;
    if (metrics != nullptr) {
        memcpy(&g_last_metrics, metrics, sizeof(me_fep_metrics_t));
    }
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MirrorEmpathyFepBridgeTest : public ::testing::Test {
protected:
    me_fep_bridge_t* bridge = nullptr;
    me_fep_config_t config;

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
        config = me_fep_config_default();
        bridge = me_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            me_fep_bridge_destroy(bridge);
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
TEST_F(MirrorEmpathyFepBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify initial state
    me_fep_state_t state = me_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, ME_FEP_STATE_IDLE) << "Initial state should be IDLE";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(MirrorEmpathyFepBridgeTest, BridgeCreationNullConfig) {
    me_fep_bridge_t* br = me_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    me_fep_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(MirrorEmpathyFepBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    me_fep_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    me_fep_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(MirrorEmpathyFepBridgeTest, DefaultConfig) {
    me_fep_config_t default_config = me_fep_config_default();

    // Verify weights are positive
    EXPECT_GT(default_config.free_energy_weight, 0.0f);
    EXPECT_GT(default_config.mirroring_accuracy_weight, 0.0f);
    EXPECT_GT(default_config.empathy_prediction_weight, 0.0f);
    EXPECT_GT(default_config.emotional_resonance_weight, 0.0f);

    // Verify thresholds are sensible
    EXPECT_GT(default_config.high_free_energy_threshold, 0.0f);
    EXPECT_GT(default_config.prediction_error_threshold, 0.0f);
    EXPECT_GT(default_config.resonance_epsilon, 0.0f);

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
TEST_F(MirrorEmpathyFepBridgeTest, ConfigValidation) {
    me_fep_config_t custom_config = me_fep_config_default();
    custom_config.mirroring_accuracy_weight = 0.5f;
    custom_config.empathy_prediction_weight = 0.3f;
    custom_config.emotional_resonance_weight = 0.2f;
    custom_config.high_free_energy_threshold = 1.8f;

    me_fep_bridge_t* custom_bridge = me_fep_bridge_create(&custom_config);
    ASSERT_NE(custom_bridge, nullptr);

    me_fep_config_t retrieved_config;
    int result = me_fep_bridge_get_config(custom_bridge, &retrieved_config);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(retrieved_config.mirroring_accuracy_weight, 0.5f);
    EXPECT_FLOAT_EQ(retrieved_config.empathy_prediction_weight, 0.3f);
    EXPECT_FLOAT_EQ(retrieved_config.emotional_resonance_weight, 0.2f);
    EXPECT_FLOAT_EQ(retrieved_config.high_free_energy_threshold, 1.8f);

    me_fep_bridge_destroy(custom_bridge);
}

/**
 * Test: BridgeReset
 * Verify bridge can be reset to initial state
 */
TEST_F(MirrorEmpathyFepBridgeTest, BridgeReset) {
    ASSERT_NE(bridge, nullptr);

    // Force an update to change state
    me_fep_bridge_update_mirroring_error(bridge, 0.8f);
    int result = me_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Reset bridge
    result = me_fep_bridge_reset(bridge);
    EXPECT_EQ(result, 0) << "Reset should succeed";

    // Verify state is back to IDLE
    me_fep_state_t state = me_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, ME_FEP_STATE_IDLE) << "State should be IDLE after reset";

    // Verify metrics are reset
    float me = me_fep_bridge_get_mirroring_error(bridge);
    EXPECT_FLOAT_EQ(me, 0.0f) << "Mirroring error should be reset";
}

/* ============================================================================
 * FEP Registration Tests
 * ============================================================================ */

/**
 * Test: RegisterWithFEP
 * Verify bridge registration state without actual orchestrator
 */
TEST_F(MirrorEmpathyFepBridgeTest, RegisterWithFEP) {
    ASSERT_NE(bridge, nullptr);

    // Before registration, should not be registered
    EXPECT_FALSE(me_fep_bridge_is_registered(bridge))
        << "Should not be registered initially";

    // Without actual orchestrator, ID should be 0
    uint32_t id = me_fep_bridge_get_id(bridge);
    EXPECT_EQ(id, 0u) << "ID should be 0 when not registered";
}

/**
 * Test: UnregisterFromFEP
 * Verify bridge can unregister cleanly
 */
TEST_F(MirrorEmpathyFepBridgeTest, UnregisterFromFEP) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should handle gracefully
    int result = me_fep_bridge_unregister(bridge);
    EXPECT_EQ(result, 0) << "Unregister when not registered should succeed";

    EXPECT_FALSE(me_fep_bridge_is_registered(bridge))
        << "Should not be registered after unregister";
}

/**
 * Test: RegistrationNullParams
 * Verify registration handles NULL parameters gracefully
 */
TEST_F(MirrorEmpathyFepBridgeTest, RegistrationNullParams) {
    ASSERT_NE(bridge, nullptr);

    uint32_t bridge_id = 0;

    // NULL bridge
    int result = me_fep_bridge_register(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1) << "Register with NULL bridge should fail";

    // NULL orchestrator
    result = me_fep_bridge_register(bridge, nullptr, nullptr, &bridge_id);
    EXPECT_EQ(result, -1) << "Register without orchestrator should fail";
}

/* ============================================================================
 * FEP Update Callback Tests
 * ============================================================================ */

/**
 * Test: UpdateCallback
 * Verify FEP update callback behavior
 */
TEST_F(MirrorEmpathyFepBridgeTest, UpdateCallback) {
    ASSERT_NE(bridge, nullptr);

    // Call update callback directly - should fail without registration
    int result = me_fep_update_callback(bridge);
    EXPECT_EQ(result, -1) << "Update callback should fail without registration";
}

/**
 * Test: ForceUpdate
 * Verify force update triggers FEP computation
 */
TEST_F(MirrorEmpathyFepBridgeTest, ForceUpdate) {
    ASSERT_NE(bridge, nullptr);

    // Get initial metrics
    me_fep_metrics_t initial_metrics;
    int result = me_fep_bridge_get_metrics(bridge, &initial_metrics);
    EXPECT_EQ(result, 0);

    // Force update
    result = me_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Get updated metrics
    me_fep_metrics_t updated_metrics;
    result = me_fep_bridge_get_metrics(bridge, &updated_metrics);
    EXPECT_EQ(result, 0);

    // Update count should have increased
    EXPECT_GT(updated_metrics.update_count, initial_metrics.update_count)
        << "Update count should increase after force update";
}

/**
 * Test: ForceUpdateNull
 * Verify force update handles NULL gracefully
 */
TEST_F(MirrorEmpathyFepBridgeTest, ForceUpdateNull) {
    int result = me_fep_bridge_force_update(nullptr);
    EXPECT_EQ(result, -1) << "Force update with NULL should fail";
}

/* ============================================================================
 * Free Energy Computation Tests
 * ============================================================================ */

/**
 * Test: FreeEnergyContribution
 * Verify free energy is calculated from mirror-empathy metrics
 */
TEST_F(MirrorEmpathyFepBridgeTest, FreeEnergyContribution) {
    ASSERT_NE(bridge, nullptr);

    // Get initial free energy
    float initial_fe = me_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(initial_fe, 0.0f) << "Free energy should be non-negative";
    EXPECT_LE(initial_fe, config.max_free_energy)
        << "Free energy should not exceed maximum";

    // Set high mirroring error
    me_fep_bridge_update_mirroring_error(bridge, 0.8f);
    me_fep_bridge_force_update(bridge);

    float updated_fe = me_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(updated_fe, initial_fe)
        << "Higher mirroring error should increase free energy";
}

/**
 * Test: MirroringErrorIncreasesFreeEnergy
 * Verify that higher mirroring error leads to higher free energy
 */
TEST_F(MirrorEmpathyFepBridgeTest, MirroringErrorIncreasesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Low error
    me_fep_bridge_update_mirroring_error(bridge, 0.1f);
    me_fep_bridge_force_update(bridge);
    float low_fe = me_fep_bridge_get_free_energy(bridge);

    // Reset and set high error
    me_fep_bridge_reset(bridge);
    me_fep_bridge_update_mirroring_error(bridge, 0.9f);
    me_fep_bridge_force_update(bridge);
    float high_fe = me_fep_bridge_get_free_energy(bridge);

    EXPECT_GT(high_fe, low_fe)
        << "High mirroring error should produce higher free energy than low";
}

/**
 * Test: HighResonanceReducesFreeEnergy
 * Verify that high resonance (low deficit) reduces free energy
 */
TEST_F(MirrorEmpathyFepBridgeTest, HighResonanceReducesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // High resonance deficit (low resonance)
    me_fep_bridge_update_resonance_deficit(bridge, 0.9f);
    me_fep_bridge_force_update(bridge);
    float low_res_fe = me_fep_bridge_get_free_energy(bridge);

    // Reset and set low resonance deficit (high resonance)
    me_fep_bridge_reset(bridge);
    me_fep_bridge_update_resonance_deficit(bridge, 0.1f);
    me_fep_bridge_force_update(bridge);
    float high_res_fe = me_fep_bridge_get_free_energy(bridge);

    EXPECT_LT(high_res_fe, low_res_fe)
        << "High resonance should produce lower free energy";
}

/**
 * Test: EmpathyErrorIncreasesFreeEnergy
 * Verify that higher empathy prediction error leads to higher free energy
 */
TEST_F(MirrorEmpathyFepBridgeTest, EmpathyErrorIncreasesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Low error
    me_fep_bridge_update_empathy_error(bridge, 0.1f);
    me_fep_bridge_force_update(bridge);
    float low_fe = me_fep_bridge_get_free_energy(bridge);

    // Reset and set high error
    me_fep_bridge_reset(bridge);
    me_fep_bridge_update_empathy_error(bridge, 0.9f);
    me_fep_bridge_force_update(bridge);
    float high_fe = me_fep_bridge_get_free_energy(bridge);

    EXPECT_GT(high_fe, low_fe)
        << "High empathy error should produce higher free energy than low";
}

/**
 * Test: FreeEnergyNull
 * Verify get_free_energy handles NULL gracefully
 */
TEST_F(MirrorEmpathyFepBridgeTest, FreeEnergyNull) {
    float fe = me_fep_bridge_get_free_energy(nullptr);
    EXPECT_LT(fe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: FreeEnergyBaseline
 * Verify baseline free energy is used when idle
 */
TEST_F(MirrorEmpathyFepBridgeTest, FreeEnergyBaseline) {
    ASSERT_NE(bridge, nullptr);

    // In idle state with no uncertainty, free energy should be near baseline
    float fe = me_fep_bridge_get_free_energy(bridge);
    // Allow some tolerance
    EXPECT_GE(fe, config.baseline_free_energy * 0.5f);
    EXPECT_LE(fe, config.baseline_free_energy * 3.0f);
}

/* ============================================================================
 * High Resonance State Tests
 * ============================================================================ */

/**
 * Test: HighResonanceDetection
 * Verify high resonance state is detected correctly
 */
TEST_F(MirrorEmpathyFepBridgeTest, HighResonanceDetection) {
    ASSERT_NE(bridge, nullptr);

    // Initially not at high resonance
    EXPECT_FALSE(me_fep_bridge_is_high_resonance(bridge))
        << "Should not be at high resonance initially";

    // Set very low deficit (high resonance, below epsilon)
    me_fep_bridge_update_resonance_deficit(bridge, 0.05f);
    me_fep_bridge_force_update(bridge);

    EXPECT_TRUE(me_fep_bridge_is_high_resonance(bridge))
        << "Should be at high resonance when deficit < epsilon";
}

/* ============================================================================
 * Prediction Error Tests
 * ============================================================================ */

/**
 * Test: PredictionError
 * Verify prediction error tracking
 */
TEST_F(MirrorEmpathyFepBridgeTest, PredictionError) {
    ASSERT_NE(bridge, nullptr);

    // Get initial prediction error
    float initial_pe = me_fep_bridge_get_prediction_error(bridge);
    EXPECT_GE(initial_pe, 0.0f) << "Prediction error should be non-negative";
    EXPECT_LE(initial_pe, 1.0f) << "Prediction error should be <= 1.0";
}

/**
 * Test: PredictionErrorNull
 * Verify get_prediction_error handles NULL gracefully
 */
TEST_F(MirrorEmpathyFepBridgeTest, PredictionErrorNull) {
    float pe = me_fep_bridge_get_prediction_error(nullptr);
    EXPECT_LT(pe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: EmpathyErrorAffectsPrediction
 * Verify empathy prediction error contributes to overall prediction error
 */
TEST_F(MirrorEmpathyFepBridgeTest, EmpathyErrorAffectsPrediction) {
    ASSERT_NE(bridge, nullptr);

    // Set high empathy error
    me_fep_bridge_update_empathy_error(bridge, 0.9f);
    me_fep_bridge_force_update(bridge);

    float pe = me_fep_bridge_get_prediction_error(bridge);
    EXPECT_GT(pe, 0.0f) << "High empathy error should produce prediction error";
}

/* ============================================================================
 * Metrics Tracking Tests
 * ============================================================================ */

/**
 * Test: MetricsTracking
 * Verify metrics are updated correctly during FEP cycles
 */
TEST_F(MirrorEmpathyFepBridgeTest, MetricsTracking) {
    ASSERT_NE(bridge, nullptr);

    me_fep_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    int result = me_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(result, 0) << "Get metrics should succeed";

    // Verify core FEP metrics are valid
    EXPECT_GE(metrics.free_energy, 0.0f);
    EXPECT_GE(metrics.prediction_error, 0.0f);
    EXPECT_LE(metrics.prediction_error, 1.0f);
    EXPECT_GE(metrics.entropy, 0.0f);

    // Verify mirror-empathy specific metrics are valid
    EXPECT_GE(metrics.mirroring_error, 0.0f);
    EXPECT_LE(metrics.mirroring_error, 1.0f);
    EXPECT_GE(metrics.empathy_prediction_error, 0.0f);
    EXPECT_LE(metrics.empathy_prediction_error, 1.0f);
    EXPECT_GE(metrics.resonance_deficit, 0.0f);
    EXPECT_LE(metrics.resonance_deficit, 1.0f);
}

/**
 * Test: MetricsTrackingNull
 * Verify get_metrics handles NULL parameters gracefully
 */
TEST_F(MirrorEmpathyFepBridgeTest, MetricsTrackingNull) {
    me_fep_metrics_t metrics;

    int result = me_fep_bridge_get_metrics(nullptr, &metrics);
    EXPECT_EQ(result, -1) << "Get metrics with NULL bridge should fail";

    result = me_fep_bridge_get_metrics(bridge, nullptr);
    EXPECT_EQ(result, -1) << "Get metrics with NULL output should fail";
}

/**
 * Test: StatisticsTracking
 * Verify statistics are accumulated correctly
 */
TEST_F(MirrorEmpathyFepBridgeTest, StatisticsTracking) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        me_fep_bridge_force_update(bridge);
    }

    me_fep_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = me_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify stats are accumulated
    EXPECT_GE(stats.total_updates, 5u) << "Should have at least 5 updates";
    EXPECT_GE(stats.avg_free_energy, 0.0f);
}

/**
 * Test: StatsReset
 * Verify statistics can be reset
 */
TEST_F(MirrorEmpathyFepBridgeTest, StatsReset) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        me_fep_bridge_force_update(bridge);
    }

    // Reset stats
    int result = me_fep_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    me_fep_stats_t stats;
    result = me_fep_bridge_get_stats(bridge, &stats);
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
TEST_F(MirrorEmpathyFepBridgeTest, GetState) {
    ASSERT_NE(bridge, nullptr);

    me_fep_state_t state = me_fep_bridge_get_state(bridge);
    // State should be one of the valid states
    EXPECT_GE((int)state, (int)ME_FEP_STATE_UNINITIALIZED);
    EXPECT_LE((int)state, (int)ME_FEP_STATE_ERROR);
}

/**
 * Test: GetStateNull
 * Verify get_state handles NULL gracefully
 */
TEST_F(MirrorEmpathyFepBridgeTest, GetStateNull) {
    me_fep_state_t state = me_fep_bridge_get_state(nullptr);
    EXPECT_EQ(state, ME_FEP_STATE_ERROR)
        << "NULL bridge should return ERROR state";
}

/**
 * Test: IsDegraded
 * Verify degraded mode detection
 */
TEST_F(MirrorEmpathyFepBridgeTest, IsDegraded) {
    ASSERT_NE(bridge, nullptr);

    // Initially should not be degraded
    bool degraded = me_fep_bridge_is_degraded(bridge);
    EXPECT_FALSE(degraded) << "Should not be degraded initially";
}

/**
 * Test: StateName
 * Verify state name conversion
 */
TEST_F(MirrorEmpathyFepBridgeTest, StateName) {
    const char* name = me_fep_state_name(ME_FEP_STATE_IDLE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u) << "State name should not be empty";

    name = me_fep_state_name(ME_FEP_STATE_ACTIVE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = me_fep_state_name(ME_FEP_STATE_DEGRADED);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = me_fep_state_name(ME_FEP_STATE_ERROR);
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
TEST_F(MirrorEmpathyFepBridgeTest, HighFECallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = me_fep_bridge_set_high_fe_callback(
        bridge, test_high_fe_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = me_fep_bridge_set_high_fe_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: SurpriseCallback
 * Verify surprise event callback registration
 */
TEST_F(MirrorEmpathyFepBridgeTest, SurpriseCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = me_fep_bridge_set_surprise_callback(
        bridge, test_surprise_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = me_fep_bridge_set_surprise_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: MetricsCallback
 * Verify metrics update callback registration
 */
TEST_F(MirrorEmpathyFepBridgeTest, MetricsCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = me_fep_bridge_set_metrics_callback(
        bridge, test_metrics_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Force update to trigger callback
    me_fep_bridge_force_update(bridge);

    // Metrics callback should have been called
    EXPECT_GE(g_metrics_callback_count.load(), 1)
        << "Metrics callback should be called on update";

    // Clear callback
    result = me_fep_bridge_set_metrics_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: CallbackNull
 * Verify callback registration handles NULL bridge
 */
TEST_F(MirrorEmpathyFepBridgeTest, CallbackNull) {
    int result = me_fep_bridge_set_high_fe_callback(
        nullptr, test_high_fe_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = me_fep_bridge_set_surprise_callback(
        nullptr, test_surprise_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = me_fep_bridge_set_metrics_callback(
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
TEST_F(MirrorEmpathyFepBridgeTest, SetConfig) {
    ASSERT_NE(bridge, nullptr);

    me_fep_config_t new_config = me_fep_config_default();
    new_config.mirroring_accuracy_weight = 0.5f;
    new_config.empathy_prediction_weight = 0.3f;
    new_config.emotional_resonance_weight = 0.2f;

    int result = me_fep_bridge_set_config(bridge, &new_config);
    EXPECT_EQ(result, 0) << "Set config should succeed";
}

/**
 * Test: SetConfigNull
 * Verify set_config handles NULL parameters
 */
TEST_F(MirrorEmpathyFepBridgeTest, SetConfigNull) {
    me_fep_config_t config_val = me_fep_config_default();

    int result = me_fep_bridge_set_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = me_fep_bridge_set_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL config should fail";
}

/**
 * Test: GetConfig
 * Verify configuration can be retrieved
 */
TEST_F(MirrorEmpathyFepBridgeTest, GetConfig) {
    ASSERT_NE(bridge, nullptr);

    me_fep_config_t retrieved_config;
    memset(&retrieved_config, 0, sizeof(retrieved_config));

    int result = me_fep_bridge_get_config(bridge, &retrieved_config);
    EXPECT_EQ(result, 0) << "Get config should succeed";

    // Verify retrieved config has valid values
    EXPECT_GT(retrieved_config.mirroring_accuracy_weight, 0.0f);
    EXPECT_GT(retrieved_config.max_free_energy, 0.0f);
}

/**
 * Test: GetConfigNull
 * Verify get_config handles NULL parameters
 */
TEST_F(MirrorEmpathyFepBridgeTest, GetConfigNull) {
    me_fep_config_t config_val;

    int result = me_fep_bridge_get_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = me_fep_bridge_get_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL output should fail";
}

/* ============================================================================
 * Manual Update Operations Tests
 * ============================================================================ */

/**
 * Test: UpdateMirroringError
 * Verify manual mirroring error update
 */
TEST_F(MirrorEmpathyFepBridgeTest, UpdateMirroringError) {
    ASSERT_NE(bridge, nullptr);

    int result = me_fep_bridge_update_mirroring_error(bridge, 0.75f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    float me = me_fep_bridge_get_mirroring_error(bridge);
    EXPECT_FLOAT_EQ(me, 0.75f) << "Value should be updated";
}

/**
 * Test: UpdateEmpathyError
 * Verify manual empathy error update
 */
TEST_F(MirrorEmpathyFepBridgeTest, UpdateEmpathyError) {
    ASSERT_NE(bridge, nullptr);

    int result = me_fep_bridge_update_empathy_error(bridge, 0.65f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    me_fep_metrics_t metrics;
    me_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_FLOAT_EQ(metrics.empathy_prediction_error, 0.65f)
        << "Value should be updated";
}

/**
 * Test: UpdateResonanceDeficit
 * Verify manual resonance deficit update
 */
TEST_F(MirrorEmpathyFepBridgeTest, UpdateResonanceDeficit) {
    ASSERT_NE(bridge, nullptr);

    int result = me_fep_bridge_update_resonance_deficit(bridge, 0.25f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    me_fep_metrics_t metrics;
    me_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_FLOAT_EQ(metrics.resonance_deficit, 0.25f) << "Value should be updated";
}

/**
 * Test: UpdateClamps
 * Verify updates are clamped to valid range
 */
TEST_F(MirrorEmpathyFepBridgeTest, UpdateClamps) {
    ASSERT_NE(bridge, nullptr);

    // Test clamping above 1.0
    me_fep_bridge_update_mirroring_error(bridge, 2.0f);
    float me = me_fep_bridge_get_mirroring_error(bridge);
    EXPECT_LE(me, 1.0f) << "Should be clamped to max 1.0";

    // Test clamping below 0.0
    me_fep_bridge_update_mirroring_error(bridge, -0.5f);
    me = me_fep_bridge_get_mirroring_error(bridge);
    EXPECT_GE(me, 0.0f) << "Should be clamped to min 0.0";
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Comprehensive test for NULL parameter handling across all functions
 */
TEST_F(MirrorEmpathyFepBridgeTest, NullHandling) {
    // Lifecycle
    me_fep_bridge_destroy(nullptr);

    // Registration
    EXPECT_EQ(me_fep_bridge_reset(nullptr), -1);
    EXPECT_FALSE(me_fep_bridge_is_registered(nullptr));
    EXPECT_EQ(me_fep_bridge_get_id(nullptr), 0u);
    EXPECT_EQ(me_fep_bridge_unregister(nullptr), -1);

    // Updates
    EXPECT_EQ(me_fep_update_callback(nullptr), -1);
    EXPECT_EQ(me_fep_bridge_force_update(nullptr), -1);
    EXPECT_EQ(me_fep_bridge_update_mirroring_error(nullptr, 0.5f), -1);
    EXPECT_EQ(me_fep_bridge_update_empathy_error(nullptr, 0.5f), -1);
    EXPECT_EQ(me_fep_bridge_update_resonance_deficit(nullptr, 0.5f), -1);

    // Metrics
    me_fep_metrics_t metrics;
    EXPECT_EQ(me_fep_bridge_get_metrics(nullptr, &metrics), -1);
    EXPECT_EQ(me_fep_bridge_get_metrics(bridge, nullptr), -1);

    me_fep_stats_t stats;
    EXPECT_EQ(me_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(me_fep_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(me_fep_bridge_reset_stats(nullptr), -1);

    // State queries
    EXPECT_EQ(me_fep_bridge_get_state(nullptr), ME_FEP_STATE_ERROR);
    EXPECT_LT(me_fep_bridge_get_free_energy(nullptr), 0.0f);
    EXPECT_LT(me_fep_bridge_get_prediction_error(nullptr), 0.0f);
    EXPECT_LT(me_fep_bridge_get_mirroring_error(nullptr), 0.0f);
    EXPECT_FALSE(me_fep_bridge_is_degraded(nullptr));
    EXPECT_FALSE(me_fep_bridge_is_high_resonance(nullptr));

    // Callbacks
    EXPECT_EQ(me_fep_bridge_set_high_fe_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(me_fep_bridge_set_surprise_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(me_fep_bridge_set_metrics_callback(nullptr, nullptr, nullptr), -1);

    // Config
    me_fep_config_t cfg;
    EXPECT_EQ(me_fep_bridge_set_config(nullptr, &cfg), -1);
    EXPECT_EQ(me_fep_bridge_set_config(bridge, nullptr), -1);
    EXPECT_EQ(me_fep_bridge_get_config(nullptr, &cfg), -1);
    EXPECT_EQ(me_fep_bridge_get_config(bridge, nullptr), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/* ============================================================================
 * Error Conditions Tests
 * ============================================================================ */

/**
 * Test: ErrorConditions
 * Verify proper error handling for various edge cases
 */
TEST_F(MirrorEmpathyFepBridgeTest, ErrorConditions) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should succeed (no-op)
    int result = me_fep_bridge_unregister(bridge);
    EXPECT_EQ(result, 0) << "Unregister when not registered should be no-op";

    // Double destroy should be safe (after first destroy, pointer is invalid
    // but we test with different bridges)
    me_fep_bridge_t* temp_bridge = me_fep_bridge_create(nullptr);
    ASSERT_NE(temp_bridge, nullptr);
    me_fep_bridge_destroy(temp_bridge);
    // temp_bridge is now invalid, but destroy(nullptr) should be safe
    me_fep_bridge_destroy(nullptr);

    SUCCEED() << "Error condition tests passed";
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Verify basic thread safety with concurrent access
 */
TEST_F(MirrorEmpathyFepBridgeTest, ThreadSafety) {
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
                me_fep_bridge_get_free_energy(bridge);
                me_fep_bridge_get_prediction_error(bridge);
                me_fep_bridge_get_mirroring_error(bridge);
                me_fep_bridge_get_state(bridge);
                me_fep_bridge_is_degraded(bridge);
                me_fep_bridge_is_high_resonance(bridge);

                me_fep_metrics_t metrics;
                me_fep_bridge_get_metrics(bridge, &metrics);

                me_fep_stats_t stats;
                me_fep_bridge_get_stats(bridge, &stats);

                // Write operations
                me_fep_bridge_update_mirroring_error(bridge,
                    (float)(i % 10) / 10.0f);
                me_fep_bridge_force_update(bridge);
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
TEST_F(MirrorEmpathyFepBridgeTest, DestroyCallback) {
    ASSERT_NE(bridge, nullptr);

    // Destroy callback should be safe to call
    me_fep_destroy_callback(bridge);
    SUCCEED() << "Destroy callback should not crash";

    // Should also handle NULL
    me_fep_destroy_callback(nullptr);
    SUCCEED() << "Destroy callback with NULL should not crash";
}

/* ============================================================================
 * Integration Scenario Tests
 * ============================================================================ */

/**
 * Test: SocialCognitionScenario
 * Simulate a social cognition scenario
 */
TEST_F(MirrorEmpathyFepBridgeTest, SocialCognitionScenario) {
    ASSERT_NE(bridge, nullptr);

    // Register metrics callback to track changes
    me_fep_bridge_set_metrics_callback(bridge, test_metrics_callback, nullptr);

    // Scenario: Start with poor social understanding, gradually build resonance

    // Phase 1: Initial poor understanding
    me_fep_bridge_update_mirroring_error(bridge, 0.9f);
    me_fep_bridge_update_empathy_error(bridge, 0.8f);
    me_fep_bridge_update_resonance_deficit(bridge, 0.95f);
    me_fep_bridge_force_update(bridge);

    float initial_fe = me_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(initial_fe, config.baseline_free_energy)
        << "Poor social understanding should produce elevated free energy";

    // Phase 2: Improving action understanding
    me_fep_bridge_update_mirroring_error(bridge, 0.4f);
    me_fep_bridge_force_update(bridge);

    float learning_fe = me_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(learning_fe, initial_fe)
        << "Better action understanding should reduce free energy";

    // Phase 3: Building empathic connection
    me_fep_bridge_update_empathy_error(bridge, 0.3f);
    me_fep_bridge_force_update(bridge);

    float empathic_fe = me_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(empathic_fe, learning_fe)
        << "Better empathy should reduce free energy";

    // Phase 4: High resonance achieved
    me_fep_bridge_update_resonance_deficit(bridge, 0.05f);  // Below epsilon
    me_fep_bridge_force_update(bridge);

    EXPECT_TRUE(me_fep_bridge_is_high_resonance(bridge))
        << "Should detect high resonance state";

    float final_fe = me_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(final_fe, empathic_fe)
        << "High resonance should have lowest free energy";

    // Verify callback was called for each update
    EXPECT_GE(g_metrics_callback_count.load(), 4)
        << "Metrics callback should be called for each update";
}

/**
 * Test: DegradedModeEntry
 * Verify degraded mode is entered with high free energy
 */
TEST_F(MirrorEmpathyFepBridgeTest, DegradedModeEntry) {
    ASSERT_NE(bridge, nullptr);

    // Register high FE callback
    me_fep_bridge_set_high_fe_callback(bridge, test_high_fe_callback, nullptr);

    // Set all error metrics to maximum
    me_fep_bridge_update_mirroring_error(bridge, 1.0f);
    me_fep_bridge_update_empathy_error(bridge, 1.0f);
    me_fep_bridge_update_resonance_deficit(bridge, 1.0f);
    me_fep_bridge_force_update(bridge);

    // Should enter degraded mode due to high free energy
    float fe = me_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(fe, config.high_free_energy_threshold)
        << "Free energy should exceed threshold";

    EXPECT_TRUE(me_fep_bridge_is_degraded(bridge))
        << "Should be in degraded mode";

    // Callback should have been invoked
    EXPECT_GE(g_high_fe_callback_count.load(), 1)
        << "High FE callback should be called";
}

/**
 * Test: SurpriseEventGeneration
 * Verify surprise events are generated on sudden changes
 */
TEST_F(MirrorEmpathyFepBridgeTest, SurpriseEventGeneration) {
    ASSERT_NE(bridge, nullptr);

    // Register surprise callback
    me_fep_bridge_set_surprise_callback(bridge, test_surprise_callback, nullptr);

    // Start at low error state
    me_fep_bridge_update_mirroring_error(bridge, 0.1f);
    me_fep_bridge_update_empathy_error(bridge, 0.1f);
    me_fep_bridge_update_resonance_deficit(bridge, 0.1f);
    me_fep_bridge_force_update(bridge);

    // Sudden large change - should trigger surprise
    me_fep_bridge_update_mirroring_error(bridge, 0.95f);
    me_fep_bridge_force_update(bridge);

    // Check if surprise was generated (depends on threshold)
    me_fep_metrics_t metrics;
    me_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_GT(metrics.surprise, 0.0f)
        << "Sudden change should generate surprise";
}

/**
 * Test: AllComponentsContribute
 * Verify all components contribute to free energy
 */
TEST_F(MirrorEmpathyFepBridgeTest, AllComponentsContribute) {
    ASSERT_NE(bridge, nullptr);

    // Start fresh
    me_fep_bridge_reset(bridge);

    // Test mirroring contribution alone
    me_fep_bridge_update_mirroring_error(bridge, 0.8f);
    me_fep_bridge_update_empathy_error(bridge, 0.0f);
    me_fep_bridge_update_resonance_deficit(bridge, 0.0f);
    me_fep_bridge_force_update(bridge);

    me_fep_metrics_t metrics;
    me_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_GT(metrics.mirroring_contribution, 0.0f)
        << "Mirroring should contribute when error is set";

    // Reset and test empathy contribution alone
    me_fep_bridge_reset(bridge);
    me_fep_bridge_update_mirroring_error(bridge, 0.0f);
    me_fep_bridge_update_empathy_error(bridge, 0.8f);
    me_fep_bridge_update_resonance_deficit(bridge, 0.0f);
    me_fep_bridge_force_update(bridge);

    me_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_GT(metrics.empathy_contribution, 0.0f)
        << "Empathy should contribute when error is set";

    // Reset and test resonance contribution alone
    me_fep_bridge_reset(bridge);
    me_fep_bridge_update_mirroring_error(bridge, 0.0f);
    me_fep_bridge_update_empathy_error(bridge, 0.0f);
    me_fep_bridge_update_resonance_deficit(bridge, 0.8f);
    me_fep_bridge_force_update(bridge);

    me_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_GT(metrics.resonance_contribution, 0.0f)
        << "Resonance should contribute when deficit is set";
}

/**
 * Test: PeakFreeEnergyTracking
 * Verify peak free energy is tracked in stats
 */
TEST_F(MirrorEmpathyFepBridgeTest, PeakFreeEnergyTracking) {
    ASSERT_NE(bridge, nullptr);

    // Reset stats
    me_fep_bridge_reset_stats(bridge);

    // Generate low FE
    me_fep_bridge_update_mirroring_error(bridge, 0.1f);
    me_fep_bridge_force_update(bridge);

    me_fep_stats_t stats;
    me_fep_bridge_get_stats(bridge, &stats);
    float first_peak = stats.peak_free_energy;

    // Generate higher FE
    me_fep_bridge_update_mirroring_error(bridge, 0.9f);
    me_fep_bridge_force_update(bridge);

    me_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.peak_free_energy, first_peak)
        << "Peak should increase or stay same";

    // Even after reducing FE, peak should stay
    me_fep_bridge_update_mirroring_error(bridge, 0.1f);
    me_fep_bridge_force_update(bridge);

    float saved_peak = stats.peak_free_energy;
    me_fep_bridge_get_stats(bridge, &stats);
    EXPECT_FLOAT_EQ(stats.peak_free_energy, saved_peak)
        << "Peak should be preserved";
}
