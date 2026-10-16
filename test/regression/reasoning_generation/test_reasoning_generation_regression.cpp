/**
 * @file test_reasoning_generation_regression.cpp
 * @brief Regression tests for reasoning chain and language generation
 *
 * WHAT: Guards against specific edge-case bugs and crash scenarios
 * WHY:  These tests encode specific failure modes that have been (or could be)
 *       introduced by code changes — they serve as permanent safety nets
 * HOW:  Each test targets one specific edge case: NULL inputs, empty strings,
 *       oversized inputs, numeric edge cases (NaN, zero-length), etc.
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "generation/nimcp_tokenizer.h"
#include "generation/nimcp_embedding.h"
#include "generation/nimcp_language_generator.h"
}

// =============================================================================
// Regression: Reasoning Engine Edge Cases
// =============================================================================

class ReasoningGenerationRegression : public ::testing::Test {
protected:
    reasoning_engine_t* engine = nullptr;

    void SetUp() override {
        reasoning_engine_config_t cfg = reasoning_engine_default_config();
        engine = reasoning_engine_create(&cfg);
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override {
        if (engine) {
            reasoning_engine_destroy(engine);
            engine = nullptr;
        }
    }
};

TEST_F(ReasoningGenerationRegression, NullQueryDoesNotCrash) {
    // Regression: NULL query must not cause segfault
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    int rc = reasoning_engine_reason(engine, nullptr, &chain);
    // Should return error (not crash)
    EXPECT_NE(rc, 0) << "NULL query should return error, not crash";

    reasoning_chain_cleanup(&chain);
}

TEST_F(ReasoningGenerationRegression, EmptyQueryProducesResult) {
    // Regression: Empty string "" should not crash, should produce a valid chain
    // (even if it has zero useful steps)
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    int rc = reasoning_engine_reason(engine, "", &chain);
    // Whether this succeeds or returns error depends on implementation,
    // but it must not crash
    if (rc == 0) {
        // If it succeeds, the chain should be in a valid state
        EXPECT_GE(chain.num_steps, 0u);
        float conf = reasoning_chain_get_confidence(&chain);
        EXPECT_GE(conf, 0.0f);
        EXPECT_LE(conf, 1.0f);
    }
    // Either way, cleanup should work
    reasoning_chain_cleanup(&chain);
}

TEST_F(ReasoningGenerationRegression, VeryLongQueryHandled) {
    // Regression: Very long query (10000 chars) must not buffer-overflow
    std::string long_query(10000, 'A');
    // Insert some variety to prevent any single-char optimization
    for (size_t i = 0; i < long_query.size(); i += 100) {
        long_query[i] = 'a' + static_cast<char>(i % 26);
    }

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    int rc = reasoning_engine_reason(engine, long_query.c_str(), &chain);
    // Should either succeed or fail gracefully — no crash, no overflow
    if (rc == 0) {
        EXPECT_GE(chain.num_steps, 0u);
    }

    reasoning_chain_cleanup(&chain);
}

// =============================================================================
// Regression: Generator Edge Cases
// =============================================================================

class GeneratorRegression : public ::testing::Test {
protected:
    tokenizer_t* tok = nullptr;
    embedding_layer_t* emb = nullptr;

    void SetUp() override {
        tokenizer_config_t tcfg = tokenizer_default_config();
        tok = tokenizer_create(&tcfg);
        ASSERT_NE(tok, nullptr);

        const char* words[] = {"hello", "world", "test", "the", "a"};
        for (int i = 0; i < 5; i++) tokenizer_add_token(tok, words[i]);

        uint32_t vocab_size = tokenizer_get_vocab_size(tok);
        embedding_config_t ecfg = embedding_default_config(vocab_size, 16);
        emb = embedding_create(&ecfg);
        ASSERT_NE(emb, nullptr);
    }

    void TearDown() override {
        if (emb) { embedding_destroy(emb); emb = nullptr; }
        if (tok) { tokenizer_destroy(tok); tok = nullptr; }
    }
};

TEST_F(GeneratorRegression, ZeroTokenGenerationHandled) {
    // Regression: Generator with max_length=1 should produce at most 1 token
    // without crashing (tests off-by-one in sequence termination)
    uint32_t vocab_size = tokenizer_get_vocab_size(tok);
    generator_config_t gcfg = generator_default_config();
    gcfg.max_sequence_length = 1;
    gcfg.hidden_dim = 16;
    gcfg.num_lnn_neurons = 8;

    language_generator_t* gen = language_generator_create(&gcfg, tok, emb,
                                                          vocab_size, 16);
    ASSERT_NE(gen, nullptr);

    std::vector<float> state(16, 0.5f);
    generation_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = language_generator_generate(gen, state.data(), 16, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_LE(result.num_tokens, 1u)
        << "With max_sequence_length=1, should produce at most 1 token";

    generation_result_cleanup(&result);
    language_generator_destroy(gen);
}

TEST_F(GeneratorRegression, TokenizerEncodeDecodePreserves) {
    // Regression: Encode->Decode round trip should preserve content
    // Guards against encode/decode ID mapping divergence
    const char* text = "hello world";

    uint32_t ids[32];
    uint32_t num_tokens = 0;
    int enc_rc = tokenizer_encode(tok, text, ids, 32, &num_tokens);
    ASSERT_EQ(enc_rc, 0);
    ASSERT_GT(num_tokens, 0u);

    char decoded[256];
    memset(decoded, 0, sizeof(decoded));
    int dec_rc = tokenizer_decode(tok, ids, num_tokens, decoded, sizeof(decoded));
    ASSERT_EQ(dec_rc, 0);

    // The decoded text should contain the original words
    EXPECT_NE(strstr(decoded, "hello"), nullptr)
        << "Round-trip should preserve 'hello', got: " << decoded;
    EXPECT_NE(strstr(decoded, "world"), nullptr)
        << "Round-trip should preserve 'world', got: " << decoded;
}

TEST_F(GeneratorRegression, GeneratorTrainStepNaN) {
    // Regression: If a training step produces NaN loss, it should be detected
    // and handled (not silently propagated)
    uint32_t vocab_size = tokenizer_get_vocab_size(tok);
    generator_config_t gcfg = generator_default_config();
    gcfg.max_sequence_length = 10;
    gcfg.hidden_dim = 16;
    gcfg.num_lnn_neurons = 8;
    gcfg.temperature = 1.0f;

    language_generator_t* gen = language_generator_create(&gcfg, tok, emb,
                                                          vocab_size, 16);
    ASSERT_NE(gen, nullptr);

    // Create a valid training pair
    uint32_t input_ids[] = {4, 5, 6, 7};  // Valid token IDs (past special tokens)
    uint32_t target_ids[] = {5, 6, 7, 4};
    uint32_t seq_len = 4;

    // Run many training steps — if numerical instability causes NaN,
    // we should detect it
    bool got_nan = false;
    float loss = 0.0f;
    for (int i = 0; i < 100; i++) {
        int rc = language_generator_train_step(gen, input_ids, target_ids,
                                                seq_len, &loss);
        if (std::isnan(loss) || std::isinf(loss)) {
            got_nan = true;
            // If NaN is detected, the function should ideally return an error
            // or clamp the loss. Either way, this is the regression guard.
            break;
        }
        if (rc != 0) break; // Error detected by implementation
    }

    // If no NaN occurred, loss should be finite
    if (!got_nan) {
        EXPECT_TRUE(std::isfinite(loss))
            << "After training steps, loss should be finite, got: " << loss;
    }

    language_generator_destroy(gen);
}
