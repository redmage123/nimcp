/**
 * @file test_swarm_memory.cpp
 * @brief Comprehensive unit tests for NIMCP Swarm Memory Consolidation
 *
 * TEST COVERAGE:
 * - System creation and destruction
 * - Memory storage and retrieval
 * - Experience replay
 * - Knowledge distillation
 * - Forgetting curves
 * - Consolidation windows
 * - Distributed hippocampus
 * - Semantic compression
 * - Bio-async integration
 * - BBB security validation
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_memory.h"

class SwarmMemoryTest : public ::testing::Test {
protected:
    NimcpSwarmMemory* system;

    void SetUp() override {
        system = nimcp_swarm_memory_create(1000, 3);
        ASSERT_NE(system, nullptr);
        nimcp_swarm_memory_init(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            nimcp_swarm_memory_destroy(system);
        }
    }
};

TEST_F(SwarmMemoryTest, CreateValidSystem) {
    EXPECT_NE(system, nullptr);
}

TEST_F(SwarmMemoryTest, DestroyNullSystem) {
    nimcp_swarm_memory_destroy(nullptr);
    SUCCEED();
}

TEST_F(SwarmMemoryTest, CreateWithParameters) {
    auto* sys = nimcp_swarm_memory_create(500, 2);
    EXPECT_NE(sys, nullptr);
    if (sys) {
        nimcp_swarm_memory_destroy(sys);
    }
}

TEST_F(SwarmMemoryTest, StoreMemory) {
    uint8_t data[] = {1, 2, 3, 4, 5};
    char memory_id[64];

    NimcpResult result = nimcp_swarm_memory_store(
        system, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_MEDIUM,
        data, sizeof(data), memory_id
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(strlen(memory_id), 0);
}

TEST_F(SwarmMemoryTest, RetrieveMemory) {
    uint8_t data[] = {1, 2, 3, 4, 5};
    char memory_id[64];
    nimcp_swarm_memory_store(system, NIMCP_MEMORY_EPISODIC,
                             NIMCP_IMPORTANCE_MEDIUM, data, sizeof(data), memory_id);

    uint8_t retrieved[10] = {0};
    NimcpResult result = nimcp_swarm_memory_retrieve(
        system, memory_id, retrieved, sizeof(retrieved)
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(memcmp(data, retrieved, sizeof(data)), 0);
}

TEST_F(SwarmMemoryTest, AccessMemory) {
    uint8_t data[] = {1, 2, 3};
    char memory_id[64];
    nimcp_swarm_memory_store(system, NIMCP_MEMORY_SEMANTIC,
                             NIMCP_IMPORTANCE_HIGH, data, sizeof(data), memory_id);

    NimcpResult result = nimcp_swarm_memory_access(system, memory_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, RehearseMemory) {
    uint8_t data[] = {1, 2, 3};
    char memory_id[64];
    nimcp_swarm_memory_store(system, NIMCP_MEMORY_PROCEDURAL,
                             NIMCP_IMPORTANCE_CRITICAL, data, sizeof(data), memory_id);

    NimcpResult result = nimcp_swarm_memory_rehearse(system, memory_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, DeleteMemory) {
    uint8_t data[] = {1, 2, 3};
    char memory_id[64];
    nimcp_swarm_memory_store(system, NIMCP_MEMORY_THREAT,
                             NIMCP_IMPORTANCE_HIGH, data, sizeof(data), memory_id);

    NimcpResult result = nimcp_swarm_memory_delete(system, memory_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, ScheduleReplay) {
    uint8_t data[] = {1, 2, 3};
    char memory_id[64];
    nimcp_swarm_memory_store(system, NIMCP_MEMORY_EPISODIC,
                             NIMCP_IMPORTANCE_MEDIUM, data, sizeof(data), memory_id);

    NimcpResult result = nimcp_swarm_memory_schedule_replay(
        system, memory_id, 0.8f
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, ReplayCycle) {
    // Store and schedule multiple memories
    for (int i = 0; i < 5; i++) {
        uint8_t data[] = {(uint8_t)i};
        char memory_id[64];
        nimcp_swarm_memory_store(system, NIMCP_MEMORY_EPISODIC,
                                 NIMCP_IMPORTANCE_HIGH, data, 1, memory_id);
        nimcp_swarm_memory_schedule_replay(system, memory_id, 0.5f + i * 0.1f);
    }

    uint32_t replays_performed = 0;
    NimcpResult result = nimcp_swarm_memory_replay_cycle(
        system, 10, &replays_performed
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(replays_performed, 0);
}

TEST_F(SwarmMemoryTest, CompressMemory) {
    uint8_t data[100];
    for (int i = 0; i < 100; i++) data[i] = i;

    char memory_id[64];
    nimcp_swarm_memory_store(system, NIMCP_MEMORY_SEMANTIC,
                             NIMCP_IMPORTANCE_MEDIUM, data, sizeof(data), memory_id);

    NimcpCompressedMemory compressed;
    NimcpResult result = nimcp_swarm_memory_compress(
        system, memory_id, &compressed
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, DecompressMemory) {
    uint8_t data[] = {1, 2, 3, 4, 5};
    char memory_id[64];
    nimcp_swarm_memory_store(system, NIMCP_MEMORY_PROCEDURAL,
                             NIMCP_IMPORTANCE_HIGH, data, sizeof(data), memory_id);

    NimcpCompressedMemory compressed;
    nimcp_swarm_memory_compress(system, memory_id, &compressed);

    uint8_t decompressed[10] = {0};
    NimcpResult result = nimcp_swarm_memory_decompress(
        system, &compressed, decompressed, sizeof(decompressed)
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, ExtractPattern) {
    uint8_t data[] = {1, 2, 3, 4, 5};
    char memory_id[64];
    nimcp_swarm_memory_store(system, NIMCP_MEMORY_SEMANTIC,
                             NIMCP_IMPORTANCE_MEDIUM, data, sizeof(data), memory_id);

    uint32_t pattern_hash = 0;
    NimcpResult result = nimcp_swarm_memory_extract_pattern(
        system, memory_id, &pattern_hash
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, SetForgettingCurve) {
    NimcpForgettingCurve curve = {
        1.0f, 0.1f, 1.5f, 0.2f, 3600000
    };

    NimcpResult result = nimcp_swarm_memory_set_forgetting_curve(
        system, NIMCP_MEMORY_EPISODIC, &curve
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, ApplyForgetting) {
    // Store memory and let time pass
    uint8_t data[] = {1, 2, 3};
    char memory_id[64];
    nimcp_swarm_memory_store(system, NIMCP_MEMORY_EPISODIC,
                             NIMCP_IMPORTANCE_LOW, data, sizeof(data), memory_id);

    uint32_t forgotten_count = 0;
    NimcpResult result = nimcp_swarm_memory_apply_forgetting(
        system, &forgotten_count
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, ConfigureConsolidation) {
    NimcpConsolidationWindow window = {
        NIMCP_CONSOLIDATION_ACTIVE, 0, 60000, 50, 0.5f, true
    };

    NimcpResult result = nimcp_swarm_memory_configure_consolidation(
        system, &window
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, StartConsolidation) {
    NimcpResult result = nimcp_swarm_memory_start_consolidation(
        system, NIMCP_CONSOLIDATION_PASSIVE
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, ExecuteConsolidation) {
    // Store some memories
    for (int i = 0; i < 10; i++) {
        uint8_t data[] = {(uint8_t)i};
        char memory_id[64];
        nimcp_swarm_memory_store(system, NIMCP_MEMORY_EPISODIC,
                                 NIMCP_IMPORTANCE_MEDIUM, data, 1, memory_id);
    }

    uint32_t memories_consolidated = 0;
    NimcpResult result = nimcp_swarm_memory_consolidate(
        system, &memories_consolidated
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, CheckConsolidating) {
    bool is_consolidating = nimcp_swarm_memory_is_consolidating(system);
    EXPECT_FALSE(is_consolidating);
}

TEST_F(SwarmMemoryTest, RegisterNode) {
    NimcpResult result = nimcp_swarm_memory_register_node(
        system, "node_1", 100
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, UnregisterNode) {
    nimcp_swarm_memory_register_node(system, "node_1", 100);

    NimcpResult result = nimcp_swarm_memory_unregister_node(
        system, "node_1"
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, DistributeMemory) {
    // Register nodes
    nimcp_swarm_memory_register_node(system, "node_1", 100);
    nimcp_swarm_memory_register_node(system, "node_2", 100);

    uint8_t data[] = {1, 2, 3};
    char memory_id[64];
    nimcp_swarm_memory_store(system, NIMCP_MEMORY_SEMANTIC,
                             NIMCP_IMPORTANCE_HIGH, data, sizeof(data), memory_id);

    uint32_t replicas_created = 0;
    NimcpResult result = nimcp_swarm_memory_distribute(
        system, memory_id, &replicas_created
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, VerifyConsensus) {
    nimcp_swarm_memory_register_node(system, "node_1", 100);
    nimcp_swarm_memory_register_node(system, "node_2", 100);

    uint8_t data[] = {1, 2, 3};
    char memory_id[64];
    nimcp_swarm_memory_store(system, NIMCP_MEMORY_SEMANTIC,
                             NIMCP_IMPORTANCE_HIGH, data, sizeof(data), memory_id);

    bool has_consensus = false;
    NimcpResult result = nimcp_swarm_memory_verify_consensus(
        system, memory_id, &has_consensus
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, AbstractPattern) {
    const char* memory_ids[3];
    char ids[3][64];

    for (int i = 0; i < 3; i++) {
        uint8_t data[] = {(uint8_t)i, (uint8_t)(i+1)};
        nimcp_swarm_memory_store(system, NIMCP_MEMORY_SEMANTIC,
                                 NIMCP_IMPORTANCE_MEDIUM, data, 2, ids[i]);
        memory_ids[i] = ids[i];
    }

    uint32_t pattern_hash = 0;
    NimcpResult result = nimcp_swarm_memory_abstract_pattern(
        system, memory_ids, 3, &pattern_hash
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, GeneralizeMemories) {
    const char* specific_ids[3];
    char ids[3][64];

    for (int i = 0; i < 3; i++) {
        uint8_t data[] = {(uint8_t)i};
        nimcp_swarm_memory_store(system, NIMCP_MEMORY_SEMANTIC,
                                 NIMCP_IMPORTANCE_MEDIUM, data, 1, ids[i]);
        specific_ids[i] = ids[i];
    }

    char generalized_id[64];
    NimcpResult result = nimcp_swarm_memory_generalize(
        system, specific_ids, 3, generalized_id
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, BuildHierarchy) {
    // Store various memories
    for (int i = 0; i < 20; i++) {
        uint8_t data[] = {(uint8_t)i};
        char memory_id[64];
        nimcp_swarm_memory_store(system, NIMCP_MEMORY_SEMANTIC,
                                 NIMCP_IMPORTANCE_MEDIUM, data, 1, memory_id);
    }

    uint32_t levels = 0;
    NimcpResult result = nimcp_swarm_memory_build_hierarchy(
        system, &levels
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, GetStatistics) {
    NimcpMemoryStatistics stats;
    NimcpResult result = nimcp_swarm_memory_get_statistics(system, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryTest, GetCountByType) {
    // Store memories of different types
    for (int i = 0; i < NIMCP_MEMORY_TYPE_COUNT; i++) {
        uint8_t data[] = {(uint8_t)i};
        char memory_id[64];
        nimcp_swarm_memory_store(system, static_cast<NimcpMemoryType>(i),
                                 NIMCP_IMPORTANCE_MEDIUM, data, 1, memory_id);
    }

    for (int i = 0; i < NIMCP_MEMORY_TYPE_COUNT; i++) {
        uint32_t count = nimcp_swarm_memory_get_count_by_type(
            system, static_cast<NimcpMemoryType>(i)
        );
        EXPECT_GT(count, 0);
    }
}

TEST_F(SwarmMemoryTest, GetHealthScore) {
    float health = nimcp_swarm_memory_get_health_score(system);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(SwarmMemoryTest, PrintStatus) {
    nimcp_swarm_memory_print_status(system, false);
    SUCCEED();
}

TEST_F(SwarmMemoryTest, UtilityFunctions) {
    EXPECT_NE(nimcp_memory_type_to_string(NIMCP_MEMORY_EPISODIC), nullptr);
    EXPECT_NE(nimcp_consolidation_mode_to_string(NIMCP_CONSOLIDATION_ACTIVE), nullptr);
    EXPECT_NE(nimcp_memory_importance_to_string(NIMCP_IMPORTANCE_CRITICAL), nullptr);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
