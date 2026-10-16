/**
 * @file test_hypo_bio_async_fep_bridge.cpp
 * @brief Unit tests for Hypothalamus Bio-Async FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Bio-Async bidirectional integration
 * WHY:  Ensure timing anomaly detection, precision modulation, and messaging work correctly
 * HOW:  Test lifecycle, connections, FE computation, precision modulation, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_bio_async_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
}

class HypoBioAsyncFepBridgeTest : public ::testing::Test {
protected:
    hypo_bio_async_fep_bridge_t* bridge = nullptr;
    fep_system_t* fep_system = nullptr;

    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_system = fep_create(&fep_config, 8, 4);

        /* Create bridge with defaults */
        bridge = hypo_bio_async_fep_create(nullptr, nullptr, fep_system);
    }

    void TearDown() override {
        if (bridge) {
            hypo_bio_async_fep_destroy(bridge);
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

TEST_F(HypoBioAsyncFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(HypoBioAsyncFepBridgeTest, CreateWithNullConfig) {
    hypo_bio_async_fep_bridge_t* br = hypo_bio_async_fep_create(nullptr, nullptr, fep_system);
    ASSERT_NE(br, nullptr);
    hypo_bio_async_fep_destroy(br);
}

TEST_F(HypoBioAsyncFepBridgeTest, CreateWithConfig) {
    hypo_bio_async_fep_config_t config;
    hypo_bio_async_fep_default_config(&config);
    config.drive_fe_weight = 0.8f;
    config.enable_active_inference = true;

    hypo_bio_async_fep_bridge_t* br = hypo_bio_async_fep_create(&config, nullptr, fep_system);
    ASSERT_NE(br, nullptr);
    EXPECT_FLOAT_EQ(br->config.drive_fe_weight, 0.8f);
    hypo_bio_async_fep_destroy(br);
}

TEST_F(HypoBioAsyncFepBridgeTest, DestroyNull) {
    hypo_bio_async_fep_destroy(nullptr);
    /* Should not crash */
}

TEST_F(HypoBioAsyncFepBridgeTest, DefaultConfig) {
    hypo_bio_async_fep_config_t config;
    int ret = hypo_bio_async_fep_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.drive_fe_weight, 0.0f);
    EXPECT_GT(config.prediction_error_gain, 0.0f);
    EXPECT_GT(config.precision_modulation, 0.0f);
    EXPECT_GT(config.free_energy_threshold, 0.0f);
}

TEST_F(HypoBioAsyncFepBridgeTest, DefaultConfigNullPtr) {
    int ret = hypo_bio_async_fep_default_config(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Reset Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncFepBridgeTest, Reset) {
    int ret = hypo_bio_async_fep_reset(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, ResetNull) {
    int ret = hypo_bio_async_fep_reset(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncFepBridgeTest, Update) {
    int ret = hypo_bio_async_fep_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, UpdateNull) {
    int ret = hypo_bio_async_fep_update(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, UpdateMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        int ret = hypo_bio_async_fep_update(bridge);
        EXPECT_EQ(ret, 0);
    }
    EXPECT_GE(bridge->state.update_count, 10u);
}

/* ============================================================================
 * FEP Computation Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncFepBridgeTest, ComputeFe) {
    int ret = hypo_bio_async_fep_compute_fe(bridge, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, ComputeFeNullBridge) {
    int ret = hypo_bio_async_fep_compute_fe(nullptr, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, ComputeFeUpdatesEffects) {
    int ret = hypo_bio_async_fep_compute_fe(bridge, nullptr);
    EXPECT_EQ(ret, 0);

    /* Check that effects are populated */
    EXPECT_GE(bridge->fep_effects.free_energy, 0.0f);
}

/* ============================================================================
 * Precision Modulation Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncFepBridgeTest, ModulatePrecision) {
    int ret = hypo_bio_async_fep_modulate_precision(bridge, 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, ModulatePrecisionNull) {
    int ret = hypo_bio_async_fep_modulate_precision(nullptr, 0.5f);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, ModulatePrecisionLowFatigue) {
    int ret = hypo_bio_async_fep_modulate_precision(bridge, 0.1f);
    EXPECT_EQ(ret, 0);
    /* Low fatigue should result in higher precision */
    EXPECT_GE(bridge->state.current_precision, HYPO_BIO_ASYNC_FEP_MIN_PRECISION);
}

TEST_F(HypoBioAsyncFepBridgeTest, ModulatePrecisionHighFatigue) {
    int ret = hypo_bio_async_fep_modulate_precision(bridge, 0.9f);
    EXPECT_EQ(ret, 0);
    /* High fatigue should reduce precision but stay above minimum */
    EXPECT_GE(bridge->state.current_precision, HYPO_BIO_ASYNC_FEP_MIN_PRECISION);
    EXPECT_LE(bridge->state.current_precision, HYPO_BIO_ASYNC_FEP_MAX_PRECISION);
}

TEST_F(HypoBioAsyncFepBridgeTest, ModulatePrecisionBounds) {
    /* Test with values outside [0,1] - should clamp */
    int ret = hypo_bio_async_fep_modulate_precision(bridge, -0.5f);
    EXPECT_EQ(ret, 0);

    ret = hypo_bio_async_fep_modulate_precision(bridge, 1.5f);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Effects Retrieval Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncFepBridgeTest, GetEffects) {
    hypo_bio_async_fep_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int ret = hypo_bio_async_fep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, GetEffectsNullBridge) {
    hypo_bio_async_fep_effects_t effects;
    int ret = hypo_bio_async_fep_get_effects(nullptr, &effects);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, GetEffectsNullOutput) {
    int ret = hypo_bio_async_fep_get_effects(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncFepBridgeTest, GetStats) {
    hypo_bio_async_fep_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int ret = hypo_bio_async_fep_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, GetStatsNullBridge) {
    hypo_bio_async_fep_stats_t stats;
    int ret = hypo_bio_async_fep_get_stats(nullptr, &stats);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, GetStatsNullOutput) {
    int ret = hypo_bio_async_fep_get_stats(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, StatsAccumulate) {
    /* Perform some updates */
    for (int i = 0; i < 5; i++) {
        hypo_bio_async_fep_update(bridge);
    }

    hypo_bio_async_fep_stats_t stats;
    hypo_bio_async_fep_get_stats(bridge, &stats);

    EXPECT_GE(stats.total_updates, 5u);
}

/* ============================================================================
 * Bio-Async Connection Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncFepBridgeTest, ConnectBioAsync) {
    int ret = hypo_bio_async_fep_connect_bio_async(bridge, nullptr);
    /* May return 0 or -1 depending on router availability */
    (void)ret;
}

TEST_F(HypoBioAsyncFepBridgeTest, ConnectBioAsyncNull) {
    /* NULL is a graceful no-op, returns 0 */
    int ret = hypo_bio_async_fep_connect_bio_async(nullptr, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, DisconnectBioAsync) {
    int ret = hypo_bio_async_fep_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, DisconnectBioAsyncNull) {
    /* NULL is a graceful no-op, returns 0 */
    int ret = hypo_bio_async_fep_disconnect_bio_async(nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, ProcessMessages) {
    int processed = hypo_bio_async_fep_process_messages(bridge, 0);
    EXPECT_GE(processed, 0);
}

TEST_F(HypoBioAsyncFepBridgeTest, ProcessMessagesNull) {
    /* NULL is a graceful no-op, returns 0 */
    int processed = hypo_bio_async_fep_process_messages(nullptr, 0);
    EXPECT_EQ(processed, 0);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncFepBridgeTest, LevelName) {
    const char* name = hypo_bio_async_fep_level_name(HYPO_BIO_ASYNC_FEP_LEVEL_NORMAL);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = hypo_bio_async_fep_level_name(HYPO_BIO_ASYNC_FEP_LEVEL_CRITICAL);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(HypoBioAsyncFepBridgeTest, ResponseName) {
    const char* name = hypo_bio_async_fep_response_name(HYPO_BIO_ASYNC_FEP_RESPONSE_NONE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = hypo_bio_async_fep_response_name(HYPO_BIO_ASYNC_FEP_RESPONSE_EMERGENCY);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(HypoBioAsyncFepBridgeTest, AnomalyName) {
    const char* name = hypo_bio_async_fep_anomaly_name(HYPO_BIO_ASYNC_ANOMALY_NONE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = hypo_bio_async_fep_anomaly_name(HYPO_BIO_ASYNC_ANOMALY_TIMING);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(HypoBioAsyncFepBridgeTest, PrintSummary) {
    hypo_bio_async_fep_print_summary(bridge);
    /* Should not crash */
}

TEST_F(HypoBioAsyncFepBridgeTest, PrintSummaryNull) {
    hypo_bio_async_fep_print_summary(nullptr);
    /* Should not crash */
}

/* ============================================================================
 * Disruption Level Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncFepBridgeTest, DisruptionLevelTracking) {
    /* Update and check disruption level is tracked */
    hypo_bio_async_fep_update(bridge);

    hypo_bio_async_fep_effects_t effects;
    hypo_bio_async_fep_get_effects(bridge, &effects);

    /* Disruption level should be valid enum value */
    EXPECT_GE(effects.disruption_level, HYPO_BIO_ASYNC_FEP_LEVEL_NORMAL);
    EXPECT_LE(effects.disruption_level, HYPO_BIO_ASYNC_FEP_LEVEL_CRITICAL);
}

/* ============================================================================
 * Temporal Pattern Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncFepBridgeTest, TemporalPatternInitialized) {
    /* Check that temporal pattern tracking is initialized */
    EXPECT_EQ(bridge->state.temporal.rate_history_idx, 0u);
}

/* ============================================================================
 * Integration Test
 * ============================================================================ */

TEST_F(HypoBioAsyncFepBridgeTest, FullWorkflow) {
    /* Reset bridge */
    EXPECT_EQ(hypo_bio_async_fep_reset(bridge), 0);

    /* Modulate precision */
    EXPECT_EQ(hypo_bio_async_fep_modulate_precision(bridge, 0.3f), 0);

    /* Compute FE */
    EXPECT_EQ(hypo_bio_async_fep_compute_fe(bridge, nullptr), 0);

    /* Update bridge multiple times */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(hypo_bio_async_fep_update(bridge), 0);
    }

    /* Get effects */
    hypo_bio_async_fep_effects_t effects;
    EXPECT_EQ(hypo_bio_async_fep_get_effects(bridge, &effects), 0);

    /* Get stats */
    hypo_bio_async_fep_stats_t stats;
    EXPECT_EQ(hypo_bio_async_fep_get_stats(bridge, &stats), 0);

    /* Verify accumulated state */
    EXPECT_GE(stats.total_updates, 5u);
}
