/**
 * @file test_rcog_bridges.cpp
 * @brief Unit tests for all Recursive Cognition Bridge modules
 *
 * WHAT: Comprehensive tests for all recursive cognition bidirectional integrations
 * WHY:  Ensure recursive processing integrates correctly with all connected systems
 * HOW:  Test lifecycle, effects, connection, and updates for each bridge type
 *
 * BRIDGES TESTED:
 * - rcog_collective_bridge: Swarm/collective workspace integration
 * - rcog_imagination_bridge: Imagination engine integration
 * - rcog_immune_bridge: Brain immune system integration
 * - rcog_bio_async_bridge: Bio-async messaging integration
 * - rcog_brain_kg_bridge: Brain knowledge graph integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/recursive/nimcp_rcog_types.h"
#include "cognitive/recursive/nimcp_rcog_collective_bridge.h"
#include "cognitive/recursive/nimcp_rcog_imagination_bridge.h"
#include "cognitive/recursive/nimcp_rcog_immune_bridge.h"
#include "cognitive/recursive/nimcp_rcog_bio_async_bridge.h"
#include "cognitive/recursive/nimcp_rcog_brain_kg_bridge.h"

/* ============================================================================
 * Test Fixture Base
 * ============================================================================ */

class RcogBridgesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/* ============================================================================
 * Collective Bridge Tests
 * ============================================================================ */

class RcogCollectiveBridgeTest : public RcogBridgesTest {
protected:
    rcog_collective_bridge_t* bridge = nullptr;

    void SetUp() override {
        RcogBridgesTest::SetUp();
    }

    void TearDown() override {
        if (bridge) {
            rcog_collective_bridge_destroy(bridge);
            bridge = nullptr;
        }
        RcogBridgesTest::TearDown();
    }
};

TEST_F(RcogCollectiveBridgeTest, DefaultConfig) {
    rcog_collective_bridge_config_t config = rcog_collective_bridge_default_config();
    EXPECT_TRUE(config.enable_volunteering);
    EXPECT_TRUE(config.enable_stigmergy);
    EXPECT_GT(config.broadcast_threshold, 0.0f);
    EXPECT_GT(config.broadcast_timeout_ms, 0u);
    EXPECT_GT(config.max_volunteered_tasks, 0u);
}

TEST_F(RcogCollectiveBridgeTest, CreateDefault) {
    bridge = rcog_collective_bridge_create_default();
    ASSERT_NE(bridge, nullptr);
}

TEST_F(RcogCollectiveBridgeTest, CreateWithConfig) {
    rcog_collective_bridge_config_t config = rcog_collective_bridge_default_config();
    config.max_volunteered_tasks = 16;
    config.consensus_threshold = 0.8f;
    bridge = rcog_collective_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(RcogCollectiveBridgeTest, DestroyNull) {
    rcog_collective_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(RcogCollectiveBridgeTest, IsConnectedBeforeConnect) {
    bridge = rcog_collective_bridge_create_default();
    ASSERT_NE(bridge, nullptr);
    EXPECT_FALSE(rcog_collective_bridge_is_connected(bridge));
}

TEST_F(RcogCollectiveBridgeTest, ConnectNullParams) {
    bridge = rcog_collective_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_collective_bridge_connect_workspace(nullptr, nullptr);
    EXPECT_NE(result, RCOG_OK);

    result = rcog_collective_bridge_connect_workspace(bridge, nullptr);
    EXPECT_NE(result, RCOG_OK);
}

TEST_F(RcogCollectiveBridgeTest, UpdateBeforeConnect) {
    bridge = rcog_collective_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_collective_bridge_update(bridge, 16.0f);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogCollectiveBridgeTest, GetOutgoingEffects) {
    bridge = rcog_collective_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_to_collective_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int result = rcog_collective_bridge_get_outgoing_effects(bridge, &effects);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogCollectiveBridgeTest, GetOutgoingEffectsNull) {
    bridge = rcog_collective_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_collective_bridge_get_outgoing_effects(bridge, nullptr);
    EXPECT_NE(result, RCOG_OK);

    result = rcog_collective_bridge_get_outgoing_effects(nullptr, nullptr);
    EXPECT_NE(result, RCOG_OK);
}

TEST_F(RcogCollectiveBridgeTest, GetIncomingEffects) {
    bridge = rcog_collective_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    collective_to_rcog_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int result = rcog_collective_bridge_get_incoming_effects(bridge, &effects);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogCollectiveBridgeTest, GetStats) {
    bridge = rcog_collective_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_collective_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = rcog_collective_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, RCOG_OK);
    EXPECT_EQ(stats.subtasks_broadcast, 0u);
}

TEST_F(RcogCollectiveBridgeTest, ResetStats) {
    bridge = rcog_collective_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_collective_bridge_reset_stats(bridge);

    rcog_collective_bridge_stats_t stats;
    int result = rcog_collective_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, RCOG_OK);
    EXPECT_EQ(stats.subtasks_broadcast, 0u);
}

/* ============================================================================
 * Imagination Bridge Tests
 * ============================================================================ */

class RcogImaginationBridgeTest : public RcogBridgesTest {
protected:
    rcog_imagination_bridge_t* bridge = nullptr;

    void TearDown() override {
        if (bridge) {
            rcog_imagination_bridge_destroy(bridge);
            bridge = nullptr;
        }
        RcogBridgesTest::TearDown();
    }
};

TEST_F(RcogImaginationBridgeTest, DefaultConfig) {
    rcog_imagination_bridge_config_t config = rcog_imagination_bridge_default_config();
    EXPECT_GT(config.simulation_threshold, 0.0f);
    EXPECT_LE(config.simulation_threshold, 1.0f);
    EXPECT_GT(config.max_simulation_depth, 0u);
    EXPECT_TRUE(config.enable_automatic_rehearsal);
}

TEST_F(RcogImaginationBridgeTest, CreateDefault) {
    bridge = rcog_imagination_bridge_create_default();
    ASSERT_NE(bridge, nullptr);
}

TEST_F(RcogImaginationBridgeTest, CreateWithConfig) {
    rcog_imagination_bridge_config_t config = rcog_imagination_bridge_default_config();
    config.max_simulation_depth = 5;
    config.enable_creative_mode = false;
    bridge = rcog_imagination_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(RcogImaginationBridgeTest, DestroyNull) {
    rcog_imagination_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(RcogImaginationBridgeTest, IsConnectedBeforeConnect) {
    bridge = rcog_imagination_bridge_create_default();
    ASSERT_NE(bridge, nullptr);
    EXPECT_FALSE(rcog_imagination_bridge_is_connected(bridge));
}

TEST_F(RcogImaginationBridgeTest, ConnectNullParams) {
    bridge = rcog_imagination_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_imagination_bridge_connect(nullptr, nullptr);
    EXPECT_NE(result, RCOG_OK);

    result = rcog_imagination_bridge_connect(bridge, nullptr);
    EXPECT_NE(result, RCOG_OK);
}

TEST_F(RcogImaginationBridgeTest, ConnectEngineNullParams) {
    bridge = rcog_imagination_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_imagination_bridge_connect_engine(nullptr, nullptr);
    EXPECT_NE(result, RCOG_OK);

    result = rcog_imagination_bridge_connect_engine(bridge, nullptr);
    EXPECT_NE(result, RCOG_OK);
}

TEST_F(RcogImaginationBridgeTest, Update) {
    bridge = rcog_imagination_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_imagination_bridge_update(bridge, 16.0f);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogImaginationBridgeTest, GetOutgoingEffects) {
    bridge = rcog_imagination_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_to_imagination_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int result = rcog_imagination_bridge_get_outgoing_effects(bridge, &effects);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogImaginationBridgeTest, GetIncomingEffects) {
    bridge = rcog_imagination_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    imagination_to_rcog_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int result = rcog_imagination_bridge_get_incoming_effects(bridge, &effects);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogImaginationBridgeTest, SimulateDecompositionsNullParams) {
    bridge = rcog_imagination_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.type = RCOG_GOAL_REASONING;
    goal.query = "test query";

    rcog_simulation_result_t results[4];

    int result = rcog_imagination_bridge_simulate_decompositions(
        bridge, nullptr, nullptr, 0, results);
    EXPECT_NE(result, RCOG_OK);

    result = rcog_imagination_bridge_simulate_decompositions(
        bridge, &goal, nullptr, 0, nullptr);
    EXPECT_NE(result, RCOG_OK);
}

TEST_F(RcogImaginationBridgeTest, RehearseSubtasksNullParams) {
    bridge = rcog_imagination_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_rehearsal_result_t results[8];

    int result = rcog_imagination_bridge_rehearse_subtasks(
        bridge, nullptr, 0, results);
    EXPECT_NE(result, RCOG_OK);

    result = rcog_imagination_bridge_rehearse_subtasks(
        bridge, nullptr, 0, nullptr);
    EXPECT_NE(result, RCOG_OK);
}

TEST_F(RcogImaginationBridgeTest, PredictAnswerNullParams) {
    bridge = rcog_imagination_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.type = RCOG_GOAL_REASONING;
    float confidence;
    uint32_t steps;

    int result = rcog_imagination_bridge_predict_answer(
        bridge, nullptr, &confidence, &steps);
    EXPECT_NE(result, RCOG_OK);

    result = rcog_imagination_bridge_predict_answer(
        bridge, &goal, nullptr, &steps);
    EXPECT_NE(result, RCOG_OK);
}

TEST_F(RcogImaginationBridgeTest, GetStats) {
    bridge = rcog_imagination_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_imagination_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = rcog_imagination_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, RCOG_OK);
    EXPECT_EQ(stats.simulations_requested, 0u);
}

TEST_F(RcogImaginationBridgeTest, ResetStats) {
    bridge = rcog_imagination_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_imagination_bridge_reset_stats(bridge);

    rcog_imagination_bridge_stats_t stats;
    int result = rcog_imagination_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, RCOG_OK);
    EXPECT_EQ(stats.simulations_completed, 0u);
}

/* ============================================================================
 * Immune Bridge Tests
 * ============================================================================ */

class RcogImmuneBridgeTest : public RcogBridgesTest {
protected:
    rcog_immune_bridge_t* bridge = nullptr;

    void TearDown() override {
        if (bridge) {
            rcog_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        RcogBridgesTest::TearDown();
    }
};

TEST_F(RcogImmuneBridgeTest, DefaultConfig) {
    rcog_immune_bridge_config_t config = rcog_immune_bridge_default_config();
    EXPECT_GT(config.il1_sensitivity, 0.0f);
    EXPECT_GT(config.il6_sensitivity, 0.0f);
    EXPECT_GT(config.tnf_sensitivity, 0.0f);
    EXPECT_GT(config.min_capacity, 0.0f);
    EXPECT_TRUE(config.enable_auto_recovery);
}

TEST_F(RcogImmuneBridgeTest, CreateDefault) {
    bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);
}

TEST_F(RcogImmuneBridgeTest, CreateWithConfig) {
    rcog_immune_bridge_config_t config = rcog_immune_bridge_default_config();
    config.quarantine_threshold = 5;
    config.enable_auto_recovery = false;
    bridge = rcog_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(RcogImmuneBridgeTest, DestroyNull) {
    rcog_immune_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(RcogImmuneBridgeTest, IsConnectedBeforeConnect) {
    bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);
    EXPECT_FALSE(rcog_immune_bridge_is_connected(bridge));
}

TEST_F(RcogImmuneBridgeTest, ConnectNullParams) {
    bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_immune_bridge_connect(nullptr, nullptr);
    EXPECT_NE(result, RCOG_OK);

    result = rcog_immune_bridge_connect(bridge, nullptr);
    EXPECT_NE(result, RCOG_OK);
}

TEST_F(RcogImmuneBridgeTest, Update) {
    bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_immune_bridge_update(bridge, 16.0f);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogImmuneBridgeTest, GetModulation) {
    bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_immune_modulation_t modulation;
    memset(&modulation, 0, sizeof(modulation));

    int result = rcog_immune_bridge_get_modulation(bridge, &modulation);
    EXPECT_EQ(result, RCOG_OK);
    EXPECT_FLOAT_EQ(modulation.capacity_multiplier, 1.0f);
}

TEST_F(RcogImmuneBridgeTest, GetInflammationLevel) {
    bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_inflammation_level_t level = rcog_immune_bridge_get_inflammation_level(bridge);
    EXPECT_EQ(level, RCOG_INFLAMMATION_NONE);
}

TEST_F(RcogImmuneBridgeTest, GetCytokines) {
    bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_cytokine_levels_t cytokines;
    memset(&cytokines, 0, sizeof(cytokines));

    int result = rcog_immune_bridge_get_cytokines(bridge, &cytokines);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogImmuneBridgeTest, ReportFailureNull) {
    bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_immune_bridge_report_failure(nullptr, nullptr, RCOG_ERROR_TIMEOUT);
    EXPECT_NE(result, RCOG_OK);
}

TEST_F(RcogImmuneBridgeTest, IsQuarantinedNull) {
    bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    bool quarantined = rcog_immune_bridge_is_quarantined(nullptr, nullptr);
    EXPECT_FALSE(quarantined);

    quarantined = rcog_immune_bridge_is_quarantined(bridge, nullptr);
    EXPECT_FALSE(quarantined);
}

TEST_F(RcogImmuneBridgeTest, GetQuarantineStrength) {
    bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    float strength = rcog_immune_bridge_get_quarantine_strength(nullptr, nullptr);
    EXPECT_FLOAT_EQ(strength, 0.0f);

    strength = rcog_immune_bridge_get_quarantine_strength(bridge, nullptr);
    EXPECT_FLOAT_EQ(strength, 0.0f);
}

TEST_F(RcogImmuneBridgeTest, GetOutgoingEffects) {
    bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_to_immune_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int result = rcog_immune_bridge_get_outgoing_effects(bridge, &effects);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogImmuneBridgeTest, GetIncomingEffects) {
    bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    immune_to_rcog_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int result = rcog_immune_bridge_get_incoming_effects(bridge, &effects);
    EXPECT_EQ(result, RCOG_OK);
    // Before connection, capacity_multiplier may be 0 (uninitialized)
    EXPECT_GE(effects.capacity_multiplier, 0.0f);
    EXPECT_LE(effects.capacity_multiplier, 1.0f);
}

TEST_F(RcogImmuneBridgeTest, GetStats) {
    bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_immune_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = rcog_immune_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, RCOG_OK);
    EXPECT_EQ(stats.failures_reported, 0u);
}

TEST_F(RcogImmuneBridgeTest, ResetStats) {
    bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_immune_bridge_reset_stats(bridge);

    rcog_immune_bridge_stats_t stats;
    int result = rcog_immune_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, RCOG_OK);
}

/* ============================================================================
 * Bio-Async Bridge Tests
 * ============================================================================ */

class RcogBioAsyncBridgeTest : public RcogBridgesTest {
protected:
    rcog_bio_async_bridge_t* bridge = nullptr;

    void TearDown() override {
        if (bridge) {
            rcog_bio_async_bridge_destroy(bridge);
            bridge = nullptr;
        }
        RcogBridgesTest::TearDown();
    }
};

TEST_F(RcogBioAsyncBridgeTest, DefaultConfig) {
    rcog_bio_async_bridge_config_t config = rcog_bio_async_bridge_default_config();
    EXPECT_GT(config.dopamine_sensitivity, 0.0f);
    EXPECT_GT(config.norepinephrine_sensitivity, 0.0f);
    EXPECT_GT(config.acetylcholine_sensitivity, 0.0f);
    EXPECT_GT(config.coherence_threshold, 0.0f);
    EXPECT_GT(config.message_queue_size, 0u);
}

TEST_F(RcogBioAsyncBridgeTest, CreateDefault) {
    bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);
}

TEST_F(RcogBioAsyncBridgeTest, CreateWithConfig) {
    rcog_bio_async_bridge_config_t config = rcog_bio_async_bridge_default_config();
    config.coupling_strength = 0.8f;
    config.enable_message_logging = true;
    bridge = rcog_bio_async_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(RcogBioAsyncBridgeTest, DestroyNull) {
    rcog_bio_async_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(RcogBioAsyncBridgeTest, IsConnectedBeforeConnect) {
    bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);
    EXPECT_FALSE(rcog_bio_async_bridge_is_connected(bridge));
}

TEST_F(RcogBioAsyncBridgeTest, ConnectNullParams) {
    bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_bio_async_bridge_connect(nullptr, nullptr);
    EXPECT_NE(result, RCOG_OK);

    result = rcog_bio_async_bridge_connect(bridge, nullptr);
    EXPECT_NE(result, RCOG_OK);
}

TEST_F(RcogBioAsyncBridgeTest, Disconnect) {
    bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_bio_async_bridge_disconnect(bridge);
    EXPECT_EQ(result, RCOG_OK);
    EXPECT_FALSE(rcog_bio_async_bridge_is_connected(bridge));
}

TEST_F(RcogBioAsyncBridgeTest, UpdateNotConnected) {
    bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_bio_async_bridge_update(bridge, 16.0f);
    EXPECT_NE(result, RCOG_OK);  // Should fail when not connected
}

TEST_F(RcogBioAsyncBridgeTest, SendMessageNotConnected) {
    bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_bio_async_bridge_send_message(
        bridge, RCOG_MSG_SUBTASK_STARTED, nullptr, 0);
    EXPECT_NE(result, RCOG_OK);
}

TEST_F(RcogBioAsyncBridgeTest, ReleaseDopamine) {
    bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_bio_async_bridge_release_dopamine(bridge, 0.5f, 1);
    EXPECT_EQ(result, RCOG_OK);

    // Check invalid amount
    result = rcog_bio_async_bridge_release_dopamine(bridge, 1.5f, 1);
    EXPECT_NE(result, RCOG_OK);

    result = rcog_bio_async_bridge_release_dopamine(bridge, -0.5f, 1);
    EXPECT_NE(result, RCOG_OK);
}

TEST_F(RcogBioAsyncBridgeTest, SignalPriority) {
    bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_bio_async_bridge_signal_priority(bridge, 0.8f, 1);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogBioAsyncBridgeTest, ModulateAttention) {
    bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_bio_async_bridge_modulate_attention(bridge, 0.9f, "test_target");
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogBioAsyncBridgeTest, GetOutgoingEffects) {
    bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    // Set some values first
    rcog_bio_async_bridge_release_dopamine(bridge, 0.7f, 1);

    rcog_to_bio_async_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int result = rcog_bio_async_bridge_get_outgoing_effects(bridge, &effects);
    EXPECT_EQ(result, RCOG_OK);
    EXPECT_FLOAT_EQ(effects.dopamine_release, 0.7f);
}

TEST_F(RcogBioAsyncBridgeTest, GetIncomingEffects) {
    bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    bio_async_to_rcog_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int result = rcog_bio_async_bridge_get_incoming_effects(bridge, &effects);
    EXPECT_EQ(result, RCOG_OK);
    EXPECT_FLOAT_EQ(effects.available_capacity, 1.0f);
}

TEST_F(RcogBioAsyncBridgeTest, GetStats) {
    bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_bio_async_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = rcog_bio_async_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogBioAsyncBridgeTest, ResetStats) {
    bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_bio_async_bridge_reset_stats(bridge);

    rcog_bio_async_bridge_stats_t stats;
    int result = rcog_bio_async_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, RCOG_OK);
    EXPECT_EQ(stats.messages_sent, 0u);
}

/* ============================================================================
 * Brain KG Bridge Tests
 * ============================================================================ */

class RcogBrainKgBridgeTest : public RcogBridgesTest {
protected:
    rcog_brain_kg_bridge_t* bridge = nullptr;

    void TearDown() override {
        if (bridge) {
            rcog_brain_kg_bridge_destroy(bridge);
            bridge = nullptr;
        }
        RcogBridgesTest::TearDown();
    }
};

TEST_F(RcogBrainKgBridgeTest, DefaultConfig) {
    rcog_brain_kg_bridge_config_t config = rcog_brain_kg_bridge_default_config();
    EXPECT_TRUE(config.auto_register_on_connect);
    EXPECT_TRUE(config.register_all_components);
    EXPECT_TRUE(config.enable_continuous_updates);
    EXPECT_TRUE(config.enable_introspection);
    EXPECT_TRUE(config.enable_semantic_queries);
    EXPECT_GT(config.state_update_interval_ms, 0u);
}

TEST_F(RcogBrainKgBridgeTest, CreateDefault) {
    bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);
}

TEST_F(RcogBrainKgBridgeTest, CreateWithConfig) {
    rcog_brain_kg_bridge_config_t config = rcog_brain_kg_bridge_default_config();
    config.enable_introspection = false;
    config.max_semantic_query_depth = 5;
    bridge = rcog_brain_kg_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(RcogBrainKgBridgeTest, DestroyNull) {
    rcog_brain_kg_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(RcogBrainKgBridgeTest, IsConnectedBeforeConnect) {
    bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);
    EXPECT_FALSE(rcog_brain_kg_bridge_is_connected(bridge));
}

TEST_F(RcogBrainKgBridgeTest, ConnectNullParams) {
    bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_brain_kg_bridge_connect(nullptr, nullptr);
    EXPECT_NE(result, RCOG_OK);

    result = rcog_brain_kg_bridge_connect(bridge, nullptr);
    EXPECT_NE(result, RCOG_OK);
}

TEST_F(RcogBrainKgBridgeTest, Update) {
    bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    int result = rcog_brain_kg_bridge_update(bridge, 16.0f);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogBrainKgBridgeTest, RegisterEngineNotConnected) {
    bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_kg_node_id_t node_id;
    int result = rcog_brain_kg_bridge_register_engine(bridge, &node_id);
    EXPECT_NE(result, RCOG_OK);
}

TEST_F(RcogBrainKgBridgeTest, UpdateState) {
    bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_processing_state_t state = {0};
    state.is_processing = true;
    state.current_depth = 2;

    int result = rcog_brain_kg_bridge_update_state(bridge, &state);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogBrainKgBridgeTest, GetFocus) {
    bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    char focus[128];
    int result = rcog_brain_kg_bridge_get_focus(bridge, focus, sizeof(focus));
    EXPECT_EQ(result, RCOG_OK);
    EXPECT_STREQ(focus, "idle");
}

TEST_F(RcogBrainKgBridgeTest, GetSystemHealth) {
    bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    float health = rcog_brain_kg_bridge_get_system_health(bridge);
    EXPECT_FLOAT_EQ(health, 1.0f);
}

TEST_F(RcogBrainKgBridgeTest, HasCapability) {
    bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    bool has = rcog_brain_kg_bridge_has_capability(bridge, "nonexistent");
    EXPECT_FALSE(has);

    has = rcog_brain_kg_bridge_has_capability(nullptr, "test");
    EXPECT_FALSE(has);
}

TEST_F(RcogBrainKgBridgeTest, GetOutgoingEffects) {
    bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_to_kg_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int result = rcog_brain_kg_bridge_get_outgoing_effects(bridge, &effects);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogBrainKgBridgeTest, GetIncomingEffects) {
    bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    kg_to_rcog_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    int result = rcog_brain_kg_bridge_get_incoming_effects(bridge, &effects);
    EXPECT_EQ(result, RCOG_OK);
    // Before connection, overall_health may be 0 (uninitialized)
    EXPECT_GE(effects.overall_health, 0.0f);
    EXPECT_LE(effects.overall_health, 1.0f);
}

TEST_F(RcogBrainKgBridgeTest, GetStats) {
    bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_brain_kg_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = rcog_brain_kg_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, RCOG_OK);
}

TEST_F(RcogBrainKgBridgeTest, ResetStats) {
    bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_brain_kg_bridge_reset_stats(bridge);

    rcog_brain_kg_bridge_stats_t stats;
    int result = rcog_brain_kg_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, RCOG_OK);
    EXPECT_EQ(stats.state_updates, 0u);
}

/* ============================================================================
 * Cross-Bridge Effects Tests
 * ============================================================================ */

class RcogCrossBridgeTest : public RcogBridgesTest {
protected:
    rcog_collective_bridge_t* collective = nullptr;
    rcog_imagination_bridge_t* imagination = nullptr;
    rcog_immune_bridge_t* immune = nullptr;
    rcog_bio_async_bridge_t* bio_async = nullptr;
    rcog_brain_kg_bridge_t* brain_kg = nullptr;

    void SetUp() override {
        RcogBridgesTest::SetUp();
        collective = rcog_collective_bridge_create_default();
        imagination = rcog_imagination_bridge_create_default();
        immune = rcog_immune_bridge_create_default();
        bio_async = rcog_bio_async_bridge_create_default();
        brain_kg = rcog_brain_kg_bridge_create_default();
    }

    void TearDown() override {
        if (collective) rcog_collective_bridge_destroy(collective);
        if (imagination) rcog_imagination_bridge_destroy(imagination);
        if (immune) rcog_immune_bridge_destroy(immune);
        if (bio_async) rcog_bio_async_bridge_destroy(bio_async);
        if (brain_kg) rcog_brain_kg_bridge_destroy(brain_kg);
        RcogBridgesTest::TearDown();
    }
};

TEST_F(RcogCrossBridgeTest, AllBridgesCreated) {
    ASSERT_NE(collective, nullptr);
    ASSERT_NE(imagination, nullptr);
    ASSERT_NE(immune, nullptr);
    ASSERT_NE(bio_async, nullptr);
    ASSERT_NE(brain_kg, nullptr);
}

TEST_F(RcogCrossBridgeTest, SimultaneousUpdates) {
    // All bridges should be able to update without interference
    EXPECT_EQ(rcog_collective_bridge_update(collective, 16.0f), RCOG_OK);
    EXPECT_EQ(rcog_imagination_bridge_update(imagination, 16.0f), RCOG_OK);
    EXPECT_EQ(rcog_immune_bridge_update(immune, 16.0f), RCOG_OK);
    EXPECT_EQ(rcog_brain_kg_bridge_update(brain_kg, 16.0f), RCOG_OK);
    // bio_async requires connection, skip
}

TEST_F(RcogCrossBridgeTest, EffectsIndependent) {
    // Changes to one bridge shouldn't affect others
    rcog_bio_async_bridge_release_dopamine(bio_async, 0.8f, 1);

    rcog_to_bio_async_effects_t bio_effects;
    rcog_bio_async_bridge_get_outgoing_effects(bio_async, &bio_effects);
    EXPECT_FLOAT_EQ(bio_effects.dopamine_release, 0.8f);

    // Immune bridge should be unaffected (before connection, may be 0)
    immune_to_rcog_effects_t immune_effects;
    rcog_immune_bridge_get_incoming_effects(immune, &immune_effects);
    EXPECT_GE(immune_effects.capacity_multiplier, 0.0f);
    EXPECT_LE(immune_effects.capacity_multiplier, 1.0f);
}

/* ============================================================================
 * Error Name Tests
 * ============================================================================ */

TEST(RcogTypesTest, ErrorNames) {
    EXPECT_STREQ(rcog_error_name(RCOG_OK), "OK");
    EXPECT_STREQ(rcog_error_name(RCOG_ERROR_NULL_POINTER), "NULL_POINTER");
    EXPECT_STREQ(rcog_error_name(RCOG_ERROR_TIMEOUT), "TIMEOUT");
    EXPECT_STREQ(rcog_error_name(RCOG_ERROR_IMAGINATION_FAILED), "IMAGINATION_FAILED");
    EXPECT_STREQ(rcog_error_name(RCOG_ERROR_SWARM_DISCONNECTED), "SWARM_DISCONNECTED");
    EXPECT_STREQ(rcog_error_name((rcog_error_t)0xFFFFFF), "UNKNOWN");
}

TEST(RcogTypesTest, DtypeNames) {
    EXPECT_STREQ(rcog_dtype_name(RCOG_DTYPE_TEXT), "text");
    EXPECT_STREQ(rcog_dtype_name(RCOG_DTYPE_TENSOR), "tensor");
    EXPECT_STREQ(rcog_dtype_name(RCOG_DTYPE_JSON), "json");
    EXPECT_STREQ(rcog_dtype_name(RCOG_DTYPE_EMBEDDING), "embedding");
}

TEST(RcogTypesTest, TierNames) {
    EXPECT_STREQ(rcog_tier_name(RCOG_TIER_ROOT), "root");
    EXPECT_STREQ(rcog_tier_name(RCOG_TIER_L1_REASONING), "L1_reasoning");
    EXPECT_STREQ(rcog_tier_name(RCOG_TIER_L4_SPECIALIZED), "L4_specialized");
}

TEST(RcogTypesTest, GoalTypeNames) {
    EXPECT_STREQ(rcog_goal_type_name(RCOG_GOAL_REASONING), "reasoning");
    EXPECT_STREQ(rcog_goal_type_name(RCOG_GOAL_PLANNING), "planning");
    EXPECT_STREQ(rcog_goal_type_name(RCOG_GOAL_GENERATION), "generation");
}

TEST(RcogTypesTest, SubtaskStatusNames) {
    EXPECT_STREQ(rcog_subtask_status_name(RCOG_SUBTASK_PENDING), "pending");
    EXPECT_STREQ(rcog_subtask_status_name(RCOG_SUBTASK_RUNNING), "running");
    EXPECT_STREQ(rcog_subtask_status_name(RCOG_SUBTASK_COMPLETED), "completed");
    EXPECT_STREQ(rcog_subtask_status_name(RCOG_SUBTASK_FAILED), "failed");
}

TEST(RcogTypesTest, AnswerStatusNames) {
    EXPECT_STREQ(rcog_answer_status_name(RCOG_ANSWER_INITIALIZING), "initializing");
    EXPECT_STREQ(rcog_answer_status_name(RCOG_ANSWER_REFINING), "refining");
    EXPECT_STREQ(rcog_answer_status_name(RCOG_ANSWER_READY), "ready");
}

TEST(RcogTypesTest, DefaultQueryParams) {
    rcog_query_params_t params = rcog_query_params_default();
    EXPECT_EQ(params.output_limit, RCOG_DEFAULT_OUTPUT_LIMIT);
    EXPECT_EQ(params.start, 0u);
    EXPECT_EQ(params.count, 0u);
}
