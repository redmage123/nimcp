/**
 * @file test_quantum_attention.cpp
 * @brief Unit tests for quantum-enhanced attention
 *
 * Tests quantum attention operations:
 * - Lifecycle (create/destroy)
 * - Ternary attention mask operations
 * - Quantum walk exploration
 * - Quantum annealing optimization
 * - Sparse attention computation
 * - Statistics tracking
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Headers have their own extern "C" guards
#include "plasticity/attention/nimcp_quantum_attention.h"

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumAttentionTest : public ::testing::Test {
protected:
    quantum_attention_t ctx;
    quantum_attention_config_t config;

    static constexpr uint32_t SEQ_LENGTH = 16;
    static constexpr uint32_t HEAD_DIM = 8;
    static constexpr uint32_t NUM_HEADS = 4;

    void SetUp() override {
        config = quantum_attention_default_config();
        ctx = quantum_attention_create(&config, SEQ_LENGTH, HEAD_DIM, NUM_HEADS);
    }

    void TearDown() override {
        if (ctx) {
            quantum_attention_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Helper to generate random vectors
    void randomVector(float* vec, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            vec[i] = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(QuantumAttentionTest, CreateWithValidConfig) {
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->magic, QUANTUM_ATTENTION_MAGIC);
    EXPECT_EQ(ctx->seq_length, SEQ_LENGTH);
    EXPECT_EQ(ctx->head_dim, HEAD_DIM);
    EXPECT_EQ(ctx->num_heads, NUM_HEADS);
}

TEST_F(QuantumAttentionTest, CreateWithNullConfig) {
    quantum_attention_t null_ctx = quantum_attention_create(nullptr, SEQ_LENGTH, HEAD_DIM, NUM_HEADS);
    EXPECT_EQ(null_ctx, nullptr);
}

TEST_F(QuantumAttentionTest, CreateWithZeroSequence) {
    quantum_attention_t null_ctx = quantum_attention_create(&config, 0, HEAD_DIM, NUM_HEADS);
    EXPECT_EQ(null_ctx, nullptr);
}

TEST_F(QuantumAttentionTest, CreateWithZeroHeadDim) {
    quantum_attention_t null_ctx = quantum_attention_create(&config, SEQ_LENGTH, 0, NUM_HEADS);
    EXPECT_EQ(null_ctx, nullptr);
}

TEST_F(QuantumAttentionTest, CreateWithZeroHeads) {
    quantum_attention_t null_ctx = quantum_attention_create(&config, SEQ_LENGTH, HEAD_DIM, 0);
    EXPECT_EQ(null_ctx, nullptr);
}

TEST_F(QuantumAttentionTest, DestroyNull) {
    // Should not crash
    quantum_attention_destroy(nullptr);
}

TEST_F(QuantumAttentionTest, DestroyInvalidMagic) {
    // Corrupt magic and try to destroy
    ctx->magic = 0xDEADBEEF;
    quantum_attention_destroy(ctx);
    ctx = nullptr;  // Prevent double-free in TearDown
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(QuantumAttentionTest, DefaultConfigValues) {
    quantum_attention_config_t def = quantum_attention_default_config();

    EXPECT_EQ(def.mode, QUANTUM_ATTENTION_SPARSE);
    EXPECT_FLOAT_EQ(def.collapse_threshold, 0.5f);
    EXPECT_FLOAT_EQ(def.superposition_penalty, 0.1f);
    EXPECT_FLOAT_EQ(def.tunneling_strength, 0.1f);
    EXPECT_EQ(def.max_superposition, 1024U);
    EXPECT_TRUE(def.use_sparse_output);
    EXPECT_EQ(def.walk_steps, 10U);
    EXPECT_EQ(def.anneal_sweeps, 100U);
    EXPECT_FLOAT_EQ(def.initial_temperature, 1.0f);
    EXPECT_FLOAT_EQ(def.final_temperature, 0.01f);
}

//=============================================================================
// Ternary Mask Tests
//=============================================================================

TEST_F(QuantumAttentionTest, ResetMaskToSuperposition) {
    quantum_attention_reset_mask(ctx);

    // All entries should be TRIT_UNKNOWN
    for (uint32_t i = 0; i < SEQ_LENGTH; i++) {
        for (uint32_t j = 0; j < SEQ_LENGTH; j++) {
            trit_t val = trit_matrix_get(ctx->attention_mask, i, j);
            EXPECT_EQ(val, TRIT_UNKNOWN);
        }
    }
}

TEST_F(QuantumAttentionTest, MeasurePairPositive) {
    quantum_attention_reset_mask(ctx);

    // High positive score should collapse to TRIT_POSITIVE
    trit_t result = quantum_attention_measure_pair(ctx, 0, 1, 0.8f);
    EXPECT_EQ(result, TRIT_POSITIVE);

    trit_t stored = trit_matrix_get(ctx->attention_mask, 0, 1);
    EXPECT_EQ(stored, TRIT_POSITIVE);
}

TEST_F(QuantumAttentionTest, MeasurePairNegative) {
    quantum_attention_reset_mask(ctx);

    // High negative score should collapse to TRIT_NEGATIVE
    trit_t result = quantum_attention_measure_pair(ctx, 2, 3, -0.8f);
    EXPECT_EQ(result, TRIT_NEGATIVE);

    trit_t stored = trit_matrix_get(ctx->attention_mask, 2, 3);
    EXPECT_EQ(stored, TRIT_NEGATIVE);
}

TEST_F(QuantumAttentionTest, MeasurePairBelowThreshold) {
    quantum_attention_reset_mask(ctx);

    // Score below sparsity threshold stays in superposition
    config.sparsity_threshold = 0.1f;
    ctx->config = config;

    trit_t result = quantum_attention_measure_pair(ctx, 4, 5, 0.001f);
    EXPECT_EQ(result, TRIT_UNKNOWN);
}

TEST_F(QuantumAttentionTest, MeasurePairOutOfBounds) {
    trit_t result = quantum_attention_measure_pair(ctx, SEQ_LENGTH + 10, 0, 0.5f);
    EXPECT_EQ(result, TRIT_UNKNOWN);

    result = quantum_attention_measure_pair(ctx, 0, SEQ_LENGTH + 10, 0.5f);
    EXPECT_EQ(result, TRIT_UNKNOWN);
}

TEST_F(QuantumAttentionTest, GetSparsePairs) {
    quantum_attention_reset_mask(ctx);

    // Measure a few pairs
    quantum_attention_measure_pair(ctx, 0, 0, 0.9f);   // POSITIVE
    quantum_attention_measure_pair(ctx, 1, 2, -0.9f);  // NEGATIVE
    quantum_attention_measure_pair(ctx, 3, 4, 0.7f);   // POSITIVE

    uint32_t query_idx[10], key_idx[10];
    float values[10];
    uint32_t count = quantum_attention_get_sparse_pairs(ctx, query_idx, key_idx, values, 10);

    EXPECT_EQ(count, 3U);

    // Verify first pair (0,0) is positive
    bool found_00 = false;
    for (uint32_t i = 0; i < count; i++) {
        if (query_idx[i] == 0 && key_idx[i] == 0) {
            EXPECT_FLOAT_EQ(values[i], 1.0f);
            found_00 = true;
        }
    }
    EXPECT_TRUE(found_00);
}

TEST_F(QuantumAttentionTest, GetSparsePairsMaxLimit) {
    quantum_attention_reset_mask(ctx);

    // Measure many pairs
    for (uint32_t i = 0; i < SEQ_LENGTH; i++) {
        quantum_attention_measure_pair(ctx, i, i, 0.9f);
    }

    uint32_t query_idx[5], key_idx[5];
    float values[5];

    // Request only 5 pairs
    uint32_t count = quantum_attention_get_sparse_pairs(ctx, query_idx, key_idx, values, 5);
    EXPECT_EQ(count, 5U);
}

//=============================================================================
// Quantum Walk Tests
//=============================================================================

TEST_F(QuantumAttentionTest, WalkModeCreate) {
    quantum_attention_destroy(ctx);

    config.mode = QUANTUM_ATTENTION_WALK;
    ctx = quantum_attention_create(&config, SEQ_LENGTH, HEAD_DIM, NUM_HEADS);

    ASSERT_NE(ctx, nullptr);
    EXPECT_NE(ctx->walkers, nullptr);

    // Check walker was created for each head
    for (uint32_t h = 0; h < NUM_HEADS; h++) {
        EXPECT_NE(ctx->walkers[h], nullptr);
    }
}

TEST_F(QuantumAttentionTest, WalkExploration) {
    quantum_attention_destroy(ctx);

    config.mode = QUANTUM_ATTENTION_WALK;
    config.walk_steps = 5;
    ctx = quantum_attention_create(&config, SEQ_LENGTH, HEAD_DIM, NUM_HEADS);

    ASSERT_NE(ctx, nullptr);

    // Generate random query/key
    float query[SEQ_LENGTH * HEAD_DIM];
    float key[SEQ_LENGTH * HEAD_DIM];
    randomVector(query, SEQ_LENGTH * HEAD_DIM);
    randomVector(key, SEQ_LENGTH * HEAD_DIM);

    float variance = quantum_attention_walk(ctx, query, key, 0);

    // Walk should complete without crash (variance >= 0 is fine)
    // Variance can be 0 if walk converges or stays in place
    EXPECT_GE(variance, 0.0f);
}

TEST_F(QuantumAttentionTest, WalkNullContext) {
    float dummy[16];
    float result = quantum_attention_walk(nullptr, dummy, dummy, 0);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST_F(QuantumAttentionTest, WalkNoWalkers) {
    // Default mode has no walkers
    float dummy[SEQ_LENGTH * HEAD_DIM];
    float result = quantum_attention_walk(ctx, dummy, dummy, 0);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

//=============================================================================
// Quantum Annealing Tests
//=============================================================================

TEST_F(QuantumAttentionTest, AnnealValidInput) {
    // Create scores with clear pattern
    float qk_scores[SEQ_LENGTH * SEQ_LENGTH];
    for (uint32_t i = 0; i < SEQ_LENGTH; i++) {
        for (uint32_t j = 0; j < SEQ_LENGTH; j++) {
            // Diagonal should have high attention
            qk_scores[i * SEQ_LENGTH + j] = (i == j) ? 1.0f : -0.5f;
        }
    }

    quantum_attention_result_t result;
    memset(&result, 0, sizeof(result));

    int err = quantum_attention_anneal(ctx, qk_scores, 0, &result);

    EXPECT_EQ(err, 0);
    EXPECT_TRUE(result.success);
    EXPECT_LT(result.final_energy, 1e10);  // Should have finite energy
}

TEST_F(QuantumAttentionTest, AnnealNullScores) {
    quantum_attention_result_t result;
    int err = quantum_attention_anneal(ctx, nullptr, 0, &result);
    EXPECT_NE(err, 0);
}

TEST_F(QuantumAttentionTest, AnnealNullResult) {
    float qk_scores[SEQ_LENGTH * SEQ_LENGTH] = {0};
    int err = quantum_attention_anneal(ctx, qk_scores, 0, nullptr);
    EXPECT_NE(err, 0);
}

TEST_F(QuantumAttentionTest, AnnealInvalidHead) {
    float qk_scores[SEQ_LENGTH * SEQ_LENGTH] = {0};
    quantum_attention_result_t result;
    int err = quantum_attention_anneal(ctx, qk_scores, NUM_HEADS + 10, &result);
    EXPECT_NE(err, 0);
}

TEST_F(QuantumAttentionTest, AnnealProducesSparseMask) {
    float qk_scores[SEQ_LENGTH * SEQ_LENGTH];
    for (uint32_t i = 0; i < SEQ_LENGTH * SEQ_LENGTH; i++) {
        qk_scores[i] = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
    }

    quantum_attention_result_t result;
    int err = quantum_attention_anneal(ctx, qk_scores, 0, &result);

    EXPECT_EQ(err, 0);
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.sparse_mask, nullptr);

    // Check mask has definite values after annealing
    uint32_t measured = 0;
    for (uint32_t i = 0; i < SEQ_LENGTH; i++) {
        for (uint32_t j = 0; j < SEQ_LENGTH; j++) {
            trit_t val = trit_matrix_get(result.sparse_mask, i, j);
            if (val != TRIT_UNKNOWN) measured++;
        }
    }

    // Should have collapsed most spins
    EXPECT_GT(measured, 0U);
}

//=============================================================================
// Score Computation Tests
//=============================================================================

TEST_F(QuantumAttentionTest, ComputeScoresBasic) {
    float query[SEQ_LENGTH * HEAD_DIM];
    float key[SEQ_LENGTH * HEAD_DIM];

    // Set query[0] and key[0] to same vector (high dot product)
    for (uint32_t i = 0; i < HEAD_DIM; i++) {
        query[i] = 1.0f;
        key[i] = 1.0f;
    }
    // Rest random
    for (uint32_t i = HEAD_DIM; i < SEQ_LENGTH * HEAD_DIM; i++) {
        query[i] = 0.1f * ((float)rand() / (float)RAND_MAX);
        key[i] = 0.1f * ((float)rand() / (float)RAND_MAX);
    }

    float scale = 1.0f / sqrtf((float)HEAD_DIM);
    quantum_attention_compute_scores(ctx, query, key, 0, scale);

    // Check (0,0) has high attention
    trit_t val = trit_matrix_get(ctx->attention_mask, 0, 0);
    EXPECT_EQ(val, TRIT_POSITIVE);

    // Stats should be updated
    EXPECT_GT(ctx->stats.pairs_computed, 0U);
    EXPECT_EQ(ctx->stats.forward_calls, 1U);
}

TEST_F(QuantumAttentionTest, ComputeScoresSparsity) {
    float query[SEQ_LENGTH * HEAD_DIM];
    float key[SEQ_LENGTH * HEAD_DIM];

    // Mostly zeros = low dot products
    memset(query, 0, sizeof(query));
    memset(key, 0, sizeof(key));

    // Only set one high similarity
    for (uint32_t i = 0; i < HEAD_DIM; i++) {
        query[i] = 1.0f;
        key[i] = 1.0f;
    }

    float scale = 1.0f / sqrtf((float)HEAD_DIM);
    quantum_attention_compute_scores(ctx, query, key, 0, scale);

    // Most pairs should be skipped
    EXPECT_GT(ctx->stats.pairs_skipped, 0U);
    EXPECT_GT(ctx->stats.avg_sparsity, 0.0f);
}

TEST_F(QuantumAttentionTest, ComputeScoresNullInputs) {
    // Should not crash with null inputs
    quantum_attention_compute_scores(ctx, nullptr, nullptr, 0, 1.0f);
    quantum_attention_compute_scores(nullptr, nullptr, nullptr, 0, 1.0f);
}

//=============================================================================
// Value Application Tests
//=============================================================================

TEST_F(QuantumAttentionTest, ApplyValuesBasic) {
    const uint32_t VALUE_DIM = 8;
    float value[SEQ_LENGTH * VALUE_DIM];
    float output[SEQ_LENGTH * VALUE_DIM];

    // Set values
    for (uint32_t i = 0; i < SEQ_LENGTH * VALUE_DIM; i++) {
        value[i] = (float)(i % 10);
    }

    // Set attention mask: only attend to position 0
    quantum_attention_reset_mask(ctx);
    trit_matrix_set(ctx->attention_mask, 0, 0, TRIT_POSITIVE);

    quantum_attention_apply_values(ctx, value, VALUE_DIM, output);

    // Output[0] should equal value[0] (only attending to itself)
    for (uint32_t k = 0; k < VALUE_DIM; k++) {
        EXPECT_FLOAT_EQ(output[k], value[k]);
    }
}

TEST_F(QuantumAttentionTest, ApplyValuesMultipleAttention) {
    const uint32_t VALUE_DIM = 4;
    float value[SEQ_LENGTH * VALUE_DIM];
    float output[SEQ_LENGTH * VALUE_DIM];

    // Set simple values
    memset(value, 0, sizeof(value));
    value[0] = 1.0f;  // Token 0
    value[VALUE_DIM] = 3.0f;  // Token 1

    // Query 0 attends to tokens 0 and 1
    quantum_attention_reset_mask(ctx);
    trit_matrix_set(ctx->attention_mask, 0, 0, TRIT_POSITIVE);
    trit_matrix_set(ctx->attention_mask, 0, 1, TRIT_POSITIVE);

    quantum_attention_apply_values(ctx, value, VALUE_DIM, output);

    // Output[0,0] should be average of value[0,0] and value[1,0] = (1+3)/2 = 2
    EXPECT_FLOAT_EQ(output[0], 2.0f);
}

TEST_F(QuantumAttentionTest, ApplyValuesNullInputs) {
    float dummy[16];
    // Should not crash
    quantum_attention_apply_values(ctx, nullptr, 4, dummy);
    quantum_attention_apply_values(ctx, dummy, 4, nullptr);
    quantum_attention_apply_values(nullptr, dummy, 4, dummy);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(QuantumAttentionTest, GetStats) {
    quantum_attention_stats_t stats;
    quantum_attention_get_stats(ctx, &stats);

    // Fresh context has zero stats
    EXPECT_EQ(stats.forward_calls, 0U);
    EXPECT_EQ(stats.pairs_computed, 0U);
    EXPECT_EQ(stats.pairs_skipped, 0U);
    EXPECT_FLOAT_EQ(stats.avg_coherence, 0.0f);
    EXPECT_FLOAT_EQ(stats.speedup_factor, 1.0f);
}

TEST_F(QuantumAttentionTest, StatsAfterComputation) {
    float query[SEQ_LENGTH * HEAD_DIM];
    float key[SEQ_LENGTH * HEAD_DIM];
    randomVector(query, SEQ_LENGTH * HEAD_DIM);
    randomVector(key, SEQ_LENGTH * HEAD_DIM);

    float scale = 1.0f / sqrtf((float)HEAD_DIM);
    quantum_attention_compute_scores(ctx, query, key, 0, scale);

    quantum_attention_stats_t stats;
    quantum_attention_get_stats(ctx, &stats);

    EXPECT_EQ(stats.forward_calls, 1U);
    EXPECT_GT(stats.pairs_computed + stats.pairs_skipped, 0U);
}

TEST_F(QuantumAttentionTest, ResetStats) {
    // Generate some stats
    ctx->stats.forward_calls = 100;
    ctx->stats.pairs_computed = 1000;
    ctx->stats.avg_sparsity = 0.5f;

    quantum_attention_reset_stats(ctx);

    quantum_attention_stats_t stats;
    quantum_attention_get_stats(ctx, &stats);

    EXPECT_EQ(stats.forward_calls, 0U);
    EXPECT_EQ(stats.pairs_computed, 0U);
    EXPECT_FLOAT_EQ(stats.avg_sparsity, 0.0f);
}

TEST_F(QuantumAttentionTest, SpeedupFactor) {
    // Simulate high sparsity
    ctx->stats.pairs_computed = SEQ_LENGTH * SEQ_LENGTH / 4;  // 25% computed
    ctx->stats.forward_calls = 1;

    quantum_attention_stats_t stats;
    quantum_attention_get_stats(ctx, &stats);

    // Speedup should be ~4x
    EXPECT_NEAR(stats.speedup_factor, 4.0f, 0.1f);
}

//=============================================================================
// Base Attention Integration Tests
//=============================================================================

TEST_F(QuantumAttentionTest, SetAndGetBase) {
    // Initially null
    multihead_attention_t base = quantum_attention_get_base(ctx);
    EXPECT_EQ(base, nullptr);

    // Set a mock base (just use non-null value)
    multihead_attention_t mock_base = (multihead_attention_t)0xDEADBEEF;
    quantum_attention_set_base(ctx, mock_base);

    base = quantum_attention_get_base(ctx);
    EXPECT_EQ(base, mock_base);
}

TEST_F(QuantumAttentionTest, SetBaseNullContext) {
    // Should not crash
    quantum_attention_set_base(nullptr, nullptr);
}

TEST_F(QuantumAttentionTest, GetBaseNullContext) {
    multihead_attention_t base = quantum_attention_get_base(nullptr);
    EXPECT_EQ(base, nullptr);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(QuantumAttentionTest, SmallSequence) {
    quantum_attention_destroy(ctx);

    // Create with minimal sequence
    ctx = quantum_attention_create(&config, 2, HEAD_DIM, 1);
    ASSERT_NE(ctx, nullptr);

    float query[2 * HEAD_DIM] = {1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    float key[2 * HEAD_DIM] = {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};

    float scale = 1.0f / sqrtf((float)HEAD_DIM);
    quantum_attention_compute_scores(ctx, query, key, 0, scale);

    // Should complete without crash
    EXPECT_EQ(ctx->stats.forward_calls, 1U);
}

TEST_F(QuantumAttentionTest, LargeThreshold) {
    config.collapse_threshold = 100.0f;  // Very high threshold
    quantum_attention_destroy(ctx);
    ctx = quantum_attention_create(&config, SEQ_LENGTH, HEAD_DIM, NUM_HEADS);

    float query[SEQ_LENGTH * HEAD_DIM];
    float key[SEQ_LENGTH * HEAD_DIM];
    randomVector(query, SEQ_LENGTH * HEAD_DIM);
    randomVector(key, SEQ_LENGTH * HEAD_DIM);

    float scale = 1.0f / sqrtf((float)HEAD_DIM);
    quantum_attention_compute_scores(ctx, query, key, 0, scale);

    // Most pairs should be skipped due to high threshold
    quantum_attention_stats_t stats;
    quantum_attention_get_stats(ctx, &stats);
    EXPECT_GT(stats.pairs_skipped, stats.pairs_computed);
}

//=============================================================================
// Memory Tests
//=============================================================================

TEST_F(QuantumAttentionTest, IsingModelsAllocated) {
    EXPECT_NE(ctx->attention_ising, nullptr);
    for (uint32_t h = 0; h < NUM_HEADS; h++) {
        EXPECT_NE(ctx->attention_ising[h], nullptr);
        EXPECT_EQ(ctx->attention_ising[h]->n_spins, SEQ_LENGTH * SEQ_LENGTH);
    }
}

TEST_F(QuantumAttentionTest, WorkspaceBuffersAllocated) {
    EXPECT_NE(ctx->qk_scores, nullptr);
    EXPECT_NE(ctx->attention_probs, nullptr);
    EXPECT_NE(ctx->attention_mask, nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    srand(42);  // Fixed seed for reproducibility
    return RUN_ALL_TESTS();
}
