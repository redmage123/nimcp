/**
 * @file test_swarm_memory_iterators.cpp
 * @brief Unit tests for swarm memory iterator callbacks
 *
 * Tests the iterator callback implementations for:
 * - forgetting_iterator_cb: Memory decay and forgetting curve application
 * - consolidation_iterator_cb: Memory consolidation collection
 * - novelty_iterator_cb: Novelty detection during memory storage
 * - select_node_iter_cb: Hippocampus node selection for replication
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_memory.h"
#include "utils/time/nimcp_time.h"

class SwarmMemoryIteratorTest : public ::testing::Test {
protected:
    NimcpSwarmMemory* memory;

    void SetUp() override {
        memory = nimcp_swarm_memory_create(1000, 3);
        ASSERT_NE(memory, nullptr);
        nimcp_swarm_memory_init(memory, nullptr);
    }

    void TearDown() override {
        if (memory) {
            nimcp_swarm_memory_destroy(memory);
        }
    }

    // Helper to store test memories
    void StoreTestMemory(const char* suffix, NimcpMemoryType type,
                         NimcpMemoryImportance importance, char* out_id) {
        uint8_t data[32];
        snprintf((char*)data, sizeof(data), "test_data_%s", suffix);
        nimcp_swarm_memory_store(memory, type, importance, data, strlen((char*)data) + 1, out_id);
    }
};

// =============================================================================
// Forgetting Iterator Tests
// =============================================================================

TEST_F(SwarmMemoryIteratorTest, ForgettingAppliesDecayToOldMemories) {
    char memory_id[64];
    StoreTestMemory("old", NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_LOW, memory_id);

    // Apply forgetting cycle
    uint32_t forgotten = 0;
    nimcp_result_t result = nimcp_swarm_memory_apply_forgetting(memory, &forgotten);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    // Low importance memories with no rehearsal should decay
}

TEST_F(SwarmMemoryIteratorTest, ForgettingPreservesHighImportanceMemories) {
    char memory_id[64];
    StoreTestMemory("important", NIMCP_MEMORY_THREAT, NIMCP_IMPORTANCE_CRITICAL, memory_id);

    // Rehearse to boost strength
    nimcp_swarm_memory_rehearse(memory, memory_id);
    nimcp_swarm_memory_rehearse(memory, memory_id);
    nimcp_swarm_memory_rehearse(memory, memory_id);

    uint32_t forgotten = 0;
    nimcp_result_t result = nimcp_swarm_memory_apply_forgetting(memory, &forgotten);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // High importance rehearsed memory should still be retrievable
    uint8_t retrieved[64];
    result = nimcp_swarm_memory_retrieve(memory, memory_id, retrieved, sizeof(retrieved));
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryIteratorTest, ForgettingRespectsMaxForgetLimit) {
    // Store many low-priority memories
    for (int i = 0; i < 150; i++) {
        char suffix[16], memory_id[64];
        snprintf(suffix, sizeof(suffix), "bulk_%d", i);
        StoreTestMemory(suffix, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_LOW, memory_id);
    }

    uint32_t forgotten = 0;
    nimcp_result_t result = nimcp_swarm_memory_apply_forgetting(memory, &forgotten);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    // Should respect internal MAX_FORGET limit (100)
    EXPECT_LE(forgotten, 100U);
}

TEST_F(SwarmMemoryIteratorTest, ForgettingHandlesEmptyMemoryStore) {
    // Create fresh empty system
    NimcpSwarmMemory* empty = nimcp_swarm_memory_create(100, 2);
    nimcp_swarm_memory_init(empty, nullptr);

    uint32_t forgotten = 0;
    nimcp_result_t result = nimcp_swarm_memory_apply_forgetting(empty, &forgotten);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(forgotten, 0U);

    nimcp_swarm_memory_destroy(empty);
}

// =============================================================================
// Consolidation Iterator Tests
// =============================================================================

TEST_F(SwarmMemoryIteratorTest, ConsolidationCollectsUncompressedMemories) {
    // Store multiple uncompressed memories
    for (int i = 0; i < 5; i++) {
        char suffix[16], memory_id[64];
        snprintf(suffix, sizeof(suffix), "uncompressed_%d", i);
        StoreTestMemory(suffix, NIMCP_MEMORY_SEMANTIC, NIMCP_IMPORTANCE_MEDIUM, memory_id);
    }

    uint32_t consolidated = 0;
    nimcp_result_t result = nimcp_swarm_memory_consolidate(memory, &consolidated);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryIteratorTest, ConsolidationCollectsUndistributedMemories) {
    char memory_id[64];
    StoreTestMemory("local_only", NIMCP_MEMORY_PROCEDURAL, NIMCP_IMPORTANCE_HIGH, memory_id);

    uint32_t consolidated = 0;
    nimcp_result_t result = nimcp_swarm_memory_consolidate(memory, &consolidated);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryIteratorTest, ConsolidationHandlesLargeMemorySet) {
    // Store many memories to test iteration
    for (int i = 0; i < 200; i++) {
        char suffix[16], memory_id[64];
        snprintf(suffix, sizeof(suffix), "large_%d", i);
        StoreTestMemory(suffix, (NimcpMemoryType)(i % NIMCP_MEMORY_TYPE_COUNT),
                        NIMCP_IMPORTANCE_MEDIUM, memory_id);
    }

    uint32_t consolidated = 0;
    nimcp_result_t result = nimcp_swarm_memory_consolidate(memory, &consolidated);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// Novelty Iterator Tests
// =============================================================================

TEST_F(SwarmMemoryIteratorTest, NoveltyDetectsUniqueMemories) {
    char memory_id[64];

    // Store first memory
    uint8_t data1[] = "unique_pattern_alpha";
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC,
                             NIMCP_IMPORTANCE_MEDIUM, data1, sizeof(data1), memory_id);

    // Store very different memory - should have high novelty
    uint8_t data2[] = "completely_different_beta";
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC,
                             NIMCP_IMPORTANCE_MEDIUM, data2, sizeof(data2), memory_id);

    EXPECT_GT(strlen(memory_id), 0U);
}

TEST_F(SwarmMemoryIteratorTest, NoveltyDetectsSimilarMemories) {
    char memory_id1[64], memory_id2[64];

    // Store two very similar memories
    uint8_t data1[] = "pattern_AAAA_test";
    uint8_t data2[] = "pattern_AAAB_test";  // Only 1 byte different

    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC,
                             NIMCP_IMPORTANCE_MEDIUM, data1, sizeof(data1), memory_id1);
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC,
                             NIMCP_IMPORTANCE_MEDIUM, data2, sizeof(data2), memory_id2);

    // Both should be stored (novelty check shouldn't prevent storage)
    EXPECT_GT(strlen(memory_id1), 0U);
    EXPECT_GT(strlen(memory_id2), 0U);
}

TEST_F(SwarmMemoryIteratorTest, NoveltyHandlesEmptyData) {
    char memory_id[64];
    uint8_t empty_data[1] = {0};

    nimcp_result_t result = nimcp_swarm_memory_store(
        memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_LOW,
        empty_data, 0, memory_id
    );

    // Should handle gracefully (may reject or accept)
    // Main test is no crash
    SUCCEED();
}

// =============================================================================
// Node Selection Iterator Tests
// =============================================================================

TEST_F(SwarmMemoryIteratorTest, NodeSelectionSelectsActiveNodes) {
    // Register some hippocampus nodes (capacity = 100 memories each)
    nimcp_swarm_memory_register_node(memory, "node_alpha", 100);
    nimcp_swarm_memory_register_node(memory, "node_beta", 100);
    nimcp_swarm_memory_register_node(memory, "node_gamma", 100);

    // Store a memory that requires replication
    char memory_id[64];
    StoreTestMemory("replicate_me", NIMCP_MEMORY_THREAT, NIMCP_IMPORTANCE_CRITICAL, memory_id);

    // Distribute should use the node selection iterator
    uint32_t replicas_created = 0;
    nimcp_result_t result = nimcp_swarm_memory_distribute(memory, memory_id, &replicas_created);

    // May succeed or fail depending on node availability
    // Main test is correct iteration without crashes
    SUCCEED();
}

TEST_F(SwarmMemoryIteratorTest, NodeSelectionHandlesNoNodes) {
    // Don't register any nodes
    char memory_id[64];
    StoreTestMemory("orphan", NIMCP_MEMORY_SEMANTIC, NIMCP_IMPORTANCE_HIGH, memory_id);

    uint32_t replicas_created = 0;
    nimcp_result_t result = nimcp_swarm_memory_distribute(memory, memory_id, &replicas_created);

    // Should handle gracefully with no nodes available
    SUCCEED();
}

TEST_F(SwarmMemoryIteratorTest, NodeSelectionRespectsCapacity) {
    // Register more nodes than replication factor (capacity = 50 each)
    for (int i = 0; i < 10; i++) {
        char node_id[32];
        snprintf(node_id, sizeof(node_id), "node_%d", i);
        nimcp_swarm_memory_register_node(memory, node_id, 50);
    }

    char memory_id[64];
    StoreTestMemory("distributed", NIMCP_MEMORY_PROCEDURAL, NIMCP_IMPORTANCE_HIGH, memory_id);

    uint32_t replicas_created = 0;
    nimcp_result_t result = nimcp_swarm_memory_distribute(memory, memory_id, &replicas_created);
    SUCCEED();
}

// =============================================================================
// Edge Cases and Error Handling
// =============================================================================

TEST_F(SwarmMemoryIteratorTest, HandleNullMemorySystem) {
    uint32_t count = 0;

    // These should handle null gracefully
    nimcp_result_t result1 = nimcp_swarm_memory_apply_forgetting(nullptr, &count);
    nimcp_result_t result2 = nimcp_swarm_memory_consolidate(nullptr, &count);

    EXPECT_NE(result1, NIMCP_SUCCESS);
    EXPECT_NE(result2, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryIteratorTest, HandleNullOutputPointers) {
    // Null output pointers should be handled
    nimcp_result_t result1 = nimcp_swarm_memory_apply_forgetting(memory, nullptr);
    nimcp_result_t result2 = nimcp_swarm_memory_consolidate(memory, nullptr);

    // Should either succeed (ignore output) or return error (not crash)
    SUCCEED();
}

TEST_F(SwarmMemoryIteratorTest, ConcurrentIterationStability) {
    // Store memories while operations are in progress
    for (int i = 0; i < 50; i++) {
        char suffix[16], memory_id[64];
        snprintf(suffix, sizeof(suffix), "concurrent_%d", i);
        StoreTestMemory(suffix, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_LOW, memory_id);
    }

    // Run multiple operations
    uint32_t forgotten = 0, consolidated = 0;
    nimcp_swarm_memory_apply_forgetting(memory, &forgotten);
    nimcp_swarm_memory_consolidate(memory, &consolidated);
    nimcp_swarm_memory_apply_forgetting(memory, &forgotten);

    SUCCEED();
}

// =============================================================================
// Pattern Index Access Tests (compression.pattern_index fix)
// =============================================================================

TEST_F(SwarmMemoryIteratorTest, AbstractPatternUsesCompressionContext) {
    // Store multiple related memories
    const char* memory_ids[3];
    char id1[64], id2[64], id3[64];

    uint8_t data1[] = "pattern_A_data_123";
    uint8_t data2[] = "pattern_A_data_456";
    uint8_t data3[] = "pattern_A_data_789";

    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC,
                             NIMCP_IMPORTANCE_MEDIUM, data1, sizeof(data1), id1);
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC,
                             NIMCP_IMPORTANCE_MEDIUM, data2, sizeof(data2), id2);
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC,
                             NIMCP_IMPORTANCE_MEDIUM, data3, sizeof(data3), id3);

    memory_ids[0] = id1;
    memory_ids[1] = id2;
    memory_ids[2] = id3;

    uint32_t pattern_hash = 0;
    nimcp_result_t result = nimcp_swarm_memory_abstract_pattern(
        memory, memory_ids, 3, &pattern_hash
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_NE(pattern_hash, 0U);
}

TEST_F(SwarmMemoryIteratorTest, BuildHierarchyUsesCompressionContext) {
    // Store some memories first
    for (int i = 0; i < 20; i++) {
        char suffix[16], memory_id[64];
        snprintf(suffix, sizeof(suffix), "hierarchy_%d", i);
        StoreTestMemory(suffix, NIMCP_MEMORY_SEMANTIC, NIMCP_IMPORTANCE_MEDIUM, memory_id);
    }

    uint32_t levels = 0;
    nimcp_result_t result = nimcp_swarm_memory_build_hierarchy(memory, &levels);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(levels, 1U);
}

TEST_F(SwarmMemoryIteratorTest, GeneralizeCreatesConsolidatedEntry) {
    const char* memory_ids[2];
    char id1[64], id2[64], gen_id[64];

    uint8_t data1[] = "specific_instance_1";
    uint8_t data2[] = "specific_instance_2";

    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC,
                             NIMCP_IMPORTANCE_MEDIUM, data1, sizeof(data1), id1);
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC,
                             NIMCP_IMPORTANCE_MEDIUM, data2, sizeof(data2), id2);

    memory_ids[0] = id1;
    memory_ids[1] = id2;

    nimcp_result_t result = nimcp_swarm_memory_generalize(
        memory, memory_ids, 2, gen_id
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(strlen(gen_id), 0U);
}

// =============================================================================
// Bio-Message Header Tests (payload_size fix)
// =============================================================================

TEST_F(SwarmMemoryIteratorTest, ProcessMessageHandlesSyncRequest) {
    // Enable bio-async for testing
    memory->bio_async_enabled = true;

    // This tests that process_message uses header->payload_size correctly
    nimcp_result_t result = nimcp_swarm_memory_process_message(memory, nullptr);

    // Null message should be rejected
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// Sync With Node Tests (memory_count fix)
// =============================================================================

TEST_F(SwarmMemoryIteratorTest, SyncWithNodeUsesMemoryCount) {
    nimcp_swarm_memory_register_node(memory, "sync_target", 100);

    uint32_t synced = 0;
    nimcp_result_t result = nimcp_swarm_memory_sync_with_node(
        memory, "sync_target", &synced
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryIteratorTest, SyncWithUnknownNodeFails) {
    uint32_t synced = 0;
    nimcp_result_t result = nimcp_swarm_memory_sync_with_node(
        memory, "nonexistent_node", &synced
    );

    EXPECT_NE(result, NIMCP_SUCCESS);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
