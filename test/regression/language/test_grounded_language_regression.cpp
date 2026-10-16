/**
 * @file test_grounded_language_regression.cpp
 * @brief Regression tests for the grounded language system
 *
 * WHAT: Tests that protect against previously-found bugs and edge cases
 * WHY:  Prevent regression of fixed issues in grounded language processing
 * HOW:  Each test reproduces a specific failure scenario
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
}

// =============================================================================
// Helpers
// =============================================================================

static std::vector<float> random_vector(uint32_t dim, unsigned seed = 42) {
    std::vector<float> v(dim);
    srand(seed);
    for (uint32_t i = 0; i < dim; i++) {
        v[i] = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
    }
    return v;
}

// =============================================================================
// Test Fixture
// =============================================================================

class GroundedLanguageRegression : public ::testing::Test {
protected:
    grounded_language_t* gl = nullptr;
    semantic_memory_system_t* sm = nullptr;

    void SetUp() override {
        sm = semantic_memory_create();
        gl = grounded_language_create(32, sm);
        ASSERT_NE(gl, nullptr);
    }

    void TearDown() override {
        grounded_language_destroy(gl);
        semantic_memory_destroy(sm);
    }
};

// =============================================================================
// Regression: Empty string handling
// =============================================================================

TEST_F(GroundedLanguageRegression, EmptyStringComprehend) {
    gl_comprehension_result_t result = {};
    int rc = grounded_language_comprehend(gl, "", &result);
    EXPECT_EQ(rc, 0); // Should succeed with zero comprehension
    EXPECT_EQ(result.concept_count, 0u);
    gl_comprehension_result_cleanup(&result);
}

TEST_F(GroundedLanguageRegression, EmptyStringLearn) {
    int updates = grounded_language_learn_from_text(gl, "");
    EXPECT_EQ(updates, 0); // No words to learn
}

TEST_F(GroundedLanguageRegression, EmptyStringLearnPair) {
    float loss = grounded_language_learn_pair(gl, "", "", 0.1f);
    // Should not crash, loss can be anything valid
    EXPECT_GE(loss, 0.0f);
}

TEST_F(GroundedLanguageRegression, EmptyStringRespond) {
    char response[256] = {};
    float confidence = 0.0f;
    int rc = grounded_language_respond(gl, "", response, sizeof(response), &confidence);
    EXPECT_GE(rc, 0); // Should not crash
}

// =============================================================================
// Regression: Very long input
// =============================================================================

TEST_F(GroundedLanguageRegression, VeryLongText) {
    // Generate a very long text (>GL_MAX_PRODUCTION_WORDS words)
    std::string long_text;
    for (int i = 0; i < 500; i++) {
        long_text += "word" + std::to_string(i) + " ";
    }

    int updates = grounded_language_learn_from_text(gl, long_text.c_str());
    EXPECT_GE(updates, 0); // Should not crash, may truncate

    gl_comprehension_result_t result = {};
    int rc = grounded_language_comprehend(gl, long_text.c_str(), &result);
    EXPECT_EQ(rc, 0);
    gl_comprehension_result_cleanup(&result);
}

// =============================================================================
// Regression: Single character words
// =============================================================================

TEST_F(GroundedLanguageRegression, SingleCharWords) {
    grounded_language_learn_from_text(gl, "a b c d e f g h");

    const gl_lexicon_entry_t* entry = grounded_language_lookup(gl, "a");
    EXPECT_NE(entry, nullptr); // 'a' is a function word
}

// =============================================================================
// Regression: Punctuation in text
// =============================================================================

TEST_F(GroundedLanguageRegression, PunctuationHandling) {
    grounded_language_learn_from_text(gl, "Hello, world! How are you?");

    // Words should be stripped of punctuation
    EXPECT_NE(grounded_language_lookup(gl, "hello"), nullptr);
    EXPECT_NE(grounded_language_lookup(gl, "world"), nullptr);
    EXPECT_NE(grounded_language_lookup(gl, "how"), nullptr);
}

// =============================================================================
// Regression: Zero-dim feature vector
// =============================================================================

TEST_F(GroundedLanguageRegression, ZeroDimFeatures) {
    float dummy = 1.0f;
    uint64_t id = grounded_language_fast_map(gl, "test", &dummy, 0, 0);
    // Should handle gracefully (may create pseudo-concept or fail)
    // Just verify no crash
    (void)id;
}

// =============================================================================
// Regression: Repeated fast-mapping same word
// =============================================================================

TEST_F(GroundedLanguageRegression, RepeatedFastMap) {
    for (int i = 0; i < 100; i++) {
        auto features = random_vector(32, i);
        grounded_language_fast_map(gl, "repeated", features.data(), 32, 0);
    }

    const gl_lexicon_entry_t* entry = grounded_language_lookup(gl, "repeated");
    ASSERT_NE(entry, nullptr);
    // Should have grown bindings, not crashed
    EXPECT_GE(entry->binding_count, 1u);
}

// =============================================================================
// Regression: Production with no vocabulary
// =============================================================================

TEST_F(GroundedLanguageRegression, ProductionNoVocab) {
    // Create system without teaching any content words
    grounded_language_t* bare = grounded_language_create(32, nullptr);
    ASSERT_NE(bare, nullptr);

    auto features = random_vector(32, 999);
    gl_production_result_t result = {};
    int rc = grounded_language_produce(bare, features.data(), 32,
                                        GL_PRODUCE_DESCRIBE, &result);
    // Should not crash — may produce empty or function-word-only text
    EXPECT_EQ(rc, 0);
    gl_production_result_cleanup(&result);
    grounded_language_destroy(bare);
}

// =============================================================================
// Regression: Comprehend with only function words
// =============================================================================

TEST_F(GroundedLanguageRegression, ComprehendOnlyFunctionWords) {
    gl_comprehension_result_t result = {};
    int rc = grounded_language_comprehend(gl, "the a an is are", &result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(result.comprehension_confidence, 1.0f); // All known words
    EXPECT_EQ(result.concept_count, 0u); // But no concept bindings
    gl_comprehension_result_cleanup(&result);
}

// =============================================================================
// Regression: Save/load round-trip preserves function words
// =============================================================================

TEST_F(GroundedLanguageRegression, SaveLoadPreservesFunctionWords) {
    const char* path = "/tmp/test_gl_regression.bin";
    EXPECT_EQ(grounded_language_save(gl, path), 0);

    grounded_language_t* loaded = grounded_language_load(path, sm);
    ASSERT_NE(loaded, nullptr);

    // Function words should survive
    EXPECT_NE(grounded_language_lookup(loaded, "the"), nullptr);
    EXPECT_NE(grounded_language_lookup(loaded, "is"), nullptr);
    EXPECT_NE(grounded_language_lookup(loaded, "and"), nullptr);

    grounded_language_destroy(loaded);
    remove(path);
}

// =============================================================================
// Regression: Concurrent comprehend + learn doesn't corrupt
// =============================================================================

TEST_F(GroundedLanguageRegression, LearnDuringComprehend) {
    // Interleave learning and comprehension (single-threaded)
    for (int i = 0; i < 50; i++) {
        grounded_language_learn_from_text(gl, "the cat sits");

        gl_comprehension_result_t result = {};
        grounded_language_comprehend(gl, "the cat", &result);
        gl_comprehension_result_cleanup(&result);
    }
    // Should not corrupt lexicon
    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT_GT(stats.vocab_size, 0u);
}

// =============================================================================
// Regression: Grounding with high emotional valence
// =============================================================================

TEST_F(GroundedLanguageRegression, HighEmotionalGrounding) {
    auto features = random_vector(32, 1234);
    gl_grounding_event_t event = {};
    event.word = "danger";
    event.modality = GL_MODALITY_EMOTIONAL;
    event.sensory_features = features.data();
    event.feature_dim = 32;
    event.attention = 1.0f;
    event.emotional_valence = -1.0f; // Very negative
    event.emotional_arousal = 1.0f;   // Very aroused

    EXPECT_EQ(grounded_language_ground(gl, &event), 0);

    const gl_lexicon_entry_t* entry = grounded_language_lookup(gl, "danger");
    ASSERT_NE(entry, nullptr);
    EXPECT_LT(entry->valence, 0.0f); // Should be negative
    EXPECT_GT(entry->arousal, 0.0f); // Should be positive
}
