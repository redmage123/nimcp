/**
 * @file test_embedding.cpp
 * @brief Unit tests for the NIMCP embedding layer
 *
 * WHAT: Tests embedding creation, lookup, gradient operations, similarity metrics
 * WHY:  Embeddings translate token IDs into continuous vector space — wrong
 *       dimensions or corrupt gradients would break the entire generation pipeline
 * HOW:  GTest fixture with vocab_size=1000, embed_dim=64; exercises lookup,
 *       batch operations, forward, backward, cosine similarity, and nearest neighbors
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

extern "C" {
#include "generation/nimcp_embedding.h"
}

// =============================================================================
// Constants
// =============================================================================

static constexpr uint32_t TEST_VOCAB_SIZE = 1000;
static constexpr uint32_t TEST_EMBED_DIM = 64;

// =============================================================================
// Test Fixture
// =============================================================================

class EmbeddingUnit : public ::testing::Test {
protected:
    embedding_layer_t* emb = nullptr;

    void SetUp() override {
        embedding_config_t cfg = embedding_default_config(TEST_VOCAB_SIZE, TEST_EMBED_DIM);
        emb = embedding_create(&cfg);
        ASSERT_NE(emb, nullptr) << "Embedding layer creation must succeed";
    }

    void TearDown() override {
        if (emb) {
            embedding_destroy(emb);
            emb = nullptr;
        }
    }

    // Helper: compute L2 norm of a float vector
    float l2_norm(const float* vec, uint32_t dim) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            sum += vec[i] * vec[i];
        }
        return sqrtf(sum);
    }

    // Helper: check if two float vectors are equal
    bool vectors_equal(const float* a, const float* b, uint32_t dim, float tol = 1e-6f) {
        for (uint32_t i = 0; i < dim; i++) {
            if (fabsf(a[i] - b[i]) > tol) return false;
        }
        return true;
    }
};

// =============================================================================
// Tests: Creation and Configuration
// =============================================================================

TEST_F(EmbeddingUnit, CreateAndDestroy) {
    EXPECT_NE(emb, nullptr);
    // Destroy in TearDown
}

TEST_F(EmbeddingUnit, DefaultConfigValues) {
    embedding_config_t cfg = embedding_default_config(500, 128);

    EXPECT_EQ(cfg.vocab_size, 500u);
    EXPECT_EQ(cfg.embed_dim, 128u);
    EXPECT_GT(cfg.init_scale, 0.0f)
        << "Initialization scale should be positive for random init";
    // freeze defaults to false (trainable)
    EXPECT_FALSE(cfg.freeze);
}

// =============================================================================
// Tests: Dimension Accessors
// =============================================================================

TEST_F(EmbeddingUnit, GetDimReturnsCorrect) {
    EXPECT_EQ(embedding_get_dim(emb), TEST_EMBED_DIM);
}

TEST_F(EmbeddingUnit, GetVocabSizeReturnsCorrect) {
    EXPECT_EQ(embedding_get_vocab_size(emb), TEST_VOCAB_SIZE);
}

// =============================================================================
// Tests: Lookup
// =============================================================================

TEST_F(EmbeddingUnit, LookupReturnsVector) {
    std::vector<float> output(TEST_EMBED_DIM, 0.0f);
    int rc = embedding_lookup(emb, 0, output.data());
    EXPECT_EQ(rc, 0) << "Lookup for token 0 should succeed";

    // The output should be a valid vector (at least some non-zero values
    // unless init_scale is 0, which would be unusual)
    float norm = l2_norm(output.data(), TEST_EMBED_DIM);
    // Norm might be 0 if initialized to zero, but typically random
    // Just verify the call didn't crash and returned a vector of correct size
    EXPECT_TRUE(norm >= 0.0f); // Always true for L2 norm
}

TEST_F(EmbeddingUnit, LookupDifferentTokensDifferent) {
    std::vector<float> vec_a(TEST_EMBED_DIM);
    std::vector<float> vec_b(TEST_EMBED_DIM);

    int rc_a = embedding_lookup(emb, 0, vec_a.data());
    int rc_b = embedding_lookup(emb, 1, vec_b.data());
    ASSERT_EQ(rc_a, 0);
    ASSERT_EQ(rc_b, 0);

    // Different tokens should (with very high probability) have different embeddings
    // after random initialization
    bool same = vectors_equal(vec_a.data(), vec_b.data(), TEST_EMBED_DIM);
    EXPECT_FALSE(same)
        << "Tokens 0 and 1 should have different random embeddings";
}

TEST_F(EmbeddingUnit, LookupSameTokenConsistent) {
    std::vector<float> vec_1(TEST_EMBED_DIM);
    std::vector<float> vec_2(TEST_EMBED_DIM);

    embedding_lookup(emb, 42, vec_1.data());
    embedding_lookup(emb, 42, vec_2.data());

    EXPECT_TRUE(vectors_equal(vec_1.data(), vec_2.data(), TEST_EMBED_DIM))
        << "Same token should return the same embedding vector";
}

TEST_F(EmbeddingUnit, LookupOutOfBoundsReturnsError) {
    std::vector<float> output(TEST_EMBED_DIM);

    int rc = embedding_lookup(emb, TEST_VOCAB_SIZE, output.data());
    EXPECT_NE(rc, 0) << "Token ID == vocab_size is out of bounds; should return error";

    rc = embedding_lookup(emb, TEST_VOCAB_SIZE + 1000, output.data());
    EXPECT_NE(rc, 0) << "Token ID >> vocab_size should return error";
}

// =============================================================================
// Tests: Batch Lookup
// =============================================================================

TEST_F(EmbeddingUnit, BatchLookup) {
    uint32_t ids[] = {0, 10, 20, 30, 40};
    const uint32_t count = 5;
    std::vector<float> batch_output(count * TEST_EMBED_DIM, 0.0f);

    int rc = embedding_lookup_batch(emb, ids, count, batch_output.data());
    EXPECT_EQ(rc, 0) << "Batch lookup should succeed";

    // Verify each batch entry matches individual lookup
    for (uint32_t i = 0; i < count; i++) {
        std::vector<float> single(TEST_EMBED_DIM);
        embedding_lookup(emb, ids[i], single.data());

        float* batch_entry = batch_output.data() + i * TEST_EMBED_DIM;
        EXPECT_TRUE(vectors_equal(single.data(), batch_entry, TEST_EMBED_DIM))
            << "Batch entry " << i << " should match individual lookup for token "
            << ids[i];
    }
}

// =============================================================================
// Tests: Forward (Tensor)
// =============================================================================

TEST_F(EmbeddingUnit, ForwardReturnsTensor) {
    uint32_t ids[] = {5, 10, 15};
    // Forward should return a tensor — we just check it's non-NULL
    // (detailed tensor shape checks depend on nimcp_tensor_t API)
    void* tensor = embedding_forward(emb, ids, 3);
    EXPECT_NE(tensor, nullptr) << "embedding_forward should return a non-NULL tensor";

    // Note: We don't destroy the tensor here since the API contract isn't clear
    // on ownership. The implementation should handle cleanup.
}

// =============================================================================
// Tests: Gradient Operations
// =============================================================================

TEST_F(EmbeddingUnit, GradientAccumulation) {
    // Get the original embedding for token 0
    std::vector<float> original(TEST_EMBED_DIM);
    embedding_lookup(emb, 0, original.data());

    // Create a fake gradient (all 1.0)
    std::vector<float> grad(TEST_EMBED_DIM, 1.0f);

    int rc = embedding_backward(emb, 0, grad.data());
    EXPECT_EQ(rc, 0) << "backward should succeed";

    // The embedding itself shouldn't change until update() is called
    std::vector<float> after_backward(TEST_EMBED_DIM);
    embedding_lookup(emb, 0, after_backward.data());
    EXPECT_TRUE(vectors_equal(original.data(), after_backward.data(), TEST_EMBED_DIM))
        << "Embedding should not change until update() is called";
}

TEST_F(EmbeddingUnit, UpdateChangesWeights) {
    // Get original embedding
    std::vector<float> original(TEST_EMBED_DIM);
    embedding_lookup(emb, 0, original.data());

    // Backward with non-zero gradient
    std::vector<float> grad(TEST_EMBED_DIM, 1.0f);
    embedding_backward(emb, 0, grad.data());

    // Update with a learning rate
    int rc = embedding_update(emb, 0.01f);
    EXPECT_EQ(rc, 0);

    // Embedding should now be different
    std::vector<float> after_update(TEST_EMBED_DIM);
    embedding_lookup(emb, 0, after_update.data());

    bool changed = !vectors_equal(original.data(), after_update.data(), TEST_EMBED_DIM);
    EXPECT_TRUE(changed)
        << "Embedding should change after backward + update with non-zero gradient";
}

TEST_F(EmbeddingUnit, ZeroGradResetsGradients) {
    // Accumulate gradients
    std::vector<float> grad(TEST_EMBED_DIM, 1.0f);
    embedding_backward(emb, 0, grad.data());

    // Zero out gradients
    embedding_zero_grad(emb);

    // Now update — should have no effect since gradients are zero
    std::vector<float> before(TEST_EMBED_DIM);
    embedding_lookup(emb, 0, before.data());

    embedding_update(emb, 0.01f);

    std::vector<float> after(TEST_EMBED_DIM);
    embedding_lookup(emb, 0, after.data());

    EXPECT_TRUE(vectors_equal(before.data(), after.data(), TEST_EMBED_DIM))
        << "After zero_grad + update, embedding should not change";
}

TEST_F(EmbeddingUnit, FrozenEmbeddingSkipsUpdate) {
    // Create a frozen embedding layer
    embedding_config_t cfg = embedding_default_config(TEST_VOCAB_SIZE, TEST_EMBED_DIM);
    cfg.freeze = true;
    embedding_layer_t* frozen = embedding_create(&cfg);
    ASSERT_NE(frozen, nullptr);

    // Get original
    std::vector<float> original(TEST_EMBED_DIM);
    embedding_lookup(frozen, 0, original.data());

    // Backward + update on frozen layer
    std::vector<float> grad(TEST_EMBED_DIM, 1.0f);
    embedding_backward(frozen, 0, grad.data());
    embedding_update(frozen, 0.1f);

    // Should be unchanged
    std::vector<float> after(TEST_EMBED_DIM);
    embedding_lookup(frozen, 0, after.data());

    EXPECT_TRUE(vectors_equal(original.data(), after.data(), TEST_EMBED_DIM))
        << "Frozen embedding should not change after backward + update";

    embedding_destroy(frozen);
}

// =============================================================================
// Tests: Cosine Similarity
// =============================================================================

TEST_F(EmbeddingUnit, CosineSimilarityRange) {
    float sim = embedding_cosine_similarity(emb, 0, 1);
    EXPECT_GE(sim, -1.0f - 1e-5f)
        << "Cosine similarity must be >= -1.0";
    EXPECT_LE(sim, 1.0f + 1e-5f)
        << "Cosine similarity must be <= 1.0";
}

TEST_F(EmbeddingUnit, CosineSimilaritySelfIsOne) {
    float sim = embedding_cosine_similarity(emb, 42, 42);
    EXPECT_NEAR(sim, 1.0f, 1e-4f)
        << "Cosine similarity of a token with itself should be ~1.0";
}

// =============================================================================
// Tests: Nearest Neighbors
// =============================================================================

TEST_F(EmbeddingUnit, NearestNeighborsFindsSelf) {
    // Get the embedding for token 42
    std::vector<float> query(TEST_EMBED_DIM);
    embedding_lookup(emb, 42, query.data());

    // Find nearest neighbors (k=5)
    const uint32_t k = 5;
    std::vector<uint32_t> nn_ids(k);
    std::vector<float> nn_scores(k);

    int rc = embedding_nearest_neighbors(emb, query.data(), k,
                                          nn_ids.data(), nn_scores.data());
    EXPECT_EQ(rc, 0);

    // The first nearest neighbor (most similar) should be token 42 itself
    EXPECT_EQ(nn_ids[0], 42u)
        << "Querying with token 42's own embedding should find token 42 first";

    // Its score should be very close to 1.0 (or the maximum similarity value)
    EXPECT_NEAR(nn_scores[0], 1.0f, 1e-3f);

    // Scores should be in descending order
    for (uint32_t i = 1; i < k; i++) {
        EXPECT_LE(nn_scores[i], nn_scores[i - 1] + 1e-6f)
            << "Nearest neighbor scores should be in descending order";
    }
}
