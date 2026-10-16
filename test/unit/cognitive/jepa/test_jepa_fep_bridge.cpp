/**
 * @file test_jepa_fep_bridge.cpp
 * @brief Comprehensive unit tests for JEPA FEP Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * Tests cover:
 * - Bridge creation and destruction
 * - Configuration validation
 * - FEP orchestrator registration/unregistration
 * - Update callbacks and force updates
 * - Free energy contribution computation
 * - Embedding prediction error tracking
 * - Representation quality tracking
 * - Representation collapse detection
 * - Statistics tracking
 * - Callback registration
 * - Thread safety
 * - Null handling and error conditions
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/jepa/nimcp_jepa_fep_bridge.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "utils/error/nimcp_error_codes.h"

namespace {

//=============================================================================
// Constants
//=============================================================================

static const float TEST_EPSILON = 1e-5f;
static const float TEST_PREDICTION_ERROR_LOW = 0.1f;
static const float TEST_PREDICTION_ERROR_HIGH = 1.5f;
static const float TEST_QUALITY_HIGH = 0.9f;
static const float TEST_QUALITY_LOW = 0.1f;

//=============================================================================
// Test Fixture
//=============================================================================

class JepaFepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = jepa_fep_config_default();
        bridge_ = jepa_fep_bridge_create(&config_);
        ASSERT_NE(bridge_, nullptr);
    }

    void TearDown() override {
        if (bridge_) {
            jepa_fep_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    jepa_fep_config_t config_;
    jepa_fep_bridge_t* bridge_;
};

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, DefaultConfigValidValues) {
    jepa_fep_config_t default_config = jepa_fep_config_default();

    /* Check weighting parameters */
    EXPECT_FLOAT_EQ(default_config.free_energy_weight, 1.0f);
    EXPECT_GT(default_config.embedding_prediction_error_weight, 0.0f);
    EXPECT_LE(default_config.embedding_prediction_error_weight, 1.0f);
    EXPECT_GT(default_config.representation_collapse_penalty, 0.0f);
    EXPECT_LE(default_config.representation_collapse_penalty, 1.0f);

    /* Check thresholds */
    EXPECT_GT(default_config.high_free_energy_threshold, 0.0f);
    EXPECT_GT(default_config.collapse_detection_threshold, 0.0f);
    EXPECT_LT(default_config.collapse_detection_threshold, 1.0f);
    EXPECT_GT(default_config.prediction_quality_threshold, 0.0f);
    EXPECT_LT(default_config.prediction_quality_threshold, 1.0f);

    /* Check behavior flags */
    EXPECT_GT(default_config.update_interval_ms, 0u);
    EXPECT_TRUE(default_config.enable_collapse_detection);
}

//=============================================================================
// Creation/Destruction Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, BridgeCreation) {
    /* Bridge created in SetUp */
    EXPECT_NE(bridge_, nullptr);

    /* Check initial state */
    EXPECT_EQ(jepa_fep_bridge_get_state(bridge_), JEPA_FEP_STATE_IDLE);
    EXPECT_FALSE(jepa_fep_bridge_is_registered(bridge_));
    EXPECT_EQ(jepa_fep_bridge_get_id(bridge_), 0u);
}

TEST_F(JepaFepBridgeTest, BridgeCreationWithNullConfig) {
    /* Should use default config */
    jepa_fep_bridge_t* bridge = jepa_fep_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Should be in idle state with default settings */
    EXPECT_EQ(jepa_fep_bridge_get_state(bridge), JEPA_FEP_STATE_IDLE);

    jepa_fep_bridge_destroy(bridge);
}

TEST_F(JepaFepBridgeTest, BridgeDestruction) {
    jepa_fep_bridge_t* bridge = jepa_fep_bridge_create(&config_);
    ASSERT_NE(bridge, nullptr);

    jepa_fep_bridge_destroy(bridge);
    /* Should not crash */
}

TEST_F(JepaFepBridgeTest, DestroyNullSafe) {
    jepa_fep_bridge_destroy(nullptr);
    /* Should not crash */
}

TEST_F(JepaFepBridgeTest, BridgeReset) {
    /* Record some data */
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge_, 0.5f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 0.7f), 0);

    /* Force update */
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge_), 0);

    /* Verify stats changed */
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge_, &stats), 0);
    EXPECT_GT(stats.total_updates, 0u);

    /* Reset */
    EXPECT_EQ(jepa_fep_bridge_reset(bridge_), 0);

    /* Verify stats reset */
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge_, &stats), 0);
    EXPECT_EQ(stats.total_updates, 0u);
    EXPECT_EQ(stats.embedding_predictions, 0u);

    /* Verify state reset */
    EXPECT_EQ(jepa_fep_bridge_get_state(bridge_), JEPA_FEP_STATE_IDLE);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, ConfigValidation) {
    jepa_fep_config_t new_config = jepa_fep_config_default();
    new_config.free_energy_weight = 0.8f;
    new_config.embedding_prediction_error_weight = 0.7f;
    new_config.enable_logging = true;

    EXPECT_EQ(jepa_fep_bridge_set_config(bridge_, &new_config), 0);

    jepa_fep_config_t retrieved_config;
    EXPECT_EQ(jepa_fep_bridge_get_config(bridge_, &retrieved_config), 0);

    EXPECT_FLOAT_EQ(retrieved_config.free_energy_weight, 0.8f);
    EXPECT_FLOAT_EQ(retrieved_config.embedding_prediction_error_weight, 0.7f);
    EXPECT_TRUE(retrieved_config.enable_logging);
}

TEST_F(JepaFepBridgeTest, ConfigSetNullBridge) {
    jepa_fep_config_t config = jepa_fep_config_default();
    EXPECT_EQ(jepa_fep_bridge_set_config(nullptr, &config), -1);
}

TEST_F(JepaFepBridgeTest, ConfigSetNullConfig) {
    EXPECT_EQ(jepa_fep_bridge_set_config(bridge_, nullptr), -1);
}

TEST_F(JepaFepBridgeTest, ConfigGetNullBridge) {
    jepa_fep_config_t config;
    EXPECT_EQ(jepa_fep_bridge_get_config(nullptr, &config), -1);
}

TEST_F(JepaFepBridgeTest, ConfigGetNullOutput) {
    EXPECT_EQ(jepa_fep_bridge_get_config(bridge_, nullptr), -1);
}

//=============================================================================
// Registration Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, RegisterWithFEPWithoutOrchestrator) {
    uint32_t bridge_id = 0;

    /* Cannot register without orchestrator */
    EXPECT_EQ(jepa_fep_bridge_register(bridge_, nullptr, nullptr, &bridge_id), -1);
    EXPECT_FALSE(jepa_fep_bridge_is_registered(bridge_));
}

TEST_F(JepaFepBridgeTest, UnregisterFromFEPWithoutRegistration) {
    /* Unregister when not registered should be no-op */
    EXPECT_EQ(jepa_fep_bridge_unregister(bridge_), 0);
    EXPECT_FALSE(jepa_fep_bridge_is_registered(bridge_));
}

TEST_F(JepaFepBridgeTest, RegisterNullBridge) {
    fep_orchestrator_t* orch = fep_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    uint32_t bridge_id = 0;
    EXPECT_EQ(jepa_fep_bridge_register(nullptr, orch, nullptr, &bridge_id), -1);

    fep_orchestrator_destroy(orch);
}

TEST_F(JepaFepBridgeTest, UnregisterNullBridge) {
    EXPECT_EQ(jepa_fep_bridge_unregister(nullptr), -1);
}

TEST_F(JepaFepBridgeTest, RegisterWithFEP) {
    fep_orchestrator_t* orch = fep_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    uint32_t bridge_id = 0;
    int ret = jepa_fep_bridge_register(bridge_, orch, nullptr, &bridge_id);
    EXPECT_EQ(ret, 0);

    if (ret == 0) {
        EXPECT_TRUE(jepa_fep_bridge_is_registered(bridge_));
        EXPECT_GT(bridge_id, 0u);
        EXPECT_EQ(jepa_fep_bridge_get_id(bridge_), bridge_id);
        EXPECT_EQ(jepa_fep_bridge_get_state(bridge_), JEPA_FEP_STATE_ACTIVE);
    }

    /* Unregister */
    EXPECT_EQ(jepa_fep_bridge_unregister(bridge_), 0);
    EXPECT_FALSE(jepa_fep_bridge_is_registered(bridge_));

    fep_orchestrator_destroy(orch);
}

TEST_F(JepaFepBridgeTest, DoubleRegisterIdemopotent) {
    fep_orchestrator_t* orch = fep_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    uint32_t bridge_id1 = 0;
    uint32_t bridge_id2 = 0;

    /* First registration */
    int ret = jepa_fep_bridge_register(bridge_, orch, nullptr, &bridge_id1);
    EXPECT_EQ(ret, 0);

    /* Second registration should succeed and return same ID */
    ret = jepa_fep_bridge_register(bridge_, orch, nullptr, &bridge_id2);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bridge_id1, bridge_id2);

    jepa_fep_bridge_unregister(bridge_);
    fep_orchestrator_destroy(orch);
}

//=============================================================================
// Update Callback Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, UpdateCallback) {
    /* Perform manual update */
    EXPECT_EQ(jepa_fep_bridge_update(bridge_), 0);

    /* Verify stats updated */
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge_, &stats), 0);
    EXPECT_EQ(stats.total_updates, 1u);
}

TEST_F(JepaFepBridgeTest, ForceUpdate) {
    /* Record some data */
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge_, 0.3f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 0.8f), 0);

    /* Force update */
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge_), 0);

    /* Verify stats updated */
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge_, &stats), 0);
    EXPECT_EQ(stats.total_updates, 1u);

    /* Free energy should be computed */
    float fe = jepa_fep_bridge_get_free_energy_contribution(bridge_);
    EXPECT_GT(fe, 0.0f);  /* Should be at least baseline */
}

TEST_F(JepaFepBridgeTest, UpdateNullBridge) {
    EXPECT_EQ(jepa_fep_bridge_update(nullptr), -1);
}

TEST_F(JepaFepBridgeTest, ForceUpdateNullBridge) {
    EXPECT_EQ(jepa_fep_bridge_force_update(nullptr), -1);
}

//=============================================================================
// Free Energy Contribution Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, FreeEnergyContribution) {
    /* Initial free energy should be baseline */
    float initial_fe = jepa_fep_bridge_get_free_energy_contribution(bridge_);
    EXPECT_NEAR(initial_fe, JEPA_FEP_BASELINE_FREE_ENERGY, TEST_EPSILON);
}

TEST_F(JepaFepBridgeTest, FreeEnergyContributionNullBridge) {
    EXPECT_LT(jepa_fep_bridge_get_free_energy_contribution(nullptr), 0.0f);
}

TEST_F(JepaFepBridgeTest, EmbeddingPredictionErrorIncreasesFreeEnergy) {
    /* Record low prediction error */
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge_, TEST_PREDICTION_ERROR_LOW), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge_), 0);
    float fe_low = jepa_fep_bridge_get_free_energy_contribution(bridge_);

    /* Reset and record high prediction error */
    EXPECT_EQ(jepa_fep_bridge_reset(bridge_), 0);
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge_, TEST_PREDICTION_ERROR_HIGH), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge_), 0);
    float fe_high = jepa_fep_bridge_get_free_energy_contribution(bridge_);

    /* Higher prediction error should result in higher free energy */
    EXPECT_GT(fe_high, fe_low);
}

TEST_F(JepaFepBridgeTest, GoodPredictiveEmbeddingsReduceFreeEnergy) {
    /* Record poor quality */
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, TEST_QUALITY_LOW), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge_), 0);
    float fe_poor = jepa_fep_bridge_get_free_energy_contribution(bridge_);

    /* Reset and record good quality */
    EXPECT_EQ(jepa_fep_bridge_reset(bridge_), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, TEST_QUALITY_HIGH), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge_), 0);
    float fe_good = jepa_fep_bridge_get_free_energy_contribution(bridge_);

    /* Better quality should result in lower free energy */
    EXPECT_LT(fe_good, fe_poor);
}

TEST_F(JepaFepBridgeTest, RepresentationCollapseHighPenalty) {
    /* Enable collapse detection */
    jepa_fep_config_t config = jepa_fep_config_default();
    config.enable_collapse_detection = true;
    config.collapse_detection_threshold = 0.3f;
    config.representation_collapse_penalty = 0.5f;
    EXPECT_EQ(jepa_fep_bridge_set_config(bridge_, &config), 0);

    /* Record quality just above threshold - no collapse */
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 0.5f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge_), 0);
    float fe_no_collapse = jepa_fep_bridge_get_free_energy_contribution(bridge_);

    /* Reset and record quality below threshold - collapse */
    EXPECT_EQ(jepa_fep_bridge_reset(bridge_), 0);
    EXPECT_EQ(jepa_fep_bridge_set_config(bridge_, &config), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 0.1f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge_), 0);
    float fe_collapse = jepa_fep_bridge_get_free_energy_contribution(bridge_);

    /* Collapse should result in higher free energy */
    EXPECT_GT(fe_collapse, fe_no_collapse);

    /* Check collapse detection stats */
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge_, &stats), 0);
    EXPECT_GT(stats.collapse_detections, 0u);
}

//=============================================================================
// Embedding Prediction Error Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, EmbeddingPredictionError) {
    /* Record prediction error */
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge_, 0.5f), 0);

    /* Get prediction error */
    float error = jepa_fep_bridge_get_embedding_prediction_error(bridge_);
    EXPECT_FLOAT_EQ(error, 0.5f);
}

TEST_F(JepaFepBridgeTest, RecordPredictionErrorNullBridge) {
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(nullptr, 0.5f), -1);
}

TEST_F(JepaFepBridgeTest, RecordPredictionErrorNegativeValue) {
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge_, -0.5f), -1);
}

TEST_F(JepaFepBridgeTest, GetEmbeddingPredictionErrorNullBridge) {
    EXPECT_LT(jepa_fep_bridge_get_embedding_prediction_error(nullptr), 0.0f);
}

//=============================================================================
// Representation Quality Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, RepresentationQuality) {
    /* Record representation quality */
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 0.75f), 0);

    /* Get representation quality */
    float quality = jepa_fep_bridge_get_representation_quality(bridge_);
    EXPECT_FLOAT_EQ(quality, 0.75f);
}

TEST_F(JepaFepBridgeTest, RecordRepresentationQualityNullBridge) {
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(nullptr, 0.5f), -1);
}

TEST_F(JepaFepBridgeTest, RecordRepresentationQualityOutOfRange) {
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, -0.5f), -1);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 1.5f), -1);
}

TEST_F(JepaFepBridgeTest, GetRepresentationQualityNullBridge) {
    EXPECT_LT(jepa_fep_bridge_get_representation_quality(nullptr), 0.0f);
}

//=============================================================================
// Statistics Tracking Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, StatisticsTracking) {
    /* Record some data and perform updates */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge_, 0.2f + i * 0.1f), 0);
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 0.8f - i * 0.05f), 0);
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge_), 0);
    }

    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge_, &stats), 0);

    EXPECT_EQ(stats.total_updates, 5u);
    EXPECT_EQ(stats.embedding_predictions, 5u);
    EXPECT_EQ(stats.representation_updates, 5u);
    EXPECT_GE(stats.avg_update_time_us, 0.0f);  /* May be 0 on fast systems */
    EXPECT_GT(stats.total_free_energy_contribution, 0.0f);
}

TEST_F(JepaFepBridgeTest, StatsReset) {
    /* Record data and update */
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge_, 0.5f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge_), 0);

    /* Verify stats exist */
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge_, &stats), 0);
    EXPECT_GT(stats.total_updates, 0u);

    /* Reset stats only */
    EXPECT_EQ(jepa_fep_bridge_reset_stats(bridge_), 0);

    /* Verify stats reset */
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge_, &stats), 0);
    EXPECT_EQ(stats.total_updates, 0u);
    EXPECT_EQ(stats.embedding_predictions, 0u);
    EXPECT_FLOAT_EQ(stats.avg_update_time_us, 0.0f);
}

TEST_F(JepaFepBridgeTest, GetStatsNullBridge) {
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(nullptr, &stats), -1);
}

TEST_F(JepaFepBridgeTest, GetStatsNullOutput) {
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge_, nullptr), -1);
}

TEST_F(JepaFepBridgeTest, ResetStatsNullBridge) {
    EXPECT_EQ(jepa_fep_bridge_reset_stats(nullptr), -1);
}

//=============================================================================
// State Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, StateTransitions) {
    /* Initial state is IDLE */
    EXPECT_EQ(jepa_fep_bridge_get_state(bridge_), JEPA_FEP_STATE_IDLE);

    /* Register with FEP */
    fep_orchestrator_t* orch = fep_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    uint32_t bridge_id = 0;
    if (jepa_fep_bridge_register(bridge_, orch, nullptr, &bridge_id) == 0) {
        /* After registration, state should be ACTIVE */
        EXPECT_EQ(jepa_fep_bridge_get_state(bridge_), JEPA_FEP_STATE_ACTIVE);
    }

    jepa_fep_bridge_unregister(bridge_);
    fep_orchestrator_destroy(orch);
}

TEST_F(JepaFepBridgeTest, DegradedState) {
    /* Configure low threshold */
    jepa_fep_config_t config = jepa_fep_config_default();
    config.high_free_energy_threshold = 0.1f;  /* Very low threshold */
    EXPECT_EQ(jepa_fep_bridge_set_config(bridge_, &config), 0);

    /* Record high error to trigger degraded state */
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge_, 2.0f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 0.1f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge_), 0);

    /* Should be in degraded state */
    EXPECT_TRUE(jepa_fep_bridge_is_degraded(bridge_));
    EXPECT_EQ(jepa_fep_bridge_get_state(bridge_), JEPA_FEP_STATE_DEGRADED);
}

TEST_F(JepaFepBridgeTest, GetStateNullBridge) {
    EXPECT_EQ(jepa_fep_bridge_get_state(nullptr), JEPA_FEP_STATE_ERROR);
}

TEST_F(JepaFepBridgeTest, IsDegradedNullBridge) {
    EXPECT_FALSE(jepa_fep_bridge_is_degraded(nullptr));
}

//=============================================================================
// Callback Registration Tests
//=============================================================================

static std::atomic<int> s_high_fe_callback_count{0};
static std::atomic<float> s_last_fe_value{0.0f};

static void test_high_fe_callback(
    jepa_fep_bridge_t* bridge,
    float free_energy,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    s_high_fe_callback_count++;
    s_last_fe_value = free_energy;
}

TEST_F(JepaFepBridgeTest, HighFreeEnergyCallback) {
    s_high_fe_callback_count = 0;
    s_last_fe_value = 0.0f;

    /* Register callback */
    EXPECT_EQ(jepa_fep_bridge_set_high_fe_callback(bridge_, test_high_fe_callback, nullptr), 0);

    /* Configure low threshold */
    jepa_fep_config_t config = jepa_fep_config_default();
    config.high_free_energy_threshold = 0.1f;
    EXPECT_EQ(jepa_fep_bridge_set_config(bridge_, &config), 0);

    /* Trigger high free energy */
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge_, 2.0f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 0.1f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge_), 0);

    /* Callback should have been called */
    EXPECT_GE(s_high_fe_callback_count.load(), 1);
    EXPECT_GT(s_last_fe_value.load(), config.high_free_energy_threshold);
}

static std::atomic<int> s_collapse_callback_count{0};
static std::atomic<float> s_last_collapse_severity{0.0f};

static void test_collapse_callback(
    jepa_fep_bridge_t* bridge,
    float collapse_severity,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    s_collapse_callback_count++;
    s_last_collapse_severity = collapse_severity;
}

TEST_F(JepaFepBridgeTest, CollapseCallback) {
    s_collapse_callback_count = 0;
    s_last_collapse_severity = 0.0f;

    /* Register callback */
    EXPECT_EQ(jepa_fep_bridge_set_collapse_callback(bridge_, test_collapse_callback, nullptr), 0);

    /* Configure collapse detection */
    jepa_fep_config_t config = jepa_fep_config_default();
    config.enable_collapse_detection = true;
    config.collapse_detection_threshold = 0.3f;
    EXPECT_EQ(jepa_fep_bridge_set_config(bridge_, &config), 0);

    /* Trigger collapse */
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 0.05f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge_), 0);

    /* Callback should have been called */
    EXPECT_GE(s_collapse_callback_count.load(), 1);
    EXPECT_GT(s_last_collapse_severity.load(), 0.0f);
}

TEST_F(JepaFepBridgeTest, SetHighFeCallbackNullBridge) {
    EXPECT_EQ(jepa_fep_bridge_set_high_fe_callback(nullptr, test_high_fe_callback, nullptr), -1);
}

TEST_F(JepaFepBridgeTest, SetCollapseCallbackNullBridge) {
    EXPECT_EQ(jepa_fep_bridge_set_collapse_callback(nullptr, test_collapse_callback, nullptr), -1);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, ThreadSafety) {
    static const int NUM_THREADS = 4;
    static const int ITERATIONS_PER_THREAD = 100;

    std::vector<std::thread> threads;
    std::atomic<int> error_count{0};

    /* Spawn threads that access bridge concurrently */
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &error_count]() {
            for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
                /* Mix of operations */
                if (t % 4 == 0) {
                    float error = 0.1f + (i % 10) * 0.1f;
                    if (jepa_fep_bridge_record_prediction_error(bridge_, error) != 0) {
                        error_count++;
                    }
                } else if (t % 4 == 1) {
                    float quality = 0.5f + (i % 5) * 0.1f;
                    if (jepa_fep_bridge_record_representation_quality(bridge_, quality) != 0) {
                        error_count++;
                    }
                } else if (t % 4 == 2) {
                    if (jepa_fep_bridge_force_update(bridge_) != 0) {
                        error_count++;
                    }
                } else {
                    jepa_fep_stats_t stats;
                    if (jepa_fep_bridge_get_stats(bridge_, &stats) != 0) {
                        error_count++;
                    }
                    jepa_fep_bridge_get_free_energy_contribution(bridge_);
                    jepa_fep_bridge_get_representation_quality(bridge_);
                }
            }
        });
    }

    /* Wait for all threads */
    for (auto& t : threads) {
        t.join();
    }

    /* Should have no errors */
    EXPECT_EQ(error_count.load(), 0);

    /* Verify state is still consistent */
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge_, &stats), 0);
    EXPECT_GT(stats.total_updates, 0u);
}

//=============================================================================
// Null Handling Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, NullHandling) {
    EXPECT_EQ(jepa_fep_bridge_reset(nullptr), -1);
    EXPECT_EQ(jepa_fep_bridge_get_id(nullptr), 0u);
    EXPECT_FALSE(jepa_fep_bridge_is_registered(nullptr));
}

//=============================================================================
// Error Conditions Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, ErrorConditions) {
    /* Invalid prediction error value */
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge_, -1.0f), -1);

    /* Invalid quality values */
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, -0.1f), -1);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 1.1f), -1);

    /* Valid values should work */
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge_, 0.0f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 0.0f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 1.0f), 0);
}

//=============================================================================
// State Name Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, StateName) {
    EXPECT_STREQ(jepa_fep_state_name(JEPA_FEP_STATE_UNINITIALIZED), "uninitialized");
    EXPECT_STREQ(jepa_fep_state_name(JEPA_FEP_STATE_IDLE), "idle");
    EXPECT_STREQ(jepa_fep_state_name(JEPA_FEP_STATE_ACTIVE), "active");
    EXPECT_STREQ(jepa_fep_state_name(JEPA_FEP_STATE_DEGRADED), "degraded");
    EXPECT_STREQ(jepa_fep_state_name(JEPA_FEP_STATE_ERROR), "error");
    EXPECT_STREQ(jepa_fep_state_name((jepa_fep_state_t)999), "unknown");
}

//=============================================================================
// Peak Free Energy Tracking Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, PeakFreeEnergyTracking) {
    /* Generate varying free energy */
    float max_error = 0.0f;
    for (int i = 0; i < 10; i++) {
        float error = (i % 5 == 2) ? 1.5f : 0.2f;  /* Spike at i=2,7 */
        if (error > max_error) max_error = error;

        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge_, error), 0);
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge_), 0);
    }

    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge_, &stats), 0);

    /* Peak should be greater than baseline */
    EXPECT_GT(stats.peak_free_energy, JEPA_FEP_BASELINE_FREE_ENERGY);
}

//=============================================================================
// Min Representation Quality Tracking Tests
//=============================================================================

TEST_F(JepaFepBridgeTest, MinRepresentationQualityTracking) {
    /* Record varying qualities */
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 0.8f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 0.3f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge_, 0.5f), 0);

    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge_, &stats), 0);

    /* Min should be 0.3 */
    EXPECT_FLOAT_EQ(stats.min_representation_quality, 0.3f);
}

}  // namespace

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
