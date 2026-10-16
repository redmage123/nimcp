//=============================================================================
// test_synapse_embeddings.cpp - Unit Tests for Synapse Semantic Embeddings
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

#include "core/neuralnet/nimcp_neuralnet.h"

class SynapseEmbeddingsTest : public ::testing::Test {
protected:
    synapse_t synapse;

    void SetUp() override {
        memset(&synapse, 0, sizeof(synapse_t));
    }

    void TearDown() override {
        if (synapse.semantic_embedding) {
            synapse_destroy_embedding(&synapse);
        }
    }
};

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(SynapseEmbeddingsTest, InitEmbeddingSuccess) {
    bool result = synapse_init_embedding(&synapse, 128);

    EXPECT_TRUE(result);
    EXPECT_NE(synapse.semantic_embedding, nullptr);
    EXPECT_EQ(synapse.embedding_dim, 128);
    EXPECT_FLOAT_EQ(synapse.semantic_relevance, 0.5f);
}

TEST_F(SynapseEmbeddingsTest, InitEmbeddingNormalized) {
    synapse_init_embedding(&synapse, 128);

    // Check that embedding is normalized to unit length
    float norm = 0.0f;
    for (uint16_t i = 0; i < 128; i++) {
        norm += synapse.semantic_embedding[i] * synapse.semantic_embedding[i];
    }
    norm = sqrtf(norm);

    EXPECT_NEAR(norm, 1.0f, 0.01f);
}

TEST_F(SynapseEmbeddingsTest, InitEmbeddingNullSynapse) {
    bool result = synapse_init_embedding(nullptr, 128);
    EXPECT_FALSE(result);
}

TEST_F(SynapseEmbeddingsTest, InitEmbeddingZeroDim) {
    bool result = synapse_init_embedding(&synapse, 0);
    EXPECT_FALSE(result);
}

TEST_F(SynapseEmbeddingsTest, InitEmbeddingReplaceExisting) {
    synapse_init_embedding(&synapse, 64);
    float* old_ptr = synapse.semantic_embedding;

    synapse_init_embedding(&synapse, 128);

    EXPECT_NE(synapse.semantic_embedding, old_ptr);
    EXPECT_EQ(synapse.embedding_dim, 128);
}

//=============================================================================
// Similarity Tests
//=============================================================================

TEST_F(SynapseEmbeddingsTest, SimilarityIdentical) {
    synapse_init_embedding(&synapse, 128);

    float similarity = synapse_semantic_similarity(&synapse, &synapse);

    EXPECT_NEAR(similarity, 1.0f, 0.01f);  // Identical should be ~1.0
}

TEST_F(SynapseEmbeddingsTest, SimilarityOrthogonal) {
    synapse_t syn1, syn2;
    memset(&syn1, 0, sizeof(synapse_t));
    memset(&syn2, 0, sizeof(synapse_t));

    synapse_init_embedding(&syn1, 128);
    synapse_init_embedding(&syn2, 128);

    // Manually set orthogonal embeddings
    for (uint16_t i = 0; i < 64; i++) {
        syn1.semantic_embedding[i] = 1.0f / sqrtf(64.0f);
        syn2.semantic_embedding[i + 64] = 1.0f / sqrtf(64.0f);
    }
    for (uint16_t i = 64; i < 128; i++) {
        syn1.semantic_embedding[i] = 0.0f;
    }
    for (uint16_t i = 0; i < 64; i++) {
        syn2.semantic_embedding[i] = 0.0f;
    }

    float similarity = synapse_semantic_similarity(&syn1, &syn2);

    EXPECT_NEAR(similarity, 0.0f, 0.1f);  // Orthogonal should be ~0.0

    synapse_destroy_embedding(&syn1);
    synapse_destroy_embedding(&syn2);
}

TEST_F(SynapseEmbeddingsTest, SimilarityNullInputs) {
    synapse_init_embedding(&synapse, 128);

    EXPECT_FLOAT_EQ(synapse_semantic_similarity(nullptr, &synapse), 0.0f);
    EXPECT_FLOAT_EQ(synapse_semantic_similarity(&synapse, nullptr), 0.0f);
    EXPECT_FLOAT_EQ(synapse_semantic_similarity(nullptr, nullptr), 0.0f);
}

TEST_F(SynapseEmbeddingsTest, SimilarityNoEmbeddings) {
    synapse_t syn1, syn2;
    memset(&syn1, 0, sizeof(synapse_t));
    memset(&syn2, 0, sizeof(synapse_t));

    float similarity = synapse_semantic_similarity(&syn1, &syn2);

    EXPECT_FLOAT_EQ(similarity, 0.0f);
}

TEST_F(SynapseEmbeddingsTest, SimilarityDimensionMismatch) {
    synapse_t syn1, syn2;
    memset(&syn1, 0, sizeof(synapse_t));
    memset(&syn2, 0, sizeof(synapse_t));

    synapse_init_embedding(&syn1, 64);
    synapse_init_embedding(&syn2, 128);

    float similarity = synapse_semantic_similarity(&syn1, &syn2);

    EXPECT_FLOAT_EQ(similarity, 0.0f);

    synapse_destroy_embedding(&syn1);
    synapse_destroy_embedding(&syn2);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(SynapseEmbeddingsTest, UpdateEmbeddingConverges) {
    synapse_init_embedding(&synapse, 128);

    // Target embedding: all 1/sqrt(128)
    float target[128];
    float norm_val = 1.0f / sqrtf(128.0f);
    for (uint16_t i = 0; i < 128; i++) {
        target[i] = norm_val;
    }

    // Save initial embedding
    float initial[128];
    memcpy(initial, synapse.semantic_embedding, 128 * sizeof(float));

    // Multiple updates should move toward target
    for (int iter = 0; iter < 10; iter++) {
        synapse_update_embedding(&synapse, target, 0.1f);
    }

    // Check that embedding moved toward target
    float initial_dist = 0.0f, final_dist = 0.0f;
    for (uint16_t i = 0; i < 128; i++) {
        initial_dist += (initial[i] - target[i]) * (initial[i] - target[i]);
        final_dist += (synapse.semantic_embedding[i] - target[i]) *
                      (synapse.semantic_embedding[i] - target[i]);
    }

    EXPECT_LT(final_dist, initial_dist);  // Should move closer
}

TEST_F(SynapseEmbeddingsTest, UpdateEmbeddingMaintainsNormalization) {
    synapse_init_embedding(&synapse, 128);

    float target[128];
    for (uint16_t i = 0; i < 128; i++) {
        target[i] = (i % 2 == 0) ? 1.0f : -1.0f;  // Non-normalized target
    }

    synapse_update_embedding(&synapse, target, 0.5f);

    // Check normalization maintained
    float norm = 0.0f;
    for (uint16_t i = 0; i < 128; i++) {
        norm += synapse.semantic_embedding[i] * synapse.semantic_embedding[i];
    }
    norm = sqrtf(norm);

    EXPECT_NEAR(norm, 1.0f, 0.01f);
}

TEST_F(SynapseEmbeddingsTest, UpdateEmbeddingNullInputs) {
    synapse_init_embedding(&synapse, 128);
    float target[128] = {0};

    EXPECT_FALSE(synapse_update_embedding(nullptr, target, 0.1f));
    EXPECT_FALSE(synapse_update_embedding(&synapse, nullptr, 0.1f));
}

//=============================================================================
// Relevance Tests
//=============================================================================

TEST_F(SynapseEmbeddingsTest, ComputeRelevanceMatchingContext) {
    synapse_init_embedding(&synapse, 128);

    // Context = synapse embedding (perfect match)
    float relevance = synapse_compute_relevance(&synapse,
                                                synapse.semantic_embedding,
                                                128);

    EXPECT_NEAR(relevance, 1.0f, 0.01f);  // Perfect match → relevance = 1
    EXPECT_NEAR(synapse.semantic_relevance, 1.0f, 0.01f);  // Cached
}

TEST_F(SynapseEmbeddingsTest, ComputeRelevanceOrthogonalContext) {
    synapse_init_embedding(&synapse, 128);

    // Orthogonal context
    float context[128];
    for (uint16_t i = 0; i < 128; i++) {
        context[i] = (synapse.semantic_embedding[i] > 0) ? -1.0f : 1.0f;
    }

    float relevance = synapse_compute_relevance(&synapse, context, 128);

    EXPECT_LT(relevance, 0.6f);  // Orthogonal → low relevance
}

TEST_F(SynapseEmbeddingsTest, ComputeRelevanceNullInputs) {
    synapse_init_embedding(&synapse, 128);
    float context[128] = {0};

    EXPECT_FLOAT_EQ(synapse_compute_relevance(nullptr, context, 128), 0.0f);
    EXPECT_FLOAT_EQ(synapse_compute_relevance(&synapse, nullptr, 128), 0.0f);
}

TEST_F(SynapseEmbeddingsTest, ComputeRelevanceDimensionMismatch) {
    synapse_init_embedding(&synapse, 128);
    float context[64] = {0};

    float relevance = synapse_compute_relevance(&synapse, context, 64);

    EXPECT_FLOAT_EQ(relevance, 0.0f);
}

//=============================================================================
// Cleanup Tests
//=============================================================================

TEST_F(SynapseEmbeddingsTest, DestroyEmbeddingSuccess) {
    synapse_init_embedding(&synapse, 128);

    synapse_destroy_embedding(&synapse);

    EXPECT_EQ(synapse.semantic_embedding, nullptr);
    EXPECT_EQ(synapse.embedding_dim, 0);
    EXPECT_FLOAT_EQ(synapse.semantic_relevance, 0.0f);
}

TEST_F(SynapseEmbeddingsTest, DestroyEmbeddingNullSynapse) {
    // Should not crash
    synapse_destroy_embedding(nullptr);
    SUCCEED();
}

TEST_F(SynapseEmbeddingsTest, DestroyEmbeddingNoEmbedding) {
    // Should not crash
    synapse_destroy_embedding(&synapse);
    SUCCEED();
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(SynapseEmbeddingsTest, FullLifecycle) {
    // Init
    ASSERT_TRUE(synapse_init_embedding(&synapse, 128));

    // Compute relevance for some context
    float context[128];
    for (uint16_t i = 0; i < 128; i++) {
        context[i] = sinf(i * 0.1f);
    }
    float relevance = synapse_compute_relevance(&synapse, context, 128);
    EXPECT_GE(relevance, 0.0f);
    EXPECT_LE(relevance, 1.0f);

    // Update toward context
    synapse_update_embedding(&synapse, context, 0.3f);

    // Relevance should increase
    float new_relevance = synapse_compute_relevance(&synapse, context, 128);
    EXPECT_GT(new_relevance, relevance);

    // Cleanup
    synapse_destroy_embedding(&synapse);
    EXPECT_EQ(synapse.semantic_embedding, nullptr);
}
