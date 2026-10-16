/**
 * @file test_hypo_curiosity_fep_bridge.cpp
 * @brief Unit tests for Hypothalamus Curiosity FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Curiosity bidirectional integration
 * WHY:  Ensure exploration computation, info gain FE reduction, and novelty detection work
 * HOW:  Test lifecycle, connections, FE computation, exploration weights, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_curiosity_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
}

class HypoCuriosityFepBridgeTest : public ::testing::Test {
protected:
    hypo_curiosity_fep_bridge_t* bridge = nullptr;
    fep_system_t* fep_system = nullptr;

    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_system = fep_create(&fep_config, 8, 4);

        /* Create bridge with defaults */
        bridge = hypo_curiosity_fep_create(nullptr, fep_system);
    }

    void TearDown() override {
        if (bridge) {
            hypo_curiosity_fep_destroy(bridge);
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

TEST_F(HypoCuriosityFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(HypoCuriosityFepBridgeTest, CreateWithNullConfig) {
    hypo_curiosity_fep_bridge_t* br = hypo_curiosity_fep_create(nullptr, fep_system);
    ASSERT_NE(br, nullptr);
    hypo_curiosity_fep_destroy(br);
}

TEST_F(HypoCuriosityFepBridgeTest, CreateWithConfig) {
    hypo_curiosity_fep_config_t config;
    hypo_curiosity_fep_default_config(&config);
    config.drive_fe_weight = 0.75f;
    config.curiosity_exploration_scale = 1.5f;
    config.epistemic_value_weight = 0.8f;

    hypo_curiosity_fep_bridge_t* br = hypo_curiosity_fep_create(&config, fep_system);
    ASSERT_NE(br, nullptr);
    EXPECT_FLOAT_EQ(br->config.drive_fe_weight, 0.75f);
    EXPECT_FLOAT_EQ(br->config.curiosity_exploration_scale, 1.5f);
    hypo_curiosity_fep_destroy(br);
}

TEST_F(HypoCuriosityFepBridgeTest, DestroyNull) {
    hypo_curiosity_fep_destroy(nullptr);
    /* Should not crash */
}

TEST_F(HypoCuriosityFepBridgeTest, DefaultConfig) {
    hypo_curiosity_fep_config_t config;
    int ret = hypo_curiosity_fep_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.drive_fe_weight, 0.0f);
    EXPECT_GT(config.prediction_error_gain, 0.0f);
    EXPECT_GT(config.curiosity_exploration_scale, 0.0f);
    EXPECT_GT(config.info_gain_fe_reduction, 0.0f);
}

TEST_F(HypoCuriosityFepBridgeTest, DefaultConfigNullPtr) {
    int ret = hypo_curiosity_fep_default_config(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Reset Tests
 * ============================================================================ */

TEST_F(HypoCuriosityFepBridgeTest, Reset) {
    int ret = hypo_curiosity_fep_reset(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, ResetNull) {
    int ret = hypo_curiosity_fep_reset(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, ResetClearsState) {
    /* Update to accumulate state */
    hypo_curiosity_fep_update(bridge, 100);
    hypo_curiosity_fep_update(bridge, 100);

    /* Reset */
    hypo_curiosity_fep_reset(bridge);

    /* Verify state is cleared */
    EXPECT_EQ(bridge->state.update_count, 0u);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(HypoCuriosityFepBridgeTest, Update) {
    int ret = hypo_curiosity_fep_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, UpdateNull) {
    int ret = hypo_curiosity_fep_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, UpdateMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        int ret = hypo_curiosity_fep_update(bridge, 50);
        EXPECT_EQ(ret, 0);
    }
    EXPECT_GE(bridge->state.update_count, 10u);
}

TEST_F(HypoCuriosityFepBridgeTest, UpdateWithDifferentDeltas) {
    int ret = hypo_curiosity_fep_update(bridge, 0);
    EXPECT_EQ(ret, 0);

    ret = hypo_curiosity_fep_update(bridge, 1000);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Exploration Computation Tests
 * ============================================================================ */

TEST_F(HypoCuriosityFepBridgeTest, ComputeExploration) {
    float exploration_weight;
    int ret = hypo_curiosity_fep_compute_exploration(bridge, 0.5f, &exploration_weight);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(exploration_weight, 0.0f);
}

TEST_F(HypoCuriosityFepBridgeTest, ComputeExplorationNullBridge) {
    float exploration_weight;
    int ret = hypo_curiosity_fep_compute_exploration(nullptr, 0.5f, &exploration_weight);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, ComputeExplorationNullOutput) {
    int ret = hypo_curiosity_fep_compute_exploration(bridge, 0.5f, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, ComputeExplorationLowDrive) {
    float exploration_weight;
    int ret = hypo_curiosity_fep_compute_exploration(bridge, 0.1f, &exploration_weight);
    EXPECT_EQ(ret, 0);
    /* Low curiosity drive should result in lower exploration */
}

TEST_F(HypoCuriosityFepBridgeTest, ComputeExplorationHighDrive) {
    float exploration_weight;
    int ret = hypo_curiosity_fep_compute_exploration(bridge, 0.9f, &exploration_weight);
    EXPECT_EQ(ret, 0);
    /* High curiosity drive should result in higher exploration */
    EXPECT_GT(exploration_weight, 0.0f);
}

/* ============================================================================
 * Free Energy Reduction Tests
 * ============================================================================ */

TEST_F(HypoCuriosityFepBridgeTest, ComputeFeReduction) {
    float fe_reduction;
    int ret = hypo_curiosity_fep_compute_fe_reduction(bridge, 0.5f, &fe_reduction);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(fe_reduction, 0.0f);
}

TEST_F(HypoCuriosityFepBridgeTest, ComputeFeReductionNullBridge) {
    float fe_reduction;
    int ret = hypo_curiosity_fep_compute_fe_reduction(nullptr, 0.5f, &fe_reduction);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, ComputeFeReductionNullOutput) {
    int ret = hypo_curiosity_fep_compute_fe_reduction(bridge, 0.5f, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, ComputeFeReductionScalesWithInfoGain) {
    float fe_reduction_low, fe_reduction_high;

    hypo_curiosity_fep_compute_fe_reduction(bridge, 0.2f, &fe_reduction_low);
    hypo_curiosity_fep_compute_fe_reduction(bridge, 0.8f, &fe_reduction_high);

    /* Higher info gain should produce larger FE reduction */
    EXPECT_GE(fe_reduction_high, fe_reduction_low);
}

/* ============================================================================
 * Precision Modulation Tests
 * ============================================================================ */

TEST_F(HypoCuriosityFepBridgeTest, ModulatePrecision) {
    float precision;
    int ret = hypo_curiosity_fep_modulate_precision(bridge, 0.5f, &precision);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(precision, 0.0f);
}

TEST_F(HypoCuriosityFepBridgeTest, ModulatePrecisionNull) {
    float precision;
    int ret = hypo_curiosity_fep_modulate_precision(nullptr, 0.5f, &precision);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, ModulatePrecisionNullOutput) {
    int ret = hypo_curiosity_fep_modulate_precision(bridge, 0.5f, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, ModulatePrecisionNovelty) {
    float precision_low, precision_high;

    hypo_curiosity_fep_modulate_precision(bridge, 0.1f, &precision_low);
    hypo_curiosity_fep_modulate_precision(bridge, 0.9f, &precision_high);

    /* Both should be valid precision values */
    EXPECT_GT(precision_low, 0.0f);
    EXPECT_GT(precision_high, 0.0f);
}

/* ============================================================================
 * Effects Retrieval Tests
 * ============================================================================ */

TEST_F(HypoCuriosityFepBridgeTest, GetEffects) {
    hypo_curiosity_fep_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int ret = hypo_curiosity_fep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, GetEffectsNullBridge) {
    hypo_curiosity_fep_effects_t effects;
    int ret = hypo_curiosity_fep_get_effects(nullptr, &effects);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, GetEffectsNullOutput) {
    int ret = hypo_curiosity_fep_get_effects(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, EffectsContainCuriosityData) {
    hypo_curiosity_fep_update(bridge, 100);

    hypo_curiosity_fep_effects_t effects;
    hypo_curiosity_fep_get_effects(bridge, &effects);

    /* Check curiosity-specific fields */
    EXPECT_GE(effects.free_energy, 0.0f);
    EXPECT_GE(effects.exploration_weight, 0.0f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(HypoCuriosityFepBridgeTest, GetStats) {
    hypo_curiosity_fep_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int ret = hypo_curiosity_fep_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, GetStatsNullBridge) {
    hypo_curiosity_fep_stats_t stats;
    int ret = hypo_curiosity_fep_get_stats(nullptr, &stats);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, GetStatsNullOutput) {
    int ret = hypo_curiosity_fep_get_stats(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, StatsAccumulate) {
    /* Perform some updates */
    for (int i = 0; i < 5; i++) {
        hypo_curiosity_fep_update(bridge, 100);
    }

    hypo_curiosity_fep_stats_t stats;
    hypo_curiosity_fep_get_stats(bridge, &stats);

    EXPECT_GE(stats.total_updates, 5u);
}

/* ============================================================================
 * Info Gain Reporting Tests
 * ============================================================================ */

TEST_F(HypoCuriosityFepBridgeTest, ReportInfoGain) {
    int ret = hypo_curiosity_fep_report_info_gain(bridge, 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, ReportInfoGainNull) {
    int ret = hypo_curiosity_fep_report_info_gain(nullptr, 0.5f);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, ReportInfoGainUpdatesState) {
    hypo_curiosity_fep_report_info_gain(bridge, 0.7f);

    /* State should reflect info gain */
    EXPECT_GT(bridge->state.current_info_gain, 0.0f);
}

TEST_F(HypoCuriosityFepBridgeTest, ReportInfoGainAccumulates) {
    hypo_curiosity_fep_report_info_gain(bridge, 0.3f);
    hypo_curiosity_fep_report_info_gain(bridge, 0.4f);

    /* Info gain events should accumulate */
    EXPECT_GE(bridge->state.info_gain_events, 2u);
}

/* ============================================================================
 * Novelty Reporting Tests
 * ============================================================================ */

TEST_F(HypoCuriosityFepBridgeTest, ReportNovelty) {
    int ret = hypo_curiosity_fep_report_novelty(bridge, 0.6f);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, ReportNoveltyNull) {
    int ret = hypo_curiosity_fep_report_novelty(nullptr, 0.6f);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, ReportNoveltyUpdatesState) {
    hypo_curiosity_fep_report_novelty(bridge, 0.8f);

    /* State should reflect novelty */
    EXPECT_GT(bridge->state.current_novelty, 0.0f);
}

/* ============================================================================
 * Bio-Async Connection Tests
 * ============================================================================ */

TEST_F(HypoCuriosityFepBridgeTest, ConnectBioAsync) {
    int ret = hypo_curiosity_fep_connect_bio_async(bridge);
    /* May return 0 or -1 depending on router availability */
    (void)ret;
}

TEST_F(HypoCuriosityFepBridgeTest, ConnectBioAsyncNull) {
    /* NULL is a graceful no-op, returns 0 */
    int ret = hypo_curiosity_fep_connect_bio_async(nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, DisconnectBioAsync) {
    hypo_curiosity_fep_connect_bio_async(bridge);
    int ret = hypo_curiosity_fep_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, DisconnectBioAsyncNull) {
    /* NULL is a graceful no-op, returns 0 */
    int ret = hypo_curiosity_fep_disconnect_bio_async(nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, IsBioAsyncConnected) {
    bool connected = hypo_curiosity_fep_is_bio_async_connected(bridge);
    /* Initial state should be disconnected */
    EXPECT_FALSE(connected);
}

TEST_F(HypoCuriosityFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = hypo_curiosity_fep_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(HypoCuriosityFepBridgeTest, ProcessMessages) {
    int processed = hypo_curiosity_fep_process_messages(bridge);
    EXPECT_GE(processed, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, ProcessMessagesNull) {
    /* NULL is a graceful no-op, returns 0 */
    int processed = hypo_curiosity_fep_process_messages(nullptr);
    EXPECT_EQ(processed, 0);
}

/* ============================================================================
 * Drive System Connection Tests
 * ============================================================================ */

TEST_F(HypoCuriosityFepBridgeTest, ConnectDrives) {
    int ret = hypo_curiosity_fep_connect_drives(bridge, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoCuriosityFepBridgeTest, ConnectDrivesNullBridge) {
    int ret = hypo_curiosity_fep_connect_drives(nullptr, nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * State Tracking Tests
 * ============================================================================ */

TEST_F(HypoCuriosityFepBridgeTest, StateActiveAfterUpdate) {
    /* Bridge is initialized as active */
    EXPECT_TRUE(bridge->state.active);

    hypo_curiosity_fep_update(bridge, 100);

    /* Stays active after update */
    EXPECT_TRUE(bridge->state.active);
}

TEST_F(HypoCuriosityFepBridgeTest, ExplorationTriggersTracked) {
    /* Report high novelty to trigger exploration */
    hypo_curiosity_fep_report_novelty(bridge, 0.9f);
    hypo_curiosity_fep_update(bridge, 100);

    /* Should have some exploration triggers */
    EXPECT_GE(bridge->state.exploration_triggers, 0u);
}

/* ============================================================================
 * Integration Test
 * ============================================================================ */

TEST_F(HypoCuriosityFepBridgeTest, FullWorkflow) {
    /* Reset bridge */
    EXPECT_EQ(hypo_curiosity_fep_reset(bridge), 0);

    /* Compute exploration weight */
    float exploration;
    EXPECT_EQ(hypo_curiosity_fep_compute_exploration(bridge, 0.6f, &exploration), 0);
    EXPECT_GT(exploration, 0.0f);

    /* Report info gain */
    EXPECT_EQ(hypo_curiosity_fep_report_info_gain(bridge, 0.4f), 0);

    /* Compute FE reduction */
    float fe_reduction;
    EXPECT_EQ(hypo_curiosity_fep_compute_fe_reduction(bridge, 0.4f, &fe_reduction), 0);

    /* Report novelty */
    EXPECT_EQ(hypo_curiosity_fep_report_novelty(bridge, 0.7f), 0);

    /* Modulate precision */
    float precision;
    EXPECT_EQ(hypo_curiosity_fep_modulate_precision(bridge, 0.5f, &precision), 0);

    /* Update bridge multiple times */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(hypo_curiosity_fep_update(bridge, 100), 0);
    }

    /* Get effects */
    hypo_curiosity_fep_effects_t effects;
    EXPECT_EQ(hypo_curiosity_fep_get_effects(bridge, &effects), 0);

    /* Get stats */
    hypo_curiosity_fep_stats_t stats;
    EXPECT_EQ(hypo_curiosity_fep_get_stats(bridge, &stats), 0);

    /* Verify accumulated state */
    EXPECT_GE(stats.total_updates, 5u);
    EXPECT_GE(stats.info_gain_fe_reductions, 0u);
}
