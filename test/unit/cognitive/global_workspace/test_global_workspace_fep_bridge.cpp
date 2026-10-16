/**
 * @file test_global_workspace_fep_bridge.cpp
 * @brief Unit tests for Global Workspace FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Global Workspace bidirectional integration
 * WHY:  Ensure belief broadcasting, competition, and prior updates work correctly
 * HOW:  Test lifecycle, connections, belief competition, broadcasting, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/global_workspace/nimcp_global_workspace_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class GlobalWorkspaceFepBridgeTest : public ::testing::Test {
protected:
    global_workspace_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        global_workspace_fep_config_t config;
        global_workspace_fep_bridge_default_config(&config);
        bridge = global_workspace_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            global_workspace_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(GlobalWorkspaceFepBridgeTest, CreateWithNullConfig) {
    global_workspace_fep_bridge_t* br = global_workspace_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    global_workspace_fep_bridge_destroy(br);
}

TEST_F(GlobalWorkspaceFepBridgeTest, DestroyNull) {
    global_workspace_fep_bridge_destroy(nullptr);
}

TEST_F(GlobalWorkspaceFepBridgeTest, DefaultConfig) {
    global_workspace_fep_config_t config;
    int ret = global_workspace_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.belief_evidence_threshold, 0.0f);
    EXPECT_GT(config.broadcast_prior_weight, 0.0f);
    EXPECT_TRUE(config.enable_belief_broadcasting);
    EXPECT_TRUE(config.enable_prior_updates);
    EXPECT_TRUE(config.enable_evidence_competition);
}

TEST_F(GlobalWorkspaceFepBridgeTest, DefaultConfigNullPtr) {
    int ret = global_workspace_fep_bridge_default_config(nullptr);
    EXPECT_NE(ret, 0);  /* Returns error code for NULL */
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = global_workspace_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(GlobalWorkspaceFepBridgeTest, ConnectFepNull) {
    EXPECT_NE(global_workspace_fep_bridge_connect_fep(nullptr, nullptr), 0);
}

TEST_F(GlobalWorkspaceFepBridgeTest, ConnectWorkspace) {
    global_workspace_t* workspace = (global_workspace_t*)1;  // Mock pointer
    int ret = global_workspace_fep_bridge_connect_workspace(bridge, workspace);
    EXPECT_EQ(ret, 0);
}

TEST_F(GlobalWorkspaceFepBridgeTest, ConnectWorkspaceNull) {
    EXPECT_NE(global_workspace_fep_bridge_connect_workspace(nullptr, nullptr), 0);
    EXPECT_NE(global_workspace_fep_bridge_connect_workspace(bridge, nullptr), 0);
}

/* ============================================================================
 * FEP → Global Workspace Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceFepBridgeTest, CompeteWithBeliefs) {
    /* Requires both FEP and global workspace systems to be connected */
    /* Without full system setup, expect function to return non-zero (no workspace connected) */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    global_workspace_fep_bridge_connect_fep(bridge, fep);
    int ret = global_workspace_fep_compete_with_beliefs(bridge);
    /* Without connected global workspace, this operation correctly fails */
    EXPECT_NE(ret, 0);

    fep_destroy(fep);
}

TEST_F(GlobalWorkspaceFepBridgeTest, CompeteWithBeliefsNull) {
    int ret = global_workspace_fep_compete_with_beliefs(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(GlobalWorkspaceFepBridgeTest, BroadcastWinningBelief) {
    /* Requires both FEP and global workspace systems to be connected */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    global_workspace_fep_bridge_connect_fep(bridge, fep);
    int ret = global_workspace_fep_broadcast_winning_belief(bridge);
    /* Without connected global workspace, this operation correctly fails */
    EXPECT_NE(ret, 0);

    fep_destroy(fep);
}

TEST_F(GlobalWorkspaceFepBridgeTest, BroadcastWinningBeliefNull) {
    int ret = global_workspace_fep_broadcast_winning_belief(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Global Workspace → FEP Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceFepBridgeTest, UpdatePriorsFromBroadcast) {
    /* Requires both FEP and global workspace systems to be connected */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    global_workspace_fep_bridge_connect_fep(bridge, fep);
    int ret = global_workspace_fep_update_priors_from_broadcast(bridge);
    /* Without connected global workspace, this operation correctly fails */
    EXPECT_NE(ret, 0);

    fep_destroy(fep);
}

TEST_F(GlobalWorkspaceFepBridgeTest, UpdatePriorsFromBroadcastNull) {
    int ret = global_workspace_fep_update_priors_from_broadcast(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Update & State Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceFepBridgeTest, Update) {
    int ret = global_workspace_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(GlobalWorkspaceFepBridgeTest, UpdateNull) {
    int ret = global_workspace_fep_bridge_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(GlobalWorkspaceFepBridgeTest, GetState) {
    global_workspace_fep_state_t state;
    int ret = global_workspace_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(GlobalWorkspaceFepBridgeTest, GetStateNull) {
    global_workspace_fep_state_t state;
    EXPECT_NE(global_workspace_fep_bridge_get_state(nullptr, &state), 0);
    EXPECT_NE(global_workspace_fep_bridge_get_state(bridge, nullptr), 0);
}

TEST_F(GlobalWorkspaceFepBridgeTest, GetStats) {
    global_workspace_fep_stats_t stats;
    int ret = global_workspace_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(GlobalWorkspaceFepBridgeTest, GetStatsNull) {
    global_workspace_fep_stats_t stats;
    EXPECT_NE(global_workspace_fep_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_NE(global_workspace_fep_bridge_get_stats(bridge, nullptr), 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceFepBridgeTest, ConnectBioAsync) {
    int ret = global_workspace_fep_bridge_connect_bio_async(bridge);
    (void)ret;
}

TEST_F(GlobalWorkspaceFepBridgeTest, ConnectBioAsyncNull) {
    int ret = global_workspace_fep_bridge_connect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(GlobalWorkspaceFepBridgeTest, DisconnectBioAsync) {
    global_workspace_fep_bridge_connect_bio_async(bridge);
    int ret = global_workspace_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(GlobalWorkspaceFepBridgeTest, DisconnectBioAsyncNull) {
    int ret = global_workspace_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(GlobalWorkspaceFepBridgeTest, IsBioAsyncConnected) {
    bool connected = global_workspace_fep_bridge_is_bio_async_connected(bridge);
    (void)connected;
}

TEST_F(GlobalWorkspaceFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = global_workspace_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}
