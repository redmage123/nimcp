/**
 * @file test_imagination_reasoning_fep_bridge.cpp
 * @brief Unit tests for Imagination-Reasoning FEP Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for FEP-Imagination-Reasoning bidirectional integration
 * WHY:  Ensure free energy computation from imagination-reasoning coordination works correctly
 * HOW:  Test lifecycle, registration, metrics, callbacks, and FEP update cycle
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Configuration Validation
 * - FEP Registration/Unregistration
 * - Update Callback Mechanism
 * - Free Energy Computation
 * - Scenario Quality Effects
 * - Reasoning Coherence Detection
 * - Counterfactual Validity
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

#include "cognitive/integration/nimcp_imagination_reasoning_fep_bridge.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_high_fe_callback_count{0};
static std::atomic<int> g_surprise_callback_count{0};
static std::atomic<int> g_metrics_callback_count{0};
static float g_last_free_energy = 0.0f;
static float g_last_surprise = 0.0f;
static std::string g_last_surprise_source;
static imag_reason_fep_metrics_t g_last_metrics;

/**
 * Test callback for high free energy events
 */
static void test_high_fe_callback(
    imag_reason_fep_bridge_t* bridge,
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
    imag_reason_fep_bridge_t* bridge,
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
    imag_reason_fep_bridge_t* bridge,
    const imag_reason_fep_metrics_t* metrics,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    g_metrics_callback_count++;
    if (metrics != nullptr) {
        memcpy(&g_last_metrics, metrics, sizeof(imag_reason_fep_metrics_t));
    }
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ImaginationReasoningFepBridgeTest : public ::testing::Test {
protected:
    imag_reason_fep_bridge_t* bridge = nullptr;
    imag_reason_fep_config_t config;

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
        config = imag_reason_fep_config_default();
        bridge = imag_reason_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            imag_reason_fep_bridge_destroy(bridge);
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
TEST_F(ImaginationReasoningFepBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify initial state
    imag_reason_fep_state_t state = imag_reason_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, IMAG_REASON_FEP_STATE_IDLE) << "Initial state should be IDLE";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(ImaginationReasoningFepBridgeTest, BridgeCreationNullConfig) {
    imag_reason_fep_bridge_t* br = imag_reason_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    imag_reason_fep_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(ImaginationReasoningFepBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    imag_reason_fep_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    imag_reason_fep_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(ImaginationReasoningFepBridgeTest, DefaultConfig) {
    imag_reason_fep_config_t default_config = imag_reason_fep_config_default();

    // Verify weights are positive
    EXPECT_GT(default_config.free_energy_weight, 0.0f);
    EXPECT_GT(default_config.scenario_quality_weight, 0.0f);
    EXPECT_GT(default_config.reasoning_coherence_weight, 0.0f);
    EXPECT_GT(default_config.counterfactual_validity_weight, 0.0f);

    // Verify thresholds are sensible
    EXPECT_GT(default_config.high_free_energy_threshold, 0.0f);
    EXPECT_GT(default_config.prediction_error_threshold, 0.0f);
    EXPECT_GT(default_config.coherence_epsilon, 0.0f);

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
TEST_F(ImaginationReasoningFepBridgeTest, ConfigValidation) {
    imag_reason_fep_config_t custom_config = imag_reason_fep_config_default();
    custom_config.scenario_quality_weight = 0.4f;
    custom_config.reasoning_coherence_weight = 0.35f;
    custom_config.counterfactual_validity_weight = 0.25f;
    custom_config.high_free_energy_threshold = 1.8f;

    imag_reason_fep_bridge_t* custom_bridge = imag_reason_fep_bridge_create(&custom_config);
    ASSERT_NE(custom_bridge, nullptr);

    imag_reason_fep_config_t retrieved_config;
    int result = imag_reason_fep_bridge_get_config(custom_bridge, &retrieved_config);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(retrieved_config.scenario_quality_weight, 0.4f);
    EXPECT_FLOAT_EQ(retrieved_config.reasoning_coherence_weight, 0.35f);
    EXPECT_FLOAT_EQ(retrieved_config.counterfactual_validity_weight, 0.25f);
    EXPECT_FLOAT_EQ(retrieved_config.high_free_energy_threshold, 1.8f);

    imag_reason_fep_bridge_destroy(custom_bridge);
}

/**
 * Test: BridgeReset
 * Verify bridge can be reset to initial state
 */
TEST_F(ImaginationReasoningFepBridgeTest, BridgeReset) {
    ASSERT_NE(bridge, nullptr);

    // Force an update to change state
    imag_reason_fep_bridge_update_scenario_quality(bridge, 0.2f);
    int result = imag_reason_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Reset bridge
    result = imag_reason_fep_bridge_reset(bridge);
    EXPECT_EQ(result, 0) << "Reset should succeed";

    // Verify state is back to IDLE
    imag_reason_fep_state_t state = imag_reason_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, IMAG_REASON_FEP_STATE_IDLE) << "State should be IDLE after reset";

    // Verify metrics are reset
    float sq = imag_reason_fep_bridge_get_scenario_quality(bridge);
    EXPECT_FLOAT_EQ(sq, 0.5f) << "Scenario quality should be reset to neutral";
}

/* ============================================================================
 * FEP Registration Tests
 * ============================================================================ */

/**
 * Test: RegisterWithFEP
 * Verify bridge registration state without actual orchestrator
 */
TEST_F(ImaginationReasoningFepBridgeTest, RegisterWithFEP) {
    ASSERT_NE(bridge, nullptr);

    // Before registration, should not be registered
    EXPECT_FALSE(imag_reason_fep_bridge_is_registered(bridge))
        << "Should not be registered initially";

    // Without actual orchestrator, ID should be 0
    uint32_t id = imag_reason_fep_bridge_get_id(bridge);
    EXPECT_EQ(id, 0u) << "ID should be 0 when not registered";
}

/**
 * Test: UnregisterFromFEP
 * Verify bridge can unregister cleanly
 */
TEST_F(ImaginationReasoningFepBridgeTest, UnregisterFromFEP) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should handle gracefully
    int result = imag_reason_fep_bridge_unregister(bridge);
    EXPECT_EQ(result, 0) << "Unregister when not registered should succeed";

    EXPECT_FALSE(imag_reason_fep_bridge_is_registered(bridge))
        << "Should not be registered after unregister";
}

/**
 * Test: RegistrationNullParams
 * Verify registration handles NULL parameters gracefully
 */
TEST_F(ImaginationReasoningFepBridgeTest, RegistrationNullParams) {
    ASSERT_NE(bridge, nullptr);

    uint32_t bridge_id = 0;

    // NULL bridge
    int result = imag_reason_fep_bridge_register(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1) << "Register with NULL bridge should fail";

    // NULL orchestrator
    result = imag_reason_fep_bridge_register(bridge, nullptr, nullptr, &bridge_id);
    EXPECT_EQ(result, -1) << "Register without orchestrator should fail";
}

/* ============================================================================
 * FEP Update Callback Tests
 * ============================================================================ */

/**
 * Test: UpdateCallback
 * Verify FEP update callback behavior
 */
TEST_F(ImaginationReasoningFepBridgeTest, UpdateCallback) {
    ASSERT_NE(bridge, nullptr);

    // Call update callback directly - should fail without registration
    int result = imag_reason_fep_update_callback(bridge);
    EXPECT_EQ(result, -1) << "Update callback should fail without registration";
}

/**
 * Test: ForceUpdate
 * Verify force update triggers FEP computation
 */
TEST_F(ImaginationReasoningFepBridgeTest, ForceUpdate) {
    ASSERT_NE(bridge, nullptr);

    // Get initial metrics
    imag_reason_fep_metrics_t initial_metrics;
    int result = imag_reason_fep_bridge_get_metrics(bridge, &initial_metrics);
    EXPECT_EQ(result, 0);

    // Force update
    result = imag_reason_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Get updated metrics
    imag_reason_fep_metrics_t updated_metrics;
    result = imag_reason_fep_bridge_get_metrics(bridge, &updated_metrics);
    EXPECT_EQ(result, 0);

    // Update count should have increased
    EXPECT_GT(updated_metrics.update_count, initial_metrics.update_count)
        << "Update count should increase after force update";
}

/**
 * Test: ForceUpdateNull
 * Verify force update handles NULL gracefully
 */
TEST_F(ImaginationReasoningFepBridgeTest, ForceUpdateNull) {
    int result = imag_reason_fep_bridge_force_update(nullptr);
    EXPECT_EQ(result, -1) << "Force update with NULL should fail";
}

/* ============================================================================
 * Free Energy Computation Tests
 * ============================================================================ */

/**
 * Test: FreeEnergyContribution
 * Verify free energy is calculated from imagination-reasoning metrics
 */
TEST_F(ImaginationReasoningFepBridgeTest, FreeEnergyContribution) {
    ASSERT_NE(bridge, nullptr);

    // Get initial free energy
    float initial_fe = imag_reason_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(initial_fe, 0.0f) << "Free energy should be non-negative";
    EXPECT_LE(initial_fe, config.max_free_energy)
        << "Free energy should not exceed maximum";

    // Set low scenario quality (should increase free energy)
    imag_reason_fep_bridge_update_scenario_quality(bridge, 0.2f);
    imag_reason_fep_bridge_force_update(bridge);

    float updated_fe = imag_reason_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(updated_fe, initial_fe)
        << "Lower scenario quality should increase free energy";
}

/**
 * Test: ScenarioQualityReducesFreeEnergy
 * Verify that higher scenario quality leads to lower free energy
 */
TEST_F(ImaginationReasoningFepBridgeTest, ScenarioQualityReducesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Low quality
    imag_reason_fep_bridge_update_scenario_quality(bridge, 0.1f);
    imag_reason_fep_bridge_force_update(bridge);
    float low_quality_fe = imag_reason_fep_bridge_get_free_energy(bridge);

    // Reset and set high quality
    imag_reason_fep_bridge_reset(bridge);
    imag_reason_fep_bridge_update_scenario_quality(bridge, 0.9f);
    imag_reason_fep_bridge_force_update(bridge);
    float high_quality_fe = imag_reason_fep_bridge_get_free_energy(bridge);

    EXPECT_LT(high_quality_fe, low_quality_fe)
        << "High scenario quality should produce lower free energy than low";
}

/**
 * Test: ReasoningCoherenceReducesFreeEnergy
 * Verify that higher reasoning coherence reduces free energy
 */
TEST_F(ImaginationReasoningFepBridgeTest, ReasoningCoherenceReducesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Low coherence
    imag_reason_fep_bridge_update_reasoning_coherence(bridge, 0.1f);
    imag_reason_fep_bridge_force_update(bridge);
    float low_coherence_fe = imag_reason_fep_bridge_get_free_energy(bridge);

    // Reset and set high coherence
    imag_reason_fep_bridge_reset(bridge);
    imag_reason_fep_bridge_update_reasoning_coherence(bridge, 0.95f);
    imag_reason_fep_bridge_force_update(bridge);
    float high_coherence_fe = imag_reason_fep_bridge_get_free_energy(bridge);

    EXPECT_LT(high_coherence_fe, low_coherence_fe)
        << "High reasoning coherence should produce lower free energy";
}

/**
 * Test: CounterfactualValidityReducesFreeEnergy
 * Verify that higher counterfactual validity reduces free energy
 */
TEST_F(ImaginationReasoningFepBridgeTest, CounterfactualValidityReducesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Low validity
    imag_reason_fep_bridge_update_counterfactual_validity(bridge, 0.1f);
    imag_reason_fep_bridge_force_update(bridge);
    float low_validity_fe = imag_reason_fep_bridge_get_free_energy(bridge);

    // Reset and set high validity
    imag_reason_fep_bridge_reset(bridge);
    imag_reason_fep_bridge_update_counterfactual_validity(bridge, 0.9f);
    imag_reason_fep_bridge_force_update(bridge);
    float high_validity_fe = imag_reason_fep_bridge_get_free_energy(bridge);

    EXPECT_LT(high_validity_fe, low_validity_fe)
        << "High counterfactual validity should produce lower free energy";
}

/**
 * Test: FreeEnergyNull
 * Verify get_free_energy handles NULL gracefully
 */
TEST_F(ImaginationReasoningFepBridgeTest, FreeEnergyNull) {
    float fe = imag_reason_fep_bridge_get_free_energy(nullptr);
    EXPECT_LT(fe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: FreeEnergyBaseline
 * Verify baseline free energy is used when idle
 */
TEST_F(ImaginationReasoningFepBridgeTest, FreeEnergyBaseline) {
    ASSERT_NE(bridge, nullptr);

    // With neutral quality values, free energy should be near baseline
    float fe = imag_reason_fep_bridge_get_free_energy(bridge);
    // Allow some tolerance
    EXPECT_GE(fe, config.baseline_free_energy * 0.5f);
    EXPECT_LE(fe, config.max_free_energy);
}

/* ============================================================================
 * Coherence Detection Tests
 * ============================================================================ */

/**
 * Test: CoherenceDetection
 * Verify coherence state is detected correctly
 */
TEST_F(ImaginationReasoningFepBridgeTest, CoherenceDetection) {
    ASSERT_NE(bridge, nullptr);

    // With neutral initial values, may not be coherent
    bool initial_coherent = imag_reason_fep_bridge_is_coherent(bridge);

    // Set very high coherence (above 1 - epsilon)
    imag_reason_fep_bridge_update_reasoning_coherence(bridge, 0.99f);
    imag_reason_fep_bridge_force_update(bridge);

    EXPECT_TRUE(imag_reason_fep_bridge_is_coherent(bridge))
        << "Should be coherent when coherence > 1 - epsilon";

    // Set low coherence
    imag_reason_fep_bridge_update_reasoning_coherence(bridge, 0.5f);
    imag_reason_fep_bridge_force_update(bridge);

    EXPECT_FALSE(imag_reason_fep_bridge_is_coherent(bridge))
        << "Should not be coherent when coherence is low";
}

/* ============================================================================
 * Prediction Error Tests
 * ============================================================================ */

/**
 * Test: PredictionError
 * Verify prediction error tracking
 */
TEST_F(ImaginationReasoningFepBridgeTest, PredictionError) {
    ASSERT_NE(bridge, nullptr);

    // Get initial prediction error
    float initial_pe = imag_reason_fep_bridge_get_prediction_error(bridge);
    EXPECT_GE(initial_pe, 0.0f) << "Prediction error should be non-negative";
    EXPECT_LE(initial_pe, 1.0f) << "Prediction error should be <= 1.0";
}

/**
 * Test: PredictionErrorNull
 * Verify get_prediction_error handles NULL gracefully
 */
TEST_F(ImaginationReasoningFepBridgeTest, PredictionErrorNull) {
    float pe = imag_reason_fep_bridge_get_prediction_error(nullptr);
    EXPECT_LT(pe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: LowQualityIncreasesPredictionError
 * Verify low quality metrics contribute to prediction error
 */
TEST_F(ImaginationReasoningFepBridgeTest, LowQualityIncreasesPredictionError) {
    ASSERT_NE(bridge, nullptr);

    // Set low quality across all dimensions
    imag_reason_fep_bridge_update_scenario_quality(bridge, 0.1f);
    imag_reason_fep_bridge_update_reasoning_coherence(bridge, 0.1f);
    imag_reason_fep_bridge_update_counterfactual_validity(bridge, 0.1f);
    imag_reason_fep_bridge_force_update(bridge);

    float pe = imag_reason_fep_bridge_get_prediction_error(bridge);
    EXPECT_GT(pe, 0.3f) << "Low quality should produce significant prediction error";
}

/* ============================================================================
 * Metrics Tracking Tests
 * ============================================================================ */

/**
 * Test: MetricsTracking
 * Verify metrics are updated correctly during FEP cycles
 */
TEST_F(ImaginationReasoningFepBridgeTest, MetricsTracking) {
    ASSERT_NE(bridge, nullptr);

    imag_reason_fep_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    int result = imag_reason_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(result, 0) << "Get metrics should succeed";

    // Verify core FEP metrics are valid
    EXPECT_GE(metrics.free_energy, 0.0f);
    EXPECT_GE(metrics.prediction_error, 0.0f);
    EXPECT_LE(metrics.prediction_error, 1.0f);
    EXPECT_GE(metrics.entropy, 0.0f);

    // Verify imagination-reasoning specific metrics are valid
    EXPECT_GE(metrics.scenario_quality, 0.0f);
    EXPECT_LE(metrics.scenario_quality, 1.0f);
    EXPECT_GE(metrics.reasoning_coherence, 0.0f);
    EXPECT_LE(metrics.reasoning_coherence, 1.0f);
    EXPECT_GE(metrics.counterfactual_validity, 0.0f);
    EXPECT_LE(metrics.counterfactual_validity, 1.0f);
}

/**
 * Test: MetricsTrackingNull
 * Verify get_metrics handles NULL parameters gracefully
 */
TEST_F(ImaginationReasoningFepBridgeTest, MetricsTrackingNull) {
    imag_reason_fep_metrics_t metrics;

    int result = imag_reason_fep_bridge_get_metrics(nullptr, &metrics);
    EXPECT_EQ(result, -1) << "Get metrics with NULL bridge should fail";

    result = imag_reason_fep_bridge_get_metrics(bridge, nullptr);
    EXPECT_EQ(result, -1) << "Get metrics with NULL output should fail";
}

/**
 * Test: StatisticsTracking
 * Verify statistics are accumulated correctly
 */
TEST_F(ImaginationReasoningFepBridgeTest, StatisticsTracking) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        imag_reason_fep_bridge_force_update(bridge);
    }

    imag_reason_fep_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = imag_reason_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify stats are accumulated
    EXPECT_GE(stats.total_updates, 5u) << "Should have at least 5 updates";
    EXPECT_GE(stats.avg_free_energy, 0.0f);
}

/**
 * Test: StatsReset
 * Verify statistics can be reset
 */
TEST_F(ImaginationReasoningFepBridgeTest, StatsReset) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        imag_reason_fep_bridge_force_update(bridge);
    }

    // Reset stats
    int result = imag_reason_fep_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    imag_reason_fep_stats_t stats;
    result = imag_reason_fep_bridge_get_stats(bridge, &stats);
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
TEST_F(ImaginationReasoningFepBridgeTest, GetState) {
    ASSERT_NE(bridge, nullptr);

    imag_reason_fep_state_t state = imag_reason_fep_bridge_get_state(bridge);
    // State should be one of the valid states
    EXPECT_GE((int)state, (int)IMAG_REASON_FEP_STATE_UNINITIALIZED);
    EXPECT_LE((int)state, (int)IMAG_REASON_FEP_STATE_ERROR);
}

/**
 * Test: GetStateNull
 * Verify get_state handles NULL gracefully
 */
TEST_F(ImaginationReasoningFepBridgeTest, GetStateNull) {
    imag_reason_fep_state_t state = imag_reason_fep_bridge_get_state(nullptr);
    EXPECT_EQ(state, IMAG_REASON_FEP_STATE_ERROR)
        << "NULL bridge should return ERROR state";
}

/**
 * Test: IsDegraded
 * Verify degraded mode detection
 */
TEST_F(ImaginationReasoningFepBridgeTest, IsDegraded) {
    ASSERT_NE(bridge, nullptr);

    // Initially should not be degraded
    bool degraded = imag_reason_fep_bridge_is_degraded(bridge);
    EXPECT_FALSE(degraded) << "Should not be degraded initially";
}

/**
 * Test: StateName
 * Verify state name conversion
 */
TEST_F(ImaginationReasoningFepBridgeTest, StateName) {
    const char* name = imag_reason_fep_state_name(IMAG_REASON_FEP_STATE_IDLE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u) << "State name should not be empty";

    name = imag_reason_fep_state_name(IMAG_REASON_FEP_STATE_ACTIVE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = imag_reason_fep_state_name(IMAG_REASON_FEP_STATE_DEGRADED);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = imag_reason_fep_state_name(IMAG_REASON_FEP_STATE_ERROR);
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
TEST_F(ImaginationReasoningFepBridgeTest, HighFECallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = imag_reason_fep_bridge_set_high_fe_callback(
        bridge, test_high_fe_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = imag_reason_fep_bridge_set_high_fe_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: SurpriseCallback
 * Verify surprise event callback registration
 */
TEST_F(ImaginationReasoningFepBridgeTest, SurpriseCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = imag_reason_fep_bridge_set_surprise_callback(
        bridge, test_surprise_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = imag_reason_fep_bridge_set_surprise_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: MetricsCallback
 * Verify metrics update callback registration
 */
TEST_F(ImaginationReasoningFepBridgeTest, MetricsCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = imag_reason_fep_bridge_set_metrics_callback(
        bridge, test_metrics_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Force update to trigger callback
    imag_reason_fep_bridge_force_update(bridge);

    // Metrics callback should have been called
    EXPECT_GE(g_metrics_callback_count.load(), 1)
        << "Metrics callback should be called on update";

    // Clear callback
    result = imag_reason_fep_bridge_set_metrics_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: CallbackNull
 * Verify callback registration handles NULL bridge
 */
TEST_F(ImaginationReasoningFepBridgeTest, CallbackNull) {
    int result = imag_reason_fep_bridge_set_high_fe_callback(
        nullptr, test_high_fe_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = imag_reason_fep_bridge_set_surprise_callback(
        nullptr, test_surprise_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = imag_reason_fep_bridge_set_metrics_callback(
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
TEST_F(ImaginationReasoningFepBridgeTest, SetConfig) {
    ASSERT_NE(bridge, nullptr);

    imag_reason_fep_config_t new_config = imag_reason_fep_config_default();
    new_config.scenario_quality_weight = 0.4f;
    new_config.reasoning_coherence_weight = 0.35f;
    new_config.counterfactual_validity_weight = 0.25f;

    int result = imag_reason_fep_bridge_set_config(bridge, &new_config);
    EXPECT_EQ(result, 0) << "Set config should succeed";
}

/**
 * Test: SetConfigNull
 * Verify set_config handles NULL parameters
 */
TEST_F(ImaginationReasoningFepBridgeTest, SetConfigNull) {
    imag_reason_fep_config_t config_val = imag_reason_fep_config_default();

    int result = imag_reason_fep_bridge_set_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = imag_reason_fep_bridge_set_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL config should fail";
}

/**
 * Test: GetConfig
 * Verify configuration can be retrieved
 */
TEST_F(ImaginationReasoningFepBridgeTest, GetConfig) {
    ASSERT_NE(bridge, nullptr);

    imag_reason_fep_config_t retrieved_config;
    memset(&retrieved_config, 0, sizeof(retrieved_config));

    int result = imag_reason_fep_bridge_get_config(bridge, &retrieved_config);
    EXPECT_EQ(result, 0) << "Get config should succeed";

    // Verify retrieved config has valid values
    EXPECT_GT(retrieved_config.scenario_quality_weight, 0.0f);
    EXPECT_GT(retrieved_config.max_free_energy, 0.0f);
}

/**
 * Test: GetConfigNull
 * Verify get_config handles NULL parameters
 */
TEST_F(ImaginationReasoningFepBridgeTest, GetConfigNull) {
    imag_reason_fep_config_t config_val;

    int result = imag_reason_fep_bridge_get_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = imag_reason_fep_bridge_get_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL output should fail";
}

/* ============================================================================
 * Manual Update Operations Tests
 * ============================================================================ */

/**
 * Test: UpdateScenarioQuality
 * Verify manual scenario quality update
 */
TEST_F(ImaginationReasoningFepBridgeTest, UpdateScenarioQuality) {
    ASSERT_NE(bridge, nullptr);

    int result = imag_reason_fep_bridge_update_scenario_quality(bridge, 0.75f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    float sq = imag_reason_fep_bridge_get_scenario_quality(bridge);
    EXPECT_FLOAT_EQ(sq, 0.75f) << "Value should be updated";
}

/**
 * Test: UpdateReasoningCoherence
 * Verify manual reasoning coherence update
 */
TEST_F(ImaginationReasoningFepBridgeTest, UpdateReasoningCoherence) {
    ASSERT_NE(bridge, nullptr);

    int result = imag_reason_fep_bridge_update_reasoning_coherence(bridge, 0.85f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    float rc = imag_reason_fep_bridge_get_reasoning_coherence(bridge);
    EXPECT_FLOAT_EQ(rc, 0.85f) << "Value should be updated";
}

/**
 * Test: UpdateCounterfactualValidity
 * Verify manual counterfactual validity update
 */
TEST_F(ImaginationReasoningFepBridgeTest, UpdateCounterfactualValidity) {
    ASSERT_NE(bridge, nullptr);

    int result = imag_reason_fep_bridge_update_counterfactual_validity(bridge, 0.65f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    imag_reason_fep_metrics_t metrics;
    imag_reason_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_FLOAT_EQ(metrics.counterfactual_validity, 0.65f) << "Value should be updated";
}

/**
 * Test: UpdateCreativeNovelty
 * Verify manual creative novelty update
 */
TEST_F(ImaginationReasoningFepBridgeTest, UpdateCreativeNovelty) {
    ASSERT_NE(bridge, nullptr);

    int result = imag_reason_fep_bridge_update_creative_novelty(bridge, 0.8f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    imag_reason_fep_metrics_t metrics;
    imag_reason_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_FLOAT_EQ(metrics.creative_novelty, 0.8f) << "Value should be updated";
}

/**
 * Test: UpdateClamps
 * Verify updates are clamped to valid range
 */
TEST_F(ImaginationReasoningFepBridgeTest, UpdateClamps) {
    ASSERT_NE(bridge, nullptr);

    // Test clamping above 1.0
    imag_reason_fep_bridge_update_scenario_quality(bridge, 2.0f);
    float sq = imag_reason_fep_bridge_get_scenario_quality(bridge);
    EXPECT_LE(sq, 1.0f) << "Should be clamped to max 1.0";

    // Test clamping below 0.0
    imag_reason_fep_bridge_update_scenario_quality(bridge, -0.5f);
    sq = imag_reason_fep_bridge_get_scenario_quality(bridge);
    EXPECT_GE(sq, 0.0f) << "Should be clamped to min 0.0";
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Comprehensive test for NULL parameter handling across all functions
 */
TEST_F(ImaginationReasoningFepBridgeTest, NullHandling) {
    // Lifecycle
    imag_reason_fep_bridge_destroy(nullptr);

    // Registration
    EXPECT_EQ(imag_reason_fep_bridge_reset(nullptr), -1);
    EXPECT_FALSE(imag_reason_fep_bridge_is_registered(nullptr));
    EXPECT_EQ(imag_reason_fep_bridge_get_id(nullptr), 0u);
    EXPECT_EQ(imag_reason_fep_bridge_unregister(nullptr), -1);

    // Updates
    EXPECT_EQ(imag_reason_fep_update_callback(nullptr), -1);
    EXPECT_EQ(imag_reason_fep_bridge_force_update(nullptr), -1);
    EXPECT_EQ(imag_reason_fep_bridge_update_scenario_quality(nullptr, 0.5f), -1);
    EXPECT_EQ(imag_reason_fep_bridge_update_reasoning_coherence(nullptr, 0.5f), -1);
    EXPECT_EQ(imag_reason_fep_bridge_update_counterfactual_validity(nullptr, 0.5f), -1);
    EXPECT_EQ(imag_reason_fep_bridge_update_creative_novelty(nullptr, 0.5f), -1);

    // Metrics
    imag_reason_fep_metrics_t metrics;
    EXPECT_EQ(imag_reason_fep_bridge_get_metrics(nullptr, &metrics), -1);
    EXPECT_EQ(imag_reason_fep_bridge_get_metrics(bridge, nullptr), -1);

    imag_reason_fep_stats_t stats;
    EXPECT_EQ(imag_reason_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(imag_reason_fep_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(imag_reason_fep_bridge_reset_stats(nullptr), -1);

    // State queries
    EXPECT_EQ(imag_reason_fep_bridge_get_state(nullptr), IMAG_REASON_FEP_STATE_ERROR);
    EXPECT_LT(imag_reason_fep_bridge_get_free_energy(nullptr), 0.0f);
    EXPECT_LT(imag_reason_fep_bridge_get_prediction_error(nullptr), 0.0f);
    EXPECT_LT(imag_reason_fep_bridge_get_scenario_quality(nullptr), 0.0f);
    EXPECT_LT(imag_reason_fep_bridge_get_reasoning_coherence(nullptr), 0.0f);
    EXPECT_FALSE(imag_reason_fep_bridge_is_degraded(nullptr));
    EXPECT_FALSE(imag_reason_fep_bridge_is_coherent(nullptr));

    // Callbacks
    EXPECT_EQ(imag_reason_fep_bridge_set_high_fe_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(imag_reason_fep_bridge_set_surprise_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(imag_reason_fep_bridge_set_metrics_callback(nullptr, nullptr, nullptr), -1);

    // Config
    imag_reason_fep_config_t cfg;
    EXPECT_EQ(imag_reason_fep_bridge_set_config(nullptr, &cfg), -1);
    EXPECT_EQ(imag_reason_fep_bridge_set_config(bridge, nullptr), -1);
    EXPECT_EQ(imag_reason_fep_bridge_get_config(nullptr, &cfg), -1);
    EXPECT_EQ(imag_reason_fep_bridge_get_config(bridge, nullptr), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/* ============================================================================
 * Error Conditions Tests
 * ============================================================================ */

/**
 * Test: ErrorConditions
 * Verify proper error handling for various edge cases
 */
TEST_F(ImaginationReasoningFepBridgeTest, ErrorConditions) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should succeed (no-op)
    int result = imag_reason_fep_bridge_unregister(bridge);
    EXPECT_EQ(result, 0) << "Unregister when not registered should be no-op";

    // Double destroy should be safe (after first destroy, pointer is invalid
    // but we test with different bridges)
    imag_reason_fep_bridge_t* temp_bridge = imag_reason_fep_bridge_create(nullptr);
    ASSERT_NE(temp_bridge, nullptr);
    imag_reason_fep_bridge_destroy(temp_bridge);
    // temp_bridge is now invalid, but destroy(nullptr) should be safe
    imag_reason_fep_bridge_destroy(nullptr);

    SUCCEED() << "Error condition tests passed";
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Verify basic thread safety with concurrent access
 */
TEST_F(ImaginationReasoningFepBridgeTest, ThreadSafety) {
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
                imag_reason_fep_bridge_get_free_energy(bridge);
                imag_reason_fep_bridge_get_prediction_error(bridge);
                imag_reason_fep_bridge_get_scenario_quality(bridge);
                imag_reason_fep_bridge_get_reasoning_coherence(bridge);
                imag_reason_fep_bridge_get_state(bridge);
                imag_reason_fep_bridge_is_degraded(bridge);
                imag_reason_fep_bridge_is_coherent(bridge);

                imag_reason_fep_metrics_t metrics;
                imag_reason_fep_bridge_get_metrics(bridge, &metrics);

                imag_reason_fep_stats_t stats;
                imag_reason_fep_bridge_get_stats(bridge, &stats);

                // Write operations
                imag_reason_fep_bridge_update_scenario_quality(bridge,
                    (float)(i % 10) / 10.0f);
                imag_reason_fep_bridge_force_update(bridge);
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
TEST_F(ImaginationReasoningFepBridgeTest, DestroyCallback) {
    ASSERT_NE(bridge, nullptr);

    // Destroy callback should be safe to call
    imag_reason_fep_destroy_callback(bridge);
    SUCCEED() << "Destroy callback should not crash";

    // Should also handle NULL
    imag_reason_fep_destroy_callback(nullptr);
    SUCCEED() << "Destroy callback with NULL should not crash";
}

/* ============================================================================
 * Integration Scenario Tests
 * ============================================================================ */

/**
 * Test: ImaginationReasoningScenario
 * Simulate an imagination-reasoning integration scenario
 */
TEST_F(ImaginationReasoningFepBridgeTest, ImaginationReasoningScenario) {
    ASSERT_NE(bridge, nullptr);

    // Register metrics callback to track changes
    imag_reason_fep_bridge_set_metrics_callback(bridge, test_metrics_callback, nullptr);

    // Scenario: Start with low quality, improve through integration

    // Phase 1: Initial poor integration
    imag_reason_fep_bridge_update_scenario_quality(bridge, 0.2f);
    imag_reason_fep_bridge_update_reasoning_coherence(bridge, 0.3f);
    imag_reason_fep_bridge_update_counterfactual_validity(bridge, 0.25f);
    imag_reason_fep_bridge_force_update(bridge);

    float initial_fe = imag_reason_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(initial_fe, config.baseline_free_energy)
        << "Poor integration should produce elevated free energy";

    // Phase 2: Improving scenario generation
    imag_reason_fep_bridge_update_scenario_quality(bridge, 0.6f);
    imag_reason_fep_bridge_force_update(bridge);

    float improved_scenario_fe = imag_reason_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(improved_scenario_fe, initial_fe)
        << "Better scenarios should reduce free energy";

    // Phase 3: Reasoning becomes coherent
    imag_reason_fep_bridge_update_reasoning_coherence(bridge, 0.85f);
    imag_reason_fep_bridge_force_update(bridge);

    float coherent_fe = imag_reason_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(coherent_fe, improved_scenario_fe)
        << "Coherent reasoning should reduce free energy";

    // Phase 4: Valid counterfactuals
    imag_reason_fep_bridge_update_counterfactual_validity(bridge, 0.9f);
    imag_reason_fep_bridge_force_update(bridge);

    float final_fe = imag_reason_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(final_fe, coherent_fe)
        << "Valid counterfactuals should have lowest free energy";

    // Phase 5: Creative insight
    imag_reason_fep_bridge_update_creative_novelty(bridge, 0.85f);
    imag_reason_fep_bridge_force_update(bridge);

    // Verify callback was called for each update
    EXPECT_GE(g_metrics_callback_count.load(), 5)
        << "Metrics callback should be called for each update";
}

/**
 * Test: CreativeInsightTracking
 * Verify creative insight events are tracked
 */
TEST_F(ImaginationReasoningFepBridgeTest, CreativeInsightTracking) {
    ASSERT_NE(bridge, nullptr);

    // Trigger creative insights by setting high novelty
    for (int i = 0; i < 5; i++) {
        imag_reason_fep_bridge_update_creative_novelty(bridge, 0.85f);
        imag_reason_fep_bridge_force_update(bridge);
    }

    imag_reason_fep_stats_t stats;
    imag_reason_fep_bridge_get_stats(bridge, &stats);

    EXPECT_GE(stats.creative_insights, 5u)
        << "Should track creative insight events";
}

/**
 * Test: DegradedModeTransition
 * Verify transition to degraded mode with high free energy
 */
TEST_F(ImaginationReasoningFepBridgeTest, DegradedModeTransition) {
    ASSERT_NE(bridge, nullptr);

    // Register high FE callback
    imag_reason_fep_bridge_set_high_fe_callback(bridge, test_high_fe_callback, nullptr);

    // Set very poor metrics to trigger high free energy
    imag_reason_fep_bridge_update_scenario_quality(bridge, 0.05f);
    imag_reason_fep_bridge_update_reasoning_coherence(bridge, 0.05f);
    imag_reason_fep_bridge_update_counterfactual_validity(bridge, 0.05f);
    imag_reason_fep_bridge_force_update(bridge);

    float fe = imag_reason_fep_bridge_get_free_energy(bridge);

    // If FE exceeds threshold, should be in degraded mode
    if (fe > config.high_free_energy_threshold) {
        EXPECT_TRUE(imag_reason_fep_bridge_is_degraded(bridge))
            << "Should be in degraded mode with high free energy";
        EXPECT_GE(g_high_fe_callback_count.load(), 1)
            << "High FE callback should have been triggered";
    }
}
