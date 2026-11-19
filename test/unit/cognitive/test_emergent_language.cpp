/**
 * @file test_emergent_language.cpp
 * @brief Unit tests for emergent language — vocabulary discovered from neural patterns.
 *
 * WHAT: Tests emergent language lifecycle, config defaults, observation,
 *       expression, comprehension, translation, vocab management, stats,
 *       persistence, and NULL safety.
 * WHY:  The emergent language system is how the brain develops its own symbols;
 *       correctness is critical for the expression/comprehension pipeline.
 * HOW:  Google Test, no real brain needed (standalone emergent language module).
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <cstdio>

extern "C" {
#include "cognitive/language/nimcp_emergent_language.h"
}

// Helper: generate a random-ish embedding
static void fill_random_embedding(float* data, uint32_t dim, float base) {
    for (uint32_t i = 0; i < dim; i++) {
        data[i] = base + 0.1f * sinf((float)i * 0.37f + base);
    }
}

// Helper: generate a similar embedding (close to another)
static void fill_similar_embedding(float* data, const float* ref, uint32_t dim, float noise) {
    for (uint32_t i = 0; i < dim; i++) {
        data[i] = ref[i] + noise * sinf((float)i * 1.7f);
    }
}

// ============================================================================
// Config Defaults
// ============================================================================

TEST(EmergentLanguage, ConfigDefaults) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    EXPECT_EQ(cfg.max_tokens, 1024u);
    EXPECT_EQ(cfg.centroid_dim, 4096u);
    EXPECT_EQ(cfg.embed_dim, 256u);
    EXPECT_NEAR(cfg.discovery_threshold, 0.3f, 0.05f);
    EXPECT_NEAR(cfg.merge_threshold, 0.1f, 0.05f);
    EXPECT_EQ(cfg.min_observations, 5u);
    EXPECT_NEAR(cfg.temperature, 0.5f, 0.1f);
    EXPECT_NEAR(cfg.decay_rate, 0.999f, 0.01f);
    EXPECT_TRUE(cfg.use_unicode_symbols);
}

// ============================================================================
// Lifecycle
// ============================================================================

TEST(EmergentLanguage, CreateDestroyDefault) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    // Use smaller dimensions for testing
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);
    nimcp_emergent_language_destroy(el);
}

TEST(EmergentLanguage, CreateWithNullConfig) {
    nimcp_emergent_language_t* el = nimcp_emergent_language_create(NULL);
    // Should create with defaults or return NULL — either is acceptable
    if (el) {
        nimcp_emergent_language_destroy(el);
    }
    SUCCEED();
}

TEST(EmergentLanguage, DestroyNull) {
    nimcp_emergent_language_destroy(NULL);
    SUCCEED() << "Destroy NULL did not crash";
}

// ============================================================================
// Initial State
// ============================================================================

TEST(EmergentLanguage, InitialVocabSizeZero) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    uint32_t vocab = nimcp_emergent_get_vocab_size(el);
    EXPECT_EQ(vocab, 0u) << "Initial vocab should be 0 (emergent, not seeded)";

    nimcp_emergent_language_destroy(el);
}

// ============================================================================
// Observation — Token Discovery
// ============================================================================

TEST(EmergentLanguage, ObserveCreatesFirstToken) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;
    cfg.discovery_threshold = 0.3f;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    std::vector<float> embedding(64);
    fill_random_embedding(embedding.data(), 64, 1.0f);

    int rc = nimcp_emergent_observe(el, embedding.data(), 64);
    EXPECT_EQ(rc, 0);

    uint32_t vocab = nimcp_emergent_get_vocab_size(el);
    EXPECT_GE(vocab, 1u) << "First observation should create at least one token";

    nimcp_emergent_language_destroy(el);
}

TEST(EmergentLanguage, ObserveSimilarReinforces) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;
    cfg.discovery_threshold = 0.3f;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    std::vector<float> base_emb(64);
    fill_random_embedding(base_emb.data(), 64, 1.0f);
    nimcp_emergent_observe(el, base_emb.data(), 64);
    uint32_t vocab_after_first = nimcp_emergent_get_vocab_size(el);

    // Observe a very similar embedding (small noise)
    std::vector<float> similar(64);
    fill_similar_embedding(similar.data(), base_emb.data(), 64, 0.01f);
    nimcp_emergent_observe(el, similar.data(), 64);

    uint32_t vocab_after_similar = nimcp_emergent_get_vocab_size(el);
    EXPECT_EQ(vocab_after_similar, vocab_after_first)
        << "Similar observation should reinforce, not create new token";

    nimcp_emergent_language_destroy(el);
}

TEST(EmergentLanguage, ObserveDifferentCreatesNew) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;
    cfg.discovery_threshold = 0.3f;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    // First observation: positive values in first half, zeros in second
    std::vector<float> emb1(64, 0.0f);
    for (uint32_t i = 0; i < 32; i++) emb1[i] = 1.0f;

    nimcp_emergent_observe(el, emb1.data(), 64);

    // Second observation: zeros in first half, positive in second half (orthogonal)
    std::vector<float> emb2(64, 0.0f);
    for (uint32_t i = 32; i < 64; i++) emb2[i] = 1.0f;

    nimcp_emergent_observe(el, emb2.data(), 64);

    uint32_t vocab = nimcp_emergent_get_vocab_size(el);
    EXPECT_GE(vocab, 2u) << "Orthogonal observations should create separate tokens";

    nimcp_emergent_language_destroy(el);
}

TEST(EmergentLanguage, VocabGrowsWithObservations) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 100;
    cfg.discovery_threshold = 0.3f;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    // Use one-hot-like embeddings to ensure maximum distance
    for (int i = 0; i < 10; i++) {
        std::vector<float> emb(64, 0.0f);
        // Set a unique subset of dimensions for each observation
        for (int j = 0; j < 6; j++) {
            emb[(i * 6 + j) % 64] = 1.0f;
        }
        nimcp_emergent_observe(el, emb.data(), 64);
    }

    uint32_t vocab = nimcp_emergent_get_vocab_size(el);
    EXPECT_GT(vocab, 1u) << "Multiple distant observations should grow vocab";

    nimcp_emergent_language_destroy(el);
}

// ============================================================================
// Expression
// ============================================================================

TEST(EmergentLanguage, ExpressProducesTokenIDs) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    // Build some vocabulary
    std::vector<float> emb(64);
    fill_random_embedding(emb.data(), 64, 1.0f);
    nimcp_emergent_observe(el, emb.data(), 64);

    uint32_t tokens[16] = {0};
    int count = nimcp_emergent_express(el, emb.data(), 64, tokens, 16);
    EXPECT_GE(count, 0) << "Express should return >= 0 token count";

    nimcp_emergent_language_destroy(el);
}

TEST(EmergentLanguage, ExpressSymbolsProducesText) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;
    cfg.use_unicode_symbols = true;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    std::vector<float> emb(64);
    fill_random_embedding(emb.data(), 64, 1.0f);
    nimcp_emergent_observe(el, emb.data(), 64);

    char text[512];
    memset(text, 0, sizeof(text));
    int rc = nimcp_emergent_express_symbols(el, emb.data(), 64, text, sizeof(text));
    EXPECT_GE(rc, 0);
    // If tokens exist, text should be non-empty
    if (nimcp_emergent_get_vocab_size(el) > 0) {
        EXPECT_GT(strlen(text), 0u) << "Symbol text should be non-empty when vocab exists";
    }

    nimcp_emergent_language_destroy(el);
}

// ============================================================================
// Comprehension
// ============================================================================

TEST(EmergentLanguage, ComprehendReconstructs) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;
    cfg.min_observations = 1; /* Make tokens "real" after 1 observation */

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    std::vector<float> emb(64);
    for (uint32_t i = 0; i < 64; i++) emb[i] = 1.0f + 0.1f * (float)i;
    nimcp_emergent_observe(el, emb.data(), 64);

    if (nimcp_emergent_get_vocab_size(el) > 0) {
        // Express to get token IDs
        uint32_t tokens[16] = {0};
        int count = nimcp_emergent_express(el, emb.data(), 64, tokens, 16);

        if (count > 0) {
            // Comprehend: reconstruct from tokens
            // Returns number of matched tokens (>= 0), not 0/error
            std::vector<float> reconstructed(64, 0.0f);
            int rc = nimcp_emergent_comprehend(el, tokens, (uint32_t)count,
                reconstructed.data(), 64);
            EXPECT_GT(rc, 0) << "Comprehend should match at least 1 token";
        } else {
            SUCCEED() << "No tokens expressed (insufficient observations for min_observations threshold)";
        }
    } else {
        SUCCEED() << "No vocab created yet";
    }

    nimcp_emergent_language_destroy(el);
}

// ============================================================================
// Translation
// ============================================================================

TEST(EmergentLanguage, AssociateHumanText) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    std::vector<float> emb(64);
    fill_random_embedding(emb.data(), 64, 1.0f);
    nimcp_emergent_observe(el, emb.data(), 64);

    if (nimcp_emergent_get_vocab_size(el) > 0) {
        int rc = nimcp_emergent_associate_human(el, 0, "happiness", 0.8f);
        EXPECT_EQ(rc, 0);
    }

    nimcp_emergent_language_destroy(el);
}

TEST(EmergentLanguage, GetTranslation) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    std::vector<float> emb(64);
    fill_random_embedding(emb.data(), 64, 1.0f);
    nimcp_emergent_observe(el, emb.data(), 64);

    if (nimcp_emergent_get_vocab_size(el) > 0) {
        nimcp_emergent_associate_human(el, 0, "concept_alpha", 0.9f);

        nimcp_emergent_translation_t trans;
        memset(&trans, 0, sizeof(trans));
        int rc = nimcp_emergent_get_translation(el, 0, &trans);
        EXPECT_EQ(rc, 0);
        EXPECT_STREQ(trans.human_gloss, "concept_alpha");
        EXPECT_NEAR(trans.translation_confidence, 0.9f, 0.05f);
    }

    nimcp_emergent_language_destroy(el);
}

// ============================================================================
// Statistics
// ============================================================================

TEST(EmergentLanguage, GetStatsObservationsCounted) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    std::vector<float> emb(64);
    fill_random_embedding(emb.data(), 64, 1.0f);
    nimcp_emergent_observe(el, emb.data(), 64);
    nimcp_emergent_observe(el, emb.data(), 64);
    nimcp_emergent_observe(el, emb.data(), 64);

    nimcp_emergent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int rc = nimcp_emergent_get_stats(el, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(stats.total_observations, 3u);
    EXPECT_EQ(stats.vocab_size, nimcp_emergent_get_vocab_size(el));

    nimcp_emergent_language_destroy(el);
}

// ============================================================================
// Vocabulary Management
// ============================================================================

TEST(EmergentLanguage, PruneRemovesUnused) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    int rc = nimcp_emergent_prune(el);
    EXPECT_EQ(rc, 0) << "Prune on empty vocab should succeed";

    nimcp_emergent_language_destroy(el);
}

TEST(EmergentLanguage, MergeSimilar) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    int rc = nimcp_emergent_merge_similar(el);
    EXPECT_EQ(rc, 0) << "Merge on empty vocab should succeed";

    nimcp_emergent_language_destroy(el);
}

TEST(EmergentLanguage, GetTokenByID) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    std::vector<float> emb(64);
    fill_random_embedding(emb.data(), 64, 1.0f);
    nimcp_emergent_observe(el, emb.data(), 64);

    if (nimcp_emergent_get_vocab_size(el) > 0) {
        nimcp_emergent_token_t token;
        memset(&token, 0, sizeof(token));
        int rc = nimcp_emergent_get_token(el, 0, &token);
        EXPECT_EQ(rc, 0);
        EXPECT_GT(strlen(token.symbol), 0u) << "Token symbol should be non-empty";
    }

    nimcp_emergent_language_destroy(el);
}

// ============================================================================
// Persistence
// ============================================================================

TEST(EmergentLanguage, SaveLoadRoundtrip) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 32;

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    // Build vocabulary
    for (int i = 0; i < 5; i++) {
        std::vector<float> emb(64);
        fill_random_embedding(emb.data(), 64, (float)(i * 50));
        nimcp_emergent_observe(el, emb.data(), 64);
    }
    uint32_t vocab_before = nimcp_emergent_get_vocab_size(el);

    const char* path = "/tmp/nimcp_test_emergent_lang.bin";
    int rc = nimcp_emergent_save(el, path);
    EXPECT_EQ(rc, 0);

    // Create new instance and load
    nimcp_emergent_language_t* el2 = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el2, nullptr);

    rc = nimcp_emergent_load(el2, path);
    EXPECT_EQ(rc, 0);

    uint32_t vocab_after = nimcp_emergent_get_vocab_size(el2);
    EXPECT_EQ(vocab_after, vocab_before) << "Loaded vocab should match saved";

    nimcp_emergent_language_destroy(el2);
    nimcp_emergent_language_destroy(el);
    remove(path);
}

// ============================================================================
// Max Tokens Cap
// ============================================================================

TEST(EmergentLanguage, MaxTokensCapsVocab) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = 64;
    cfg.embed_dim = 32;
    cfg.max_tokens = 5;
    cfg.discovery_threshold = 0.01f; // Very low threshold = many new tokens

    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    ASSERT_NE(el, nullptr);

    // Feed many orthogonal observations to force new tokens
    for (int i = 0; i < 20; i++) {
        std::vector<float> emb(64, 0.0f);
        // One-hot style: each observation activates a unique set of dims
        for (int j = 0; j < 3; j++) {
            emb[(i * 3 + j) % 64] = 1.0f;
        }
        nimcp_emergent_observe(el, emb.data(), 64);
    }

    uint32_t vocab = nimcp_emergent_get_vocab_size(el);
    EXPECT_LE(vocab, 5u) << "Vocab should not exceed max_tokens";

    nimcp_emergent_language_destroy(el);
}

// ============================================================================
// NULL Safety
// ============================================================================

TEST(EmergentLanguage, GetVocabSizeNull) {
    uint32_t size = nimcp_emergent_get_vocab_size(NULL);
    EXPECT_EQ(size, 0u);
}

TEST(EmergentLanguage, ObserveNull) {
    float emb[64];
    int rc = nimcp_emergent_observe(NULL, emb, 64);
    EXPECT_LT(rc, 0);
}

TEST(EmergentLanguage, ExpressNull) {
    uint32_t tokens[16];
    int rc = nimcp_emergent_express(NULL, NULL, 0, tokens, 16);
    EXPECT_LE(rc, 0);
}

TEST(EmergentLanguage, ComprehendNull) {
    float emb[64];
    uint32_t tokens[] = {0};
    int rc = nimcp_emergent_comprehend(NULL, tokens, 1, emb, 64);
    EXPECT_LT(rc, 0);
}

TEST(EmergentLanguage, GetStatsNull) {
    nimcp_emergent_stats_t stats;
    int rc = nimcp_emergent_get_stats(NULL, &stats);
    EXPECT_LT(rc, 0);
}

TEST(EmergentLanguage, SaveNull) {
    int rc = nimcp_emergent_save(NULL, "/tmp/test.bin");
    EXPECT_LT(rc, 0);
}

TEST(EmergentLanguage, LoadNull) {
    int rc = nimcp_emergent_load(NULL, "/tmp/test.bin");
    EXPECT_LT(rc, 0);
}

TEST(EmergentLanguage, GetTokenNull) {
    nimcp_emergent_token_t token;
    int rc = nimcp_emergent_get_token(NULL, 0, &token);
    EXPECT_LT(rc, 0);
}

TEST(EmergentLanguage, PruneNull) {
    int rc = nimcp_emergent_prune(NULL);
    EXPECT_LT(rc, 0);
}

TEST(EmergentLanguage, MergeNull) {
    int rc = nimcp_emergent_merge_similar(NULL);
    EXPECT_LT(rc, 0);
}

TEST(EmergentLanguage, AssociateNull) {
    int rc = nimcp_emergent_associate_human(NULL, 0, "test", 1.0f);
    EXPECT_LT(rc, 0);
}

TEST(EmergentLanguage, GetTranslationNull) {
    nimcp_emergent_translation_t trans;
    int rc = nimcp_emergent_get_translation(NULL, 0, &trans);
    EXPECT_LT(rc, 0);
}
