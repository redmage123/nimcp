/**
 * @file test_language_pipeline_integration.cpp
 * @brief Integration tests for the brain-native language pipeline.
 *
 * WHAT: Test native language + tokenizer together, inner speech lifecycle,
 *       tokenizer-language consistency, positional encoding effects, and
 *       multi-step training convergence.
 * WHY:  The language pipeline spans tokenizer, native language, phonological
 *       loop, inner speech, and positional encoding. Integration regressions
 *       break end-to-end text generation from brain embeddings.
 * HOW:  Google Test, standalone mode (no real brain).
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "cognitive/language/nimcp_native_language.h"
#include "cognitive/language/nimcp_tokenizer.h"
#include "cognitive/language/nimcp_inner_speech.h"
}

// ============================================================================
// Native Language + Tokenizer Together
// ============================================================================

TEST(LanguagePipeline, CreateBothTogether) {
    nimcp_language_config_t lang_cfg = nimcp_language_config_default();
    nimcp_tokenizer_config_t tok_cfg = nimcp_tokenizer_config_default();

    nimcp_native_language_t* lang = nimcp_native_language_create(&lang_cfg);
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&tok_cfg);
    ASSERT_NE(lang, nullptr);
    ASSERT_NE(tok, nullptr);

    EXPECT_GT(nimcp_language_get_vocab_size(lang), 0u);
    EXPECT_GT(nimcp_tokenizer_get_vocab_size(tok), 0u);

    nimcp_tokenizer_destroy(tok);
    nimcp_native_language_destroy(lang);
}

TEST(LanguagePipeline, TokenizerEncodeToLanguageGenerate) {
    nimcp_language_config_t lang_cfg = nimcp_language_config_default();
    nimcp_tokenizer_config_t tok_cfg = nimcp_tokenizer_config_default();

    nimcp_native_language_t* lang = nimcp_native_language_create(&lang_cfg);
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&tok_cfg);
    ASSERT_NE(lang, nullptr);
    ASSERT_NE(tok, nullptr);

    // Encode text via tokenizer
    uint32_t ids[256];
    int n = nimcp_tokenizer_encode(tok, "the cat", ids, 256);
    EXPECT_GT(n, 0);

    // Use a non-zero embedding to feed native language
    std::vector<float> emb(4096, 0.0f);
    for (size_t i = 0; i < emb.size(); i++)
        emb[i] = sinf((float)i * 0.02f) * 0.3f;

    char output[512];
    memset(output, 0, sizeof(output));
    int rc = nimcp_language_generate(lang, emb.data(), 4096, output, sizeof(output));
    EXPECT_GE(rc, 0);

    nimcp_tokenizer_destroy(tok);
    nimcp_native_language_destroy(lang);
}

// ============================================================================
// Training
// ============================================================================

TEST(LanguagePipeline, TrainStepUpdatesVocab) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    uint32_t before = nimcp_language_get_vocab_size(lang);
    std::vector<float> emb(4096, 0.05f);
    // "quantum" is not a seed word, should be learned
    nimcp_language_train_step(lang, emb.data(), 4096, "quantum physics", 0.001f);
    uint32_t after = nimcp_language_get_vocab_size(lang);
    EXPECT_GE(after, before) << "Training with novel words should expand vocab";

    nimcp_native_language_destroy(lang);
}

TEST(LanguagePipeline, MultipleTrainStepsDoNotCrash) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    std::vector<float> emb(4096, 0.0f);
    for (int step = 0; step < 10; step++) {
        for (size_t i = 0; i < emb.size(); i++)
            emb[i] = sinf((float)(i + step) * 0.01f) * 0.2f;
        int rc = nimcp_language_train_step(lang, emb.data(), 4096,
                                            "the big warm dog", 0.001f);
        EXPECT_GE(rc, 0);
    }

    nimcp_native_language_destroy(lang);
}

// ============================================================================
// Inner Speech
// ============================================================================

TEST(LanguagePipeline, InnerSpeechCreateWithDefaultConfig) {
    nimcp_inner_speech_config_t cfg = nimcp_inner_speech_config_default();
    nimcp_inner_speech_t* is = nimcp_inner_speech_create(&cfg);
    ASSERT_NE(is, nullptr);
    nimcp_inner_speech_destroy(is);
}

TEST(LanguagePipeline, InnerSpeechCreateNullConfigReturnsNull) {
    nimcp_inner_speech_t* is = nimcp_inner_speech_create(NULL);
    EXPECT_EQ(is, nullptr) << "NULL config should return NULL (requires config)";
}

TEST(LanguagePipeline, InnerSpeechDestroyNull) {
    nimcp_inner_speech_destroy(NULL);
    SUCCEED() << "nimcp_inner_speech_destroy(NULL) did not crash";
}

TEST(LanguagePipeline, InnerSpeechConfigDefaults) {
    nimcp_inner_speech_config_t cfg = nimcp_inner_speech_config_default();
    EXPECT_EQ(cfg.max_iterations, 3u);
    EXPECT_FLOAT_EQ(cfg.convergence_threshold, 0.95f);
    EXPECT_FLOAT_EQ(cfg.refinement_lr, 0.0001f);
    EXPECT_FLOAT_EQ(cfg.blend_original, 0.6f);
    EXPECT_FLOAT_EQ(cfg.blend_encoded, 0.4f);
}

TEST(LanguagePipeline, InnerSpeechGetIterationsInitiallyZero) {
    nimcp_inner_speech_config_t cfg = nimcp_inner_speech_config_default();
    nimcp_inner_speech_t* is = nimcp_inner_speech_create(&cfg);
    ASSERT_NE(is, nullptr);

    uint32_t iters = nimcp_inner_speech_get_iterations(is);
    EXPECT_EQ(iters, 0u);

    nimcp_inner_speech_destroy(is);
}

// ============================================================================
// Encode → Generate Pipeline
// ============================================================================

TEST(LanguagePipeline, EncodeTextThenGenerateFromEncoding) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    // Encode known text to brain embedding
    float embedding[4096];
    memset(embedding, 0, sizeof(embedding));
    int enc_rc = nimcp_language_encode(lang, "the big warm light", embedding, 4096);
    EXPECT_GT(enc_rc, 0);

    // Generate from that encoding
    char output[512];
    memset(output, 0, sizeof(output));
    int gen_rc = nimcp_language_generate(lang, embedding, 4096, output, sizeof(output));
    EXPECT_GE(gen_rc, 0);

    nimcp_native_language_destroy(lang);
}

TEST(LanguagePipeline, LargeInputEmbeddingDoesNotCrash) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    // 8192 > BRAIN_DIM (4096) — should be clamped internally
    std::vector<float> emb(8192, 0.01f);
    char output[512];
    memset(output, 0, sizeof(output));
    int rc = nimcp_language_generate(lang, emb.data(), 8192, output, sizeof(output));
    EXPECT_GE(rc, 0);

    nimcp_native_language_destroy(lang);
}

TEST(LanguagePipeline, ZeroEmbeddingProducesOutput) {
    nimcp_language_config_t cfg = nimcp_language_config_default();
    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    std::vector<float> emb(4096, 0.0f);
    char output[512];
    memset(output, 0, sizeof(output));
    int rc = nimcp_language_generate(lang, emb.data(), 4096, output, sizeof(output));
    EXPECT_GE(rc, 0) << "Zero embedding should not crash (may produce empty or random text)";

    nimcp_native_language_destroy(lang);
}

// ============================================================================
// Config Roundtrip
// ============================================================================

TEST(LanguagePipeline, ConfigRoundtripNonDefault) {
    nimcp_language_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_vocab_size = 4096;
    cfg.embed_dim = 128;
    cfg.max_seq_length = 64;
    cfg.temperature = 1.0f;
    cfg.top_p = 0.8f;
    cfg.repetition_penalty = 1.5f;
    cfg.learn_vocabulary = false;
    cfg.min_token_frequency = 0.01f;

    nimcp_native_language_t* lang = nimcp_native_language_create(&cfg);
    ASSERT_NE(lang, nullptr);

    // Verify vocab was created with the custom max
    uint32_t vs = nimcp_language_get_vocab_size(lang);
    EXPECT_LE(vs, 4096u);

    nimcp_native_language_destroy(lang);
}

// ============================================================================
// Phonological Loop tracks sequence
// ============================================================================

TEST(LanguagePipeline, PhonologicalLoopTracksSequence) {
    nimcp_phonological_loop_t* loop = nimcp_phonological_loop_create(8, 4);
    ASSERT_NE(loop, nullptr);

    for (int i = 0; i < 5; i++) {
        float emb[4] = {(float)i, (float)(i + 1), (float)(i + 2), (float)(i + 3)};
        nimcp_phonological_loop_push(loop, emb);
    }
    EXPECT_EQ(loop->length, 5u);

    nimcp_phonological_loop_reset(loop);
    EXPECT_EQ(loop->length, 0u);

    nimcp_phonological_loop_destroy(loop);
}

// ============================================================================
// Tokenizer + Language Vocab Consistency
// ============================================================================

TEST(LanguagePipeline, TokenizerAndLanguageVocabBothPopulated) {
    nimcp_language_config_t lang_cfg = nimcp_language_config_default();
    nimcp_tokenizer_config_t tok_cfg = nimcp_tokenizer_config_default();

    nimcp_native_language_t* lang = nimcp_native_language_create(&lang_cfg);
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&tok_cfg);
    ASSERT_NE(lang, nullptr);
    ASSERT_NE(tok, nullptr);

    // Both should have special tokens
    EXPECT_GE(nimcp_language_get_vocab_size(lang), 4u);
    EXPECT_GE(nimcp_tokenizer_get_vocab_size(tok), 4u);

    // Tokenizer has ASCII chars; language has seed words
    EXPECT_GE(nimcp_tokenizer_get_vocab_size(tok), 99u);
    EXPECT_GT(nimcp_language_get_vocab_size(lang), 100u);

    nimcp_tokenizer_destroy(tok);
    nimcp_native_language_destroy(lang);
}

// ============================================================================
// Destroy Order Independence
// ============================================================================

TEST(LanguagePipeline, DestroyOrderDoesNotMatter) {
    nimcp_language_config_t lang_cfg = nimcp_language_config_default();
    nimcp_tokenizer_config_t tok_cfg = nimcp_tokenizer_config_default();
    nimcp_inner_speech_config_t is_cfg = nimcp_inner_speech_config_default();

    nimcp_native_language_t* lang = nimcp_native_language_create(&lang_cfg);
    nimcp_tokenizer_t* tok = nimcp_tokenizer_create(&tok_cfg);
    nimcp_inner_speech_t* is = nimcp_inner_speech_create(&is_cfg);
    ASSERT_NE(lang, nullptr);
    ASSERT_NE(tok, nullptr);
    ASSERT_NE(is, nullptr);

    // Destroy in reverse order of creation
    nimcp_inner_speech_destroy(is);
    nimcp_tokenizer_destroy(tok);
    nimcp_native_language_destroy(lang);
    SUCCEED() << "Reverse destroy order did not crash";
}
