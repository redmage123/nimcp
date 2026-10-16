/**
 * @file test_grounded_language_nlp.cpp
 * @brief Unit tests for the NLP frontend: morphology, POS hints,
 *        embedding/tokenizer connect points, and the comprehend
 *        lookup chain (exact → morph → fuzzy).
 *
 * WHAT: Verify each NLP stage in isolation + the integrated chain.
 *       Embedding/tokenizer paths use NULL handles (passthrough)
 *       since constructing real instances pulls in heavy deps that
 *       don't belong in a unit test.
 *
 * WHY:  The NLP frontend sits in the comprehend hot path. Bugs there
 *       silently degrade vocabulary recall without surfacing in any
 *       end-to-end metric for hours of training.
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

class GLNlp : public ::testing::Test {
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
        if (gl) grounded_language_destroy(gl);
        if (sm) semantic_memory_destroy(sm);
    }
};

/* --- Morphology: suffix stripping ----------------------------------- */
TEST_F(GLNlp, MorphPlural) {
    char out[32];
    EXPECT_GT(gl_morph_normalize("cats", out, sizeof(out)), 0);
    EXPECT_STREQ(out, "cat");
}
TEST_F(GLNlp, MorphPastTense) {
    char out[32];
    EXPECT_GT(gl_morph_normalize("walked", out, sizeof(out)), 0);
    EXPECT_STREQ(out, "walk");
}
TEST_F(GLNlp, MorphIngDeDouble) {
    char out[32];
    EXPECT_GT(gl_morph_normalize("running", out, sizeof(out)), 0);
    EXPECT_STREQ(out, "run");
}
TEST_F(GLNlp, MorphIesToY) {
    char out[32];
    EXPECT_GT(gl_morph_normalize("studies", out, sizeof(out)), 0);
    EXPECT_STREQ(out, "study");
}
TEST_F(GLNlp, MorphAdverbLy) {
    char out[32];
    EXPECT_GT(gl_morph_normalize("quickly", out, sizeof(out)), 0);
    EXPECT_STREQ(out, "quick");
}
TEST_F(GLNlp, MorphTooShortLeftAlone) {
    char out[32];
    EXPECT_EQ(0, gl_morph_normalize("cat", out, sizeof(out)));
    EXPECT_STREQ(out, "cat");
}
TEST_F(GLNlp, MorphNullSafe) {
    char out[32];
    EXPECT_EQ(-1, gl_morph_normalize(nullptr, out, sizeof(out)));
    EXPECT_EQ(-1, gl_morph_normalize("cat", nullptr, sizeof(out)));
    EXPECT_EQ(-1, gl_morph_normalize("cat", out, 0));
}

/* --- POS hints from morphology -------------------------------------- */
TEST_F(GLNlp, PosHintNounTion) {
    EXPECT_EQ(GL_CLASS_NOUN, gl_morph_pos_hint("evaluation"));
    EXPECT_EQ(GL_CLASS_NOUN, gl_morph_pos_hint("happiness"));
    EXPECT_EQ(GL_CLASS_NOUN, gl_morph_pos_hint("development"));
}
TEST_F(GLNlp, PosHintVerbIngEd) {
    EXPECT_EQ(GL_CLASS_VERB, gl_morph_pos_hint("running"));
    EXPECT_EQ(GL_CLASS_VERB, gl_morph_pos_hint("walked"));
    EXPECT_EQ(GL_CLASS_VERB, gl_morph_pos_hint("organize"));
}
TEST_F(GLNlp, PosHintAdjective) {
    EXPECT_EQ(GL_CLASS_ADJECTIVE, gl_morph_pos_hint("dangerous"));
    EXPECT_EQ(GL_CLASS_ADJECTIVE, gl_morph_pos_hint("creative"));
    EXPECT_EQ(GL_CLASS_ADJECTIVE, gl_morph_pos_hint("readable"));
}
TEST_F(GLNlp, PosHintAdverbLy) {
    EXPECT_EQ(GL_CLASS_ADVERB, gl_morph_pos_hint("quickly"));
    EXPECT_EQ(GL_CLASS_ADVERB, gl_morph_pos_hint("silently"));
}
TEST_F(GLNlp, PosHintUnknownNoSuffix) {
    EXPECT_EQ(GL_CLASS_UNKNOWN, gl_morph_pos_hint("dog"));
    EXPECT_EQ(GL_CLASS_UNKNOWN, gl_morph_pos_hint(""));
    EXPECT_EQ(GL_CLASS_UNKNOWN, gl_morph_pos_hint(nullptr));
}

/* --- Newly-created lexicon entries get a morph-derived class hint --- */
TEST_F(GLNlp, NewWordSeedsClassFromMorphology) {
    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    grounded_language_fast_map(gl, "running", f.data(), TEST_DIM, 0);
    const gl_lexicon_entry_t* e = grounded_language_lookup(gl, "running");
    ASSERT_NE(e, nullptr);
    /* Either the morph hint won (VERB at 0.4) OR the seed gave it a
     * different class — but it must not be UNKNOWN. */
    EXPECT_NE(GL_CLASS_UNKNOWN, e->learned_class);
}

/* --- Connect points: NULL safety ------------------------------------ */
TEST_F(GLNlp, ConnectEmbeddingsNullSafe) {
    grounded_language_connect_embeddings(gl, nullptr, 0, nullptr, nullptr);
    /* Nothing to assert — must not crash. comprehend below tests no-op. */
}
TEST_F(GLNlp, ConnectTokenizerNullSafe) {
    grounded_language_connect_tokenizer(gl, nullptr);
}

/* --- Comprehend with morph chain: "cats" matches a seeded "cat"-like
 *     entry. We seed an arbitrary lexicon word, then comprehend its
 *     plural form and confirm the morph stem matches (confidence > 0). */
TEST_F(GLNlp, ComprehendChainResolvesPluralViaMorph) {
    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    grounded_language_fast_map(gl, "widget", f.data(), TEST_DIM, 0);

    gl_comprehension_result_t r;
    ASSERT_EQ(0, grounded_language_comprehend(gl, "widgets", &r));
    /* "widgets" → morph-strip "s" → "widget" → exact hit. Confidence
     * should be 1.0 (or close, depending on SNN bridge blend). */
    EXPECT_GT(r.comprehension_confidence, 0.5f)
        << "morph chain should recover plural form";

    gl_comprehension_result_cleanup(&r);
}

/* --- Comprehend without morph match still returns valid struct ----- */
TEST_F(GLNlp, ComprehendOnlyOOVStillSafe) {
    /* No lexicon word here that "zxqvkbjpfm" can morph or fuzzy to. */
    gl_comprehension_result_t r;
    ASSERT_EQ(0, grounded_language_comprehend(gl, "zxqvkbjpfm", &r));
    EXPECT_LT(r.comprehension_confidence, 0.5f);
    gl_comprehension_result_cleanup(&r);
}

}  // namespace
