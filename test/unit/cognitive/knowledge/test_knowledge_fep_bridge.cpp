/**
 * @file test_knowledge_fep_bridge.cpp
 * @brief Unit tests for Knowledge FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Knowledge bidirectional integration
 * WHY:  Ensure knowledge updates from PE and semantic priors work correctly
 * HOW:  Test lifecycle, connections, knowledge updates, semantic priors, and bio-async integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/knowledge/nimcp_knowledge_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class KnowledgeFepBridgeTest : public ::testing::Test {
protected:
    knowledge_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        knowledge_fep_config_t config;
        knowledge_fep_bridge_default_config(&config);
        bridge = knowledge_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            knowledge_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(KnowledgeFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(KnowledgeFepBridgeTest, CreateWithNullConfig) {
    knowledge_fep_bridge_t* br = knowledge_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    knowledge_fep_bridge_destroy(br);
}

TEST_F(KnowledgeFepBridgeTest, DestroyNull) {
    knowledge_fep_bridge_destroy(nullptr);
}

TEST_F(KnowledgeFepBridgeTest, DefaultConfig) {
    knowledge_fep_config_t config;
    int ret = knowledge_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.knowledge_update_threshold, 0.0f);
    EXPECT_GT(config.semantic_prior_weight, 0.0f);
    EXPECT_TRUE(config.enable_knowledge_updates);
    EXPECT_TRUE(config.enable_semantic_priors);
}

TEST_F(KnowledgeFepBridgeTest, DefaultConfigNullPtr) {
    int ret = knowledge_fep_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(KnowledgeFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = knowledge_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(KnowledgeFepBridgeTest, ConnectFepNull) {
    EXPECT_NE(knowledge_fep_bridge_connect_fep(nullptr, nullptr), 0);

    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);

    EXPECT_NE(knowledge_fep_bridge_connect_fep(nullptr, fep), 0);
    EXPECT_NE(knowledge_fep_bridge_connect_fep(bridge, nullptr), 0);

    fep_destroy(fep);
}

TEST_F(KnowledgeFepBridgeTest, ConnectKnowledge) {
    knowledge_system_t knowledge = 0;
    int ret = knowledge_fep_bridge_connect_knowledge(bridge, knowledge);
    EXPECT_EQ(ret, 0);
}

TEST_F(KnowledgeFepBridgeTest, ConnectKnowledgeNull) {
    knowledge_system_t knowledge = 0;
    EXPECT_NE(knowledge_fep_bridge_connect_knowledge(nullptr, knowledge), 0);
}

/* ============================================================================
 * FEP → Knowledge Tests
 * ============================================================================ */

TEST_F(KnowledgeFepBridgeTest, UpdateKnowledge) {
    int ret = knowledge_fep_update_knowledge(bridge, 5.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(KnowledgeFepBridgeTest, UpdateKnowledgeNull) {
    int ret = knowledge_fep_update_knowledge(nullptr, 5.0f);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Knowledge → FEP Tests
 * ============================================================================ */

TEST_F(KnowledgeFepBridgeTest, ApplySemanticPriors) {
    int ret = knowledge_fep_apply_semantic_priors(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(KnowledgeFepBridgeTest, ApplySemanticPriorsNull) {
    int ret = knowledge_fep_apply_semantic_priors(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Update & State Tests
 * ============================================================================ */

TEST_F(KnowledgeFepBridgeTest, Update) {
    int ret = knowledge_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(KnowledgeFepBridgeTest, UpdateNull) {
    int ret = knowledge_fep_bridge_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(KnowledgeFepBridgeTest, GetState) {
    knowledge_fep_state_t state;
    int ret = knowledge_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(KnowledgeFepBridgeTest, GetStateNull) {
    knowledge_fep_state_t state;
    EXPECT_NE(knowledge_fep_bridge_get_state(nullptr, &state), 0);
    EXPECT_NE(knowledge_fep_bridge_get_state(bridge, nullptr), 0);
}

TEST_F(KnowledgeFepBridgeTest, GetStats) {
    knowledge_fep_stats_t stats;
    int ret = knowledge_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(KnowledgeFepBridgeTest, GetStatsNull) {
    knowledge_fep_stats_t stats;
    EXPECT_NE(knowledge_fep_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_NE(knowledge_fep_bridge_get_stats(bridge, nullptr), 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(KnowledgeFepBridgeTest, ConnectBioAsync) {
    int ret = knowledge_fep_bridge_connect_bio_async(bridge);
    (void)ret;
}

TEST_F(KnowledgeFepBridgeTest, ConnectBioAsyncNull) {
    int ret = knowledge_fep_bridge_connect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(KnowledgeFepBridgeTest, DisconnectBioAsync) {
    knowledge_fep_bridge_connect_bio_async(bridge);
    int ret = knowledge_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(KnowledgeFepBridgeTest, DisconnectBioAsyncNull) {
    int ret = knowledge_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(KnowledgeFepBridgeTest, IsBioAsyncConnected) {
    bool connected = knowledge_fep_bridge_is_bio_async_connected(bridge);
    (void)connected;
}

TEST_F(KnowledgeFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = knowledge_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}
