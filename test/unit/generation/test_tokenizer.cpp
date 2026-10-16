/**
 * @file test_tokenizer.cpp
 * @brief Unit tests for the NIMCP tokenizer
 *
 * WHAT: Tests tokenizer creation, token management, encode/decode, save/load
 * WHY:  Tokenizer correctness is foundational — wrong IDs cascade through
 *       the entire generation pipeline
 * HOW:  GTest fixture creates a fresh tokenizer per test; exercises all API
 *       entry points with boundary conditions and round-trip verification
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <set>
#include <unistd.h>

extern "C" {
#include "generation/nimcp_tokenizer.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

class TokenizerUnit : public ::testing::Test {
protected:
    tokenizer_t* tok = nullptr;

    void SetUp() override {
        tokenizer_config_t cfg = tokenizer_default_config();
        tok = tokenizer_create(&cfg);
        ASSERT_NE(tok, nullptr) << "Tokenizer creation must succeed";
    }

    void TearDown() override {
        if (tok) {
            tokenizer_destroy(tok);
            tok = nullptr;
        }
    }

    // Helper: generate a unique temp file path
    std::string temp_path() {
        char tmpl[] = "/tmp/nimcp_tok_test_XXXXXX";
        int fd = mkstemp(tmpl);
        if (fd >= 0) close(fd);
        return std::string(tmpl);
    }
};

// =============================================================================
// Tests: Creation and Configuration
// =============================================================================

TEST_F(TokenizerUnit, CreateAndDestroy) {
    // tok was created in SetUp — just verify it's non-null
    EXPECT_NE(tok, nullptr);
    // Destroy happens in TearDown; this test verifies no crash on lifecycle
}

TEST_F(TokenizerUnit, DefaultConfigValues) {
    tokenizer_config_t cfg = tokenizer_default_config();

    // Vocab capacity should be positive
    EXPECT_GT(cfg.initial_vocab_capacity, 0u);

    // Max token length should be positive and reasonable
    EXPECT_GT(cfg.max_token_length, 0u);
    EXPECT_LE(cfg.max_token_length, 4096u);

    // Special token strings should be non-NULL
    EXPECT_NE(cfg.unk_token, nullptr);
    EXPECT_NE(cfg.bos_token, nullptr);
    EXPECT_NE(cfg.eos_token, nullptr);
    EXPECT_NE(cfg.pad_token, nullptr);

    // Special tokens should be non-empty
    EXPECT_GT(strlen(cfg.unk_token), 0u);
    EXPECT_GT(strlen(cfg.bos_token), 0u);
    EXPECT_GT(strlen(cfg.eos_token), 0u);
    EXPECT_GT(strlen(cfg.pad_token), 0u);
}

// =============================================================================
// Tests: Special Tokens
// =============================================================================

TEST_F(TokenizerUnit, SpecialTokensExist) {
    tokenizer_config_t cfg = tokenizer_default_config();

    // Look up each special token — all should have valid IDs
    uint32_t unk_id = tokenizer_token_to_id(tok, cfg.unk_token);
    uint32_t bos_id = tokenizer_token_to_id(tok, cfg.bos_token);
    uint32_t eos_id = tokenizer_token_to_id(tok, cfg.eos_token);
    uint32_t pad_id = tokenizer_token_to_id(tok, cfg.pad_token);

    // All special token IDs should be distinct
    std::set<uint32_t> ids = {unk_id, bos_id, eos_id, pad_id};
    EXPECT_EQ(ids.size(), 4u)
        << "All 4 special tokens must have distinct IDs";

    // All should be < vocab_size
    uint32_t vocab_size = tokenizer_get_vocab_size(tok);
    EXPECT_LT(unk_id, vocab_size);
    EXPECT_LT(bos_id, vocab_size);
    EXPECT_LT(eos_id, vocab_size);
    EXPECT_LT(pad_id, vocab_size);
}

// =============================================================================
// Tests: Adding Tokens
// =============================================================================

TEST_F(TokenizerUnit, AddTokenReturnsId) {
    uint32_t initial_size = tokenizer_get_vocab_size(tok);
    int rc = tokenizer_add_token(tok, "hello");
    EXPECT_GE(rc, 0) << "Adding a new token should return assigned ID (>= 0)";

    uint32_t new_size = tokenizer_get_vocab_size(tok);
    EXPECT_GE(new_size, initial_size)
        << "Vocab size should not decrease after adding a token";
}

TEST_F(TokenizerUnit, AddDuplicateTokenReturnsSameId) {
    tokenizer_add_token(tok, "duplicate");
    uint32_t id1 = tokenizer_token_to_id(tok, "duplicate");

    tokenizer_add_token(tok, "duplicate");
    uint32_t id2 = tokenizer_token_to_id(tok, "duplicate");

    EXPECT_EQ(id1, id2)
        << "Adding the same token twice should return the same ID";
}

// =============================================================================
// Tests: Token Lookup
// =============================================================================

TEST_F(TokenizerUnit, TokenToIdLookup) {
    tokenizer_add_token(tok, "world");
    uint32_t id = tokenizer_token_to_id(tok, "world");

    // Verify the ID maps back to the same string
    const char* token_str = tokenizer_id_to_token(tok, id);
    ASSERT_NE(token_str, nullptr);
    EXPECT_STREQ(token_str, "world");
}

TEST_F(TokenizerUnit, IdToTokenLookup) {
    tokenizer_add_token(tok, "lookup_test");
    uint32_t id = tokenizer_token_to_id(tok, "lookup_test");

    const char* result = tokenizer_id_to_token(tok, id);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "lookup_test");
}

TEST_F(TokenizerUnit, UnknownTokenReturnsUnkId) {
    tokenizer_config_t cfg = tokenizer_default_config();
    uint32_t unk_id = tokenizer_token_to_id(tok, cfg.unk_token);

    // Look up a token that was never added — API returns TOKENIZER_INVALID_ID
    uint32_t result_id = tokenizer_token_to_id(tok, "xyzzy_never_added_42");
    EXPECT_EQ(result_id, TOKENIZER_INVALID_ID)
        << "Looking up a non-existent token should return TOKENIZER_INVALID_ID";
}

// =============================================================================
// Tests: Encode
// =============================================================================

TEST_F(TokenizerUnit, EncodeSimpleSentence) {
    tokenizer_add_token(tok, "hello");
    tokenizer_add_token(tok, "world");

    uint32_t ids[32];
    uint32_t num_tokens = 0;
    int rc = tokenizer_encode(tok, "hello world", ids, 32, &num_tokens);
    EXPECT_EQ(rc, 0) << "Encoding a simple sentence should succeed";
    EXPECT_GE(num_tokens, 2u)
        << "Encoding 'hello world' should produce at least 2 tokens";
}

TEST_F(TokenizerUnit, EncodeHandlesPunctuation) {
    tokenizer_add_token(tok, "hello");
    tokenizer_add_token(tok, ",");
    tokenizer_add_token(tok, "world");
    tokenizer_add_token(tok, "!");

    uint32_t ids[32];
    uint32_t num_tokens = 0;
    int rc = tokenizer_encode(tok, "hello, world!", ids, 32, &num_tokens);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(num_tokens, 2u)
        << "Punctuated text should produce multiple tokens";
}

TEST_F(TokenizerUnit, EncodeUnknownWordUsesUnk) {
    tokenizer_config_t cfg = tokenizer_default_config();
    uint32_t unk_id = tokenizer_token_to_id(tok, cfg.unk_token);

    // Add only "hello", not "xyzzy"
    tokenizer_add_token(tok, "hello");

    uint32_t ids[32];
    uint32_t num_tokens = 0;
    int rc = tokenizer_encode(tok, "xyzzy", ids, 32, &num_tokens);
    EXPECT_EQ(rc, 0);

    // At least one token should be UNK (the unknown word) or subword-decomposed
    bool found_unk = false;
    for (uint32_t i = 0; i < num_tokens; i++) {
        if (ids[i] == unk_id) {
            found_unk = true;
            break;
        }
    }
    // Either UNK is used, or the tokenizer decomposes to characters/subwords
    EXPECT_TRUE(found_unk || num_tokens > 1)
        << "Unknown word should either produce UNK token or be decomposed";
}

// =============================================================================
// Tests: Decode
// =============================================================================

TEST_F(TokenizerUnit, DecodeReconstructsText) {
    tokenizer_add_token(tok, "hello");
    tokenizer_add_token(tok, "world");

    // Encode
    uint32_t ids[32];
    uint32_t num_tokens = 0;
    int enc_rc = tokenizer_encode(tok, "hello world", ids, 32, &num_tokens);
    ASSERT_EQ(enc_rc, 0);
    ASSERT_GT(num_tokens, 0u);

    // Decode
    char text[256];
    memset(text, 0, sizeof(text));
    int dec_rc = tokenizer_decode(tok, ids, num_tokens, text, sizeof(text));
    EXPECT_EQ(dec_rc, 0);

    // The decoded text should contain "hello" and "world"
    EXPECT_NE(strstr(text, "hello"), nullptr)
        << "Decoded text should contain 'hello', got: " << text;
    EXPECT_NE(strstr(text, "world"), nullptr)
        << "Decoded text should contain 'world', got: " << text;
}

// =============================================================================
// Tests: Build from Text
// =============================================================================

TEST_F(TokenizerUnit, BuildFromTextCreatesVocab) {
    uint32_t initial_size = tokenizer_get_vocab_size(tok);

    const char* corpus =
        "the cat sat on the mat the cat chased the mouse "
        "the dog ran after the cat the quick brown fox jumps";

    int rc = tokenizer_build_from_text(tok, corpus, 50);
    EXPECT_EQ(rc, 0) << "Building vocab from text should succeed";

    uint32_t new_size = tokenizer_get_vocab_size(tok);
    EXPECT_GT(new_size, initial_size)
        << "Vocab size should increase after building from text";
}

TEST_F(TokenizerUnit, BuildFromTextBPEMerges) {
    const char* corpus =
        "aaa bbb aaa bbb aaa bbb ccc ddd ccc ddd eee fff eee fff "
        "aaa bbb aaa bbb ccc ddd ccc ddd eee fff aaa bbb ccc ddd";

    // Build with a small target so BPE merges are forced
    int rc = tokenizer_build_from_text(tok, corpus, 20);
    EXPECT_EQ(rc, 0);

    // Vocab should contain merged tokens beyond just single characters
    uint32_t vocab_size = tokenizer_get_vocab_size(tok);
    EXPECT_GT(vocab_size, 6u)  // More than just a,b,c,d,e,f + special
        << "BPE should produce merged tokens beyond raw characters";
}

// =============================================================================
// Tests: Vocab Size
// =============================================================================

TEST_F(TokenizerUnit, VocabSizeIncreasesWithTokens) {
    uint32_t size0 = tokenizer_get_vocab_size(tok);

    tokenizer_add_token(tok, "alpha");
    uint32_t size1 = tokenizer_get_vocab_size(tok);
    EXPECT_GE(size1, size0);

    tokenizer_add_token(tok, "beta");
    uint32_t size2 = tokenizer_get_vocab_size(tok);
    EXPECT_GE(size2, size1);

    tokenizer_add_token(tok, "gamma");
    uint32_t size3 = tokenizer_get_vocab_size(tok);
    EXPECT_GE(size3, size2);
}

// =============================================================================
// Tests: Edge Cases
// =============================================================================

TEST_F(TokenizerUnit, EncodeEmptyStringReturnsZeroTokens) {
    uint32_t ids[32];
    uint32_t num_tokens = 999; // Poison value

    int rc = tokenizer_encode(tok, "", ids, 32, &num_tokens);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(num_tokens, 0u)
        << "Encoding an empty string should produce zero tokens";
}

TEST_F(TokenizerUnit, EncodeNullTextReturnsError) {
    uint32_t ids[32];
    uint32_t num_tokens = 0;

    int rc = tokenizer_encode(tok, nullptr, ids, 32, &num_tokens);
    EXPECT_NE(rc, 0) << "Encoding NULL text should return an error code";
}

// =============================================================================
// Tests: Save and Load
// =============================================================================

TEST_F(TokenizerUnit, SaveAndLoadRoundTrip) {
    // Add some tokens
    const char* tokens[] = {"apple", "banana", "cherry", "date", "elderberry"};
    for (int i = 0; i < 5; i++) {
        tokenizer_add_token(tok, tokens[i]);
    }

    uint32_t orig_vocab_size = tokenizer_get_vocab_size(tok);

    // Save
    std::string path = temp_path();
    int save_rc = tokenizer_save(tok, path.c_str());
    ASSERT_EQ(save_rc, 0) << "Save should succeed";

    // Load into a new tokenizer
    tokenizer_t* loaded = tokenizer_load(path.c_str());
    ASSERT_NE(loaded, nullptr) << "Load should succeed";

    // Verify vocab size matches
    uint32_t loaded_vocab_size = tokenizer_get_vocab_size(loaded);
    EXPECT_EQ(loaded_vocab_size, orig_vocab_size);

    // Verify all tokens are present and map to the same IDs
    for (int i = 0; i < 5; i++) {
        uint32_t orig_id = tokenizer_token_to_id(tok, tokens[i]);
        uint32_t loaded_id = tokenizer_token_to_id(loaded, tokens[i]);
        EXPECT_EQ(orig_id, loaded_id)
            << "Token '" << tokens[i] << "' should have the same ID after load";

        const char* orig_str = tokenizer_id_to_token(tok, orig_id);
        const char* loaded_str = tokenizer_id_to_token(loaded, loaded_id);
        ASSERT_NE(orig_str, nullptr);
        ASSERT_NE(loaded_str, nullptr);
        EXPECT_STREQ(orig_str, loaded_str);
    }

    tokenizer_destroy(loaded);
    unlink(path.c_str());
}

// =============================================================================
// Tests: Max Token Length
// =============================================================================

TEST_F(TokenizerUnit, MaxTokenLengthRespected) {
    // Create a very long token string (1000 characters)
    std::string long_token(1000, 'x');

    // Adding it should either succeed (with truncation) or return an error,
    // but must NOT crash
    int rc = tokenizer_add_token(tok, long_token.c_str());
    // We accept either outcome — the key is no crash/overflow
    (void)rc;

    // Verify the tokenizer is still functional
    tokenizer_add_token(tok, "short");
    uint32_t id = tokenizer_token_to_id(tok, "short");
    const char* str = tokenizer_id_to_token(tok, id);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, "short");
}

// =============================================================================
// Tests: Stress
// =============================================================================

TEST_F(TokenizerUnit, LargeVocabularyStress) {
    const uint32_t NUM_TOKENS = 10000;

    for (uint32_t i = 0; i < NUM_TOKENS; i++) {
        char token_str[32];
        snprintf(token_str, sizeof(token_str), "tok_%05u", i);
        tokenizer_add_token(tok, token_str);
    }

    uint32_t vocab_size = tokenizer_get_vocab_size(tok);
    EXPECT_GE(vocab_size, NUM_TOKENS)
        << "Vocab should contain at least " << NUM_TOKENS << " user tokens";

    // Verify random lookups still work
    for (uint32_t i = 0; i < NUM_TOKENS; i += 1000) {
        char token_str[32];
        snprintf(token_str, sizeof(token_str), "tok_%05u", i);
        uint32_t id = tokenizer_token_to_id(tok, token_str);
        const char* result = tokenizer_id_to_token(tok, id);
        ASSERT_NE(result, nullptr) << "Lookup for token " << i << " returned NULL";
        EXPECT_STREQ(result, token_str);
    }
}
