//=============================================================================
// test_sparse_synapse_ternary.cpp - Unit Tests for Ternary Sparse Synapse Operations
//=============================================================================
/**
 * @file test_sparse_synapse_ternary.cpp
 * @brief Comprehensive unit tests for ternary sparse synapse operations
 *
 * WHAT: Tests ternary sparse synapse creation, add/remove with ternary weights
 * WHY:  Validate memory-efficient ternary synapse storage and operations
 * HOW:  GTest-based unit tests with edge cases and memory verification
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "core/neuralnet/nimcp_sparse_synapse.h"
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include "utils/ternary/nimcp_ternary_convert.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class SparseSynapseTernaryTest : public ::testing::Test {
protected:
    sparse_synapse_pool_t pool;
    sparse_synapse_storage_t storage;

    void SetUp() override {
        sparse_synapse_pool_config_t config = sparse_synapse_pool_default_config();
        config.pool_size = 10000;
        config.enable_statistics = true;
        config.thread_safe = false;  // Single-threaded tests

        pool = sparse_synapse_pool_create(&config);
        ASSERT_NE(pool, nullptr);

        sparse_synapse_storage_init(&storage);
    }

    void TearDown() override {
        sparse_synapse_storage_cleanup(pool, &storage);
        sparse_synapse_pool_destroy(pool);
    }

    // Helper to convert weight to ternary and back
    float ternary_weight(float original, float threshold = 0.5f) {
        trit_t t = trit_from_float_threshold(original, threshold);
        return trit_to_float(t);
    }
};

//=============================================================================
// Ternary Sparse Synapse Add/Remove Tests
//=============================================================================

TEST_F(SparseSynapseTernaryTest, AddSynapseWithTernaryWeight) {
    // Add synapses with ternary-quantized weights
    float original_weights[] = {0.8f, -0.7f, 0.2f, -0.3f, 0.9f};
    const float threshold = 0.5f;

    for (size_t i = 0; i < 5; i++) {
        // Quantize weight to ternary
        trit_t t = trit_from_float_threshold(original_weights[i], threshold);
        float ternary_w = trit_to_float(t);

        int result = sparse_synapse_add(pool, &storage, (uint32_t)i, ternary_w);
        EXPECT_EQ(result, 0);
    }

    EXPECT_EQ(sparse_synapse_count(&storage), 5U);

    // Verify ternary weights
    synapse_handle_t* h0 = sparse_synapse_get(&storage, 0);
    EXPECT_NE(h0, nullptr);
    EXPECT_FLOAT_EQ(h0->weight, 1.0f);  // 0.8 -> +1

    synapse_handle_t* h1 = sparse_synapse_get(&storage, 1);
    EXPECT_NE(h1, nullptr);
    EXPECT_FLOAT_EQ(h1->weight, -1.0f);  // -0.7 -> -1

    synapse_handle_t* h2 = sparse_synapse_get(&storage, 2);
    EXPECT_NE(h2, nullptr);
    EXPECT_FLOAT_EQ(h2->weight, 0.0f);  // 0.2 -> 0

    synapse_handle_t* h3 = sparse_synapse_get(&storage, 3);
    EXPECT_NE(h3, nullptr);
    EXPECT_FLOAT_EQ(h3->weight, 0.0f);  // -0.3 -> 0

    synapse_handle_t* h4 = sparse_synapse_get(&storage, 4);
    EXPECT_NE(h4, nullptr);
    EXPECT_FLOAT_EQ(h4->weight, 1.0f);  // 0.9 -> +1
}

TEST_F(SparseSynapseTernaryTest, RemoveTernarySynapse) {
    // Add synapses
    for (uint32_t i = 0; i < 10; i++) {
        trit_t t = (trit_t)((i % 3) - 1);  // -1, 0, +1 pattern
        float ternary_w = trit_to_float(t);
        int result = sparse_synapse_add(pool, &storage, i, ternary_w);
        EXPECT_EQ(result, 0);
    }

    EXPECT_EQ(sparse_synapse_count(&storage), 10U);

    // Remove middle synapse
    int result = sparse_synapse_remove(pool, &storage, 5);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sparse_synapse_count(&storage), 9U);

    // Remove first synapse
    result = sparse_synapse_remove(pool, &storage, 0);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sparse_synapse_count(&storage), 8U);

    // Remove last synapse
    result = sparse_synapse_remove(pool, &storage, 7);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sparse_synapse_count(&storage), 7U);
}

TEST_F(SparseSynapseTernaryTest, TernaryWeightPatterns) {
    // Test all ternary weight patterns
    const uint32_t num_synapses = 30;

    for (uint32_t i = 0; i < num_synapses; i++) {
        trit_t t = (trit_t)((i % 3) - 1);
        float ternary_w = trit_to_float(t);
        int result = sparse_synapse_add(pool, &storage, i, ternary_w);
        EXPECT_EQ(result, 0);
    }

    // Verify pattern
    for (uint32_t i = 0; i < num_synapses; i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, i);
        ASSERT_NE(handle, nullptr);

        trit_t expected = (trit_t)((i % 3) - 1);
        float expected_w = trit_to_float(expected);
        EXPECT_FLOAT_EQ(handle->weight, expected_w);
    }
}

TEST_F(SparseSynapseTernaryTest, OverflowWithTernaryWeights) {
    // Fill embedded storage and force overflow
    const uint32_t embedded_cap = SPARSE_SYNAPSE_EMBEDDED_CAPACITY;
    const uint32_t total_synapses = embedded_cap + 20;

    for (uint32_t i = 0; i < total_synapses; i++) {
        trit_t t = (trit_t)((i % 3) - 1);
        float ternary_w = trit_to_float(t);
        int result = sparse_synapse_add(pool, &storage, i, ternary_w);
        EXPECT_EQ(result, 0);
    }

    EXPECT_EQ(sparse_synapse_count(&storage), total_synapses);

    // Verify all synapses (embedded and overflow)
    for (uint32_t i = 0; i < total_synapses; i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, i);
        ASSERT_NE(handle, nullptr);

        trit_t expected = (trit_t)((i % 3) - 1);
        float expected_w = trit_to_float(expected);
        EXPECT_FLOAT_EQ(handle->weight, expected_w);
    }
}

//=============================================================================
// Iterator Tests with Ternary Weights
//=============================================================================

TEST_F(SparseSynapseTernaryTest, IterateTernarySynapses) {
    const uint32_t num_synapses = 50;

    // Add synapses with ternary weights
    for (uint32_t i = 0; i < num_synapses; i++) {
        trit_t t = (trit_t)((i % 3) - 1);
        float ternary_w = trit_to_float(t);
        sparse_synapse_add(pool, &storage, i, ternary_w);
    }

    // Iterate and count ternary values
    sparse_synapse_iterator_t it;
    sparse_synapse_iterator_init(&it, &storage);

    uint32_t count = 0;
    uint32_t positive_count = 0;
    uint32_t negative_count = 0;
    uint32_t zero_count = 0;

    synapse_handle_t* handle;
    while ((handle = sparse_synapse_iterator_next(&it)) != nullptr) {
        count++;

        if (handle->weight > 0.5f) positive_count++;
        else if (handle->weight < -0.5f) negative_count++;
        else zero_count++;
    }

    EXPECT_EQ(count, num_synapses);

    // With pattern (i % 3) - 1, we expect roughly equal distribution
    EXPECT_NEAR(positive_count, num_synapses / 3, 2);
    EXPECT_NEAR(negative_count, num_synapses / 3, 2);
    EXPECT_NEAR(zero_count, num_synapses / 3, 2);
}

TEST_F(SparseSynapseTernaryTest, IteratorReset) {
    // Add synapses
    for (uint32_t i = 0; i < 10; i++) {
        sparse_synapse_add(pool, &storage, i, trit_to_float((trit_t)((i % 3) - 1)));
    }

    sparse_synapse_iterator_t it;
    sparse_synapse_iterator_init(&it, &storage);

    // First iteration
    uint32_t count1 = 0;
    while (sparse_synapse_iterator_next(&it) != nullptr) {
        count1++;
    }

    // Reset and iterate again
    sparse_synapse_iterator_reset(&it);

    uint32_t count2 = 0;
    while (sparse_synapse_iterator_next(&it) != nullptr) {
        count2++;
    }

    EXPECT_EQ(count1, count2);
    EXPECT_EQ(count1, 10U);
}

//=============================================================================
// Null Pointer and Error Handling Tests
//=============================================================================

TEST_F(SparseSynapseTernaryTest, NullStorageHandling) {
    // Operations on null storage should handle gracefully
    EXPECT_EQ(sparse_synapse_count(nullptr), 0U);
    EXPECT_EQ(sparse_synapse_get(nullptr, 0), nullptr);
}

TEST_F(SparseSynapseTernaryTest, InvalidIndexHandling) {
    // Add some synapses
    for (uint32_t i = 0; i < 5; i++) {
        sparse_synapse_add(pool, &storage, i, 1.0f);
    }

    // Access invalid index
    synapse_handle_t* handle = sparse_synapse_get(&storage, 100);
    EXPECT_EQ(handle, nullptr);

    // Remove invalid index
    int result = sparse_synapse_remove(pool, &storage, 100);
    EXPECT_EQ(result, -1);
}

TEST_F(SparseSynapseTernaryTest, RemoveFromEmptyStorage) {
    // Remove from empty storage
    int result = sparse_synapse_remove(pool, &storage, 0);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Memory Efficiency Tests
//=============================================================================

TEST_F(SparseSynapseTernaryTest, CompactAfterRemoval) {
    // Fill beyond embedded capacity
    const uint32_t total = SPARSE_SYNAPSE_EMBEDDED_CAPACITY + 30;

    for (uint32_t i = 0; i < total; i++) {
        sparse_synapse_add(pool, &storage, i, trit_to_float((trit_t)((i % 3) - 1)));
    }

    // Remove many synapses (leaving less than embedded capacity)
    for (uint32_t i = total - 1; i >= SPARSE_SYNAPSE_EMBEDDED_CAPACITY / 2; i--) {
        sparse_synapse_remove(pool, &storage, i);
        if (i == 0) break;
    }

    // Compact should move overflow to embedded
    uint32_t moved = sparse_synapse_compact(pool, &storage);

    // After compaction, remaining synapses should fit in embedded
    EXPECT_LE(sparse_synapse_count(&storage), SPARSE_SYNAPSE_EMBEDDED_CAPACITY);
}

TEST_F(SparseSynapseTernaryTest, PoolUtilization) {
    // Add many synapses with overflow
    const uint32_t num_storage = 10;
    std::vector<sparse_synapse_storage_t> storages(num_storage);

    for (auto& s : storages) {
        sparse_synapse_storage_init(&s);

        // Add synapses beyond embedded capacity for each
        for (uint32_t i = 0; i < SPARSE_SYNAPSE_EMBEDDED_CAPACITY + 10; i++) {
            sparse_synapse_add(pool, &s, i, trit_to_float((trit_t)((i % 3) - 1)));
        }
    }

    // Check pool utilization
    float utilization = sparse_synapse_pool_utilization(pool);
    EXPECT_GT(utilization, 0.0f);
    EXPECT_LE(utilization, 1.0f);

    // Cleanup
    for (auto& s : storages) {
        sparse_synapse_storage_cleanup(pool, &s);
    }
}

//=============================================================================
// Ternary Weight Computation Tests
//=============================================================================

TEST_F(SparseSynapseTernaryTest, ComputeWeightedSum) {
    // Create synapses with ternary weights
    float weights[] = {1.0f, -1.0f, 0.0f, 1.0f, -1.0f};  // Already ternary
    float inputs[] = {0.5f, 0.3f, 0.8f, 0.2f, 0.4f};

    for (size_t i = 0; i < 5; i++) {
        sparse_synapse_add(pool, &storage, (uint32_t)i, weights[i]);
    }

    // Compute weighted sum: sum(w_i * x_i)
    float weighted_sum = 0.0f;
    for (size_t i = 0; i < 5; i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, (uint32_t)i);
        ASSERT_NE(handle, nullptr);
        weighted_sum += handle->weight * inputs[i];
    }

    // Expected: 1*0.5 + (-1)*0.3 + 0*0.8 + 1*0.2 + (-1)*0.4
    //         = 0.5 - 0.3 + 0 + 0.2 - 0.4 = 0.0
    EXPECT_NEAR(weighted_sum, 0.0f, 1e-6f);
}

TEST_F(SparseSynapseTernaryTest, BatchTernaryWeightUpdate) {
    // Add synapses with initial ternary weights
    const uint32_t num = 20;
    for (uint32_t i = 0; i < num; i++) {
        sparse_synapse_add(pool, &storage, i, trit_to_float((trit_t)((i % 3) - 1)));
    }

    // Simulate weight update (re-quantize)
    for (uint32_t i = 0; i < num; i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, i);
        ASSERT_NE(handle, nullptr);

        // Perturb weight and re-quantize
        float perturbed = handle->weight + 0.3f * ((i % 2) ? 1.0f : -1.0f);
        trit_t new_t = trit_from_float_threshold(perturbed, 0.5f);
        handle->weight = trit_to_float(new_t);
    }

    // Verify all weights are still ternary
    for (uint32_t i = 0; i < num; i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, i);
        ASSERT_NE(handle, nullptr);

        EXPECT_TRUE(handle->weight == -1.0f ||
                    handle->weight == 0.0f ||
                    handle->weight == 1.0f);
    }
}

//=============================================================================
// Pool Statistics with Ternary Weights
//=============================================================================

TEST_F(SparseSynapseTernaryTest, PoolStatistics) {
    // Add synapses with ternary weights
    for (uint32_t i = 0; i < 100; i++) {
        sparse_synapse_add(pool, &storage, i, trit_to_float((trit_t)((i % 3) - 1)));
    }

    sparse_synapse_stats_t stats;
    int result = sparse_synapse_pool_get_stats(pool, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(stats.total_synapses, 100U);
    EXPECT_EQ(stats.embedded_synapses, 64U);  // First 64 in embedded
    EXPECT_EQ(stats.overflow_synapses, 36U);  // Remaining in overflow
}

TEST_F(SparseSynapseTernaryTest, MemorySavingsCalculation) {
    // Add many synapses
    const size_t num_neurons = 100;
    const size_t synapses_per_neuron = 50;

    std::vector<sparse_synapse_storage_t> neuron_storages(num_neurons);

    for (auto& s : neuron_storages) {
        sparse_synapse_storage_init(&s);
        for (size_t j = 0; j < synapses_per_neuron; j++) {
            sparse_synapse_add(pool, &s, (uint32_t)j, trit_to_float((trit_t)((j % 3) - 1)));
        }
    }

    // Calculate memory savings
    float savings = sparse_synapse_memory_savings(
        pool,
        num_neurons,
        100,  // dense allocation would use 100 synapses per neuron
        600   // full synapse_t is ~600 bytes
    );

    // Should achieve significant savings with sparse+ternary
    EXPECT_GT(savings, 0.5f);

    // Cleanup
    for (auto& s : neuron_storages) {
        sparse_synapse_storage_cleanup(pool, &s);
    }
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(SparseSynapseTernaryTest, SingleSynapseOperations) {
    // Add single synapse
    int result = sparse_synapse_add(pool, &storage, 42, 1.0f);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sparse_synapse_count(&storage), 1U);

    synapse_handle_t* handle = sparse_synapse_get(&storage, 0);
    ASSERT_NE(handle, nullptr);
    EXPECT_EQ(handle->target_neuron_id, 42U);
    EXPECT_FLOAT_EQ(handle->weight, 1.0f);

    // Remove single synapse
    result = sparse_synapse_remove(pool, &storage, 0);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sparse_synapse_count(&storage), 0U);
}

TEST_F(SparseSynapseTernaryTest, ExactlyEmbeddedCapacity) {
    // Fill exactly to embedded capacity
    for (uint32_t i = 0; i < SPARSE_SYNAPSE_EMBEDDED_CAPACITY; i++) {
        int result = sparse_synapse_add(pool, &storage, i,
                                         trit_to_float((trit_t)((i % 3) - 1)));
        EXPECT_EQ(result, 0);
    }

    EXPECT_EQ(sparse_synapse_count(&storage), SPARSE_SYNAPSE_EMBEDDED_CAPACITY);
    EXPECT_EQ(storage.embedded_count, SPARSE_SYNAPSE_EMBEDDED_CAPACITY);
    EXPECT_EQ(storage.overflow_count, 0U);

    // Add one more to trigger overflow
    int result = sparse_synapse_add(pool, &storage, SPARSE_SYNAPSE_EMBEDDED_CAPACITY, 1.0f);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(storage.overflow_count, 1U);
}

TEST_F(SparseSynapseTernaryTest, AllZeroWeights) {
    // Add synapses with all zero weights (sparse representation)
    for (uint32_t i = 0; i < 50; i++) {
        int result = sparse_synapse_add(pool, &storage, i, 0.0f);
        EXPECT_EQ(result, 0);
    }

    // Verify all weights are zero
    for (uint32_t i = 0; i < 50; i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, i);
        ASSERT_NE(handle, nullptr);
        EXPECT_FLOAT_EQ(handle->weight, 0.0f);
    }
}

TEST_F(SparseSynapseTernaryTest, AllPositiveWeights) {
    // Add synapses with all positive ternary weights
    for (uint32_t i = 0; i < 50; i++) {
        int result = sparse_synapse_add(pool, &storage, i, 1.0f);
        EXPECT_EQ(result, 0);
    }

    // Verify all weights are +1
    for (uint32_t i = 0; i < 50; i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, i);
        ASSERT_NE(handle, nullptr);
        EXPECT_FLOAT_EQ(handle->weight, 1.0f);
    }
}

TEST_F(SparseSynapseTernaryTest, AllNegativeWeights) {
    // Add synapses with all negative ternary weights
    for (uint32_t i = 0; i < 50; i++) {
        int result = sparse_synapse_add(pool, &storage, i, -1.0f);
        EXPECT_EQ(result, 0);
    }

    // Verify all weights are -1
    for (uint32_t i = 0; i < 50; i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, i);
        ASSERT_NE(handle, nullptr);
        EXPECT_FLOAT_EQ(handle->weight, -1.0f);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
