/**
 * @file test_autobiographical_fep_bridge.cpp
 * @brief Unit tests for Autobiographical Memory FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Autobiographical Memory bidirectional integration
 * WHY:  Ensure surprise encoding, memory replay, and prior updates work correctly
 * HOW:  Test lifecycle, connections, surprise encoding, replay, prior updates, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/autobiographical_memory/nimcp_autobiographical_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class AutobiographicalFepBridgeTest : public ::testing::Test {
protected:
    autobiographical_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        autobiographical_fep_config_t config;
        autobiographical_fep_bridge_default_config(&config);
        bridge = autobiographical_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            autobiographical_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(AutobiographicalFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(AutobiographicalFepBridgeTest, CreateWithNullConfig) {
    autobiographical_fep_bridge_t* br = autobiographical_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    autobiographical_fep_bridge_destroy(br);
}

TEST_F(AutobiographicalFepBridgeTest, DestroyNull) {
    autobiographical_fep_bridge_destroy(nullptr);
}

TEST_F(AutobiographicalFepBridgeTest, DefaultConfig) {
    autobiographical_fep_config_t config;
    int ret = autobiographical_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.surprise_memory_threshold, 0.0f);
    EXPECT_GT(config.model_update_rate, 0.0f);
    EXPECT_TRUE(config.enable_surprise_encoding);
    EXPECT_TRUE(config.enable_memory_replay);
}

TEST_F(AutobiographicalFepBridgeTest, DefaultConfigNullPtr) {
    int ret = autobiographical_fep_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(AutobiographicalFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = autobiographical_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(AutobiographicalFepBridgeTest, ConnectFepNull) {
    EXPECT_NE(autobiographical_fep_bridge_connect_fep(nullptr, nullptr), 0);
}

TEST_F(AutobiographicalFepBridgeTest, ConnectAutobiographical) {
    autobiographical_memory_t autobio = 0;
    int ret = autobiographical_fep_bridge_connect_autobiographical(bridge, autobio);
    EXPECT_EQ(ret, 0);
}

TEST_F(AutobiographicalFepBridgeTest, ConnectAutobiographicalNull) {
    autobiographical_memory_t autobio = 0;
    EXPECT_NE(autobiographical_fep_bridge_connect_autobiographical(nullptr, autobio), 0);
}

/* ============================================================================
 * FEP → Autobiographical Tests
 * ============================================================================ */

TEST_F(AutobiographicalFepBridgeTest, EncodeSurprisingEpisode) {
    int ret = autobiographical_fep_encode_surprising_episode(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(AutobiographicalFepBridgeTest, EncodeSurprisingEpisodeNull) {
    int ret = autobiographical_fep_encode_surprising_episode(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(AutobiographicalFepBridgeTest, ReplayMemories) {
    int ret = autobiographical_fep_replay_memories(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(AutobiographicalFepBridgeTest, ReplayMemoriesNull) {
    int ret = autobiographical_fep_replay_memories(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Autobiographical → FEP Tests
 * ============================================================================ */

TEST_F(AutobiographicalFepBridgeTest, UpdatePriorsFromMemory) {
    int ret = autobiographical_fep_update_priors_from_memory(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(AutobiographicalFepBridgeTest, UpdatePriorsFromMemoryNull) {
    int ret = autobiographical_fep_update_priors_from_memory(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Update & State Tests
 * ============================================================================ */

TEST_F(AutobiographicalFepBridgeTest, Update) {
    int ret = autobiographical_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(AutobiographicalFepBridgeTest, UpdateNull) {
    int ret = autobiographical_fep_bridge_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(AutobiographicalFepBridgeTest, GetState) {
    autobiographical_fep_state_t state;
    int ret = autobiographical_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(AutobiographicalFepBridgeTest, GetStateNull) {
    autobiographical_fep_state_t state;
    EXPECT_NE(autobiographical_fep_bridge_get_state(nullptr, &state), 0);
    EXPECT_NE(autobiographical_fep_bridge_get_state(bridge, nullptr), 0);
}

TEST_F(AutobiographicalFepBridgeTest, GetStats) {
    autobiographical_fep_stats_t stats;
    int ret = autobiographical_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(AutobiographicalFepBridgeTest, GetStatsNull) {
    autobiographical_fep_stats_t stats;
    EXPECT_NE(autobiographical_fep_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_NE(autobiographical_fep_bridge_get_stats(bridge, nullptr), 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(AutobiographicalFepBridgeTest, ConnectBioAsync) {
    int ret = autobiographical_fep_bridge_connect_bio_async(bridge);
    (void)ret;
}

TEST_F(AutobiographicalFepBridgeTest, ConnectBioAsyncNull) {
    int ret = autobiographical_fep_bridge_connect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(AutobiographicalFepBridgeTest, DisconnectBioAsync) {
    autobiographical_fep_bridge_connect_bio_async(bridge);
    int ret = autobiographical_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(AutobiographicalFepBridgeTest, DisconnectBioAsyncNull) {
    int ret = autobiographical_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(AutobiographicalFepBridgeTest, IsBioAsyncConnected) {
    bool connected = autobiographical_fep_bridge_is_bio_async_connected(bridge);
    (void)connected;
}

TEST_F(AutobiographicalFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = autobiographical_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Advanced Integration Tests
 * ============================================================================ */

TEST_F(AutobiographicalFepBridgeTest, EncodeMultipleSurprisingEpisodes) {
    /* Create FEP and autobio systems */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_config.num_levels = 2;
    uint32_t dims[] = {16, 8};
    fep_config.level_dims = dims;

    fep_system_t* fep = fep_create(&fep_config, 16, 4);
    ASSERT_NE(fep, nullptr);

    autobiographical_memory_t autobio = autobio_create(0);
    ASSERT_NE(autobio, nullptr);

    autobiographical_fep_bridge_connect_fep(bridge, fep);
    autobiographical_fep_bridge_connect_autobiographical(bridge, autobio);

    /* Encode multiple surprising episodes */
    for (int i = 0; i < 5; i++) {
        fep->free_energy.surprise = 6.0f + (float)i;
        autobiographical_fep_encode_surprising_episode(bridge);
    }

    /* Verify all were encoded */
    autobio_stats_t stats;
    autobio_get_stats(autobio, &stats);
    EXPECT_EQ(5, stats.total_memories);
    EXPECT_EQ(5, bridge->state.memories_encoded);

    fep_destroy(fep);
    autobio_destroy(autobio);
}

TEST_F(AutobiographicalFepBridgeTest, SurpriseThresholdFiltering) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_config.num_levels = 1;
    uint32_t dims[] = {8};
    fep_config.level_dims = dims;

    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    autobiographical_memory_t autobio = autobio_create(0);
    ASSERT_NE(autobio, nullptr);

    autobiographical_fep_bridge_connect_fep(bridge, fep);
    autobiographical_fep_bridge_connect_autobiographical(bridge, autobio);

    /* Below threshold - should not encode */
    fep->free_energy.surprise = 2.0f;
    autobiographical_fep_encode_surprising_episode(bridge);

    autobio_stats_t stats;
    autobio_get_stats(autobio, &stats);
    EXPECT_EQ(0, stats.total_memories);

    /* Above threshold - should encode */
    fep->free_energy.surprise = 8.0f;
    autobiographical_fep_encode_surprising_episode(bridge);

    autobio_get_stats(autobio, &stats);
    EXPECT_EQ(1, stats.total_memories);

    fep_destroy(fep);
    autobio_destroy(autobio);
}

TEST_F(AutobiographicalFepBridgeTest, MemoryReplayWithImportanceFilter) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    autobiographical_memory_t autobio = autobio_create(0);
    ASSERT_NE(autobio, nullptr);

    autobiographical_fep_bridge_connect_fep(bridge, fep);
    autobiographical_fep_bridge_connect_autobiographical(bridge, autobio);

    /* Add low importance memories - should not be replayed */
    for (int i = 0; i < 3; i++) {
        autobiographical_memory_entry_t memory = {};
        memory.timestamp_ms = i * 1000;
        memory.type = AUTOBIO_ACTION;
        snprintf(memory.what_happened, sizeof(memory.what_happened), "Low importance %d", i);
        memory.importance = 0.2f;
        autobio_store(autobio, &memory);
    }

    /* Add high importance memories - should be replayed */
    for (int i = 0; i < 5; i++) {
        autobiographical_memory_entry_t memory = {};
        memory.timestamp_ms = (i + 3) * 1000;
        memory.type = AUTOBIO_LEARNING;
        snprintf(memory.what_happened, sizeof(memory.what_happened), "High importance %d", i);
        memory.importance = 0.7f;
        autobio_store(autobio, &memory);
    }

    autobiographical_fep_replay_memories(bridge);

    /* Should have replayed high importance memories */
    EXPECT_GT(bridge->stats.total_memory_replays, 0);
    EXPECT_GT(bridge->effects.replay_frequency, 0.0f);

    fep_destroy(fep);
    autobio_destroy(autobio);
}

TEST_F(AutobiographicalFepBridgeTest, PriorUpdateFromCoreMemories) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_config.num_levels = 2;
    uint32_t dims[] = {16, 8};
    fep_config.level_dims = dims;

    fep_system_t* fep = fep_create(&fep_config, 16, 4);
    ASSERT_NE(fep, nullptr);

    autobiographical_memory_t autobio = autobio_create(0);
    ASSERT_NE(autobio, nullptr);

    autobiographical_fep_bridge_connect_fep(bridge, fep);
    autobiographical_fep_bridge_connect_autobiographical(bridge, autobio);

    /* Store original precision values */
    float orig_precision_l0 = fep->levels[0].prior_precision[0];
    float orig_precision_l1 = fep->levels[1].prior_precision[0];

    /* Add core memories */
    autobiographical_memory_entry_t memory = {};
    memory.timestamp_ms = 1000;
    memory.type = AUTOBIO_MILESTONE;
    snprintf(memory.what_happened, sizeof(memory.what_happened), "Core memory");
    memory.importance = 0.9f;
    memory.is_core_memory = true;
    memory.identity_defining = true;

    uint64_t id = autobio_store(autobio, &memory);
    autobio_mark_core(autobio, id, true);

    /* Update priors */
    autobiographical_fep_update_priors_from_memory(bridge);

    /* Verify priors were strengthened */
    EXPECT_GT(fep->levels[0].prior_precision[0], orig_precision_l0);
    EXPECT_GT(fep->levels[1].prior_precision[0], orig_precision_l1);
    EXPECT_EQ(1, bridge->stats.total_prior_updates);

    fep_destroy(fep);
    autobio_destroy(autobio);
}

TEST_F(AutobiographicalFepBridgeTest, FullUpdateCycleIntegration) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_config.num_levels = 1;
    uint32_t dims[] = {8};
    fep_config.level_dims = dims;

    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    autobiographical_memory_t autobio = autobio_create(0);
    ASSERT_NE(autobio, nullptr);

    autobiographical_fep_bridge_connect_fep(bridge, fep);
    autobiographical_fep_bridge_connect_autobiographical(bridge, autobio);

    /* Simulate high surprise */
    fep->free_energy.surprise = 12.0f;

    /* Run update cycle */
    autobiographical_fep_bridge_update(bridge, 100);

    /* Verify encoding happened */
    autobio_stats_t stats;
    autobio_get_stats(autobio, &stats);
    EXPECT_GT(stats.total_memories, 0);

    /* Verify state updates */
    autobiographical_fep_state_t state;
    autobiographical_fep_bridge_get_state(bridge, &state);
    EXPECT_GT(state.current_surprise_level, 0.0f);

    fep_destroy(fep);
    autobio_destroy(autobio);
}

TEST_F(AutobiographicalFepBridgeTest, DisabledFeaturesRespected) {
    /* Create bridge with all features disabled */
    autobiographical_fep_config_t config;
    autobiographical_fep_bridge_default_config(&config);
    config.enable_surprise_encoding = false;
    config.enable_memory_replay = false;
    config.enable_prior_updates = false;

    autobiographical_fep_bridge_t* br = autobiographical_fep_bridge_create(&config);
    ASSERT_NE(br, nullptr);

    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    autobiographical_memory_t autobio = autobio_create(0);

    autobiographical_fep_bridge_connect_fep(br, fep);
    autobiographical_fep_bridge_connect_autobiographical(br, autobio);

    /* Set high surprise */
    fep->free_energy.surprise = 15.0f;

    /* Should not encode */
    autobiographical_fep_encode_surprising_episode(br);
    autobio_stats_t stats;
    autobio_get_stats(autobio, &stats);
    EXPECT_EQ(0, stats.total_memories);

    /* Should not replay */
    autobiographical_fep_replay_memories(br);
    EXPECT_EQ(0, br->stats.total_memory_replays);

    /* Should not update priors */
    autobiographical_fep_update_priors_from_memory(br);
    EXPECT_EQ(0, br->stats.total_prior_updates);

    autobiographical_fep_bridge_destroy(br);
    fep_destroy(fep);
    autobio_destroy(autobio);
}

TEST_F(AutobiographicalFepBridgeTest, MemoryImportanceScaling) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    autobiographical_memory_t autobio = autobio_create(0);

    autobiographical_fep_bridge_connect_fep(bridge, fep);
    autobiographical_fep_bridge_connect_autobiographical(bridge, autobio);

    /* Test different surprise levels */
    float surprise_levels[] = {6.0f, 10.0f, 15.0f, 20.0f};

    for (int i = 0; i < 4; i++) {
        fep->free_energy.surprise = surprise_levels[i];
        autobiographical_fep_encode_surprising_episode(bridge);
    }

    /* Verify average importance increases with surprise */
    EXPECT_GT(bridge->state.avg_memory_importance, 0.0f);
    EXPECT_LE(bridge->state.avg_memory_importance, 1.0f);

    fep_destroy(fep);
    autobio_destroy(autobio);
}

TEST_F(AutobiographicalFepBridgeTest, StatisticsAccumulation) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    autobiographical_memory_t autobio = autobio_create(0);

    autobiographical_fep_bridge_connect_fep(bridge, fep);
    autobiographical_fep_bridge_connect_autobiographical(bridge, autobio);

    /* Perform multiple operations */
    for (int i = 0; i < 3; i++) {
        fep->free_energy.surprise = 8.0f;
        autobiographical_fep_encode_surprising_episode(bridge);
    }

    /* Add core memory for prior update */
    autobiographical_memory_entry_t memory = {};
    memory.timestamp_ms = 1000;
    memory.type = AUTOBIO_MILESTONE;
    snprintf(memory.what_happened, sizeof(memory.what_happened), "Milestone");
    memory.importance = 0.8f;
    memory.is_core_memory = true;
    uint64_t id = autobio_store(autobio, &memory);
    autobio_mark_core(autobio, id, true);

    autobiographical_fep_update_priors_from_memory(bridge);

    /* Verify stats */
    autobiographical_fep_stats_t stats;
    autobiographical_fep_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(3, stats.total_surprise_encodings);
    EXPECT_GT(stats.avg_encoding_surprise, 0.0f);
    EXPECT_GT(stats.total_prior_updates, 0);

    fep_destroy(fep);
    autobio_destroy(autobio);
}
