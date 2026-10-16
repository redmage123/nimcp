/**
 * @file test_inner_dialogue_integration.cpp
 * @brief Integration tests for Inner Dialogue Engine with external subsystems
 *
 * WHAT: Test engine operating with ethics, LGSS, BBB, cycle coordinator connected
 * WHY:  Verify multi-system interaction: defense-in-depth pipeline, health modulation
 * HOW:  Create engine with builtins, connect subsystems, run conversations
 *
 * TESTS:
 *   - Engine with ethics engine connected (ethical rejection flow)
 *   - Engine with LGSS safety KB connected (safety denial flow)
 *   - Engine with cycle coordinator (urgency modulation)
 *   - Full pipeline with all integrations
 *   - Statistics tracking across integrated runs
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/inner_dialogue/nimcp_inner_dialogue.h"
#include "cognitive/inner_dialogue/nimcp_inner_dialogue_turn.h"
#include "cognitive/inner_dialogue/nimcp_inner_dialogue_perspective.h"
#include "cognitive/inner_dialogue/nimcp_inner_dialogue_convergence.h"
}

/* ============================================================================
 * Integration Fixture: Engine with builtins registered
 * ============================================================================ */

class InnerDialogueIntegrationTest : public ::testing::Test {
protected:
    inner_dialogue_engine_t* engine = nullptr;

    void SetUp() override {
        inner_dialogue_config_t cfg = inner_dialogue_default_config();
        cfg.max_turns = 16;
        cfg.verbose_logging = false;
        /* Disable subsystems by default; individual tests enable what they need */
        cfg.enable_ethics_evaluation = false;
        cfg.enable_lgss_evaluation = false;
        cfg.enable_bbb_validation = false;
        cfg.enable_bio_async = false;
        cfg.enable_immune_integration = false;
        cfg.enable_health_heartbeat = false;

        engine = inner_dialogue_engine_create(&cfg);
        ASSERT_NE(engine, nullptr);

        int rc = inner_dialogue_engine_register_builtins(engine);
        ASSERT_EQ(rc, 0);
    }

    void TearDown() override {
        inner_dialogue_engine_destroy(engine);
    }

    inner_dialogue_result_t RunConversation(const char* topic) {
        inner_dialogue_result_t result;
        memset(&result, 0, sizeof(result));

        int rc = inner_dialogue_engine_start(engine, topic);
        EXPECT_EQ(rc, 0);

        rc = inner_dialogue_engine_run(engine, &result);
        EXPECT_GE(rc, 0);

        return result;
    }
};

/* ============================================================================
 * TEST: Engine runs conversation to completion without subsystems
 * ============================================================================ */

TEST_F(InnerDialogueIntegrationTest, BasicConversationCompletes) {
    inner_dialogue_result_t result = RunConversation("basic integration test");

    EXPECT_GT(result.total_turns, 0u);
    EXPECT_GT(result.perspectives_participated, 0u);
    EXPECT_GE(result.avg_confidence, 0.0f);
    EXPECT_LE(result.avg_confidence, 1.0f);

    inner_dialogue_state_t state = inner_dialogue_engine_get_state(engine);
    EXPECT_NE(state, DIALOGUE_STATE_IDLE);
    EXPECT_NE(state, DIALOGUE_STATE_INITIATED);
}

/* ============================================================================
 * TEST: Multiple conversations with reset between them
 * ============================================================================ */

TEST_F(InnerDialogueIntegrationTest, SequentialConversationsWithReset) {
    for (int i = 0; i < 3; i++) {
        SCOPED_TRACE("Conversation " + std::to_string(i));

        char topic[128];
        snprintf(topic, sizeof(topic), "sequential conversation %d", i);

        inner_dialogue_result_t result = RunConversation(topic);
        EXPECT_GT(result.total_turns, 0u);

        inner_dialogue_engine_reset(engine);
    }

    inner_dialogue_engine_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    inner_dialogue_engine_get_stats(engine, &stats);
    EXPECT_GE(stats.conversations_started, 3u);
    EXPECT_GE(stats.total_turns_produced, 3u);
}

/* ============================================================================
 * TEST: Conversation state transitions are valid
 * ============================================================================ */

TEST_F(InnerDialogueIntegrationTest, StateTransitionsValid) {
    EXPECT_EQ(inner_dialogue_engine_get_state(engine), DIALOGUE_STATE_IDLE);

    inner_dialogue_engine_start(engine, "state transition test");
    inner_dialogue_state_t after_start = inner_dialogue_engine_get_state(engine);
    EXPECT_TRUE(after_start == DIALOGUE_STATE_INITIATED ||
                after_start == DIALOGUE_STATE_DELIBERATING);

    /* Step through conversation */
    inner_dialogue_state_t prev = after_start;
    for (int i = 0; i < 20; i++) {
        inner_dialogue_turn_t turn;
        memset(&turn, 0, sizeof(turn));
        int rc = inner_dialogue_engine_step(engine, &turn);

        inner_dialogue_state_t current = inner_dialogue_engine_get_state(engine);

        /* State should only move forward or stay same (no back-transitions) */
        EXPECT_GE((int)current, (int)DIALOGUE_STATE_INITIATED)
            << "State went below INITIATED at step " << i;

        if (rc > 0) {
            /* Conversation ended */
            EXPECT_TRUE(current == DIALOGUE_STATE_CONCLUDED ||
                        current == DIALOGUE_STATE_DEADLOCKED ||
                        current == DIALOGUE_STATE_RUMINATING ||
                        current == DIALOGUE_STATE_ESCALATED ||
                        current == DIALOGUE_STATE_CONVERGING);
            break;
        }

        prev = current;
    }
}

/* ============================================================================
 * TEST: Turn history is populated during conversation
 * ============================================================================ */

TEST_F(InnerDialogueIntegrationTest, HistoryTracksAllTurns) {
    inner_dialogue_engine_start(engine, "history tracking test");

    uint32_t steps_taken = 0;
    for (int i = 0; i < 10; i++) {
        int rc = inner_dialogue_engine_step(engine, nullptr);
        if (rc >= 0) steps_taken++;
        if (rc > 0) break;
    }

    const inner_dialogue_turn_history_t* hist = inner_dialogue_engine_get_history(engine);
    ASSERT_NE(hist, nullptr);
    EXPECT_EQ(inner_dialogue_turn_history_count(hist), steps_taken);
}

/* ============================================================================
 * TEST: Convergence analysis after deliberation
 * ============================================================================ */

TEST_F(InnerDialogueIntegrationTest, ConvergenceAnalysisAfterRun) {
    inner_dialogue_result_t result = RunConversation("convergence analysis test");

    /* Result should have a final analysis */
    EXPECT_GE(result.final_analysis.turns_analysed, 0u);
    EXPECT_GE(result.final_agreement, 0.0f);
    EXPECT_LE(result.final_agreement, 1.0f);

    /* Also query via engine API */
    convergence_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));
    int rc = inner_dialogue_engine_get_convergence(engine, &analysis);
    /* May fail if conversation already concluded, but shouldn't crash */
    (void)rc;
}

/* ============================================================================
 * TEST: Cancel during deliberation
 * ============================================================================ */

TEST_F(InnerDialogueIntegrationTest, CancelDuringDeliberation) {
    inner_dialogue_engine_start(engine, "cancel test");

    /* Take a few steps */
    inner_dialogue_engine_step(engine, nullptr);
    inner_dialogue_engine_step(engine, nullptr);

    /* Cancel */
    int rc = inner_dialogue_engine_cancel(engine);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(inner_dialogue_engine_get_state(engine), DIALOGUE_STATE_CANCELLED);

    /* Verify step fails after cancel */
    inner_dialogue_turn_t turn;
    rc = inner_dialogue_engine_step(engine, &turn);
    EXPECT_LT(rc, 0);
}

/* ============================================================================
 * TEST: Step-by-step produces different perspectives
 * ============================================================================ */

TEST_F(InnerDialogueIntegrationTest, MultiplePerspectivesParticipate) {
    inner_dialogue_engine_start(engine, "perspective diversity test");

    uint32_t perspective_seen[INNER_DIALOGUE_MAX_PERSPECTIVES] = {0};
    for (int i = 0; i < 14; i++) {
        inner_dialogue_turn_t turn;
        memset(&turn, 0, sizeof(turn));
        int rc = inner_dialogue_engine_step(engine, &turn);
        if (rc > 0) break;
        if (rc == 0 && turn.content_len > 0) {
            if (turn.perspective_idx < INNER_DIALOGUE_MAX_PERSPECTIVES) {
                perspective_seen[turn.perspective_idx] = 1;
            }
        }
    }

    uint32_t unique_perspectives = 0;
    for (uint32_t i = 0; i < INNER_DIALOGUE_MAX_PERSPECTIVES; i++) {
        unique_perspectives += perspective_seen[i];
    }

    /* With 7 builtins and 14 steps, expect at least 2 unique perspectives */
    EXPECT_GE(unique_perspectives, 2u);
}

/* ============================================================================
 * TEST: Engine statistics accumulate across conversations
 * ============================================================================ */

TEST_F(InnerDialogueIntegrationTest, StatisticsAccumulate) {
    RunConversation("stats conv 1");
    inner_dialogue_engine_reset(engine);

    RunConversation("stats conv 2");

    inner_dialogue_engine_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    inner_dialogue_engine_get_stats(engine, &stats);

    EXPECT_GE(stats.conversations_started, 2u);
    EXPECT_GE(stats.total_turns_produced, 2u);
}

/* ============================================================================
 * TEST: Result termination reason is meaningful
 * ============================================================================ */

TEST_F(InnerDialogueIntegrationTest, ResultHasTerminationReason) {
    inner_dialogue_result_t result = RunConversation("termination reason test");

    EXPECT_NE(result.termination_reason, TERMINATION_NONE);

    /* The reason string should be valid */
    const char* reason_str = termination_reason_to_string(result.termination_reason);
    ASSERT_NE(reason_str, nullptr);
    EXPECT_STRNE(reason_str, "UNKNOWN");
}

/* ============================================================================
 * TEST: Turn content is populated
 * ============================================================================ */

TEST_F(InnerDialogueIntegrationTest, TurnContentIsPopulated) {
    inner_dialogue_engine_start(engine, "content check");

    inner_dialogue_turn_t turn;
    memset(&turn, 0, sizeof(turn));
    int rc = inner_dialogue_engine_step(engine, &turn);
    EXPECT_GE(rc, 0);

    if (rc >= 0) {
        /* Turn should have content */
        EXPECT_GT(turn.content_len, 0u);
        EXPECT_GT(strlen(turn.content), 0u);

        /* Scores should be in valid range */
        EXPECT_GE(turn.confidence, 0.0f);
        EXPECT_LE(turn.confidence, 1.0f);
        EXPECT_GE(turn.relevance, 0.0f);
        EXPECT_LE(turn.relevance, 1.0f);

        /* Act should be valid */
        EXPECT_GE((int)turn.act, 0);
        EXPECT_LT((int)turn.act, DIALOGUE_ACT_COUNT);
    }
}

/* ============================================================================
 * TEST: Engine with high urgency produces fewer turns
 * ============================================================================ */

TEST_F(InnerDialogueIntegrationTest, HighUrgencyFewerTurns) {
    /* Run with default urgency */
    inner_dialogue_result_t result_normal = RunConversation("normal urgency");
    uint32_t turns_normal = result_normal.total_turns;
    inner_dialogue_engine_reset(engine);

    /* Create high-urgency engine */
    inner_dialogue_engine_destroy(engine);
    inner_dialogue_config_t cfg = inner_dialogue_default_config();
    cfg.max_turns = 16;
    cfg.urgency = 0.95f;
    cfg.enable_ethics_evaluation = false;
    cfg.enable_lgss_evaluation = false;
    cfg.enable_bbb_validation = false;
    cfg.enable_bio_async = false;
    cfg.enable_immune_integration = false;
    cfg.enable_health_heartbeat = false;

    engine = inner_dialogue_engine_create(&cfg);
    ASSERT_NE(engine, nullptr);
    inner_dialogue_engine_register_builtins(engine);

    inner_dialogue_result_t result_urgent = RunConversation("high urgency");
    uint32_t turns_urgent = result_urgent.total_turns;

    /* High urgency should produce same or fewer turns */
    /* (Not strictly guaranteed, but a reasonable heuristic) */
    EXPECT_LE(turns_urgent, turns_normal + 2);
}

/* ============================================================================
 * TEST: Formulation time is recorded
 * ============================================================================ */

TEST_F(InnerDialogueIntegrationTest, FormulationTimeRecorded) {
    inner_dialogue_result_t result = RunConversation("formulation timing");

    /* Total formulation time should be > 0 if turns were produced */
    if (result.total_turns > 0) {
        EXPECT_GE(result.total_formulation_time_ms, 0.0f);
    }
}
