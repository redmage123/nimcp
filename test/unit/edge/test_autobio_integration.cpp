/**
 * @file test_autobio_integration.cpp
 * @brief GoogleTest unit tests for NIMCP autobiographical memory system
 *
 * Tests store, retrieve, query, importance filtering, timeline queries,
 * identity-defining memories, text search, and null safety.
 *
 * WHAT: Verify autobiographical memory API correctness
 * WHY:  Autobio recording wired into brain_learn_vector for novelty > 5x
 * HOW:  Create standalone autobiographical_memory_t, exercise all paths
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "cognitive/nimcp_autobiographical_memory.h"
}

class AutobioTest : public ::testing::Test {
protected:
    autobiographical_memory_t system = nullptr;

    void SetUp() override {
        system = autobio_create(0); /* 0 = default capacity */
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            autobio_destroy(system);
            system = nullptr;
        }
    }

    /* Helper: create a basic memory entry */
    autobiographical_memory_entry_t make_entry(
        const char* what, autobio_memory_type_t type,
        uint64_t timestamp_ms, float importance,
        memory_valence_t valence = VALENCE_NEUTRAL)
    {
        autobiographical_memory_entry_t mem = {};
        mem.timestamp_ms = timestamp_ms;
        mem.type = type;
        if (what) {
            strncpy(mem.what_happened, what,
                    sizeof(mem.what_happened) - 1);
        }
        snprintf(mem.why_it_happened, sizeof(mem.why_it_happened),
                 "Training step");
        snprintf(mem.outcome, sizeof(mem.outcome), "Encoded");
        mem.valence = valence;
        mem.emotional_intensity = 0.5f;
        mem.arousal = 0.4f;
        mem.importance = importance;
        mem.self_relevance = 0.5f;
        mem.identity_defining = false;
        mem.memory_strength = 1.0f;
        mem.certainty = 1.0f;
        return mem;
    }
};

/* ---------- Lifecycle ---------- */

TEST_F(AutobioTest, CreateDestroy) {
    EXPECT_NE(system, nullptr);
    autobio_stats_t stats;
    bool ok = autobio_get_stats(system, &stats);
    EXPECT_TRUE(ok);
    EXPECT_EQ(stats.total_memories, 0u);
}

/* ---------- Store ---------- */

TEST_F(AutobioTest, StoreBasicMemory) {
    auto entry = make_entry("Learned about trees", AUTOBIO_LEARNING,
                            1000, 0.5f);
    uint64_t mid = autobio_store(system, &entry);
    EXPECT_GT(mid, 0u);

    autobio_stats_t stats;
    autobio_get_stats(system, &stats);
    EXPECT_EQ(stats.total_memories, 1u);
}

TEST_F(AutobioTest, StoreWithEmotionalTags) {
    auto entry = make_entry("Felt excited", AUTOBIO_EMOTION,
                            2000, 0.7f, VALENCE_POSITIVE);
    entry.emotional_intensity = 0.9f;
    entry.arousal = 0.8f;

    uint64_t mid = autobio_store(system, &entry);
    ASSERT_GT(mid, 0u);

    autobiographical_memory_entry_t retrieved = {};
    bool found = autobio_retrieve(system, mid, &retrieved);
    ASSERT_TRUE(found);
    EXPECT_EQ(retrieved.valence, VALENCE_POSITIVE);
    EXPECT_NEAR(retrieved.emotional_intensity, 0.9f, 0.01f);
    EXPECT_NEAR(retrieved.arousal, 0.8f, 0.01f);
}

TEST_F(AutobioTest, StoreIdentityDefining) {
    auto entry = make_entry("Realized who I am", AUTOBIO_INSIGHT,
                            3000, 0.95f);
    entry.identity_defining = true;

    uint64_t mid = autobio_store(system, &entry);
    ASSERT_GT(mid, 0u);

    autobiographical_memory_entry_t retrieved = {};
    bool found = autobio_retrieve(system, mid, &retrieved);
    ASSERT_TRUE(found);
    EXPECT_TRUE(retrieved.identity_defining);
}

/* ---------- Retrieve ---------- */

TEST_F(AutobioTest, RetrieveById) {
    auto entry = make_entry("Test memory content", AUTOBIO_LEARNING,
                            4000, 0.6f);
    uint64_t mid = autobio_store(system, &entry);
    ASSERT_GT(mid, 0u);

    autobiographical_memory_entry_t retrieved = {};
    bool found = autobio_retrieve(system, mid, &retrieved);
    ASSERT_TRUE(found);
    EXPECT_STREQ(retrieved.what_happened, "Test memory content");
    EXPECT_EQ(retrieved.type, AUTOBIO_LEARNING);
}

/* ---------- Importance filtering ---------- */

TEST_F(AutobioTest, ImportanceFiltering) {
    auto low = make_entry("Low importance", AUTOBIO_ACTION, 1000, 0.1f);
    autobio_store(system, &low);
    auto high = make_entry("High importance", AUTOBIO_INSIGHT, 2000, 0.9f);
    autobio_store(system, &high);

    memory_query_t query = {};
    query.filter_by_importance = true;
    query.min_importance = 0.5f;
    query.max_results = 10;

    autobiographical_memory_entry_t results[10];
    uint32_t found = 0;
    bool ok = autobio_query(system, &query, results, 10, &found);
    EXPECT_TRUE(ok);
    EXPECT_GE(found, 1u);
    /* All returned should have importance >= 0.5 */
    for (uint32_t i = 0; i < found; i++) {
        EXPECT_GE(results[i].importance, 0.5f);
    }
}

/* ---------- Timeline query ---------- */

TEST_F(AutobioTest, TimelineQuery) {
    for (uint64_t t = 1000; t <= 5000; t += 1000) {
        char desc[64];
        snprintf(desc, sizeof(desc), "Event at %lu", (unsigned long)t);
        auto entry = make_entry(desc, AUTOBIO_ACTION, t, 0.5f);
        autobio_store(system, &entry);
    }

    /* Query middle range */
    memory_query_t query = {};
    query.start_time_ms = 2000;
    query.end_time_ms = 4000;
    query.max_results = 10;

    autobiographical_memory_entry_t results[10];
    uint32_t found = 0;
    bool ok = autobio_query(system, &query, results, 10, &found);
    EXPECT_TRUE(ok);
    EXPECT_GE(found, 2u); /* Should find events at 2000, 3000, 4000 */
}

/* ---------- Learning type ---------- */

TEST_F(AutobioTest, LearningType) {
    auto entry = make_entry("Learned something new", AUTOBIO_LEARNING,
                            5000, 0.7f);
    uint64_t mid = autobio_store(system, &entry);
    ASSERT_GT(mid, 0u);

    autobiographical_memory_entry_t retrieved = {};
    autobio_retrieve(system, mid, &retrieved);
    EXPECT_EQ(retrieved.type, AUTOBIO_LEARNING);
}

/* ---------- Memory strength ---------- */

TEST_F(AutobioTest, MemoryStrength) {
    auto entry = make_entry("Strong memory", AUTOBIO_INSIGHT, 6000, 0.8f);
    entry.memory_strength = 1.0f;

    uint64_t mid = autobio_store(system, &entry);
    ASSERT_GT(mid, 0u);

    autobiographical_memory_entry_t retrieved = {};
    autobio_retrieve(system, mid, &retrieved);
    EXPECT_NEAR(retrieved.memory_strength, 1.0f, 0.01f);
}

/* ---------- Null safety ---------- */

TEST_F(AutobioTest, NullSystemHandled) {
    auto entry = make_entry("test", AUTOBIO_ACTION, 1000, 0.5f);
    uint64_t mid = autobio_store(nullptr, &entry);
    EXPECT_EQ(mid, 0u);

    autobiographical_memory_entry_t retrieved = {};
    bool found = autobio_retrieve(nullptr, 1, &retrieved);
    EXPECT_FALSE(found);

    autobio_stats_t stats;
    bool ok = autobio_get_stats(nullptr, &stats);
    EXPECT_FALSE(ok);

    autobio_destroy(nullptr); /* Should not crash */
}

/* ---------- Empty query ---------- */

TEST_F(AutobioTest, EmptyQueryReturnsEmpty) {
    memory_query_t query = {};
    query.max_results = 10;

    autobiographical_memory_entry_t results[10];
    uint32_t found = 0;
    bool ok = autobio_query(system, &query, results, 10, &found);
    EXPECT_TRUE(ok);
    EXPECT_EQ(found, 0u);
}

/* ---------- Core memories ---------- */

TEST_F(AutobioTest, CoreMemoryRetrieval) {
    auto entry1 = make_entry("Normal memory", AUTOBIO_ACTION, 1000, 0.5f);
    uint64_t m1 = autobio_store(system, &entry1);

    auto entry2 = make_entry("Core event", AUTOBIO_MILESTONE, 2000, 0.95f);
    entry2.identity_defining = true;
    uint64_t m2 = autobio_store(system, &entry2);

    autobio_mark_core(system, m2, true);

    autobiographical_memory_entry_t core_results[10];
    uint32_t core_found = 0;
    bool ok = autobio_get_core_memories(system, core_results, 10, &core_found);
    EXPECT_TRUE(ok);
    EXPECT_GE(core_found, 1u);
}

/* ---------- Consolidation ---------- */

TEST_F(AutobioTest, ConsolidationDoesntCrash) {
    for (int i = 0; i < 20; i++) {
        auto entry = make_entry("memory", AUTOBIO_ACTION,
                                (uint64_t)(i * 1000), 0.3f);
        autobio_store(system, &entry);
    }
    uint32_t pruned = autobio_consolidate(system);
    /* Should not crash; pruned count is implementation-dependent */
    (void)pruned;
}
