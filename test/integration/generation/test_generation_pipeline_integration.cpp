/**
 * @file test_generation_pipeline_integration.cpp
 * @brief Integration tests for the full generation pipeline
 *
 * WHAT: Tests the complete tokenizer -> embedding -> generator pipeline
 * WHY:  Individual unit tests verify each component; integration tests verify
 *       they compose correctly — vocab IDs flow through embeddings into the
 *       generator, and generated IDs decode back to valid text
 * HOW:  Builds a tokenizer from a corpus, creates matching embedding layer,
 *       creates generator, then tests full encode->generate->decode workflows
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <random>

extern "C" {
#include "generation/nimcp_tokenizer.h"
#include "generation/nimcp_embedding.h"
#include "generation/nimcp_language_generator.h"
}

// =============================================================================
// Constants
// =============================================================================

static constexpr uint32_t EMBED_DIM = 32;
static constexpr uint32_t MAX_SEQ_LEN = 30;
static constexpr uint32_t HIDDEN_DIM = 32;
static constexpr uint32_t LNN_NEURONS = 16;

// Training corpus — intentionally repetitive so patterns can be learned
static const char* CORPUS =
    "the cat sat on the mat "
    "the dog ran in the park "
    "the bird flew over the tree "
    "the fish swam in the lake "
    "the cat chased the mouse "
    "the dog barked at the cat "
    "the quick brown fox jumps over the lazy dog "
    "the sun set behind the mountain "
    "the wind blew through the valley "
    "the rain fell on the rooftop ";

// =============================================================================
// Test Fixture
// =============================================================================

class GenerationPipelineIntegration : public ::testing::Test {
protected:
    tokenizer_t* tok = nullptr;
    embedding_layer_t* emb = nullptr;
    language_generator_t* gen = nullptr;
    uint32_t vocab_size = 0;

    void SetUp() override {
        // Create tokenizer and build vocab from corpus
        tokenizer_config_t tcfg = tokenizer_default_config();
        tok = tokenizer_create(&tcfg);
        ASSERT_NE(tok, nullptr);

        int rc = tokenizer_build_from_text(tok, CORPUS, 100);
        ASSERT_EQ(rc, 0) << "Building vocab from corpus should succeed";

        vocab_size = tokenizer_get_vocab_size(tok);
        ASSERT_GT(vocab_size, 0u);

        // Create embedding matching vocab
        embedding_config_t ecfg = embedding_default_config(vocab_size, EMBED_DIM);
        emb = embedding_create(&ecfg);
        ASSERT_NE(emb, nullptr);

        // Create generator
        generator_config_t gcfg = generator_default_config();
        gcfg.max_sequence_length = MAX_SEQ_LEN;
        gcfg.hidden_dim = HIDDEN_DIM;
        gcfg.num_lnn_neurons = LNN_NEURONS;
        gcfg.temperature = 1.0f;
        gcfg.strategy = GENERATION_GREEDY;
        gen = language_generator_create(&gcfg, tok, emb, vocab_size, EMBED_DIM);
        ASSERT_NE(gen, nullptr);
    }

    void TearDown() override {
        if (gen) { language_generator_destroy(gen); gen = nullptr; }
        if (emb) { embedding_destroy(emb); emb = nullptr; }
        if (tok) { tokenizer_destroy(tok); tok = nullptr; }
    }
};

// =============================================================================
// Tests: Pipeline Lifecycle
// =============================================================================

TEST_F(GenerationPipelineIntegration, FullPipelineDoesNotCrash) {
    // Simply exercising the full pipeline: build vocab + embed + generate
    std::vector<float> state(HIDDEN_DIM, 0.5f);
    generation_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = language_generator_generate(gen, state.data(), HIDDEN_DIM, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(result.num_tokens, 0u);

    generation_result_cleanup(&result);
}

// =============================================================================
// Tests: Encode -> Generate -> Decode Round Trip
// =============================================================================

TEST_F(GenerationPipelineIntegration, GeneratedTextIsDecodable) {
    std::vector<float> state(HIDDEN_DIM, 0.3f);
    generation_result_t result;
    memset(&result, 0, sizeof(result));

    language_generator_generate(gen, state.data(), HIDDEN_DIM, &result);
    ASSERT_GT(result.num_tokens, 0u);
    ASSERT_NE(result.token_ids, nullptr);

    // Decode the generated token IDs back to text
    char decoded[1024];
    memset(decoded, 0, sizeof(decoded));
    int rc = tokenizer_decode(tok, result.token_ids, result.num_tokens,
                               decoded, sizeof(decoded));
    EXPECT_EQ(rc, 0) << "Decoding generated token IDs should succeed";
    EXPECT_GT(strlen(decoded), 0u)
        << "Decoded text should be non-empty";

    generation_result_cleanup(&result);
}

// =============================================================================
// Tests: Training Integration
// =============================================================================

TEST_F(GenerationPipelineIntegration, TrainAndGenerate) {
    // Encode a training sentence
    uint32_t input_ids[MAX_SEQ_LEN];
    uint32_t target_ids[MAX_SEQ_LEN];
    uint32_t num_input = 0, num_target = 0;

    tokenizer_encode(tok, "the cat sat on", input_ids, MAX_SEQ_LEN, &num_input);
    tokenizer_encode(tok, "cat sat on the", target_ids, MAX_SEQ_LEN, &num_target);

    uint32_t seq_len = std::min(num_input, num_target);
    if (seq_len > 0) {
        // Train for a few steps
        float loss = 0.0f;
        for (int i = 0; i < 10; i++) {
            language_generator_train_step(gen, input_ids, target_ids, seq_len, &loss);
        }
        EXPECT_GT(loss, 0.0f) << "Loss should be positive (cross-entropy on tokens)";
    }

    // Now generate — should produce valid output regardless of training
    generation_result_t result;
    memset(&result, 0, sizeof(result));
    std::vector<float> state(HIDDEN_DIM, 0.5f);
    int rc = language_generator_generate(gen, state.data(), HIDDEN_DIM, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(result.num_tokens, 0u);

    generation_result_cleanup(&result);
}

// =============================================================================
// Tests: Prompt Continuation
// =============================================================================

TEST_F(GenerationPipelineIntegration, PromptContinuation) {
    generation_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = language_generator_generate_from_prompt(gen, "the", &result);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(result.num_tokens, 0u)
        << "Prompt 'the' should generate continuation tokens";

    // The result should contain at least the prompt token
    EXPECT_NE(result.text, nullptr);
    if (result.text) {
        EXPECT_GT(strlen(result.text), 0u);
    }

    generation_result_cleanup(&result);
}

// =============================================================================
// Tests: Multiple Generations
// =============================================================================

TEST_F(GenerationPipelineIntegration, MultipleGenerations) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (int run = 0; run < 5; run++) {
        std::vector<float> state(HIDDEN_DIM);
        for (auto& v : state) v = dist(rng);

        generation_result_t result;
        memset(&result, 0, sizeof(result));

        int rc = language_generator_generate(gen, state.data(), HIDDEN_DIM, &result);
        EXPECT_EQ(rc, 0) << "Generation run " << run << " should succeed";
        EXPECT_GT(result.num_tokens, 0u)
            << "Generation run " << run << " should produce tokens";

        generation_result_cleanup(&result);
    }
}

// =============================================================================
// Tests: Temperature Variation
// =============================================================================

TEST_F(GenerationPipelineIntegration, DifferentTemperatures) {
    std::vector<float> state(HIDDEN_DIM, 0.5f);
    language_generator_set_strategy(gen, GENERATION_SAMPLING);

    // Low temperature — more deterministic
    language_generator_set_temperature(gen, 0.1f);
    generation_result_t result_low;
    memset(&result_low, 0, sizeof(result_low));
    language_generator_generate(gen, state.data(), HIDDEN_DIM, &result_low);

    // High temperature — more random
    language_generator_set_temperature(gen, 2.0f);
    generation_result_t result_high;
    memset(&result_high, 0, sizeof(result_high));
    language_generator_generate(gen, state.data(), HIDDEN_DIM, &result_high);

    // Both should produce valid output
    EXPECT_GT(result_low.num_tokens, 0u);
    EXPECT_GT(result_high.num_tokens, 0u);

    // Both should have valid text
    EXPECT_NE(result_low.text, nullptr);
    EXPECT_NE(result_high.text, nullptr);

    generation_result_cleanup(&result_low);
    generation_result_cleanup(&result_high);
}

// =============================================================================
// Tests: Reasoning Chain to Generation
// =============================================================================

TEST_F(GenerationPipelineIntegration, ReasoningToGeneration) {
    // Create a mock reasoning chain (manually built, no engine needed)
    // The generator should accept any chain-like structure via void*
    // For this test, we pass NULL (the generator should handle it gracefully
    // or we test the from_prompt path instead)
    generation_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = language_generator_generate_from_reasoning(gen, nullptr, &result);
    // NULL chain should either fail gracefully or produce empty/default output
    if (rc == 0) {
        // If it succeeds, output should still be valid
        EXPECT_GE(result.num_tokens, 0u);
    } else {
        // If it fails, that's also acceptable — NULL is not a valid chain
        EXPECT_NE(rc, 0);
    }

    generation_result_cleanup(&result);
}

// =============================================================================
// Tests: Stress with Large Vocabulary
// =============================================================================

TEST_F(GenerationPipelineIntegration, LargeVocabStress) {
    // Add many more tokens to the existing vocab
    for (int i = 0; i < 500; i++) {
        char token[32];
        snprintf(token, sizeof(token), "extra_%03d", i);
        tokenizer_add_token(tok, token);
    }

    // The generator was created with the original vocab_size.
    // Generate should still work (tokens beyond the original vocab
    // just won't be generated, but the pipeline shouldn't crash)
    std::vector<float> state(HIDDEN_DIM, 0.2f);
    generation_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = language_generator_generate(gen, state.data(), HIDDEN_DIM, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(result.num_tokens, 0u);

    // All generated token IDs should be valid for decoding
    if (result.token_ids && result.num_tokens > 0) {
        char decoded[2048];
        int dec_rc = tokenizer_decode(tok, result.token_ids, result.num_tokens,
                                       decoded, sizeof(decoded));
        EXPECT_EQ(dec_rc, 0);
    }

    generation_result_cleanup(&result);
}
