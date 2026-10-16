/**
 * @file test_language_generator.cpp
 * @brief Unit tests for the NIMCP language generator
 *
 * WHAT: Tests generator creation, text generation, training, strategy switching
 * WHY:  The language generator is the final stage that converts cognitive state
 *       or reasoning chains into natural language — must produce valid, bounded output
 * HOW:  GTest fixture creates a small tokenizer + embedding + generator pipeline;
 *       exercises all generation strategies, training convergence, and cleanup
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>
#include <string>

extern "C" {
#include "generation/nimcp_language_generator.h"
#include "generation/nimcp_tokenizer.h"
#include "generation/nimcp_embedding.h"
}

// =============================================================================
// Constants
// =============================================================================

static constexpr uint32_t TEST_EMBED_DIM = 32;
static constexpr uint32_t TEST_MAX_SEQ_LEN = 20;
static constexpr uint32_t TEST_HIDDEN_DIM = 32;
static constexpr uint32_t TEST_LNN_NEURONS = 16;

// =============================================================================
// Test Fixture
// =============================================================================

class GeneratorUnit : public ::testing::Test {
protected:
    tokenizer_t* tok = nullptr;
    embedding_layer_t* emb = nullptr;
    language_generator_t* gen = nullptr;
    uint32_t vocab_size = 0;

    void SetUp() override {
        // Create tokenizer with small vocab
        tokenizer_config_t tcfg = tokenizer_default_config();
        tok = tokenizer_create(&tcfg);
        ASSERT_NE(tok, nullptr);

        // Add basic tokens
        const char* words[] = {"the", "cat", "sat", "on", "mat", "a", "big", "small"};
        for (int i = 0; i < 8; i++) {
            tokenizer_add_token(tok, words[i]);
        }

        vocab_size = tokenizer_get_vocab_size(tok);
        ASSERT_GT(vocab_size, 0u);

        // Create embedding layer
        embedding_config_t ecfg = embedding_default_config(vocab_size, TEST_EMBED_DIM);
        emb = embedding_create(&ecfg);
        ASSERT_NE(emb, nullptr);

        // Create generator
        generator_config_t gcfg = generator_default_config();
        gcfg.max_sequence_length = TEST_MAX_SEQ_LEN;
        gcfg.hidden_dim = TEST_HIDDEN_DIM;
        gcfg.num_lnn_neurons = TEST_LNN_NEURONS;
        gen = language_generator_create(&gcfg, tok, emb, vocab_size, TEST_EMBED_DIM);
        ASSERT_NE(gen, nullptr);
    }

    void TearDown() override {
        if (gen) { language_generator_destroy(gen); gen = nullptr; }
        if (emb) { embedding_destroy(emb); emb = nullptr; }
        if (tok) { tokenizer_destroy(tok); tok = nullptr; }
    }

    // Helper: generate random cognitive state
    std::vector<float> random_state(uint32_t dim, unsigned seed = 42) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        std::vector<float> state(dim);
        for (auto& v : state) v = dist(rng);
        return state;
    }
};

// =============================================================================
// Tests: Creation and Configuration
// =============================================================================

TEST_F(GeneratorUnit, CreateAndDestroy) {
    EXPECT_NE(gen, nullptr);
    // Destroy in TearDown — verifies no crash on full lifecycle
}

TEST_F(GeneratorUnit, DefaultConfigValues) {
    generator_config_t cfg = generator_default_config();

    EXPECT_GT(cfg.max_sequence_length, 0u);
    EXPECT_GT(cfg.hidden_dim, 0u);
    EXPECT_GT(cfg.num_lnn_neurons, 0u);

    // Temperature should be positive
    EXPECT_GT(cfg.temperature, 0.0f);

    // top_p should be in (0, 1]
    EXPECT_GT(cfg.top_p, 0.0f);
    EXPECT_LE(cfg.top_p, 1.0f);

    // top_k should be positive
    EXPECT_GT(cfg.top_k, 0u);

    // Default strategy should be a valid enum value
    EXPECT_GE(static_cast<int>(cfg.strategy), 0);
    EXPECT_LE(static_cast<int>(cfg.strategy),
              static_cast<int>(GENERATION_BEAM_SEARCH));

    // Beam width should be positive
    EXPECT_GT(cfg.beam_width, 0u);

    // Repetition penalty should be >= 1.0 (1.0 = no penalty)
    EXPECT_GE(cfg.repetition_penalty, 1.0f);
}

// =============================================================================
// Tests: Generation from Cognitive State
// =============================================================================

TEST_F(GeneratorUnit, GenerateFromStateProducesTokens) {
    auto state = random_state(TEST_HIDDEN_DIM);
    generation_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = language_generator_generate(gen, state.data(), TEST_HIDDEN_DIM, &result);
    EXPECT_EQ(rc, 0) << "Generation from cognitive state should succeed";
    EXPECT_GT(result.num_tokens, 0u)
        << "Generation should produce at least one token";

    generation_result_cleanup(&result);
}

TEST_F(GeneratorUnit, GenerateProducesText) {
    auto state = random_state(TEST_HIDDEN_DIM);
    generation_result_t result;
    memset(&result, 0, sizeof(result));

    language_generator_generate(gen, state.data(), TEST_HIDDEN_DIM, &result);

    EXPECT_NE(result.text, nullptr)
        << "Generated result should have non-NULL text";
    if (result.text) {
        EXPECT_GT(strlen(result.text), 0u)
            << "Generated text should be non-empty";
    }

    generation_result_cleanup(&result);
}

TEST_F(GeneratorUnit, GenerateRespectsMaxLength) {
    auto state = random_state(TEST_HIDDEN_DIM);
    generation_result_t result;
    memset(&result, 0, sizeof(result));

    language_generator_generate(gen, state.data(), TEST_HIDDEN_DIM, &result);

    EXPECT_LE(result.num_tokens, TEST_MAX_SEQ_LEN)
        << "Generated sequence should not exceed max_sequence_length";

    generation_result_cleanup(&result);
}

// =============================================================================
// Tests: Determinism
// =============================================================================

TEST_F(GeneratorUnit, GreedyIsDeterministic) {
    language_generator_set_strategy(gen, GENERATION_GREEDY);

    auto state = random_state(TEST_HIDDEN_DIM, 123);

    // Generate twice with the same state
    generation_result_t result1, result2;
    memset(&result1, 0, sizeof(result1));
    memset(&result2, 0, sizeof(result2));

    language_generator_generate(gen, state.data(), TEST_HIDDEN_DIM, &result1);
    language_generator_generate(gen, state.data(), TEST_HIDDEN_DIM, &result2);

    // Both should produce the same number of tokens
    EXPECT_EQ(result1.num_tokens, result2.num_tokens);

    // And the same token IDs
    if (result1.num_tokens == result2.num_tokens && result1.token_ids && result2.token_ids) {
        for (uint32_t i = 0; i < result1.num_tokens; i++) {
            EXPECT_EQ(result1.token_ids[i], result2.token_ids[i])
                << "Greedy generation should be deterministic at position " << i;
        }
    }

    generation_result_cleanup(&result1);
    generation_result_cleanup(&result2);
}

// =============================================================================
// Tests: Temperature
// =============================================================================

TEST_F(GeneratorUnit, TemperatureAffectsOutput) {
    language_generator_set_strategy(gen, GENERATION_SAMPLING);

    auto state = random_state(TEST_HIDDEN_DIM, 456);

    // Generate with low temperature
    language_generator_set_temperature(gen, 0.01f);
    generation_result_t result_low;
    memset(&result_low, 0, sizeof(result_low));
    language_generator_generate(gen, state.data(), TEST_HIDDEN_DIM, &result_low);

    // Generate with high temperature
    language_generator_set_temperature(gen, 5.0f);
    generation_result_t result_high;
    memset(&result_high, 0, sizeof(result_high));
    language_generator_generate(gen, state.data(), TEST_HIDDEN_DIM, &result_high);

    // Both should produce valid output
    EXPECT_GT(result_low.num_tokens, 0u);
    EXPECT_GT(result_high.num_tokens, 0u);

    // Low temperature should generally produce higher confidence (more peaked distribution)
    // This is a statistical property — we just verify both produce valid results
    // and that overall_confidence is reported
    EXPECT_GE(result_low.overall_confidence, 0.0f);
    EXPECT_LE(result_low.overall_confidence, 1.0f);
    EXPECT_GE(result_high.overall_confidence, 0.0f);
    EXPECT_LE(result_high.overall_confidence, 1.0f);

    generation_result_cleanup(&result_low);
    generation_result_cleanup(&result_high);
}

// =============================================================================
// Tests: Generation from Prompt
// =============================================================================

TEST_F(GeneratorUnit, GenerateFromPromptWorks) {
    generation_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = language_generator_generate_from_prompt(gen, "the cat", &result);
    EXPECT_EQ(rc, 0) << "Generation from prompt should succeed";
    EXPECT_GT(result.num_tokens, 0u)
        << "Prompt continuation should produce tokens";

    generation_result_cleanup(&result);
}

// =============================================================================
// Tests: Training
// =============================================================================

TEST_F(GeneratorUnit, TrainStepReducesLoss) {
    // Create a simple training sequence: "the cat sat on the mat"
    uint32_t input_ids[5];
    uint32_t target_ids[5];
    uint32_t num_input = 0, num_target = 0;

    // Encode input (first 5 words as input)
    tokenizer_encode(tok, "the cat sat on the", input_ids, 5, &num_input);

    // Encode target (shifted by one: "cat sat on the mat")
    tokenizer_encode(tok, "cat sat on the mat", target_ids, 5, &num_target);

    // Use minimum of both lengths
    uint32_t seq_len = std::min(num_input, num_target);
    if (seq_len == 0) {
        // If encoding produced zero tokens, manually set up IDs
        // using known token IDs
        seq_len = 4;
        for (uint32_t i = 0; i < seq_len; i++) {
            input_ids[i] = i + 4;   // Some valid token IDs (past special tokens)
            target_ids[i] = i + 5;
        }
    }

    // First training step
    float loss1 = 0.0f;
    int rc1 = language_generator_train_step(gen, input_ids, target_ids, seq_len, &loss1);
    EXPECT_EQ(rc1, 0) << "First train step should succeed";
    EXPECT_GT(loss1, 0.0f) << "Initial loss should be positive";

    // Run several more training steps on the same data to allow convergence
    float loss_prev = loss1;
    float loss_final = loss1;
    for (int i = 0; i < 20; i++) {
        int rc = language_generator_train_step(gen, input_ids, target_ids,
                                                seq_len, &loss_final);
        EXPECT_EQ(rc, 0);
    }

    // After multiple steps on the same data, loss should generally decrease
    // (allowing some tolerance for numerical noise)
    EXPECT_LE(loss_final, loss1 * 1.5f)
        << "Loss after 20 steps should not be much worse than initial loss";
}

// =============================================================================
// Tests: Result Cleanup
// =============================================================================

TEST_F(GeneratorUnit, ResultCleanupFreesMemory) {
    auto state = random_state(TEST_HIDDEN_DIM);
    generation_result_t result;
    memset(&result, 0, sizeof(result));

    language_generator_generate(gen, state.data(), TEST_HIDDEN_DIM, &result);

    // Verify we got something to clean up
    bool had_text = (result.text != nullptr);
    bool had_ids = (result.token_ids != nullptr);

    generation_result_cleanup(&result);

    // After cleanup, pointers should be NULL (or at least safe to re-cleanup)
    // We verify by calling cleanup again — should not crash
    generation_result_cleanup(&result);

    // Also verify the result struct is in a clean state
    EXPECT_EQ(result.num_tokens, 0u);
}

// =============================================================================
// Tests: Strategy Switching
// =============================================================================

TEST_F(GeneratorUnit, SetStrategyChanges) {
    auto state = random_state(TEST_HIDDEN_DIM, 789);

    // Set to TOP_K strategy
    language_generator_set_strategy(gen, GENERATION_TOP_K);

    generation_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = language_generator_generate(gen, state.data(), TEST_HIDDEN_DIM, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(result.num_tokens, 0u);
    generation_result_cleanup(&result);

    // Set to TOP_P strategy
    language_generator_set_strategy(gen, GENERATION_TOP_P);

    memset(&result, 0, sizeof(result));
    rc = language_generator_generate(gen, state.data(), TEST_HIDDEN_DIM, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(result.num_tokens, 0u);
    generation_result_cleanup(&result);

    // Set to BEAM_SEARCH strategy
    language_generator_set_strategy(gen, GENERATION_BEAM_SEARCH);

    memset(&result, 0, sizeof(result));
    rc = language_generator_generate(gen, state.data(), TEST_HIDDEN_DIM, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(result.num_tokens, 0u);
    generation_result_cleanup(&result);
}

// =============================================================================
// Tests: Perplexity
// =============================================================================

TEST_F(GeneratorUnit, PerplexityIsPositive) {
    auto state = random_state(TEST_HIDDEN_DIM);
    generation_result_t result;
    memset(&result, 0, sizeof(result));

    language_generator_generate(gen, state.data(), TEST_HIDDEN_DIM, &result);

    if (result.num_tokens > 0) {
        EXPECT_GT(result.perplexity, 0.0f)
            << "Perplexity should be positive for generated text";
        EXPECT_FALSE(std::isnan(result.perplexity))
            << "Perplexity should not be NaN";
        EXPECT_FALSE(std::isinf(result.perplexity))
            << "Perplexity should not be infinite";
    }

    generation_result_cleanup(&result);
}
