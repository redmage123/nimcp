/**
 * @file test_inner_dialogue_unit.cpp
 * @brief Unit tests for Inner Dialogue Engine — Turn, Perspective, Convergence, Engine
 *
 * WHAT: Comprehensive unit tests for all four inner dialogue modules
 * WHY:  Verify each function in isolation before integration
 * HOW:  GoogleTest fixtures with table-driven tests where appropriate
 *
 * MODULES TESTED:
 *   - Turn History (circular buffer, stats, entropy, similarity)
 *   - Perspective Registry (registration, scheduling, builtins)
 *   - Convergence Analysis (agreement, deadlock, rumination, emotion, entropy)
 *   - Engine Lifecycle (create/destroy, setters, start/step/run/cancel, queries)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>

extern "C" {
#include "cognitive/inner_dialogue/nimcp_inner_dialogue.h"
#include "cognitive/inner_dialogue/nimcp_inner_dialogue_turn.h"
#include "cognitive/inner_dialogue/nimcp_inner_dialogue_perspective.h"
#include "cognitive/inner_dialogue/nimcp_inner_dialogue_convergence.h"
}

/* ============================================================================
 * Helper: Create a turn with specified fields
 * ============================================================================ */
static inner_dialogue_turn_t make_turn(uint32_t perspective_idx,
                                        dialogue_act_t act,
                                        const char* content,
                                        float confidence,
                                        float relevance,
                                        float novelty,
                                        float agreement,
                                        float valence) {
    inner_dialogue_turn_t t;
    memset(&t, 0, sizeof(t));
    t.perspective_idx = perspective_idx;
    t.act = act;
    if (content) {
        strncpy(t.content, content, INNER_DIALOGUE_TURN_MAX_CONTENT - 1);
        t.content_len = (uint32_t)strlen(content);
    }
    t.confidence = confidence;
    t.relevance = relevance;
    t.novelty = novelty;
    t.agreement_with_prior = agreement;
    t.emotional_valence = valence;
    return t;
}

/* Helper: stub formulate callback that returns an ASSERT turn */
static bool stub_formulate(const perspective_turn_context_t* ctx,
                           inner_dialogue_turn_t* output) {
    (void)ctx;
    memset(output, 0, sizeof(*output));
    output->act = DIALOGUE_ACT_ASSERT;
    strncpy(output->content, "stub assertion", INNER_DIALOGUE_TURN_MAX_CONTENT - 1);
    output->content_len = 14;
    output->confidence = 0.7f;
    output->relevance = 0.8f;
    output->novelty = 0.5f;
    output->agreement_with_prior = 0.6f;
    output->emotional_valence = 0.0f;
    return true;
}

/* Helper: stub formulate that always skips */
static bool stub_formulate_skip(const perspective_turn_context_t* ctx,
                                inner_dialogue_turn_t* output) {
    (void)ctx;
    (void)output;
    return false;
}

/* Helper: stub relevance returning high relevance */
static float stub_relevance_high(const perspective_turn_context_t* ctx) {
    (void)ctx;
    return 0.9f;
}

/* Helper: stub relevance returning low relevance */
static float stub_relevance_low(const perspective_turn_context_t* ctx) {
    (void)ctx;
    return 0.05f;
}

/* ============================================================================
 * TEST SUITE 1: Turn History
 * ============================================================================ */

class TurnHistoryTest : public ::testing::Test {
protected:
    inner_dialogue_turn_history_t* history = nullptr;

    void SetUp() override {
        history = inner_dialogue_turn_history_create();
        ASSERT_NE(history, nullptr);
    }

    void TearDown() override {
        inner_dialogue_turn_history_destroy(history);
    }
};

TEST_F(TurnHistoryTest, CreateReturnsNonNull) {
    /* Covered by SetUp ASSERT */
    EXPECT_EQ(inner_dialogue_turn_history_count(history), 0u);
}

TEST_F(TurnHistoryTest, DestroyNullSafe) {
    inner_dialogue_turn_history_destroy(nullptr);
    /* No crash = pass */
}

TEST_F(TurnHistoryTest, ResetClearsHistory) {
    inner_dialogue_turn_t turn = make_turn(0, DIALOGUE_ACT_ASSERT, "test", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
    inner_dialogue_turn_history_record(history, &turn);
    ASSERT_GT(inner_dialogue_turn_history_count(history), 0u);

    int rc = inner_dialogue_turn_history_reset(history);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(inner_dialogue_turn_history_count(history), 0u);
}

TEST_F(TurnHistoryTest, ResetNullReturnsError) {
    int rc = inner_dialogue_turn_history_reset(nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(TurnHistoryTest, RecordIncrementsCount) {
    inner_dialogue_turn_t turn = make_turn(0, DIALOGUE_ACT_ASSERT, "first turn", 0.8f, 0.7f, 0.6f, 0.5f, 0.1f);
    int id = inner_dialogue_turn_history_record(history, &turn);
    EXPECT_GE(id, 0);
    EXPECT_EQ(inner_dialogue_turn_history_count(history), 1u);
}

TEST_F(TurnHistoryTest, RecordMultipleTurns) {
    for (uint32_t i = 0; i < 10; i++) {
        inner_dialogue_turn_t turn = make_turn(i % 3, DIALOGUE_ACT_ASSERT, "turn", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
        int id = inner_dialogue_turn_history_record(history, &turn);
        EXPECT_GE(id, 0) << "Failed to record turn " << i;
    }
    EXPECT_EQ(inner_dialogue_turn_history_count(history), 10u);
}

TEST_F(TurnHistoryTest, RecordNullHistoryReturnsError) {
    inner_dialogue_turn_t turn = make_turn(0, DIALOGUE_ACT_ASSERT, "test", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
    int id = inner_dialogue_turn_history_record(nullptr, &turn);
    EXPECT_EQ(id, -1);
}

TEST_F(TurnHistoryTest, RecordNullTurnReturnsError) {
    int id = inner_dialogue_turn_history_record(history, nullptr);
    EXPECT_EQ(id, -1);
}

TEST_F(TurnHistoryTest, GetLatestReturnsLastRecorded) {
    inner_dialogue_turn_t t1 = make_turn(0, DIALOGUE_ACT_ASSERT, "first", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
    inner_dialogue_turn_t t2 = make_turn(1, DIALOGUE_ACT_QUESTION, "second", 0.6f, 0.6f, 0.6f, 0.6f, 0.1f);
    inner_dialogue_turn_history_record(history, &t1);
    inner_dialogue_turn_history_record(history, &t2);

    const inner_dialogue_turn_t* latest = inner_dialogue_turn_history_get_latest(history);
    ASSERT_NE(latest, nullptr);
    EXPECT_EQ(latest->act, DIALOGUE_ACT_QUESTION);
    EXPECT_STREQ(latest->content, "second");
}

TEST_F(TurnHistoryTest, GetLatestOnEmptyReturnsNull) {
    const inner_dialogue_turn_t* latest = inner_dialogue_turn_history_get_latest(history);
    EXPECT_EQ(latest, nullptr);
}

TEST_F(TurnHistoryTest, GetLatestNullHistoryReturnsNull) {
    const inner_dialogue_turn_t* latest = inner_dialogue_turn_history_get_latest(nullptr);
    EXPECT_EQ(latest, nullptr);
}

TEST_F(TurnHistoryTest, GetAtReverseIndex) {
    for (uint32_t i = 0; i < 5; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "turn_%u", i);
        inner_dialogue_turn_t turn = make_turn(i, DIALOGUE_ACT_ASSERT, buf, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
        inner_dialogue_turn_history_record(history, &turn);
    }

    /* Index 0 = most recent (turn_4), index 4 = oldest (turn_0) */
    const inner_dialogue_turn_t* newest = inner_dialogue_turn_history_get_at(history, 0);
    ASSERT_NE(newest, nullptr);
    EXPECT_STREQ(newest->content, "turn_4");

    const inner_dialogue_turn_t* oldest = inner_dialogue_turn_history_get_at(history, 4);
    ASSERT_NE(oldest, nullptr);
    EXPECT_STREQ(oldest->content, "turn_0");
}

TEST_F(TurnHistoryTest, GetAtOutOfRangeReturnsNull) {
    inner_dialogue_turn_t turn = make_turn(0, DIALOGUE_ACT_ASSERT, "only", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
    inner_dialogue_turn_history_record(history, &turn);
    EXPECT_EQ(inner_dialogue_turn_history_get_at(history, 1), nullptr);
    EXPECT_EQ(inner_dialogue_turn_history_get_at(history, 100), nullptr);
}

TEST_F(TurnHistoryTest, GetByIdFindsCorrectTurn) {
    inner_dialogue_turn_t t1 = make_turn(0, DIALOGUE_ACT_ASSERT, "findme", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
    int id = inner_dialogue_turn_history_record(history, &t1);
    ASSERT_GE(id, 0);

    const inner_dialogue_turn_t* found = inner_dialogue_turn_history_get_by_id(history, (uint32_t)id);
    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found->content, "findme");
}

TEST_F(TurnHistoryTest, GetByIdNotFoundReturnsNull) {
    EXPECT_EQ(inner_dialogue_turn_history_get_by_id(history, 9999), nullptr);
}

TEST_F(TurnHistoryTest, CountNullReturnsZero) {
    EXPECT_EQ(inner_dialogue_turn_history_count(nullptr), 0u);
}

TEST_F(TurnHistoryTest, CircularBufferWraps) {
    /* Fill beyond capacity; old entries should be overwritten */
    for (uint32_t i = 0; i < INNER_DIALOGUE_MAX_HISTORY + 10; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "turn_%u", i);
        inner_dialogue_turn_t turn = make_turn(i % 4, DIALOGUE_ACT_ASSERT, buf, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
        inner_dialogue_turn_history_record(history, &turn);
    }

    /* Count should be capped at capacity */
    EXPECT_LE(inner_dialogue_turn_history_count(history), (uint32_t)INNER_DIALOGUE_MAX_HISTORY);

    /* Most recent should be the last recorded */
    const inner_dialogue_turn_t* latest = inner_dialogue_turn_history_get_latest(history);
    ASSERT_NE(latest, nullptr);
    char expected[32];
    snprintf(expected, sizeof(expected), "turn_%u", INNER_DIALOGUE_MAX_HISTORY + 9);
    EXPECT_STREQ(latest->content, expected);
}

TEST_F(TurnHistoryTest, GetStatsPopulatesCorrectly) {
    inner_dialogue_turn_t t1 = make_turn(0, DIALOGUE_ACT_ASSERT, "a", 0.8f, 0.6f, 0.4f, 0.5f, 0.0f);
    inner_dialogue_turn_t t2 = make_turn(1, DIALOGUE_ACT_QUESTION, "b", 0.6f, 0.8f, 0.6f, 0.7f, 0.2f);
    inner_dialogue_turn_history_record(history, &t1);
    inner_dialogue_turn_history_record(history, &t2);

    inner_dialogue_turn_history_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int rc = inner_dialogue_turn_history_get_stats(history, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.current_count, 2u);
    EXPECT_GT(stats.act_counts[DIALOGUE_ACT_ASSERT], 0u);
    EXPECT_GT(stats.act_counts[DIALOGUE_ACT_QUESTION], 0u);
}

TEST_F(TurnHistoryTest, GetStatsNullReturnsError) {
    inner_dialogue_turn_history_stats_t stats;
    EXPECT_NE(inner_dialogue_turn_history_get_stats(nullptr, &stats), 0);
    EXPECT_NE(inner_dialogue_turn_history_get_stats(history, nullptr), 0);
}

TEST_F(TurnHistoryTest, ActEntropyMaxForUniformDistribution) {
    /* Record one of each act type for max entropy */
    for (int a = 0; a < DIALOGUE_ACT_COUNT; a++) {
        inner_dialogue_turn_t turn = make_turn(0, (dialogue_act_t)a, "diverse", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
        inner_dialogue_turn_history_record(history, &turn);
    }

    float entropy = inner_dialogue_turn_history_act_entropy(history, 0);
    EXPECT_GT(entropy, 0.0f);
    /* Max entropy for 10 acts = log2(10) ≈ 3.32 bits */
    float max_entropy = log2f((float)DIALOGUE_ACT_COUNT);
    EXPECT_NEAR(entropy, max_entropy, 0.1f);
}

TEST_F(TurnHistoryTest, ActEntropyZeroForSingleAct) {
    /* All same act = zero entropy */
    for (int i = 0; i < 5; i++) {
        inner_dialogue_turn_t turn = make_turn(0, DIALOGUE_ACT_ASSERT, "same", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
        inner_dialogue_turn_history_record(history, &turn);
    }

    float entropy = inner_dialogue_turn_history_act_entropy(history, 0);
    EXPECT_NEAR(entropy, 0.0f, 0.01f);
}

TEST_F(TurnHistoryTest, ActEntropyNullReturnsNegative) {
    float entropy = inner_dialogue_turn_history_act_entropy(nullptr, 0);
    EXPECT_LT(entropy, 0.0f);
}

TEST_F(TurnHistoryTest, ContentSimilarityIdentical) {
    inner_dialogue_turn_t t = make_turn(0, DIALOGUE_ACT_ASSERT, "hello world test", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
    float sim = inner_dialogue_turn_content_similarity(&t, &t);
    EXPECT_NEAR(sim, 1.0f, 0.01f);
}

TEST_F(TurnHistoryTest, ContentSimilarityDifferent) {
    inner_dialogue_turn_t t1 = make_turn(0, DIALOGUE_ACT_ASSERT, "alpha beta gamma", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
    inner_dialogue_turn_t t2 = make_turn(0, DIALOGUE_ACT_ASSERT, "delta epsilon zeta", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
    float sim = inner_dialogue_turn_content_similarity(&t1, &t2);
    EXPECT_NEAR(sim, 0.0f, 0.01f);
}

TEST_F(TurnHistoryTest, ContentSimilarityPartial) {
    inner_dialogue_turn_t t1 = make_turn(0, DIALOGUE_ACT_ASSERT, "the quick brown fox", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
    inner_dialogue_turn_t t2 = make_turn(0, DIALOGUE_ACT_ASSERT, "the slow brown dog", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
    float sim = inner_dialogue_turn_content_similarity(&t1, &t2);
    EXPECT_GT(sim, 0.0f);
    EXPECT_LT(sim, 1.0f);
}

TEST_F(TurnHistoryTest, ContentSimilarityNullReturnsError) {
    inner_dialogue_turn_t t = make_turn(0, DIALOGUE_ACT_ASSERT, "test", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
    EXPECT_LT(inner_dialogue_turn_content_similarity(nullptr, &t), 0.0f);
    EXPECT_LT(inner_dialogue_turn_content_similarity(&t, nullptr), 0.0f);
}

/* ============================================================================
 * TEST: dialogue_act_to_string
 * ============================================================================ */

TEST(DialogueActStringTest, AllActsHaveNames) {
    for (int a = 0; a < DIALOGUE_ACT_COUNT; a++) {
        const char* s = dialogue_act_to_string((dialogue_act_t)a);
        ASSERT_NE(s, nullptr) << "act " << a << " returned NULL";
        EXPECT_STRNE(s, "UNKNOWN") << "act " << a << " returned UNKNOWN";
    }
}

TEST(DialogueActStringTest, InvalidActReturnsUnknown) {
    const char* s = dialogue_act_to_string((dialogue_act_t)999);
    ASSERT_NE(s, nullptr);
    EXPECT_STREQ(s, "UNKNOWN");
}

/* ============================================================================
 * TEST SUITE 2: Perspective Registry
 * ============================================================================ */

class PerspectiveRegistryTest : public ::testing::Test {
protected:
    inner_dialogue_perspective_registry_t registry;

    void SetUp() override {
        int rc = inner_dialogue_perspective_registry_init(&registry);
        ASSERT_EQ(rc, 0);
    }
};

TEST_F(PerspectiveRegistryTest, InitSetsCountToZero) {
    EXPECT_EQ(registry.count, 0u);
}

TEST_F(PerspectiveRegistryTest, InitNullReturnsError) {
    int rc = inner_dialogue_perspective_registry_init(nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(PerspectiveRegistryTest, RegisterValidPerspective) {
    inner_dialogue_perspective_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = PERSPECTIVE_ANALYTICAL;
    strncpy(desc.name, "Analytical", sizeof(desc.name) - 1);
    desc.base_priority = 0.8f;
    desc.formulate = stub_formulate;

    int rc = inner_dialogue_perspective_register(&registry, &desc);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(inner_dialogue_perspective_count(&registry), 1u);
}

TEST_F(PerspectiveRegistryTest, RegisterNullFormulateReturnsError) {
    inner_dialogue_perspective_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = PERSPECTIVE_ANALYTICAL;
    strncpy(desc.name, "NoFormulate", sizeof(desc.name) - 1);
    desc.formulate = nullptr;

    int rc = inner_dialogue_perspective_register(&registry, &desc);
    EXPECT_NE(rc, 0);
}

TEST_F(PerspectiveRegistryTest, RegisterDuplicateTypeReturnsError) {
    inner_dialogue_perspective_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = PERSPECTIVE_EMOTIONAL;
    strncpy(desc.name, "Emotional", sizeof(desc.name) - 1);
    desc.formulate = stub_formulate;

    EXPECT_EQ(inner_dialogue_perspective_register(&registry, &desc), 0);
    EXPECT_NE(inner_dialogue_perspective_register(&registry, &desc), 0);
}

TEST_F(PerspectiveRegistryTest, RegisterUpToMax) {
    for (uint32_t i = 0; i < INNER_DIALOGUE_MAX_PERSPECTIVES; i++) {
        inner_dialogue_perspective_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.type = (perspective_type_t)(PERSPECTIVE_CUSTOM_START + i);
        snprintf(desc.name, sizeof(desc.name), "custom_%u", i);
        desc.base_priority = 0.5f;
        desc.formulate = stub_formulate;

        int rc = inner_dialogue_perspective_register(&registry, &desc);
        EXPECT_EQ(rc, 0) << "Failed at slot " << i;
    }
    EXPECT_EQ(inner_dialogue_perspective_count(&registry), (uint32_t)INNER_DIALOGUE_MAX_PERSPECTIVES);
}

TEST_F(PerspectiveRegistryTest, RegisterBeyondMaxReturnsError) {
    for (uint32_t i = 0; i < INNER_DIALOGUE_MAX_PERSPECTIVES; i++) {
        inner_dialogue_perspective_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.type = (perspective_type_t)(PERSPECTIVE_CUSTOM_START + i);
        snprintf(desc.name, sizeof(desc.name), "custom_%u", i);
        desc.formulate = stub_formulate;
        inner_dialogue_perspective_register(&registry, &desc);
    }

    inner_dialogue_perspective_desc_t extra;
    memset(&extra, 0, sizeof(extra));
    extra.type = (perspective_type_t)(PERSPECTIVE_CUSTOM_START + INNER_DIALOGUE_MAX_PERSPECTIVES);
    strncpy(extra.name, "overflow", sizeof(extra.name) - 1);
    extra.formulate = stub_formulate;

    int rc = inner_dialogue_perspective_register(&registry, &extra);
    EXPECT_NE(rc, 0);
}

TEST_F(PerspectiveRegistryTest, UnregisterRemovesPerspective) {
    inner_dialogue_perspective_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = PERSPECTIVE_CRITICAL;
    strncpy(desc.name, "Critical", sizeof(desc.name) - 1);
    desc.formulate = stub_formulate;

    inner_dialogue_perspective_register(&registry, &desc);
    EXPECT_EQ(inner_dialogue_perspective_count(&registry), 1u);

    int rc = inner_dialogue_perspective_unregister(&registry, PERSPECTIVE_CRITICAL);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(inner_dialogue_perspective_count(&registry), 0u);
}

TEST_F(PerspectiveRegistryTest, UnregisterNotFoundReturnsError) {
    int rc = inner_dialogue_perspective_unregister(&registry, PERSPECTIVE_CREATIVE);
    EXPECT_NE(rc, 0);
}

TEST_F(PerspectiveRegistryTest, FindReturnsPerspective) {
    inner_dialogue_perspective_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = PERSPECTIVE_MEMORY;
    strncpy(desc.name, "Memory", sizeof(desc.name) - 1);
    desc.base_priority = 0.6f;
    desc.formulate = stub_formulate;

    inner_dialogue_perspective_register(&registry, &desc);
    const inner_dialogue_perspective_entry_t* entry = inner_dialogue_perspective_find(&registry, PERSPECTIVE_MEMORY);
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->desc.name, "Memory");
    EXPECT_FLOAT_EQ(entry->desc.base_priority, 0.6f);
}

TEST_F(PerspectiveRegistryTest, FindNotRegisteredReturnsNull) {
    EXPECT_EQ(inner_dialogue_perspective_find(&registry, PERSPECTIVE_ETHICAL), nullptr);
}

TEST_F(PerspectiveRegistryTest, CountNullReturnsZero) {
    EXPECT_EQ(inner_dialogue_perspective_count(nullptr), 0u);
}

TEST_F(PerspectiveRegistryTest, ClearResetsToEmpty) {
    inner_dialogue_perspective_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = PERSPECTIVE_ANALYTICAL;
    strncpy(desc.name, "A", sizeof(desc.name) - 1);
    desc.formulate = stub_formulate;
    inner_dialogue_perspective_register(&registry, &desc);

    inner_dialogue_perspective_registry_clear(&registry);
    EXPECT_EQ(inner_dialogue_perspective_count(&registry), 0u);
}

TEST_F(PerspectiveRegistryTest, RegisterBuiltinsAddsAll7) {
    int rc = inner_dialogue_register_builtin_perspectives(&registry);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(inner_dialogue_perspective_count(&registry), (uint32_t)PERSPECTIVE_BUILTIN_COUNT);

    /* Verify each built-in is findable */
    EXPECT_NE(inner_dialogue_perspective_find(&registry, PERSPECTIVE_ANALYTICAL), nullptr);
    EXPECT_NE(inner_dialogue_perspective_find(&registry, PERSPECTIVE_EMOTIONAL), nullptr);
    EXPECT_NE(inner_dialogue_perspective_find(&registry, PERSPECTIVE_CRITICAL), nullptr);
    EXPECT_NE(inner_dialogue_perspective_find(&registry, PERSPECTIVE_CREATIVE), nullptr);
    EXPECT_NE(inner_dialogue_perspective_find(&registry, PERSPECTIVE_MEMORY), nullptr);
    EXPECT_NE(inner_dialogue_perspective_find(&registry, PERSPECTIVE_ETHICAL), nullptr);
    EXPECT_NE(inner_dialogue_perspective_find(&registry, PERSPECTIVE_METACOGNITIVE), nullptr);
}

TEST_F(PerspectiveRegistryTest, ComputePriorityPositive) {
    inner_dialogue_perspective_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = PERSPECTIVE_ANALYTICAL;
    strncpy(desc.name, "A", sizeof(desc.name) - 1);
    desc.base_priority = 0.8f;
    desc.formulate = stub_formulate;
    desc.check_relevance = stub_relevance_high;
    inner_dialogue_perspective_register(&registry, &desc);

    const inner_dialogue_perspective_entry_t* entry = inner_dialogue_perspective_find(&registry, PERSPECTIVE_ANALYTICAL);
    ASSERT_NE(entry, nullptr);

    perspective_turn_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.topic = "test topic";
    ctx.urgency = 0.5f;

    float priority = inner_dialogue_perspective_compute_priority(entry, &ctx, 0);
    EXPECT_GT(priority, 0.0f);
}

TEST_F(PerspectiveRegistryTest, SelectNextReturnsValidIndex) {
    inner_dialogue_register_builtin_perspectives(&registry);

    perspective_turn_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.topic = "test topic";
    ctx.urgency = 0.5f;

    int idx = inner_dialogue_perspective_select_next(&registry, &ctx, 0);
    EXPECT_GE(idx, 0);
    EXPECT_LT(idx, (int)INNER_DIALOGUE_MAX_PERSPECTIVES);
}

TEST_F(PerspectiveRegistryTest, SelectNextEmptyReturnsNegative) {
    perspective_turn_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.topic = "empty";

    int idx = inner_dialogue_perspective_select_next(&registry, &ctx, 0);
    EXPECT_LT(idx, 0);
}

TEST(PerspectiveTypeStringTest, AllTypesHaveNames) {
    for (int t = 0; t < PERSPECTIVE_BUILTIN_COUNT; t++) {
        const char* s = perspective_type_to_string((perspective_type_t)t);
        ASSERT_NE(s, nullptr);
        EXPECT_STRNE(s, "UNKNOWN") << "type " << t << " returned UNKNOWN";
    }
}

TEST(PerspectiveTypeStringTest, InvalidReturnsNonNull) {
    const char* s = perspective_type_to_string((perspective_type_t)999);
    ASSERT_NE(s, nullptr);
    /* Custom/unknown types return "CUSTOM/UNKNOWN" */
    EXPECT_TRUE(strstr(s, "UNKNOWN") != nullptr || strstr(s, "CUSTOM") != nullptr);
}

/* ============================================================================
 * TEST SUITE 3: Convergence Analysis
 * ============================================================================ */

class ConvergenceTest : public ::testing::Test {
protected:
    inner_dialogue_turn_history_t* history = nullptr;
    convergence_config_t config;

    void SetUp() override {
        history = inner_dialogue_turn_history_create();
        ASSERT_NE(history, nullptr);
        config = inner_dialogue_convergence_default_config();
    }

    void TearDown() override {
        inner_dialogue_turn_history_destroy(history);
    }

    void AddAgreementTurns(uint32_t count, float agreement) {
        for (uint32_t i = 0; i < count; i++) {
            inner_dialogue_turn_t turn = make_turn(
                i % 3, DIALOGUE_ACT_SYNTHESIZE, "agreeing",
                0.8f, 0.7f, 0.3f, agreement, 0.0f);
            inner_dialogue_turn_history_record(history, &turn);
        }
    }

    void AddDeadlockPattern(uint32_t cycles) {
        for (uint32_t i = 0; i < cycles; i++) {
            inner_dialogue_turn_t t1 = make_turn(0, DIALOGUE_ACT_ASSERT, "position A", 0.9f, 0.8f, 0.1f, 0.1f, 0.0f);
            inner_dialogue_turn_t t2 = make_turn(1, DIALOGUE_ACT_CHALLENGE, "position B", 0.9f, 0.8f, 0.1f, 0.1f, 0.0f);
            inner_dialogue_turn_history_record(history, &t1);
            inner_dialogue_turn_history_record(history, &t2);
        }
    }

    void AddRuminationPattern(uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            inner_dialogue_turn_t turn = make_turn(0, DIALOGUE_ACT_ASSERT, "same repetitive thought over and over",
                0.5f, 0.5f, 0.05f, 0.5f, 0.0f);
            inner_dialogue_turn_history_record(history, &turn);
        }
    }

    void AddEmotionalSpiral(uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            float intensity = 0.1f + (0.8f * i / (float)count);
            inner_dialogue_turn_t turn = make_turn(0, DIALOGUE_ACT_WARN, "emotional",
                0.5f, 0.5f, 0.3f, 0.5f, intensity);
            inner_dialogue_turn_history_record(history, &turn);
        }
    }
};

TEST_F(ConvergenceTest, DefaultConfigHasReasonableValues) {
    EXPECT_GT(config.agreement_threshold, 0.0f);
    EXPECT_LE(config.agreement_threshold, 1.0f);
    EXPECT_GT(config.deadlock_threshold, 0.0f);
    EXPECT_GT(config.rumination_threshold, 0.0f);
    EXPECT_GT(config.emotional_spiral_threshold, 0.0f);
}

TEST_F(ConvergenceTest, AnalyseNullHistoryReturnsError) {
    convergence_analysis_t analysis;
    int rc = inner_dialogue_convergence_analyse(nullptr, &config, &analysis);
    EXPECT_NE(rc, 0);
}

TEST_F(ConvergenceTest, AnalyseInsufficientTurns) {
    /* Less than MIN_TURNS should return insufficient error or partial analysis */
    inner_dialogue_turn_t turn = make_turn(0, DIALOGUE_ACT_ASSERT, "only one", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
    inner_dialogue_turn_history_record(history, &turn);

    convergence_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));
    int rc = inner_dialogue_convergence_analyse(history, &config, &analysis);
    /* Either error or returns with TERMINATION_NONE */
    if (rc == 0) {
        EXPECT_EQ(analysis.recommended_action, TERMINATION_NONE);
    }
}

TEST_F(ConvergenceTest, AnalyseHighAgreementDetected) {
    /* Use varied content to avoid triggering rumination detection */
    const char* contents[] = {
        "perspective alpha initial position on topic",
        "perspective beta agrees with modified framing",
        "perspective gamma extends the consensus view",
        "perspective alpha refines position with new data",
        "perspective beta synthesizes all viewpoints together",
        "perspective gamma concludes with strong endorsement",
        "perspective alpha final confirmation of consensus",
        "perspective beta closing synthesis statement",
    };
    for (uint32_t i = 0; i < 8; i++) {
        inner_dialogue_turn_t turn = make_turn(
            i % 3, DIALOGUE_ACT_SYNTHESIZE, contents[i],
            0.8f, 0.7f, 0.3f + 0.05f * i, 0.9f, 0.0f);
        inner_dialogue_turn_history_record(history, &turn);
    }

    convergence_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));
    int rc = inner_dialogue_convergence_analyse(history, &config, &analysis);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(analysis.agreement_score, 0.5f);
    /* With varied content and high agreement, expect convergence or at least no deadlock */
    EXPECT_FALSE(analysis.deadlocked);
}

TEST_F(ConvergenceTest, AgreementScoreHighForHighAgreement) {
    AddAgreementTurns(8, 0.95f);
    float score = inner_dialogue_convergence_agreement(history, 8);
    EXPECT_GT(score, 0.7f);
}

TEST_F(ConvergenceTest, AgreementScoreLowForLowAgreement) {
    AddAgreementTurns(8, 0.1f);
    float score = inner_dialogue_convergence_agreement(history, 8);
    EXPECT_LT(score, 0.3f);
}

TEST_F(ConvergenceTest, AgreementNullReturnsNegative) {
    EXPECT_LT(inner_dialogue_convergence_agreement(nullptr, 8), 0.0f);
}

TEST_F(ConvergenceTest, TrendPositiveForIncreasingAgreement) {
    /* Add turns with increasing agreement */
    for (uint32_t i = 0; i < 8; i++) {
        float agree = 0.1f + 0.1f * i;
        inner_dialogue_turn_t turn = make_turn(i % 3, DIALOGUE_ACT_ASSERT, "trend", 0.5f, 0.5f, 0.5f, agree, 0.0f);
        inner_dialogue_turn_history_record(history, &turn);
    }

    float trend = inner_dialogue_convergence_trend(history, 8);
    EXPECT_GT(trend, 0.0f);
}

TEST_F(ConvergenceTest, TrendNullReturnsZero) {
    float trend = inner_dialogue_convergence_trend(nullptr, 8);
    /* NULL history returns 0.0f (no trend) */
    EXPECT_FLOAT_EQ(trend, 0.0f);
}

TEST_F(ConvergenceTest, DeadlockDetected) {
    AddDeadlockPattern(6);
    float score = inner_dialogue_convergence_deadlock(history, 12);
    EXPECT_GT(score, 0.3f);
}

TEST_F(ConvergenceTest, DeadlockNullReturnsNegative) {
    EXPECT_LT(inner_dialogue_convergence_deadlock(nullptr, 8), 0.0f);
}

TEST_F(ConvergenceTest, RuminationDetected) {
    AddRuminationPattern(8);
    float score = inner_dialogue_convergence_rumination(history, 8);
    EXPECT_GT(score, 0.3f);
}

TEST_F(ConvergenceTest, RuminationNullReturnsNegative) {
    EXPECT_LT(inner_dialogue_convergence_rumination(nullptr, 8), 0.0f);
}

TEST_F(ConvergenceTest, EmotionalTemperatureIncreases) {
    AddEmotionalSpiral(8);
    float temp = inner_dialogue_convergence_emotional_temperature(history, 8);
    EXPECT_GT(temp, 0.3f);
}

TEST_F(ConvergenceTest, EmotionalTemperatureNullReturnsNegative) {
    EXPECT_LT(inner_dialogue_convergence_emotional_temperature(nullptr, 8), 0.0f);
}

TEST_F(ConvergenceTest, PerspectiveEntropyHighForDiverse) {
    /* Record from multiple perspectives */
    for (uint32_t i = 0; i < 7; i++) {
        inner_dialogue_turn_t turn = make_turn(i, DIALOGUE_ACT_ASSERT, "diverse", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
        inner_dialogue_turn_history_record(history, &turn);
    }

    float entropy = inner_dialogue_convergence_perspective_entropy(history, 0);
    EXPECT_GT(entropy, 1.0f);
}

TEST_F(ConvergenceTest, PerspectiveEntropyLowForMonologue) {
    for (uint32_t i = 0; i < 8; i++) {
        inner_dialogue_turn_t turn = make_turn(0, DIALOGUE_ACT_ASSERT, "monologue", 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
        inner_dialogue_turn_history_record(history, &turn);
    }

    float entropy = inner_dialogue_convergence_perspective_entropy(history, 0);
    EXPECT_NEAR(entropy, 0.0f, 0.01f);
}

TEST_F(ConvergenceTest, PerspectiveEntropyNullReturnsNegative) {
    EXPECT_LT(inner_dialogue_convergence_perspective_entropy(nullptr, 0), 0.0f);
}

TEST(TerminationReasonStringTest, AllReasonsHaveNames) {
    for (int r = 0; r < TERMINATION_COUNT; r++) {
        const char* s = termination_reason_to_string((termination_reason_t)r);
        ASSERT_NE(s, nullptr);
        EXPECT_STRNE(s, "UNKNOWN") << "reason " << r << " returned UNKNOWN";
    }
}

TEST(TerminationReasonStringTest, InvalidReturnsUnknown) {
    const char* s = termination_reason_to_string((termination_reason_t)999);
    ASSERT_NE(s, nullptr);
    EXPECT_STREQ(s, "UNKNOWN");
}

/* ============================================================================
 * TEST SUITE 4: Engine Lifecycle
 * ============================================================================ */

class EngineTest : public ::testing::Test {
protected:
    inner_dialogue_engine_t* engine = nullptr;

    void SetUp() override {
        engine = inner_dialogue_engine_create(nullptr);
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override {
        inner_dialogue_engine_destroy(engine);
    }
};

TEST_F(EngineTest, CreateWithDefaultConfig) {
    /* Covered by SetUp */
    EXPECT_EQ(inner_dialogue_engine_get_state(engine), DIALOGUE_STATE_IDLE);
}

TEST_F(EngineTest, CreateWithCustomConfig) {
    inner_dialogue_config_t cfg = inner_dialogue_default_config();
    cfg.max_turns = 64;
    cfg.urgency = 0.8f;
    cfg.verbose_logging = true;

    inner_dialogue_engine_t* e2 = inner_dialogue_engine_create(&cfg);
    ASSERT_NE(e2, nullptr);
    EXPECT_EQ(inner_dialogue_engine_get_state(e2), DIALOGUE_STATE_IDLE);
    inner_dialogue_engine_destroy(e2);
}

TEST_F(EngineTest, DestroyNullSafe) {
    inner_dialogue_engine_destroy(nullptr);
    /* No crash = pass */
}

TEST_F(EngineTest, ResetReturnsToIdle) {
    int rc = inner_dialogue_engine_reset(engine);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(inner_dialogue_engine_get_state(engine), DIALOGUE_STATE_IDLE);
}

TEST_F(EngineTest, ResetNullReturnsError) {
    int rc = inner_dialogue_engine_reset(nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(EngineTest, DefaultConfigHasSensibleValues) {
    inner_dialogue_config_t cfg = inner_dialogue_default_config();
    EXPECT_GT(cfg.max_turns, 0u);
    EXPECT_GT(cfg.urgency, 0.0f);
    EXPECT_LE(cfg.urgency, 1.0f);
    EXPECT_GT(cfg.min_relevance_threshold, 0.0f);
}

/* ============================================================================
 * TEST: Setters (NULL safety)
 * ============================================================================ */

TEST_F(EngineTest, SetHealthAgentNullEngineReturnsError) {
    EXPECT_NE(inner_dialogue_engine_set_health_agent(nullptr, nullptr), 0);
}

TEST_F(EngineTest, SetImmuneNullEngineReturnsError) {
    EXPECT_NE(inner_dialogue_engine_set_immune(nullptr, nullptr), 0);
}

TEST_F(EngineTest, SetBbbNullEngineReturnsError) {
    EXPECT_NE(inner_dialogue_engine_set_bbb(nullptr, nullptr), 0);
}

TEST_F(EngineTest, SetBioRouterNullEngineReturnsError) {
    EXPECT_NE(inner_dialogue_engine_set_bio_router(nullptr, nullptr), 0);
}

TEST_F(EngineTest, SetCycleCoordinatorNullEngineReturnsError) {
    EXPECT_NE(inner_dialogue_engine_set_cycle_coordinator(nullptr, nullptr), 0);
}

TEST_F(EngineTest, SetEthicsNullEngineReturnsError) {
    EXPECT_NE(inner_dialogue_engine_set_ethics(nullptr, nullptr), 0);
}

TEST_F(EngineTest, SetLgssNullEngineReturnsError) {
    EXPECT_NE(inner_dialogue_engine_set_lgss(nullptr, nullptr), 0);
}

/* Setters with valid engine but NULL subsystem should succeed (disconnect) */
TEST_F(EngineTest, SetEthicsNullDisconnects) {
    int rc = inner_dialogue_engine_set_ethics(engine, nullptr);
    EXPECT_EQ(rc, 0);
}

TEST_F(EngineTest, SetLgssNullDisconnects) {
    int rc = inner_dialogue_engine_set_lgss(engine, nullptr);
    EXPECT_EQ(rc, 0);
}

/* ============================================================================
 * TEST: Registry access
 * ============================================================================ */

TEST_F(EngineTest, GetRegistryReturnsNonNull) {
    inner_dialogue_perspective_registry_t* reg = inner_dialogue_engine_get_registry(engine);
    EXPECT_NE(reg, nullptr);
}

TEST_F(EngineTest, GetRegistryNullReturnsNull) {
    EXPECT_EQ(inner_dialogue_engine_get_registry(nullptr), nullptr);
}

TEST_F(EngineTest, RegisterBuiltinsPopulatesRegistry) {
    int rc = inner_dialogue_engine_register_builtins(engine);
    EXPECT_EQ(rc, 0);

    inner_dialogue_perspective_registry_t* reg = inner_dialogue_engine_get_registry(engine);
    ASSERT_NE(reg, nullptr);
    EXPECT_EQ(inner_dialogue_perspective_count(reg), (uint32_t)PERSPECTIVE_BUILTIN_COUNT);
}

/* ============================================================================
 * TEST: Conversation lifecycle
 * ============================================================================ */

TEST_F(EngineTest, StartTransitionsToInitiated) {
    inner_dialogue_engine_register_builtins(engine);
    int rc = inner_dialogue_engine_start(engine, "test topic");
    EXPECT_EQ(rc, 0);

    inner_dialogue_state_t state = inner_dialogue_engine_get_state(engine);
    EXPECT_TRUE(state == DIALOGUE_STATE_INITIATED || state == DIALOGUE_STATE_DELIBERATING);
}

TEST_F(EngineTest, StartNullTopicReturnsError) {
    inner_dialogue_engine_register_builtins(engine);
    int rc = inner_dialogue_engine_start(engine, nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(EngineTest, StartNullEngineReturnsError) {
    int rc = inner_dialogue_engine_start(nullptr, "topic");
    EXPECT_NE(rc, 0);
}

TEST_F(EngineTest, StartWithoutPerspectivesReturnsError) {
    int rc = inner_dialogue_engine_start(engine, "topic");
    EXPECT_NE(rc, 0);
}

TEST_F(EngineTest, GetTopicAfterStart) {
    inner_dialogue_engine_register_builtins(engine);
    inner_dialogue_engine_start(engine, "deliberation about AI safety");

    const char* topic = inner_dialogue_engine_get_topic(engine);
    ASSERT_NE(topic, nullptr);
    EXPECT_STREQ(topic, "deliberation about AI safety");
}

TEST_F(EngineTest, GetTopicBeforeStartReturnsNull) {
    EXPECT_EQ(inner_dialogue_engine_get_topic(engine), nullptr);
}

TEST_F(EngineTest, GetTopicNullEngineReturnsNull) {
    EXPECT_EQ(inner_dialogue_engine_get_topic(nullptr), nullptr);
}

TEST_F(EngineTest, StepProducesTurn) {
    inner_dialogue_engine_register_builtins(engine);
    inner_dialogue_engine_start(engine, "step test");

    inner_dialogue_turn_t turn;
    memset(&turn, 0, sizeof(turn));
    int rc = inner_dialogue_engine_step(engine, &turn);

    /* rc == 0 means turn completed, positive means conversation ended */
    EXPECT_GE(rc, 0);
    if (rc == 0) {
        EXPECT_GT(turn.content_len, 0u);
        EXPECT_GE(inner_dialogue_engine_get_turn_number(engine), 1u);
    }
}

TEST_F(EngineTest, StepNullEngineReturnsError) {
    inner_dialogue_turn_t turn;
    int rc = inner_dialogue_engine_step(nullptr, &turn);
    EXPECT_LT(rc, 0);
}

TEST_F(EngineTest, StepWithNullTurnOutSucceeds) {
    inner_dialogue_engine_register_builtins(engine);
    inner_dialogue_engine_start(engine, "no output turn");

    int rc = inner_dialogue_engine_step(engine, nullptr);
    EXPECT_GE(rc, 0);
}

TEST_F(EngineTest, CancelActiveConversation) {
    inner_dialogue_engine_register_builtins(engine);
    inner_dialogue_engine_start(engine, "cancel test");

    int rc = inner_dialogue_engine_cancel(engine);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(inner_dialogue_engine_get_state(engine), DIALOGUE_STATE_CANCELLED);
}

TEST_F(EngineTest, CancelNoActiveConversationReturnsError) {
    int rc = inner_dialogue_engine_cancel(engine);
    EXPECT_NE(rc, 0);
}

TEST_F(EngineTest, CancelNullEngineReturnsError) {
    int rc = inner_dialogue_engine_cancel(nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(EngineTest, RunToCompletion) {
    inner_dialogue_engine_register_builtins(engine);
    inner_dialogue_engine_start(engine, "run to completion test");

    inner_dialogue_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = inner_dialogue_engine_run(engine, &result);

    EXPECT_GE(rc, 0);
    EXPECT_GT(result.total_turns, 0u);
    EXPECT_GT(result.perspectives_participated, 0u);

    inner_dialogue_state_t final_state = inner_dialogue_engine_get_state(engine);
    EXPECT_TRUE(final_state == DIALOGUE_STATE_CONCLUDED ||
                final_state == DIALOGUE_STATE_CONVERGING ||
                final_state == DIALOGUE_STATE_DEADLOCKED ||
                final_state == DIALOGUE_STATE_RUMINATING);
}

TEST_F(EngineTest, RunNullEngineReturnsError) {
    inner_dialogue_result_t result;
    int rc = inner_dialogue_engine_run(nullptr, &result);
    EXPECT_LT(rc, 0);
}

TEST_F(EngineTest, RunNullResultReturnsError) {
    inner_dialogue_engine_register_builtins(engine);
    inner_dialogue_engine_start(engine, "no result");
    int rc = inner_dialogue_engine_run(engine, nullptr);
    EXPECT_LT(rc, 0);
}

/* ============================================================================
 * TEST: Query API
 * ============================================================================ */

TEST_F(EngineTest, GetStateNullReturnsIdle) {
    EXPECT_EQ(inner_dialogue_engine_get_state(nullptr), DIALOGUE_STATE_IDLE);
}

TEST_F(EngineTest, GetHistoryAfterStep) {
    inner_dialogue_engine_register_builtins(engine);
    inner_dialogue_engine_start(engine, "history query test");
    inner_dialogue_engine_step(engine, nullptr);

    const inner_dialogue_turn_history_t* hist = inner_dialogue_engine_get_history(engine);
    ASSERT_NE(hist, nullptr);
    EXPECT_GE(inner_dialogue_turn_history_count(hist), 1u);
}

TEST_F(EngineTest, GetHistoryNullReturnsNull) {
    EXPECT_EQ(inner_dialogue_engine_get_history(nullptr), nullptr);
}

TEST_F(EngineTest, GetTurnNumberInitiallyZero) {
    EXPECT_EQ(inner_dialogue_engine_get_turn_number(engine), 0u);
}

TEST_F(EngineTest, GetTurnNumberNullReturnsZero) {
    EXPECT_EQ(inner_dialogue_engine_get_turn_number(nullptr), 0u);
}

TEST_F(EngineTest, GetConvergenceNullReturnsError) {
    convergence_analysis_t analysis;
    EXPECT_NE(inner_dialogue_engine_get_convergence(nullptr, &analysis), 0);
}

TEST_F(EngineTest, GetStatsPopulatesCounts) {
    inner_dialogue_engine_register_builtins(engine);
    inner_dialogue_engine_start(engine, "stats test");

    /* Run a few steps */
    for (int i = 0; i < 3; i++) {
        int rc = inner_dialogue_engine_step(engine, nullptr);
        if (rc > 0) break; /* conversation ended */
    }

    inner_dialogue_engine_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int rc = inner_dialogue_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(stats.conversations_started, 1u);
}

TEST_F(EngineTest, GetStatsNullReturnsError) {
    inner_dialogue_engine_stats_t stats;
    EXPECT_NE(inner_dialogue_engine_get_stats(nullptr, &stats), 0);
    EXPECT_NE(inner_dialogue_engine_get_stats(engine, nullptr), 0);
}

/* ============================================================================
 * TEST: State string conversion
 * ============================================================================ */

TEST(EngineStateStringTest, AllStatesHaveNames) {
    for (int s = 0; s < DIALOGUE_STATE_COUNT; s++) {
        const char* str = inner_dialogue_state_to_string((inner_dialogue_state_t)s);
        ASSERT_NE(str, nullptr);
        EXPECT_STRNE(str, "UNKNOWN") << "state " << s << " returned UNKNOWN";
    }
}

TEST(EngineStateStringTest, InvalidReturnsUnknown) {
    const char* str = inner_dialogue_state_to_string((inner_dialogue_state_t)999);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, "UNKNOWN");
}

/* ============================================================================
 * TEST: Multiple conversations
 * ============================================================================ */

TEST_F(EngineTest, MultipleConversationsSequentially) {
    inner_dialogue_engine_register_builtins(engine);

    for (int conv = 0; conv < 3; conv++) {
        SCOPED_TRACE("Conversation " + std::to_string(conv));

        char topic[64];
        snprintf(topic, sizeof(topic), "conversation %d", conv);

        int rc = inner_dialogue_engine_start(engine, topic);
        EXPECT_EQ(rc, 0);

        inner_dialogue_result_t result;
        memset(&result, 0, sizeof(result));
        rc = inner_dialogue_engine_run(engine, &result);
        EXPECT_GE(rc, 0);
        EXPECT_GT(result.total_turns, 0u);

        /* Reset for next conversation */
        inner_dialogue_engine_reset(engine);
    }

    inner_dialogue_engine_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    inner_dialogue_engine_get_stats(engine, &stats);
    EXPECT_GE(stats.conversations_started, 3u);
}
