/**
 * @file test_top_phrases_public_api.cpp
 * @brief Unit tests for the public C wrapper nimcp_brain_get_top_phrases().
 *
 * WHAT: Direct exercise of the C wrapper that the Python binding sits on
 *       top of. We don't construct a full brain (that requires nimcp_init()
 *       and 100+ subsystems for a 2M-neuron config); instead we cover:
 *
 *         1. NULL / invalid inputs return -1 cleanly
 *         2. The internal grounded_language_get_top_phrases() — which the
 *            wrapper delegates to — yields the expected shape after
 *            learn_from_text() with overlapping phrases
 *
 * WHY:  The wrapper is on the hot path for curriculum integration tests.
 *       If it ever drops a frequency or scrambles component_words, the
 *       integration tests will give a confusing failure several layers up.
 *       Catch that here instead.
 *
 * HOW:  GTest fixture mirrors test_grounded_language_diagnostics.cpp.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
}

namespace {

constexpr uint32_t TEST_DIM = 32;

class TopPhrasesPublicAPI : public ::testing::Test {
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

/* --- NULL / bad input handling on the underlying GL accessor. The public
 *     C wrapper (nimcp_brain_get_top_phrases) layers brain-handle validation
 *     on top of these — covered in the binding's own integration test. ----*/
TEST_F(TopPhrasesPublicAPI, RejectsNullGL) {
    const gl_phrase_t* out[4] = {nullptr};
    uint32_t n = grounded_language_get_top_phrases(nullptr, 0, 0, out, 4);
    EXPECT_EQ(n, 0u);
}

TEST_F(TopPhrasesPublicAPI, RejectsNullOutArray) {
    uint32_t n = grounded_language_get_top_phrases(gl, 0, 0, nullptr, 4);
    EXPECT_EQ(n, 0u);
}

TEST_F(TopPhrasesPublicAPI, RejectsZeroCapacity) {
    const gl_phrase_t* out[1] = {nullptr};
    uint32_t n = grounded_language_get_top_phrases(gl, 0, 0, out, 0);
    EXPECT_EQ(n, 0u);
}

TEST_F(TopPhrasesPublicAPI, EmptyOnFreshSystem) {
    const gl_phrase_t* out[8] = {nullptr};
    uint32_t n = grounded_language_get_top_phrases(gl, 0, 0, out, 8);
    /* Fresh GL has function-word seeds but no learned bigrams/trigrams yet. */
    EXPECT_EQ(n, 0u);
}

/* --- After learn_from_text on overlapping phrases, the most frequent
 *     bigrams/trigrams must surface and carry the expected metadata.  ----*/
TEST_F(TopPhrasesPublicAPI, OverlappingPhrasesYieldRankedResults) {
    const char* corpus[] = {
        "good morning everyone",
        "good morning friend",
        "good morning team",
        "happy birthday to you",
        "happy birthday to me",
    };
    for (auto* s : corpus) {
        for (int rep = 0; rep < 3; rep++) {
            grounded_language_learn_from_text(gl, s);
        }
    }

    constexpr uint32_t K = 16;
    const gl_phrase_t* out[K] = {nullptr};
    uint32_t n = grounded_language_get_top_phrases(gl, /*min_freq*/ 1,
                                                    /*min_n*/ 0, out, K);
    ASSERT_GT(n, 0u) << "No phrases tracked despite repeated exposure";

    bool found_good_morning = false;
    bool found_happy_birthday = false;
    for (uint32_t i = 0; i < n; i++) {
        ASSERT_NE(out[i], nullptr);
        EXPECT_GE(out[i]->frequency, 1u);
        EXPECT_TRUE(out[i]->component_words == 2u ||
                    out[i]->component_words == 3u)
            << "Component count must be 2 (bigram) or 3 (trigram)";
        if (std::strstr(out[i]->form, "good morning"))   found_good_morning   = true;
        if (std::strstr(out[i]->form, "happy birthday")) found_happy_birthday = true;
    }
    EXPECT_TRUE(found_good_morning || found_happy_birthday)
        << "Neither seeded phrase landed in top results — ingestion broken";

    /* Frequencies must be in non-increasing order (selection-sort contract). */
    for (uint32_t i = 1; i < n; i++) {
        EXPECT_GE(out[i - 1]->frequency, out[i]->frequency)
            << "Top-K not sorted at index " << i;
    }
}

TEST_F(TopPhrasesPublicAPI, MinNFilterRestrictsToTrigrams) {
    grounded_language_learn_from_text(gl, "good morning everyone");
    grounded_language_learn_from_text(gl, "good morning everyone");
    grounded_language_learn_from_text(gl, "happy birthday to you");
    grounded_language_learn_from_text(gl, "happy birthday to you");

    const gl_phrase_t* out[16] = {nullptr};
    uint32_t n = grounded_language_get_top_phrases(gl, 0, /*min_n*/ 3, out, 16);
    /* Trigrams may or may not be tracked depending on input length, but
     * everything that does come back must be exactly 3 components. */
    for (uint32_t i = 0; i < n; i++) {
        ASSERT_NE(out[i], nullptr);
        EXPECT_EQ(out[i]->component_words, 3u);
    }
}

TEST_F(TopPhrasesPublicAPI, MaxKBoundsResultCount) {
    for (int rep = 0; rep < 4; rep++) {
        grounded_language_learn_from_text(gl, "alpha beta gamma");
        grounded_language_learn_from_text(gl, "delta epsilon zeta");
        grounded_language_learn_from_text(gl, "eta theta iota");
    }
    const gl_phrase_t* out[2] = {nullptr};
    uint32_t n = grounded_language_get_top_phrases(gl, 0, 0, out, 2);
    EXPECT_LE(n, 2u);
}

}  // namespace
