/**
 * @file test_ground_word_with_emotion.cpp
 * @brief Unit tests for nimcp_brain_ground_word_with_emotion (the C wrapper
 *        used by the daemon ground_word RPC chain).
 *
 * WHAT: Direct exercise of the new emotion-aware C wrapper:
 *
 *         1. NULL handle returns non-OK cleanly
 *         2. Valid call with non-zero valence/arousal lands a binding and
 *            stamps the lexicon entry's valence/arousal fields
 *         3. The legacy nimcp_brain_ground_word and the new function with
 *            valence=0,arousal=0 produce equivalent lexicon state — they
 *            share an implementation, so they had better
 *
 * WHY:  This is the wrapper at the bottom of the
 *       BrainProxy.ground_word -> _cmd_ground_word -> Python
 *       Brain.ground_word -> nimcp_brain_ground_word_with_emotion chain
 *       wired in for curriculum-driven grounding events. Drift between
 *       the legacy and emotion variants would silently change
 *       behaviour for every existing call site.
 *
 * HOW:  Spin up a small fast brain (mirrors test_grounded_language_brain_integration
 *       style; the emotion path lives behind nimcp_brain_t). Compare lexicon
 *       state via grounded_language_lookup on the brain's internal_brain.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "nimcp.h"
#include "language/nimcp_grounded_language.h"
}

/* nimcp_brain_internal.h pulls C++-flavoured CUDA headers transitively;
 * include outside extern "C" so the toolchain treats it as C++ TU. */
#include "core/brain/nimcp_brain_internal.h"

/* Mirror the opaque handle so we can reach internal_brain — same trick the
 * cognitive_dispatch integration test uses. */
struct nimcp_brain_handle {
    brain_t internal_brain;
    float   last_loss;
    float   last_gradient_norm;
};

namespace {

constexpr uint32_t BRAIN_NEURONS = 100;
constexpr uint32_t SEMANTIC_DIM = 128;

class GroundWordWithEmotion : public ::testing::Test {
protected:
    nimcp_brain_t brain = nullptr;

    void SetUp() override {
        brain = nimcp_brain_create_fast("ground_word_emotion_test",
                                        NIMCP_TASK_CLASSIFICATION,
                                        BRAIN_NEURONS, 10, BRAIN_NEURONS);
        if (!brain) {
            GTEST_SKIP() << "Brain creation failed (resource constrained)";
        }
    }

    void TearDown() override {
        if (brain) nimcp_brain_destroy(brain);
    }

    /* Reach into the internal brain to grab the grounded_language handle.
     * We need it for direct lexicon inspection — the public API does not
     * expose per-entry valence/arousal. */
    grounded_language_t* gl() const {
        if (!brain || !brain->internal_brain) return nullptr;
        return brain->internal_brain->grounded_lang;
    }

    static std::vector<float> features(uint32_t dim, float fill) {
        return std::vector<float>(dim, fill);
    }
};

// ---- NULL handling -------------------------------------------------------

TEST_F(GroundWordWithEmotion, RejectsNullBrain) {
    auto f = features(SEMANTIC_DIM, 0.1f);
    nimcp_status_t s = nimcp_brain_ground_word_with_emotion(
        nullptr, "alpha", f.data(), SEMANTIC_DIM, 0, 0.7f, 0.5f, 0.4f);
    EXPECT_NE(s, NIMCP_OK);
}

TEST_F(GroundWordWithEmotion, RejectsNullWord) {
    auto f = features(SEMANTIC_DIM, 0.1f);
    nimcp_status_t s = nimcp_brain_ground_word_with_emotion(
        brain, nullptr, f.data(), SEMANTIC_DIM, 0, 0.7f, 0.0f, 0.0f);
    EXPECT_NE(s, NIMCP_OK);
}

TEST_F(GroundWordWithEmotion, RejectsNullFeatures) {
    nimcp_status_t s = nimcp_brain_ground_word_with_emotion(
        brain, "alpha", nullptr, SEMANTIC_DIM, 0, 0.7f, 0.0f, 0.0f);
    EXPECT_NE(s, NIMCP_OK);
}

// ---- Happy path stamps lexicon valence/arousal --------------------------

TEST_F(GroundWordWithEmotion, ValenceArousalLandOnLexiconEntry) {
    ASSERT_NE(gl(), nullptr) << "grounded_language not available on brain";

    auto f = features(SEMANTIC_DIM, 0.25f);
    nimcp_status_t s = nimcp_brain_ground_word_with_emotion(
        brain, "joyful", f.data(), SEMANTIC_DIM,
        /*modality=*/0, /*attention=*/0.9f,
        /*valence=*/0.7f, /*arousal=*/0.6f);
    EXPECT_EQ(s, NIMCP_OK);

    const gl_lexicon_entry_t* entry = grounded_language_lookup(gl(), "joyful");
    ASSERT_NE(entry, nullptr) << "Word 'joyful' missing from lexicon after ground";

    /* The implementation does an EMA blend toward event valence/arousal,
     * so a single fresh-entry call moves them strictly off 0 toward the
     * targets. We only need to see movement in the right direction. */
    EXPECT_GT(entry->valence, 0.0f) << "Positive valence did not propagate";
    EXPECT_GT(entry->arousal, 0.0f) << "Positive arousal did not propagate";
    EXPECT_LE(entry->valence, 0.7f + 1e-3f);
    EXPECT_LE(entry->arousal, 0.6f + 1e-3f);
}

TEST_F(GroundWordWithEmotion, NegativeValenceLandsAsNegative) {
    ASSERT_NE(gl(), nullptr);

    auto f = features(SEMANTIC_DIM, -0.1f);
    nimcp_status_t s = nimcp_brain_ground_word_with_emotion(
        brain, "sorrow", f.data(), SEMANTIC_DIM, 0, 0.9f,
        /*valence=*/-0.8f, /*arousal=*/0.3f);
    EXPECT_EQ(s, NIMCP_OK);

    const gl_lexicon_entry_t* entry = grounded_language_lookup(gl(), "sorrow");
    ASSERT_NE(entry, nullptr);
    EXPECT_LT(entry->valence, 0.0f) << "Negative valence did not propagate";
}

// ---- Legacy <-> new equivalence at zero emotion -------------------------

TEST_F(GroundWordWithEmotion, LegacyMatchesZeroEmotionWrapper) {
    ASSERT_NE(gl(), nullptr);

    auto f = features(SEMANTIC_DIM, 0.1f);

    /* Same word twice would just stack frequency; use distinct words. */
    nimcp_status_t s1 = nimcp_brain_ground_word(
        brain, "legacy_word", f.data(), SEMANTIC_DIM, 0, 0.7f);
    nimcp_status_t s2 = nimcp_brain_ground_word_with_emotion(
        brain, "emotion_word", f.data(), SEMANTIC_DIM, 0, 0.7f,
        /*valence=*/0.0f, /*arousal=*/0.0f);
    EXPECT_EQ(s1, NIMCP_OK);
    EXPECT_EQ(s2, NIMCP_OK);

    const gl_lexicon_entry_t* legacy = grounded_language_lookup(gl(), "legacy_word");
    const gl_lexicon_entry_t* fresh  = grounded_language_lookup(gl(), "emotion_word");
    ASSERT_NE(legacy, nullptr);
    ASSERT_NE(fresh,  nullptr);

    /* Both took the same code path with zero emotion — entry valence/arousal
     * should be identical (both essentially 0 ± EMA noise). */
    EXPECT_FLOAT_EQ(legacy->valence, fresh->valence);
    EXPECT_FLOAT_EQ(legacy->arousal, fresh->arousal);
    EXPECT_EQ(legacy->frequency, fresh->frequency);
}

}  // namespace
