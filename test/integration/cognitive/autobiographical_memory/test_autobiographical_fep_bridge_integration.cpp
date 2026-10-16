/**
 * @file test_autobiographical_fep_bridge_integration.cpp
 * @brief Integration tests for autobiographical-FEP bridge with real FEP and memory systems
 *
 * WHAT: Test end-to-end integration of FEP surprise encoding and memory-based prior updates
 * WHY:  Verify bidirectional integration works correctly with actual subsystems
 * HOW:  Create real FEP and autobiographical memory systems, test full interaction cycles
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/autobiographical_memory/nimcp_autobiographical_fep_bridge.h"
#include "cognitive/nimcp_autobiographical_memory.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class AutobiographicalFepBridgeIntegrationTest : public ::testing::Test {
protected:
    autobiographical_fep_bridge_t* bridge;
    fep_system_t* fep;
    autobiographical_memory_t autobio;

    void SetUp() override {
        /* Create bridge */
        autobiographical_fep_config_t config;
        autobiographical_fep_bridge_default_config(&config);
        bridge = autobiographical_fep_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);

        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_config.num_levels = 3;
        uint32_t dims[] = {32, 16, 8};
        fep_config.level_dims = dims;
        fep = fep_create(&fep_config, 32, 8);
        ASSERT_NE(nullptr, fep);

        /* Create autobiographical memory */
        autobio = autobio_create(1000);
        ASSERT_NE(nullptr, autobio);

        /* Connect all systems */
        autobiographical_fep_bridge_connect_fep(bridge, fep);
        autobiographical_fep_bridge_connect_autobiographical(bridge, autobio);
    }

    void TearDown() override {
        if (bridge) autobiographical_fep_bridge_destroy(bridge);
        if (fep) fep_destroy(fep);
        if (autobio) autobio_destroy(autobio);
    }
};

/* ============================================================================
 * Surprise-Driven Memory Encoding Tests
 * ============================================================================ */

TEST_F(AutobiographicalFepBridgeIntegrationTest, HighSurpriseCreatesMemory) {
    /* Set FEP to high surprise state */
    fep->free_energy.surprise = 12.0f;
    fep->free_energy.total = 15.0f;

    /* Encode episode */
    int ret = autobiographical_fep_encode_surprising_episode(bridge);
    EXPECT_EQ(0, ret);

    /* Verify memory was created */
    autobio_stats_t stats;
    autobio_get_stats(autobio, &stats);
    EXPECT_EQ(1, stats.total_memories);

    /* Verify it's a learning-type memory */
    autobiographical_memory_entry_t memory;
    uint32_t found = 0;
    autobio_get_recent(autobio, 1, &memory, &found);
    ASSERT_GE(found, 1u);
    EXPECT_EQ(AUTOBIO_LEARNING, memory.type);
    EXPECT_GT(memory.importance, 0.0f);
}

TEST_F(AutobiographicalFepBridgeIntegrationTest, LowSurpriseNoMemory) {
    /* Set FEP to low surprise */
    fep->free_energy.surprise = 1.0f;

    /* Attempt encode */
    int ret = autobiographical_fep_encode_surprising_episode(bridge);
    EXPECT_EQ(0, ret);

    /* No memory should be created */
    autobio_stats_t stats;
    autobio_get_stats(autobio, &stats);
    EXPECT_EQ(0, stats.total_memories);
}

TEST_F(AutobiographicalFepBridgeIntegrationTest, VeryHighSurpriseIdentityDefining) {
    /* Extremely high surprise */
    fep->free_energy.surprise = 25.0f;

    autobiographical_fep_encode_surprising_episode(bridge);

    /* Check memory properties */
    autobiographical_memory_entry_t memory;
    uint32_t found = 0;
    autobio_get_recent(autobio, 1, &memory, &found);
    ASSERT_EQ(1, found);

    /* Should be identity-defining or core */
    EXPECT_TRUE(memory.identity_defining || memory.is_core_memory);
    EXPECT_GT(memory.importance, 0.7f);
}

TEST_F(AutobiographicalFepBridgeIntegrationTest, SurpriseSequenceCreatesTimeline) {
    /* Simulate sequence of surprising events */
    float surprises[] = {7.0f, 3.0f, 12.0f, 5.0f, 15.0f};

    for (int i = 0; i < 5; i++) {
        fep->free_energy.surprise = surprises[i];
        autobiographical_fep_encode_surprising_episode(bridge);
    }

    /* Events at or above threshold (5.0) should be encoded: 7.0, 12.0, 5.0, 15.0 */
    autobio_stats_t stats;
    autobio_get_stats(autobio, &stats);
    EXPECT_EQ(4, stats.total_memories);
}

/* ============================================================================
 * Memory-Driven Prior Update Tests
 * ============================================================================ */

TEST_F(AutobiographicalFepBridgeIntegrationTest, CoreMemoriesStrengthenPriors) {
    /* Add core autobiographical memories */
    for (int i = 0; i < 3; i++) {
        autobiographical_memory_entry_t memory = {};
        memory.timestamp_ms = i * 1000;
        memory.type = AUTOBIO_MILESTONE;
        snprintf(memory.what_happened, sizeof(memory.what_happened),
            "Core event %d", i);
        memory.importance = 0.9f;
        memory.is_core_memory = true;
        memory.identity_defining = true;

        uint64_t id = autobio_store(autobio, &memory);
        autobio_mark_core(autobio, id, true);
    }

    /* Store original precision */
    float orig_precision[3];
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        orig_precision[l] = fep->levels[l].prior_precision[0];
    }

    /* Update priors from memories */
    int ret = autobiographical_fep_update_priors_from_memory(bridge);
    EXPECT_EQ(0, ret);

    /* Verify all levels have strengthened priors */
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        EXPECT_GT(fep->levels[l].prior_precision[0], orig_precision[l]);
    }

    /* Verify stats */
    EXPECT_EQ(1, bridge->stats.total_prior_updates);
    EXPECT_GT(bridge->effects.model_prior_adjustment, 0.0f);
}

TEST_F(AutobiographicalFepBridgeIntegrationTest, NoMemoriesNoUpdate) {
    /* Store original precision */
    float orig_precision = fep->levels[0].prior_precision[0];

    /* Update with no memories */
    int ret = autobiographical_fep_update_priors_from_memory(bridge);
    EXPECT_EQ(0, ret);

    /* Precision should be unchanged */
    EXPECT_EQ(orig_precision, fep->levels[0].prior_precision[0]);
    EXPECT_EQ(0, bridge->stats.total_prior_updates);
}

TEST_F(AutobiographicalFepBridgeIntegrationTest, MemoryImportanceAffectsPriorStrength) {
    /* Add low importance core memory */
    autobiographical_memory_entry_t low_mem = {};
    low_mem.timestamp_ms = 1000;
    low_mem.type = AUTOBIO_MILESTONE;
    snprintf(low_mem.what_happened, sizeof(low_mem.what_happened), "Low importance");
    low_mem.importance = 0.3f;
    low_mem.is_core_memory = true;
    uint64_t id1 = autobio_store(autobio, &low_mem);
    autobio_mark_core(autobio, id1, true);

    float orig_precision = fep->levels[0].prior_precision[0];
    autobiographical_fep_update_priors_from_memory(bridge);
    float low_adjustment = bridge->effects.model_prior_adjustment;

    /* Reset FEP and bridge */
    TearDown();
    SetUp();

    /* Add high importance core memory */
    autobiographical_memory_entry_t high_mem = {};
    high_mem.timestamp_ms = 2000;
    high_mem.type = AUTOBIO_MILESTONE;
    snprintf(high_mem.what_happened, sizeof(high_mem.what_happened), "High importance");
    high_mem.importance = 0.9f;
    high_mem.is_core_memory = true;
    uint64_t id2 = autobio_store(autobio, &high_mem);
    autobio_mark_core(autobio, id2, true);

    autobiographical_fep_update_priors_from_memory(bridge);
    float high_adjustment = bridge->effects.model_prior_adjustment;

    /* High importance should produce larger adjustment */
    EXPECT_GT(high_adjustment, low_adjustment);
}

/* ============================================================================
 * Memory Replay Tests
 * ============================================================================ */

TEST_F(AutobiographicalFepBridgeIntegrationTest, ReplayHighImportanceMemories) {
    /* Add mix of low and high importance memories */
    for (int i = 0; i < 10; i++) {
        autobiographical_memory_entry_t memory = {};
        memory.timestamp_ms = i * 1000;
        memory.type = (i % 2 == 0) ? AUTOBIO_LEARNING : AUTOBIO_ACTION;
        snprintf(memory.what_happened, sizeof(memory.what_happened),
            "Memory %d", i);
        /* Alternate between low (0.3) and high (0.7) importance */
        memory.importance = (i % 2 == 0) ? 0.7f : 0.3f;
        autobio_store(autobio, &memory);
    }

    /* Replay memories */
    int ret = autobiographical_fep_replay_memories(bridge);
    EXPECT_EQ(0, ret);

    /* Should have replayed high-importance memories (5 of them) */
    EXPECT_GT(bridge->stats.total_memory_replays, 0);
    EXPECT_LE(bridge->stats.total_memory_replays, 10);  /* Max 10 replayed */
    EXPECT_GT(bridge->effects.replay_frequency, 0.0f);
}

TEST_F(AutobiographicalFepBridgeIntegrationTest, ReplayEmptyMemorySystem) {
    /* Replay with no memories */
    int ret = autobiographical_fep_replay_memories(bridge);
    EXPECT_EQ(0, ret);

    /* Should succeed but replay nothing */
    EXPECT_EQ(0, bridge->stats.total_memory_replays);
    EXPECT_EQ(0.0f, bridge->effects.replay_frequency);
}

/* ============================================================================
 * Full Integration Cycle Tests
 * ============================================================================ */

TEST_F(AutobiographicalFepBridgeIntegrationTest, FullCycleSurpriseToMemoryToPrior) {
    /* 1. FEP experiences high surprise */
    fep->free_energy.surprise = 18.0f;

    /* 2. Encode as memory */
    autobiographical_fep_encode_surprising_episode(bridge);

    /* Verify memory created */
    autobio_stats_t stats;
    autobio_get_stats(autobio, &stats);
    ASSERT_EQ(1, stats.total_memories);

    /* Get the memory and mark as core */
    autobiographical_memory_entry_t memory;
    uint32_t found = 0;
    autobio_get_recent(autobio, 1, &memory, &found);
    ASSERT_EQ(1, found);

    autobio_mark_core(autobio, memory.memory_id, true);

    /* 3. Update priors from this new core memory */
    float orig_precision = fep->levels[0].prior_precision[0];
    autobiographical_fep_update_priors_from_memory(bridge);

    /* Verify prior was updated */
    EXPECT_GT(fep->levels[0].prior_precision[0], orig_precision);
    EXPECT_EQ(1, bridge->stats.total_prior_updates);
}

TEST_F(AutobiographicalFepBridgeIntegrationTest, UpdateCyclePerformsAllOperations) {
    /* Set high surprise */
    fep->free_energy.surprise = 10.0f;

    /* Add some existing memories */
    for (int i = 0; i < 3; i++) {
        autobiographical_memory_entry_t memory = {};
        memory.timestamp_ms = i * 1000;
        memory.type = AUTOBIO_LEARNING;
        snprintf(memory.what_happened, sizeof(memory.what_happened), "Prior %d", i);
        memory.importance = 0.6f;
        autobio_store(autobio, &memory);
    }

    /* Run update cycle */
    int ret = autobiographical_fep_bridge_update(bridge, 100);
    EXPECT_EQ(0, ret);

    /* Verify encoding occurred */
    autobio_stats_t stats;
    autobio_get_stats(autobio, &stats);
    EXPECT_GT(stats.total_memories, 3);  /* New memory added */

    /* Verify state was updated */
    autobiographical_fep_state_t state;
    autobiographical_fep_bridge_get_state(bridge, &state);
    EXPECT_GT(state.current_surprise_level, 0.0f);
}

TEST_F(AutobiographicalFepBridgeIntegrationTest, MultipleUpdateCycles) {
    /* Run multiple update cycles with varying surprise */
    float surprises[] = {8.0f, 2.0f, 15.0f, 4.0f, 20.0f};

    for (int i = 0; i < 5; i++) {
        fep->free_energy.surprise = surprises[i];
        autobiographical_fep_bridge_update(bridge, 100);
    }

    /* Verify state reflects multiple operations */
    autobiographical_fep_state_t state;
    autobiographical_fep_bridge_get_state(bridge, &state);

    autobiographical_fep_stats_t stats;
    autobiographical_fep_bridge_get_stats(bridge, &stats);

    /* Should have encoded high-surprise episodes */
    EXPECT_GT(stats.total_surprise_encodings, 0);
    EXPECT_GT(stats.avg_encoding_surprise, 0.0f);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(AutobiographicalFepBridgeIntegrationTest, BioAsyncConnectionLifecycle) {
    /* Connect */
    int ret = autobiographical_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(0, ret);

    /* May or may not actually connect depending on router availability */
    /* Just verify no crash */

    /* Disconnect */
    ret = autobiographical_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(autobiographical_fep_bridge_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(AutobiographicalFepBridgeIntegrationTest, MaxSurpriseHandling) {
    /* Set extremely high surprise */
    fep->free_energy.surprise = 1000.0f;

    /* Should handle gracefully */
    int ret = autobiographical_fep_encode_surprising_episode(bridge);
    EXPECT_EQ(0, ret);

    /* Memory should be created with clamped importance */
    autobiographical_memory_entry_t memory;
    uint32_t found = 0;
    autobio_get_recent(autobio, 1, &memory, &found);
    ASSERT_EQ(1, found);

    /* Importance should be clamped to [0, 1] */
    EXPECT_GE(memory.importance, 0.0f);
    EXPECT_LE(memory.importance, 1.0f);
}

TEST_F(AutobiographicalFepBridgeIntegrationTest, ZeroSurpriseHandling) {
    fep->free_energy.surprise = 0.0f;

    /* Should not create memory */
    autobiographical_fep_encode_surprising_episode(bridge);

    autobio_stats_t stats;
    autobio_get_stats(autobio, &stats);
    EXPECT_EQ(0, stats.total_memories);
}

TEST_F(AutobiographicalFepBridgeIntegrationTest, LargeBatchMemoryProcessing) {
    /* Create many memories */
    for (int i = 0; i < 100; i++) {
        autobiographical_memory_entry_t memory = {};
        memory.timestamp_ms = i * 1000;
        memory.type = AUTOBIO_LEARNING;
        snprintf(memory.what_happened, sizeof(memory.what_happened), "Event %d", i);
        memory.importance = (i % 10 == 0) ? 0.8f : 0.3f;  /* 10% high importance */
        memory.is_core_memory = (i % 20 == 0);  /* 5% core */
        uint64_t id = autobio_store(autobio, &memory);
        if (memory.is_core_memory) {
            autobio_mark_core(autobio, id, true);
        }
    }

    /* Replay should handle large set */
    int ret = autobiographical_fep_replay_memories(bridge);
    EXPECT_EQ(0, ret);

    /* Prior update should handle multiple core memories */
    ret = autobiographical_fep_update_priors_from_memory(bridge);
    EXPECT_EQ(0, ret);

    /* Should not crash and should update stats */
    EXPECT_GT(bridge->stats.total_memory_replays, 0);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
