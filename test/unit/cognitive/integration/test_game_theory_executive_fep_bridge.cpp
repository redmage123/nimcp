/**
 * @file test_game_theory_executive_fep_bridge.cpp
 * @brief Unit tests for Game Theory-Executive FEP Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for FEP-Game Theory Executive bidirectional integration
 * WHY:  Ensure free energy computation from strategic decision-making works correctly
 * HOW:  Test lifecycle, registration, metrics, callbacks, and FEP update cycle
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Configuration Validation
 * - FEP Registration/Unregistration
 * - Update Callback Mechanism
 * - Free Energy Computation
 * - Decision Quality Effects
 * - Executive Alignment Detection
 * - Action Coherence Tracking
 * - Prediction Error Tracking
 * - Metrics and Statistics
 * - Callback Registration
 * - Recommendation Result Tracking
 * - Null Parameter Handling
 * - Thread Safety
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/integration/nimcp_game_theory_executive_fep_bridge.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_high_fe_callback_count{0};
static std::atomic<int> g_surprise_callback_count{0};
static std::atomic<int> g_metrics_callback_count{0};
static float g_last_free_energy = 0.0f;
static float g_last_surprise = 0.0f;
static std::string g_last_surprise_source;
static gt_exec_fep_metrics_t g_last_metrics;

/**
 * Test callback for high free energy events
 */
static void test_high_fe_callback(
    gt_exec_fep_bridge_t* bridge,
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
    gt_exec_fep_bridge_t* bridge,
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
    gt_exec_fep_bridge_t* bridge,
    const gt_exec_fep_metrics_t* metrics,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    g_metrics_callback_count++;
    if (metrics != nullptr) {
        memcpy(&g_last_metrics, metrics, sizeof(gt_exec_fep_metrics_t));
    }
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class GameTheoryExecutiveFepBridgeTest : public ::testing::Test {
protected:
    gt_exec_fep_bridge_t* bridge = nullptr;
    gt_exec_fep_config_t config;

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
        config = gt_exec_fep_config_default();
        bridge = gt_exec_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            gt_exec_fep_bridge_destroy(bridge);
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
TEST_F(GameTheoryExecutiveFepBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify initial state
    gt_exec_fep_state_t state = gt_exec_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, GT_EXEC_FEP_STATE_IDLE) << "Initial state should be IDLE";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, BridgeCreationNullConfig) {
    gt_exec_fep_bridge_t* br = gt_exec_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    gt_exec_fep_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    gt_exec_fep_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    gt_exec_fep_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, DefaultConfig) {
    gt_exec_fep_config_t default_config = gt_exec_fep_config_default();

    // Verify weights are positive
    EXPECT_GT(default_config.free_energy_weight, 0.0f);
    EXPECT_GT(default_config.decision_quality_weight, 0.0f);
    EXPECT_GT(default_config.executive_alignment_weight, 0.0f);
    EXPECT_GT(default_config.action_coherence_weight, 0.0f);

    // Verify thresholds are sensible
    EXPECT_GT(default_config.high_free_energy_threshold, 0.0f);
    EXPECT_GT(default_config.prediction_error_threshold, 0.0f);
    EXPECT_GT(default_config.alignment_epsilon, 0.0f);

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
TEST_F(GameTheoryExecutiveFepBridgeTest, ConfigValidation) {
    gt_exec_fep_config_t custom_config = gt_exec_fep_config_default();
    custom_config.decision_quality_weight = 0.5f;
    custom_config.executive_alignment_weight = 0.3f;
    custom_config.action_coherence_weight = 0.2f;
    custom_config.high_free_energy_threshold = 1.8f;

    gt_exec_fep_bridge_t* custom_bridge = gt_exec_fep_bridge_create(&custom_config);
    ASSERT_NE(custom_bridge, nullptr);

    gt_exec_fep_config_t retrieved_config;
    int result = gt_exec_fep_bridge_get_config(custom_bridge, &retrieved_config);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(retrieved_config.decision_quality_weight, 0.5f);
    EXPECT_FLOAT_EQ(retrieved_config.executive_alignment_weight, 0.3f);
    EXPECT_FLOAT_EQ(retrieved_config.action_coherence_weight, 0.2f);
    EXPECT_FLOAT_EQ(retrieved_config.high_free_energy_threshold, 1.8f);

    gt_exec_fep_bridge_destroy(custom_bridge);
}

/**
 * Test: BridgeReset
 * Verify bridge can be reset to initial state
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, BridgeReset) {
    ASSERT_NE(bridge, nullptr);

    // Force an update to change state
    gt_exec_fep_bridge_update_decision_quality(bridge, 0.3f);
    int result = gt_exec_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Reset bridge
    result = gt_exec_fep_bridge_reset(bridge);
    EXPECT_EQ(result, 0) << "Reset should succeed";

    // Verify state is back to IDLE
    gt_exec_fep_state_t state = gt_exec_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, GT_EXEC_FEP_STATE_IDLE) << "State should be IDLE after reset";

    // Verify metrics are reset
    float dq = gt_exec_fep_bridge_get_decision_quality(bridge);
    EXPECT_FLOAT_EQ(dq, 1.0f) << "Decision quality should be reset to 1.0";
}

/* ============================================================================
 * FEP Registration Tests
 * ============================================================================ */

/**
 * Test: RegisterWithFEP
 * Verify bridge registration state without actual orchestrator
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, RegisterWithFEP) {
    ASSERT_NE(bridge, nullptr);

    // Before registration, should not be registered
    EXPECT_FALSE(gt_exec_fep_bridge_is_registered(bridge))
        << "Should not be registered initially";

    // Without actual orchestrator, ID should be 0
    uint32_t id = gt_exec_fep_bridge_get_id(bridge);
    EXPECT_EQ(id, 0u) << "ID should be 0 when not registered";
}

/**
 * Test: UnregisterFromFEP
 * Verify bridge can unregister cleanly
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, UnregisterFromFEP) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should handle gracefully
    int result = gt_exec_fep_bridge_unregister(bridge);
    EXPECT_EQ(result, 0) << "Unregister when not registered should succeed";

    EXPECT_FALSE(gt_exec_fep_bridge_is_registered(bridge))
        << "Should not be registered after unregister";
}

/**
 * Test: RegistrationNullParams
 * Verify registration handles NULL parameters gracefully
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, RegistrationNullParams) {
    ASSERT_NE(bridge, nullptr);

    uint32_t bridge_id = 0;

    // NULL bridge
    int result = gt_exec_fep_bridge_register(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1) << "Register with NULL bridge should fail";

    // NULL orchestrator
    result = gt_exec_fep_bridge_register(bridge, nullptr, nullptr, &bridge_id);
    EXPECT_EQ(result, -1) << "Register without orchestrator should fail";
}

/* ============================================================================
 * FEP Update Callback Tests
 * ============================================================================ */

/**
 * Test: UpdateCallback
 * Verify FEP update callback behavior
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, UpdateCallback) {
    ASSERT_NE(bridge, nullptr);

    // Call update callback directly - should fail without registration
    int result = gt_exec_fep_update_callback(bridge);
    EXPECT_EQ(result, -1) << "Update callback should fail without registration";
}

/**
 * Test: ForceUpdate
 * Verify force update triggers FEP computation
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, ForceUpdate) {
    ASSERT_NE(bridge, nullptr);

    // Get initial metrics
    gt_exec_fep_metrics_t initial_metrics;
    int result = gt_exec_fep_bridge_get_metrics(bridge, &initial_metrics);
    EXPECT_EQ(result, 0);

    // Force update
    result = gt_exec_fep_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // Get updated metrics
    gt_exec_fep_metrics_t updated_metrics;
    result = gt_exec_fep_bridge_get_metrics(bridge, &updated_metrics);
    EXPECT_EQ(result, 0);

    // Update count should have increased
    EXPECT_GT(updated_metrics.update_count, initial_metrics.update_count)
        << "Update count should increase after force update";
}

/**
 * Test: ForceUpdateNull
 * Verify force update handles NULL gracefully
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, ForceUpdateNull) {
    int result = gt_exec_fep_bridge_force_update(nullptr);
    EXPECT_EQ(result, -1) << "Force update with NULL should fail";
}

/* ============================================================================
 * Free Energy Computation Tests
 * ============================================================================ */

/**
 * Test: FreeEnergyContribution
 * Verify free energy is calculated from decision metrics
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, FreeEnergyContribution) {
    ASSERT_NE(bridge, nullptr);

    // Get initial free energy
    float initial_fe = gt_exec_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(initial_fe, 0.0f) << "Free energy should be non-negative";
    EXPECT_LE(initial_fe, config.max_free_energy)
        << "Free energy should not exceed maximum";

    // Set low decision quality
    gt_exec_fep_bridge_update_decision_quality(bridge, 0.2f);
    gt_exec_fep_bridge_force_update(bridge);

    float updated_fe = gt_exec_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(updated_fe, initial_fe)
        << "Lower decision quality should increase free energy";
}

/**
 * Test: DecisionQualityAffectsFreeEnergy
 * Verify that lower decision quality leads to higher free energy
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, DecisionQualityAffectsFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // High quality decisions
    gt_exec_fep_bridge_update_decision_quality(bridge, 0.9f);
    gt_exec_fep_bridge_force_update(bridge);
    float high_quality_fe = gt_exec_fep_bridge_get_free_energy(bridge);

    // Reset and set low quality decisions
    gt_exec_fep_bridge_reset(bridge);
    gt_exec_fep_bridge_update_decision_quality(bridge, 0.1f);
    gt_exec_fep_bridge_force_update(bridge);
    float low_quality_fe = gt_exec_fep_bridge_get_free_energy(bridge);

    EXPECT_GT(low_quality_fe, high_quality_fe)
        << "Low decision quality should produce higher free energy";
}

/**
 * Test: ExecutiveAlignmentReducesFreeEnergy
 * Verify that high alignment reduces free energy
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, ExecutiveAlignmentReducesFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Low alignment (executive overrides strategy)
    gt_exec_fep_bridge_update_executive_alignment(bridge, 0.1f);
    gt_exec_fep_bridge_force_update(bridge);
    float low_align_fe = gt_exec_fep_bridge_get_free_energy(bridge);

    // Reset and set high alignment
    gt_exec_fep_bridge_reset(bridge);
    gt_exec_fep_bridge_update_executive_alignment(bridge, 0.9f);
    gt_exec_fep_bridge_force_update(bridge);
    float high_align_fe = gt_exec_fep_bridge_get_free_energy(bridge);

    EXPECT_LT(high_align_fe, low_align_fe)
        << "High executive alignment should produce lower free energy";
}

/**
 * Test: ActionCoherenceAffectsFreeEnergy
 * Verify that action coherence affects free energy
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, ActionCoherenceAffectsFreeEnergy) {
    ASSERT_NE(bridge, nullptr);

    // Low coherence
    gt_exec_fep_bridge_update_action_coherence(bridge, 0.2f);
    gt_exec_fep_bridge_force_update(bridge);
    float low_coherence_fe = gt_exec_fep_bridge_get_free_energy(bridge);

    // Reset and set high coherence
    gt_exec_fep_bridge_reset(bridge);
    gt_exec_fep_bridge_update_action_coherence(bridge, 0.9f);
    gt_exec_fep_bridge_force_update(bridge);
    float high_coherence_fe = gt_exec_fep_bridge_get_free_energy(bridge);

    EXPECT_LT(high_coherence_fe, low_coherence_fe)
        << "High action coherence should produce lower free energy";
}

/**
 * Test: FreeEnergyNull
 * Verify get_free_energy handles NULL gracefully
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, FreeEnergyNull) {
    float fe = gt_exec_fep_bridge_get_free_energy(nullptr);
    EXPECT_LT(fe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: FreeEnergyBaseline
 * Verify baseline free energy is used when idle
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, FreeEnergyBaseline) {
    ASSERT_NE(bridge, nullptr);

    // In idle state with optimal metrics, free energy should be near baseline
    float fe = gt_exec_fep_bridge_get_free_energy(bridge);
    // Allow some tolerance
    EXPECT_GE(fe, config.baseline_free_energy * 0.5f);
    EXPECT_LE(fe, config.baseline_free_energy * 3.0f);
}

/* ============================================================================
 * Executive Alignment Tests
 * ============================================================================ */

/**
 * Test: ExecutiveAlignmentDetection
 * Verify executive alignment state is detected correctly
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, ExecutiveAlignmentDetection) {
    ASSERT_NE(bridge, nullptr);

    // Initially should be aligned (default value is 1.0)
    EXPECT_TRUE(gt_exec_fep_bridge_is_exec_aligned(bridge))
        << "Should be aligned initially";

    // Set low alignment
    gt_exec_fep_bridge_update_executive_alignment(bridge, 0.5f);
    gt_exec_fep_bridge_force_update(bridge);

    EXPECT_FALSE(gt_exec_fep_bridge_is_exec_aligned(bridge))
        << "Should not be aligned when alignment < threshold";

    // Set very high alignment
    gt_exec_fep_bridge_update_executive_alignment(bridge, 0.999f);
    gt_exec_fep_bridge_force_update(bridge);

    EXPECT_TRUE(gt_exec_fep_bridge_is_exec_aligned(bridge))
        << "Should be aligned when alignment > threshold";
}

/**
 * Test: GetExecutiveAlignment
 * Verify getting executive alignment value
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, GetExecutiveAlignment) {
    ASSERT_NE(bridge, nullptr);

    gt_exec_fep_bridge_update_executive_alignment(bridge, 0.75f);
    float alignment = gt_exec_fep_bridge_get_executive_alignment(bridge);
    EXPECT_FLOAT_EQ(alignment, 0.75f);
}

/* ============================================================================
 * Prediction Error Tests
 * ============================================================================ */

/**
 * Test: PredictionError
 * Verify prediction error tracking
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, PredictionError) {
    ASSERT_NE(bridge, nullptr);

    // Get initial prediction error
    float initial_pe = gt_exec_fep_bridge_get_prediction_error(bridge);
    EXPECT_GE(initial_pe, 0.0f) << "Prediction error should be non-negative";
    EXPECT_LE(initial_pe, 1.0f) << "Prediction error should be <= 1.0";
}

/**
 * Test: PredictionErrorNull
 * Verify get_prediction_error handles NULL gracefully
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, PredictionErrorNull) {
    float pe = gt_exec_fep_bridge_get_prediction_error(nullptr);
    EXPECT_LT(pe, 0.0f) << "NULL bridge should return negative value";
}

/**
 * Test: LowQualityAffectsPredictionError
 * Verify low quality decisions contribute to prediction error
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, LowQualityAffectsPredictionError) {
    ASSERT_NE(bridge, nullptr);

    // Set low quality
    gt_exec_fep_bridge_update_decision_quality(bridge, 0.1f);
    gt_exec_fep_bridge_force_update(bridge);

    float pe = gt_exec_fep_bridge_get_prediction_error(bridge);
    EXPECT_GT(pe, 0.0f) << "Low decision quality should produce prediction error";
}

/* ============================================================================
 * Metrics Tracking Tests
 * ============================================================================ */

/**
 * Test: MetricsTracking
 * Verify metrics are updated correctly during FEP cycles
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, MetricsTracking) {
    ASSERT_NE(bridge, nullptr);

    gt_exec_fep_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    int result = gt_exec_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(result, 0) << "Get metrics should succeed";

    // Verify core FEP metrics are valid
    EXPECT_GE(metrics.free_energy, 0.0f);
    EXPECT_GE(metrics.prediction_error, 0.0f);
    EXPECT_LE(metrics.prediction_error, 1.0f);
    EXPECT_GE(metrics.entropy, 0.0f);

    // Verify game theory-executive specific metrics are valid
    EXPECT_GE(metrics.decision_quality, 0.0f);
    EXPECT_LE(metrics.decision_quality, 1.0f);
    EXPECT_GE(metrics.executive_alignment, 0.0f);
    EXPECT_LE(metrics.executive_alignment, 1.0f);
    EXPECT_GE(metrics.action_coherence, 0.0f);
    EXPECT_LE(metrics.action_coherence, 1.0f);
}

/**
 * Test: MetricsTrackingNull
 * Verify get_metrics handles NULL parameters gracefully
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, MetricsTrackingNull) {
    gt_exec_fep_metrics_t metrics;

    int result = gt_exec_fep_bridge_get_metrics(nullptr, &metrics);
    EXPECT_EQ(result, -1) << "Get metrics with NULL bridge should fail";

    result = gt_exec_fep_bridge_get_metrics(bridge, nullptr);
    EXPECT_EQ(result, -1) << "Get metrics with NULL output should fail";
}

/**
 * Test: StatisticsTracking
 * Verify statistics are accumulated correctly
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, StatisticsTracking) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        gt_exec_fep_bridge_force_update(bridge);
    }

    gt_exec_fep_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = gt_exec_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify stats are accumulated
    EXPECT_GE(stats.total_updates, 5u) << "Should have at least 5 updates";
    EXPECT_GE(stats.avg_free_energy, 0.0f);
}

/**
 * Test: StatsReset
 * Verify statistics can be reset
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, StatsReset) {
    ASSERT_NE(bridge, nullptr);

    // Force several updates
    for (int i = 0; i < 5; i++) {
        gt_exec_fep_bridge_force_update(bridge);
    }

    // Reset stats
    int result = gt_exec_fep_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    gt_exec_fep_stats_t stats;
    result = gt_exec_fep_bridge_get_stats(bridge, &stats);
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
TEST_F(GameTheoryExecutiveFepBridgeTest, GetState) {
    ASSERT_NE(bridge, nullptr);

    gt_exec_fep_state_t state = gt_exec_fep_bridge_get_state(bridge);
    // State should be one of the valid states
    EXPECT_GE((int)state, (int)GT_EXEC_FEP_STATE_UNINITIALIZED);
    EXPECT_LE((int)state, (int)GT_EXEC_FEP_STATE_ERROR);
}

/**
 * Test: GetStateNull
 * Verify get_state handles NULL gracefully
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, GetStateNull) {
    gt_exec_fep_state_t state = gt_exec_fep_bridge_get_state(nullptr);
    EXPECT_EQ(state, GT_EXEC_FEP_STATE_ERROR)
        << "NULL bridge should return ERROR state";
}

/**
 * Test: IsDegraded
 * Verify degraded mode detection
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, IsDegraded) {
    ASSERT_NE(bridge, nullptr);

    // Initially should not be degraded
    bool degraded = gt_exec_fep_bridge_is_degraded(bridge);
    EXPECT_FALSE(degraded) << "Should not be degraded initially";
}

/**
 * Test: StateName
 * Verify state name conversion
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, StateName) {
    const char* name = gt_exec_fep_state_name(GT_EXEC_FEP_STATE_IDLE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u) << "State name should not be empty";

    name = gt_exec_fep_state_name(GT_EXEC_FEP_STATE_ACTIVE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = gt_exec_fep_state_name(GT_EXEC_FEP_STATE_DEGRADED);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = gt_exec_fep_state_name(GT_EXEC_FEP_STATE_ERROR);
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
TEST_F(GameTheoryExecutiveFepBridgeTest, HighFECallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = gt_exec_fep_bridge_set_high_fe_callback(
        bridge, test_high_fe_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = gt_exec_fep_bridge_set_high_fe_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: SurpriseCallback
 * Verify surprise event callback registration
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, SurpriseCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = gt_exec_fep_bridge_set_surprise_callback(
        bridge, test_surprise_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = gt_exec_fep_bridge_set_surprise_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: MetricsCallback
 * Verify metrics update callback registration
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, MetricsCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;
    int result = gt_exec_fep_bridge_set_metrics_callback(
        bridge, test_metrics_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Force update to trigger callback
    gt_exec_fep_bridge_force_update(bridge);

    // Metrics callback should have been called
    EXPECT_GE(g_metrics_callback_count.load(), 1)
        << "Metrics callback should be called on update";

    // Clear callback
    result = gt_exec_fep_bridge_set_metrics_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: CallbackNull
 * Verify callback registration handles NULL bridge
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, CallbackNull) {
    int result = gt_exec_fep_bridge_set_high_fe_callback(
        nullptr, test_high_fe_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = gt_exec_fep_bridge_set_surprise_callback(
        nullptr, test_surprise_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = gt_exec_fep_bridge_set_metrics_callback(
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
TEST_F(GameTheoryExecutiveFepBridgeTest, SetConfig) {
    ASSERT_NE(bridge, nullptr);

    gt_exec_fep_config_t new_config = gt_exec_fep_config_default();
    new_config.decision_quality_weight = 0.5f;
    new_config.executive_alignment_weight = 0.3f;
    new_config.action_coherence_weight = 0.2f;

    int result = gt_exec_fep_bridge_set_config(bridge, &new_config);
    EXPECT_EQ(result, 0) << "Set config should succeed";
}

/**
 * Test: SetConfigNull
 * Verify set_config handles NULL parameters
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, SetConfigNull) {
    gt_exec_fep_config_t config_val = gt_exec_fep_config_default();

    int result = gt_exec_fep_bridge_set_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = gt_exec_fep_bridge_set_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL config should fail";
}

/**
 * Test: GetConfig
 * Verify configuration can be retrieved
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, GetConfig) {
    ASSERT_NE(bridge, nullptr);

    gt_exec_fep_config_t retrieved_config;
    memset(&retrieved_config, 0, sizeof(retrieved_config));

    int result = gt_exec_fep_bridge_get_config(bridge, &retrieved_config);
    EXPECT_EQ(result, 0) << "Get config should succeed";

    // Verify retrieved config has valid values
    EXPECT_GT(retrieved_config.decision_quality_weight, 0.0f);
    EXPECT_GT(retrieved_config.max_free_energy, 0.0f);
}

/**
 * Test: GetConfigNull
 * Verify get_config handles NULL parameters
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, GetConfigNull) {
    gt_exec_fep_config_t config_val;

    int result = gt_exec_fep_bridge_get_config(nullptr, &config_val);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = gt_exec_fep_bridge_get_config(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL output should fail";
}

/* ============================================================================
 * Manual Update Operations Tests
 * ============================================================================ */

/**
 * Test: UpdateDecisionQuality
 * Verify manual decision quality update
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, UpdateDecisionQuality) {
    ASSERT_NE(bridge, nullptr);

    int result = gt_exec_fep_bridge_update_decision_quality(bridge, 0.75f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    float dq = gt_exec_fep_bridge_get_decision_quality(bridge);
    EXPECT_FLOAT_EQ(dq, 0.75f) << "Value should be updated";
}

/**
 * Test: UpdateExecutiveAlignment
 * Verify manual executive alignment update
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, UpdateExecutiveAlignment) {
    ASSERT_NE(bridge, nullptr);

    int result = gt_exec_fep_bridge_update_executive_alignment(bridge, 0.65f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    float ea = gt_exec_fep_bridge_get_executive_alignment(bridge);
    EXPECT_FLOAT_EQ(ea, 0.65f) << "Value should be updated";
}

/**
 * Test: UpdateActionCoherence
 * Verify manual action coherence update
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, UpdateActionCoherence) {
    ASSERT_NE(bridge, nullptr);

    int result = gt_exec_fep_bridge_update_action_coherence(bridge, 0.85f);
    EXPECT_EQ(result, 0) << "Update should succeed";

    gt_exec_fep_metrics_t metrics;
    gt_exec_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_FLOAT_EQ(metrics.action_coherence, 0.85f) << "Value should be updated";
}

/**
 * Test: UpdateClamps
 * Verify updates are clamped to valid range
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, UpdateClamps) {
    ASSERT_NE(bridge, nullptr);

    // Test clamping above 1.0
    gt_exec_fep_bridge_update_decision_quality(bridge, 2.0f);
    float dq = gt_exec_fep_bridge_get_decision_quality(bridge);
    EXPECT_LE(dq, 1.0f) << "Should be clamped to max 1.0";

    // Test clamping below 0.0
    gt_exec_fep_bridge_update_decision_quality(bridge, -0.5f);
    dq = gt_exec_fep_bridge_get_decision_quality(bridge);
    EXPECT_GE(dq, 0.0f) << "Should be clamped to min 0.0";
}

/* ============================================================================
 * Recommendation Result Tests
 * ============================================================================ */

/**
 * Test: NotifyRecommendationFollowed
 * Verify recommendation follow tracking
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, NotifyRecommendationFollowed) {
    ASSERT_NE(bridge, nullptr);

    int result = gt_exec_fep_bridge_notify_recommendation_result(bridge, true, 0.8f);
    EXPECT_EQ(result, 0) << "Notification should succeed";

    gt_exec_fep_stats_t stats;
    gt_exec_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.recommendations_followed, 1u)
        << "Followed count should increment";
}

/**
 * Test: NotifyRecommendationOverridden
 * Verify recommendation override tracking
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, NotifyRecommendationOverridden) {
    ASSERT_NE(bridge, nullptr);

    int result = gt_exec_fep_bridge_notify_recommendation_result(bridge, false, 0.3f);
    EXPECT_EQ(result, 0) << "Notification should succeed";

    gt_exec_fep_stats_t stats;
    gt_exec_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.recommendations_overridden, 1u)
        << "Override count should increment";
}

/**
 * Test: RecommendationAccuracyTracking
 * Verify recommendation accuracy is computed correctly
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, RecommendationAccuracyTracking) {
    ASSERT_NE(bridge, nullptr);

    // Follow 3, override 1
    gt_exec_fep_bridge_notify_recommendation_result(bridge, true, 0.7f);
    gt_exec_fep_bridge_notify_recommendation_result(bridge, true, 0.8f);
    gt_exec_fep_bridge_notify_recommendation_result(bridge, true, 0.6f);
    gt_exec_fep_bridge_notify_recommendation_result(bridge, false, 0.4f);

    gt_exec_fep_metrics_t metrics;
    gt_exec_fep_bridge_get_metrics(bridge, &metrics);

    // Accuracy should be 3/4 = 0.75
    EXPECT_NEAR(metrics.recommendation_accuracy, 0.75f, 0.01f)
        << "Recommendation accuracy should be 75%";
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Comprehensive test for NULL parameter handling across all functions
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, NullHandling) {
    // Lifecycle
    gt_exec_fep_bridge_destroy(nullptr);

    // Registration
    EXPECT_EQ(gt_exec_fep_bridge_reset(nullptr), -1);
    EXPECT_FALSE(gt_exec_fep_bridge_is_registered(nullptr));
    EXPECT_EQ(gt_exec_fep_bridge_get_id(nullptr), 0u);
    EXPECT_EQ(gt_exec_fep_bridge_unregister(nullptr), -1);

    // Updates
    EXPECT_EQ(gt_exec_fep_update_callback(nullptr), -1);
    EXPECT_EQ(gt_exec_fep_bridge_force_update(nullptr), -1);
    EXPECT_EQ(gt_exec_fep_bridge_update_decision_quality(nullptr, 0.5f), -1);
    EXPECT_EQ(gt_exec_fep_bridge_update_executive_alignment(nullptr, 0.5f), -1);
    EXPECT_EQ(gt_exec_fep_bridge_update_action_coherence(nullptr, 0.5f), -1);
    EXPECT_EQ(gt_exec_fep_bridge_notify_recommendation_result(nullptr, true, 0.5f), -1);

    // Metrics
    gt_exec_fep_metrics_t metrics;
    EXPECT_EQ(gt_exec_fep_bridge_get_metrics(nullptr, &metrics), -1);
    EXPECT_EQ(gt_exec_fep_bridge_get_metrics(bridge, nullptr), -1);

    gt_exec_fep_stats_t stats;
    EXPECT_EQ(gt_exec_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(gt_exec_fep_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(gt_exec_fep_bridge_reset_stats(nullptr), -1);

    // State queries
    EXPECT_EQ(gt_exec_fep_bridge_get_state(nullptr), GT_EXEC_FEP_STATE_ERROR);
    EXPECT_LT(gt_exec_fep_bridge_get_free_energy(nullptr), 0.0f);
    EXPECT_LT(gt_exec_fep_bridge_get_prediction_error(nullptr), 0.0f);
    EXPECT_LT(gt_exec_fep_bridge_get_decision_quality(nullptr), 0.0f);
    EXPECT_LT(gt_exec_fep_bridge_get_executive_alignment(nullptr), 0.0f);
    EXPECT_FALSE(gt_exec_fep_bridge_is_degraded(nullptr));
    EXPECT_FALSE(gt_exec_fep_bridge_is_exec_aligned(nullptr));

    // Callbacks
    EXPECT_EQ(gt_exec_fep_bridge_set_high_fe_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(gt_exec_fep_bridge_set_surprise_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(gt_exec_fep_bridge_set_metrics_callback(nullptr, nullptr, nullptr), -1);

    // Config
    gt_exec_fep_config_t cfg;
    EXPECT_EQ(gt_exec_fep_bridge_set_config(nullptr, &cfg), -1);
    EXPECT_EQ(gt_exec_fep_bridge_set_config(bridge, nullptr), -1);
    EXPECT_EQ(gt_exec_fep_bridge_get_config(nullptr, &cfg), -1);
    EXPECT_EQ(gt_exec_fep_bridge_get_config(bridge, nullptr), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/* ============================================================================
 * Error Conditions Tests
 * ============================================================================ */

/**
 * Test: ErrorConditions
 * Verify proper error handling for various edge cases
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, ErrorConditions) {
    ASSERT_NE(bridge, nullptr);

    // Unregister when not registered should succeed (no-op)
    int result = gt_exec_fep_bridge_unregister(bridge);
    EXPECT_EQ(result, 0) << "Unregister when not registered should be no-op";

    // Double destroy should be safe (after first destroy, pointer is invalid
    // but we test with different bridges)
    gt_exec_fep_bridge_t* temp_bridge = gt_exec_fep_bridge_create(nullptr);
    ASSERT_NE(temp_bridge, nullptr);
    gt_exec_fep_bridge_destroy(temp_bridge);
    // temp_bridge is now invalid, but destroy(nullptr) should be safe
    gt_exec_fep_bridge_destroy(nullptr);

    SUCCEED() << "Error condition tests passed";
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Verify basic thread safety with concurrent access
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, ThreadSafety) {
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
                gt_exec_fep_bridge_get_free_energy(bridge);
                gt_exec_fep_bridge_get_prediction_error(bridge);
                gt_exec_fep_bridge_get_decision_quality(bridge);
                gt_exec_fep_bridge_get_executive_alignment(bridge);
                gt_exec_fep_bridge_get_state(bridge);
                gt_exec_fep_bridge_is_degraded(bridge);
                gt_exec_fep_bridge_is_exec_aligned(bridge);

                gt_exec_fep_metrics_t metrics;
                gt_exec_fep_bridge_get_metrics(bridge, &metrics);

                gt_exec_fep_stats_t stats;
                gt_exec_fep_bridge_get_stats(bridge, &stats);

                // Write operations
                gt_exec_fep_bridge_update_decision_quality(bridge,
                    (float)(i % 10) / 10.0f);
                gt_exec_fep_bridge_force_update(bridge);
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
TEST_F(GameTheoryExecutiveFepBridgeTest, DestroyCallback) {
    ASSERT_NE(bridge, nullptr);

    // Destroy callback should be safe to call
    gt_exec_fep_destroy_callback(bridge);
    SUCCEED() << "Destroy callback should not crash";

    // Should also handle NULL
    gt_exec_fep_destroy_callback(nullptr);
    SUCCEED() << "Destroy callback with NULL should not crash";
}

/* ============================================================================
 * Integration Scenario Tests
 * ============================================================================ */

/**
 * Test: DecisionMakingScenario
 * Simulate a strategic decision-making scenario
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, DecisionMakingScenario) {
    ASSERT_NE(bridge, nullptr);

    // Register metrics callback to track changes
    gt_exec_fep_bridge_set_metrics_callback(bridge, test_metrics_callback, nullptr);

    // Scenario: Start with poor decisions, gradually improve

    // Phase 1: Initial poor decision quality
    gt_exec_fep_bridge_update_decision_quality(bridge, 0.2f);
    gt_exec_fep_bridge_update_executive_alignment(bridge, 0.3f);
    gt_exec_fep_bridge_update_action_coherence(bridge, 0.25f);
    gt_exec_fep_bridge_force_update(bridge);

    float initial_fe = gt_exec_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(initial_fe, config.baseline_free_energy)
        << "Poor metrics should produce elevated free energy";

    // Phase 2: Executive starts following recommendations
    gt_exec_fep_bridge_update_executive_alignment(bridge, 0.7f);
    gt_exec_fep_bridge_notify_recommendation_result(bridge, true, 0.6f);
    gt_exec_fep_bridge_force_update(bridge);

    float learning_fe = gt_exec_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(learning_fe, initial_fe)
        << "Better alignment should reduce free energy";

    // Phase 3: Decision quality improves
    gt_exec_fep_bridge_update_decision_quality(bridge, 0.8f);
    gt_exec_fep_bridge_notify_recommendation_result(bridge, true, 0.8f);
    gt_exec_fep_bridge_force_update(bridge);

    float improved_fe = gt_exec_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(improved_fe, learning_fe)
        << "Better decision quality should reduce free energy";

    // Phase 4: Full alignment achieved
    gt_exec_fep_bridge_update_executive_alignment(bridge, 0.99f);
    gt_exec_fep_bridge_update_action_coherence(bridge, 0.95f);
    gt_exec_fep_bridge_force_update(bridge);

    EXPECT_TRUE(gt_exec_fep_bridge_is_exec_aligned(bridge))
        << "Should detect executive alignment";

    float final_fe = gt_exec_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(final_fe, improved_fe)
        << "Full alignment should have lowest free energy";

    // Verify callback was called for each update
    EXPECT_GE(g_metrics_callback_count.load(), 4)
        << "Metrics callback should be called for each update";
}

/**
 * Test: ExecutiveOverrideScenario
 * Simulate scenario where executive overrides recommendations
 */
TEST_F(GameTheoryExecutiveFepBridgeTest, ExecutiveOverrideScenario) {
    ASSERT_NE(bridge, nullptr);

    // Start with good metrics
    gt_exec_fep_bridge_update_decision_quality(bridge, 0.8f);
    gt_exec_fep_bridge_update_executive_alignment(bridge, 0.9f);
    gt_exec_fep_bridge_update_action_coherence(bridge, 0.85f);
    gt_exec_fep_bridge_force_update(bridge);

    float initial_fe = gt_exec_fep_bridge_get_free_energy(bridge);

    // Executive starts overriding recommendations
    for (int i = 0; i < 5; i++) {
        gt_exec_fep_bridge_notify_recommendation_result(bridge, false, 0.4f);
    }

    // Alignment should decrease
    gt_exec_fep_metrics_t metrics;
    gt_exec_fep_bridge_get_metrics(bridge, &metrics);

    // Override rate should affect alignment in actual bridge integration
    gt_exec_fep_stats_t stats;
    gt_exec_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.recommendations_overridden, 5u)
        << "Should track override count";

    // Recommendation accuracy should reflect low follow rate
    EXPECT_LT(metrics.recommendation_accuracy, 0.5f)
        << "Low follow rate should show in accuracy";
}
