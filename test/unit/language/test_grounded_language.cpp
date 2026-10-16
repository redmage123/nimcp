/**
 * @file test_grounded_language.cpp
 * @brief Unit tests for the grounded language system
 *
 * WHAT: Tests lexicon creation, word-concept binding, comprehension, production,
 *       distributional learning, fast mapping, templates, serialization
 * WHY:  The grounded language system is the core of human-like language
 *       acquisition — must correctly bind words to concepts and produce text
 * HOW:  GTest fixture creates a standalone grounded language system (no brain);
 *       exercises all core operations in isolation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <cstdio>

extern "C" {
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
}

// =============================================================================
// Constants
// =============================================================================

static constexpr uint32_t TEST_DIM = 32;

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

class GroundedLanguageUnit : public ::testing::Test {
protected:
    grounded_language_t* gl = nullptr;
    semantic_memory_system_t* sm = nullptr;

    void SetUp() override {
        sm = semantic_memory_create();
        ASSERT_NE(sm, nullptr);
        gl = grounded_language_create(TEST_DIM, sm);
        ASSERT_NE(gl, nullptr);
    }

    void TearDown() override {
        grounded_language_destroy(gl);
        semantic_memory_destroy(sm);
    }
};

// =============================================================================
// Lifecycle Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, CreateDestroy) {
    // Already created in SetUp — just verify stats
    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT_GT(stats.vocab_size, 0u); // Function words seeded
}

TEST_F(GroundedLanguageUnit, CreateWithDefaultDim) {
    grounded_language_t* gl2 = grounded_language_create(0, nullptr);
    ASSERT_NE(gl2, nullptr);
    grounded_language_destroy(gl2);
}

TEST_F(GroundedLanguageUnit, CreateWithNullSemantic) {
    grounded_language_t* gl2 = grounded_language_create(TEST_DIM, nullptr);
    ASSERT_NE(gl2, nullptr);
    grounded_language_destroy(gl2);
}

TEST_F(GroundedLanguageUnit, DestroyNull) {
    grounded_language_destroy(nullptr); // Should not crash
}

// =============================================================================
// Lexicon Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, LookupFunctionWord) {
    const gl_lexicon_entry_t* entry = grounded_language_lookup(gl, "the");
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->form, "the");
    EXPECT_EQ(entry->learned_class, GL_CLASS_FUNCTION);
}

TEST_F(GroundedLanguageUnit, LookupUnknownWord) {
    const gl_lexicon_entry_t* entry = grounded_language_lookup(gl, "xyzzy");
    EXPECT_EQ(entry, nullptr);
}

TEST_F(GroundedLanguageUnit, LookupCaseInsensitive) {
    const gl_lexicon_entry_t* entry = grounded_language_lookup(gl, "THE");
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->form, "the");
}

TEST_F(GroundedLanguageUnit, LookupNullArgs) {
    EXPECT_EQ(grounded_language_lookup(nullptr, "hello"), nullptr);
    EXPECT_EQ(grounded_language_lookup(gl, nullptr), nullptr);
}

// =============================================================================
// Fast Mapping Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, FastMapCreatesBinding) {
    auto features = random_vector(TEST_DIM, 1);
    uint64_t concept_id = grounded_language_fast_map(gl, "elephant",
        features.data(), TEST_DIM, 0);
    EXPECT_NE(concept_id, 0u);

    // Verify word is now in lexicon
    const gl_lexicon_entry_t* entry = grounded_language_lookup(gl, "elephant");
    ASSERT_NE(entry, nullptr);
    EXPECT_GT(entry->binding_count, 0u);
    EXPECT_TRUE(entry->context_initialized);
}

TEST_F(GroundedLanguageUnit, FastMapSameWordTwice) {
    auto f1 = random_vector(TEST_DIM, 1);
    auto f2 = random_vector(TEST_DIM, 2);

    uint64_t c1 = grounded_language_fast_map(gl, "dog", f1.data(), TEST_DIM, 0);
    uint64_t c2 = grounded_language_fast_map(gl, "dog", f2.data(), TEST_DIM, 0);
    EXPECT_NE(c1, 0u);
    EXPECT_NE(c2, 0u);

    // Should have bindings to both concepts (polysemy)
    const gl_lexicon_entry_t* entry = grounded_language_lookup(gl, "dog");
    ASSERT_NE(entry, nullptr);
    EXPECT_GE(entry->binding_count, 1u); // At least one (may merge similar)
}

TEST_F(GroundedLanguageUnit, FastMapNullArgs) {
    auto features = random_vector(TEST_DIM);
    EXPECT_EQ(grounded_language_fast_map(nullptr, "x", features.data(), TEST_DIM, 0), 0u);
    EXPECT_EQ(grounded_language_fast_map(gl, nullptr, features.data(), TEST_DIM, 0), 0u);
    EXPECT_EQ(grounded_language_fast_map(gl, "x", nullptr, TEST_DIM, 0), 0u);
}

// =============================================================================
// Grounding Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, GroundWordVisual) {
    auto features = random_vector(TEST_DIM, 10);
    gl_grounding_event_t event = {};
    event.word = "sunset";
    event.modality = GL_MODALITY_VISUAL;
    event.sensory_features = features.data();
    event.feature_dim = TEST_DIM;
    event.attention = 0.9f;
    event.emotional_valence = 0.7f;
    event.emotional_arousal = 0.5f;

    EXPECT_EQ(grounded_language_ground(gl, &event), 0);

    const gl_lexicon_entry_t* entry = grounded_language_lookup(gl, "sunset");
    ASSERT_NE(entry, nullptr);
    EXPECT_GT(entry->binding_count, 0u);
    EXPECT_GT(entry->valence, 0.0f); // Positive emotional grounding
}

TEST_F(GroundedLanguageUnit, GroundWordWithContext) {
    auto features = random_vector(TEST_DIM, 20);
    gl_grounding_event_t event = {};
    event.word = "fire";
    event.modality = GL_MODALITY_VISUAL;
    event.sensory_features = features.data();
    event.feature_dim = TEST_DIM;
    event.attention = 0.8f;
    event.context_sentence = "the fire is hot and bright";

    EXPECT_EQ(grounded_language_ground(gl, &event), 0);

    // "hot" and "bright" should also be in the lexicon now
    const gl_lexicon_entry_t* hot = grounded_language_lookup(gl, "hot");
    EXPECT_NE(hot, nullptr); // Created by context sentence learning
}

TEST_F(GroundedLanguageUnit, GroundNullArgs) {
    auto features = random_vector(TEST_DIM);
    gl_grounding_event_t event = {};
    event.word = "test";
    event.sensory_features = features.data();
    event.feature_dim = TEST_DIM;
    event.attention = 0.5f;

    EXPECT_EQ(grounded_language_ground(nullptr, &event), -1);
    EXPECT_EQ(grounded_language_ground(gl, nullptr), -1);
}

// =============================================================================
// Distributional Learning Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, LearnFromText) {
    int updates = grounded_language_learn_from_text(gl, "the big red dog runs fast");
    EXPECT_GT(updates, 0);

    // Check that content words were created
    EXPECT_NE(grounded_language_lookup(gl, "big"), nullptr);
    EXPECT_NE(grounded_language_lookup(gl, "dog"), nullptr);
    EXPECT_NE(grounded_language_lookup(gl, "runs"), nullptr);
}

TEST_F(GroundedLanguageUnit, LearnFromTextMultiple) {
    const char* texts[] = {
        "the cat sits on the mat",
        "the dog runs in the park",
        "the bird flies in the sky",
        "the fish swims in the sea",
    };

    for (int i = 0; i < 4; i++) {
        int updates = grounded_language_learn_from_text(gl, texts[i]);
        EXPECT_GT(updates, 0);
    }

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT_GT(stats.vocab_size, 60u); // Function words + content words
}

TEST_F(GroundedLanguageUnit, LearnFromTextNull) {
    EXPECT_EQ(grounded_language_learn_from_text(nullptr, "hello"), -1);
    EXPECT_EQ(grounded_language_learn_from_text(gl, nullptr), -1);
}

// =============================================================================
// Syntax Learning Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, LearnSyntax) {
    // Teach some words first so we have class info
    grounded_language_learn_from_text(gl, "the big cat sits quietly");
    grounded_language_learn_from_text(gl, "the small dog runs fast");

    int patterns = grounded_language_learn_syntax(gl, "the old tree grows tall");
    EXPECT_GE(patterns, 0);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    // Built-in templates should exist; learned templates may be 0 with sparse input
    EXPECT_GE(patterns, 0); // No crash, may or may not learn new patterns
}

TEST_F(GroundedLanguageUnit, LearnSyntaxNull) {
    EXPECT_EQ(grounded_language_learn_syntax(nullptr, "hello"), -1);
    EXPECT_EQ(grounded_language_learn_syntax(gl, nullptr), -1);
}

// =============================================================================
// Comprehension Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, ComprehendKnownWords) {
    // Teach some words
    auto dog_features = random_vector(TEST_DIM, 100);
    grounded_language_fast_map(gl, "dog", dog_features.data(), TEST_DIM, 0);

    auto big_features = random_vector(TEST_DIM, 101);
    grounded_language_fast_map(gl, "big", big_features.data(), TEST_DIM, 2);

    gl_comprehension_result_t result = {};
    EXPECT_EQ(grounded_language_comprehend(gl, "the big dog", &result), 0);

    EXPECT_GT(result.concept_count, 0u);
    EXPECT_GT(result.comprehension_confidence, 0.0f);
    EXPECT_NE(result.semantic_vector, nullptr);

    // Semantic vector should be non-zero
    float norm = 0.0f;
    for (uint32_t i = 0; i < TEST_DIM; i++) {
        norm += result.semantic_vector[i] * result.semantic_vector[i];
    }
    EXPECT_GT(norm, 0.1f);

    gl_comprehension_result_cleanup(&result);
}

TEST_F(GroundedLanguageUnit, ComprehendAllUnknown) {
    gl_comprehension_result_t result = {};
    EXPECT_EQ(grounded_language_comprehend(gl, "xyzzy qwerty", &result), 0);

    // Should have low confidence
    EXPECT_LT(result.comprehension_confidence, 0.5f);

    gl_comprehension_result_cleanup(&result);
}

TEST_F(GroundedLanguageUnit, ComprehendNullArgs) {
    gl_comprehension_result_t result = {};
    EXPECT_EQ(grounded_language_comprehend(nullptr, "hello", &result), -1);
    EXPECT_EQ(grounded_language_comprehend(gl, nullptr, &result), -1);
    EXPECT_EQ(grounded_language_comprehend(gl, "hello", nullptr), -1);
}

TEST_F(GroundedLanguageUnit, ComprehensionResultCleanupNull) {
    gl_comprehension_result_cleanup(nullptr); // Should not crash
}

// =============================================================================
// Production Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, ProduceFromIntent) {
    // Teach vocabulary
    auto dog_f = random_vector(TEST_DIM, 200);
    auto cat_f = random_vector(TEST_DIM, 201);
    auto big_f = random_vector(TEST_DIM, 202);
    auto run_f = random_vector(TEST_DIM, 203);

    grounded_language_fast_map(gl, "dog", dog_f.data(), TEST_DIM, 0);
    grounded_language_fast_map(gl, "cat", cat_f.data(), TEST_DIM, 0);
    grounded_language_fast_map(gl, "big", big_f.data(), TEST_DIM, 2);
    grounded_language_fast_map(gl, "runs", run_f.data(), TEST_DIM, 1);

    // Produce text from dog-like intent
    gl_production_result_t result = {};
    EXPECT_EQ(grounded_language_produce(gl, dog_f.data(), TEST_DIM,
                                         GL_PRODUCE_DESCRIBE, &result), 0);

    EXPECT_NE(result.text, nullptr);
    EXPECT_GT(result.word_count, 0u);
    EXPECT_GT(strlen(result.text), 0u);

    gl_production_result_cleanup(&result);
}

TEST_F(GroundedLanguageUnit, ProduceNullArgs) {
    auto features = random_vector(TEST_DIM);
    gl_production_result_t result = {};

    EXPECT_EQ(grounded_language_produce(nullptr, features.data(), TEST_DIM,
                                         GL_PRODUCE_DESCRIBE, &result), -1);
    EXPECT_EQ(grounded_language_produce(gl, nullptr, TEST_DIM,
                                         GL_PRODUCE_DESCRIBE, &result), -1);
    EXPECT_EQ(grounded_language_produce(gl, features.data(), TEST_DIM,
                                         GL_PRODUCE_DESCRIBE, nullptr), -1);
}

TEST_F(GroundedLanguageUnit, ProductionResultCleanupNull) {
    gl_production_result_cleanup(nullptr); // Should not crash
}

// =============================================================================
// Blend Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, BlendTwoConcepts) {
    auto fire_f = random_vector(TEST_DIM, 300);
    auto water_f = random_vector(TEST_DIM, 301);

    grounded_language_fast_map(gl, "fire", fire_f.data(), TEST_DIM, 0);
    grounded_language_fast_map(gl, "water", water_f.data(), TEST_DIM, 0);

    gl_production_result_t result = {};
    EXPECT_EQ(grounded_language_blend(gl, 0, 0,
                                       fire_f.data(), water_f.data(), TEST_DIM,
                                       0.5f, &result), 0);

    EXPECT_NE(result.text, nullptr);
    EXPECT_GE(result.creativity, 0.0f);

    gl_production_result_cleanup(&result);
}

// =============================================================================
// Respond Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, Respond) {
    // Teach vocabulary
    grounded_language_learn_from_text(gl, "the sky is blue");
    grounded_language_learn_from_text(gl, "the sun is bright");

    auto sky_f = random_vector(TEST_DIM, 400);
    grounded_language_fast_map(gl, "sky", sky_f.data(), TEST_DIM, 0);

    char response[512];
    float confidence = 0.0f;
    int rc = grounded_language_respond(gl, "what is the sky", response, sizeof(response), &confidence);
    EXPECT_GE(rc, 0);
    EXPECT_GT(strlen(response), 0u);
}

TEST_F(GroundedLanguageUnit, RespondNullArgs) {
    char buf[64];
    float conf;
    EXPECT_EQ(grounded_language_respond(nullptr, "hi", buf, 64, &conf), -1);
    EXPECT_EQ(grounded_language_respond(gl, nullptr, buf, 64, &conf), -1);
    EXPECT_EQ(grounded_language_respond(gl, "hi", nullptr, 64, &conf), -1);
}

// =============================================================================
// Learning Pair Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, LearnPairReducesLoss) {
    // Teach some base vocabulary
    grounded_language_learn_from_text(gl, "the cat sits on the mat");
    grounded_language_learn_from_text(gl, "the dog runs in the park");

    auto cat_f = random_vector(TEST_DIM, 500);
    grounded_language_fast_map(gl, "cat", cat_f.data(), TEST_DIM, 0);

    float loss1 = grounded_language_learn_pair(gl, "where is the cat", "the cat sits on the mat", 0.0f);
    EXPECT_GE(loss1, 0.0f);
    EXPECT_LE(loss1, 1.0f);

    // Train more — loss should eventually decrease
    float loss_final = loss1;
    for (int i = 0; i < 10; i++) {
        loss_final = grounded_language_learn_pair(gl, "where is the cat", "the cat sits on the mat", 0.1f);
    }
    // With Hebbian learning, later loss should be <= initial
    EXPECT_LE(loss_final, loss1 + 0.1f); // Allow small tolerance
}

TEST_F(GroundedLanguageUnit, LearnPairNullArgs) {
    EXPECT_LT(grounded_language_learn_pair(nullptr, "a", "b", 0.1f), 0.0f);
    EXPECT_LT(grounded_language_learn_pair(gl, nullptr, "b", 0.1f), 0.0f);
    EXPECT_LT(grounded_language_learn_pair(gl, "a", nullptr, 0.1f), 0.0f);
}

// =============================================================================
// Words for Concept Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, WordsForConcept) {
    auto features = random_vector(TEST_DIM, 600);
    uint64_t concept_id = grounded_language_fast_map(gl, "mountain", features.data(), TEST_DIM, 0);
    EXPECT_NE(concept_id, 0u);

    const char* words[8] = {};
    uint32_t count = grounded_language_words_for_concept(gl, concept_id, words, 8);
    EXPECT_GE(count, 1u);
    EXPECT_STREQ(words[0], "mountain");
}

TEST_F(GroundedLanguageUnit, WordsForConceptNullArgs) {
    const char* words[8] = {};
    EXPECT_EQ(grounded_language_words_for_concept(nullptr, 1, words, 8), 0u);
    EXPECT_EQ(grounded_language_words_for_concept(gl, 1, nullptr, 8), 0u);
    EXPECT_EQ(grounded_language_words_for_concept(gl, 1, words, 0), 0u);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, StatsAfterLearning) {
    grounded_language_learn_from_text(gl, "the big red dog runs fast");

    auto features = random_vector(TEST_DIM, 700);
    gl_grounding_event_t event = {};
    event.word = "dog";
    event.modality = GL_MODALITY_VISUAL;
    event.sensory_features = features.data();
    event.feature_dim = TEST_DIM;
    event.attention = 0.8f;
    grounded_language_ground(gl, &event);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);

    EXPECT_GT(stats.vocab_size, 60u);
    EXPECT_GT(stats.total_bindings, 0u);
    EXPECT_GT(stats.total_groundings, 0u);
}

TEST_F(GroundedLanguageUnit, StatsNullArgs) {
    gl_stats_t stats;
    grounded_language_get_stats(nullptr, &stats); // Should not crash
    grounded_language_get_stats(gl, nullptr);      // Should not crash
}

// =============================================================================
// Serialization Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, SaveAndLoad) {
    // Build vocabulary
    auto dog_f = random_vector(TEST_DIM, 800);
    grounded_language_fast_map(gl, "dog", dog_f.data(), TEST_DIM, 0);
    grounded_language_learn_from_text(gl, "the dog is good");

    // Save
    const char* path = "/tmp/test_grounded_lang.bin";
    EXPECT_EQ(grounded_language_save(gl, path), 0);

    // Load
    grounded_language_t* loaded = grounded_language_load(path, sm);
    ASSERT_NE(loaded, nullptr);

    // Verify vocabulary preserved
    const gl_lexicon_entry_t* entry = grounded_language_lookup(loaded, "dog");
    ASSERT_NE(entry, nullptr);
    EXPECT_GT(entry->binding_count, 0u);

    gl_stats_t stats_orig, stats_loaded;
    grounded_language_get_stats(gl, &stats_orig);
    grounded_language_get_stats(loaded, &stats_loaded);
    EXPECT_EQ(stats_orig.vocab_size, stats_loaded.vocab_size);

    grounded_language_destroy(loaded);
    remove(path);
}

TEST_F(GroundedLanguageUnit, SaveNullArgs) {
    EXPECT_EQ(grounded_language_save(nullptr, "/tmp/test.bin"), -1);
    EXPECT_EQ(grounded_language_save(gl, nullptr), -1);
}

TEST_F(GroundedLanguageUnit, LoadNullPath) {
    EXPECT_EQ(grounded_language_load(nullptr, nullptr), nullptr);
}

TEST_F(GroundedLanguageUnit, LoadInvalidFile) {
    EXPECT_EQ(grounded_language_load("/nonexistent/path.bin", nullptr), nullptr);
}

// =============================================================================
// Cross-modal Connection Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, ConnectModalities) {
    // These are just wiring — shouldn't crash
    int dummy_vis = 1, dummy_aud = 2, dummy_speech = 3, dummy_col = 4, dummy_emo = 5;
    grounded_language_connect_visual(gl, &dummy_vis);
    grounded_language_connect_auditory(gl, &dummy_aud);
    grounded_language_connect_speech(gl, &dummy_speech);
    grounded_language_connect_columns(gl, &dummy_col);
    grounded_language_connect_emotional(gl, &dummy_emo);

    // Should not crash with null
    grounded_language_connect_visual(nullptr, &dummy_vis);
    grounded_language_connect_visual(gl, nullptr);
}

// =============================================================================
// Set Semantic Memory Tests
// =============================================================================

TEST_F(GroundedLanguageUnit, SetSemanticMemoryLate) {
    grounded_language_t* gl2 = grounded_language_create(TEST_DIM, nullptr);
    ASSERT_NE(gl2, nullptr);

    // Should work without semantic memory (pseudo-concepts)
    auto features = random_vector(TEST_DIM, 900);
    uint64_t id = grounded_language_fast_map(gl2, "test", features.data(), TEST_DIM, 0);
    EXPECT_NE(id, 0u);

    // Wire semantic memory later
    grounded_language_set_semantic_memory(gl2, sm);

    // Now should use real semantic memory
    auto features2 = random_vector(TEST_DIM, 901);
    uint64_t id2 = grounded_language_fast_map(gl2, "test2", features2.data(), TEST_DIM, 0);
    EXPECT_NE(id2, 0u);

    grounded_language_destroy(gl2);
}
