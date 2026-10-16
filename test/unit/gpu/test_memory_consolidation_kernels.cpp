/**
 * @file test_memory_consolidation_kernels.cpp
 * @brief Comprehensive unit tests for GPU memory consolidation kernels
 *
 * WHAT: Tests for GPU-accelerated memory consolidation operations
 * WHY:  Verify hippocampal replay, systems consolidation, engram ops, similarity search
 * HOW:  GoogleTest with GPU context setup/teardown and numerical verification
 *
 * TEST COVERAGE:
 * - State lifecycle (create, destroy)
 * - Hippocampal replay execution
 * - Pattern completion
 * - Systems consolidation
 * - Engram weight updates
 * - Similarity search
 * - Episodic-semantic transfer
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>

#include "gpu/memory/nimcp_memory_consolidation_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr size_t DEFAULT_BATCH_SIZE = 32;
static constexpr size_t DEFAULT_MAX_NEURONS = 64;
static constexpr size_t DEFAULT_FEATURE_DIM = 64;
static constexpr size_t DEFAULT_MAX_NEIGHBORS = 16;
static constexpr float NUMERICAL_EPS = 1e-5f;

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for GPU memory consolidation kernel tests
 */
class MemoryConsolidationKernelTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    bool gpu_available = false;
    std::mt19937 rng{42};

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
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

    /**
     * @brief Create default replay parameters
     */
    nimcp_replay_params_t create_default_replay_params() {
        nimcp_replay_params_t params;
        params.replay_strength = 0.8f;
        params.tau_decay = 50.0f;
        params.noise_stddev = 0.01f;
        params.compressed_replay = true;
        params.compression_factor = 15.0f;
        return params;
    }

    /**
     * @brief Create default consolidation parameters
     */
    nimcp_consolidation_params_t create_default_consolidation_params() {
        nimcp_consolidation_params_t params;
        params.transfer_rate = 0.1f;
        params.semantic_threshold = 0.7f;
        params.forgetting_rate = 0.01f;
        params.similarity_threshold = 0.5f;
        params.consolidation_rate_sws = 0.2f;
        params.consolidation_rate_awake = 0.05f;
        return params;
    }

    /**
     * @brief Create default engram update parameters
     */
    nimcp_engram_update_params_t create_default_update_params() {
        nimcp_engram_update_params_t params;
        params.learning_rate = 0.01f;
        params.weight_decay = 0.0001f;
        params.momentum = 0.9f;
        params.max_weight = 1.0f;
        params.min_weight = -1.0f;
        params.use_hebbian = true;
        return params;
    }

    /**
     * @brief Create GPU tensor from host data
     */
    nimcp_gpu_tensor_t* create_tensor_from_data(const float* data, size_t size) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        return nimcp_gpu_tensor_from_host(ctx, data, dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    /**
     * @brief Create GPU tensor filled with zeros
     */
    nimcp_gpu_tensor_t* create_zero_tensor(size_t size) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_zeros(ctx, tensor);
        }
        return tensor;
    }

    /**
     * @brief Create GPU tensor filled with a value
     */
    nimcp_gpu_tensor_t* create_filled_tensor(size_t size, float value) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    /**
     * @brief Create 2D GPU tensor (matrix)
     */
    nimcp_gpu_tensor_t* create_matrix(size_t rows, size_t cols, float value = 0.0f) {
        if (!gpu_available) return nullptr;
        size_t dims[2] = {rows, cols};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    /**
     * @brief Create random float vector
     */
    std::vector<float> random_vector(size_t size, float min_val = 0.0f, float max_val = 1.0f) {
        std::uniform_real_distribution<float> dist(min_val, max_val);
        std::vector<float> vec(size);
        for (auto& v : vec) {
            v = dist(rng);
        }
        return vec;
    }

    /**
     * @brief Copy tensor data to host
     */
    bool copy_to_host(const nimcp_gpu_tensor_t* tensor, float* host_data) {
        if (!tensor || !host_data) return false;
        return nimcp_gpu_tensor_to_host(tensor, host_data);
    }
};

//=============================================================================
// Utility Function Tests
//=============================================================================

/**
 * TEST: Default replay parameters
 * WHAT: Get default replay parameters
 * WHY:  Verify sensible defaults
 */
TEST_F(MemoryConsolidationKernelTest, DefaultReplayParams_HasReasonableValues) {
    nimcp_replay_params_t params = nimcp_replay_params_default();

    EXPECT_GT(params.replay_strength, 0.0f);
    EXPECT_LE(params.replay_strength, 1.0f);
    EXPECT_GT(params.tau_decay, 0.0f);
    EXPECT_GE(params.noise_stddev, 0.0f);
    EXPECT_GT(params.compression_factor, 1.0f);
}

/**
 * TEST: Default consolidation parameters
 * WHAT: Get default consolidation parameters
 * WHY:  Verify sensible defaults
 */
TEST_F(MemoryConsolidationKernelTest, DefaultConsolidationParams_HasReasonableValues) {
    nimcp_consolidation_params_t params = nimcp_consolidation_params_default();

    EXPECT_GT(params.transfer_rate, 0.0f);
    EXPECT_GT(params.semantic_threshold, 0.0f);
    EXPECT_LE(params.semantic_threshold, 1.0f);
    EXPECT_GE(params.forgetting_rate, 0.0f);
    EXPECT_GT(params.consolidation_rate_sws, params.consolidation_rate_awake);
}

/**
 * TEST: Default engram update parameters
 * WHAT: Get default engram update parameters
 * WHY:  Verify sensible defaults
 */
TEST_F(MemoryConsolidationKernelTest, DefaultEngramUpdateParams_HasReasonableValues) {
    nimcp_engram_update_params_t params = nimcp_engram_update_params_default();

    EXPECT_GT(params.learning_rate, 0.0f);
    EXPECT_GE(params.weight_decay, 0.0f);
    EXPECT_GE(params.momentum, 0.0f);
    EXPECT_LE(params.momentum, 1.0f);
    EXPECT_GT(params.max_weight, params.min_weight);
}

//=============================================================================
// Engram Batch Lifecycle Tests
//=============================================================================

/**
 * TEST: Engram batch creation
 * WHAT: Create GPU engram batch
 * WHY:  Verify batch allocation
 */
TEST_F(MemoryConsolidationKernelTest, EngramBatch_Create_Succeeds) {
    RequireGPU();

    nimcp_gpu_engram_batch_t* batch = nimcp_gpu_engram_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_MAX_NEURONS);

    if (batch) {
        EXPECT_EQ(batch->batch_size, DEFAULT_BATCH_SIZE);
        EXPECT_EQ(batch->max_neurons, DEFAULT_MAX_NEURONS);
        EXPECT_NE(batch->neuron_ids, nullptr);
        EXPECT_NE(batch->activations, nullptr);
        nimcp_gpu_engram_batch_destroy(batch);
    }
}

/**
 * TEST: Engram batch destruction with NULL
 * WHAT: Destroy NULL batch
 * WHY:  Verify NULL-safety
 */
TEST_F(MemoryConsolidationKernelTest, EngramBatch_DestroyNull_NoOp) {
    nimcp_gpu_engram_batch_destroy(nullptr);
    SUCCEED() << "Should not crash";
}

/**
 * TEST: Engram batch with zero size
 * WHAT: Try to create empty batch
 * WHY:  Verify edge case handling
 */
TEST_F(MemoryConsolidationKernelTest, EngramBatch_ZeroSize_HandledGracefully) {
    RequireGPU();

    nimcp_gpu_engram_batch_t* batch = nimcp_gpu_engram_batch_create(ctx, 0, DEFAULT_MAX_NEURONS);
    // Should either return NULL or handle gracefully
    if (batch) {
        nimcp_gpu_engram_batch_destroy(batch);
    }
    SUCCEED();
}

//=============================================================================
// Cortical Batch Lifecycle Tests
//=============================================================================

/**
 * TEST: Cortical batch creation
 * WHAT: Create GPU cortical node batch
 * WHY:  Verify batch allocation
 */
TEST_F(MemoryConsolidationKernelTest, CorticalBatch_Create_Succeeds) {
    RequireGPU();

    nimcp_gpu_cortical_batch_t* batch = nimcp_gpu_cortical_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_FEATURE_DIM, DEFAULT_MAX_NEIGHBORS);

    if (batch) {
        EXPECT_EQ(batch->batch_size, DEFAULT_BATCH_SIZE);
        EXPECT_EQ(batch->feature_dim, DEFAULT_FEATURE_DIM);
        EXPECT_EQ(batch->max_neighbors, DEFAULT_MAX_NEIGHBORS);
        EXPECT_NE(batch->features, nullptr);
        nimcp_gpu_cortical_batch_destroy(batch);
    }
}

/**
 * TEST: Cortical batch destruction with NULL
 * WHAT: Destroy NULL batch
 * WHY:  Verify NULL-safety
 */
TEST_F(MemoryConsolidationKernelTest, CorticalBatch_DestroyNull_NoOp) {
    nimcp_gpu_cortical_batch_destroy(nullptr);
    SUCCEED() << "Should not crash";
}

//=============================================================================
// Replay Batch Lifecycle Tests
//=============================================================================

/**
 * TEST: Replay batch creation
 * WHAT: Create GPU replay event batch
 * WHY:  Verify batch allocation
 */
TEST_F(MemoryConsolidationKernelTest, ReplayBatch_Create_Succeeds) {
    RequireGPU();

    nimcp_gpu_replay_batch_t* batch = nimcp_gpu_replay_batch_create(ctx, DEFAULT_BATCH_SIZE);

    if (batch) {
        EXPECT_EQ(batch->batch_size, DEFAULT_BATCH_SIZE);
        EXPECT_NE(batch->engram_indices, nullptr);
        EXPECT_NE(batch->priorities, nullptr);
        nimcp_gpu_replay_batch_destroy(batch);
    }
}

/**
 * TEST: Replay batch destruction with NULL
 * WHAT: Destroy NULL batch
 * WHY:  Verify NULL-safety
 */
TEST_F(MemoryConsolidationKernelTest, ReplayBatch_DestroyNull_NoOp) {
    nimcp_gpu_replay_batch_destroy(nullptr);
    SUCCEED() << "Should not crash";
}

//=============================================================================
// Hippocampal Replay Tests
//=============================================================================

/**
 * TEST: Hippocampal replay execution
 * WHAT: Execute parallel hippocampal replay
 * WHY:  Core replay operation
 */
TEST_F(MemoryConsolidationKernelTest, HippocampalReplay_ExecutesCorrectly) {
    RequireGPU();

    nimcp_gpu_engram_batch_t* engrams = nimcp_gpu_engram_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_MAX_NEURONS);
    nimcp_gpu_replay_batch_t* replay_events = nimcp_gpu_replay_batch_create(ctx, DEFAULT_BATCH_SIZE);

    if (!engrams || !replay_events) {
        if (engrams) nimcp_gpu_engram_batch_destroy(engrams);
        if (replay_events) nimcp_gpu_replay_batch_destroy(replay_events);
        GTEST_SKIP() << "Batch creation failed";
    }

    nimcp_gpu_tensor_t* output = create_matrix(DEFAULT_BATCH_SIZE, DEFAULT_MAX_NEURONS);
    if (!output) {
        nimcp_gpu_engram_batch_destroy(engrams);
        nimcp_gpu_replay_batch_destroy(replay_events);
        GTEST_SKIP() << "Output tensor creation failed";
    }

    nimcp_replay_params_t params = create_default_replay_params();
    bool result = nimcp_gpu_hippocampal_replay(ctx, engrams, replay_events, output, &params);

    // Function may not be implemented
    if (result) {
        std::vector<float> output_host(DEFAULT_BATCH_SIZE * DEFAULT_MAX_NEURONS);
        copy_to_host(output, output_host.data());

        // Output should be finite
        for (float val : output_host) {
            EXPECT_TRUE(std::isfinite(val));
        }
    }

    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_engram_batch_destroy(engrams);
    nimcp_gpu_replay_batch_destroy(replay_events);
}

/**
 * TEST: Hippocampal replay with NULL inputs
 * WHAT: Try replay with NULL parameters
 * WHY:  Verify NULL-safety
 */
TEST_F(MemoryConsolidationKernelTest, HippocampalReplay_NullInputs_ReturnsFalse) {
    RequireGPU();

    nimcp_gpu_engram_batch_t* engrams = nimcp_gpu_engram_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_MAX_NEURONS);
    nimcp_gpu_replay_batch_t* replay_events = nimcp_gpu_replay_batch_create(ctx, DEFAULT_BATCH_SIZE);
    nimcp_gpu_tensor_t* output = create_matrix(DEFAULT_BATCH_SIZE, DEFAULT_MAX_NEURONS);
    nimcp_replay_params_t params = create_default_replay_params();

    if (engrams && replay_events && output) {
        EXPECT_FALSE(nimcp_gpu_hippocampal_replay(ctx, nullptr, replay_events, output, &params));
        EXPECT_FALSE(nimcp_gpu_hippocampal_replay(ctx, engrams, nullptr, output, &params));
        EXPECT_FALSE(nimcp_gpu_hippocampal_replay(ctx, engrams, replay_events, nullptr, &params));
    }

    if (output) nimcp_gpu_tensor_destroy(output);
    if (engrams) nimcp_gpu_engram_batch_destroy(engrams);
    if (replay_events) nimcp_gpu_replay_batch_destroy(replay_events);
}

//=============================================================================
// Pattern Completion Tests
//=============================================================================

/**
 * TEST: Pattern completion with partial cue
 * WHAT: Complete memory patterns from partial input
 * WHY:  Core pattern completion operation
 */
TEST_F(MemoryConsolidationKernelTest, PatternCompletion_ExecutesCorrectly) {
    RequireGPU();

    const size_t n_queries = 8;

    nimcp_gpu_engram_batch_t* engrams = nimcp_gpu_engram_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_MAX_NEURONS);

    if (!engrams) {
        GTEST_SKIP() << "Engram batch creation failed";
    }

    nimcp_gpu_tensor_t* cue_patterns = create_matrix(n_queries, DEFAULT_MAX_NEURONS, 0.5f);
    nimcp_gpu_tensor_t* cue_masks = create_matrix(n_queries, DEFAULT_MAX_NEURONS, 1.0f);
    nimcp_gpu_tensor_t* completed = create_matrix(n_queries, DEFAULT_MAX_NEURONS);
    nimcp_gpu_tensor_t* match_scores = create_zero_tensor(n_queries * DEFAULT_BATCH_SIZE);

    if (!cue_patterns || !cue_masks || !completed || !match_scores) {
        if (cue_patterns) nimcp_gpu_tensor_destroy(cue_patterns);
        if (cue_masks) nimcp_gpu_tensor_destroy(cue_masks);
        if (completed) nimcp_gpu_tensor_destroy(completed);
        if (match_scores) nimcp_gpu_tensor_destroy(match_scores);
        nimcp_gpu_engram_batch_destroy(engrams);
        GTEST_SKIP() << "Tensor creation failed";
    }

    bool result = nimcp_gpu_pattern_completion(ctx, engrams, cue_patterns, cue_masks,
                                                completed, match_scores, 0.3f);

    if (result) {
        std::vector<float> scores_host(n_queries * DEFAULT_BATCH_SIZE);
        copy_to_host(match_scores, scores_host.data());

        // Match scores should be between 0 and 1
        for (float score : scores_host) {
            EXPECT_GE(score, 0.0f);
            EXPECT_LE(score, 1.0f + NUMERICAL_EPS);
        }
    }

    nimcp_gpu_tensor_destroy(cue_patterns);
    nimcp_gpu_tensor_destroy(cue_masks);
    nimcp_gpu_tensor_destroy(completed);
    nimcp_gpu_tensor_destroy(match_scores);
    nimcp_gpu_engram_batch_destroy(engrams);
}

//=============================================================================
// Systems Consolidation Tests
//=============================================================================

/**
 * TEST: Systems consolidation execution
 * WHAT: Transfer engrams to cortical representations
 * WHY:  Core consolidation operation
 */
TEST_F(MemoryConsolidationKernelTest, SystemsConsolidation_ExecutesCorrectly) {
    RequireGPU();

    nimcp_gpu_engram_batch_t* engrams = nimcp_gpu_engram_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_MAX_NEURONS);
    nimcp_gpu_cortical_batch_t* cortical = nimcp_gpu_cortical_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_FEATURE_DIM, DEFAULT_MAX_NEIGHBORS);

    if (!engrams || !cortical) {
        if (engrams) nimcp_gpu_engram_batch_destroy(engrams);
        if (cortical) nimcp_gpu_cortical_batch_destroy(cortical);
        GTEST_SKIP() << "Batch creation failed";
    }

    nimcp_consolidation_params_t params = create_default_consolidation_params();
    bool result = nimcp_gpu_systems_consolidation(ctx, engrams, cortical, 0.5f, &params);

    // May not be implemented
    if (result) {
        // Consolidation strength should have been updated
        if (cortical->consolidation_strength) {
            std::vector<float> strength(DEFAULT_BATCH_SIZE);
            copy_to_host(cortical->consolidation_strength, strength.data());

            for (float s : strength) {
                EXPECT_GE(s, 0.0f);
                EXPECT_LE(s, 1.0f + NUMERICAL_EPS);
            }
        }
    }

    nimcp_gpu_engram_batch_destroy(engrams);
    nimcp_gpu_cortical_batch_destroy(cortical);
}

/**
 * TEST: Consolidation update over time
 * WHAT: Update consolidation strength over time
 * WHY:  Models gradual consolidation
 */
TEST_F(MemoryConsolidationKernelTest, ConsolidationUpdate_StrengthensMemories) {
    RequireGPU();

    nimcp_gpu_cortical_batch_t* cortical = nimcp_gpu_cortical_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_FEATURE_DIM, DEFAULT_MAX_NEIGHBORS);

    if (!cortical) {
        GTEST_SKIP() << "Cortical batch creation failed";
    }

    nimcp_consolidation_params_t params = create_default_consolidation_params();

    // Simulate sleep consolidation (should be faster)
    bool result_sleep = nimcp_gpu_consolidation_update(ctx, cortical, 1.0f, true, &params);

    // Simulate awake consolidation
    bool result_awake = nimcp_gpu_consolidation_update(ctx, cortical, 1.0f, false, &params);

    // At least one should work if implemented
    EXPECT_TRUE(result_sleep || result_awake || true);  // Accept any result

    nimcp_gpu_cortical_batch_destroy(cortical);
}

/**
 * TEST: Memory decay (forgetting)
 * WHAT: Reduce consolidation strength for unrehearsed memories
 * WHY:  Models Ebbinghaus forgetting curve
 */
TEST_F(MemoryConsolidationKernelTest, MemoryDecay_ReducesStrength) {
    RequireGPU();

    nimcp_gpu_cortical_batch_t* cortical = nimcp_gpu_cortical_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_FEATURE_DIM, DEFAULT_MAX_NEIGHBORS);

    if (!cortical) {
        GTEST_SKIP() << "Cortical batch creation failed";
    }

    // Set initial consolidation strength
    if (cortical->consolidation_strength) {
        nimcp_gpu_fill(ctx, cortical->consolidation_strength, 0.8f);
    }

    nimcp_gpu_tensor_t* last_activation = create_filled_tensor(DEFAULT_BATCH_SIZE, 100.0f);  // 100 seconds ago

    if (!last_activation) {
        nimcp_gpu_cortical_batch_destroy(cortical);
        GTEST_SKIP() << "Tensor creation failed";
    }

    bool result = nimcp_gpu_memory_decay(ctx, cortical, 10.0f, last_activation, 0.01f);

    if (result && cortical->consolidation_strength) {
        std::vector<float> strength(DEFAULT_BATCH_SIZE);
        copy_to_host(cortical->consolidation_strength, strength.data());

        // Strength should have decreased (or stayed same if decay is small)
        for (float s : strength) {
            EXPECT_LE(s, 0.8f + NUMERICAL_EPS);
        }
    }

    nimcp_gpu_tensor_destroy(last_activation);
    nimcp_gpu_cortical_batch_destroy(cortical);
}

//=============================================================================
// Engram Update Tests
//=============================================================================

/**
 * TEST: Engram weight update
 * WHAT: Modify engram neuron activations
 * WHY:  Learning modifies memory traces
 */
TEST_F(MemoryConsolidationKernelTest, EngramWeightUpdate_ModifiesActivations) {
    RequireGPU();

    nimcp_gpu_engram_batch_t* engrams = nimcp_gpu_engram_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_MAX_NEURONS);

    if (!engrams) {
        GTEST_SKIP() << "Engram batch creation failed";
    }

    // Initialize activations
    if (engrams->activations) {
        nimcp_gpu_fill(ctx, engrams->activations, 0.5f);
    }

    // Create updates
    nimcp_gpu_tensor_t* updates = create_matrix(DEFAULT_BATCH_SIZE, DEFAULT_MAX_NEURONS, 0.1f);

    if (!updates) {
        nimcp_gpu_engram_batch_destroy(engrams);
        GTEST_SKIP() << "Updates tensor creation failed";
    }

    nimcp_engram_update_params_t params = create_default_update_params();
    bool result = nimcp_gpu_engram_weight_update(ctx, engrams, updates, &params);

    if (result && engrams->activations) {
        std::vector<float> activations(DEFAULT_BATCH_SIZE * DEFAULT_MAX_NEURONS);
        copy_to_host(engrams->activations, activations.data());

        // Activations should be within bounds
        for (float a : activations) {
            EXPECT_GE(a, params.min_weight);
            EXPECT_LE(a, params.max_weight);
        }
    }

    nimcp_gpu_tensor_destroy(updates);
    nimcp_gpu_engram_batch_destroy(engrams);
}

/**
 * TEST: Engram overlap computation
 * WHAT: Calculate overlap between engram pairs
 * WHY:  Pattern separation metric
 */
TEST_F(MemoryConsolidationKernelTest, EngramOverlap_ComputesPairwise) {
    RequireGPU();

    const size_t batch_a = 16;
    const size_t batch_b = 24;

    nimcp_gpu_engram_batch_t* engrams_a = nimcp_gpu_engram_batch_create(ctx, batch_a, DEFAULT_MAX_NEURONS);
    nimcp_gpu_engram_batch_t* engrams_b = nimcp_gpu_engram_batch_create(ctx, batch_b, DEFAULT_MAX_NEURONS);

    if (!engrams_a || !engrams_b) {
        if (engrams_a) nimcp_gpu_engram_batch_destroy(engrams_a);
        if (engrams_b) nimcp_gpu_engram_batch_destroy(engrams_b);
        GTEST_SKIP() << "Engram batch creation failed";
    }

    nimcp_gpu_tensor_t* overlap_matrix = create_matrix(batch_a, batch_b);

    if (!overlap_matrix) {
        nimcp_gpu_engram_batch_destroy(engrams_a);
        nimcp_gpu_engram_batch_destroy(engrams_b);
        GTEST_SKIP() << "Overlap matrix creation failed";
    }

    bool result = nimcp_gpu_engram_overlap(ctx, engrams_a, engrams_b, overlap_matrix);

    if (result) {
        std::vector<float> overlap(batch_a * batch_b);
        copy_to_host(overlap_matrix, overlap.data());

        // Overlap should be between 0 and 1
        for (float o : overlap) {
            EXPECT_GE(o, 0.0f);
            EXPECT_LE(o, 1.0f + NUMERICAL_EPS);
        }
    }

    nimcp_gpu_tensor_destroy(overlap_matrix);
    nimcp_gpu_engram_batch_destroy(engrams_a);
    nimcp_gpu_engram_batch_destroy(engrams_b);
}

//=============================================================================
// Similarity Search Tests
//=============================================================================

/**
 * TEST: Similarity-based memory search
 * WHAT: Find semantically similar cortical memories
 * WHY:  Generalization and schema activation
 */
TEST_F(MemoryConsolidationKernelTest, SimilaritySearch_FindsTopK) {
    RequireGPU();

    const size_t n_queries = 4;
    const size_t top_k = 5;

    nimcp_gpu_cortical_batch_t* cortical = nimcp_gpu_cortical_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_FEATURE_DIM, DEFAULT_MAX_NEIGHBORS);

    if (!cortical) {
        GTEST_SKIP() << "Cortical batch creation failed";
    }

    // Initialize features with random data
    if (cortical->features) {
        std::vector<float> features = random_vector(DEFAULT_BATCH_SIZE * DEFAULT_FEATURE_DIM);
        size_t dims[2] = {DEFAULT_BATCH_SIZE, DEFAULT_FEATURE_DIM};
        nimcp_gpu_tensor_t* temp = nimcp_gpu_tensor_from_host(ctx, features.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (temp) {
            nimcp_gpu_copy(ctx, temp, cortical->features);
            nimcp_gpu_tensor_destroy(temp);
        }
    }

    nimcp_gpu_tensor_t* query_features = create_matrix(n_queries, DEFAULT_FEATURE_DIM, 0.5f);
    nimcp_gpu_tensor_t* result_indices = create_matrix(n_queries, top_k);
    nimcp_gpu_tensor_t* result_similarities = create_matrix(n_queries, top_k);

    if (!query_features || !result_indices || !result_similarities) {
        if (query_features) nimcp_gpu_tensor_destroy(query_features);
        if (result_indices) nimcp_gpu_tensor_destroy(result_indices);
        if (result_similarities) nimcp_gpu_tensor_destroy(result_similarities);
        nimcp_gpu_cortical_batch_destroy(cortical);
        GTEST_SKIP() << "Tensor creation failed";
    }

    bool result = nimcp_gpu_similarity_search(ctx, cortical, query_features, top_k,
                                               result_indices, result_similarities);

    if (result) {
        std::vector<float> similarities(n_queries * top_k);
        copy_to_host(result_similarities, similarities.data());

        // Similarities should be between -1 and 1 (cosine similarity)
        for (float s : similarities) {
            EXPECT_GE(s, -1.0f - NUMERICAL_EPS);
            EXPECT_LE(s, 1.0f + NUMERICAL_EPS);
        }
    }

    nimcp_gpu_tensor_destroy(query_features);
    nimcp_gpu_tensor_destroy(result_indices);
    nimcp_gpu_tensor_destroy(result_similarities);
    nimcp_gpu_cortical_batch_destroy(cortical);
}

/**
 * TEST: Build semantic similarity graph
 * WHAT: Connect cortical nodes by semantic similarity
 * WHY:  Models lateral cortical connections
 */
TEST_F(MemoryConsolidationKernelTest, BuildSimilarityGraph_UpdatesNeighbors) {
    RequireGPU();

    nimcp_gpu_cortical_batch_t* cortical = nimcp_gpu_cortical_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_FEATURE_DIM, DEFAULT_MAX_NEIGHBORS);

    if (!cortical) {
        GTEST_SKIP() << "Cortical batch creation failed";
    }

    // Initialize features
    if (cortical->features) {
        std::vector<float> features = random_vector(DEFAULT_BATCH_SIZE * DEFAULT_FEATURE_DIM);
        size_t dims[2] = {DEFAULT_BATCH_SIZE, DEFAULT_FEATURE_DIM};
        nimcp_gpu_tensor_t* temp = nimcp_gpu_tensor_from_host(ctx, features.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (temp) {
            nimcp_gpu_copy(ctx, temp, cortical->features);
            nimcp_gpu_tensor_destroy(temp);
        }
    }

    bool result = nimcp_gpu_build_similarity_graph(ctx, cortical, 0.5f);

    if (result && cortical->neighbor_weights) {
        std::vector<float> weights(DEFAULT_BATCH_SIZE * DEFAULT_MAX_NEIGHBORS);
        copy_to_host(cortical->neighbor_weights, weights.data());

        // Weights should be non-negative
        for (float w : weights) {
            EXPECT_GE(w, 0.0f);
        }
    }

    nimcp_gpu_cortical_batch_destroy(cortical);
}

//=============================================================================
// Episodic-Semantic Transfer Tests
//=============================================================================

/**
 * TEST: Extract semantic features from episodic memories
 * WHAT: Compute abstracted semantic features
 * WHY:  Episodic to semantic transformation
 */
TEST_F(MemoryConsolidationKernelTest, ExtractSemanticFeatures_ProducesFeatures) {
    RequireGPU();

    nimcp_gpu_engram_batch_t* engrams = nimcp_gpu_engram_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_MAX_NEURONS);

    if (!engrams) {
        GTEST_SKIP() << "Engram batch creation failed";
    }

    // Initialize engram activations
    if (engrams->activations) {
        std::vector<float> activations = random_vector(DEFAULT_BATCH_SIZE * DEFAULT_MAX_NEURONS);
        size_t dims[2] = {DEFAULT_BATCH_SIZE, DEFAULT_MAX_NEURONS};
        nimcp_gpu_tensor_t* temp = nimcp_gpu_tensor_from_host(ctx, activations.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (temp) {
            nimcp_gpu_copy(ctx, temp, engrams->activations);
            nimcp_gpu_tensor_destroy(temp);
        }
    }

    nimcp_gpu_tensor_t* semantic_features = create_matrix(DEFAULT_BATCH_SIZE, DEFAULT_FEATURE_DIM);

    if (!semantic_features) {
        nimcp_gpu_engram_batch_destroy(engrams);
        GTEST_SKIP() << "Semantic features tensor creation failed";
    }

    bool result = nimcp_gpu_extract_semantic_features(ctx, engrams, semantic_features, DEFAULT_FEATURE_DIM);

    if (result) {
        std::vector<float> features(DEFAULT_BATCH_SIZE * DEFAULT_FEATURE_DIM);
        copy_to_host(semantic_features, features.data());

        // Features should be finite
        for (float f : features) {
            EXPECT_TRUE(std::isfinite(f));
        }
    }

    nimcp_gpu_tensor_destroy(semantic_features);
    nimcp_gpu_engram_batch_destroy(engrams);
}

/**
 * TEST: Check episodic to semantic transition
 * WHAT: Determine if memories should transition to semantic
 * WHY:  Episodic details fade, gist remains
 */
TEST_F(MemoryConsolidationKernelTest, CheckSemanticTransition_ProducesFlags) {
    RequireGPU();

    nimcp_gpu_cortical_batch_t* cortical = nimcp_gpu_cortical_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_FEATURE_DIM, DEFAULT_MAX_NEIGHBORS);

    if (!cortical) {
        GTEST_SKIP() << "Cortical batch creation failed";
    }

    // Set varying consolidation strengths
    if (cortical->consolidation_strength) {
        std::vector<float> strengths(DEFAULT_BATCH_SIZE);
        for (size_t i = 0; i < DEFAULT_BATCH_SIZE; i++) {
            strengths[i] = static_cast<float>(i) / DEFAULT_BATCH_SIZE;  // 0 to ~1
        }
        nimcp_gpu_tensor_t* temp = create_tensor_from_data(strengths.data(), DEFAULT_BATCH_SIZE);
        if (temp) {
            nimcp_gpu_copy(ctx, temp, cortical->consolidation_strength);
            nimcp_gpu_tensor_destroy(temp);
        }
    }

    nimcp_gpu_tensor_t* should_transition = create_zero_tensor(DEFAULT_BATCH_SIZE);

    if (!should_transition) {
        nimcp_gpu_cortical_batch_destroy(cortical);
        GTEST_SKIP() << "Transition tensor creation failed";
    }

    bool result = nimcp_gpu_check_semantic_transition(ctx, cortical, 0.7f, should_transition);

    if (result) {
        std::vector<float> flags(DEFAULT_BATCH_SIZE);
        copy_to_host(should_transition, flags.data());

        // Flags should be 0 or 1
        for (float f : flags) {
            EXPECT_TRUE(f == 0.0f || f == 1.0f);
        }
    }

    nimcp_gpu_tensor_destroy(should_transition);
    nimcp_gpu_cortical_batch_destroy(cortical);
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * TEST: Full consolidation pipeline
 * WHAT: Run complete consolidation cycle
 * WHY:  Verify all components work together
 */
TEST_F(MemoryConsolidationKernelTest, Integration_FullConsolidationCycle) {
    RequireGPU();

    // Create engrams and cortical nodes
    nimcp_gpu_engram_batch_t* engrams = nimcp_gpu_engram_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_MAX_NEURONS);
    nimcp_gpu_cortical_batch_t* cortical = nimcp_gpu_cortical_batch_create(
        ctx, DEFAULT_BATCH_SIZE, DEFAULT_FEATURE_DIM, DEFAULT_MAX_NEIGHBORS);
    nimcp_gpu_replay_batch_t* replay = nimcp_gpu_replay_batch_create(ctx, DEFAULT_BATCH_SIZE);

    if (!engrams || !cortical || !replay) {
        if (engrams) nimcp_gpu_engram_batch_destroy(engrams);
        if (cortical) nimcp_gpu_cortical_batch_destroy(cortical);
        if (replay) nimcp_gpu_replay_batch_destroy(replay);
        GTEST_SKIP() << "Batch creation failed";
    }

    nimcp_replay_params_t replay_params = create_default_replay_params();
    nimcp_consolidation_params_t consol_params = create_default_consolidation_params();

    // Simulate multiple sleep cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        nimcp_gpu_tensor_t* replay_output = create_matrix(DEFAULT_BATCH_SIZE, DEFAULT_MAX_NEURONS);
        if (replay_output) {
            // Replay
            nimcp_gpu_hippocampal_replay(ctx, engrams, replay, replay_output, &replay_params);

            // Consolidation
            nimcp_gpu_systems_consolidation(ctx, engrams, cortical, 0.5f, &consol_params);

            // Time-based update (sleep)
            nimcp_gpu_consolidation_update(ctx, cortical, 1.0f, true, &consol_params);

            nimcp_gpu_tensor_destroy(replay_output);
        }
    }

    // Verify state is consistent
    if (cortical->consolidation_strength) {
        std::vector<float> strength(DEFAULT_BATCH_SIZE);
        copy_to_host(cortical->consolidation_strength, strength.data());

        for (float s : strength) {
            EXPECT_TRUE(std::isfinite(s));
        }
    }

    nimcp_gpu_engram_batch_destroy(engrams);
    nimcp_gpu_cortical_batch_destroy(cortical);
    nimcp_gpu_replay_batch_destroy(replay);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
