/**
 * @file test_grounded_language_chunker.cpp
 * @brief Unit tests for NER + shallow chunker + chinking.
 *
 * WHAT: Verify gl_ner_classify(), grounded_language_chunk(), and the
 *       chinking rules that break NPs at commas/conjunctions/verbs.
 *
 * WHY:  Chunks feed working memory + KG; bad chunks corrupt downstream
 *       reasoning silently. NER tags drive entity-graph populate; bad
 *       NER plants nonsense entities the brain can't easily delete.
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

class GLChunker : public ::testing::Test {
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

/* --- NER --------------------------------------------------------- */
TEST_F(GLChunker, NERSentenceInitialNotEntity) {
    /* Sentence-initial caps don't imply entity. */
    EXPECT_EQ(GL_ENTITY_NONE,
              gl_ner_classify("The", nullptr, true));
}
TEST_F(GLChunker, NERMidSentenceCapsIsPerson) {
    EXPECT_EQ(GL_ENTITY_PERSON,
              gl_ner_classify("John", "met", false));
}
TEST_F(GLChunker, NERAfterSentenceFinalIsNotEntity) {
    /* After "?" the next caps token is sentence-initial. */
    EXPECT_EQ(GL_ENTITY_NONE,
              gl_ner_classify("Hello", "?", false));
}
TEST_F(GLChunker, NERPlaceSuffix) {
    EXPECT_EQ(GL_ENTITY_PLACE,
              gl_ner_classify("Springville", "in", false));
    EXPECT_EQ(GL_ENTITY_PLACE,
              gl_ner_classify("Pittsburg", "from", false));
}
TEST_F(GLChunker, NEROrgAllCaps) {
    EXPECT_EQ(GL_ENTITY_ORG,
              gl_ner_classify("NASA", "the", false));
    EXPECT_EQ(GL_ENTITY_ORG,
              gl_ner_classify("IBM", "at", false));
}
TEST_F(GLChunker, NERNumber) {
    EXPECT_EQ(GL_ENTITY_NUMBER,
              gl_ner_classify("42", "is", false));
}
TEST_F(GLChunker, NERYearAsDate) {
    EXPECT_EQ(GL_ENTITY_DATE,
              gl_ner_classify("2026", "in", false));
}
TEST_F(GLChunker, NERSlashDate) {
    EXPECT_EQ(GL_ENTITY_DATE,
              gl_ner_classify("12/31/2025", "on", false));
}
TEST_F(GLChunker, NERLowercaseNotEntity) {
    EXPECT_EQ(GL_ENTITY_NONE,
              gl_ner_classify("john", "met", false));
}
TEST_F(GLChunker, NERNullSafe) {
    EXPECT_EQ(GL_ENTITY_NONE, gl_ner_classify(nullptr, "x", false));
    EXPECT_EQ(GL_ENTITY_NONE, gl_ner_classify("", "x", false));
}

/* --- Chunker: NP --------------------------------------------------- */
TEST_F(GLChunker, ChunkSimpleNP) {
    gl_chunk_t chunks[8];
    uint32_t n = 0;
    ASSERT_EQ(0, grounded_language_chunk(gl,
        "the big red ball", chunks, 8, &n));
    /* "the big red ball" → 1 NP. */
    ASSERT_GE(n, 1u);
    EXPECT_EQ(GL_CHUNK_NP, chunks[0].type);
    EXPECT_STREQ("ball", chunks[0].head_word);
}

TEST_F(GLChunker, ChunkProperNounEntity) {
    gl_chunk_t chunks[8];
    uint32_t n = 0;
    ASSERT_EQ(0, grounded_language_chunk(gl,
        "I met John yesterday", chunks, 8, &n));
    /* Should produce at least an NP whose head is "John" with PERSON tag. */
    bool found_person = false;
    for (uint32_t i = 0; i < n; i++) {
        if (chunks[i].type == GL_CHUNK_NP &&
            chunks[i].head_entity == GL_ENTITY_PERSON) {
            found_person = true;
            EXPECT_STREQ("John", chunks[i].head_word);
        }
    }
    EXPECT_TRUE(found_person);
}

TEST_F(GLChunker, ChunkVPWithAux) {
    gl_chunk_t chunks[8];
    uint32_t n = 0;
    ASSERT_EQ(0, grounded_language_chunk(gl,
        "she was running quickly", chunks, 8, &n));
    /* Expect a VP somewhere. */
    bool found_vp = false;
    for (uint32_t i = 0; i < n; i++) {
        if (chunks[i].type == GL_CHUNK_VP) found_vp = true;
    }
    EXPECT_TRUE(found_vp);
}

TEST_F(GLChunker, ChunkPP) {
    gl_chunk_t chunks[8];
    uint32_t n = 0;
    ASSERT_EQ(0, grounded_language_chunk(gl,
        "in the park", chunks, 8, &n));
    ASSERT_GE(n, 1u);
    EXPECT_EQ(GL_CHUNK_PP, chunks[0].type);
}

/* --- Chinking: comma breaks an NP run ----------------------------- */
TEST_F(GLChunker, ChinkCommaSplitsNP) {
    gl_chunk_t chunks[16];
    uint32_t n = 0;
    /* Note: "John" mid-sentence (not at index 0) so NER fires.
     * Sentence-initial caps are intentionally ambiguous → ent=NONE. */
    ASSERT_EQ(0, grounded_language_chunk(gl,
        "I met John , the doctor , yesterday",
        chunks, 16, &n));
    uint32_t np_count = 0;
    bool any_chinked = false;
    for (uint32_t i = 0; i < n; i++) {
        if (chunks[i].type == GL_CHUNK_NP) np_count++;
        if (chunks[i].chinked) any_chinked = true;
    }
    EXPECT_GE(np_count, 2u) << "comma chink should split into ≥2 NPs";
    EXPECT_TRUE(any_chinked) << "at least one NP must be flagged chinked";
}

TEST_F(GLChunker, ChinkConjunctionSplitsNP) {
    gl_chunk_t chunks[16];
    uint32_t n = 0;
    ASSERT_EQ(0, grounded_language_chunk(gl,
        "the cat and the dog", chunks, 16, &n));
    uint32_t np_count = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (chunks[i].type == GL_CHUNK_NP) np_count++;
    }
    EXPECT_GE(np_count, 2u) << "conjunction chink should split NPs";
}

/* --- Edge: empty / NULL ------------------------------------------- */
TEST_F(GLChunker, EmptyTextNoChunks) {
    gl_chunk_t chunks[4];
    uint32_t n = 99;
    ASSERT_EQ(0, grounded_language_chunk(gl, "", chunks, 4, &n));
    EXPECT_EQ(0u, n);
}
TEST_F(GLChunker, NullSafe) {
    gl_chunk_t chunks[4];
    uint32_t n = 0;
    EXPECT_EQ(-1, grounded_language_chunk(gl, nullptr, chunks, 4, &n));
    EXPECT_EQ(-1, grounded_language_chunk(gl, "x", nullptr, 4, &n));
    EXPECT_EQ(-1, grounded_language_chunk(gl, "x", chunks, 4, nullptr));
}

}  // namespace
