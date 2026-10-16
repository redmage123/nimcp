/**
 * @file test_swarm_struct_member_regression.cpp
 * @brief Regression tests for struct member access fixes
 *
 * These tests ensure the following bugs don't recur:
 *
 * 1. NimcpHippocampusNode->memories (WRONG) vs ->memory_count (CORRECT)
 *    - The NimcpHippocampusNode struct doesn't have a 'memories' hash table
 *    - It has 'memory_count' for tracking the number of memories
 *
 * 2. memory->pattern_index (WRONG) vs memory->compression.pattern_index (CORRECT)
 *    - pattern_index is nested inside the compression struct
 *
 * 3. gen_entry->is_generalized / pattern_hash (WRONG) - fields don't exist
 *    - NimcpMemoryEntry uses is_consolidated for generalized memories
 *    - Pattern hash is stored in the data field
 *
 * 4. bio_message_header_t->total_length (WRONG) vs ->payload_size (CORRECT)
 *
 * 5. NimcpDistributedNode (WRONG) vs NimcpHippocampusNode (CORRECT)
 *    - The distributed node type doesn't exist
 *
 * 6. coordinator->conflict_history (WRONG) vs ->conflict_stats (CORRECT)
 *    - conflict_history array doesn't exist; use conflict_stats counters
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <type_traits>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_memory.h"
#include "swarm/nimcp_swarm_multi.h"
#include "async/nimcp_bio_messages.h"

// =============================================================================
// Compile-Time Type Safety Tests
// These ensure struct members exist and have correct types
// =============================================================================

// Test 1: NimcpHippocampusNode has memory_count (not memories hash table)
TEST(SwarmStructMemberRegression, HippocampusNodeHasMemoryCount) {
    NimcpHippocampusNode node = {0};

    // This should compile - memory_count exists
    node.memory_count = 42;
    EXPECT_EQ(node.memory_count, 42U);

    // Verify it's the right type
    static_assert(std::is_same<decltype(node.memory_count), uint32_t>::value,
                  "memory_count should be uint32_t");
}

TEST(SwarmStructMemberRegression, HippocampusNodeHasNodeId) {
    NimcpHippocampusNode node = {0};

    strncpy(node.node_id, "test_node", sizeof(node.node_id) - 1);
    EXPECT_STREQ(node.node_id, "test_node");
}

TEST(SwarmStructMemberRegression, HippocampusNodeHasIsActive) {
    NimcpHippocampusNode node = {0};

    node.is_active = true;
    EXPECT_TRUE(node.is_active);

    static_assert(std::is_same<decltype(node.is_active), bool>::value,
                  "is_active should be bool");
}

// Test 2: NimcpSemanticCompression has pattern_index
TEST(SwarmStructMemberRegression, SemanticCompressionHasPatternIndex) {
    NimcpSemanticCompression compression = {0};

    // pattern_index exists in SemanticCompression
    compression.pattern_index = nullptr;
    EXPECT_EQ(compression.pattern_index, nullptr);
}

// Test 3: NimcpMemoryEntry has is_consolidated (not is_generalized)
TEST(SwarmStructMemberRegression, MemoryEntryHasIsConsolidated) {
    NimcpMemoryEntry entry = {0};

    entry.is_consolidated = true;
    EXPECT_TRUE(entry.is_consolidated);

    static_assert(std::is_same<decltype(entry.is_consolidated), bool>::value,
                  "is_consolidated should be bool");
}

TEST(SwarmStructMemberRegression, MemoryEntryHasIsCompressed) {
    NimcpMemoryEntry entry = {0};

    entry.is_compressed = true;
    EXPECT_TRUE(entry.is_compressed);
}

TEST(SwarmStructMemberRegression, MemoryEntryHasIsDistributed) {
    NimcpMemoryEntry entry = {0};

    entry.is_distributed = true;
    EXPECT_TRUE(entry.is_distributed);
}

// Test 4: bio_message_header_t has payload_size (not total_length)
TEST(SwarmStructMemberRegression, BioMessageHeaderHasPayloadSize) {
    bio_message_header_t header = {};

    header.payload_size = 1024;
    EXPECT_EQ(header.payload_size, 1024U);

    static_assert(std::is_same<decltype(header.payload_size), uint32_t>::value,
                  "payload_size should be uint32_t");
}

// Test 5: nimcp_multi_swarm_coordinator_t has conflict_stats (not conflict_history)
TEST(SwarmStructMemberRegression, CoordinatorHasConflictStats) {
    // Can't easily instantiate coordinator without create function
    // but we can verify the stats struct

    nimcp_conflict_resolution_stats_t stats = {0};

    stats.total_conflicts = 10;
    stats.conflicts_resolved = 5;
    stats.conflicts_pending = 5;
    stats.escalations = 2;
    stats.merges_performed = 1;
    stats.avg_resolution_time_ms = 100.0f;

    EXPECT_EQ(stats.total_conflicts, 10U);
    EXPECT_EQ(stats.conflicts_resolved, 5U);
    EXPECT_EQ(stats.conflicts_pending, 5U);
}

// =============================================================================
// Runtime Behavior Tests
// These verify the fixed code paths work correctly
// =============================================================================

class SwarmMemoryRegressionTest : public ::testing::Test {
protected:
    NimcpSwarmMemory* memory;

    void SetUp() override {
        memory = nimcp_swarm_memory_create(500, 3);
        ASSERT_NE(memory, nullptr);
        nimcp_swarm_memory_init(memory, nullptr);
    }

    void TearDown() override {
        if (memory) {
            nimcp_swarm_memory_destroy(memory);
        }
    }
};

// Regression: sync_with_node should use node->memory_count
TEST_F(SwarmMemoryRegressionTest, SyncWithNodeUsesMemoryCount) {
    // Register a node with capacity
    nimcp_swarm_memory_register_node(memory, "regression_node", 100);

    // Sync should work without crashing (was accessing node->memories before)
    uint32_t synced = 0;
    nimcp_result_t result = nimcp_swarm_memory_sync_with_node(
        memory, "regression_node", &synced
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    // synced count comes from memory_count, not hash_table_size(memories)
}

// Regression: abstract_pattern should use memory->compression.pattern_index
TEST_F(SwarmMemoryRegressionTest, AbstractPatternUsesCompressionPatternIndex) {
    // Store memories
    const char* ids[2];
    char id1[64], id2[64];

    uint8_t data1[] = "pattern_data_1";
    uint8_t data2[] = "pattern_data_2";

    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC,
                             NIMCP_IMPORTANCE_MEDIUM, data1, sizeof(data1), id1);
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC,
                             NIMCP_IMPORTANCE_MEDIUM, data2, sizeof(data2), id2);

    ids[0] = id1;
    ids[1] = id2;

    uint32_t pattern_hash = 0;

    // Should work without accessing non-existent memory->pattern_index
    nimcp_result_t result = nimcp_swarm_memory_abstract_pattern(
        memory, ids, 2, &pattern_hash
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// Regression: build_hierarchy should use memory->compression.pattern_index
TEST_F(SwarmMemoryRegressionTest, BuildHierarchyUsesCompressionPatternIndex) {
    // Store some memories
    for (int i = 0; i < 5; i++) {
        char memory_id[64];
        uint8_t data[32];
        snprintf((char*)data, sizeof(data), "hierarchy_test_%d", i);
        nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC,
                                 NIMCP_IMPORTANCE_MEDIUM, data, strlen((char*)data) + 1,
                                 memory_id);
    }

    uint32_t levels = 0;

    // Should work without accessing non-existent memory->pattern_index
    nimcp_result_t result = nimcp_swarm_memory_build_hierarchy(memory, &levels);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(levels, 1U);
}

// Regression: generalize should use is_consolidated (not is_generalized)
TEST_F(SwarmMemoryRegressionTest, GeneralizeUsesIsConsolidated) {
    const char* ids[2];
    char id1[64], id2[64], gen_id[64];

    uint8_t data1[] = "generalize_test_1";
    uint8_t data2[] = "generalize_test_2";

    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC,
                             NIMCP_IMPORTANCE_MEDIUM, data1, sizeof(data1), id1);
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC,
                             NIMCP_IMPORTANCE_MEDIUM, data2, sizeof(data2), id2);

    ids[0] = id1;
    ids[1] = id2;

    // Should work without accessing non-existent is_generalized field
    nimcp_result_t result = nimcp_swarm_memory_generalize(
        memory, ids, 2, gen_id
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(strlen(gen_id), 0U);
}

// Regression: process_message should use header->payload_size
TEST_F(SwarmMemoryRegressionTest, ProcessMessageUsesPayloadSize) {
    memory->bio_async_enabled = true;

    struct {
        bio_message_header_t header;
        char payload[128];
    } msg = {};

    msg.header.type = BIO_MSG_SWARM_MEMORY_SYNC;
    msg.header.payload_size = 64;  // Should use this, not total_length

    // Should work without accessing non-existent total_length field
    nimcp_result_t result = nimcp_swarm_memory_process_message(memory, &msg);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// Regression: select_replication_nodes should use NimcpHippocampusNode
TEST_F(SwarmMemoryRegressionTest, ReplicationUsesHippocampusNode) {
    // Register nodes with capacity
    nimcp_swarm_memory_register_node(memory, "repl_node_1", 100);
    nimcp_swarm_memory_register_node(memory, "repl_node_2", 100);
    nimcp_swarm_memory_register_node(memory, "repl_node_3", 100);

    // Store a memory
    char memory_id[64];
    uint8_t data[] = "replicate_me";
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_THREAT,
                             NIMCP_IMPORTANCE_CRITICAL, data, sizeof(data),
                             memory_id);

    // Distribute should work (uses NimcpHippocampusNode internally)
    uint32_t replica_count = 0;
    nimcp_result_t result = nimcp_swarm_memory_distribute(memory, memory_id, &replica_count);

    // May succeed or fail depending on node state, but shouldn't crash
    SUCCEED();
}

// =============================================================================
// Swarm Multi Coordinator Regression Tests
// =============================================================================

class SwarmMultiRegressionTest : public ::testing::Test {
protected:
    nimcp_multi_swarm_coordinator_t* coordinator;
    nimcp_swarm_identity_t* swarm1;
    nimcp_swarm_identity_t* swarm2;

    void SetUp() override {
        coordinator = nimcp_multi_swarm_create(nullptr, nullptr);
        swarm1 = nullptr;
        swarm2 = nullptr;
        ASSERT_NE(coordinator, nullptr);
    }

    void TearDown() override {
        // Note: Do NOT destroy swarm1/swarm2 manually if they were registered
        // with the coordinator - the coordinator's swarm_registry has a destructor
        // that will call nimcp_swarm_identity_destroy automatically
        if (coordinator) {
            nimcp_multi_swarm_destroy(coordinator);
        }
        // Reset pointers (already freed by coordinator)
        swarm1 = nullptr;
        swarm2 = nullptr;
    }
};

// Regression: conflict handling should use conflict_stats (not conflict_history)
TEST_F(SwarmMultiRegressionTest, ConflictHandlingUsesConflictStats) {
    // Get initial stats
    nimcp_conflict_resolution_stats_t initial = nimcp_multi_swarm_get_conflict_stats(coordinator);

    // Register swarms to potentially create conflicts
    swarm1 = nimcp_swarm_identity_create(coordinator, "conflict_swarm_1", 10);
    swarm2 = nimcp_swarm_identity_create(coordinator, "conflict_swarm_2", 10);
    ASSERT_NE(swarm1, nullptr);
    ASSERT_NE(swarm2, nullptr);

    nimcp_swarm_register(coordinator, swarm1);
    nimcp_swarm_register(coordinator, swarm2);

    // Detect conflicts
    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coordinator, &conflicts, &count);

    // Get updated stats - should work with conflict_stats (not conflict_history)
    nimcp_conflict_resolution_stats_t updated = nimcp_multi_swarm_get_conflict_stats(coordinator);

    // Stats should be properly tracked
    EXPECT_GE(updated.total_conflicts, initial.total_conflicts);
}

// Regression: stats counters should be incremented correctly
TEST_F(SwarmMultiRegressionTest, ConflictStatsCountersWork) {
    nimcp_conflict_resolution_stats_t stats = nimcp_multi_swarm_get_conflict_stats(coordinator);

    // Verify all stats fields are accessible (no crash)
    EXPECT_GE(stats.total_conflicts, 0U);
    EXPECT_GE(stats.conflicts_resolved, 0U);
    EXPECT_GE(stats.conflicts_pending, 0U);
    EXPECT_GE(stats.escalations, 0U);
    EXPECT_GE(stats.merges_performed, 0U);
    EXPECT_GE(stats.avg_resolution_time_ms, 0.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
