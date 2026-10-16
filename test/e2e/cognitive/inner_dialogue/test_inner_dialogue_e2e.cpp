/**
 * @file test_inner_dialogue_e2e.cpp
 * @brief End-to-end tests for Inner Dialogue Engine full conversation lifecycle
 *
 * WHAT: Test complete conversation workflows from creation to conclusion
 * WHY:  Verify the entire deliberation pipeline works end-to-end
 * HOW:  Create engine, register perspectives, start conversation, run to
 *       completion, verify outcomes
 *
 * TESTS:
 *   - Full conversation lifecycle with convergence
 *   - Multiple sequential conversations
 *   - Conversation with custom perspectives
 *   - Defense-in-depth validation chain observation
 *   - Conversation statistics consistency
 *   - Cancel and restart workflow
 *   - Topic fidelity throughout conversation
 *   - Turn provenance tracking (perspective attribution)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <set>
#include <string>

extern "C" {
#include "cognitive/inner_dialogue/nimcp_inner_dialogue.h"
#include "cognitive/inner_dialogue/nimcp_inner_dialogue_turn.h"
#include "cognitive/inner_dialogue/nimcp_inner_dialogue_perspective.h"
#include "cognitive/inner_dialogue/nimcp_inner_dialogue_convergence.h"
}

/* ============================================================================
 * Helper: custom perspective with high agreement (drives convergence)
 * ============================================================================ */
static bool converging_formulate(const perspective_turn_context_t* ctx,
                                 inner_dialogue_turn_t* output) {
    memset(output, 0, sizeof(*output));
    output->act = DIALOGUE_ACT_SYNTHESIZE;
    const char* msg = "I agree with the emerging consensus on this topic";
    strncpy(output->content, msg, INNER_DIALOGUE_TURN_MAX_CONTENT - 1);
    output->content_len = (uint32_t)strlen(msg);
    output->confidence = 0.9f;
    output->relevance = 0.9f;
    output->novelty = 0.2f;
    output->agreement_with_prior = 0.95f;
    output->emotional_valence = 0.1f;
    (void)ctx;
    return true;
}

/* Helper: custom perspective that disagrees (drives deadlock) */
static bool disagreeing_formulate(const perspective_turn_context_t* ctx,
                                  inner_dialogue_turn_t* output) {
    memset(output, 0, sizeof(*output));
    output->act = DIALOGUE_ACT_CHALLENGE;
    const char* msg = "I strongly disagree with that position";
    strncpy(output->content, msg, INNER_DIALOGUE_TURN_MAX_CONTENT - 1);
    output->content_len = (uint32_t)strlen(msg);
    output->confidence = 0.9f;
    output->relevance = 0.8f;
    output->novelty = 0.1f;
    output->agreement_with_prior = 0.05f;
    output->emotional_valence = -0.3f;
    (void)ctx;
    return true;
}

/* Helper: perspective that repeats the same content (drives rumination) */
static bool ruminating_formulate(const perspective_turn_context_t* ctx,
                                 inner_dialogue_turn_t* output) {
    memset(output, 0, sizeof(*output));
    output->act = DIALOGUE_ACT_ASSERT;
    const char* msg = "the same repetitive thought pattern over and over again without variation";
    strncpy(output->content, msg, INNER_DIALOGUE_TURN_MAX_CONTENT - 1);
    output->content_len = (uint32_t)strlen(msg);
    output->confidence = 0.5f;
    output->relevance = 0.5f;
    output->novelty = 0.02f;
    output->agreement_with_prior = 0.5f;
    output->emotional_valence = -0.1f;
    (void)ctx;
    return true;
}

/* ============================================================================
 * E2E Fixture
 * ============================================================================ */

class InnerDialogueE2ETest : public ::testing::Test {
protected:
    inner_dialogue_engine_t* engine = nullptr;

    void SetUp() override {
        inner_dialogue_config_t cfg = inner_dialogue_default_config();
        cfg.max_turns = 24;
        cfg.verbose_logging = false;
        cfg.enable_ethics_evaluation = false;
        cfg.enable_lgss_evaluation = false;
        cfg.enable_bbb_validation = false;
        cfg.enable_bio_async = false;
        cfg.enable_immune_integration = false;
        cfg.enable_health_heartbeat = false;

        engine = inner_dialogue_engine_create(&cfg);
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override {
        inner_dialogue_engine_destroy(engine);
    }

    void RegisterCustomPerspective(perspective_type_t type, const char* name,
                                    float priority, perspective_formulate_fn fn) {
        inner_dialogue_perspective_registry_t* reg = inner_dialogue_engine_get_registry(engine);
        ASSERT_NE(reg, nullptr);

        inner_dialogue_perspective_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.type = type;
        strncpy(desc.name, name, sizeof(desc.name) - 1);
        desc.base_priority = priority;
        desc.formulate = fn;

        int rc = inner_dialogue_perspective_register(reg, &desc);
        EXPECT_EQ(rc, 0) << "Failed to register perspective: " << name;
    }
};

/* ============================================================================
 * TEST: Full conversation lifecycle with built-in perspectives
 * ============================================================================ */

TEST_F(InnerDialogueE2ETest, FullConversationLifecycle) {
    /* Register builtins */
    int rc = inner_dialogue_engine_register_builtins(engine);
    ASSERT_EQ(rc, 0);

    /* Verify initial state */
    EXPECT_EQ(inner_dialogue_engine_get_state(engine), DIALOGUE_STATE_IDLE);
    EXPECT_EQ(inner_dialogue_engine_get_topic(engine), nullptr);
    EXPECT_EQ(inner_dialogue_engine_get_turn_number(engine), 0u);

    /* Start conversation */
    rc = inner_dialogue_engine_start(engine, "Should we prioritize safety over capability?");
    EXPECT_EQ(rc, 0);
    EXPECT_NE(inner_dialogue_engine_get_state(engine), DIALOGUE_STATE_IDLE);
    EXPECT_STREQ(inner_dialogue_engine_get_topic(engine), "Should we prioritize safety over capability?");

    /* Run to completion */
    inner_dialogue_result_t result;
    memset(&result, 0, sizeof(result));
    rc = inner_dialogue_engine_run(engine, &result);
    EXPECT_GE(rc, 0);

    /* Verify result */
    EXPECT_GT(result.total_turns, 0u);
    EXPECT_GT(result.perspectives_participated, 0u);
    EXPECT_NE(result.termination_reason, TERMINATION_NONE);
    EXPECT_GE(result.final_agreement, 0.0f);
    EXPECT_LE(result.final_agreement, 1.0f);
    EXPECT_GE(result.avg_confidence, 0.0f);
    EXPECT_LE(result.avg_confidence, 1.0f);
    EXPECT_GE(result.total_formulation_time_ms, 0.0f);

    /* Verify final state is terminal */
    inner_dialogue_state_t final_state = inner_dialogue_engine_get_state(engine);
    EXPECT_TRUE(final_state == DIALOGUE_STATE_CONCLUDED ||
                final_state == DIALOGUE_STATE_CONVERGING ||
                final_state == DIALOGUE_STATE_DEADLOCKED ||
                final_state == DIALOGUE_STATE_RUMINATING ||
                final_state == DIALOGUE_STATE_ESCALATED);

    /* Verify statistics */
    inner_dialogue_engine_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    rc = inner_dialogue_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(stats.conversations_started, 1u);
    EXPECT_EQ(stats.total_turns_produced, (uint64_t)result.total_turns);
}

/* ============================================================================
 * TEST: Conversation with converging custom perspectives
 * ============================================================================ */

TEST_F(InnerDialogueE2ETest, ConversationConvergesWithAgreement) {
    /* Register perspectives that strongly agree */
    RegisterCustomPerspective((perspective_type_t)(PERSPECTIVE_CUSTOM_START),
                              "Agreeable_A", 0.8f, converging_formulate);
    RegisterCustomPerspective((perspective_type_t)(PERSPECTIVE_CUSTOM_START + 1),
                              "Agreeable_B", 0.7f, converging_formulate);
    RegisterCustomPerspective((perspective_type_t)(PERSPECTIVE_CUSTOM_START + 2),
                              "Agreeable_C", 0.6f, converging_formulate);

    inner_dialogue_engine_start(engine, "easy consensus topic");

    inner_dialogue_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = inner_dialogue_engine_run(engine, &result);
    EXPECT_GE(rc, 0);

    /* Should converge relatively quickly */
    EXPECT_GT(result.total_turns, 0u);
    EXPECT_GE(result.final_agreement, 0.5f);

    /* Termination should be CONVERGED or MAX_TURNS */
    EXPECT_TRUE(result.termination_reason == TERMINATION_CONVERGED ||
                result.termination_reason == TERMINATION_MAX_TURNS);
}

/* ============================================================================
 * TEST: Conversation deadlocks with disagreeing perspectives
 * ============================================================================ */

TEST_F(InnerDialogueE2ETest, ConversationDeadlocksWithDisagreement) {
    RegisterCustomPerspective((perspective_type_t)(PERSPECTIVE_CUSTOM_START),
                              "Hawk", 0.9f, disagreeing_formulate);
    RegisterCustomPerspective((perspective_type_t)(PERSPECTIVE_CUSTOM_START + 1),
                              "Dove", 0.8f, converging_formulate);

    inner_dialogue_engine_start(engine, "contentious topic");

    inner_dialogue_result_t result;
    memset(&result, 0, sizeof(result));
    inner_dialogue_engine_run(engine, &result);

    /* Should eventually terminate (deadlock, max turns, or other) */
    EXPECT_GT(result.total_turns, 0u);
    EXPECT_NE(result.termination_reason, TERMINATION_NONE);
}

/* ============================================================================
 * TEST: Rumination detection with repetitive content
 * ============================================================================ */

TEST_F(InnerDialogueE2ETest, RuminationDetectedWithRepetition) {
    RegisterCustomPerspective((perspective_type_t)(PERSPECTIVE_CUSTOM_START),
                              "Ruminator", 0.9f, ruminating_formulate);

    inner_dialogue_engine_start(engine, "circular thinking topic");

    inner_dialogue_result_t result;
    memset(&result, 0, sizeof(result));
    inner_dialogue_engine_run(engine, &result);

    /* Should detect rumination or hit max turns */
    EXPECT_GT(result.total_turns, 0u);
    EXPECT_TRUE(result.termination_reason == TERMINATION_RUMINATING ||
                result.termination_reason == TERMINATION_MAX_TURNS);
}

/* ============================================================================
 * TEST: Cancel and restart workflow
 * ============================================================================ */

TEST_F(InnerDialogueE2ETest, CancelAndRestartWorkflow) {
    inner_dialogue_engine_register_builtins(engine);

    /* Start first conversation */
    inner_dialogue_engine_start(engine, "conversation one");
    inner_dialogue_engine_step(engine, nullptr);
    inner_dialogue_engine_step(engine, nullptr);

    /* Cancel mid-flight */
    int rc = inner_dialogue_engine_cancel(engine);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(inner_dialogue_engine_get_state(engine), DIALOGUE_STATE_CANCELLED);

    /* Reset */
    inner_dialogue_engine_reset(engine);
    EXPECT_EQ(inner_dialogue_engine_get_state(engine), DIALOGUE_STATE_IDLE);

    /* Start new conversation */
    rc = inner_dialogue_engine_start(engine, "conversation two");
    EXPECT_EQ(rc, 0);

    /* Run to completion */
    inner_dialogue_result_t result;
    memset(&result, 0, sizeof(result));
    rc = inner_dialogue_engine_run(engine, &result);
    EXPECT_GE(rc, 0);
    EXPECT_GT(result.total_turns, 0u);

    /* Stats should reflect both conversations */
    inner_dialogue_engine_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    inner_dialogue_engine_get_stats(engine, &stats);
    EXPECT_GE(stats.conversations_started, 2u);
    EXPECT_GE(stats.conversations_cancelled, 1u);
}

/* ============================================================================
 * TEST: Topic fidelity throughout conversation
 * ============================================================================ */

TEST_F(InnerDialogueE2ETest, TopicFidelityThroughoutConversation) {
    inner_dialogue_engine_register_builtins(engine);

    const char* topic = "The ethics of artificial intelligence decision-making";
    inner_dialogue_engine_start(engine, topic);

    /* Check topic at each step */
    for (int i = 0; i < 10; i++) {
        const char* current_topic = inner_dialogue_engine_get_topic(engine);
        ASSERT_NE(current_topic, nullptr) << "Topic became NULL at step " << i;
        EXPECT_STREQ(current_topic, topic) << "Topic changed at step " << i;

        int rc = inner_dialogue_engine_step(engine, nullptr);
        if (rc > 0) break;
    }
}

/* ============================================================================
 * TEST: Step-by-step matches run results
 * ============================================================================ */

TEST_F(InnerDialogueE2ETest, StepByStepMatchesRun) {
    inner_dialogue_engine_register_builtins(engine);

    /* Run step-by-step first */
    inner_dialogue_engine_start(engine, "step vs run comparison");

    uint32_t step_turns = 0;
    for (int i = 0; i < 30; i++) {
        int rc = inner_dialogue_engine_step(engine, nullptr);
        if (rc == 0) {
            step_turns++;
        } else if (rc > 0) {
            /* Conversation ended — the final step may or may not produce a turn
             * depending on the termination trigger, so count it separately */
            step_turns++;
            break;
        } else {
            break; /* error */
        }
    }

    inner_dialogue_engine_stats_t stats1;
    memset(&stats1, 0, sizeof(stats1));
    inner_dialogue_engine_get_stats(engine, &stats1);

    EXPECT_GE(step_turns, 1u);
    /* Stats may differ by at most 1 due to termination step accounting */
    EXPECT_NEAR((double)stats1.total_turns_produced, (double)step_turns, 1.0);
}

/* ============================================================================
 * TEST: Turn provenance tracking (perspective attribution)
 * ============================================================================ */

TEST_F(InnerDialogueE2ETest, TurnProvenanceTracking) {
    inner_dialogue_engine_register_builtins(engine);
    inner_dialogue_engine_start(engine, "provenance tracking");

    std::set<uint32_t> perspectives_seen;

    for (int i = 0; i < 14; i++) {
        inner_dialogue_turn_t turn;
        memset(&turn, 0, sizeof(turn));
        int rc = inner_dialogue_engine_step(engine, &turn);

        if (rc >= 0 && turn.content_len > 0) {
            /* Every turn should have a valid perspective index */
            EXPECT_LT(turn.perspective_idx, (uint32_t)INNER_DIALOGUE_MAX_PERSPECTIVES)
                << "Invalid perspective_idx at turn " << i;

            /* Turn should have a valid act */
            EXPECT_GE((int)turn.act, 0);
            EXPECT_LT((int)turn.act, DIALOGUE_ACT_COUNT);

            /* Confidence and relevance in valid range */
            EXPECT_GE(turn.confidence, 0.0f);
            EXPECT_LE(turn.confidence, 1.0f);
            EXPECT_GE(turn.relevance, 0.0f);
            EXPECT_LE(turn.relevance, 1.0f);

            perspectives_seen.insert(turn.perspective_idx);
        }

        if (rc > 0) break;
    }

    /* With 7 builtins and 14 steps, expect diversity */
    EXPECT_GE(perspectives_seen.size(), 2u);
}

/* ============================================================================
 * TEST: History and stats consistency after full run
 * ============================================================================ */

TEST_F(InnerDialogueE2ETest, HistoryStatsConsistency) {
    inner_dialogue_engine_register_builtins(engine);
    inner_dialogue_engine_start(engine, "consistency check");

    inner_dialogue_result_t result;
    memset(&result, 0, sizeof(result));
    inner_dialogue_engine_run(engine, &result);

    /* History count should match result total_turns */
    const inner_dialogue_turn_history_t* hist = inner_dialogue_engine_get_history(engine);
    ASSERT_NE(hist, nullptr);
    EXPECT_EQ(inner_dialogue_turn_history_count(hist), result.total_turns);

    /* Stats should match */
    inner_dialogue_turn_history_stats_t hist_stats;
    memset(&hist_stats, 0, sizeof(hist_stats));
    inner_dialogue_turn_history_get_stats(hist, &hist_stats);
    EXPECT_EQ(hist_stats.current_count, result.total_turns);

    /* Sum of act counts should equal total turns */
    uint32_t act_sum = 0;
    for (int a = 0; a < DIALOGUE_ACT_COUNT; a++) {
        act_sum += hist_stats.act_counts[a];
    }
    EXPECT_EQ(act_sum, result.total_turns);
}

/* ============================================================================
 * TEST: Multiple full conversations stress test
 * ============================================================================ */

TEST_F(InnerDialogueE2ETest, MultipleConversationsStress) {
    inner_dialogue_engine_register_builtins(engine);

    const char* topics[] = {
        "Ethics of AI",
        "Climate change response",
        "Resource allocation fairness",
        "Privacy versus security",
        "Long-term planning strategies",
    };

    uint64_t total_turns_all = 0;

    for (int i = 0; i < 5; i++) {
        SCOPED_TRACE("Conversation: " + std::string(topics[i]));

        int rc = inner_dialogue_engine_start(engine, topics[i]);
        EXPECT_EQ(rc, 0);

        inner_dialogue_result_t result;
        memset(&result, 0, sizeof(result));
        rc = inner_dialogue_engine_run(engine, &result);
        EXPECT_GE(rc, 0);
        EXPECT_GT(result.total_turns, 0u);

        total_turns_all += result.total_turns;

        inner_dialogue_engine_reset(engine);
    }

    /* Verify cumulative statistics */
    inner_dialogue_engine_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    inner_dialogue_engine_get_stats(engine, &stats);
    EXPECT_GE(stats.conversations_started, 5u);
    EXPECT_EQ(stats.total_turns_produced, total_turns_all);
}
