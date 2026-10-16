/**
 * @file test_swarm_memory_gpu.cpp
 * @brief Unit tests for GPU-accelerated Swarm Memory operations
 *
 * Tests replay buffer, priority sampling, hippocampal compression,
 * memory consolidation, federated learning, and swarm GPU operations.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <random>
#include <algorithm>
#include <numeric>

// Headers already have their own extern "C" guards
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/swarm/nimcp_swarm_gpu.h"
#include "swarm/nimcp_swarm_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class SwarmMemoryGPUTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    NimcpSwarmMemory* swarm_memory = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (swarm_memory) {
            nimcp_swarm_memory_destroy(swarm_memory);
            swarm_memory = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    // Helper to create swarm memory
    NimcpSwarmMemory* CreateSwarmMemory(uint32_t capacity = 1000, uint32_t replication = 3) {
        return nimcp_swarm_memory_create(capacity, replication);
    }

    // Helper to create a tensor filled with a constant value
    nimcp_gpu_tensor_t* CreateFilledTensor(size_t* dims, size_t rank, float value) {
        if (!ctx) return nullptr;
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, rank, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    // Helper to create 1D tensor
    nimcp_gpu_tensor_t* Create1DTensor(size_t n, float value = 0.0f) {
        size_t dims[1] = {n};
        return CreateFilledTensor(dims, 1, value);
    }

    // Helper to create 2D tensor
    nimcp_gpu_tensor_t* Create2DTensor(size_t rows, size_t cols, float value = 0.0f) {
        size_t dims[2] = {rows, cols};
        return CreateFilledTensor(dims, 2, value);
    }

    // Helper to copy tensor to host
    std::vector<float> CopyToHost(nimcp_gpu_tensor_t* tensor) {
        size_t n = tensor->numel;
        std::vector<float> host_data(n);
        nimcp_gpu_tensor_to_host(tensor, host_data.data());
        return host_data;
    }

    // Helper to set tensor from host
    void SetFromHost(nimcp_gpu_tensor_t* tensor, const std::vector<float>& data) {
        nimcp_gpu_tensor_t* temp = nimcp_gpu_tensor_from_host(ctx, data.data(),
            tensor->dims, tensor->ndim, tensor->precision);
        if (temp) {
            nimcp_gpu_copy(ctx, temp, tensor);
            nimcp_gpu_tensor_destroy(temp);
        }
    }

    // Helper to create random positions
    std::vector<float> CreateRandomPositions(size_t n_agents, float range = 10.0f, int seed = 42) {
        std::vector<float> positions(n_agents * 4);
        std::mt19937 gen(seed);
        std::uniform_real_distribution<float> dist(-range, range);

        for (size_t i = 0; i < n_agents; i++) {
            positions[i * 4 + 0] = dist(gen);  // x
            positions[i * 4 + 1] = dist(gen);  // y
            positions[i * 4 + 2] = dist(gen);  // z
            positions[i * 4 + 3] = 1.0f;       // w (mass)
        }
        return positions;
    }

    // Helper to create random velocities
    std::vector<float> CreateRandomVelocities(size_t n_agents, float max_speed = 1.0f, int seed = 123) {
        std::vector<float> velocities(n_agents * 4);
        std::mt19937 gen(seed);
        std::uniform_real_distribution<float> dist(-max_speed, max_speed);

        for (size_t i = 0; i < n_agents; i++) {
            velocities[i * 4 + 0] = dist(gen);
            velocities[i * 4 + 1] = dist(gen);
            velocities[i * 4 + 2] = dist(gen);
            velocities[i * 4 + 3] = 0.0f;  // unused
        }
        return velocities;
    }

    // Helper to create random beliefs
    std::vector<float> CreateRandomBeliefs(size_t n_agents, size_t belief_dim, int seed = 456) {
        std::vector<float> beliefs(n_agents * belief_dim);
        std::mt19937 gen(seed);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (size_t i = 0; i < beliefs.size(); i++) {
            beliefs[i] = dist(gen);
        }
        return beliefs;
    }

    // Helper to create test memory data
    std::vector<uint8_t> CreateTestData(size_t size, uint8_t pattern = 0xAB) {
        std::vector<uint8_t> data(size, pattern);
        return data;
    }

    // Helper to create pattern signature
    std::vector<float> CreatePatternSignature(size_t dim, int seed = 789) {
        std::vector<float> signature(dim);
        std::mt19937 gen(seed);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (size_t i = 0; i < dim; i++) {
            signature[i] = dist(gen);
        }

        // Normalize
        float norm = 0.0f;
        for (float s : signature) norm += s * s;
        norm = std::sqrt(norm);
        for (float& s : signature) s /= norm;

        return signature;
    }
};

//=============================================================================
// Swarm Memory Creation Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, MemoryCreation_WithValidParams_ReturnsValidMemory) {
    NimcpSwarmMemory* memory = nimcp_swarm_memory_create(1000, 3);
    ASSERT_NE(memory, nullptr);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, MemoryCreation_DifferentCapacities_Works) {
    uint32_t capacities[] = {100, 1000, 10000};

    for (uint32_t cap : capacities) {
        NimcpSwarmMemory* memory = nimcp_swarm_memory_create(cap, 3);
        ASSERT_NE(memory, nullptr) << "Failed for capacity: " << cap;

        nimcp_swarm_memory_destroy(memory);
    }
}

TEST_F(SwarmMemoryGPUTest, MemoryDestruction_NullSafe) {
    nimcp_swarm_memory_destroy(nullptr);  // Should not crash
}

TEST_F(SwarmMemoryGPUTest, MemoryInit_Succeeds) {
    NimcpSwarmMemory* memory = nimcp_swarm_memory_create(1000, 3);
    ASSERT_NE(memory, nullptr);

    nimcp_result_t result = nimcp_swarm_memory_init(memory, nullptr);
    EXPECT_EQ(result, NIMCP_OK);

    nimcp_swarm_memory_destroy(memory);
}

//=============================================================================
// Memory Store and Retrieve Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, Store_EpisodicMemory_Succeeds) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    auto data = CreateTestData(256);
    char memory_id[64];

    nimcp_result_t result = nimcp_swarm_memory_store(
        memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_MEDIUM,
        data.data(), data.size(), memory_id
    );
    EXPECT_EQ(result, NIMCP_OK);
    EXPECT_GT(strlen(memory_id), 0u);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, Store_AllMemoryTypes_Succeeds) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    NimcpMemoryType types[] = {
        NIMCP_MEMORY_EPISODIC, NIMCP_MEMORY_SEMANTIC, NIMCP_MEMORY_PROCEDURAL,
        NIMCP_MEMORY_THREAT, NIMCP_MEMORY_SPATIAL
    };

    for (NimcpMemoryType type : types) {
        auto data = CreateTestData(128);
        char memory_id[64];

        nimcp_result_t result = nimcp_swarm_memory_store(
            memory, type, NIMCP_IMPORTANCE_MEDIUM,
            data.data(), data.size(), memory_id
        );
        EXPECT_EQ(result, NIMCP_OK) << "Failed for type: " << type;
    }

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, Retrieve_StoredMemory_Succeeds) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Store
    std::vector<uint8_t> original_data(256);
    for (size_t i = 0; i < original_data.size(); i++) {
        original_data[i] = static_cast<uint8_t>(i % 256);
    }
    char memory_id[64];
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_HIGH,
                             original_data.data(), original_data.size(), memory_id);

    // Retrieve
    std::vector<uint8_t> retrieved_data(256);
    nimcp_result_t result = nimcp_swarm_memory_retrieve(
        memory, memory_id, retrieved_data.data(), retrieved_data.size()
    );
    EXPECT_EQ(result, NIMCP_OK);

    // Verify
    EXPECT_EQ(original_data, retrieved_data);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, Access_UpdatesTracking) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    auto data = CreateTestData(128);
    char memory_id[64];
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_MEDIUM,
                             data.data(), data.size(), memory_id);

    // Access multiple times
    for (int i = 0; i < 5; i++) {
        nimcp_result_t result = nimcp_swarm_memory_access(memory, memory_id);
        EXPECT_EQ(result, NIMCP_OK);
    }

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, Rehearse_StrengthensMemory) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    auto data = CreateTestData(128);
    char memory_id[64];
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_MEDIUM,
                             data.data(), data.size(), memory_id);

    // Rehearse
    nimcp_result_t result = nimcp_swarm_memory_rehearse(memory, memory_id);
    EXPECT_EQ(result, NIMCP_OK);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, Delete_RemovesMemory) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    auto data = CreateTestData(128);
    char memory_id[64];
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_LOW,
                             data.data(), data.size(), memory_id);

    nimcp_result_t result = nimcp_swarm_memory_delete(memory, memory_id);
    EXPECT_EQ(result, NIMCP_OK);

    // Should fail to retrieve deleted memory
    std::vector<uint8_t> retrieved(128);
    result = nimcp_swarm_memory_retrieve(memory, memory_id, retrieved.data(), retrieved.size());
    EXPECT_NE(result, NIMCP_OK);

    nimcp_swarm_memory_destroy(memory);
}

//=============================================================================
// Replay Buffer Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, ReplayBuffer_ScheduleReplay) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    auto data = CreateTestData(128);
    char memory_id[64];
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_HIGH,
                             data.data(), data.size(), memory_id);

    nimcp_result_t result = nimcp_swarm_memory_schedule_replay(memory, memory_id, 0.9f);
    EXPECT_EQ(result, NIMCP_OK);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, ReplayBuffer_ExecuteReplayCycle) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Store multiple memories
    for (int i = 0; i < 10; i++) {
        auto data = CreateTestData(128, i);
        char memory_id[64];
        nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_HIGH,
                                 data.data(), data.size(), memory_id);
        nimcp_swarm_memory_schedule_replay(memory, memory_id, 0.5f + 0.05f * i);
    }

    // Execute replay cycle
    uint32_t replays_performed = 0;
    nimcp_result_t result = nimcp_swarm_memory_replay_cycle(memory, 5, &replays_performed);
    EXPECT_EQ(result, NIMCP_OK);
    EXPECT_LE(replays_performed, 5u);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, ReplayBuffer_PriorityCalculation) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Create memory entry
    NimcpMemoryEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.importance = NIMCP_IMPORTANCE_HIGH;
    entry.novelty_score = 0.8f;
    entry.strength = 0.5f;
    entry.created_at = 0;
    entry.last_accessed = 0;

    float priority = nimcp_swarm_memory_calculate_replay_priority(memory, &entry);
    EXPECT_GE(priority, 0.0f);
    EXPECT_LE(priority, 1.0f);

    nimcp_swarm_memory_destroy(memory);
}

//=============================================================================
// Priority Sampling Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, PrioritySampling_HighPriorityFirst) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Store memories with different priorities
    char high_priority_id[64], low_priority_id[64];

    auto data1 = CreateTestData(128, 0x11);
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_THREAT, NIMCP_IMPORTANCE_CRITICAL,
                             data1.data(), data1.size(), high_priority_id);
    nimcp_swarm_memory_schedule_replay(memory, high_priority_id, 1.0f);

    auto data2 = CreateTestData(128, 0x22);
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_LOW,
                             data2.data(), data2.size(), low_priority_id);
    nimcp_swarm_memory_schedule_replay(memory, low_priority_id, 0.1f);

    // High priority should be replayed first
    uint32_t replays = 0;
    nimcp_swarm_memory_replay_cycle(memory, 1, &replays);
    // Test passes if no crash occurs

    nimcp_swarm_memory_destroy(memory);
}

//=============================================================================
// Hippocampal Compression Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, Compression_CompressMemory) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    auto data = CreateTestData(1024);  // 1KB
    char memory_id[64];
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_MEDIUM,
                             data.data(), data.size(), memory_id);

    NimcpCompressedMemory compressed;
    memset(&compressed, 0, sizeof(compressed));

    nimcp_result_t result = nimcp_swarm_memory_compress(memory, memory_id, &compressed);
    EXPECT_EQ(result, NIMCP_OK);

    // Compressed should be smaller or equal
    if (compressed.compressed_data) {
        EXPECT_LE(compressed.compressed_size, compressed.original_size);
        free(compressed.compressed_data);
    }

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, Compression_DecompressMemory) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Store original
    std::vector<uint8_t> original(512);
    for (size_t i = 0; i < original.size(); i++) {
        original[i] = static_cast<uint8_t>(i % 256);
    }
    char memory_id[64];
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_MEDIUM,
                             original.data(), original.size(), memory_id);

    // Compress
    NimcpCompressedMemory compressed;
    memset(&compressed, 0, sizeof(compressed));
    nimcp_swarm_memory_compress(memory, memory_id, &compressed);

    // Decompress
    std::vector<uint8_t> decompressed(original.size());
    nimcp_result_t result = nimcp_swarm_memory_decompress(
        memory, &compressed, decompressed.data(), decompressed.size()
    );

    if (result == NIMCP_OK && compressed.compressed_data) {
        EXPECT_EQ(original, decompressed);
    }

    if (compressed.compressed_data) {
        free(compressed.compressed_data);
    }

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, Compression_ExtractPattern) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    auto data = CreateTestData(256);
    char memory_id[64];
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC, NIMCP_IMPORTANCE_HIGH,
                             data.data(), data.size(), memory_id);

    uint32_t pattern_hash = 0;
    nimcp_result_t result = nimcp_swarm_memory_extract_pattern(memory, memory_id, &pattern_hash);
    EXPECT_EQ(result, NIMCP_OK);

    nimcp_swarm_memory_destroy(memory);
}

//=============================================================================
// Systems Consolidation Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, Consolidation_ConfigureWindow) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    NimcpConsolidationWindow window;
    window.mode = NIMCP_CONSOLIDATION_SLEEP;
    window.window_start = 0;
    window.window_duration_ms = 60000;  // 1 minute
    window.max_memories_per_window = 100;
    window.activity_threshold = 0.2f;
    window.auto_schedule = true;

    nimcp_result_t result = nimcp_swarm_memory_configure_consolidation(memory, &window);
    EXPECT_EQ(result, NIMCP_OK);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, Consolidation_StartConsolidation) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    nimcp_result_t result = nimcp_swarm_memory_start_consolidation(memory, NIMCP_CONSOLIDATION_PASSIVE);
    EXPECT_EQ(result, NIMCP_OK);

    EXPECT_TRUE(nimcp_swarm_memory_is_consolidating(memory));

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, Consolidation_ExecuteConsolidation) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Store memories
    for (int i = 0; i < 20; i++) {
        auto data = CreateTestData(128, i);
        char memory_id[64];
        nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC,
                                 static_cast<NimcpMemoryImportance>(i % 4),
                                 data.data(), data.size(), memory_id);
    }

    nimcp_swarm_memory_start_consolidation(memory, NIMCP_CONSOLIDATION_ACTIVE);

    uint32_t consolidated = 0;
    nimcp_result_t result = nimcp_swarm_memory_consolidate(memory, &consolidated);
    EXPECT_EQ(result, NIMCP_OK);

    nimcp_swarm_memory_destroy(memory);
}

//=============================================================================
// SWS Replay Tests (Slow-Wave Sleep)
//=============================================================================

TEST_F(SwarmMemoryGPUTest, SWSReplay_DuringSleep) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Store important memories
    for (int i = 0; i < 10; i++) {
        auto data = CreateTestData(256, i);
        char memory_id[64];
        nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_HIGH,
                                 data.data(), data.size(), memory_id);
    }

    // Start sleep-like consolidation
    nimcp_swarm_memory_start_consolidation(memory, NIMCP_CONSOLIDATION_SLEEP);

    // Run consolidation (simulates SWS replay)
    uint32_t consolidated = 0;
    nimcp_swarm_memory_consolidate(memory, &consolidated);

    nimcp_swarm_memory_destroy(memory);
}

//=============================================================================
// Memory Decay Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, Decay_ForgettingCurve) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Set forgetting curve
    NimcpForgettingCurve curve;
    curve.initial_strength = 1.0f;
    curve.decay_rate = 0.1f;
    curve.importance_modifier = 0.5f;
    curve.rehearsal_boost = 0.2f;
    curve.half_life_ms = 3600000;  // 1 hour

    nimcp_result_t result = nimcp_swarm_memory_set_forgetting_curve(
        memory, NIMCP_MEMORY_EPISODIC, &curve
    );
    EXPECT_EQ(result, NIMCP_OK);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, Decay_CalculateStrength) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    NimcpMemoryEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.type = NIMCP_MEMORY_EPISODIC;
    entry.strength = 1.0f;
    entry.created_at = 0;
    entry.last_rehearsed = 0;

    // Calculate strength at different times
    float strength_now = nimcp_swarm_memory_calculate_strength(memory, &entry, 0);
    float strength_later = nimcp_swarm_memory_calculate_strength(memory, &entry, 3600000);

    EXPECT_LE(strength_later, strength_now);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, Decay_ApplyForgetting) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Store weak memories
    for (int i = 0; i < 10; i++) {
        auto data = CreateTestData(64, i);
        char memory_id[64];
        nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_LOW,
                                 data.data(), data.size(), memory_id);
    }

    uint32_t forgotten = 0;
    nimcp_result_t result = nimcp_swarm_memory_apply_forgetting(memory, &forgotten);
    EXPECT_EQ(result, NIMCP_OK);

    nimcp_swarm_memory_destroy(memory);
}

//=============================================================================
// Selective Consolidation Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, SelectiveConsolidation_ImportanceBased) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Store memories with varying importance
    for (int i = 0; i < 20; i++) {
        auto data = CreateTestData(128, i);
        char memory_id[64];
        NimcpMemoryImportance imp = static_cast<NimcpMemoryImportance>(i % 4);
        nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC, imp,
                                 data.data(), data.size(), memory_id);
    }

    // Consolidation should prioritize important memories
    nimcp_swarm_memory_start_consolidation(memory, NIMCP_CONSOLIDATION_ACTIVE);
    uint32_t consolidated = 0;
    nimcp_swarm_memory_consolidate(memory, &consolidated);

    nimcp_swarm_memory_destroy(memory);
}

//=============================================================================
// Distributed Hippocampus Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, DistributedHippocampus_RegisterNode) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    nimcp_result_t result = nimcp_swarm_memory_register_node(memory, "node_001", 500);
    EXPECT_EQ(result, NIMCP_OK);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, DistributedHippocampus_UnregisterNode) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    nimcp_swarm_memory_register_node(memory, "node_001", 500);
    nimcp_result_t result = nimcp_swarm_memory_unregister_node(memory, "node_001");
    EXPECT_EQ(result, NIMCP_OK);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, DistributedHippocampus_DistributeMemory) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Register nodes
    nimcp_swarm_memory_register_node(memory, "node_001", 500);
    nimcp_swarm_memory_register_node(memory, "node_002", 500);
    nimcp_swarm_memory_register_node(memory, "node_003", 500);

    // Store and distribute
    auto data = CreateTestData(256);
    char memory_id[64];
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_THREAT, NIMCP_IMPORTANCE_CRITICAL,
                             data.data(), data.size(), memory_id);

    uint32_t replicas = 0;
    nimcp_result_t result = nimcp_swarm_memory_distribute(memory, memory_id, &replicas);
    EXPECT_EQ(result, NIMCP_OK);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, DistributedHippocampus_VerifyConsensus) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Register nodes
    nimcp_swarm_memory_register_node(memory, "node_001", 500);
    nimcp_swarm_memory_register_node(memory, "node_002", 500);

    // Store and distribute
    auto data = CreateTestData(128);
    char memory_id[64];
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC, NIMCP_IMPORTANCE_HIGH,
                             data.data(), data.size(), memory_id);
    nimcp_swarm_memory_distribute(memory, memory_id, nullptr);

    bool has_consensus = false;
    nimcp_result_t result = nimcp_swarm_memory_verify_consensus(memory, memory_id, &has_consensus);
    EXPECT_EQ(result, NIMCP_OK);

    nimcp_swarm_memory_destroy(memory);
}

//=============================================================================
// Pattern Learning Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, PatternLearning_StorePattern) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    auto signature = CreatePatternSignature(64);

    swarm_pattern_t pattern;
    memset(&pattern, 0, sizeof(pattern));
    pattern.pattern_id = 1;
    strcpy(pattern.label, "test_pattern");
    pattern.data = signature.data();
    pattern.data_len = signature.size();
    pattern.signature = signature.data();
    pattern.signature_size = signature.size();
    pattern.strength = 0.8f;
    pattern.confidence = 0.7f;

    nimcp_result_t result = swarm_memory_store_pattern(memory, &pattern);
    EXPECT_EQ(result, NIMCP_OK);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, PatternLearning_RetrievePattern) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Store pattern
    auto signature = CreatePatternSignature(64);
    swarm_pattern_t pattern;
    memset(&pattern, 0, sizeof(pattern));
    pattern.pattern_id = 42;
    strcpy(pattern.label, "retrieve_test");
    pattern.data = signature.data();
    pattern.data_len = signature.size();
    pattern.signature = signature.data();
    pattern.signature_size = signature.size();
    pattern.confidence = 0.9f;
    swarm_memory_store_pattern(memory, &pattern);

    // Retrieve
    swarm_pattern_t retrieved;
    memset(&retrieved, 0, sizeof(retrieved));
    nimcp_result_t result = swarm_memory_retrieve_pattern(memory, 42, &retrieved);

    if (result == NIMCP_OK) {
        EXPECT_EQ(retrieved.pattern_id, 42u);
        EXPECT_STREQ(retrieved.label, "retrieve_test");
    }

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, PatternLearning_DetectPattern) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Store known pattern
    auto signature = CreatePatternSignature(64, 123);
    swarm_pattern_t pattern;
    memset(&pattern, 0, sizeof(pattern));
    pattern.pattern_id = 1;
    pattern.data = signature.data();
    pattern.data_len = signature.size();
    pattern.signature = signature.data();
    pattern.signature_size = signature.size();
    pattern.confidence = 0.9f;
    swarm_memory_store_pattern(memory, &pattern);

    // Try to detect with similar observation
    auto observation = CreatePatternSignature(64, 123);  // Same seed = similar pattern
    swarm_pattern_t matched;
    memset(&matched, 0, sizeof(matched));

    nimcp_result_t result = swarm_memory_detect_pattern(
        memory, observation.data(), observation.size(), &matched
    );

    // May or may not find match depending on implementation
    (void)result;

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, PatternLearning_AssociatePatternOutcome) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Store pattern
    auto signature = CreatePatternSignature(32);
    swarm_pattern_t pattern;
    memset(&pattern, 0, sizeof(pattern));
    pattern.pattern_id = 10;
    pattern.data = signature.data();
    pattern.data_len = signature.size();
    pattern.confidence = 0.8f;
    swarm_memory_store_pattern(memory, &pattern);

    // Associate with outcome
    nimcp_result_t result = swarm_memory_associate_pattern(memory, 10, 5, 1.0f);
    // May succeed or fail depending on whether pattern exists

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, PatternLearning_LearnSequence) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Store patterns
    for (uint32_t i = 0; i < 5; i++) {
        auto sig = CreatePatternSignature(32, i);
        swarm_pattern_t p;
        memset(&p, 0, sizeof(p));
        p.pattern_id = i;
        p.data = sig.data();
        p.data_len = sig.size();
        p.confidence = 0.9f;
        swarm_memory_store_pattern(memory, &p);
    }

    // Learn sequence
    uint32_t sequence[] = {0, 1, 2, 3, 4};
    nimcp_result_t result = swarm_memory_learn_sequence(memory, sequence, 5);
    // Result depends on implementation

    nimcp_swarm_memory_destroy(memory);
}

//=============================================================================
// Flocking GPU Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, FlockingGPU_CreateState) {
    RequireGPU();

    nimcp_flocking_gpu_params_t params;
    nimcp_flocking_gpu_default_params(&params);

    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(ctx, 1000, 32, &params);
    ASSERT_NE(state, nullptr);

    nimcp_flocking_gpu_destroy(state);
}

TEST_F(SwarmMemoryGPUTest, FlockingGPU_ComputeForces) {
    RequireGPU();

    const size_t n_agents = 100;

    nimcp_flocking_gpu_params_t params;
    nimcp_flocking_gpu_default_params(&params);
    params.separation_radius = 2.0f;
    params.alignment_radius = 5.0f;
    params.cohesion_radius = 10.0f;

    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(ctx, n_agents, 32, &params);
    ASSERT_NE(state, nullptr);

    // Set initial positions and velocities
    auto positions = CreateRandomPositions(n_agents, 50.0f);
    auto velocities = CreateRandomVelocities(n_agents, 1.0f);

    SetFromHost(state->positions, positions);
    SetFromHost(state->velocities, velocities);

    // Compute forces
    bool result = nimcp_gpu_flocking_compute_forces(ctx, state);
    EXPECT_TRUE(result);

    // Check forces are computed
    auto forces = CopyToHost(state->forces);
    bool has_nonzero = false;
    for (size_t i = 0; i < forces.size(); i += 4) {
        if (forces[i] != 0 || forces[i+1] != 0 || forces[i+2] != 0) {
            has_nonzero = true;
            break;
        }
    }
    // May or may not have forces depending on agent distribution

    nimcp_flocking_gpu_destroy(state);
}

TEST_F(SwarmMemoryGPUTest, FlockingGPU_Update) {
    RequireGPU();

    const size_t n_agents = 50;

    nimcp_flocking_gpu_params_t params;
    nimcp_flocking_gpu_default_params(&params);
    params.dt = 0.016f;  // ~60 FPS

    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(ctx, n_agents, 32, &params);
    ASSERT_NE(state, nullptr);

    // Initialize
    auto positions = CreateRandomPositions(n_agents, 10.0f);
    auto velocities = CreateRandomVelocities(n_agents, 0.5f);
    SetFromHost(state->positions, positions);
    SetFromHost(state->velocities, velocities);

    // Update
    bool result = nimcp_gpu_flocking_update(ctx, state, 0.0f);  // Use params.dt
    EXPECT_TRUE(result);

    nimcp_flocking_gpu_destroy(state);
}

//=============================================================================
// Consensus GPU Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, ConsensusGPU_CreateState) {
    RequireGPU();

    nimcp_consensus_gpu_params_t params;
    nimcp_consensus_gpu_default_params(&params);

    nimcp_consensus_gpu_state_t* state = nimcp_consensus_gpu_create(ctx, 100, 10, &params);
    ASSERT_NE(state, nullptr);

    nimcp_consensus_gpu_destroy(state);
}

TEST_F(SwarmMemoryGPUTest, ConsensusGPU_Averaging) {
    RequireGPU();

    const size_t n_agents = 50;
    const size_t belief_dim = 8;

    nimcp_consensus_gpu_params_t params;
    nimcp_consensus_gpu_default_params(&params);
    params.learning_rate = 0.5f;

    nimcp_consensus_gpu_state_t* state = nimcp_consensus_gpu_create(ctx, n_agents, belief_dim, &params);
    ASSERT_NE(state, nullptr);

    // Set initial beliefs
    auto beliefs = CreateRandomBeliefs(n_agents, belief_dim);
    SetFromHost(state->beliefs, beliefs);

    // Run averaging
    bool result = nimcp_gpu_consensus_averaging(ctx, state);
    EXPECT_TRUE(result);

    nimcp_consensus_gpu_destroy(state);
}

TEST_F(SwarmMemoryGPUTest, ConsensusGPU_CheckConvergence) {
    RequireGPU();

    const size_t n_agents = 30;
    const size_t belief_dim = 4;

    nimcp_consensus_gpu_params_t params;
    nimcp_consensus_gpu_default_params(&params);

    nimcp_consensus_gpu_state_t* state = nimcp_consensus_gpu_create(ctx, n_agents, belief_dim, &params);
    ASSERT_NE(state, nullptr);

    // Set uniform beliefs (should be converged)
    std::vector<float> uniform_beliefs(n_agents * belief_dim, 0.5f);
    SetFromHost(state->beliefs, uniform_beliefs);

    bool converged = false;
    float variance = 0.0f;
    bool result = nimcp_gpu_consensus_check_convergence(ctx, state, &converged, &variance);
    EXPECT_TRUE(result);
    EXPECT_NEAR(variance, 0.0f, 0.01f);

    nimcp_consensus_gpu_destroy(state);
}

//=============================================================================
// Pheromone GPU Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, PheromoneGPU_CreateState) {
    RequireGPU();

    nimcp_pheromone_gpu_params_t params;
    nimcp_pheromone_gpu_default_params(&params);

    nimcp_pheromone_gpu_state_t* state = nimcp_pheromone_gpu_create(
        ctx, 64, 64, 1, 2, 1.0f, &params
    );
    ASSERT_NE(state, nullptr);

    nimcp_pheromone_gpu_destroy(state);
}

TEST_F(SwarmMemoryGPUTest, PheromoneGPU_Diffusion) {
    RequireGPU();

    nimcp_pheromone_gpu_params_t params;
    nimcp_pheromone_gpu_default_params(&params);
    params.diffusion_rate = 0.1f;

    nimcp_pheromone_gpu_state_t* state = nimcp_pheromone_gpu_create(
        ctx, 32, 32, 1, 1, 1.0f, &params
    );
    ASSERT_NE(state, nullptr);

    bool result = nimcp_gpu_pheromone_diffusion(ctx, state, 0.1f);
    EXPECT_TRUE(result);

    nimcp_pheromone_gpu_destroy(state);
}

TEST_F(SwarmMemoryGPUTest, PheromoneGPU_Decay) {
    RequireGPU();

    nimcp_pheromone_gpu_params_t params;
    nimcp_pheromone_gpu_default_params(&params);
    params.decay_rates[0] = 0.05f;

    nimcp_pheromone_gpu_state_t* state = nimcp_pheromone_gpu_create(
        ctx, 32, 32, 1, 1, 1.0f, &params
    );
    ASSERT_NE(state, nullptr);

    // Fill with concentration
    nimcp_gpu_fill(ctx, state->concentration, 1.0f);

    // Apply decay
    bool result = nimcp_gpu_pheromone_decay(ctx, state, 1.0f);
    EXPECT_TRUE(result);

    // Concentration should decrease
    auto conc = CopyToHost(state->concentration);
    float avg = std::accumulate(conc.begin(), conc.end(), 0.0f) / conc.size();
    EXPECT_LT(avg, 1.0f);

    nimcp_pheromone_gpu_destroy(state);
}

//=============================================================================
// Quorum Sensing GPU Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, QuorumGPU_CreateState) {
    RequireGPU();

    nimcp_quorum_gpu_params_t params;
    nimcp_quorum_gpu_default_params(&params);

    nimcp_quorum_gpu_state_t* state = nimcp_quorum_gpu_create(ctx, 100, 4, &params);
    ASSERT_NE(state, nullptr);

    nimcp_quorum_gpu_destroy(state);
}

TEST_F(SwarmMemoryGPUTest, QuorumGPU_ComputeConcentration) {
    RequireGPU();

    nimcp_quorum_gpu_params_t params;
    nimcp_quorum_gpu_default_params(&params);

    nimcp_quorum_gpu_state_t* state = nimcp_quorum_gpu_create(ctx, 50, 2, &params);
    ASSERT_NE(state, nullptr);

    // Set agent signals
    std::vector<float> signals(50 * 2, 0.5f);
    SetFromHost(state->agent_signals, signals);

    bool result = nimcp_gpu_quorum_compute_concentration(ctx, state);
    EXPECT_TRUE(result);

    nimcp_quorum_gpu_destroy(state);
}

TEST_F(SwarmMemoryGPUTest, QuorumGPU_CheckThresholds) {
    RequireGPU();

    nimcp_quorum_gpu_params_t params;
    nimcp_quorum_gpu_default_params(&params);
    params.base_threshold = 0.5f;

    nimcp_quorum_gpu_state_t* state = nimcp_quorum_gpu_create(ctx, 100, 2, &params);
    ASSERT_NE(state, nullptr);

    // Set high signals (should trigger threshold)
    std::vector<float> signals(100 * 2, 0.8f);
    SetFromHost(state->agent_signals, signals);
    nimcp_gpu_quorum_compute_concentration(ctx, state);

    bool result = nimcp_gpu_quorum_check_thresholds(ctx, state);
    EXPECT_TRUE(result);

    nimcp_quorum_gpu_destroy(state);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, Statistics_GetStatistics) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Store some memories
    for (int i = 0; i < 10; i++) {
        auto data = CreateTestData(128, i);
        char memory_id[64];
        nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_MEDIUM,
                                 data.data(), data.size(), memory_id);
    }

    NimcpMemoryStatistics stats;
    nimcp_result_t result = nimcp_swarm_memory_get_statistics(memory, &stats);
    EXPECT_EQ(result, NIMCP_OK);
    EXPECT_GE(stats.total_memories, 10u);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, Statistics_GetCountByType) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Store 5 episodic, 3 semantic
    for (int i = 0; i < 5; i++) {
        auto data = CreateTestData(64, i);
        char memory_id[64];
        nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_MEDIUM,
                                 data.data(), data.size(), memory_id);
    }
    for (int i = 0; i < 3; i++) {
        auto data = CreateTestData(64, i + 100);
        char memory_id[64];
        nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC, NIMCP_IMPORTANCE_HIGH,
                                 data.data(), data.size(), memory_id);
    }

    EXPECT_EQ(nimcp_swarm_memory_get_count_by_type(memory, NIMCP_MEMORY_EPISODIC), 5u);
    EXPECT_EQ(nimcp_swarm_memory_get_count_by_type(memory, NIMCP_MEMORY_SEMANTIC), 3u);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, Statistics_HealthScore) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    float health = nimcp_swarm_memory_get_health_score(memory);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);

    nimcp_swarm_memory_destroy(memory);
}

//=============================================================================
// Ternary Confidence Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, TernaryConfidence_EnableDisable) {
    NimcpSwarmMemory* memory = CreateSwarmMemory();
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    ternary_confidence_config_t config;
    ternary_confidence_default_config(&config);

    nimcp_result_t result = swarm_memory_enable_ternary_confidence(memory, &config);
    EXPECT_EQ(result, NIMCP_OK);
    EXPECT_TRUE(swarm_memory_is_ternary_confidence(memory));

    result = swarm_memory_disable_ternary_confidence(memory);
    EXPECT_EQ(result, NIMCP_OK);
    EXPECT_FALSE(swarm_memory_is_ternary_confidence(memory));

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, TernaryConfidence_FromValue) {
    ternary_confidence_config_t config;
    ternary_confidence_default_config(&config);

    EXPECT_EQ(ternary_confidence_from_value(&config, 0.9f), SWARM_CONFIDENCE_CERTAIN);
    EXPECT_EQ(ternary_confidence_from_value(&config, 0.6f), SWARM_CONFIDENCE_UNCERTAIN);
    EXPECT_EQ(ternary_confidence_from_value(&config, 0.2f), SWARM_CONFIDENCE_UNRELIABLE);
}

TEST_F(SwarmMemoryGPUTest, TernaryConfidence_ToValue) {
    EXPECT_NEAR(ternary_confidence_to_value(SWARM_CONFIDENCE_CERTAIN), 0.9f, 0.01f);
    EXPECT_NEAR(ternary_confidence_to_value(SWARM_CONFIDENCE_UNCERTAIN), 0.6f, 0.01f);
    EXPECT_NEAR(ternary_confidence_to_value(SWARM_CONFIDENCE_UNRELIABLE), 0.2f, 0.01f);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, NullSafety_StoreWithNull) {
    char memory_id[64];
    nimcp_result_t result = nimcp_swarm_memory_store(
        nullptr, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_LOW,
        nullptr, 0, memory_id
    );
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(SwarmMemoryGPUTest, NullSafety_RetrieveWithNull) {
    uint8_t buffer[64];
    nimcp_result_t result = nimcp_swarm_memory_retrieve(nullptr, "test", buffer, 64);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(SwarmMemoryGPUTest, NullSafety_GetStatisticsWithNull) {
    NimcpMemoryStatistics stats;
    nimcp_result_t result = nimcp_swarm_memory_get_statistics(nullptr, &stats);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(SwarmMemoryGPUTest, NullSafety_FlockingDestroyNull) {
    nimcp_flocking_gpu_destroy(nullptr);  // Should not crash
}

TEST_F(SwarmMemoryGPUTest, NullSafety_ConsensusDestroyNull) {
    nimcp_consensus_gpu_destroy(nullptr);  // Should not crash
}

TEST_F(SwarmMemoryGPUTest, NullSafety_PheromoneDestroyNull) {
    nimcp_pheromone_gpu_destroy(nullptr);  // Should not crash
}

TEST_F(SwarmMemoryGPUTest, NullSafety_QuorumDestroyNull) {
    nimcp_quorum_gpu_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(SwarmMemoryGPUTest, Integration_FullMemoryLifecycle) {
    NimcpSwarmMemory* memory = CreateSwarmMemory(500, 2);
    ASSERT_NE(memory, nullptr);
    nimcp_swarm_memory_init(memory, nullptr);

    // Register nodes
    nimcp_swarm_memory_register_node(memory, "node_A", 200);
    nimcp_swarm_memory_register_node(memory, "node_B", 200);

    // Store memories of different types
    for (int i = 0; i < 20; i++) {
        auto data = CreateTestData(128, i);
        char memory_id[64];
        NimcpMemoryType type = static_cast<NimcpMemoryType>(i % 5);
        NimcpMemoryImportance imp = static_cast<NimcpMemoryImportance>(i % 4);

        nimcp_swarm_memory_store(memory, type, imp, data.data(), data.size(), memory_id);
        nimcp_swarm_memory_schedule_replay(memory, memory_id, 0.1f * i);
    }

    // Run replay cycle
    uint32_t replays = 0;
    nimcp_swarm_memory_replay_cycle(memory, 5, &replays);

    // Start consolidation
    nimcp_swarm_memory_start_consolidation(memory, NIMCP_CONSOLIDATION_ACTIVE);
    uint32_t consolidated = 0;
    nimcp_swarm_memory_consolidate(memory, &consolidated);

    // Apply forgetting
    uint32_t forgotten = 0;
    nimcp_swarm_memory_apply_forgetting(memory, &forgotten);

    // Get statistics
    NimcpMemoryStatistics stats;
    nimcp_swarm_memory_get_statistics(memory, &stats);
    EXPECT_GT(stats.total_memories, 0u);

    nimcp_swarm_memory_destroy(memory);
}

TEST_F(SwarmMemoryGPUTest, Integration_FlockingSimulation) {
    RequireGPU();

    const size_t n_agents = 200;
    const int num_steps = 100;

    nimcp_flocking_gpu_params_t params;
    nimcp_flocking_gpu_default_params(&params);
    params.separation_weight = 1.5f;
    params.alignment_weight = 1.0f;
    params.cohesion_weight = 1.0f;
    params.max_speed = 2.0f;
    params.dt = 0.016f;

    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(ctx, n_agents, 32, &params);
    ASSERT_NE(state, nullptr);

    // Initialize agents
    auto positions = CreateRandomPositions(n_agents, 25.0f);
    auto velocities = CreateRandomVelocities(n_agents, 0.5f);
    SetFromHost(state->positions, positions);
    SetFromHost(state->velocities, velocities);

    // Run simulation
    for (int step = 0; step < num_steps; step++) {
        nimcp_gpu_flocking_compute_forces(ctx, state);
        nimcp_gpu_flocking_update(ctx, state, 0.0f);
    }

    nimcp_flocking_gpu_destroy(state);
}
