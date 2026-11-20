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
#include <unistd.h>

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

// ============================================================================
// Wiring additions (2026-11-19): observe_ex, vote_human, vote stats, save
// format for multi-word glosses + votes, votes surviving merge.
// ============================================================================

static nimcp_emergent_language_t* small_el(uint32_t dim) {
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = dim;
    cfg.embed_dim = dim;
    cfg.max_tokens = 64;
    return nimcp_emergent_language_create(&cfg);
}

TEST(EmergentLanguageWiring, ObserveExReportsTokenId) {
    const uint32_t dim = 32;
    nimcp_emergent_language_t* el = small_el(dim);
    ASSERT_NE(el, nullptr);
    std::vector<float> a(dim), a2(dim), b(dim);
    fill_random_embedding(a.data(), dim, 1.0f);
    fill_similar_embedding(a2.data(), a.data(), dim, 0.01f);
    for (uint32_t i = 0; i < dim; i++) b[i] = (i % 2) ? 1.0f : -1.0f;

    uint32_t t1 = 999, t2 = 999, t3 = 999;
    EXPECT_EQ(nimcp_emergent_observe_ex(el, a.data(), dim, &t1), 0);
    EXPECT_EQ(nimcp_emergent_observe_ex(el, a2.data(), dim, &t2), 0);
    EXPECT_EQ(nimcp_emergent_observe_ex(el, b.data(), dim, &t3), 0);
    EXPECT_NE(t1, UINT32_MAX);
    EXPECT_EQ(t1, t2);          // near-identical output reinforces the same token
    EXPECT_NE(t1, t3);          // a dissimilar output discovers a new one

    std::vector<float> zero(dim, 0.0f);
    uint32_t tz = 0;
    EXPECT_EQ(nimcp_emergent_observe_ex(el, zero.data(), dim, &tz), 0);
    EXPECT_EQ(tz, UINT32_MAX);  // near-zero output is skipped
    EXPECT_EQ(nimcp_emergent_observe_ex(el, a.data(), dim, NULL), 0);  // NULL out ok
    nimcp_emergent_language_destroy(el);
}

TEST(EmergentLanguageWiring, VoteHumanScoresBeforeCountingAndTracksMajority) {
    const uint32_t dim = 16;
    nimcp_emergent_language_t* el = small_el(dim);
    std::vector<float> a(dim);
    fill_random_embedding(a.data(), dim, 0.5f);
    uint32_t t = UINT32_MAX;
    nimcp_emergent_observe_ex(el, a.data(), dim, &t);
    ASSERT_NE(t, UINT32_MAX);

    EXPECT_EQ(nimcp_emergent_vote_human(el, t, "big dog"), 0);  // no gloss yet
    EXPECT_EQ(nimcp_emergent_vote_human(el, t, "big dog"), 1);  // gloss now "big dog"
    EXPECT_EQ(nimcp_emergent_vote_human(el, t, "cat"), 0);
    nimcp_emergent_translation_t tr;
    ASSERT_EQ(nimcp_emergent_get_translation(el, t, &tr), 0);
    EXPECT_STREQ(tr.human_gloss, "big dog");
    EXPECT_NEAR(tr.translation_confidence, 2.0f / 3.0f, 1e-5f);

    uint32_t labeled = 0; float purity = 0.0f;
    ASSERT_EQ(nimcp_emergent_get_vote_stats(el, &labeled, &purity), 0);
    EXPECT_EQ(labeled, 1u);
    EXPECT_NEAR(purity, 2.0f / 3.0f, 1e-5f);

    EXPECT_EQ(nimcp_emergent_vote_human(el, 12345u, "x"), -1);  // unknown token
    EXPECT_EQ(nimcp_emergent_vote_human(el, t, ""), -1);
    EXPECT_EQ(nimcp_emergent_vote_human(NULL, t, "x"), -1);
    nimcp_emergent_language_destroy(el);
}

TEST(EmergentLanguageWiring, SaveLoadKeepsMultiWordGlossAndVotes) {
    const uint32_t dim = 16;
    nimcp_emergent_language_t* el = small_el(dim);
    std::vector<float> a(dim), b(dim);
    fill_random_embedding(a.data(), dim, 0.5f);
    for (uint32_t i = 0; i < dim; i++) b[i] = (i % 2) ? 1.0f : -1.0f;
    uint32_t ta, tb;
    nimcp_emergent_observe_ex(el, a.data(), dim, &ta);
    nimcp_emergent_observe_ex(el, b.data(), dim, &tb);
    nimcp_emergent_vote_human(el, ta, "the big dog");
    nimcp_emergent_vote_human(el, ta, "the big dog");
    nimcp_emergent_vote_human(el, ta, "cat");
    nimcp_emergent_vote_human(el, tb, "red ball");

    char path[] = "/tmp/emergent_wiring_XXXXXX";
    int fd = mkstemp(path);
    ASSERT_GE(fd, 0);
    close(fd);
    ASSERT_EQ(nimcp_emergent_save(el, path), 0);

    nimcp_emergent_language_t* el2 = small_el(dim);
    ASSERT_EQ(nimcp_emergent_load(el2, path), 0);
    EXPECT_EQ(nimcp_emergent_get_vocab_size(el2), 2u);
    nimcp_emergent_translation_t tr;
    ASSERT_EQ(nimcp_emergent_get_translation(el2, ta, &tr), 0);
    EXPECT_STREQ(tr.human_gloss, "the big dog");
    ASSERT_EQ(nimcp_emergent_get_translation(el2, tb, &tr), 0);
    EXPECT_STREQ(tr.human_gloss, "red ball");
    // Votes came back too: a further "the big dog" vote is a correct guess,
    // and purity matches the saved counts (3 of 4 votes are top words).
    EXPECT_EQ(nimcp_emergent_vote_human(el2, ta, "the big dog"), 1);
    float purity = 0.0f;
    nimcp_emergent_get_vote_stats(el2, NULL, &purity);
    EXPECT_NEAR(purity, 4.0f / 5.0f, 1e-5f);

    remove(path);
    nimcp_emergent_language_destroy(el);
    nimcp_emergent_language_destroy(el2);
}

TEST(EmergentLanguageWiring, MergeCombinesVotes) {
    const uint32_t dim = 16;
    nimcp_emergent_config_t cfg = nimcp_emergent_config_default();
    cfg.centroid_dim = dim; cfg.embed_dim = dim; cfg.max_tokens = 64;
    cfg.discovery_threshold = 0.001f;   // nearly every output becomes its own token
    cfg.merge_threshold = 0.2f;         // ...and close ones merge on merge_similar
    nimcp_emergent_language_t* el = nimcp_emergent_language_create(&cfg);
    std::vector<float> a(dim), a2(dim);
    fill_random_embedding(a.data(), dim, 0.5f);
    fill_similar_embedding(a2.data(), a.data(), dim, 0.05f);
    uint32_t t1, t2;
    nimcp_emergent_observe_ex(el, a.data(), dim, &t1);
    nimcp_emergent_observe_ex(el, a2.data(), dim, &t2);
    ASSERT_NE(t1, t2);
    nimcp_emergent_vote_human(el, t1, "dog");
    nimcp_emergent_vote_human(el, t2, "dog");
    nimcp_emergent_vote_human(el, t2, "cat");
    ASSERT_GE(nimcp_emergent_merge_similar(el), 1);
    EXPECT_EQ(nimcp_emergent_get_vocab_size(el), 1u);
    uint32_t labeled = 0; float purity = 0.0f;
    nimcp_emergent_get_vote_stats(el, &labeled, &purity);
    EXPECT_EQ(labeled, 1u);
    EXPECT_NEAR(purity, 2.0f / 3.0f, 1e-5f);   // dog 2, cat 1 on the merged token
    nimcp_emergent_language_destroy(el);
}

TEST(EmergentLanguageWiring, ActiveTokensReflectsRecentUse) {
    const uint32_t dim = 16;
    nimcp_emergent_language_t* el = small_el(dim);
    std::vector<float> a(dim), b(dim);
    fill_random_embedding(a.data(), dim, 0.5f);
    for (uint32_t i = 0; i < dim; i++) b[i] = (i % 2) ? 1.0f : -1.0f;
    nimcp_emergent_observe(el, b.data(), dim);          // token B, then never again
    for (int i = 0; i < 1100; i++) nimcp_emergent_observe(el, a.data(), dim);
    nimcp_emergent_stats_t s;
    ASSERT_EQ(nimcp_emergent_get_stats(el, &s), 0);
    EXPECT_EQ(s.vocab_size, 2u);
    EXPECT_EQ(s.active_tokens, 1u);
    nimcp_emergent_language_destroy(el);
}

// ============================================================================
// Bridge: labels voted onto the brain's own output at learn time
// ============================================================================

extern "C" {
#include "cognitive/language/nimcp_emergent_language_bridge.h"
}

TEST(EmergentLanguageBridge, LearnTimeOutputsCarryTheirLabels) {
    setenv("NIMCP_EMERGENT_LANGUAGE", "1", 1);
    unsetenv("NIMCP_EMERGENT_LANGUAGE_PATH");
    void* slot = nullptr;
    emergent_bridge_t* b = emergent_bridge_for(&slot);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(emergent_bridge_observe_learn_enabled());   // default on
    const uint32_t dim = 16;
    std::vector<float> cat(dim), dog(dim), v(dim);
    for (uint32_t i = 0; i < dim; i++) { cat[i] = (i < 8) ? 1.0f : 0.0f; dog[i] = (i < 8) ? 0.0f : 1.0f; }
    // Two clearly separate outputs, each always taught with its own word.
    // Every training input is distinct, so every observation is novel.
    std::vector<float> in_feat(4);
    for (int r = 0; r < 20; r++) {
        in_feat = {1.0f, (float)r, 0.0f, 0.0f};
        fill_similar_embedding(v.data(), cat.data(), dim, 0.02f * (r % 3));
        emergent_bridge_on_learn_output(b, in_feat.data(), 4, v.data(), dim, "cat");
        in_feat = {2.0f, (float)r, 0.0f, 0.0f};
        fill_similar_embedding(v.data(), dog.data(), dim, 0.02f * (r % 3));
        emergent_bridge_on_learn_output(b, in_feat.data(), 4, v.data(), dim, "dog");
    }
    // Unlabelled and invalid calls are ignored.
    emergent_bridge_on_learn_output(b, in_feat.data(), 4, cat.data(), dim, "");
    emergent_bridge_on_learn_output(b, in_feat.data(), 4, cat.data(), dim, nullptr);
    emergent_bridge_on_learn_output(b, nullptr, 4, cat.data(), dim, "cat");
    emergent_bridge_on_learn_output(nullptr, in_feat.data(), 4, cat.data(), dim, "cat");

    emergent_bridge_stats_t s;
    ASSERT_EQ(emergent_bridge_get_stats(b, &s), 0);
    EXPECT_EQ(s.learn_observations, 40u);
    EXPECT_EQ(s.decide_observations, 0u);
    EXPECT_EQ(s.observations, 40u);
    EXPECT_EQ(s.labeled_decides, 40u);
    EXPECT_EQ(s.distinct_labels, 2u);
    EXPECT_GE(s.labeled_tokens, 2u);
    ASSERT_GE(s.predictions, 30u);
    // After the first sighting of each, the token's gloss names the word
    // nearly every time; always guessing one word gets about half.
    EXPECT_GT((double)s.hits / s.predictions, 0.9);
    EXPECT_LT((double)s.majority_hits / s.labeled_decides, 0.6);
    // All inputs were new: the novel counters equal the overall ones.
    EXPECT_EQ(s.novel_labeled, 40u);
    EXPECT_EQ(s.novel_predictions, s.predictions);
    EXPECT_EQ(s.novel_hits, s.hits);
    EXPECT_EQ(s.new_tokens, (uint64_t)s.vocab_size);
    uint64_t hist_total = 0;
    for (int i = 0; i < 11; i++) hist_total += s.nearest_sim_hist[i];
    EXPECT_EQ(hist_total, s.observations);
    EXPECT_EQ(s.nearest_sim_hist[0], 1u);          // only the very first had no token
    EXPECT_GE(s.nearest_sim_hist[10], 30u);        // later ones sit near their token

    // A replay of an input already seen counts overall but not as novel.
    in_feat = {1.0f, 3.0f, 0.0f, 0.0f};
    emergent_bridge_on_learn_output(b, in_feat.data(), 4, cat.data(), dim, "cat");
    ASSERT_EQ(emergent_bridge_get_stats(b, &s), 0);
    EXPECT_EQ(s.learn_observations, 41u);
    EXPECT_EQ(s.novel_labeled, 40u);

    // The symbols for a cat-like output come from the cat token.
    char text[64];
    EXPECT_GT(emergent_bridge_express(b, cat.data(), dim, text, sizeof text), 0);

    // A decide is counted separately; it is labelled only if the same
    // input was trained recently.
    std::vector<float> in(4, 0.25f);
    emergent_bridge_on_learn(b, in.data(), 4, "dog");
    emergent_bridge_on_decide(b, in.data(), 4, dog.data(), dim);
    std::vector<float> other(4, 0.5f);
    emergent_bridge_on_decide(b, other.data(), 4, cat.data(), dim);
    ASSERT_EQ(emergent_bridge_get_stats(b, &s), 0);
    EXPECT_EQ(s.decide_observations, 2u);
    EXPECT_EQ(s.labeled_decides, 42u);
    // A different output size is a different head: ignored.
    std::vector<float> wide(dim * 2, 1.0f);
    emergent_bridge_on_learn_output(b, in_feat.data(), 4, wide.data(), dim * 2, "cat");
    ASSERT_EQ(emergent_bridge_get_stats(b, &s), 0);
    EXPECT_EQ(s.learn_observations, 41u);
    emergent_bridge_destroy(b);
}

TEST(EmergentLanguageBridge, TimedSaveAndDiscoveryOverride) {
    setenv("NIMCP_EMERGENT_LANGUAGE", "1", 1);
    char path[] = "/tmp/emergent_bridge_XXXXXX";
    int fd = mkstemp(path); ASSERT_GE(fd, 0); close(fd); remove(path);
    setenv("NIMCP_EMERGENT_LANGUAGE_PATH", path, 1);
    setenv("NIMCP_EMERGENT_SAVE_SECS", "1", 1);
    setenv("NIMCP_EMERGENT_DISCOVERY", "0.9", 1);   // new token only below cosine 0.1
    emergent_bridge_t* b = emergent_bridge_create_from_env();
    ASSERT_NE(b, nullptr);
    const uint32_t dim = 8;
    std::vector<float> f(2), a(dim, 0.0f), c(dim, 0.0f);
    a[0] = 1.0f; a[1] = 1.0f;                // cosine(a, c) = 0.5
    c[0] = 1.0f; c[2] = 1.0f;
    f = {1.0f, 0.0f};
    emergent_bridge_on_learn_output(b, f.data(), 2, a.data(), dim, "x");
    f = {2.0f, 0.0f};
    emergent_bridge_on_learn_output(b, f.data(), 2, c.data(), dim, "y");
    emergent_bridge_stats_t s;
    ASSERT_EQ(emergent_bridge_get_stats(b, &s), 0);
    EXPECT_EQ(s.vocab_size, 1u);             // default 0.3 would have made two
    EXPECT_EQ(s.nearest_sim_hist[6], 1u);    // the second sat at cosine 0.5
    FILE* probe = fopen(path, "rb");
    EXPECT_EQ(probe, nullptr);               // nothing saved yet
    if (probe) fclose(probe);
    usleep(1100 * 1000);
    f = {3.0f, 0.0f};
    emergent_bridge_on_learn_output(b, f.data(), 2, a.data(), dim, "x");
    probe = fopen(path, "rb");
    EXPECT_NE(probe, nullptr);               // the timer fired on this observation
    if (probe) fclose(probe);
    emergent_bridge_destroy(b);
    remove(path);
    unsetenv("NIMCP_EMERGENT_LANGUAGE_PATH");
    unsetenv("NIMCP_EMERGENT_SAVE_SECS");
    unsetenv("NIMCP_EMERGENT_DISCOVERY");
}
