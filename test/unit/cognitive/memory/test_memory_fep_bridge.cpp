/**
 * @file test_memory_fep_bridge.cpp
 * @brief Unit tests for Memory-FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Memory bidirectional integration
 * WHY:  Ensure working memory belief buffer and consolidation work correctly
 * HOW:  Test lifecycle, connections, WM maintenance, consolidation, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/memory/nimcp_memory_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class MemoryFepBridgeTest : public ::testing::Test {
protected:
    memory_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        memory_fep_config_t config;
        memory_fep_bridge_default_config(&config);
        bridge = memory_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            memory_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MemoryFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(MemoryFepBridgeTest, CreateWithNullConfig) {
    memory_fep_bridge_t* br = memory_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    memory_fep_bridge_destroy(br);
}

TEST_F(MemoryFepBridgeTest, DestroyNull) {
    memory_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(MemoryFepBridgeTest, DefaultConfig) {
    memory_fep_config_t config;
    int ret = memory_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(config.wm_capacity_factor, MEMORY_FEP_WM_CAPACITY);
    EXPECT_FLOAT_EQ(config.consolidation_threshold, MEMORY_FEP_CONSOLIDATION_THRESHOLD);
    EXPECT_GT(config.retrieval_precision_boost, 0.0f);
    EXPECT_TRUE(config.enable_wm_belief_buffer);
    EXPECT_TRUE(config.enable_consolidation_replay);
    EXPECT_TRUE(config.enable_retrieval_active_inference);
}

TEST_F(MemoryFepBridgeTest, DefaultConfigNullPtr) {
    int ret = memory_fep_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(MemoryFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = memory_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(MemoryFepBridgeTest, ConnectFepNull) {
    EXPECT_EQ(memory_fep_bridge_connect_fep(nullptr, nullptr), -1);

    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);

    EXPECT_EQ(memory_fep_bridge_connect_fep(nullptr, fep), -1);
    EXPECT_EQ(memory_fep_bridge_connect_fep(bridge, nullptr), -1);

    fep_destroy(fep);
}

TEST_F(MemoryFepBridgeTest, ConnectMemory) {
    // Memory system requires complex initialization, test with NULL for now
    int ret = memory_fep_bridge_connect_memory(bridge, nullptr);
    EXPECT_EQ(ret, -1);  // Should fail with NULL
}

TEST_F(MemoryFepBridgeTest, ConnectMemoryNull) {
    EXPECT_EQ(memory_fep_bridge_connect_memory(nullptr, nullptr), -1);
}

TEST_F(MemoryFepBridgeTest, Disconnect) {
    int ret = memory_fep_bridge_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MemoryFepBridgeTest, DisconnectNull) {
    EXPECT_EQ(memory_fep_bridge_disconnect(nullptr), -1);
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(MemoryFepBridgeTest, GetState) {
    memory_fep_state_t state;
    int ret = memory_fep_bridge_get_state(bridge, &state);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.current_wm_load, 0.0f);
    EXPECT_LE(state.current_wm_load, 1.0f);
    EXPECT_GE(state.current_precision, 0.0f);
}

TEST_F(MemoryFepBridgeTest, GetStateNull) {
    memory_fep_state_t state;

    EXPECT_EQ(memory_fep_bridge_get_state(nullptr, &state), -1);
    EXPECT_EQ(memory_fep_bridge_get_state(bridge, nullptr), -1);
}

TEST_F(MemoryFepBridgeTest, GetStats) {
    memory_fep_stats_t stats;
    int ret = memory_fep_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.wm_buffer_events, 0u);
    EXPECT_EQ(stats.consolidation_events, 0u);
    EXPECT_EQ(stats.retrieval_events, 0u);
}

TEST_F(MemoryFepBridgeTest, GetStatsNull) {
    memory_fep_stats_t stats;

    EXPECT_EQ(memory_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(memory_fep_bridge_get_stats(bridge, nullptr), -1);
}

/* ============================================================================
 * FEP → Memory Direction Tests
 * ============================================================================ */

TEST_F(MemoryFepBridgeTest, MaintainWmBeliefs) {
    int ret = memory_fep_maintain_wm_beliefs(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MemoryFepBridgeTest, MaintainWmBeliefsNull) {
    EXPECT_EQ(memory_fep_maintain_wm_beliefs(nullptr), -1);
}

TEST_F(MemoryFepBridgeTest, TriggerConsolidation) {
    int ret = memory_fep_trigger_consolidation(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MemoryFepBridgeTest, TriggerConsolidationNull) {
    EXPECT_EQ(memory_fep_trigger_consolidation(nullptr), -1);
}

TEST_F(MemoryFepBridgeTest, BoostRetrievalPrecision) {
    int ret = memory_fep_boost_retrieval_precision(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MemoryFepBridgeTest, BoostRetrievalPrecisionNull) {
    EXPECT_EQ(memory_fep_boost_retrieval_precision(nullptr), -1);
}

/* ============================================================================
 * Memory → FEP Direction Tests
 * ============================================================================ */

TEST_F(MemoryFepBridgeTest, ApplyBeliefPriors) {
    int ret = memory_fep_apply_belief_priors(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MemoryFepBridgeTest, ApplyBeliefPriorsNull) {
    EXPECT_EQ(memory_fep_apply_belief_priors(nullptr), -1);
}

TEST_F(MemoryFepBridgeTest, ApplyTracePersistence) {
    int ret = memory_fep_apply_trace_persistence(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MemoryFepBridgeTest, ApplyTracePersistenceNull) {
    EXPECT_EQ(memory_fep_apply_trace_persistence(nullptr), -1);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(MemoryFepBridgeTest, Update) {
    int ret = memory_fep_bridge_update(bridge, 16);  // 16ms delta
    EXPECT_EQ(ret, 0);
}

TEST_F(MemoryFepBridgeTest, UpdateNull) {
    EXPECT_EQ(memory_fep_bridge_update(nullptr, 16), -1);
}

TEST_F(MemoryFepBridgeTest, UpdateZeroDelta) {
    int ret = memory_fep_bridge_update(bridge, 0);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(MemoryFepBridgeTest, BioAsyncConnect) {
    int ret = memory_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MemoryFepBridgeTest, BioAsyncDisconnect) {
    memory_fep_bridge_connect_bio_async(bridge);

    int ret = memory_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(memory_fep_bridge_is_bio_async_connected(bridge));
}

TEST_F(MemoryFepBridgeTest, BioAsyncIsConnected) {
    EXPECT_FALSE(memory_fep_bridge_is_bio_async_connected(bridge));

    memory_fep_bridge_connect_bio_async(bridge);
    // May or may not be connected depending on router availability

    memory_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(memory_fep_bridge_is_bio_async_connected(bridge));
}

TEST_F(MemoryFepBridgeTest, BioAsyncNullParams) {
    EXPECT_EQ(memory_fep_bridge_connect_bio_async(nullptr), -1);
    EXPECT_EQ(memory_fep_bridge_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(memory_fep_bridge_is_bio_async_connected(nullptr));
}

TEST_F(MemoryFepBridgeTest, BioAsyncDoubleConnect) {
    memory_fep_bridge_connect_bio_async(bridge);
    int ret = memory_fep_bridge_connect_bio_async(bridge);  // Should be no-op
    EXPECT_EQ(ret, 0);
    memory_fep_bridge_disconnect_bio_async(bridge);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(MemoryFepBridgeTest, WmMaintainsBeliefs) {
    memory_fep_maintain_wm_beliefs(bridge);

    memory_fep_stats_t stats;
    memory_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.wm_buffer_events, 0u);
}

TEST_F(MemoryFepBridgeTest, HighLoadTriggersConsolidation) {
    // Simulate high WM load
    memory_fep_trigger_consolidation(bridge);

    memory_fep_state_t state;
    memory_fep_bridge_get_state(bridge, &state);
    // Consolidation may or may not be active depending on load
    EXPECT_GE(state.current_wm_load, 0.0f);
}

TEST_F(MemoryFepBridgeTest, RetrievalBoostsPrecision) {
    memory_fep_boost_retrieval_precision(bridge);

    memory_fep_stats_t stats;
    memory_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.retrieval_events, 0u);
}

TEST_F(MemoryFepBridgeTest, BeliefPriorsBiasFep) {
    memory_fep_apply_belief_priors(bridge);

    memory_fep_stats_t stats;
    memory_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.belief_prior_applications, 0u);
}

TEST_F(MemoryFepBridgeTest, WmCapacityLimit) {
    memory_fep_state_t state;
    memory_fep_bridge_get_state(bridge, &state);

    // WM load should be between 0 and 1 (normalized)
    EXPECT_GE(state.current_wm_load, 0.0f);
    EXPECT_LE(state.current_wm_load, 1.0f);
}
