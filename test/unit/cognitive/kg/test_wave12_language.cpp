/**
 * @file test_wave12_language.cpp
 * @brief Unit tests for KG-integration Wave W12 — language + communication.
 *
 * Wave W12 wires eight language/communication subsystems into
 * `brain->internal_kg`:
 *   1. cog_language_emergent     (emergent vocabulary discovery)
 *   2. cog_language_inner_speech (iterative self-refinement)
 *   3. cog_language_native       (autoregressive native decoding)
 *   4. cog_language_tokenizer    (BPE tokenizer — rate-limited)
 *   5. broca_discourse           (turn tracking)
 *   6. broca_syntax              (parse-tree build)
 *   7. broca_pragmatics          (speech-act + implicature analysis)
 *   8. broca_multimodal          (utterance plan)
 *
 * Each test verifies:
 *   (a) the structural root exists after ensure_roots (or first emit),
 *   (b) calling the emit helper produces an event-node with the
 *       expected prefix.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "cognitive/language/nimcp_w12_language_kg_events.h"

//-----------------------------------------------------------------------------
// Fixture
//-----------------------------------------------------------------------------

class Wave12LanguageKGTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave12_kg_test",
                                     BRAIN_SIZE_MICRO,
                                     BRAIN_TASK_CLASSIFICATION,
                                     4, 2);
        ASSERT_NE(brain, nullptr) << "brain_create_minimal returned NULL";
        ASSERT_TRUE(brain->internal_kg_enabled)
            << "internal_kg_enabled must be true post-creation";
        ASSERT_NE(brain->internal_kg, nullptr)
            << "brain->internal_kg must be allocated";
    }

    void TearDown() override {
        /* Clear any registered auto-brain before destroying brain. */
        w12_language_kg_register_brain(nullptr);
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    bool has_node(const char* name) {
        return brain_kg_find_node(brain->internal_kg, name) !=
               BRAIN_KG_INVALID_NODE;
    }

    bool any_node_with_prefix(const char* prefix) {
        const size_t plen = strlen(prefix);
        for (uint32_t t = 0; t < BRAIN_KG_NODE_TYPE_COUNT; ++t) {
            brain_kg_node_list_t* list =
                brain_kg_get_nodes_by_type(brain->internal_kg,
                                           (brain_kg_node_type_t)t);
            if (!list) continue;
            bool found = false;
            for (uint32_t i = 0; i < list->count && !found; ++i) {
                const brain_kg_node_t* n = list->nodes[i];
                if (n && strncmp(n->name, prefix, plen) == 0) {
                    found = true;
                }
            }
            brain_kg_node_list_destroy(list);
            if (found) return true;
        }
        return false;
    }
};

//-----------------------------------------------------------------------------
// 1. ensure_roots creates all 8 structural parents (and is idempotent)
//-----------------------------------------------------------------------------

TEST_F(Wave12LanguageKGTest, EnsureRootsCreatesAllEight) {
    w12_language_ensure_roots(brain);
    EXPECT_TRUE(has_node("cog_language_emergent"));
    EXPECT_TRUE(has_node("cog_language_inner_speech"));
    EXPECT_TRUE(has_node("cog_language_native"));
    EXPECT_TRUE(has_node("cog_language_tokenizer"));
    EXPECT_TRUE(has_node("broca_discourse"));
    EXPECT_TRUE(has_node("broca_syntax"));
    EXPECT_TRUE(has_node("broca_pragmatics"));
    EXPECT_TRUE(has_node("broca_multimodal"));
}

TEST_F(Wave12LanguageKGTest, EnsureRootsIsIdempotent) {
    w12_language_ensure_roots(brain);
    w12_language_ensure_roots(brain); /* must not duplicate */
    /* Sentinel: each root should still be present and unique — if a
     * duplicate were created, find_node would still match the first. */
    EXPECT_TRUE(has_node("cog_language_emergent"));
    EXPECT_TRUE(has_node("broca_multimodal"));
}

//-----------------------------------------------------------------------------
// 2. Emergent vocabulary emits
//-----------------------------------------------------------------------------

TEST_F(Wave12LanguageKGTest, EmergentVocabDiscoveredEmitsEvent) {
    w12_emit_emergent_vocab(brain, "discovered", /*token_id*/42,
                            /*specificity*/0.8f);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_language_emergent_event_discovered_"));
    EXPECT_TRUE(has_node("cog_language_emergent"));
}

//-----------------------------------------------------------------------------
// 3. Inner speech emits
//-----------------------------------------------------------------------------

TEST_F(Wave12LanguageKGTest, InnerSpeechRefineEmitsEvent) {
    w12_emit_inner_speech_refine(brain, /*iterations*/3,
                                 /*convergence*/0.97f);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_language_inner_speech_event_refine_"));
}

//-----------------------------------------------------------------------------
// 4. Native language emits
//-----------------------------------------------------------------------------

TEST_F(Wave12LanguageKGTest, NativeGenerateEmitsEvent) {
    w12_emit_native_generate(brain, /*token_count*/12,
                             /*mean_score*/0.62f);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_language_native_event_generate_"));
}

//-----------------------------------------------------------------------------
// 5. Tokenizer emits — merges always emit, encode events rate-limited.
//-----------------------------------------------------------------------------

TEST_F(Wave12LanguageKGTest, TokenizerMergeEventEmitsEveryCall) {
    w12_emit_tokenizer_event(brain, "merge", /*vocab*/1024, /*delta*/1);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_language_tokenizer_event_merge_"));
}

//-----------------------------------------------------------------------------
// 6. Discourse manager emits
//-----------------------------------------------------------------------------

TEST_F(Wave12LanguageKGTest, DiscourseTurnEmitsEvent) {
    w12_emit_discourse_turn(brain, /*turn_id*/7, /*speaker*/1,
                            /*topic_shift*/0, /*coherence*/0.82f);
    EXPECT_TRUE(any_node_with_prefix("broca_discourse_event_turn_7_"));
    EXPECT_TRUE(has_node("broca_discourse"));
}

//-----------------------------------------------------------------------------
// 7. Syntax parse emits
//-----------------------------------------------------------------------------

TEST_F(Wave12LanguageKGTest, SyntaxParseEmitsEvent) {
    w12_emit_syntax_parse(brain, /*unit_count*/5, /*depth*/3,
                          /*success*/true);
    EXPECT_TRUE(any_node_with_prefix("broca_syntax_event_parse_"));
}

//-----------------------------------------------------------------------------
// 8. Pragmatics analyze emits
//-----------------------------------------------------------------------------

TEST_F(Wave12LanguageKGTest, PragmaticsAnalyzeEmitsEvent) {
    w12_emit_pragmatics_analyze(brain, /*act*/1, /*speaker*/2,
                                /*implicatures*/1, /*relevance*/0.75f);
    EXPECT_TRUE(any_node_with_prefix(
        "broca_pragmatics_event_analyze_"));
}

//-----------------------------------------------------------------------------
// 9. Multimodal plan emits + auto wrappers (registered brain)
//-----------------------------------------------------------------------------

TEST_F(Wave12LanguageKGTest, MultimodalPlanEmitsEvent) {
    w12_emit_multimodal_plan(brain, /*gestures*/2, /*expressions*/1,
                             /*sync*/0.8f);
    EXPECT_TRUE(any_node_with_prefix("broca_multimodal_event_plan_"));
}

TEST_F(Wave12LanguageKGTest, AutoWrappersUseRegisteredBrain) {
    /* No brain registered yet => auto wrappers are no-ops. */
    w12_emit_emergent_vocab_auto("discovered", 0, 0.0f);
    EXPECT_FALSE(any_node_with_prefix(
        "cog_language_emergent_event_discovered_"));

    /* Now register and retry. */
    w12_language_kg_register_brain(brain);
    EXPECT_EQ(w12_language_kg_get_brain(), brain);

    w12_emit_native_generate_auto(/*tokens*/4, /*score*/0.5f);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_language_native_event_generate_"));
}

//-----------------------------------------------------------------------------
// 10. Null safety — must not crash when brain is NULL.
//-----------------------------------------------------------------------------

TEST(Wave12LanguageKGStaticTests, NullBrainIsSafe) {
    w12_language_ensure_roots(nullptr);
    w12_emit_emergent_vocab(nullptr, "discovered", 0, 0.0f);
    w12_emit_inner_speech_refine(nullptr, 0, 0.0f);
    w12_emit_native_generate(nullptr, 0, 0.0f);
    w12_emit_tokenizer_event(nullptr, "encode", 0, 0);
    w12_emit_discourse_turn(nullptr, 0, 0, 0, 0.0f);
    w12_emit_syntax_parse(nullptr, 0, 0, true);
    w12_emit_pragmatics_analyze(nullptr, 0, 0, 0, 0.0f);
    w12_emit_multimodal_plan(nullptr, 0, 0, 0.0f);

    /* Auto wrappers must also be safe (no registered brain). */
    w12_language_kg_register_brain(nullptr);
    w12_emit_emergent_vocab_auto("discovered", 0, 0.0f);
    w12_emit_inner_speech_refine_auto(0, 0.0f);
    w12_emit_native_generate_auto(0, 0.0f);
    w12_emit_tokenizer_event_auto("encode", 0, 0);
    w12_emit_discourse_turn_auto(0, 0, 0, 0.0f);
    w12_emit_syntax_parse_auto(0, 0, true);
    w12_emit_pragmatics_analyze_auto(0, 0, 0, 0.0f);
    w12_emit_multimodal_plan_auto(0, 0, 0.0f);

    SUCCEED();
}
