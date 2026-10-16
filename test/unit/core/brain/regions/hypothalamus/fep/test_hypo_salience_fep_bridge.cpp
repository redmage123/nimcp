/**
 * @file test_hypo_salience_fep_bridge.cpp
 * @brief Unit tests for Hypothalamus Salience FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Salience bidirectional integration
 * WHY:  Ensure drive-to-salience mapping, precision modulation, and conflict detection work
 * HOW:  Test lifecycle, connections, FE computation, salience weights, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_salience_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
}

class HypoSalienceFepBridgeTest : public ::testing::Test {
protected:
    hypo_salience_fep_bridge_t* bridge = nullptr;
    fep_system_t* fep_system = nullptr;

    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_system = fep_create(&fep_config, 8, 4);

        /* Create bridge with defaults */
        bridge = hypo_salience_fep_create(nullptr, nullptr, fep_system);
    }

    void TearDown() override {
        if (bridge) {
            hypo_salience_fep_destroy(bridge);
            bridge = nullptr;
        }
        if (fep_system) {
            fep_destroy(fep_system);
            fep_system = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(HypoSalienceFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(HypoSalienceFepBridgeTest, CreateWithNullConfig) {
    hypo_salience_fep_bridge_t* br = hypo_salience_fep_create(nullptr, nullptr, fep_system);
    ASSERT_NE(br, nullptr);
    hypo_salience_fep_destroy(br);
}

TEST_F(HypoSalienceFepBridgeTest, CreateWithConfig) {
    hypo_salience_fep_config_t config;
    hypo_salience_fep_default_config(&config);
    config.drive_fe_weight = 0.9f;
    config.softmax_temperature = 2.0f;
    config.enable_winner_take_all = true;

    hypo_salience_fep_bridge_t* br = hypo_salience_fep_create(&config, nullptr, fep_system);
    ASSERT_NE(br, nullptr);
    EXPECT_FLOAT_EQ(br->config.drive_fe_weight, 0.9f);
    EXPECT_FLOAT_EQ(br->config.softmax_temperature, 2.0f);
    hypo_salience_fep_destroy(br);
}

TEST_F(HypoSalienceFepBridgeTest, DestroyNull) {
    hypo_salience_fep_destroy(nullptr);
    /* Should not crash */
}

TEST_F(HypoSalienceFepBridgeTest, DefaultConfig) {
    hypo_salience_fep_config_t config;
    int ret = hypo_salience_fep_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.drive_fe_weight, 0.0f);
    EXPECT_GT(config.prediction_error_gain, 0.0f);
    EXPECT_GT(config.softmax_temperature, 0.0f);
    EXPECT_GT(config.urgency_to_salience_scale, 0.0f);
}

TEST_F(HypoSalienceFepBridgeTest, DefaultConfigNullPtr) {
    int ret = hypo_salience_fep_default_config(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Reset Tests
 * ============================================================================ */

TEST_F(HypoSalienceFepBridgeTest, Reset) {
    int ret = hypo_salience_fep_reset(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, ResetNull) {
    int ret = hypo_salience_fep_reset(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(HypoSalienceFepBridgeTest, Update) {
    int ret = hypo_salience_fep_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, UpdateNull) {
    int ret = hypo_salience_fep_update(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, UpdateMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        int ret = hypo_salience_fep_update(bridge);
        EXPECT_EQ(ret, 0);
    }
    EXPECT_GE(bridge->state.update_count, 10u);
}

/* ============================================================================
 * FEP Computation Tests
 * ============================================================================ */

TEST_F(HypoSalienceFepBridgeTest, ComputeFe) {
    int ret = hypo_salience_fep_compute_fe(bridge, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, ComputeFeNullBridge) {
    int ret = hypo_salience_fep_compute_fe(nullptr, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, ComputeFeUpdatesEffects) {
    int ret = hypo_salience_fep_compute_fe(bridge, nullptr);
    EXPECT_EQ(ret, 0);

    /* Check that effects are populated */
    EXPECT_GE(bridge->fep_effects.free_energy, 0.0f);
}

/* ============================================================================
 * Precision Modulation Tests
 * ============================================================================ */

TEST_F(HypoSalienceFepBridgeTest, ModulatePrecision) {
    int ret = hypo_salience_fep_modulate_precision(bridge, 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, ModulatePrecisionNull) {
    int ret = hypo_salience_fep_modulate_precision(nullptr, 0.5f);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, ModulatePrecisionLowFatigue) {
    int ret = hypo_salience_fep_modulate_precision(bridge, 0.1f);
    EXPECT_EQ(ret, 0);
    /* Low fatigue should result in higher precision */
    EXPECT_GE(bridge->state.current_precision, HYPO_SALIENCE_FEP_MIN_PRECISION);
}

TEST_F(HypoSalienceFepBridgeTest, ModulatePrecisionHighFatigue) {
    int ret = hypo_salience_fep_modulate_precision(bridge, 0.9f);
    EXPECT_EQ(ret, 0);
    /* High fatigue should reduce precision but stay above minimum */
    EXPECT_GE(bridge->state.current_precision, HYPO_SALIENCE_FEP_MIN_PRECISION);
    EXPECT_LE(bridge->state.current_precision, HYPO_SALIENCE_FEP_MAX_PRECISION);
}

/* ============================================================================
 * Salience Weight Tests
 * ============================================================================ */

TEST_F(HypoSalienceFepBridgeTest, GetWeight) {
    float weight = hypo_salience_fep_get_weight(bridge, HYPO_DRIVE_HUNGER);
    /* Weight should be valid or -1 on error */
    if (weight >= 0.0f) {
        EXPECT_GE(weight, HYPO_SALIENCE_FEP_MIN_WEIGHT);
        EXPECT_LE(weight, HYPO_SALIENCE_FEP_MAX_WEIGHT);
    }
}

TEST_F(HypoSalienceFepBridgeTest, GetWeightNull) {
    float weight = hypo_salience_fep_get_weight(nullptr, HYPO_DRIVE_HUNGER);
    EXPECT_LT(weight, 0.0f);
}

TEST_F(HypoSalienceFepBridgeTest, GetWeights) {
    float weights[HYPO_DRIVE_COUNT];
    memset(weights, 0, sizeof(weights));

    int ret = hypo_salience_fep_get_weights(bridge, weights);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, GetWeightsNullBridge) {
    float weights[HYPO_DRIVE_COUNT];
    int ret = hypo_salience_fep_get_weights(nullptr, weights);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, GetWeightsNullOutput) {
    int ret = hypo_salience_fep_get_weights(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Conflict Detection Tests
 * ============================================================================ */

TEST_F(HypoSalienceFepBridgeTest, DetectConflict) {
    hypo_salience_conflict_t conflict;
    float intensity;

    int ret = hypo_salience_fep_detect_conflict(bridge, &conflict, &intensity);
    EXPECT_EQ(ret, 0);

    /* Conflict level should be valid */
    EXPECT_GE(conflict, HYPO_SALIENCE_CONFLICT_NONE);
    EXPECT_LE(conflict, HYPO_SALIENCE_CONFLICT_SEVERE);

    /* Intensity should be in range */
    EXPECT_GE(intensity, 0.0f);
    EXPECT_LE(intensity, 1.0f);
}

TEST_F(HypoSalienceFepBridgeTest, DetectConflictNullBridge) {
    hypo_salience_conflict_t conflict;
    float intensity;
    int ret = hypo_salience_fep_detect_conflict(nullptr, &conflict, &intensity);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, DetectConflictNullOutputs) {
    int ret = hypo_salience_fep_detect_conflict(bridge, nullptr, nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Effects Retrieval Tests
 * ============================================================================ */

TEST_F(HypoSalienceFepBridgeTest, GetEffects) {
    hypo_salience_fep_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int ret = hypo_salience_fep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, GetEffectsNullBridge) {
    hypo_salience_fep_effects_t effects;
    int ret = hypo_salience_fep_get_effects(nullptr, &effects);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, GetEffectsNullOutput) {
    int ret = hypo_salience_fep_get_effects(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, EffectsContainSalienceData) {
    hypo_salience_fep_update(bridge);

    hypo_salience_fep_effects_t effects;
    hypo_salience_fep_get_effects(bridge, &effects);

    /* Check salience-specific fields */
    EXPECT_GE(effects.total_salience, 0.0f);
    EXPECT_GE(effects.attention_capacity, 0.0f);
    EXPECT_LE(effects.attention_capacity, 1.0f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(HypoSalienceFepBridgeTest, GetStats) {
    hypo_salience_fep_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int ret = hypo_salience_fep_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, GetStatsNullBridge) {
    hypo_salience_fep_stats_t stats;
    int ret = hypo_salience_fep_get_stats(nullptr, &stats);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, GetStatsNullOutput) {
    int ret = hypo_salience_fep_get_stats(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, StatsAccumulate) {
    /* Perform some updates */
    for (int i = 0; i < 5; i++) {
        hypo_salience_fep_update(bridge);
    }

    hypo_salience_fep_stats_t stats;
    hypo_salience_fep_get_stats(bridge, &stats);

    EXPECT_GE(stats.total_updates, 5u);
}

/* ============================================================================
 * Bio-Async Connection Tests
 * ============================================================================ */

TEST_F(HypoSalienceFepBridgeTest, ConnectBioAsync) {
    int ret = hypo_salience_fep_connect_bio_async(bridge, nullptr);
    /* May return 0 or -1 depending on router availability */
    (void)ret;
}

TEST_F(HypoSalienceFepBridgeTest, ConnectBioAsyncNull) {
    /* NULL is a graceful no-op, returns 0 */
    int ret = hypo_salience_fep_connect_bio_async(nullptr, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, DisconnectBioAsync) {
    int ret = hypo_salience_fep_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, DisconnectBioAsyncNull) {
    /* NULL is a graceful no-op, returns 0 */
    int ret = hypo_salience_fep_disconnect_bio_async(nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoSalienceFepBridgeTest, ProcessMessages) {
    int processed = hypo_salience_fep_process_messages(bridge, 0);
    EXPECT_GE(processed, 0);
}

TEST_F(HypoSalienceFepBridgeTest, ProcessMessagesNull) {
    /* NULL is a graceful no-op, returns 0 */
    int processed = hypo_salience_fep_process_messages(nullptr, 0);
    EXPECT_EQ(processed, 0);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(HypoSalienceFepBridgeTest, LevelName) {
    const char* name = hypo_salience_fep_level_name(HYPO_SALIENCE_FEP_LEVEL_LOW);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = hypo_salience_fep_level_name(HYPO_SALIENCE_FEP_LEVEL_CRITICAL);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(HypoSalienceFepBridgeTest, ResponseName) {
    const char* name = hypo_salience_fep_response_name(HYPO_SALIENCE_FEP_RESPONSE_MAINTAIN);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = hypo_salience_fep_response_name(HYPO_SALIENCE_FEP_RESPONSE_EMERGENCY);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(HypoSalienceFepBridgeTest, ConflictName) {
    const char* name = hypo_salience_fep_conflict_name(HYPO_SALIENCE_CONFLICT_NONE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = hypo_salience_fep_conflict_name(HYPO_SALIENCE_CONFLICT_SEVERE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(HypoSalienceFepBridgeTest, PrintSummary) {
    hypo_salience_fep_print_summary(bridge);
    /* Should not crash */
}

TEST_F(HypoSalienceFepBridgeTest, PrintSummaryNull) {
    hypo_salience_fep_print_summary(nullptr);
    /* Should not crash */
}

/* ============================================================================
 * Urgency Level Tests
 * ============================================================================ */

TEST_F(HypoSalienceFepBridgeTest, UrgencyLevelTracking) {
    /* Update and check urgency level is tracked */
    hypo_salience_fep_update(bridge);

    hypo_salience_fep_effects_t effects;
    hypo_salience_fep_get_effects(bridge, &effects);

    /* Urgency level should be valid enum value */
    EXPECT_GE(effects.urgency_level, HYPO_SALIENCE_FEP_LEVEL_LOW);
    EXPECT_LE(effects.urgency_level, HYPO_SALIENCE_FEP_LEVEL_CRITICAL);
}

/* ============================================================================
 * Dominant Drive Tests
 * ============================================================================ */

TEST_F(HypoSalienceFepBridgeTest, DominantDriveTracking) {
    hypo_salience_fep_update(bridge);

    hypo_salience_fep_effects_t effects;
    hypo_salience_fep_get_effects(bridge, &effects);

    /* Dominant drive should be a valid drive type */
    EXPECT_GE(effects.dominant_drive, 0);
    EXPECT_LT(effects.dominant_drive, HYPO_DRIVE_COUNT);

    /* Dominant salience should be non-negative */
    EXPECT_GE(effects.dominant_salience, 0.0f);
}

/* ============================================================================
 * Integration Test
 * ============================================================================ */

TEST_F(HypoSalienceFepBridgeTest, FullWorkflow) {
    /* Reset bridge */
    EXPECT_EQ(hypo_salience_fep_reset(bridge), 0);

    /* Modulate precision */
    EXPECT_EQ(hypo_salience_fep_modulate_precision(bridge, 0.3f), 0);

    /* Compute FE */
    EXPECT_EQ(hypo_salience_fep_compute_fe(bridge, nullptr), 0);

    /* Update bridge multiple times */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(hypo_salience_fep_update(bridge), 0);
    }

    /* Detect conflict */
    hypo_salience_conflict_t conflict;
    float intensity;
    EXPECT_EQ(hypo_salience_fep_detect_conflict(bridge, &conflict, &intensity), 0);

    /* Get salience weights */
    float weights[HYPO_DRIVE_COUNT];
    EXPECT_EQ(hypo_salience_fep_get_weights(bridge, weights), 0);

    /* Get effects */
    hypo_salience_fep_effects_t effects;
    EXPECT_EQ(hypo_salience_fep_get_effects(bridge, &effects), 0);

    /* Get stats */
    hypo_salience_fep_stats_t stats;
    EXPECT_EQ(hypo_salience_fep_get_stats(bridge, &stats), 0);

    /* Verify accumulated state */
    EXPECT_GE(stats.total_updates, 5u);
}
