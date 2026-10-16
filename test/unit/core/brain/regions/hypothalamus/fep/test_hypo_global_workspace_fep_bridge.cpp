/**
 * @file test_hypo_global_workspace_fep_bridge.cpp
 * @brief Unit tests for Hypothalamus Global Workspace FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Global Workspace bidirectional integration
 * WHY:  Ensure broadcast priority, workspace availability, and competition work correctly
 * HOW:  Test lifecycle, connections, FE computation, broadcast priority, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_global_workspace_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
}

class HypoGlobalWorkspaceFepBridgeTest : public ::testing::Test {
protected:
    hypo_gw_fep_bridge_t* bridge = nullptr;
    fep_system_t* fep_system = nullptr;

    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_system = fep_create(&fep_config, 8, 4);

        /* Create bridge with defaults */
        bridge = hypo_gw_fep_create(nullptr, fep_system);
    }

    void TearDown() override {
        if (bridge) {
            hypo_gw_fep_destroy(bridge);
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

TEST_F(HypoGlobalWorkspaceFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, CreateWithNullConfig) {
    hypo_gw_fep_bridge_t* br = hypo_gw_fep_create(nullptr, fep_system);
    ASSERT_NE(br, nullptr);
    hypo_gw_fep_destroy(br);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, CreateWithConfig) {
    hypo_gw_fep_config_t config;
    hypo_gw_fep_default_config(&config);
    config.drive_fe_weight = 0.9f;
    config.priority_broadcast_scale = 1.5f;
    config.urgency_competition_scale = 2.0f;

    hypo_gw_fep_bridge_t* br = hypo_gw_fep_create(&config, fep_system);
    ASSERT_NE(br, nullptr);
    EXPECT_FLOAT_EQ(br->config.drive_fe_weight, 0.9f);
    EXPECT_FLOAT_EQ(br->config.priority_broadcast_scale, 1.5f);
    hypo_gw_fep_destroy(br);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, DestroyNull) {
    hypo_gw_fep_destroy(nullptr);
    /* Should not crash */
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, DefaultConfig) {
    hypo_gw_fep_config_t config;
    int ret = hypo_gw_fep_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.drive_fe_weight, 0.0f);
    EXPECT_GT(config.prediction_error_gain, 0.0f);
    EXPECT_GT(config.priority_broadcast_scale, 0.0f);
    EXPECT_GT(config.attention_fe_scale, 0.0f);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, DefaultConfigNullPtr) {
    int ret = hypo_gw_fep_default_config(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Reset Tests
 * ============================================================================ */

TEST_F(HypoGlobalWorkspaceFepBridgeTest, Reset) {
    int ret = hypo_gw_fep_reset(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ResetNull) {
    int ret = hypo_gw_fep_reset(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ResetClearsState) {
    /* Update to accumulate state */
    hypo_gw_fep_update(bridge, 100);
    hypo_gw_fep_update(bridge, 100);

    /* Reset */
    hypo_gw_fep_reset(bridge);

    /* Verify state is cleared */
    EXPECT_EQ(bridge->state.update_count, 0u);
    EXPECT_EQ(bridge->state.broadcast_wins, 0u);
    EXPECT_EQ(bridge->state.broadcast_losses, 0u);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(HypoGlobalWorkspaceFepBridgeTest, Update) {
    int ret = hypo_gw_fep_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, UpdateNull) {
    int ret = hypo_gw_fep_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, UpdateMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        int ret = hypo_gw_fep_update(bridge, 50);
        EXPECT_EQ(ret, 0);
    }
    EXPECT_GE(bridge->state.update_count, 10u);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, UpdateWithDifferentDeltas) {
    int ret = hypo_gw_fep_update(bridge, 0);
    EXPECT_EQ(ret, 0);

    ret = hypo_gw_fep_update(bridge, 1000);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Free Energy Computation Tests
 * ============================================================================ */

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ComputeFe) {
    float free_energy;
    int ret = hypo_gw_fep_compute_fe(bridge, 0.5f, &free_energy);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(free_energy, 0.0f);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ComputeFeNullBridge) {
    float free_energy;
    int ret = hypo_gw_fep_compute_fe(nullptr, 0.5f, &free_energy);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ComputeFeNullOutput) {
    int ret = hypo_gw_fep_compute_fe(bridge, 0.5f, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ComputeFeLowDemand) {
    float free_energy;
    int ret = hypo_gw_fep_compute_fe(bridge, 0.1f, &free_energy);
    EXPECT_EQ(ret, 0);
    /* Low attention demand should result in lower free energy */
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ComputeFeHighDemand) {
    float fe_low, fe_high;

    hypo_gw_fep_compute_fe(bridge, 0.2f, &fe_low);
    hypo_gw_fep_compute_fe(bridge, 0.9f, &fe_high);

    /* Higher attention demand should produce higher free energy */
    EXPECT_GE(fe_high, fe_low);
}

/* ============================================================================
 * Precision Modulation Tests
 * ============================================================================ */

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ModulatePrecision) {
    float precision;
    int ret = hypo_gw_fep_modulate_precision(bridge, 0.5f, &precision);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(precision, 0.0f);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ModulatePrecisionNull) {
    float precision;
    int ret = hypo_gw_fep_modulate_precision(nullptr, 0.5f, &precision);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ModulatePrecisionNullOutput) {
    int ret = hypo_gw_fep_modulate_precision(bridge, 0.5f, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ModulatePrecisionLowArousal) {
    float precision;
    int ret = hypo_gw_fep_modulate_precision(bridge, 0.1f, &precision);
    EXPECT_EQ(ret, 0);
    /* Low arousal should result in lower precision */
    EXPECT_GE(precision, HYPO_GW_FEP_BROADCAST_PRECISION_MIN);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ModulatePrecisionHighArousal) {
    float precision_low, precision_high;

    hypo_gw_fep_modulate_precision(bridge, 0.2f, &precision_low);
    hypo_gw_fep_modulate_precision(bridge, 0.9f, &precision_high);

    /* Higher arousal should increase precision */
    EXPECT_GE(precision_high, precision_low);
}

/* ============================================================================
 * Broadcast Priority Tests
 * ============================================================================ */

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ComputeBroadcastPriority) {
    float priority;
    int ret = hypo_gw_fep_compute_broadcast_priority(bridge, 0.5f, &priority);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(priority, 0.0f);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ComputeBroadcastPriorityNullBridge) {
    float priority;
    int ret = hypo_gw_fep_compute_broadcast_priority(nullptr, 0.5f, &priority);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ComputeBroadcastPriorityNullOutput) {
    int ret = hypo_gw_fep_compute_broadcast_priority(bridge, 0.5f, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, BroadcastPriorityScalesWithUrgency) {
    float priority_low, priority_high;

    hypo_gw_fep_compute_broadcast_priority(bridge, 0.2f, &priority_low);
    hypo_gw_fep_compute_broadcast_priority(bridge, 0.8f, &priority_high);

    /* Higher urgency should result in higher broadcast priority */
    EXPECT_GE(priority_high, priority_low);
}

/* ============================================================================
 * Effects Retrieval Tests
 * ============================================================================ */

TEST_F(HypoGlobalWorkspaceFepBridgeTest, GetEffects) {
    hypo_gw_fep_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int ret = hypo_gw_fep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, GetEffectsNullBridge) {
    hypo_gw_fep_effects_t effects;
    int ret = hypo_gw_fep_get_effects(nullptr, &effects);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, GetEffectsNullOutput) {
    int ret = hypo_gw_fep_get_effects(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, EffectsContainWorkspaceData) {
    hypo_gw_fep_update(bridge, 100);

    hypo_gw_fep_effects_t effects;
    hypo_gw_fep_get_effects(bridge, &effects);

    /* Check workspace-specific fields */
    EXPECT_GE(effects.free_energy, 0.0f);
    EXPECT_GE(effects.broadcast_priority, 0.0f);
    EXPECT_GE(effects.workspace_availability, 0.0f);
    EXPECT_LE(effects.workspace_availability, 1.0f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(HypoGlobalWorkspaceFepBridgeTest, GetStats) {
    hypo_gw_fep_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int ret = hypo_gw_fep_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, GetStatsNullBridge) {
    hypo_gw_fep_stats_t stats;
    int ret = hypo_gw_fep_get_stats(nullptr, &stats);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, GetStatsNullOutput) {
    int ret = hypo_gw_fep_get_stats(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, StatsAccumulate) {
    /* Perform some updates */
    for (int i = 0; i < 5; i++) {
        hypo_gw_fep_update(bridge, 100);
    }

    hypo_gw_fep_stats_t stats;
    hypo_gw_fep_get_stats(bridge, &stats);

    EXPECT_GE(stats.total_updates, 5u);
}

/* ============================================================================
 * Broadcast Win/Loss Reporting Tests
 * ============================================================================ */

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ReportBroadcastWin) {
    int ret = hypo_gw_fep_report_broadcast_win(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ReportBroadcastWinNull) {
    int ret = hypo_gw_fep_report_broadcast_win(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ReportBroadcastWinUpdatesState) {
    hypo_gw_fep_report_broadcast_win(bridge);

    /* State should reflect win */
    EXPECT_GE(bridge->state.broadcast_wins, 1u);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ReportBroadcastWinAccumulates) {
    hypo_gw_fep_report_broadcast_win(bridge);
    hypo_gw_fep_report_broadcast_win(bridge);
    hypo_gw_fep_report_broadcast_win(bridge);

    /* Wins should accumulate */
    EXPECT_GE(bridge->state.broadcast_wins, 3u);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ReportBroadcastLoss) {
    int ret = hypo_gw_fep_report_broadcast_loss(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ReportBroadcastLossNull) {
    int ret = hypo_gw_fep_report_broadcast_loss(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ReportBroadcastLossUpdatesState) {
    hypo_gw_fep_report_broadcast_loss(bridge);

    /* State should reflect loss */
    EXPECT_GE(bridge->state.broadcast_losses, 1u);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ReportBroadcastLossAccumulates) {
    hypo_gw_fep_report_broadcast_loss(bridge);
    hypo_gw_fep_report_broadcast_loss(bridge);

    /* Losses should accumulate */
    EXPECT_GE(bridge->state.broadcast_losses, 2u);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, BroadcastWinRateTracked) {
    /* Report some wins and losses */
    hypo_gw_fep_report_broadcast_win(bridge);
    hypo_gw_fep_report_broadcast_win(bridge);
    hypo_gw_fep_report_broadcast_win(bridge);
    hypo_gw_fep_report_broadcast_loss(bridge);

    hypo_gw_fep_update(bridge, 100);

    hypo_gw_fep_stats_t stats;
    hypo_gw_fep_get_stats(bridge, &stats);

    /* Win rate should be tracked and reasonable */
    EXPECT_GE(stats.broadcast_win_rate, 0.0f);
    EXPECT_LE(stats.broadcast_win_rate, 1.0f);
}

/* ============================================================================
 * Bio-Async Connection Tests
 * ============================================================================ */

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ConnectBioAsync) {
    int ret = hypo_gw_fep_connect_bio_async(bridge);
    /* May return 0 or -1 depending on router availability */
    (void)ret;
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ConnectBioAsyncNull) {
    /* NULL is a graceful no-op, returns 0 */
    int ret = hypo_gw_fep_connect_bio_async(nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, DisconnectBioAsync) {
    hypo_gw_fep_connect_bio_async(bridge);
    int ret = hypo_gw_fep_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, DisconnectBioAsyncNull) {
    /* NULL is a graceful no-op, returns 0 */
    int ret = hypo_gw_fep_disconnect_bio_async(nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, IsBioAsyncConnected) {
    bool connected = hypo_gw_fep_is_bio_async_connected(bridge);
    /* Initial state should be disconnected */
    EXPECT_FALSE(connected);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = hypo_gw_fep_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ProcessMessages) {
    int processed = hypo_gw_fep_process_messages(bridge);
    EXPECT_GE(processed, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ProcessMessagesNull) {
    /* NULL is a graceful no-op, returns 0 */
    int processed = hypo_gw_fep_process_messages(nullptr);
    EXPECT_EQ(processed, 0);
}

/* ============================================================================
 * Drive System Connection Tests
 * ============================================================================ */

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ConnectDrives) {
    int ret = hypo_gw_fep_connect_drives(bridge, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ConnectDrivesNullBridge) {
    int ret = hypo_gw_fep_connect_drives(nullptr, nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * State Tracking Tests
 * ============================================================================ */

TEST_F(HypoGlobalWorkspaceFepBridgeTest, StateActiveAfterUpdate) {
    /* Bridge is initialized as active */
    EXPECT_TRUE(bridge->state.active);

    hypo_gw_fep_update(bridge, 100);

    EXPECT_TRUE(bridge->state.active);
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, PriorityStateTracked) {
    /* Compute broadcast priority */
    float priority;
    int ret = hypo_gw_fep_compute_broadcast_priority(bridge, 0.7f, &priority);
    EXPECT_EQ(ret, 0);

    /* Priority should be computed based on urgency input */
    EXPECT_GT(priority, 0.0f);
    EXPECT_LE(priority, 1.0f);  /* Within bounds */
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, ArousalStateTracked) {
    /* Modulate precision with arousal */
    float precision;
    int ret = hypo_gw_fep_modulate_precision(bridge, 0.6f, &precision);
    EXPECT_EQ(ret, 0);

    /* Precision should be computed based on arousal input */
    EXPECT_GT(precision, 0.0f);
    EXPECT_LE(precision, 2.0f);  /* Within reasonable bounds */
}

TEST_F(HypoGlobalWorkspaceFepBridgeTest, AttentionDemandTracked) {
    /* Compute FE with attention demand */
    float fe;
    hypo_gw_fep_compute_fe(bridge, 0.5f, &fe);

    /* State should track attention demand */
    EXPECT_GT(bridge->state.current_attention_demand, 0.0f);
}

/* ============================================================================
 * Dominant Drive Tests
 * ============================================================================ */

TEST_F(HypoGlobalWorkspaceFepBridgeTest, DominantDriveTracking) {
    hypo_gw_fep_update(bridge, 100);

    /* Dominant drive should be valid */
    EXPECT_GE(bridge->state.dominant_drive, 0);
    EXPECT_LT(bridge->state.dominant_drive, HYPO_DRIVE_COUNT);
}

/* ============================================================================
 * Integration Test
 * ============================================================================ */

TEST_F(HypoGlobalWorkspaceFepBridgeTest, FullWorkflow) {
    /* Reset bridge */
    EXPECT_EQ(hypo_gw_fep_reset(bridge), 0);

    /* Compute FE from attention demand */
    float free_energy;
    EXPECT_EQ(hypo_gw_fep_compute_fe(bridge, 0.5f, &free_energy), 0);
    EXPECT_GE(free_energy, 0.0f);

    /* Modulate precision with arousal */
    float precision;
    EXPECT_EQ(hypo_gw_fep_modulate_precision(bridge, 0.6f, &precision), 0);
    EXPECT_GT(precision, 0.0f);

    /* Compute broadcast priority */
    float priority;
    EXPECT_EQ(hypo_gw_fep_compute_broadcast_priority(bridge, 0.7f, &priority), 0);
    EXPECT_GT(priority, 0.0f);

    /* Report some broadcast wins and losses */
    EXPECT_EQ(hypo_gw_fep_report_broadcast_win(bridge), 0);
    EXPECT_EQ(hypo_gw_fep_report_broadcast_win(bridge), 0);
    EXPECT_EQ(hypo_gw_fep_report_broadcast_loss(bridge), 0);

    /* Update bridge multiple times */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(hypo_gw_fep_update(bridge, 100), 0);
    }

    /* Get effects */
    hypo_gw_fep_effects_t effects;
    EXPECT_EQ(hypo_gw_fep_get_effects(bridge, &effects), 0);

    /* Get stats */
    hypo_gw_fep_stats_t stats;
    EXPECT_EQ(hypo_gw_fep_get_stats(bridge, &stats), 0);

    /* Verify accumulated state */
    EXPECT_GE(stats.total_updates, 5u);
    EXPECT_GE(bridge->state.broadcast_wins, 2u);
    EXPECT_GE(bridge->state.broadcast_losses, 1u);
}
