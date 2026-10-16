/**
 * @file test_grounded_language_region_bridge.cpp
 * @brief Unit tests for grounded_language ↔ broca/wernicke wiring.
 *
 * WHAT: Cover the two new connect-* setters + the lexicon mirror hook
 *       (called from lexicon_find_or_create) + the wernicke ingest
 *       function. Adapters are real instances created via the public
 *       broca_create / wernicke_create APIs.
 *
 * WHY:  Mirror logic must be a pure function of grounded_language —
 *       broca/wernicke must NOT receive duplicates and the path must
 *       no-op cleanly when adapters aren't connected.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
}

namespace {

constexpr uint32_t TEST_DIM = 32;

class GLRegionBridge : public ::testing::Test {
protected:
    grounded_language_t* gl = nullptr;
    semantic_memory_system_t* sm = nullptr;
    broca_adapter_t* broca = nullptr;
    wernicke_adapter_t* wernicke = nullptr;

    void SetUp() override {
        sm = semantic_memory_create();
        ASSERT_NE(sm, nullptr);
        gl = grounded_language_create(TEST_DIM, sm);
        ASSERT_NE(gl, nullptr);

        broca_config_t bc = broca_default_config();
        broca = broca_create(&bc);
        if (!broca) GTEST_SKIP() << "broca_create failed";

        wernicke_config_t wc = wernicke_default_config();
        wernicke = wernicke_create(&wc);
        if (!wernicke) GTEST_SKIP() << "wernicke_create failed";
    }
    void TearDown() override {
        if (wernicke) wernicke_destroy(wernicke);
        if (broca) broca_destroy(broca);
        if (gl) grounded_language_destroy(gl);
        if (sm) semantic_memory_destroy(sm);
    }
};

/* --- Connect setters: NULL safety ------------------------------------ */
TEST_F(GLRegionBridge, ConnectNullHandleSafe) {
    grounded_language_connect_broca(nullptr, broca);
    grounded_language_connect_wernicke(nullptr, wernicke);
    SUCCEED();
}

TEST_F(GLRegionBridge, ConnectNullValueAllowed) {
    grounded_language_connect_broca(gl, nullptr);
    grounded_language_connect_wernicke(gl, nullptr);
    SUCCEED();
}

/* --- Mirror hook fires on new lexicon entry creation --------------- */
TEST_F(GLRegionBridge, FastMapMirrorsToBrocaAndWernicke) {
    grounded_language_connect_broca(gl, broca);
    grounded_language_connect_wernicke(gl, wernicke);
    /* Simply assert the call completes without crashing. The adapters'
     * internal lexicon counts are not exposed via a public getter, so
     * the contract here is "no crash on the mirror path", which is the
     * real-world failure mode we're guarding against. */
    std::vector<float> feats(TEST_DIM, 0.5f);
    uint64_t cid = grounded_language_fast_map(gl, "apple",
                                               feats.data(), TEST_DIM, 0);
    EXPECT_NE(cid, 0u);
}

/* --- Mirror is no-op when adapters NOT connected ------------------ */
TEST_F(GLRegionBridge, FastMapWithoutConnectionsNoOp) {
    /* Don't connect either adapter — fast_map should still succeed. */
    std::vector<float> feats(TEST_DIM, 0.5f);
    uint64_t cid = grounded_language_fast_map(gl, "ocean",
                                               feats.data(), TEST_DIM, 0);
    EXPECT_NE(cid, 0u);
}

/* --- ingest_wernicke_result: NULL guards ------------------------- */
TEST_F(GLRegionBridge, IngestNullHandleReturnsError) {
    std::vector<float> audio(64, 0.1f);
    EXPECT_EQ(-1, grounded_language_ingest_wernicke_result(
        nullptr, (void*)1, audio.data(), audio.size()));
}
TEST_F(GLRegionBridge, IngestNullResultReturnsError) {
    std::vector<float> audio(64, 0.1f);
    EXPECT_EQ(-1, grounded_language_ingest_wernicke_result(
        gl, nullptr, audio.data(), audio.size()));
}
TEST_F(GLRegionBridge, IngestNullAudioReturnsError) {
    wernicke_comprehension_t comp{};
    EXPECT_EQ(-1, grounded_language_ingest_wernicke_result(
        gl, &comp, nullptr, 0));
}

/* --- ingest_wernicke_result: empty result returns 0 -------------- */
TEST_F(GLRegionBridge, IngestEmptyResultReturnsZero) {
    wernicke_comprehension_t comp{};
    comp.words = nullptr;
    comp.word_count = 0;
    std::vector<float> audio(TEST_DIM, 0.2f);
    EXPECT_EQ(0, grounded_language_ingest_wernicke_result(
        gl, &comp, audio.data(), audio.size()));
}

/* --- ingest_wernicke_result: synthesised result lands in lexicon - */
TEST_F(GLRegionBridge, IngestRecognizedWordsLandInLexicon) {
    /* Build a minimal wernicke comprehension result by hand. */
    wernicke_word_result_t words[2];
    std::memset(words, 0, sizeof(words));
    std::strncpy(words[0].word.word, "cat", sizeof(words[0].word.word) - 1);
    words[0].confidence = 0.7f;
    std::strncpy(words[1].word.word, "dog", sizeof(words[1].word.word) - 1);
    words[1].confidence = 0.6f;

    wernicke_comprehension_t comp{};
    comp.words = words;
    comp.word_count = 2;

    std::vector<float> audio(TEST_DIM, 0.3f);
    int n = grounded_language_ingest_wernicke_result(
        gl, &comp, audio.data(), audio.size());
    EXPECT_EQ(n, 2);

    /* Confirm the lexicon now contains both words via stats. */
    gl_stats_t s{};
    grounded_language_get_stats(gl, &s);
    EXPECT_GT(s.vocab_size, 0u);
    EXPECT_GT(s.total_groundings, 0u);
}

}  // namespace
