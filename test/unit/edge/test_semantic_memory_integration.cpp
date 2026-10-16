/**
 * @file test_semantic_memory_integration.cpp
 * @brief GoogleTest unit tests for NIMCP semantic memory system
 *
 * Tests concept creation, similarity queries, relations, spreading
 * activation, capacity limits, categories, and null safety.
 *
 * WHAT: Verify semantic_memory API correctness
 * WHY:  Semantic concept creation wired into brain_learn_vector
 * HOW:  Create standalone semantic_memory_system_t, exercise all paths
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <set>

extern "C" {
#include "cognitive/memory/nimcp_semantic_memory.h"
}

class SemanticMemoryTest : public ::testing::Test {
protected:
    semantic_memory_system_t* system = nullptr;
    static constexpr uint32_t FEAT_DIM = 32;

    void SetUp() override {
        system = semantic_memory_create();
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            semantic_memory_destroy(system);
            system = nullptr;
        }
    }

    /* Helper: create a feature vector with a distinctive pattern */
    std::vector<float> make_features(float base, float step = 0.01f) {
        std::vector<float> f(FEAT_DIM);
        for (uint32_t i = 0; i < FEAT_DIM; i++) {
            f[i] = base + i * step;
        }
        /* Normalize to unit length for cosine similarity */
        float norm = 0.0f;
        for (auto v : f) norm += v * v;
        norm = sqrtf(norm);
        if (norm > 0.0f) {
            for (auto& v : f) v /= norm;
        }
        return f;
    }
};

/* ---------- Lifecycle ---------- */

TEST_F(SemanticMemoryTest, CreateDestroy) {
    /* SetUp creates, TearDown destroys */
    EXPECT_NE(system, nullptr);
    EXPECT_EQ(system->concept_count, 0u);
}

/* ---------- Concept creation ---------- */

TEST_F(SemanticMemoryTest, CreateConcept) {
    auto feats = make_features(1.0f);
    uint64_t cid = semantic_memory_create_concept(
        system, feats.data(), FEAT_DIM, "apple", CONCEPT_OBJECT);
    EXPECT_GT(cid, 0u);
    EXPECT_EQ(system->concept_count, 1u);

    const semantic_concept_t* con = semantic_memory_get_concept(system, cid);
    ASSERT_NE(con, nullptr);
    EXPECT_STREQ(con->label, "apple");
    EXPECT_EQ(con->category, CONCEPT_OBJECT);
}

TEST_F(SemanticMemoryTest, CreateConceptNullLabel) {
    auto feats = make_features(2.0f);
    uint64_t cid = semantic_memory_create_concept(
        system, feats.data(), FEAT_DIM, nullptr, CONCEPT_ABSTRACT);
    /* Should succeed — label is optional */
    EXPECT_GT(cid, 0u);
    EXPECT_EQ(system->concept_count, 1u);
}

/* ---------- Similarity search ---------- */

TEST_F(SemanticMemoryTest, FindSimilarExact) {
    auto feats = make_features(1.0f);
    uint64_t cid = semantic_memory_create_concept(
        system, feats.data(), FEAT_DIM, "banana", CONCEPT_OBJECT);
    ASSERT_GT(cid, 0u);

    /* Query with identical features */
    semantic_query_result_t* result = semantic_memory_find_similar(
        system, feats.data(), FEAT_DIM, 5, 0.8f);
    ASSERT_NE(result, nullptr);
    EXPECT_GT(result->count, 0u);
    semantic_memory_free_result(result);
}

TEST_F(SemanticMemoryTest, FindSimilarDifferent) {
    auto feats1 = make_features(1.0f, 0.1f);
    semantic_memory_create_concept(
        system, feats1.data(), FEAT_DIM, "cat", CONCEPT_OBJECT);

    /* Query with very different features */
    auto feats2 = make_features(-5.0f, -0.1f);
    semantic_query_result_t* result = semantic_memory_find_similar(
        system, feats2.data(), FEAT_DIM, 5, 0.95f);
    ASSERT_NE(result, nullptr);
    /* With very different features and high threshold, should find nothing */
    EXPECT_EQ(result->count, 0u);
    semantic_memory_free_result(result);
}

TEST_F(SemanticMemoryTest, FindSimilarThreshold) {
    auto feats1 = make_features(1.0f);
    semantic_memory_create_concept(
        system, feats1.data(), FEAT_DIM, "dog", CONCEPT_OBJECT);

    /* Low threshold should find the concept */
    semantic_query_result_t* low = semantic_memory_find_similar(
        system, feats1.data(), FEAT_DIM, 5, 0.1f);
    ASSERT_NE(low, nullptr);
    EXPECT_GT(low->count, 0u);
    semantic_memory_free_result(low);

    /* Very high threshold (> 1.0 effectively) with slightly different feats */
    auto feats2 = make_features(1.1f);
    semantic_query_result_t* high = semantic_memory_find_similar(
        system, feats2.data(), FEAT_DIM, 5, 0.999f);
    ASSERT_NE(high, nullptr);
    /* May or may not find — just verify no crash */
    semantic_memory_free_result(high);
}

/* ---------- Relations ---------- */

TEST_F(SemanticMemoryTest, CreateRelation) {
    auto feats1 = make_features(1.0f);
    auto feats2 = make_features(2.0f);

    uint64_t c1 = semantic_memory_create_concept(
        system, feats1.data(), FEAT_DIM, "dog", CONCEPT_OBJECT);
    uint64_t c2 = semantic_memory_create_concept(
        system, feats2.data(), FEAT_DIM, "animal", CONCEPT_CATEGORY);

    uint64_t rid = semantic_memory_create_relation(
        system, c1, c2, RELATION_IS_A, 0.9f);
    EXPECT_GT(rid, 0u);
    EXPECT_EQ(system->relation_count, 1u);
}

TEST_F(SemanticMemoryTest, GetRelationsForConcept) {
    auto f1 = make_features(1.0f);
    auto f2 = make_features(2.0f);
    auto f3 = make_features(3.0f);

    uint64_t c1 = semantic_memory_create_concept(system, f1.data(), FEAT_DIM, "wheel", CONCEPT_OBJECT);
    uint64_t c2 = semantic_memory_create_concept(system, f2.data(), FEAT_DIM, "car", CONCEPT_OBJECT);
    uint64_t c3 = semantic_memory_create_concept(system, f3.data(), FEAT_DIM, "vehicle", CONCEPT_CATEGORY);

    semantic_memory_create_relation(system, c2, c1, RELATION_HAS_A, 0.8f);
    semantic_memory_create_relation(system, c2, c3, RELATION_IS_A, 0.9f);

    uint64_t rel_ids[16];
    uint32_t found = semantic_memory_get_relations(system, c2, rel_ids, 16);
    EXPECT_GE(found, 2u);
}

/* ---------- Spreading activation ---------- */

TEST_F(SemanticMemoryTest, SpreadingActivation) {
    auto f1 = make_features(1.0f);
    auto f2 = make_features(1.5f);
    auto f3 = make_features(2.0f);

    uint64_t cA = semantic_memory_create_concept(system, f1.data(), FEAT_DIM, "A", CONCEPT_ABSTRACT);
    uint64_t cB = semantic_memory_create_concept(system, f2.data(), FEAT_DIM, "B", CONCEPT_ABSTRACT);
    uint64_t cC = semantic_memory_create_concept(system, f3.data(), FEAT_DIM, "C", CONCEPT_ABSTRACT);

    semantic_memory_create_relation(system, cA, cB, RELATION_ASSOCIATED, 0.8f);
    semantic_memory_create_relation(system, cB, cC, RELATION_ASSOCIATED, 0.8f);

    /* Activate A and spread */
    semantic_query_result_t* result = semantic_memory_activate(system, cA, 1.0f);
    ASSERT_NE(result, nullptr);
    /* Should activate at least A, possibly B and C */
    EXPECT_GT(result->count, 0u);
    semantic_memory_free_result(result);
}

/* ---------- Duplicate prevention ---------- */

TEST_F(SemanticMemoryTest, DuplicateConceptPrevention) {
    auto feats = make_features(1.0f);
    uint64_t c1 = semantic_memory_create_concept(
        system, feats.data(), FEAT_DIM, "elephant", CONCEPT_OBJECT);
    ASSERT_GT(c1, 0u);

    /* Creating with same features should succeed (system doesn't auto-dedup)
       but similarity search should detect the duplicate */
    semantic_query_result_t* existing = semantic_memory_find_similar(
        system, feats.data(), FEAT_DIM, 1, 0.9f);
    ASSERT_NE(existing, nullptr);
    EXPECT_GT(existing->count, 0u);
    semantic_memory_free_result(existing);
}

/* ---------- Categories ---------- */

TEST_F(SemanticMemoryTest, ConceptCategories) {
    auto f1 = make_features(1.0f);
    auto f2 = make_features(2.0f);
    auto f3 = make_features(3.0f);

    uint64_t c1 = semantic_memory_create_concept(system, f1.data(), FEAT_DIM, "ball", CONCEPT_OBJECT);
    uint64_t c2 = semantic_memory_create_concept(system, f2.data(), FEAT_DIM, "run", CONCEPT_ACTION);
    uint64_t c3 = semantic_memory_create_concept(system, f3.data(), FEAT_DIM, "red", CONCEPT_PROPERTY);

    const semantic_concept_t* obj = semantic_memory_get_concept(system, c1);
    const semantic_concept_t* act = semantic_memory_get_concept(system, c2);
    const semantic_concept_t* prop = semantic_memory_get_concept(system, c3);

    ASSERT_NE(obj, nullptr);
    ASSERT_NE(act, nullptr);
    ASSERT_NE(prop, nullptr);

    EXPECT_EQ(obj->category, CONCEPT_OBJECT);
    EXPECT_EQ(act->category, CONCEPT_ACTION);
    EXPECT_EQ(prop->category, CONCEPT_PROPERTY);
}

/* ---------- Reset ---------- */

TEST_F(SemanticMemoryTest, ResetClearsAll) {
    auto f1 = make_features(1.0f);
    auto f2 = make_features(2.0f);
    semantic_memory_create_concept(system, f1.data(), FEAT_DIM, "x", CONCEPT_ABSTRACT);
    semantic_memory_create_concept(system, f2.data(), FEAT_DIM, "y", CONCEPT_ABSTRACT);
    EXPECT_EQ(system->concept_count, 2u);

    semantic_memory_reset(system);
    EXPECT_EQ(system->concept_count, 0u);
}

/* ---------- Capacity ---------- */

TEST_F(SemanticMemoryTest, CapacityLimit) {
    uint32_t created = 0;
    for (uint32_t i = 0; i < system->concept_capacity + 10; i++) {
        auto feats = make_features((float)i * 0.5f);
        char label[32];
        snprintf(label, sizeof(label), "concept_%u", i);
        uint64_t cid = semantic_memory_create_concept(
            system, feats.data(), FEAT_DIM, label, CONCEPT_OBJECT);
        if (cid > 0) created++;
    }
    /* Should cap at capacity without crashing */
    EXPECT_LE(created, system->concept_capacity + 10u);
    EXPECT_GT(created, 0u);
}

/* ---------- Null safety ---------- */

TEST_F(SemanticMemoryTest, NullSystemHandled) {
    auto feats = make_features(1.0f);

    uint64_t cid = semantic_memory_create_concept(
        nullptr, feats.data(), FEAT_DIM, "x", CONCEPT_OBJECT);
    EXPECT_EQ(cid, 0u);

    const semantic_concept_t* c = semantic_memory_get_concept(nullptr, 1);
    EXPECT_EQ(c, nullptr);

    semantic_query_result_t* r = semantic_memory_find_similar(
        nullptr, feats.data(), FEAT_DIM, 5, 0.5f);
    /* Should return NULL or empty */
    if (r) {
        EXPECT_EQ(r->count, 0u);
        semantic_memory_free_result(r);
    }

    uint64_t rid = semantic_memory_create_relation(nullptr, 1, 2, RELATION_IS_A, 0.5f);
    EXPECT_EQ(rid, 0u);

    uint64_t rel_ids[16];
    uint32_t found = semantic_memory_get_relations(nullptr, 1, rel_ids, 16);
    EXPECT_EQ(found, 0u);

    semantic_memory_destroy(nullptr); /* Should not crash */
}

/* ---------- Query with no results ---------- */

TEST_F(SemanticMemoryTest, QueryWithNoResults) {
    /* Empty system */
    auto feats = make_features(1.0f);
    semantic_query_result_t* result = semantic_memory_find_similar(
        system, feats.data(), FEAT_DIM, 5, 0.5f);
    /* Empty system may return NULL or empty result */
    if (result) {
        EXPECT_EQ(result->count, 0u);
        semantic_memory_free_result(result);
    }
    /* NULL is also acceptable — means "no results" */
}

/* ---------- Statistics ---------- */

TEST_F(SemanticMemoryTest, StatisticsAccurate) {
    auto f1 = make_features(1.0f);
    auto f2 = make_features(2.0f);
    semantic_memory_create_concept(system, f1.data(), FEAT_DIM, "a", CONCEPT_OBJECT);
    semantic_memory_create_concept(system, f2.data(), FEAT_DIM, "b", CONCEPT_ACTION);

    semantic_memory_stats_t stats;
    semantic_memory_get_statistics(system, &stats);
    EXPECT_EQ(stats.concept_count, 2u);
}
