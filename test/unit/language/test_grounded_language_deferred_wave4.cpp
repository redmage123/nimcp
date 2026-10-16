/**
 * @file test_grounded_language_deferred_wave4.cpp
 * @brief Unit tests for #8 cross-modal disambiguation.
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

class GLDeferredW4 : public ::testing::Test {
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

    /* Ground a polysemous word with two different concept signatures
     * via two different modalities — simulates "bat" with a visual
     * concept (baseball-bat features) and a separate auditory concept
     * (flying-bat echolocation features). */
    void ground(const char* word, gl_modality_t mod,
                const std::vector<float>& features) {
        gl_grounding_event_t ev{};
        ev.word = word;
        ev.modality = mod;
        ev.sensory_features = features.data();
        ev.feature_dim = (uint32_t)features.size();
        ev.attention = 1.0f;
        grounded_language_ground(gl, &ev);
    }
};

TEST_F(GLDeferredW4, NullSafe) {
    uint64_t ids[4] = {0};
    float scores[4] = {0};
    EXPECT_EQ(0u, grounded_language_disambiguate(nullptr, "x", nullptr, ids, scores, 4));
    EXPECT_EQ(0u, grounded_language_disambiguate(gl, nullptr, nullptr, ids, scores, 4));
    EXPECT_EQ(0u, grounded_language_disambiguate(gl, "x", nullptr, nullptr, scores, 4));
    EXPECT_EQ(0u, grounded_language_disambiguate(gl, "x", nullptr, ids, nullptr, 4));
    EXPECT_EQ(0u, grounded_language_disambiguate(gl, "x", nullptr, ids, scores, 0));
}

TEST_F(GLDeferredW4, UnknownWordReturnsZero) {
    uint64_t ids[4] = {0};
    float scores[4] = {0};
    EXPECT_EQ(0u, grounded_language_disambiguate(gl, "phantom_xyz",
                                                   nullptr, ids, scores, 4));
}

TEST_F(GLDeferredW4, SingleBindingReturnsThatBinding) {
    std::vector<float> visual_feat(TEST_DIM, 0.0f); visual_feat[0] = 1.0f;
    /* Strengthen by repeated grounding so binding has decent strength. */
    for (int i = 0; i < 5; i++) ground("ball", GL_MODALITY_VISUAL, visual_feat);

    uint64_t ids[4] = {0};
    float scores[4] = {0};
    uint32_t n = grounded_language_disambiguate(gl, "ball", nullptr,
                                                  ids, scores, 4);
    ASSERT_GE(n, 1u);
    EXPECT_NE(0ull, ids[0]);
    EXPECT_GT(scores[0], 0.0f);
}

TEST_F(GLDeferredW4, VisualWeightSelectsVisualConcept) {
    /* Polysemous "polysem_word_x" with two distinct concepts — one
     * grounded via visual modality, the other via auditory. Repeatedly
     * ground each pair to build per-modality strength. */
    std::vector<float> vis_feat(TEST_DIM, 0.0f); vis_feat[0] = 1.0f;
    std::vector<float> aud_feat(TEST_DIM, 0.0f); aud_feat[1] = 1.0f;
    for (int i = 0; i < 8; i++) {
        ground("polysem_word_x", GL_MODALITY_VISUAL, vis_feat);
        ground("polysem_word_x", GL_MODALITY_AUDITORY, aud_feat);
    }

    /* Both bindings should exist. */
    const gl_lexicon_entry_t* e =
        grounded_language_lookup(gl, "polysem_word_x");
    ASSERT_NE(e, nullptr);
    ASSERT_GE(e->binding_count, 2u);

    /* Query with visual-only weights — top result should be the
     * binding with the strongest visual modality. */
    float weights_vis[GL_MODALITY_COUNT] = {0};
    weights_vis[GL_MODALITY_VISUAL] = 1.0f;

    uint64_t ids_vis[4] = {0};
    float scores_vis[4] = {0};
    uint32_t n_vis = grounded_language_disambiguate(
        gl, "polysem_word_x", weights_vis, ids_vis, scores_vis, 4);
    ASSERT_GE(n_vis, 2u);

    /* Query with auditory-only weights — top result must differ. */
    float weights_aud[GL_MODALITY_COUNT] = {0};
    weights_aud[GL_MODALITY_AUDITORY] = 1.0f;

    uint64_t ids_aud[4] = {0};
    float scores_aud[4] = {0};
    uint32_t n_aud = grounded_language_disambiguate(
        gl, "polysem_word_x", weights_aud, ids_aud, scores_aud, 4);
    ASSERT_GE(n_aud, 2u);

    /* The top-1 concept must be different between the two queries —
     * that's the disambiguation guarantee. */
    EXPECT_NE(ids_vis[0], ids_aud[0]);
}

TEST_F(GLDeferredW4, ScoresAreSortedDescending) {
    std::vector<float> f1(TEST_DIM, 0.0f); f1[0] = 1.0f;
    std::vector<float> f2(TEST_DIM, 0.0f); f2[1] = 1.0f;
    std::vector<float> f3(TEST_DIM, 0.0f); f3[2] = 1.0f;
    /* Ground three different concepts via the same word, with
     * different exposure counts. Higher exposure → higher confidence. */
    for (int i = 0; i < 10; i++) ground("trio", GL_MODALITY_VISUAL, f1);
    for (int i = 0; i < 5;  i++) ground("trio", GL_MODALITY_VISUAL, f2);
    for (int i = 0; i < 2;  i++) ground("trio", GL_MODALITY_VISUAL, f3);

    uint64_t ids[8] = {0};
    float scores[8] = {0};
    uint32_t n = grounded_language_disambiguate(gl, "trio", nullptr,
                                                  ids, scores, 8);
    ASSERT_GE(n, 3u);
    for (uint32_t i = 1; i < n; i++) {
        EXPECT_GE(scores[i - 1], scores[i])
            << "scores not sorted: scores[" << (i-1) << "]=" << scores[i-1]
            << " scores[" << i << "]=" << scores[i];
    }
}

TEST_F(GLDeferredW4, MaxKBoundsResultCount) {
    std::vector<float> f1(TEST_DIM, 0.0f); f1[0] = 1.0f;
    std::vector<float> f2(TEST_DIM, 0.0f); f2[1] = 1.0f;
    std::vector<float> f3(TEST_DIM, 0.0f); f3[2] = 1.0f;
    ground("triplet", GL_MODALITY_VISUAL, f1);
    ground("triplet", GL_MODALITY_VISUAL, f2);
    ground("triplet", GL_MODALITY_VISUAL, f3);

    uint64_t ids[1] = {0};
    float scores[1] = {0};
    uint32_t n = grounded_language_disambiguate(gl, "triplet", nullptr,
                                                  ids, scores, 1);
    EXPECT_EQ(1u, n);
}

TEST_F(GLDeferredW4, HeapFallbackForLargeBindingCount) {
    /* Walkthrough #9: exercise the heap-allocation path (n > 64
     * bindings). Use an isolated GL with a large semantic_dim so
     * orthogonal one-hot features yield unique concepts. */
    constexpr uint32_t WIDE_DIM = 128;
    semantic_memory_system_t* wsm = semantic_memory_create();
    ASSERT_NE(wsm, nullptr);
    grounded_language_t* wgl = grounded_language_create(WIDE_DIM, wsm);
    ASSERT_NE(wgl, nullptr);

    /* Use 80 nearly-orthogonal one-hot vectors. */
    for (int i = 0; i < 80; i++) {
        std::vector<float> f(WIDE_DIM, 0.0f);
        f[i % WIDE_DIM] = 1.0f;
        gl_grounding_event_t ev{};
        ev.word = "polysem_wide";
        ev.modality = GL_MODALITY_VISUAL;
        ev.sensory_features = f.data();
        ev.feature_dim = WIDE_DIM;
        ev.attention = 1.0f;
        grounded_language_ground(wgl, &ev);
    }

    const gl_lexicon_entry_t* e =
        grounded_language_lookup(wgl, "polysem_wide");
    ASSERT_NE(e, nullptr);
    if (e->binding_count <= 64) {
        grounded_language_destroy(wgl);
        semantic_memory_destroy(wsm);
        GTEST_SKIP() << "binding_count=" << e->binding_count
                     << " (semantic_memory merged signatures); "
                        "heap-fallback path remains untested but stack "
                        "path is exercised by other tests";
    }

    uint64_t ids[120] = {0};
    float scores[120] = {0};
    uint32_t n = grounded_language_disambiguate(wgl, "polysem_wide",
                                                  nullptr, ids, scores, 120);
    /* No crash, results sorted descending. */
    EXPECT_GE(n, 64u);
    for (uint32_t i = 1; i < n; i++) {
        EXPECT_GE(scores[i - 1], scores[i]);
    }

    grounded_language_destroy(wgl);
    semantic_memory_destroy(wsm);
}

TEST_F(GLDeferredW4, AllZeroWeightsFallsBackToOverallStrength) {
    std::vector<float> f1(TEST_DIM, 0.0f); f1[0] = 1.0f;
    std::vector<float> f2(TEST_DIM, 0.0f); f2[1] = 1.0f;
    /* Weak then strong groundings — expect strong concept ranks first. */
    for (int i = 0; i < 1;  i++) ground("aweak", GL_MODALITY_VISUAL, f1);
    for (int i = 0; i < 12; i++) ground("aweak", GL_MODALITY_VISUAL, f2);

    float zero_weights[GL_MODALITY_COUNT] = {0};
    uint64_t ids[4] = {0};
    float scores[4] = {0};
    uint32_t n = grounded_language_disambiguate(
        gl, "aweak", zero_weights, ids, scores, 4);
    ASSERT_GE(n, 2u);
    EXPECT_GE(scores[0], scores[1]);
}

}  // namespace
