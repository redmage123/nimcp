//=============================================================================
// test_sparse_ternary_integration.cpp - Ternary Sparse Synapse Integration Tests
//=============================================================================
/**
 * @file test_sparse_ternary_integration.cpp
 * @brief Integration tests for ternary representation with sparse synapse module
 *
 * WHAT: Tests ternary sparse synapse operations with neuralnet module
 * WHY:  Verify ternary weights work correctly with memory-efficient sparse storage
 * HOW:  Create sparse synapse pools with ternary weights, test operations
 *
 * TEST CATEGORIES:
 * 1. Ternary sparse synapse with neuralnet module
 * 2. Sparse operations with cortical columns
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_sparse_synapse.h"
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include "utils/ternary/nimcp_ternary_matrix.h"
#include "utils/ternary/nimcp_ternary_convert.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SparseTernaryIntegrationTest : public ::testing::Test {
protected:
    sparse_synapse_pool_t pool = nullptr;
    synapse_metadata_pool_t metadata_pool = nullptr;

    void SetUp() override {
        // Create sparse synapse pool
        sparse_synapse_pool_config_t pool_config = sparse_synapse_pool_default_config();
        pool_config.pool_size = 10000;
        pool_config.enable_statistics = true;
        pool_config.thread_safe = true;

        pool = sparse_synapse_pool_create(&pool_config);
        ASSERT_NE(pool, nullptr) << "Failed to create sparse synapse pool";

        // Create metadata pool
        synapse_metadata_pool_config_t meta_config = synapse_metadata_pool_default_config();
        meta_config.pool_size = 1000;
        meta_config.enable_statistics = true;
        meta_config.thread_safe = true;

        metadata_pool = synapse_metadata_pool_create(&meta_config);
        ASSERT_NE(metadata_pool, nullptr) << "Failed to create metadata pool";
    }

    void TearDown() override {
        if (metadata_pool) {
            synapse_metadata_pool_destroy(metadata_pool);
            metadata_pool = nullptr;
        }
        if (pool) {
            sparse_synapse_pool_destroy(pool);
            pool = nullptr;
        }
    }

    // Helper: Convert float weight to ternary
    trit_t FloatToTernary(float weight, float threshold = 0.3f) {
        return trit_from_float_threshold(weight, threshold);
    }
};

//=============================================================================
// Test Category 1: Ternary Sparse Synapse with NeuralNet Module
//=============================================================================

TEST_F(SparseTernaryIntegrationTest, BasicSparseSynapseWithTernaryWeights) {
    // Initialize sparse synapse storage
    sparse_synapse_storage_t storage;
    sparse_synapse_storage_init(&storage);

    // Add synapses with weights that will be quantized to ternary
    float weights[] = {0.8f, -0.7f, 0.1f, -0.05f, 0.9f, -0.85f, 0.0f, 0.3f};
    const size_t num_weights = sizeof(weights) / sizeof(weights[0]);

    for (size_t i = 0; i < num_weights; i++) {
        int result = sparse_synapse_add(pool, &storage, (uint32_t)i + 10, weights[i]);
        EXPECT_EQ(result, 0) << "Failed to add synapse " << i;
    }

    // Verify synapse count
    EXPECT_EQ(sparse_synapse_count(&storage), num_weights);

    // Quantize weights to ternary and verify
    float threshold = 0.3f;
    for (size_t i = 0; i < num_weights; i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, (uint32_t)i);
        ASSERT_NE(handle, nullptr);

        trit_t ternary = FloatToTernary(handle->weight, threshold);
        trit_t expected = trit_from_float_threshold(weights[i], threshold);
        EXPECT_EQ(ternary, expected) << "Ternary mismatch for weight " << weights[i];
    }

    // Cleanup
    sparse_synapse_storage_cleanup(pool, &storage);
}

TEST_F(SparseTernaryIntegrationTest, SparseTernaryWeightDistribution) {
    // Test that sparse storage correctly handles ternary weight distribution
    sparse_synapse_storage_t storage;
    sparse_synapse_storage_init(&storage);

    // Add many synapses with known ternary distribution
    const size_t num_synapses = 100;
    int positive_count = 0;
    int negative_count = 0;
    int zero_count = 0;

    for (size_t i = 0; i < num_synapses; i++) {
        float weight;
        if (i % 3 == 0) {
            weight = 0.8f;  // Will become TRIT_POSITIVE
            positive_count++;
        } else if (i % 3 == 1) {
            weight = -0.7f; // Will become TRIT_NEGATIVE
            negative_count++;
        } else {
            weight = 0.1f;  // Will become TRIT_UNKNOWN (within threshold)
            zero_count++;
        }

        int result = sparse_synapse_add(pool, &storage, (uint32_t)i + 100, weight);
        EXPECT_EQ(result, 0);
    }

    // Verify distribution using ternary vector
    trit_vector_t* ternary_weights = trit_vector_create(num_synapses, TERNARY_PACK_NONE);
    ASSERT_NE(ternary_weights, nullptr);

    for (size_t i = 0; i < num_synapses; i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, (uint32_t)i);
        ASSERT_NE(handle, nullptr);

        trit_t ternary = FloatToTernary(handle->weight, 0.3f);
        trit_vector_set(ternary_weights, i, ternary);
    }

    // Count ternary values
    size_t n_pos, n_unk, n_neg;
    trit_vector_count(ternary_weights, &n_pos, &n_unk, &n_neg);

    EXPECT_EQ((int)n_pos, positive_count);
    EXPECT_EQ((int)n_neg, negative_count);
    EXPECT_EQ((int)n_unk, zero_count);

    // Cleanup
    trit_vector_destroy(ternary_weights);
    sparse_synapse_storage_cleanup(pool, &storage);
}

TEST_F(SparseTernaryIntegrationTest, SparseOverflowWithTernaryWeights) {
    // Test overflow behavior with ternary weights
    sparse_synapse_storage_t storage;
    sparse_synapse_storage_init(&storage);

    // Add more than embedded capacity (64) synapses
    const size_t num_synapses = 100;  // > SPARSE_SYNAPSE_EMBEDDED_CAPACITY

    for (size_t i = 0; i < num_synapses; i++) {
        float weight = (float)(i % 3 - 1);  // -1, 0, 1 cycling
        int result = sparse_synapse_add(pool, &storage, (uint32_t)i + 200, weight);
        EXPECT_EQ(result, 0) << "Failed to add synapse at overflow " << i;
    }

    // Verify count includes overflow
    EXPECT_EQ(sparse_synapse_count(&storage), num_synapses);
    EXPECT_GT(storage.overflow_count, 0u) << "Should have overflow synapses";

    // Verify ternary values in both embedded and overflow
    for (size_t i = 0; i < num_synapses; i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, (uint32_t)i);
        ASSERT_NE(handle, nullptr);

        // Weight pattern: -1, 0, 1, -1, 0, 1, ...
        int expected_sign = (int)(i % 3) - 1;
        trit_t expected_ternary = trit_from_int(expected_sign);
        trit_t actual_ternary = trit_from_int((int)handle->weight);

        EXPECT_EQ(actual_ternary, expected_ternary) << "Ternary mismatch at index " << i;
    }

    // Cleanup
    sparse_synapse_storage_cleanup(pool, &storage);
}

TEST_F(SparseTernaryIntegrationTest, SparseSynapseIteratorWithTernary) {
    // Test iterator with ternary weight conversion
    sparse_synapse_storage_t storage;
    sparse_synapse_storage_init(&storage);

    // Add synapses
    const size_t num_synapses = 50;
    for (size_t i = 0; i < num_synapses; i++) {
        float weight = 0.5f * sinf((float)i * 0.5f);  // Varying weights
        sparse_synapse_add(pool, &storage, (uint32_t)i + 300, weight);
    }

    // Iterate and collect ternary weights
    sparse_synapse_iterator_t it;
    sparse_synapse_iterator_init(&it, &storage);

    std::vector<trit_t> ternary_weights;
    synapse_handle_t* handle;
    while ((handle = sparse_synapse_iterator_next(&it)) != nullptr) {
        trit_t ternary = FloatToTernary(handle->weight, 0.2f);
        ternary_weights.push_back(ternary);
    }

    EXPECT_EQ(ternary_weights.size(), num_synapses);

    // Verify iterator covered all synapses
    EXPECT_FALSE(sparse_synapse_iterator_has_next(&it));

    // Cleanup
    sparse_synapse_storage_cleanup(pool, &storage);
}

//=============================================================================
// Test Category 2: Sparse Operations with Cortical Columns
//=============================================================================

TEST_F(SparseTernaryIntegrationTest, CorticalColumnSparseTernary) {
    // Simulate cortical column with sparse ternary connectivity
    const size_t column_neurons = 20;
    std::vector<sparse_synapse_storage_t> storages(column_neurons);

    // Initialize all storages
    for (size_t i = 0; i < column_neurons; i++) {
        sparse_synapse_storage_init(&storages[i]);
    }

    // Create sparse connectivity pattern (10% connectivity)
    for (size_t from = 0; from < column_neurons; from++) {
        for (size_t to = 0; to < column_neurons; to++) {
            if (from == to) continue;  // No self-connections

            // 10% connectivity
            if ((from + to) % 10 == 0) {
                // Determine ternary weight based on neuron types
                trit_t ternary_weight;
                if (from < column_neurons / 2) {
                    // Excitatory neurons in upper half
                    ternary_weight = TRIT_POSITIVE;
                } else {
                    // Inhibitory neurons in lower half
                    ternary_weight = TRIT_NEGATIVE;
                }

                float weight = trit_to_float_scaled(ternary_weight, 1.0f);
                sparse_synapse_add(pool, &storages[from], (uint32_t)to, weight);
            }
        }
    }

    // Verify connectivity pattern
    size_t total_synapses = 0;
    size_t excitatory_count = 0;
    size_t inhibitory_count = 0;

    for (size_t i = 0; i < column_neurons; i++) {
        uint32_t count = sparse_synapse_count(&storages[i]);
        total_synapses += count;

        // Count excitatory vs inhibitory
        for (uint32_t j = 0; j < count; j++) {
            synapse_handle_t* handle = sparse_synapse_get(&storages[i], j);
            if (handle->weight > 0) {
                excitatory_count++;
            } else if (handle->weight < 0) {
                inhibitory_count++;
            }
        }
    }

    EXPECT_GT(total_synapses, 0u) << "Should have some synapses";
    EXPECT_GT(excitatory_count, 0u) << "Should have excitatory synapses";
    EXPECT_GT(inhibitory_count, 0u) << "Should have inhibitory synapses";

    // Cleanup
    for (size_t i = 0; i < column_neurons; i++) {
        sparse_synapse_storage_cleanup(pool, &storages[i]);
    }
}

TEST_F(SparseTernaryIntegrationTest, TernaryMatrixToSparseSynapses) {
    // Create ternary weight matrix and convert to sparse synapses
    const size_t rows = 10;  // Pre-synaptic neurons
    const size_t cols = 10;  // Post-synaptic neurons

    trit_matrix_t* weight_matrix = trit_matrix_create(rows, cols, TERNARY_PACK_NONE);
    ASSERT_NE(weight_matrix, nullptr);

    // Create sparse connectivity pattern in matrix
    for (size_t r = 0; r < rows; r++) {
        for (size_t c = 0; c < cols; c++) {
            trit_t val;
            if (r == c) {
                val = TRIT_POSITIVE;  // Self-excitation (diagonal)
            } else if ((r + c) % 4 == 0) {
                val = TRIT_NEGATIVE;  // Sparse inhibition
            } else {
                val = TRIT_UNKNOWN;   // No connection (sparse)
            }
            trit_matrix_set(weight_matrix, r, c, val);
        }
    }

    // Convert matrix to sparse synapse storage
    std::vector<sparse_synapse_storage_t> storages(rows);
    for (size_t i = 0; i < rows; i++) {
        sparse_synapse_storage_init(&storages[i]);
    }

    // Add only non-zero connections
    size_t nonzero_count = 0;
    for (size_t r = 0; r < rows; r++) {
        for (size_t c = 0; c < cols; c++) {
            trit_t weight = trit_matrix_get(weight_matrix, r, c);
            if (weight != TRIT_UNKNOWN) {
                float float_weight = trit_to_float(weight);
                sparse_synapse_add(pool, &storages[r], (uint32_t)c, float_weight);
                nonzero_count++;
            }
        }
    }

    // Verify sparsity is preserved
    float matrix_sparsity = trit_matrix_sparsity(weight_matrix);
    size_t total_sparse_synapses = 0;
    for (size_t i = 0; i < rows; i++) {
        total_sparse_synapses += sparse_synapse_count(&storages[i]);
    }

    EXPECT_EQ(total_sparse_synapses, nonzero_count);
    EXPECT_GT(matrix_sparsity, 0.5f) << "Matrix should be sparse";

    // Cleanup
    for (size_t i = 0; i < rows; i++) {
        sparse_synapse_storage_cleanup(pool, &storages[i]);
    }
    trit_matrix_destroy(weight_matrix);
}

TEST_F(SparseTernaryIntegrationTest, SparsePruningWithTernary) {
    // Test synapse pruning based on ternary values
    sparse_synapse_storage_t storage;
    sparse_synapse_storage_init(&storage);

    // Add synapses with various weights
    const size_t initial_count = 50;
    for (size_t i = 0; i < initial_count; i++) {
        float weight = 0.5f * (float)(rand() % 100 - 50) / 50.0f;
        sparse_synapse_add(pool, &storage, (uint32_t)i + 400, weight);
    }

    // Prune synapses that would become TRIT_UNKNOWN (zero)
    float threshold = 0.3f;
    std::vector<uint32_t> to_remove;

    for (uint32_t i = 0; i < sparse_synapse_count(&storage); i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, i);
        if (handle) {
            trit_t ternary = FloatToTernary(handle->weight, threshold);
            if (ternary == TRIT_UNKNOWN) {
                to_remove.push_back(i);
            }
        }
    }

    // Remove synapses (in reverse order to maintain indices)
    for (int i = (int)to_remove.size() - 1; i >= 0; i--) {
        sparse_synapse_remove(pool, &storage, to_remove[i]);
    }

    // Verify all remaining synapses have non-zero ternary weights
    uint32_t remaining = sparse_synapse_count(&storage);
    for (uint32_t i = 0; i < remaining; i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, i);
        ASSERT_NE(handle, nullptr);
        trit_t ternary = FloatToTernary(handle->weight, threshold);
        EXPECT_NE(ternary, TRIT_UNKNOWN) << "All remaining should be non-zero";
    }

    // Cleanup
    sparse_synapse_storage_cleanup(pool, &storage);
}

TEST_F(SparseTernaryIntegrationTest, SparseMetadataWithTernary) {
    // Test sparse synapses with metadata and ternary weights
    sparse_synapse_storage_t storage;
    sparse_synapse_storage_init(&storage);

    // Add synapses with metadata
    const size_t num_synapses = 10;
    for (size_t i = 0; i < num_synapses; i++) {
        trit_t ternary = (i % 3 == 0) ? TRIT_POSITIVE :
                         (i % 3 == 1) ? TRIT_NEGATIVE : TRIT_UNKNOWN;
        float weight = trit_to_float(ternary);

        // Use add_with_metadata for full synapse_t features
        int result = sparse_synapse_add_with_metadata(
            pool, metadata_pool, &storage,
            (uint32_t)i + 500, weight, 0  // synapse_type = 0 (default)
        );
        EXPECT_EQ(result, 0) << "Failed to add synapse with metadata";
    }

    // Verify metadata is accessible
    for (size_t i = 0; i < num_synapses; i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, (uint32_t)i);
        ASSERT_NE(handle, nullptr);

        if (handle->metadata_index != SPARSE_SYNAPSE_NO_METADATA) {
            struct synapse_t* full_synapse = sparse_synapse_get_metadata(metadata_pool, handle);
            EXPECT_NE(full_synapse, nullptr) << "Should have metadata";
        }
    }

    // Cleanup
    for (size_t i = 0; i < sparse_synapse_count(&storage); i++) {
        sparse_synapse_remove_with_metadata(pool, metadata_pool, &storage, 0);
    }
    sparse_synapse_storage_cleanup(pool, &storage);
}

//=============================================================================
// Test: Pool Statistics with Ternary Operations
//=============================================================================

TEST_F(SparseTernaryIntegrationTest, PoolStatisticsWithTernary) {
    // Create multiple storages and track pool statistics
    const size_t num_neurons = 50;
    std::vector<sparse_synapse_storage_t> storages(num_neurons);

    for (size_t i = 0; i < num_neurons; i++) {
        sparse_synapse_storage_init(&storages[i]);
    }

    // Add varying numbers of synapses per neuron
    for (size_t i = 0; i < num_neurons; i++) {
        size_t num_synapses = 10 + (i % 80);  // 10-90 synapses per neuron
        for (size_t j = 0; j < num_synapses; j++) {
            trit_t ternary = (j % 2 == 0) ? TRIT_POSITIVE : TRIT_NEGATIVE;
            float weight = trit_to_float(ternary);
            sparse_synapse_add(pool, &storages[i], (uint32_t)j, weight);
        }
    }

    // Get pool statistics
    sparse_synapse_stats_t stats;
    int result = sparse_synapse_pool_get_stats(pool, &stats);
    EXPECT_EQ(result, 0);

    // Verify statistics
    EXPECT_GT(stats.total_synapses, 0u);
    EXPECT_GE(stats.memory_savings_percent, 0.0f);

    // Cleanup
    for (size_t i = 0; i < num_neurons; i++) {
        sparse_synapse_storage_cleanup(pool, &storages[i]);
    }
}

TEST_F(SparseTernaryIntegrationTest, CompactStorageWithTernary) {
    // Test storage compaction after pruning
    sparse_synapse_storage_t storage;
    sparse_synapse_storage_init(&storage);

    // Add more than embedded capacity
    const size_t initial_count = 100;
    for (size_t i = 0; i < initial_count; i++) {
        float weight = (i % 2 == 0) ? 1.0f : -1.0f;
        sparse_synapse_add(pool, &storage, (uint32_t)i + 600, weight);
    }

    EXPECT_GT(storage.overflow_count, 0u) << "Should have overflow before compact";

    // Remove most synapses (keep only first 30)
    while (sparse_synapse_count(&storage) > 30) {
        sparse_synapse_remove(pool, &storage, sparse_synapse_count(&storage) - 1);
    }

    // Compact storage
    uint32_t compacted = sparse_synapse_compact(pool, &storage);

    // After compaction with <=64 synapses, overflow should be freed
    EXPECT_LE(sparse_synapse_count(&storage), SPARSE_SYNAPSE_EMBEDDED_CAPACITY);

    // Verify all synapses still have valid ternary representations
    for (uint32_t i = 0; i < sparse_synapse_count(&storage); i++) {
        synapse_handle_t* handle = sparse_synapse_get(&storage, i);
        ASSERT_NE(handle, nullptr);

        trit_t ternary = trit_from_float_sign(handle->weight);
        EXPECT_NE(ternary, TRIT_UNKNOWN) << "Weight should be non-zero";
    }

    // Cleanup
    sparse_synapse_storage_cleanup(pool, &storage);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
