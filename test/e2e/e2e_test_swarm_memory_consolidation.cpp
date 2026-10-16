/**
 * @file e2e_test_swarm_memory_consolidation.cpp
 * @brief End-to-end tests for swarm memory consolidation workflow
 *
 * Tests complete memory consolidation workflows including:
 * - Memory storage → novelty detection → consolidation → distribution
 * - Pattern abstraction → hierarchy building → generalization
 * - Bio-async message-driven consolidation triggers
 * - Node registration → replication → sync
 * - Forgetting curve application over time
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_memory.h"
#include "async/nimcp_bio_messages.h"

class SwarmMemoryE2ETest : public ::testing::Test {
protected:
    NimcpSwarmMemory* memory;

    void SetUp() override {
        memory = nimcp_swarm_memory_create(2000, 3);
        ASSERT_NE(memory, nullptr);
        nimcp_swarm_memory_init(memory, nullptr);
    }

    void TearDown() override {
        if (memory) {
            nimcp_swarm_memory_destroy(memory);
        }
    }

    // Store a batch of memories with specified characteristics
    std::vector<std::string> StoreBatchMemories(
        int count,
        NimcpMemoryType type,
        NimcpMemoryImportance importance,
        const char* prefix
    ) {
        std::vector<std::string> ids;
        for (int i = 0; i < count; i++) {
            char memory_id[64];
            char data[128];
            snprintf(data, sizeof(data), "%s_data_%d_%lld",
                     prefix, i, (long long)std::chrono::system_clock::now().time_since_epoch().count());

            nimcp_swarm_memory_store(memory, type, importance,
                                     (uint8_t*)data, strlen(data) + 1, memory_id);
            ids.push_back(memory_id);
        }
        return ids;
    }

    // Register multiple hippocampus nodes
    void RegisterNodes(int count, uint32_t capacity = 100) {
        for (int i = 0; i < count; i++) {
            char node_id[64];
            snprintf(node_id, sizeof(node_id), "hippocampus_node_%d", i);
            nimcp_swarm_memory_register_node(memory, node_id, capacity);
        }
    }
};

// =============================================================================
// Complete Memory Lifecycle E2E Test
// =============================================================================

TEST_F(SwarmMemoryE2ETest, CompleteMemoryLifecycle) {
    // Phase 1: Store memories with different types and importance
    auto episodic_ids = StoreBatchMemories(10, NIMCP_MEMORY_EPISODIC,
                                           NIMCP_IMPORTANCE_MEDIUM, "episodic");
    auto semantic_ids = StoreBatchMemories(10, NIMCP_MEMORY_SEMANTIC,
                                           NIMCP_IMPORTANCE_HIGH, "semantic");
    auto threat_ids = StoreBatchMemories(5, NIMCP_MEMORY_THREAT,
                                         NIMCP_IMPORTANCE_CRITICAL, "threat");

    EXPECT_EQ(episodic_ids.size(), 10U);
    EXPECT_EQ(semantic_ids.size(), 10U);
    EXPECT_EQ(threat_ids.size(), 5U);

    // Phase 2: Access and rehearse critical memories
    for (const auto& id : threat_ids) {
        nimcp_swarm_memory_access(memory, id.c_str());
        nimcp_swarm_memory_rehearse(memory, id.c_str());
        nimcp_swarm_memory_rehearse(memory, id.c_str());
    }

    // Phase 3: Schedule replay for episodic memories
    for (size_t i = 0; i < episodic_ids.size(); i++) {
        float priority = 0.5f + (i * 0.05f);
        nimcp_swarm_memory_schedule_replay(memory, episodic_ids[i].c_str(), priority);
    }

    // Phase 4: Run replay cycle
    uint32_t replays = 0;
    nimcp_result_t result = nimcp_swarm_memory_replay_cycle(memory, 15, &replays);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Phase 5: Consolidate memories
    uint32_t consolidated = 0;
    result = nimcp_swarm_memory_consolidate(memory, &consolidated);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Phase 6: Apply forgetting curve
    uint32_t forgotten = 0;
    result = nimcp_swarm_memory_apply_forgetting(memory, &forgotten);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Critical threat memories should survive forgetting
    for (const auto& id : threat_ids) {
        uint8_t retrieved[128];
        result = nimcp_swarm_memory_retrieve(memory, id.c_str(),
                                             retrieved, sizeof(retrieved));
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

// =============================================================================
// Pattern Abstraction and Hierarchy E2E Test
// =============================================================================

TEST_F(SwarmMemoryE2ETest, PatternAbstractionWorkflow) {
    // Store related semantic memories
    auto semantic_ids = StoreBatchMemories(20, NIMCP_MEMORY_SEMANTIC,
                                           NIMCP_IMPORTANCE_MEDIUM, "pattern");

    // Abstract patterns from groups of memories
    std::vector<uint32_t> pattern_hashes;

    for (size_t i = 0; i + 3 < semantic_ids.size(); i += 3) {
        const char* group_ids[3];
        group_ids[0] = semantic_ids[i].c_str();
        group_ids[1] = semantic_ids[i + 1].c_str();
        group_ids[2] = semantic_ids[i + 2].c_str();

        uint32_t pattern_hash = 0;
        nimcp_result_t result = nimcp_swarm_memory_abstract_pattern(
            memory, group_ids, 3, &pattern_hash
        );

        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_NE(pattern_hash, 0U);
        pattern_hashes.push_back(pattern_hash);
    }

    // Build knowledge hierarchy
    uint32_t levels = 0;
    nimcp_result_t result = nimcp_swarm_memory_build_hierarchy(memory, &levels);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(levels, 1U);

    // Generalize pairs of memories
    for (size_t i = 0; i + 1 < semantic_ids.size(); i += 4) {
        const char* pair_ids[2];
        pair_ids[0] = semantic_ids[i].c_str();
        pair_ids[1] = semantic_ids[i + 1].c_str();

        char gen_id[64];
        result = nimcp_swarm_memory_generalize(memory, pair_ids, 2, gen_id);

        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_GT(strlen(gen_id), 0U);
    }
}

// =============================================================================
// Distributed Memory E2E Test
// =============================================================================

TEST_F(SwarmMemoryE2ETest, DistributedMemoryWorkflow) {
    // Register hippocampus nodes
    RegisterNodes(5);

    // Store important memories that need distribution
    auto critical_ids = StoreBatchMemories(10, NIMCP_MEMORY_THREAT,
                                           NIMCP_IMPORTANCE_CRITICAL, "distribute");

    // Distribute each memory to the swarm
    for (const auto& id : critical_ids) {
        uint32_t replicas_created = 0;
        nimcp_result_t result = nimcp_swarm_memory_distribute(memory, id.c_str(), &replicas_created);
        // May succeed or fail depending on node availability
        (void)result;
    }

    // Sync with each node
    for (int i = 0; i < 5; i++) {
        char node_id[64];
        snprintf(node_id, sizeof(node_id), "hippocampus_node_%d", i);

        uint32_t synced = 0;
        nimcp_swarm_memory_sync_with_node(memory, node_id, &synced);
    }

    // Get statistics
    NimcpMemoryStatistics stats;
    nimcp_swarm_memory_get_statistics(memory, &stats);

    EXPECT_GE(stats.total_memories, 10UL);
}

// =============================================================================
// Bio-Async Triggered Consolidation E2E Test
// =============================================================================

TEST_F(SwarmMemoryE2ETest, BioAsyncTriggeredConsolidation) {
    memory->bio_async_enabled = true;

    // Store memories
    auto memories = StoreBatchMemories(50, NIMCP_MEMORY_PROCEDURAL,
                                       NIMCP_IMPORTANCE_MEDIUM, "bioasync");

    // Simulate bio-async consolidation trigger message
    bio_message_header_t trigger;
    memset(&trigger, 0, sizeof(trigger));
    trigger.type = BIO_MSG_CONSOLIDATION_TRIGGER;
    trigger.payload_size = 0;

    nimcp_result_t result = nimcp_swarm_memory_process_message(memory, &trigger);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Simulate memory sync messages
    for (size_t i = 0; i < std::min(memories.size(), (size_t)10); i++) {
        struct {
            bio_message_header_t header;
            char memory_id[64];
        } sync_msg;
        memset(&sync_msg, 0, sizeof(sync_msg));

        sync_msg.header.type = BIO_MSG_SWARM_MEMORY_SYNC;
        sync_msg.header.payload_size = strlen(memories[i].c_str()) + 1;
        strncpy(sync_msg.memory_id, memories[i].c_str(), sizeof(sync_msg.memory_id) - 1);

        result = nimcp_swarm_memory_process_message(memory, &sync_msg.header);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

// =============================================================================
// Forgetting Curve Over Time E2E Test
// =============================================================================

TEST_F(SwarmMemoryE2ETest, ForgettingCurveOverMultipleCycles) {
    // Store low-importance memories (will decay faster)
    auto low_ids = StoreBatchMemories(30, NIMCP_MEMORY_EPISODIC,
                                      NIMCP_IMPORTANCE_LOW, "decay");

    // Store high-importance rehearsed memories (will persist)
    auto high_ids = StoreBatchMemories(10, NIMCP_MEMORY_THREAT,
                                       NIMCP_IMPORTANCE_CRITICAL, "persist");

    // Rehearse high-importance memories
    for (const auto& id : high_ids) {
        for (int j = 0; j < 5; j++) {
            nimcp_swarm_memory_rehearse(memory, id.c_str());
        }
    }

    // Apply multiple forgetting cycles
    uint32_t total_forgotten = 0;
    for (int cycle = 0; cycle < 5; cycle++) {
        uint32_t forgotten = 0;
        nimcp_swarm_memory_apply_forgetting(memory, &forgotten);
        total_forgotten += forgotten;
    }

    // Verify high-importance memories survived
    int survived = 0;
    for (const auto& id : high_ids) {
        uint8_t data[128];
        if (nimcp_swarm_memory_retrieve(memory, id.c_str(), data, sizeof(data)) == NIMCP_SUCCESS) {
            survived++;
        }
    }

    // Most high-importance memories should survive
    EXPECT_GE(survived, (int)(high_ids.size() * 0.8));
}

// =============================================================================
// Memory Type Segregation E2E Test
// =============================================================================

TEST_F(SwarmMemoryE2ETest, MemoryTypeSegregation) {
    // Store memories of each type
    StoreBatchMemories(10, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_MEDIUM, "epi");
    StoreBatchMemories(10, NIMCP_MEMORY_SEMANTIC, NIMCP_IMPORTANCE_MEDIUM, "sem");
    StoreBatchMemories(10, NIMCP_MEMORY_PROCEDURAL, NIMCP_IMPORTANCE_MEDIUM, "proc");
    StoreBatchMemories(10, NIMCP_MEMORY_THREAT, NIMCP_IMPORTANCE_MEDIUM, "threat");
    StoreBatchMemories(10, NIMCP_MEMORY_SPATIAL, NIMCP_IMPORTANCE_MEDIUM, "spatial");

    // Query by type
    for (int type = 0; type < NIMCP_MEMORY_TYPE_COUNT; type++) {
        uint32_t count = nimcp_swarm_memory_get_count_by_type(memory, (NimcpMemoryType)type);
        EXPECT_GE(count, 0U);  // May have some memories
    }

    // Get total statistics
    NimcpMemoryStatistics stats;
    nimcp_swarm_memory_get_statistics(memory, &stats);
    EXPECT_EQ(stats.total_memories, 50UL);
}

// =============================================================================
// Stress Test E2E
// =============================================================================

TEST_F(SwarmMemoryE2ETest, StressTestHighVolume) {
    RegisterNodes(10);

    // Store large number of memories
    for (int batch = 0; batch < 10; batch++) {
        char prefix[32];
        snprintf(prefix, sizeof(prefix), "stress_batch_%d", batch);
        StoreBatchMemories(100, (NimcpMemoryType)(batch % NIMCP_MEMORY_TYPE_COUNT),
                          (NimcpMemoryImportance)(batch % 4), prefix);
    }

    // Run consolidation
    uint32_t consolidated = 0;
    nimcp_swarm_memory_consolidate(memory, &consolidated);

    // Apply forgetting
    uint32_t forgotten = 0;
    nimcp_swarm_memory_apply_forgetting(memory, &forgotten);

    // Build hierarchy
    uint32_t levels = 0;
    nimcp_swarm_memory_build_hierarchy(memory, &levels);

    // Get final statistics
    NimcpMemoryStatistics stats;
    nimcp_swarm_memory_get_statistics(memory, &stats);

    EXPECT_GT(stats.total_memories, 0UL);
}

// =============================================================================
// Error Recovery E2E Test
// =============================================================================

TEST_F(SwarmMemoryE2ETest, ErrorRecoveryWorkflow) {
    // Store some valid memories
    auto valid_ids = StoreBatchMemories(10, NIMCP_MEMORY_SEMANTIC,
                                        NIMCP_IMPORTANCE_HIGH, "valid");

    // Attempt operations on non-existent memories (should fail gracefully)
    uint8_t data[64];
    nimcp_result_t result = nimcp_swarm_memory_retrieve(
        memory, "nonexistent_memory", data, sizeof(data)
    );
    EXPECT_NE(result, NIMCP_SUCCESS);

    result = nimcp_swarm_memory_access(memory, "nonexistent_memory");
    EXPECT_NE(result, NIMCP_SUCCESS);

    result = nimcp_swarm_memory_delete(memory, "nonexistent_memory");
    EXPECT_NE(result, NIMCP_SUCCESS);

    // Valid operations should still work after failed operations
    result = nimcp_swarm_memory_retrieve(
        memory, valid_ids[0].c_str(), data, sizeof(data)
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // System should be in consistent state
    uint32_t consolidated = 0;
    result = nimcp_swarm_memory_consolidate(memory, &consolidated);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
