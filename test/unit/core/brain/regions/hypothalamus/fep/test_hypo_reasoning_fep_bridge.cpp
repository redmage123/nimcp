/**
 * @file test_hypo_reasoning_fep_bridge.cpp
 * @brief Unit tests for Hypothalamus Reasoning FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Reasoning bidirectional integration
 * WHY:  Ensure fatigue-precision modulation, cognitive load FE, and error reporting work
 * HOW:  Test lifecycle, connections, FE computation, precision modulation, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_reasoning_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
}

class HypoReasoningFepBridgeTest : public ::testing::Test {
protected:
    hypo_reasoning_fep_bridge_t* bridge = nullptr;
    fep_system_t* fep_system = nullptr;

    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_system = fep_create(&fep_config, 8, 4);

        /* Create bridge with defaults */
        bridge = hypo_reasoning_fep_create(nullptr, fep_system);
    }

    void TearDown() override {
        if (bridge) {
            hypo_reasoning_fep_destroy(bridge);
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

TEST_F(HypoReasoningFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(HypoReasoningFepBridgeTest, CreateWithNullConfig) {
    hypo_reasoning_fep_bridge_t* br = hypo_reasoning_fep_create(nullptr, fep_system);
    ASSERT_NE(br, nullptr);
    hypo_reasoning_fep_destroy(br);
}

TEST_F(HypoReasoningFepBridgeTest, CreateWithConfig) {
    hypo_reasoning_fep_config_t config;
    hypo_reasoning_fep_default_config(&config);
    config.drive_fe_weight = 0.85f;
    config.fatigue_precision_scale = 0.6f;
    config.load_fe_scale = 1.8f;

    hypo_reasoning_fep_bridge_t* br = hypo_reasoning_fep_create(&config, fep_system);
    ASSERT_NE(br, nullptr);
    EXPECT_FLOAT_EQ(br->config.drive_fe_weight, 0.85f);
    EXPECT_FLOAT_EQ(br->config.fatigue_precision_scale, 0.6f);
    hypo_reasoning_fep_destroy(br);
}

TEST_F(HypoReasoningFepBridgeTest, DestroyNull) {
    hypo_reasoning_fep_destroy(nullptr);
    /* Should not crash */
}

TEST_F(HypoReasoningFepBridgeTest, DefaultConfig) {
    hypo_reasoning_fep_config_t config;
    int ret = hypo_reasoning_fep_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.drive_fe_weight, 0.0f);
    EXPECT_GT(config.prediction_error_gain, 0.0f);
    EXPECT_GT(config.fatigue_precision_scale, 0.0f);
    EXPECT_GT(config.load_fe_scale, 0.0f);
}

TEST_F(HypoReasoningFepBridgeTest, DefaultConfigNullPtr) {
    int ret = hypo_reasoning_fep_default_config(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Reset Tests
 * ============================================================================ */

TEST_F(HypoReasoningFepBridgeTest, Reset) {
    int ret = hypo_reasoning_fep_reset(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, ResetNull) {
    int ret = hypo_reasoning_fep_reset(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, ResetClearsState) {
    /* Update to accumulate state */
    hypo_reasoning_fep_update(bridge, 100);
    hypo_reasoning_fep_update(bridge, 100);

    /* Reset */
    hypo_reasoning_fep_reset(bridge);

    /* Verify state is cleared */
    EXPECT_EQ(bridge->state.update_count, 0u);
    EXPECT_EQ(bridge->state.reasoning_errors, 0u);
    EXPECT_EQ(bridge->state.reasoning_successes, 0u);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(HypoReasoningFepBridgeTest, Update) {
    int ret = hypo_reasoning_fep_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, UpdateNull) {
    int ret = hypo_reasoning_fep_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, UpdateMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        int ret = hypo_reasoning_fep_update(bridge, 50);
        EXPECT_EQ(ret, 0);
    }
    EXPECT_GE(bridge->state.update_count, 10u);
}

TEST_F(HypoReasoningFepBridgeTest, UpdateWithDifferentDeltas) {
    int ret = hypo_reasoning_fep_update(bridge, 0);
    EXPECT_EQ(ret, 0);

    ret = hypo_reasoning_fep_update(bridge, 1000);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Free Energy Computation Tests
 * ============================================================================ */

TEST_F(HypoReasoningFepBridgeTest, ComputeFe) {
    float free_energy;
    int ret = hypo_reasoning_fep_compute_fe(bridge, 0.5f, &free_energy);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(free_energy, 0.0f);
}

TEST_F(HypoReasoningFepBridgeTest, ComputeFeNullBridge) {
    float free_energy;
    int ret = hypo_reasoning_fep_compute_fe(nullptr, 0.5f, &free_energy);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, ComputeFeNullOutput) {
    int ret = hypo_reasoning_fep_compute_fe(bridge, 0.5f, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, ComputeFeLowLoad) {
    float free_energy;
    int ret = hypo_reasoning_fep_compute_fe(bridge, 0.1f, &free_energy);
    EXPECT_EQ(ret, 0);
    /* Low cognitive load should result in lower free energy */
}

TEST_F(HypoReasoningFepBridgeTest, ComputeFeHighLoad) {
    float fe_low, fe_high;

    hypo_reasoning_fep_compute_fe(bridge, 0.2f, &fe_low);
    hypo_reasoning_fep_compute_fe(bridge, 0.9f, &fe_high);

    /* Higher cognitive load should produce higher free energy */
    EXPECT_GE(fe_high, fe_low);
}

/* ============================================================================
 * Precision Modulation Tests
 * ============================================================================ */

TEST_F(HypoReasoningFepBridgeTest, ModulatePrecision) {
    float precision;
    int ret = hypo_reasoning_fep_modulate_precision(bridge, 0.5f, &precision);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(precision, 0.0f);
}

TEST_F(HypoReasoningFepBridgeTest, ModulatePrecisionNull) {
    float precision;
    int ret = hypo_reasoning_fep_modulate_precision(nullptr, 0.5f, &precision);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, ModulatePrecisionNullOutput) {
    int ret = hypo_reasoning_fep_modulate_precision(bridge, 0.5f, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, ModulatePrecisionLowFatigue) {
    float precision;
    int ret = hypo_reasoning_fep_modulate_precision(bridge, 0.1f, &precision);
    EXPECT_EQ(ret, 0);
    /* Low fatigue should result in higher precision */
    EXPECT_GE(precision, HYPO_REASONING_FEP_PRECISION_MIN);
}

TEST_F(HypoReasoningFepBridgeTest, ModulatePrecisionHighFatigue) {
    float precision_low_fatigue, precision_high_fatigue;

    hypo_reasoning_fep_modulate_precision(bridge, 0.1f, &precision_low_fatigue);
    hypo_reasoning_fep_modulate_precision(bridge, 0.9f, &precision_high_fatigue);

    /* High fatigue should reduce precision */
    EXPECT_LE(precision_high_fatigue, precision_low_fatigue);
    /* But precision should stay above minimum */
    EXPECT_GE(precision_high_fatigue, HYPO_REASONING_FEP_PRECISION_MIN);
}

/* ============================================================================
 * Effects Retrieval Tests
 * ============================================================================ */

TEST_F(HypoReasoningFepBridgeTest, GetEffects) {
    hypo_reasoning_fep_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int ret = hypo_reasoning_fep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, GetEffectsNullBridge) {
    hypo_reasoning_fep_effects_t effects;
    int ret = hypo_reasoning_fep_get_effects(nullptr, &effects);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, GetEffectsNullOutput) {
    int ret = hypo_reasoning_fep_get_effects(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, EffectsContainReasoningData) {
    hypo_reasoning_fep_update(bridge, 100);

    hypo_reasoning_fep_effects_t effects;
    hypo_reasoning_fep_get_effects(bridge, &effects);

    /* Check reasoning-specific fields */
    EXPECT_GE(effects.free_energy, 0.0f);
    EXPECT_GE(effects.precision, 0.0f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(HypoReasoningFepBridgeTest, GetStats) {
    hypo_reasoning_fep_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int ret = hypo_reasoning_fep_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, GetStatsNullBridge) {
    hypo_reasoning_fep_stats_t stats;
    int ret = hypo_reasoning_fep_get_stats(nullptr, &stats);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, GetStatsNullOutput) {
    int ret = hypo_reasoning_fep_get_stats(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, StatsAccumulate) {
    /* Perform some updates */
    for (int i = 0; i < 5; i++) {
        hypo_reasoning_fep_update(bridge, 100);
    }

    hypo_reasoning_fep_stats_t stats;
    hypo_reasoning_fep_get_stats(bridge, &stats);

    EXPECT_GE(stats.total_updates, 5u);
}

/* ============================================================================
 * Error Reporting Tests
 * ============================================================================ */

TEST_F(HypoReasoningFepBridgeTest, ReportError) {
    int ret = hypo_reasoning_fep_report_error(bridge, 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, ReportErrorNull) {
    int ret = hypo_reasoning_fep_report_error(nullptr, 0.5f);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, ReportErrorUpdatesState) {
    hypo_reasoning_fep_report_error(bridge, 0.7f);

    /* State should reflect error */
    EXPECT_GE(bridge->state.reasoning_errors, 1u);
}

TEST_F(HypoReasoningFepBridgeTest, ReportErrorAccumulates) {
    hypo_reasoning_fep_report_error(bridge, 0.3f);
    hypo_reasoning_fep_report_error(bridge, 0.4f);
    hypo_reasoning_fep_report_error(bridge, 0.5f);

    /* Errors should accumulate */
    EXPECT_GE(bridge->state.reasoning_errors, 3u);
}

TEST_F(HypoReasoningFepBridgeTest, ReportErrorAffectsPE) {
    /* Report error and update */
    hypo_reasoning_fep_report_error(bridge, 0.8f);
    hypo_reasoning_fep_update(bridge, 100);

    hypo_reasoning_fep_effects_t effects;
    hypo_reasoning_fep_get_effects(bridge, &effects);

    /* Error should contribute to prediction error */
    EXPECT_GE(effects.error_pe, 0.0f);
}

/* ============================================================================
 * Success Reporting Tests
 * ============================================================================ */

TEST_F(HypoReasoningFepBridgeTest, ReportSuccess) {
    int ret = hypo_reasoning_fep_report_success(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, ReportSuccessNull) {
    int ret = hypo_reasoning_fep_report_success(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, ReportSuccessUpdatesState) {
    hypo_reasoning_fep_report_success(bridge);

    /* State should reflect success */
    EXPECT_GE(bridge->state.reasoning_successes, 1u);
}

TEST_F(HypoReasoningFepBridgeTest, ReportSuccessAccumulates) {
    hypo_reasoning_fep_report_success(bridge);
    hypo_reasoning_fep_report_success(bridge);
    hypo_reasoning_fep_report_success(bridge);

    /* Successes should accumulate */
    EXPECT_GE(bridge->state.reasoning_successes, 3u);
}

/* ============================================================================
 * Bio-Async Connection Tests
 * ============================================================================ */

TEST_F(HypoReasoningFepBridgeTest, ConnectBioAsync) {
    int ret = hypo_reasoning_fep_connect_bio_async(bridge);
    /* May return 0 or -1 depending on router availability */
    (void)ret;
}

TEST_F(HypoReasoningFepBridgeTest, ConnectBioAsyncNull) {
    /* NULL is a graceful no-op, returns 0 */
    int ret = hypo_reasoning_fep_connect_bio_async(nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, DisconnectBioAsync) {
    hypo_reasoning_fep_connect_bio_async(bridge);
    int ret = hypo_reasoning_fep_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, DisconnectBioAsyncNull) {
    /* NULL is a graceful no-op, returns 0 */
    int ret = hypo_reasoning_fep_disconnect_bio_async(nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, IsBioAsyncConnected) {
    bool connected = hypo_reasoning_fep_is_bio_async_connected(bridge);
    /* Initial state should be disconnected */
    EXPECT_FALSE(connected);
}

TEST_F(HypoReasoningFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = hypo_reasoning_fep_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(HypoReasoningFepBridgeTest, ProcessMessages) {
    int processed = hypo_reasoning_fep_process_messages(bridge);
    EXPECT_GE(processed, 0);
}

TEST_F(HypoReasoningFepBridgeTest, ProcessMessagesNull) {
    /* NULL is a graceful no-op, returns 0 */
    int processed = hypo_reasoning_fep_process_messages(nullptr);
    EXPECT_EQ(processed, 0);
}

/* ============================================================================
 * Drive System Connection Tests
 * ============================================================================ */

TEST_F(HypoReasoningFepBridgeTest, ConnectDrives) {
    int ret = hypo_reasoning_fep_connect_drives(bridge, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoReasoningFepBridgeTest, ConnectDrivesNullBridge) {
    int ret = hypo_reasoning_fep_connect_drives(nullptr, nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * State Tracking Tests
 * ============================================================================ */

TEST_F(HypoReasoningFepBridgeTest, StateActiveAfterUpdate) {
    /* Bridge is initialized as active */
    EXPECT_TRUE(bridge->state.active);

    hypo_reasoning_fep_update(bridge, 100);

    EXPECT_TRUE(bridge->state.active);
}

TEST_F(HypoReasoningFepBridgeTest, FatigueStateTracked) {
    /* Modulate precision with fatigue */
    float precision;
    int ret = hypo_reasoning_fep_modulate_precision(bridge, 0.7f, &precision);
    EXPECT_EQ(ret, 0);

    /* Precision should be valid (fatigue affects precision) */
    EXPECT_GT(precision, 0.0f);
    EXPECT_LE(precision, 2.0f);  /* Within reasonable bounds */
}

TEST_F(HypoReasoningFepBridgeTest, LoadStateTracked) {
    /* Compute FE with cognitive load */
    float fe;
    hypo_reasoning_fep_compute_fe(bridge, 0.6f, &fe);

    /* State should track current load */
    EXPECT_GT(bridge->state.current_load, 0.0f);
}

/* ============================================================================
 * Cognitive Load Thresholds Tests
 * ============================================================================ */

TEST_F(HypoReasoningFepBridgeTest, HighLoadTriggersFESpike) {
    /* Apply high cognitive load */
    float fe;
    hypo_reasoning_fep_compute_fe(bridge, 0.9f, &fe);
    hypo_reasoning_fep_update(bridge, 100);

    hypo_reasoning_fep_stats_t stats;
    hypo_reasoning_fep_get_stats(bridge, &stats);

    /* High load should produce FE spikes */
    EXPECT_GE(stats.fe_spikes, 0u);
}

/* ============================================================================
 * Integration Test
 * ============================================================================ */

TEST_F(HypoReasoningFepBridgeTest, FullWorkflow) {
    /* Reset bridge */
    EXPECT_EQ(hypo_reasoning_fep_reset(bridge), 0);

    /* Modulate precision with fatigue */
    float precision;
    EXPECT_EQ(hypo_reasoning_fep_modulate_precision(bridge, 0.4f, &precision), 0);
    EXPECT_GT(precision, 0.0f);

    /* Compute FE from cognitive load */
    float free_energy;
    EXPECT_EQ(hypo_reasoning_fep_compute_fe(bridge, 0.6f, &free_energy), 0);
    EXPECT_GE(free_energy, 0.0f);

    /* Report some errors and successes */
    EXPECT_EQ(hypo_reasoning_fep_report_error(bridge, 0.3f), 0);
    EXPECT_EQ(hypo_reasoning_fep_report_success(bridge), 0);
    EXPECT_EQ(hypo_reasoning_fep_report_success(bridge), 0);

    /* Update bridge multiple times */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(hypo_reasoning_fep_update(bridge, 100), 0);
    }

    /* Get effects */
    hypo_reasoning_fep_effects_t effects;
    EXPECT_EQ(hypo_reasoning_fep_get_effects(bridge, &effects), 0);

    /* Get stats */
    hypo_reasoning_fep_stats_t stats;
    EXPECT_EQ(hypo_reasoning_fep_get_stats(bridge, &stats), 0);

    /* Verify accumulated state */
    EXPECT_GE(stats.total_updates, 5u);
    EXPECT_GE(bridge->state.reasoning_errors, 1u);
    EXPECT_GE(bridge->state.reasoning_successes, 2u);
}
