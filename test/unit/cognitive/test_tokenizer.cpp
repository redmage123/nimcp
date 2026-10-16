/**
 * @file test_tokenizer.cpp
 * @brief Unit tests for NIMCP brain-native tokenizer (BPE-style).
 *
 * WHAT: Test tokenizer lifecycle, encode/decode, vocabulary management,
 *       merge rules, embeddings, save/load, and NULL safety.
 * WHY:  Tokenizer is the text-to-token bridge for native language;
 *       encode/decode bugs silently corrupt all language processing.
 * HOW:  Google Test, standalone mode.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "cognitive/language/nimcp_tokenizer.h"
}

// ============================================================================
// Lifecycle
// ============================================================================

TEST(Tokenizer, CreateDestroy) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);
    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, CreateWithNullConfig) {
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(NULL);
    ASSERT_NE(tok, nullptr) << "NULL config should use defaults";
    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, DestroyNull) {
    nimcp_tokenizer_destroy(NULL);
    SUCCEED() << "nimcp_tokenizer_destroy(NULL) did not crash";
}

// ============================================================================
// Config Defaults
// ============================================================================

TEST(Tokenizer, ConfigDefaultValues) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    EXPECT_EQ(cfg.max_vocab_size, 8192u);
    EXPECT_EQ(cfg.min_frequency, 2u);
    EXPECT_EQ(cfg.embed_dim, 256u);
    EXPECT_FLOAT_EQ(cfg.merge_threshold, 10.0f);
    EXPECT_TRUE(cfg.learn_from_brain);
    EXPECT_TRUE(cfg.enable_subword);
    EXPECT_TRUE(cfg.enable_phrase);
    EXPECT_EQ(cfg.max_token_length, 32u);
}

// ============================================================================
// Initial Vocabulary
// ============================================================================

TEST(Tokenizer, InitialVocabHasSpecialTokens) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    // Special tokens at positions 0-3
    EXPECT_STREQ(nimcp_tokenizer_id_to_text(tok, 0), "<BOS>");
    EXPECT_STREQ(nimcp_tokenizer_id_to_text(tok, 1), "<EOS>");
    EXPECT_STREQ(nimcp_tokenizer_id_to_text(tok, 2), "<UNK>");
    EXPECT_STREQ(nimcp_tokenizer_id_to_text(tok, 3), "<PAD>");

    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, InitialVocabHasASCII) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    // 4 special + 95 printable ASCII (32..126) = 99
    uint32_t vs = nimcp_tokenizer_get_vocab_size(tok);
    EXPECT_GE(vs, 99u) << "Should have at least 4 special + 95 ASCII";

    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, GetVocabSizeNull) {
    EXPECT_EQ(nimcp_tokenizer_get_vocab_size(NULL), 0u);
}

TEST(Tokenizer, GetNumMergesInitiallyZero) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);
    EXPECT_EQ(nimcp_tokenizer_get_num_merges(tok), 0u);
    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, GetNumMergesNull) {
    EXPECT_EQ(nimcp_tokenizer_get_num_merges(NULL), 0u);
}

// ============================================================================
// Encode
// ============================================================================

TEST(Tokenizer, EncodeSimpleText) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    uint32_t ids[256];
    int n = nimcp_tokenizer_encode(tok, "hi", ids, 256);
    EXPECT_EQ(n, 2) << "\"hi\" should produce 2 character tokens";
    // Each character should map to its ASCII slot
    EXPECT_NE(ids[0], 2u) << "h should not be UNK";
    EXPECT_NE(ids[1], 2u) << "i should not be UNK";

    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, EncodeNullReturnsError) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    uint32_t ids[16];
    EXPECT_EQ(nimcp_tokenizer_encode(tok, NULL, ids, 16), -1);
    EXPECT_EQ(nimcp_tokenizer_encode(NULL, "hi", ids, 16), -1);
    EXPECT_EQ(nimcp_tokenizer_encode(tok, "hi", NULL, 16), -1);
    EXPECT_EQ(nimcp_tokenizer_encode(tok, "hi", ids, 0), -1);

    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, EncodeEmptyStringReturns0Tokens) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    uint32_t ids[16];
    int n = nimcp_tokenizer_encode(tok, "", ids, 16);
    EXPECT_EQ(n, 0) << "Empty string should produce 0 tokens";

    nimcp_tokenizer_destroy(tok);
}

// ============================================================================
// Decode
// ============================================================================

TEST(Tokenizer, DecodeTokenIds) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    // Encode "AB" then decode back
    uint32_t ids[16];
    int n = nimcp_tokenizer_encode(tok, "AB", ids, 16);
    ASSERT_EQ(n, 2);

    char text[64];
    int len = nimcp_tokenizer_decode(tok, ids, (uint32_t)n, text, sizeof(text));
    EXPECT_EQ(len, 2);
    EXPECT_STREQ(text, "AB");

    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, DecodeNullReturnsError) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    uint32_t ids[] = {4, 5};
    char text[64];
    EXPECT_EQ(nimcp_tokenizer_decode(NULL, ids, 2, text, 64), -1);
    EXPECT_EQ(nimcp_tokenizer_decode(tok, NULL, 2, text, 64), -1);
    EXPECT_EQ(nimcp_tokenizer_decode(tok, ids, 2, NULL, 64), -1);
    EXPECT_EQ(nimcp_tokenizer_decode(tok, ids, 2, text, 0), -1);

    nimcp_tokenizer_destroy(tok);
}

// ============================================================================
// Encode / Decode Roundtrip
// ============================================================================

TEST(Tokenizer, EncodeDecodeRoundtrip) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    const char* original = "Hello World";
    uint32_t ids[256];
    int n = nimcp_tokenizer_encode(tok, original, ids, 256);
    ASSERT_GT(n, 0);

    char decoded[256];
    int len = nimcp_tokenizer_decode(tok, ids, (uint32_t)n, decoded, sizeof(decoded));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(decoded, original) << "Character-level roundtrip should preserve text";

    nimcp_tokenizer_destroy(tok);
}

// ============================================================================
// Text-to-ID and ID-to-Text Lookup
// ============================================================================

TEST(Tokenizer, TextToIdForKnownChar) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    // 'A' is ASCII 65, added at position 4 + (65 - 32) = 37
    uint32_t id = nimcp_tokenizer_text_to_id(tok, "A");
    EXPECT_NE(id, 2u) << "'A' should not be UNK";
    EXPECT_GE(id, 4u) << "'A' should be after special tokens";

    const char* text = nimcp_tokenizer_id_to_text(tok, id);
    EXPECT_STREQ(text, "A");

    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, TextToIdUnknownReturnsUNK) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    uint32_t id = nimcp_tokenizer_text_to_id(tok, "xyzzy_not_found_99");
    EXPECT_EQ(id, 2u) << "Unknown text should return UNK id (2)";

    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, IdToTextOutOfRange) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    const char* text = nimcp_tokenizer_id_to_text(tok, 99999);
    EXPECT_STREQ(text, "<UNK>");

    nimcp_tokenizer_destroy(tok);
}

// ============================================================================
// Training — Learn
// ============================================================================

TEST(Tokenizer, TrainOnCorpusExpandsVocab) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    uint32_t before = nimcp_tokenizer_get_vocab_size(tok);
    const char* texts[] = {"hello world", "goodbye world", "hello again"};
    int rc = nimcp_tokenizer_train(tok, texts, 3);
    EXPECT_EQ(rc, 0);

    uint32_t after = nimcp_tokenizer_get_vocab_size(tok);
    EXPECT_GT(after, before) << "Training should add word-level tokens";

    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, TrainNullReturnsError) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    EXPECT_EQ(nimcp_tokenizer_train(NULL, NULL, 0), -1);
    EXPECT_EQ(nimcp_tokenizer_train(tok, NULL, 0), -1);
    const char* texts[] = {"hello"};
    EXPECT_EQ(nimcp_tokenizer_train(tok, texts, 0), -1);

    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, LearnSingleText) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    uint32_t before = nimcp_tokenizer_get_vocab_size(tok);
    int rc = nimcp_tokenizer_learn(tok, "quantum entanglement");
    EXPECT_EQ(rc, 0);
    uint32_t after = nimcp_tokenizer_get_vocab_size(tok);
    EXPECT_GT(after, before);

    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, LearnFromBrainWithEmbedding) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    float emb[256];
    for (int i = 0; i < 256; i++) emb[i] = 0.01f * (float)i;

    int rc = nimcp_tokenizer_learn_from_brain(tok, "neural plasticity", emb, 256);
    EXPECT_EQ(rc, 0);

    nimcp_tokenizer_destroy(tok);
}

// ============================================================================
// Embeddings
// ============================================================================

TEST(Tokenizer, GetEmbeddingInitiallyNull) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    // Character tokens initially have no embedding
    const float* emb = nimcp_tokenizer_get_embedding(tok, 4);
    EXPECT_EQ(emb, nullptr) << "Initial char tokens should have NULL embedding";

    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, SetAndGetEmbedding) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    float emb[256];
    for (int i = 0; i < 256; i++) emb[i] = (float)i * 0.1f;

    int rc = nimcp_tokenizer_set_embedding(tok, 4, emb, 256);
    EXPECT_EQ(rc, 0);

    const float* got = nimcp_tokenizer_get_embedding(tok, 4);
    ASSERT_NE(got, nullptr);
    EXPECT_FLOAT_EQ(got[0], 0.0f);
    EXPECT_NEAR(got[1], 0.1f, 0.001f);

    nimcp_tokenizer_destroy(tok);
}

TEST(Tokenizer, GetEmbeddingNullTok) {
    EXPECT_EQ(nimcp_tokenizer_get_embedding(NULL, 0), nullptr);
}

TEST(Tokenizer, SetEmbeddingNullReturnsError) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    EXPECT_EQ(nimcp_tokenizer_set_embedding(NULL, 0, NULL, 0), -1);
    EXPECT_EQ(nimcp_tokenizer_set_embedding(tok, 0, NULL, 256), -1);

    nimcp_tokenizer_destroy(tok);
}

// ============================================================================
// Save / Load
// ============================================================================

TEST(Tokenizer, SaveLoadRoundtrip) {
    nimcp_tokenizer_config_t cfg = nimcp_tokenizer_config_default();
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok, nullptr);

    // Train to expand vocab
    nimcp_tokenizer_learn(tok, "hello world");
    uint32_t vs_before = nimcp_tokenizer_get_vocab_size(tok);

    const char* path = "/tmp/nimcp_test_tokenizer.dat";
    int rc = nimcp_tokenizer_save(tok, path);
    EXPECT_EQ(rc, 0);

    // Create fresh tokenizer and load
    nimcp_tokenizer_t* tok2 = nimcp_tokenizer_create(&cfg);
    ASSERT_NE(tok2, nullptr);

    rc = nimcp_tokenizer_load(tok2, path);
    EXPECT_EQ(rc, 0);
    uint32_t vs_after = nimcp_tokenizer_get_vocab_size(tok2);
    EXPECT_EQ(vs_before, vs_after);

    nimcp_tokenizer_destroy(tok);
    nimcp_tokenizer_destroy(tok2);
    remove(path);
}

TEST(Tokenizer, SaveNullReturnsError) {
    EXPECT_EQ(nimcp_tokenizer_save(NULL, "/tmp/x.dat"), -1);
}

TEST(Tokenizer, LoadNullReturnsError) {
    EXPECT_EQ(nimcp_tokenizer_load(NULL, "/tmp/x.dat"), -1);
}
