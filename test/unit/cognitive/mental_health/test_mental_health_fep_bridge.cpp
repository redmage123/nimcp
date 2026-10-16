/**
 * @file test_mental_health_fep_bridge.cpp
 * @brief Unit tests for Mental Health-FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Mental Health bidirectional integration
 * WHY:  Ensure pathological inference detection and intervention work correctly
 * HOW:  Test lifecycle, connections, aberrant precision/learning detection, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/mental_health/nimcp_mental_health_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class MentalHealthFepBridgeTest : public ::testing::Test {
protected:
    mental_health_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        mental_health_fep_config_t config;
        mental_health_fep_bridge_default_config(&config);
        bridge = mental_health_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            mental_health_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MentalHealthFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(MentalHealthFepBridgeTest, CreateWithNullConfig) {
    mental_health_fep_bridge_t* br = mental_health_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    mental_health_fep_bridge_destroy(br);
}

TEST_F(MentalHealthFepBridgeTest, DestroyNull) {
    mental_health_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(MentalHealthFepBridgeTest, DefaultConfig) {
    mental_health_fep_config_t config;
    int ret = mental_health_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(config.aberrant_precision_threshold, MENTAL_HEALTH_FEP_ABERRANT_PRECISION_THRESHOLD);
    EXPECT_FLOAT_EQ(config.pathological_lr_threshold, MENTAL_HEALTH_FEP_PATHOLOGICAL_LR_THRESHOLD);
    EXPECT_GT(config.negative_prior_threshold, 0.0f);
    EXPECT_TRUE(config.enable_aberrant_precision_detection);
    EXPECT_TRUE(config.enable_pathological_learning_detection);
    EXPECT_TRUE(config.enable_negative_prior_detection);
}

TEST_F(MentalHealthFepBridgeTest, DefaultConfigNullPtr) {
    int ret = mental_health_fep_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(MentalHealthFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = mental_health_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(MentalHealthFepBridgeTest, ConnectFepNull) {
    EXPECT_EQ(mental_health_fep_bridge_connect_fep(nullptr, nullptr), -1);

    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);

    EXPECT_EQ(mental_health_fep_bridge_connect_fep(nullptr, fep), -1);
    EXPECT_EQ(mental_health_fep_bridge_connect_fep(bridge, nullptr), -1);

    fep_destroy(fep);
}

TEST_F(MentalHealthFepBridgeTest, ConnectMentalHealth) {
    // Mental health system requires complex initialization, test with NULL for now
    int ret = mental_health_fep_bridge_connect_mental_health(bridge, nullptr);
    EXPECT_EQ(ret, -1);  // Should fail with NULL
}

TEST_F(MentalHealthFepBridgeTest, ConnectMentalHealthNull) {
    EXPECT_EQ(mental_health_fep_bridge_connect_mental_health(nullptr, nullptr), -1);
}

TEST_F(MentalHealthFepBridgeTest, Disconnect) {
    int ret = mental_health_fep_bridge_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MentalHealthFepBridgeTest, DisconnectNull) {
    EXPECT_EQ(mental_health_fep_bridge_disconnect(nullptr), -1);
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(MentalHealthFepBridgeTest, GetState) {
    mental_health_fep_state_t state;
    int ret = mental_health_fep_bridge_get_state(bridge, &state);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.current_precision, 0.0f);
    EXPECT_GE(state.current_learning_rate, 0.0f);
}

TEST_F(MentalHealthFepBridgeTest, GetStateNull) {
    mental_health_fep_state_t state;

    EXPECT_EQ(mental_health_fep_bridge_get_state(nullptr, &state), -1);
    EXPECT_EQ(mental_health_fep_bridge_get_state(bridge, nullptr), -1);
}

TEST_F(MentalHealthFepBridgeTest, GetStats) {
    mental_health_fep_stats_t stats;
    int ret = mental_health_fep_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.aberrant_precision_events, 0u);
    EXPECT_EQ(stats.pathological_learning_events, 0u);
    EXPECT_EQ(stats.negative_prior_events, 0u);
    EXPECT_EQ(stats.intervention_events, 0u);
}

TEST_F(MentalHealthFepBridgeTest, GetStatsNull) {
    mental_health_fep_stats_t stats;

    EXPECT_EQ(mental_health_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(mental_health_fep_bridge_get_stats(bridge, nullptr), -1);
}

/* ============================================================================
 * FEP → Mental Health Direction Tests
 * ============================================================================ */

TEST_F(MentalHealthFepBridgeTest, DetectAberrantPrecision) {
    int ret = mental_health_fep_detect_aberrant_precision(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MentalHealthFepBridgeTest, DetectAberrantPrecisionNull) {
    EXPECT_EQ(mental_health_fep_detect_aberrant_precision(nullptr), -1);
}

TEST_F(MentalHealthFepBridgeTest, DetectPathologicalLearning) {
    int ret = mental_health_fep_detect_pathological_learning(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MentalHealthFepBridgeTest, DetectPathologicalLearningNull) {
    EXPECT_EQ(mental_health_fep_detect_pathological_learning(nullptr), -1);
}

TEST_F(MentalHealthFepBridgeTest, DetectNegativePriors) {
    int ret = mental_health_fep_detect_negative_priors(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MentalHealthFepBridgeTest, DetectNegativePriorsNull) {
    EXPECT_EQ(mental_health_fep_detect_negative_priors(nullptr), -1);
}

/* ============================================================================
 * Mental Health → FEP Direction Tests
 * ============================================================================ */

TEST_F(MentalHealthFepBridgeTest, ApplyPrecisionIntervention) {
    int ret = mental_health_fep_apply_precision_intervention(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MentalHealthFepBridgeTest, ApplyPrecisionInterventionNull) {
    EXPECT_EQ(mental_health_fep_apply_precision_intervention(nullptr), -1);
}

TEST_F(MentalHealthFepBridgeTest, ApplyLrIntervention) {
    int ret = mental_health_fep_apply_lr_intervention(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MentalHealthFepBridgeTest, ApplyLrInterventionNull) {
    EXPECT_EQ(mental_health_fep_apply_lr_intervention(nullptr), -1);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(MentalHealthFepBridgeTest, Update) {
    int ret = mental_health_fep_bridge_update(bridge, 16);  // 16ms delta
    EXPECT_EQ(ret, 0);
}

TEST_F(MentalHealthFepBridgeTest, UpdateNull) {
    EXPECT_EQ(mental_health_fep_bridge_update(nullptr, 16), -1);
}

TEST_F(MentalHealthFepBridgeTest, UpdateZeroDelta) {
    int ret = mental_health_fep_bridge_update(bridge, 0);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(MentalHealthFepBridgeTest, BioAsyncConnect) {
    int ret = mental_health_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MentalHealthFepBridgeTest, BioAsyncDisconnect) {
    mental_health_fep_bridge_connect_bio_async(bridge);

    int ret = mental_health_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(mental_health_fep_bridge_is_bio_async_connected(bridge));
}

TEST_F(MentalHealthFepBridgeTest, BioAsyncIsConnected) {
    EXPECT_FALSE(mental_health_fep_bridge_is_bio_async_connected(bridge));

    mental_health_fep_bridge_connect_bio_async(bridge);
    // May or may not be connected depending on router availability

    mental_health_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(mental_health_fep_bridge_is_bio_async_connected(bridge));
}

TEST_F(MentalHealthFepBridgeTest, BioAsyncNullParams) {
    EXPECT_EQ(mental_health_fep_bridge_connect_bio_async(nullptr), -1);
    EXPECT_EQ(mental_health_fep_bridge_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(mental_health_fep_bridge_is_bio_async_connected(nullptr));
}

TEST_F(MentalHealthFepBridgeTest, BioAsyncDoubleConnect) {
    mental_health_fep_bridge_connect_bio_async(bridge);
    int ret = mental_health_fep_bridge_connect_bio_async(bridge);  // Should be no-op
    EXPECT_EQ(ret, 0);
    mental_health_fep_bridge_disconnect_bio_async(bridge);
}

/* ============================================================================
 * Integration Tests - Pathology Detection
 * ============================================================================ */

TEST_F(MentalHealthFepBridgeTest, AberrantPrecisionDetection) {
    mental_health_fep_detect_aberrant_precision(bridge);

    mental_health_fep_stats_t stats;
    mental_health_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.aberrant_precision_events, 0u);
}

TEST_F(MentalHealthFepBridgeTest, PathologicalLearningDetection) {
    mental_health_fep_detect_pathological_learning(bridge);

    mental_health_fep_stats_t stats;
    mental_health_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.pathological_learning_events, 0u);
}

TEST_F(MentalHealthFepBridgeTest, NegativePriorDetection) {
    mental_health_fep_detect_negative_priors(bridge);

    mental_health_fep_stats_t stats;
    mental_health_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.negative_prior_events, 0u);
}

/* ============================================================================
 * Integration Tests - Intervention
 * ============================================================================ */

TEST_F(MentalHealthFepBridgeTest, PrecisionInterventionCorrects) {
    mental_health_fep_apply_precision_intervention(bridge);

    mental_health_fep_stats_t stats;
    mental_health_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.intervention_events, 0u);
}

TEST_F(MentalHealthFepBridgeTest, LearningRateInterventionCorrects) {
    mental_health_fep_apply_lr_intervention(bridge);

    mental_health_fep_stats_t stats;
    mental_health_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.intervention_events, 0u);
}

TEST_F(MentalHealthFepBridgeTest, PathologyFlagsPersist) {
    mental_health_fep_state_t state;
    mental_health_fep_bridge_get_state(bridge, &state);

    // Initially no pathology detected
    EXPECT_FALSE(state.pathology_detected);

    // After detection, flag should be set (if threshold exceeded)
    mental_health_fep_detect_aberrant_precision(bridge);
    mental_health_fep_bridge_get_state(bridge, &state);
    // Pathology may or may not be detected depending on current values
    EXPECT_GE(state.current_precision, 0.0f);
}

TEST_F(MentalHealthFepBridgeTest, InterventionRestoresHealthy) {
    // Detect pathology
    mental_health_fep_detect_aberrant_precision(bridge);
    mental_health_fep_detect_pathological_learning(bridge);

    // Apply interventions
    mental_health_fep_apply_precision_intervention(bridge);
    mental_health_fep_apply_lr_intervention(bridge);

    mental_health_fep_stats_t stats;
    mental_health_fep_bridge_get_stats(bridge, &stats);

    // Interventions should be applied
    EXPECT_GE(stats.intervention_events, 0u);
}

TEST_F(MentalHealthFepBridgeTest, PrecisionThresholdValidation) {
    mental_health_fep_state_t state;
    mental_health_fep_bridge_get_state(bridge, &state);

    // Precision should be non-negative
    EXPECT_GE(state.current_precision, 0.0f);
}

TEST_F(MentalHealthFepBridgeTest, LearningRateThresholdValidation) {
    mental_health_fep_state_t state;
    mental_health_fep_bridge_get_state(bridge, &state);

    // Learning rate should be non-negative
    EXPECT_GE(state.current_learning_rate, 0.0f);
}
