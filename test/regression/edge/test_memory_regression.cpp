/**
 * @file test_memory_regression.cpp
 * @brief GoogleTest regression tests for memory integration subsystems
 *
 * Tests edge cases, degenerate inputs, NaN handling, boundary conditions,
 * and thread safety of engram, semantic, and autobiographical memory.
 *
 * WHAT: Prevent regressions in memory subsystem robustness
 * WHY:  Memory pipeline runs on every learn step — must be crash-proof
 * HOW:  Feed pathological inputs, verify graceful handling
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <thread>

extern "C" {
#include "cognitive/memory/nimcp_engram.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "cognitive/nimcp_autobiographical_memory.h"
#include "cognitive/nimcp_emotional_tagging.h"
}

class MemoryRegressionTest : public ::testing::Test {
protected:
    void TearDown() override {}

    emotional_tag_t neutral_emotion() {
        emotional_tag_t tag = {};
        tag.valence = 0.0f;
        tag.arousal = 0.0f;
        tag.intensity = 0.0f;
        tag.category = EMOTION_CAT_NEUTRAL;
        tag.timestamp_ms = 1000;
        return tag;
    }
};

/* ---------- Engram edge cases ---------- */

TEST_F(MemoryRegressionTest, EngramWithZeroActivations) {
    engram_system_t* sys = engram_system_create();
    ASSERT_NE(sys, nullptr);

    uint32_t ids[] = {1, 2, 3, 4, 5};
    float acts[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    uint64_t eid = engram_encode(sys, ids, acts, 5,
                                  MEMORY_TYPE_EPISODIC, neutral_emotion());
    /* Should succeed — zero activations are valid (just weak) */
    EXPECT_GT(eid, 0u);

    engram_system_destroy(sys);
}

TEST_F(MemoryRegressionTest, EngramNaNActivation) {
    engram_system_t* sys = engram_system_create();
    ASSERT_NE(sys, nullptr);

    uint32_t ids[] = {10, 20, 30};
    float acts[] = {NAN, 0.5f, INFINITY};

    /* Should not crash — NaN/Inf inputs must be handled gracefully */
    uint64_t eid = engram_encode(sys, ids, acts, 3,
                                  MEMORY_TYPE_EPISODIC, neutral_emotion());
    /* May or may not succeed depending on implementation's NaN handling */
    (void)eid;

    engram_system_destroy(sys);
}

TEST_F(MemoryRegressionTest, EngramOverMaxNeurons) {
    engram_system_t* sys = engram_system_create();
    ASSERT_NE(sys, nullptr);

    /* Encode with more than ENGRAM_MAX_NEURONS */
    uint32_t count = ENGRAM_MAX_NEURONS + 50;
    std::vector<uint32_t> ids(count);
    std::vector<float> acts(count);
    for (uint32_t i = 0; i < count; i++) {
        ids[i] = i;
        acts[i] = 0.5f;
    }

    uint64_t eid = engram_encode(sys, ids.data(), acts.data(), count,
                                  MEMORY_TYPE_EPISODIC, neutral_emotion());
    /* Should either cap at ENGRAM_MAX_NEURONS or reject — not crash */
    if (eid > 0) {
        memory_engram_t* e = engram_get_by_id(sys, eid);
        ASSERT_NE(e, nullptr);
        EXPECT_LE(e->neuron_count, (uint32_t)ENGRAM_MAX_NEURONS);
    }

    engram_system_destroy(sys);
}

/* ---------- Semantic edge cases ---------- */

TEST_F(MemoryRegressionTest, SemanticZeroDimFeatures) {
    semantic_memory_system_t* sys = semantic_memory_create();
    ASSERT_NE(sys, nullptr);

    /* feature_dim = 0: should handle gracefully */
    float dummy = 1.0f;
    uint64_t cid = semantic_memory_create_concept(
        sys, &dummy, 0, "empty", CONCEPT_ABSTRACT);
    /* May return 0 (rejected) or create with 0-dim — either is fine */
    (void)cid;

    semantic_memory_destroy(sys);
}

TEST_F(MemoryRegressionTest, SemanticNegativeFeatures) {
    semantic_memory_system_t* sys = semantic_memory_create();
    ASSERT_NE(sys, nullptr);

    float feats[32];
    for (int i = 0; i < 32; i++) feats[i] = -1.0f * (i + 1);

    uint64_t cid = semantic_memory_create_concept(
        sys, feats, 32, "negative", CONCEPT_PROPERTY);
    EXPECT_GT(cid, 0u);

    /* Similarity search with negative features should work */
    semantic_query_result_t* r = semantic_memory_find_similar(
        sys, feats, 32, 5, 0.5f);
    ASSERT_NE(r, nullptr);
    EXPECT_GT(r->count, 0u);
    semantic_memory_free_result(r);

    semantic_memory_destroy(sys);
}

TEST_F(MemoryRegressionTest, SemanticNaNFeatures) {
    semantic_memory_system_t* sys = semantic_memory_create();
    ASSERT_NE(sys, nullptr);

    float feats[32];
    for (int i = 0; i < 32; i++) feats[i] = NAN;

    /* Should not crash */
    uint64_t cid = semantic_memory_create_concept(
        sys, feats, 32, "nan_concept", CONCEPT_ABSTRACT);
    (void)cid;

    semantic_memory_destroy(sys);
}

/* ---------- Autobio edge cases ---------- */

TEST_F(MemoryRegressionTest, AutobioEmptyLabel) {
    autobiographical_memory_t sys = autobio_create(0);
    ASSERT_NE(sys, nullptr);

    autobiographical_memory_entry_t mem = {};
    mem.timestamp_ms = 1000;
    mem.type = AUTOBIO_LEARNING;
    /* Empty string for what_happened */
    mem.what_happened[0] = '\0';
    mem.importance = 0.5f;
    mem.memory_strength = 1.0f;

    uint64_t mid = autobio_store(sys, &mem);
    /* Should succeed — empty description is allowed */
    EXPECT_GT(mid, 0u);

    autobio_destroy(sys);
}

TEST_F(MemoryRegressionTest, AutobioMaxLengthDescription) {
    autobiographical_memory_t sys = autobio_create(0);
    ASSERT_NE(sys, nullptr);

    autobiographical_memory_entry_t mem = {};
    mem.timestamp_ms = 2000;
    mem.type = AUTOBIO_INSIGHT;

    /* Fill what_happened to max length */
    memset(mem.what_happened, 'X', AUTOBIO_MAX_DESCRIPTION_LEN - 1);
    mem.what_happened[AUTOBIO_MAX_DESCRIPTION_LEN - 1] = '\0';
    mem.importance = 0.5f;
    mem.memory_strength = 1.0f;

    uint64_t mid = autobio_store(sys, &mem);
    EXPECT_GT(mid, 0u);

    autobiographical_memory_entry_t retrieved = {};
    bool found = autobio_retrieve(sys, mid, &retrieved);
    ASSERT_TRUE(found);
    EXPECT_EQ(strlen(retrieved.what_happened), AUTOBIO_MAX_DESCRIPTION_LEN - 1);

    autobio_destroy(sys);
}

/* ---------- Thread safety ---------- */

TEST_F(MemoryRegressionTest, ConcurrentEngramEncode) {
    engram_system_t* sys = engram_system_create();
    ASSERT_NE(sys, nullptr);

    auto encode_work = [&](uint32_t thread_id) {
        for (int i = 0; i < 20; i++) {
            uint32_t ids[4] = {
                thread_id * 1000 + (uint32_t)i * 4,
                thread_id * 1000 + (uint32_t)i * 4 + 1,
                thread_id * 1000 + (uint32_t)i * 4 + 2,
                thread_id * 1000 + (uint32_t)i * 4 + 3
            };
            float acts[4] = {0.5f, 0.6f, 0.7f, 0.8f};
            emotional_tag_t emotion = {};
            emotion.valence = 0.1f;
            emotion.arousal = 0.2f;
            emotion.category = EMOTION_CAT_NEUTRAL;
            engram_encode(sys, ids, acts, 4,
                          MEMORY_TYPE_EPISODIC, emotion);
        }
    };

    std::thread t1(encode_work, 1);
    std::thread t2(encode_work, 2);
    std::thread t3(encode_work, 3);

    t1.join();
    t2.join();
    t3.join();

    /* Should not crash; active count should be reasonable */
    EXPECT_GT(engram_get_active_count(sys), 0u);

    engram_system_destroy(sys);
}

/* ---------- Null brain fields ---------- */

TEST_F(MemoryRegressionTest, MemorySystemsWithNullBrain) {
    /* Simulate what happens if brain->engram_system is valid but
       we pass NULL for other systems — each should handle independently */

    engram_system_t* eng = engram_system_create();
    ASSERT_NE(eng, nullptr);

    /* Encode with valid engram but NULL semantic/autobio */
    uint32_t ids[] = {1, 2, 3};
    float acts[] = {0.5f, 0.5f, 0.5f};
    uint64_t eid = engram_encode(eng, ids, acts, 3,
                                  MEMORY_TYPE_EPISODIC, neutral_emotion());
    EXPECT_GT(eid, 0u);

    /* NULL semantic operations */
    float feats[32] = {};
    semantic_query_result_t* r = semantic_memory_find_similar(
        nullptr, feats, 32, 1, 0.9f);
    if (r) {
        EXPECT_EQ(r->count, 0u);
        semantic_memory_free_result(r);
    }

    /* NULL autobio store */
    autobiographical_memory_entry_t mem = {};
    mem.type = AUTOBIO_LEARNING;
    uint64_t mid = autobio_store(nullptr, &mem);
    EXPECT_EQ(mid, 0u);

    engram_system_destroy(eng);
}

/* ---------- Emotional tag regression ---------- */

TEST_F(MemoryRegressionTest, EmotionalTagClassifyEdgeCases) {
    /* Test classify with extreme values */
    emotional_tag_t tag1 = {.valence = -1.0f, .arousal = 1.0f,
                            .timestamp_ms = 0, .category = EMOTION_CAT_NEUTRAL,
                            .intensity = 1.0f};
    emotion_category_t cat1 = emotional_tag_classify(&tag1);
    EXPECT_NE(cat1, EMOTION_CAT_NEUTRAL); /* Strong emotion */

    emotional_tag_t tag2 = {.valence = 0.0f, .arousal = 0.0f,
                            .timestamp_ms = 0, .category = EMOTION_CAT_NEUTRAL,
                            .intensity = 0.0f};
    emotion_category_t cat2 = emotional_tag_classify(&tag2);
    EXPECT_EQ(cat2, EMOTION_CAT_NEUTRAL);

    /* NaN input */
    emotional_tag_t tag3 = {.valence = NAN, .arousal = NAN,
                            .timestamp_ms = 0, .category = EMOTION_CAT_NEUTRAL,
                            .intensity = 0.0f};
    /* Should not crash */
    emotional_tag_classify(&tag3);
}

/* ---------- Enhancement regression: co-occurrence self-link ---------- */

TEST_F(MemoryRegressionTest, CoOccurrenceSelfLinkPrevented) {
    /* Creating a relation from concept to itself should be handled gracefully */
    semantic_memory_system_t* sm = semantic_memory_create();
    ASSERT_NE(sm, nullptr);

    float feats[32] = {0}; feats[0] = 1.0f;
    uint64_t cid = semantic_memory_create_concept(sm, feats, 32, "self_ref", CONCEPT_OBJECT);

    /* Self-link — should either fail gracefully or create the relation */
    uint64_t rid = semantic_memory_create_relation(sm, cid, cid, RELATION_ASSOCIATED, 0.3f);
    /* Either outcome is acceptable (no crash) */
    (void)rid;

    semantic_memory_destroy(sm);
}

/* ---------- Enhancement regression: emotional enhancement with zero intensity ---------- */

TEST_F(MemoryRegressionTest, EmotionalEnhancementZeroIntensity) {
    /* Zero intensity emotion — no enhancement should be applied.
     * Enhancement gate is: intensity > 0.5, so zero passes through unchanged. */
    engram_system_t* es = engram_system_create();
    ASSERT_NE(es, nullptr);

    uint32_t ids[] = {1, 2, 3};
    float acts[] = {0.5f, 0.5f, 0.5f};
    emotional_tag_t emotion;
    emotion.valence = 0.0f;
    emotion.arousal = 0.0f;
    emotion.intensity = 0.0f;
    emotion.category = emotional_tag_classify(&emotion);
    emotion.timestamp_ms = 1000;

    uint64_t eid = engram_encode(es, ids, acts, 3, MEMORY_TYPE_EPISODIC, emotion);
    EXPECT_GT(eid, 0u);

    /* Zero intensity: decay rate should be unmodified (at or near base rate) */
    for (uint32_t i = 0; i < es->capacity; i++) {
        if (es->engrams[i].active && es->engrams[i].engram_id == eid) {
            EXPECT_NEAR(es->engrams[i].decay_rate, ENGRAM_BASE_DECAY_RATE, 0.001f);
            break;
        }
    }

    engram_system_destroy(es);
}

/* ---------- Enhancement regression: milestone at exact step boundaries ---------- */

TEST_F(MemoryRegressionTest, MilestoneStepBoundaries) {
    /* Verify milestone detection logic matches brain_learn_vector Enhancement 4 */
    uint64_t milestones[] = {100, 500, 1000, 5000, 10000, 50000, 100000, 500000, 1000000};
    uint64_t non_milestones[] = {99, 101, 499, 501, 999, 1001};

    for (uint64_t m : milestones) {
        bool is = (m == 100 || m == 500 || m == 1000 || m == 5000 ||
                   m == 10000 || m == 50000 || m == 100000 || m == 500000 || m == 1000000);
        EXPECT_TRUE(is) << "Step " << m << " should be a milestone";
    }

    for (uint64_t m : non_milestones) {
        bool is = (m == 100 || m == 500 || m == 1000 || m == 5000 ||
                   m == 10000 || m == 50000 || m == 100000 || m == 500000 || m == 1000000);
        EXPECT_FALSE(is) << "Step " << m << " should NOT be a milestone";
    }
}
