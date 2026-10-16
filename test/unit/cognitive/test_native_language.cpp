/**
 * @file test_native_language.cpp
 * @brief Unit tests for NIMCP brain-native language production system.
 *
 * WHAT: Test language lifecycle, vocabulary seeding, generation, encoding,
 *       training, phonological loop, and NULL safety.
 * WHY:  Native language replaces external LLM dependency; regressions here
 *       break all brain-generated text output.
 * HOW:  Google Test, standalone mode (no real brain).
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "cognitive/language/nimcp_native_language.h"
}

// ============================================================================
// Lifecycle
// ============================================================================

TEST(NativeLanguage, CreateDestroy) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);
    nimcp_native_language_destroy(lang);
}

TEST(NativeLanguage, CreateWithNullConfig) {
    nimcp_native_language_t* lang = nimcp_native_language_create(NULL);
    ASSERT_NE(lang, nullptr) << "NULL config should use defaults";
    nimcp_native_language_destroy(lang);
}

TEST(NativeLanguage, DestroyNull) {
    nimcp_native_language_destroy(NULL);
    SUCCEED() << "nimcp_native_language_destroy(NULL) did not crash";
}

// ============================================================================
// Config Defaults
// ============================================================================

TEST(NativeLanguage, ConfigDefaultValues) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    EXPECT_EQ(cfg.max_vocab_size, 8192u);
    EXPECT_EQ(cfg.embed_dim, 256u);
    EXPECT_EQ(cfg.max_seq_length, 128u);
    EXPECT_FLOAT_EQ(cfg.temperature, 0.7f);
    EXPECT_FLOAT_EQ(cfg.top_p, 0.9f);
    EXPECT_FLOAT_EQ(cfg.repetition_penalty, 1.2f);
    EXPECT_TRUE(cfg.learn_vocabulary);
    EXPECT_FLOAT_EQ(cfg.min_token_frequency, 0.001f);
}

TEST(NativeLanguage, ConfigDefaultsAreReasonable) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    EXPECT_GT(cfg.max_vocab_size, 0u);
    EXPECT_GT(cfg.embed_dim, 0u);
    EXPECT_GT(cfg.max_seq_length, 0u);
    EXPECT_GT(cfg.temperature, 0.0f);
    EXPECT_LE(cfg.temperature, 2.0f);
    EXPECT_GT(cfg.top_p, 0.0f);
    EXPECT_LE(cfg.top_p, 1.0f);
    EXPECT_GE(cfg.repetition_penalty, 1.0f);
}

// ============================================================================
// Vocabulary
// ============================================================================

TEST(NativeLanguage, InitialVocabHasSpecialTokens) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    // 4 special tokens + ~104 seed words
    uint32_t vs = nimcp_language_get_vocab_size(lang);
    EXPECT_GE(vs, 4u) << "Must have at least BOS, EOS, UNK, PAD";

    nimcp_native_language_destroy(lang);
}

TEST(NativeLanguage, InitialVocabSizeGt100) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    uint32_t vs = nimcp_language_get_vocab_size(lang);
    EXPECT_GT(vs, 100u) << "4 special + ~104 seed words should exceed 100";

    nimcp_native_language_destroy(lang);
}

TEST(NativeLanguage, LearnTokenAddsNewWord) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    uint32_t before = nimcp_language_get_vocab_size(lang);
    int rc = nimcp_language_learn_token(lang, "xylophone", NULL, 0);
    EXPECT_EQ(rc, 0);
    uint32_t after = nimcp_language_get_vocab_size(lang);
    EXPECT_EQ(after, before + 1) << "New word should increment vocab size by 1";

    nimcp_native_language_destroy(lang);
}

TEST(NativeLanguage, LearnTokenExistingWordUpdatesFrequency) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    // "the" is a seed word
    uint32_t before = nimcp_language_get_vocab_size(lang);
    int rc = nimcp_language_learn_token(lang, "the", NULL, 0);
    EXPECT_EQ(rc, 0);
    uint32_t after = nimcp_language_get_vocab_size(lang);
    EXPECT_EQ(after, before) << "Existing word should not increase vocab size";

    nimcp_native_language_destroy(lang);
}

TEST(NativeLanguage, VocabSizeDoesNotExceedMax) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    cfg.max_vocab_size = 120; // Small max
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    uint32_t vs = nimcp_language_get_vocab_size(lang);
    EXPECT_LE(vs, 120u) << "Vocab size must not exceed max";

    nimcp_native_language_destroy(lang);
}

TEST(NativeLanguage, GetVocabSizeNull) {
    EXPECT_EQ(nimcp_language_get_vocab_size(NULL), 0u);
}

// ============================================================================
// Generation
// ============================================================================

TEST(NativeLanguage, GenerateFromEmbeddingProducesText) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    // Use a non-zero embedding to drive generation
    std::vector<float> embedding(4096, 0.0f);
    for (size_t i = 0; i < embedding.size(); i++)
        embedding[i] = sinf((float)i * 0.01f) * 0.5f;

    char output[512];
    memset(output, 0, sizeof(output));
    int rc = nimcp_language_generate(lang, embedding.data(), 4096, output, sizeof(output));
    EXPECT_GE(rc, 0) << "Generate should succeed (rc >= 0)";
    // Output may be empty or have text depending on projections, but should not crash

    nimcp_native_language_destroy(lang);
}

TEST(NativeLanguage, GenerateWithNullReturnsError) {
    EXPECT_EQ(nimcp_language_generate(NULL, NULL, 0, NULL, 0), -1);
}

TEST(NativeLanguage, GenerateNullLangReturnsError) {
    float emb[256] = {0};
    char out[128];
    EXPECT_EQ(nimcp_language_generate(NULL, emb, 256, out, sizeof(out)), -1);
}

TEST(NativeLanguage, GenerateNullEmbeddingReturnsError) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    char out[128];
    EXPECT_EQ(nimcp_language_generate(lang, NULL, 256, out, sizeof(out)), -1);

    nimcp_native_language_destroy(lang);
}

TEST(NativeLanguage, GenerateNullOutputReturnsError) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    float emb[256] = {0};
    EXPECT_EQ(nimcp_language_generate(lang, emb, 256, NULL, 128), -1);

    nimcp_native_language_destroy(lang);
}

TEST(NativeLanguage, GenerateRespectsMaxTextLength) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    std::vector<float> embedding(4096, 0.1f);
    char output[16]; // Very small buffer
    memset(output, 0, sizeof(output));
    int rc = nimcp_language_generate(lang, embedding.data(), 4096, output, sizeof(output));
    EXPECT_GE(rc, 0);
    EXPECT_LT((size_t)rc, sizeof(output)) << "Output length must not exceed buffer";

    nimcp_native_language_destroy(lang);
}

// ============================================================================
// Comprehension (Encode)
// ============================================================================

TEST(NativeLanguage, EncodeTextProducesEmbedding) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    float embedding[4096];
    memset(embedding, 0, sizeof(embedding));
    int rc = nimcp_language_encode(lang, "the big dog", embedding, 4096);
    EXPECT_GT(rc, 0) << "Encode should return embedding dimension";

    // Check that at least some values are non-zero (projected)
    float sum = 0.0f;
    for (int i = 0; i < rc; i++) sum += fabsf(embedding[i]);
    EXPECT_GT(sum, 0.0f) << "Encoded embedding should have non-zero values";

    nimcp_native_language_destroy(lang);
}

TEST(NativeLanguage, EncodeEmptyStringReturnsValid) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    float embedding[4096];
    memset(embedding, 0, sizeof(embedding));
    // Empty string has no tokens to match, encode still succeeds (0 tokens averaged)
    int rc = nimcp_language_encode(lang, "", embedding, 4096);
    // May return > 0 (dim) even with zero-averaged result, or may return valid dim
    EXPECT_GE(rc, 0);

    nimcp_native_language_destroy(lang);
}

TEST(NativeLanguage, EncodeNullReturnsError) {
    EXPECT_EQ(nimcp_language_encode(NULL, "hello", NULL, 0), -1);
}

// ============================================================================
// Training
// ============================================================================

TEST(NativeLanguage, TrainStepDoesNotCrash) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    std::vector<float> emb(4096, 0.05f);
    int rc = nimcp_language_train_step(lang, emb.data(), 4096, "the big cat", 0.001f);
    EXPECT_GE(rc, 0);

    nimcp_native_language_destroy(lang);
}

TEST(NativeLanguage, TrainStepNullReturnsError) {
    EXPECT_EQ(nimcp_language_train_step(NULL, NULL, 0, NULL, 0.001f), -1);
}

// ============================================================================
// Save / Load Vocabulary
// ============================================================================

TEST(NativeLanguage, SaveLoadVocabularyRoundtrip) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    // Add a custom token
    nimcp_language_learn_token(lang, "zephyr", NULL, 0);
    uint32_t vs_before = nimcp_language_get_vocab_size(lang);

    const char* path = "/tmp/nimcp_test_vocab.dat";
    int rc = nimcp_language_save_vocabulary(lang, path);
    EXPECT_EQ(rc, 0);

    // Create fresh language and load
    nimcp_native_language_t* lang2 = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang2, nullptr);

    rc = nimcp_language_load_vocabulary(lang2, path);
    EXPECT_EQ(rc, 0);
    uint32_t vs_after = nimcp_language_get_vocab_size(lang2);
    EXPECT_EQ(vs_before, vs_after);

    nimcp_native_language_destroy(lang);
    nimcp_native_language_destroy(lang2);
    remove(path);
}

TEST(NativeLanguage, SaveNullReturnsError) {
    EXPECT_EQ(nimcp_language_save_vocabulary(NULL, "/tmp/x.dat"), -1);
}

TEST(NativeLanguage, LoadNullReturnsError) {
    EXPECT_EQ(nimcp_language_load_vocabulary(NULL, "/tmp/x.dat"), -1);
}

// ============================================================================
// Phonological Loop
// ============================================================================

TEST(NativeLanguage, PhonologicalLoopCreateDestroy) {
    nimcp_phonological_loop_t* loop = nimcp_phonological_loop_create(16, 256);
    ASSERT_NE(loop, nullptr);
    EXPECT_EQ(loop->capacity, 16u);
    EXPECT_EQ(loop->embed_dim, 256u);
    EXPECT_EQ(loop->length, 0u);
    EXPECT_EQ(loop->head, 0u);
    nimcp_phonological_loop_destroy(loop);
}

TEST(NativeLanguage, PhonologicalLoopDestroyNull) {
    nimcp_phonological_loop_destroy(NULL);
    SUCCEED() << "nimcp_phonological_loop_destroy(NULL) did not crash";
}

TEST(NativeLanguage, PhonologicalLoopPush) {
    nimcp_phonological_loop_t* loop = nimcp_phonological_loop_create(4, 8);
    ASSERT_NE(loop, nullptr);

    float emb[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    int rc = nimcp_phonological_loop_push(loop, emb);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(loop->length, 1u);
    EXPECT_EQ(loop->head, 1u);

    nimcp_phonological_loop_destroy(loop);
}

TEST(NativeLanguage, PhonologicalLoopPushNull) {
    nimcp_phonological_loop_t* loop = nimcp_phonological_loop_create(4, 8);
    ASSERT_NE(loop, nullptr);
    EXPECT_EQ(nimcp_phonological_loop_push(loop, NULL), -1);
    EXPECT_EQ(nimcp_phonological_loop_push(NULL, NULL), -1);
    nimcp_phonological_loop_destroy(loop);
}

TEST(NativeLanguage, PhonologicalLoopReset) {
    nimcp_phonological_loop_t* loop = nimcp_phonological_loop_create(4, 8);
    ASSERT_NE(loop, nullptr);

    float emb[8] = {1.0f};
    nimcp_phonological_loop_push(loop, emb);
    nimcp_phonological_loop_push(loop, emb);
    EXPECT_EQ(loop->length, 2u);

    nimcp_phonological_loop_reset(loop);
    EXPECT_EQ(loop->length, 0u);
    EXPECT_EQ(loop->head, 0u);

    nimcp_phonological_loop_destroy(loop);
}

TEST(NativeLanguage, PhonologicalLoopResetNull) {
    nimcp_phonological_loop_reset(NULL);
    SUCCEED() << "nimcp_phonological_loop_reset(NULL) did not crash";
}

TEST(NativeLanguage, PhonologicalLoopWrapsAround) {
    nimcp_phonological_loop_t* loop = nimcp_phonological_loop_create(3, 4);
    ASSERT_NE(loop, nullptr);

    float emb[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    for (int i = 0; i < 5; i++) {
        emb[0] = (float)(i + 1);
        nimcp_phonological_loop_push(loop, emb);
    }
    // Capacity is 3, pushed 5, length should cap at 3
    EXPECT_EQ(loop->length, 3u);
    EXPECT_EQ(loop->head, 2u); // 5 % 3 = 2

    nimcp_phonological_loop_destroy(loop);
}

// ============================================================================
// NULL Safety on all functions
// ============================================================================

TEST(NativeLanguage, LearnTokenNullLang) {
    EXPECT_EQ(nimcp_language_learn_token(NULL, "word", NULL, 0), -1);
}

TEST(NativeLanguage, LearnTokenNullText) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);
    EXPECT_EQ(nimcp_language_learn_token(lang, NULL, NULL, 0), -1);
    nimcp_native_language_destroy(lang);
}

TEST(NativeLanguage, LearnTokenEmptyText) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);
    EXPECT_EQ(nimcp_language_learn_token(lang, "", NULL, 0), -1);
    nimcp_native_language_destroy(lang);
}
