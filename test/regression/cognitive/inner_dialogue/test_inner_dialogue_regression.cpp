/**
 * @file test_inner_dialogue_regression.cpp
 * @brief Regression tests for Inner Dialogue Engine API contract stability
 *
 * WHAT: Prevent regressions in API contracts, error handling, boundary conditions
 * WHY:  Ensure behaviour is stable across versions; catch subtle contract violations
 * HOW:  Test NULL pointers, boundary values, error codes, circular buffer edge cases
 *
 * TESTS:
 *   - All functions with NULL inputs
 *   - Error code correctness
 *   - Boundary conditions (max turns, empty history, full registry)
 *   - Circular buffer wrapping correctness
 *   - Config validation
 *   - State machine invariants
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
 * Helper: stub formulate callback
 * ============================================================================ */
static bool stub_formulate(const perspective_turn_context_t* ctx,
                           inner_dialogue_turn_t* output) {
    (void)ctx;
    memset(output, 0, sizeof(*output));
    output->act = DIALOGUE_ACT_ASSERT;
    strncpy(output->content, "regression stub", INNER_DIALOGUE_TURN_MAX_CONTENT - 1);
    output->content_len = 15;
    output->confidence = 0.5f;
    output->relevance = 0.5f;
    output->novelty = 0.5f;
    output->agreement_with_prior = 0.5f;
    return true;
}

/* ============================================================================
 * TEST SUITE: NULL Safety Regression
 * ============================================================================ */

class NullSafetyRegressionTest : public ::testing::Test {};

TEST_F(NullSafetyRegressionTest, TurnHistoryAllNullInputs) {
    inner_dialogue_turn_t turn;
    memset(&turn, 0, sizeof(turn));
    inner_dialogue_turn_history_stats_t stats;

    /* Every function with NULL history should not crash */
    inner_dialogue_turn_history_destroy(nullptr);
    EXPECT_NE(inner_dialogue_turn_history_reset(nullptr), 0);
    EXPECT_EQ(inner_dialogue_turn_history_record(nullptr, &turn), -1);
    EXPECT_EQ(inner_dialogue_turn_history_get_latest(nullptr), nullptr);
    EXPECT_EQ(inner_dialogue_turn_history_get_at(nullptr, 0), nullptr);
    EXPECT_EQ(inner_dialogue_turn_history_get_by_id(nullptr, 0), nullptr);
    EXPECT_EQ(inner_dialogue_turn_history_count(nullptr), 0u);
    EXPECT_NE(inner_dialogue_turn_history_get_stats(nullptr, &stats), 0);
    EXPECT_LT(inner_dialogue_turn_history_act_entropy(nullptr, 0), 0.0f);
}

TEST_F(NullSafetyRegressionTest, TurnHistoryNullTurnPointers) {
    inner_dialogue_turn_history_t* history = inner_dialogue_turn_history_create();
    ASSERT_NE(history, nullptr);

    EXPECT_EQ(inner_dialogue_turn_history_record(history, nullptr), -1);
    EXPECT_NE(inner_dialogue_turn_history_get_stats(history, nullptr), 0);

    inner_dialogue_turn_history_destroy(history);
}

TEST_F(NullSafetyRegressionTest, ContentSimilarityNullInputs) {
    inner_dialogue_turn_t turn;
    memset(&turn, 0, sizeof(turn));
    strncpy(turn.content, "test", sizeof(turn.content) - 1);
    turn.content_len = 4;

    EXPECT_LT(inner_dialogue_turn_content_similarity(nullptr, &turn), 0.0f);
    EXPECT_LT(inner_dialogue_turn_content_similarity(&turn, nullptr), 0.0f);
    EXPECT_LT(inner_dialogue_turn_content_similarity(nullptr, nullptr), 0.0f);
}

TEST_F(NullSafetyRegressionTest, PerspectiveRegistryNullInputs) {
    inner_dialogue_perspective_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.formulate = stub_formulate;

    EXPECT_NE(inner_dialogue_perspective_registry_init(nullptr), 0);
    EXPECT_NE(inner_dialogue_perspective_register(nullptr, &desc), 0);
    EXPECT_EQ(inner_dialogue_perspective_find(nullptr, PERSPECTIVE_ANALYTICAL), nullptr);
    EXPECT_EQ(inner_dialogue_perspective_count(nullptr), 0u);
}

TEST_F(NullSafetyRegressionTest, ConvergenceNullInputs) {
    convergence_analysis_t analysis;
    convergence_config_t config = inner_dialogue_convergence_default_config();

    EXPECT_NE(inner_dialogue_convergence_analyse(nullptr, &config, &analysis), 0);
    EXPECT_LT(inner_dialogue_convergence_agreement(nullptr, 8), 0.0f);
    EXPECT_FLOAT_EQ(inner_dialogue_convergence_trend(nullptr, 8), 0.0f);
    EXPECT_LT(inner_dialogue_convergence_deadlock(nullptr, 8), 0.0f);
    EXPECT_LT(inner_dialogue_convergence_rumination(nullptr, 8), 0.0f);
    EXPECT_LT(inner_dialogue_convergence_emotional_temperature(nullptr, 8), 0.0f);
    EXPECT_LT(inner_dialogue_convergence_perspective_entropy(nullptr, 0), 0.0f);
}

TEST_F(NullSafetyRegressionTest, EngineNullInputs) {
    inner_dialogue_turn_t turn;
    inner_dialogue_result_t result;
    convergence_analysis_t analysis;
    inner_dialogue_engine_stats_t stats;

    /* All engine functions with NULL engine */
    inner_dialogue_engine_destroy(nullptr);
    EXPECT_NE(inner_dialogue_engine_reset(nullptr), 0);
    EXPECT_NE(inner_dialogue_engine_set_health_agent(nullptr, nullptr), 0);
    EXPECT_NE(inner_dialogue_engine_set_immune(nullptr, nullptr), 0);
    EXPECT_NE(inner_dialogue_engine_set_bbb(nullptr, nullptr), 0);
    EXPECT_NE(inner_dialogue_engine_set_bio_router(nullptr, nullptr), 0);
    EXPECT_NE(inner_dialogue_engine_set_cycle_coordinator(nullptr, nullptr), 0);
    EXPECT_NE(inner_dialogue_engine_set_ethics(nullptr, nullptr), 0);
    EXPECT_NE(inner_dialogue_engine_set_lgss(nullptr, nullptr), 0);
    EXPECT_EQ(inner_dialogue_engine_get_registry(nullptr), nullptr);
    EXPECT_NE(inner_dialogue_engine_start(nullptr, "topic"), 0);
    EXPECT_LT(inner_dialogue_engine_step(nullptr, &turn), 0);
    EXPECT_LT(inner_dialogue_engine_run(nullptr, &result), 0);
    EXPECT_NE(inner_dialogue_engine_cancel(nullptr), 0);
    EXPECT_EQ(inner_dialogue_engine_get_state(nullptr), DIALOGUE_STATE_IDLE);
    EXPECT_EQ(inner_dialogue_engine_get_history(nullptr), nullptr);
    EXPECT_EQ(inner_dialogue_engine_get_topic(nullptr), nullptr);
    EXPECT_EQ(inner_dialogue_engine_get_turn_number(nullptr), 0u);
    EXPECT_NE(inner_dialogue_engine_get_convergence(nullptr, &analysis), 0);
    EXPECT_NE(inner_dialogue_engine_get_stats(nullptr, &stats), 0);
}

/* ============================================================================
 * TEST SUITE: Boundary Conditions
 * ============================================================================ */

class BoundaryRegressionTest : public ::testing::Test {};

TEST_F(BoundaryRegressionTest, HistoryCircularBufferExactCapacity) {
    inner_dialogue_turn_history_t* history = inner_dialogue_turn_history_create();
    ASSERT_NE(history, nullptr);

    /* Fill to exactly capacity */
    for (uint32_t i = 0; i < INNER_DIALOGUE_MAX_HISTORY; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "turn_%u", i);
        inner_dialogue_turn_t turn;
        memset(&turn, 0, sizeof(turn));
        turn.act = DIALOGUE_ACT_ASSERT;
        turn.perspective_idx = i % 4;
        strncpy(turn.content, buf, INNER_DIALOGUE_TURN_MAX_CONTENT - 1);
        turn.content_len = (uint32_t)strlen(buf);
        turn.confidence = 0.5f;

        int id = inner_dialogue_turn_history_record(history, &turn);
        EXPECT_GE(id, 0) << "Failed at turn " << i;
    }

    EXPECT_EQ(inner_dialogue_turn_history_count(history), (uint32_t)INNER_DIALOGUE_MAX_HISTORY);

    /* Verify first and last entries */
    const inner_dialogue_turn_t* newest = inner_dialogue_turn_history_get_at(history, 0);
    ASSERT_NE(newest, nullptr);
    char expected_newest[32];
    snprintf(expected_newest, sizeof(expected_newest), "turn_%u", INNER_DIALOGUE_MAX_HISTORY - 1);
    EXPECT_STREQ(newest->content, expected_newest);

    const inner_dialogue_turn_t* oldest = inner_dialogue_turn_history_get_at(history, INNER_DIALOGUE_MAX_HISTORY - 1);
    ASSERT_NE(oldest, nullptr);
    EXPECT_STREQ(oldest->content, "turn_0");

    inner_dialogue_turn_history_destroy(history);
}

TEST_F(BoundaryRegressionTest, HistoryWrapsCorrectly) {
    inner_dialogue_turn_history_t* history = inner_dialogue_turn_history_create();
    ASSERT_NE(history, nullptr);

    /* Fill past capacity by 5 entries */
    uint32_t total = INNER_DIALOGUE_MAX_HISTORY + 5;
    for (uint32_t i = 0; i < total; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "wrap_%u", i);
        inner_dialogue_turn_t turn;
        memset(&turn, 0, sizeof(turn));
        turn.act = DIALOGUE_ACT_ELABORATE;
        strncpy(turn.content, buf, INNER_DIALOGUE_TURN_MAX_CONTENT - 1);
        turn.content_len = (uint32_t)strlen(buf);
        turn.confidence = 0.5f;
        inner_dialogue_turn_history_record(history, &turn);
    }

    /* Count should be capped */
    EXPECT_EQ(inner_dialogue_turn_history_count(history), (uint32_t)INNER_DIALOGUE_MAX_HISTORY);

    /* Newest should be the last entry */
    const inner_dialogue_turn_t* newest = inner_dialogue_turn_history_get_latest(history);
    ASSERT_NE(newest, nullptr);
    char expected[32];
    snprintf(expected, sizeof(expected), "wrap_%u", total - 1);
    EXPECT_STREQ(newest->content, expected);

    /* Oldest should be entry 5 (first 5 were overwritten) */
    const inner_dialogue_turn_t* oldest_in_buffer = inner_dialogue_turn_history_get_at(history, INNER_DIALOGUE_MAX_HISTORY - 1);
    ASSERT_NE(oldest_in_buffer, nullptr);
    snprintf(expected, sizeof(expected), "wrap_%u", 5u);
    EXPECT_STREQ(oldest_in_buffer->content, expected);

    inner_dialogue_turn_history_destroy(history);
}

TEST_F(BoundaryRegressionTest, EmptyHistoryAnalysisHandled) {
    inner_dialogue_turn_history_t* history = inner_dialogue_turn_history_create();
    ASSERT_NE(history, nullptr);

    convergence_config_t config = inner_dialogue_convergence_default_config();
    convergence_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));

    int rc = inner_dialogue_convergence_analyse(history, &config, &analysis);
    /* Should either fail gracefully or return TERMINATION_NONE */
    if (rc == 0) {
        EXPECT_EQ(analysis.recommended_action, TERMINATION_NONE);
    }

    inner_dialogue_turn_history_destroy(history);
}

TEST_F(BoundaryRegressionTest, PerspectiveRegistryFullThenClearThenRefill) {
    inner_dialogue_perspective_registry_t registry;
    inner_dialogue_perspective_registry_init(&registry);

    /* Fill to max */
    for (uint32_t i = 0; i < INNER_DIALOGUE_MAX_PERSPECTIVES; i++) {
        inner_dialogue_perspective_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.type = (perspective_type_t)(PERSPECTIVE_CUSTOM_START + i);
        snprintf(desc.name, sizeof(desc.name), "p%u", i);
        desc.formulate = stub_formulate;
        EXPECT_EQ(inner_dialogue_perspective_register(&registry, &desc), 0);
    }
    EXPECT_EQ(inner_dialogue_perspective_count(&registry), (uint32_t)INNER_DIALOGUE_MAX_PERSPECTIVES);

    /* Clear */
    inner_dialogue_perspective_registry_clear(&registry);
    EXPECT_EQ(inner_dialogue_perspective_count(&registry), 0u);

    /* Refill should work */
    for (uint32_t i = 0; i < INNER_DIALOGUE_MAX_PERSPECTIVES; i++) {
        inner_dialogue_perspective_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.type = (perspective_type_t)(PERSPECTIVE_CUSTOM_START + i);
        snprintf(desc.name, sizeof(desc.name), "q%u", i);
        desc.formulate = stub_formulate;
        EXPECT_EQ(inner_dialogue_perspective_register(&registry, &desc), 0);
    }
    EXPECT_EQ(inner_dialogue_perspective_count(&registry), (uint32_t)INNER_DIALOGUE_MAX_PERSPECTIVES);
}

/* ============================================================================
 * TEST SUITE: Error Code Regression
 * ============================================================================ */

class ErrorCodeRegressionTest : public ::testing::Test {};

TEST_F(ErrorCodeRegressionTest, ErrorCodeRangesCorrect) {
    /* Turn errors: 29000-29099 */
    EXPECT_GE(NIMCP_INNER_DIALOGUE_TURN_ERROR_NULL, 29000);
    EXPECT_LT(NIMCP_INNER_DIALOGUE_TURN_ERROR_NULL, 29100);

    /* Perspective errors: 29100-29199 */
    EXPECT_GE(NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_NULL, 29100);
    EXPECT_LT(NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_NULL, 29200);

    /* Convergence errors: 29200-29299 */
    EXPECT_GE(NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_NULL, 29200);
    EXPECT_LT(NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_NULL, 29300);

    /* Engine errors: 29300-29499 */
    EXPECT_GE(NIMCP_INNER_DIALOGUE_ERROR_NULL, 29300);
    EXPECT_LT(NIMCP_INNER_DIALOGUE_ERROR_NULL, 29500);
}

TEST_F(ErrorCodeRegressionTest, EthicsAndLgssErrorCodesExist) {
    EXPECT_EQ(NIMCP_INNER_DIALOGUE_ERROR_ETHICS_REJECTED, 29315);
    EXPECT_EQ(NIMCP_INNER_DIALOGUE_ERROR_LGSS_DENIED, 29316);
}

TEST_F(ErrorCodeRegressionTest, AllErrorCodesUnique) {
    /* Engine error codes should all be distinct */
    int codes[] = {
        NIMCP_INNER_DIALOGUE_ERROR_NULL,
        NIMCP_INNER_DIALOGUE_ERROR_NO_MEMORY,
        NIMCP_INNER_DIALOGUE_ERROR_INVALID_CONFIG,
        NIMCP_INNER_DIALOGUE_ERROR_INVALID_STATE,
        NIMCP_INNER_DIALOGUE_ERROR_ALREADY_RUNNING,
        NIMCP_INNER_DIALOGUE_ERROR_NOT_RUNNING,
        NIMCP_INNER_DIALOGUE_ERROR_NO_PERSPECTIVES,
        NIMCP_INNER_DIALOGUE_ERROR_TURN_FAILED,
        NIMCP_INNER_DIALOGUE_ERROR_CONVERGENCE_CHECK,
        NIMCP_INNER_DIALOGUE_ERROR_MUTEX,
        NIMCP_INNER_DIALOGUE_ERROR_BBB_REJECTED,
        NIMCP_INNER_DIALOGUE_ERROR_IMMUNE_SUPPRESSED,
        NIMCP_INNER_DIALOGUE_ERROR_MAX_TURNS,
        NIMCP_INNER_DIALOGUE_ERROR_CANCELLED,
        NIMCP_INNER_DIALOGUE_ERROR_ETHICS_REJECTED,
        NIMCP_INNER_DIALOGUE_ERROR_LGSS_DENIED,
    };
    int count = sizeof(codes) / sizeof(codes[0]);

    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            EXPECT_NE(codes[i], codes[j])
                << "Duplicate error code: " << codes[i] << " at indices " << i << " and " << j;
        }
    }
}

/* ============================================================================
 * TEST SUITE: State Machine Invariants
 * ============================================================================ */

class StateMachineRegressionTest : public ::testing::Test {
protected:
    inner_dialogue_engine_t* engine = nullptr;

    void SetUp() override {
        inner_dialogue_config_t cfg = inner_dialogue_default_config();
        cfg.max_turns = 8;
        cfg.enable_ethics_evaluation = false;
        cfg.enable_lgss_evaluation = false;
        cfg.enable_bbb_validation = false;
        cfg.enable_bio_async = false;
        cfg.enable_immune_integration = false;
        cfg.enable_health_heartbeat = false;

        engine = inner_dialogue_engine_create(&cfg);
        ASSERT_NE(engine, nullptr);
        inner_dialogue_engine_register_builtins(engine);
    }

    void TearDown() override {
        inner_dialogue_engine_destroy(engine);
    }
};

TEST_F(StateMachineRegressionTest, CannotStepInIdleState) {
    inner_dialogue_turn_t turn;
    int rc = inner_dialogue_engine_step(engine, &turn);
    EXPECT_LT(rc, 0);
}

TEST_F(StateMachineRegressionTest, CannotStartTwice) {
    EXPECT_EQ(inner_dialogue_engine_start(engine, "first"), 0);

    /* Starting again while active should fail */
    int rc = inner_dialogue_engine_start(engine, "second");
    EXPECT_NE(rc, 0);
}

TEST_F(StateMachineRegressionTest, CannotCancelIdle) {
    int rc = inner_dialogue_engine_cancel(engine);
    EXPECT_NE(rc, 0);
}

TEST_F(StateMachineRegressionTest, ResetAllowsNewConversation) {
    inner_dialogue_engine_start(engine, "first conv");
    inner_dialogue_result_t result;
    memset(&result, 0, sizeof(result));
    inner_dialogue_engine_run(engine, &result);

    /* After completion, must reset before starting again */
    inner_dialogue_engine_reset(engine);
    EXPECT_EQ(inner_dialogue_engine_get_state(engine), DIALOGUE_STATE_IDLE);

    int rc = inner_dialogue_engine_start(engine, "second conv");
    EXPECT_EQ(rc, 0);
}

TEST_F(StateMachineRegressionTest, MaxTurnsEnforcedByRun) {
    inner_dialogue_engine_start(engine, "max turns test");

    inner_dialogue_result_t result;
    memset(&result, 0, sizeof(result));
    inner_dialogue_engine_run(engine, &result);

    /* Should not exceed max_turns (8) */
    EXPECT_LE(result.total_turns, 8u);
}

/* ============================================================================
 * TEST SUITE: Config Validation Regression
 * ============================================================================ */

TEST(ConfigRegressionTest, DefaultConfigIsValid) {
    inner_dialogue_config_t cfg = inner_dialogue_default_config();

    EXPECT_GT(cfg.max_turns, 0u);
    EXPECT_GE(cfg.urgency, 0.0f);
    EXPECT_LE(cfg.urgency, 1.0f);
    EXPECT_GE(cfg.min_relevance_threshold, 0.0f);
    EXPECT_LE(cfg.min_relevance_threshold, 1.0f);

    /* Config should produce a valid engine */
    inner_dialogue_engine_t* eng = inner_dialogue_engine_create(&cfg);
    EXPECT_NE(eng, nullptr);
    inner_dialogue_engine_destroy(eng);
}

TEST(ConfigRegressionTest, DefaultConvergenceConfigIsValid) {
    convergence_config_t cfg = inner_dialogue_convergence_default_config();

    EXPECT_GE(cfg.agreement_threshold, 0.0f);
    EXPECT_LE(cfg.agreement_threshold, 1.0f);
    EXPECT_GE(cfg.deadlock_threshold, 0.0f);
    EXPECT_LE(cfg.deadlock_threshold, 1.0f);
    EXPECT_GE(cfg.rumination_threshold, 0.0f);
    EXPECT_LE(cfg.rumination_threshold, 1.0f);
    EXPECT_GE(cfg.emotional_spiral_threshold, 0.0f);
    EXPECT_LE(cfg.emotional_spiral_threshold, 1.0f);
    EXPECT_GT(cfg.trend_window, 0u);
}

/* ============================================================================
 * TEST SUITE: String Conversion Regression (stable across versions)
 * ============================================================================ */

TEST(StringConversionRegressionTest, DialogueActStrings) {
    EXPECT_STREQ(dialogue_act_to_string(DIALOGUE_ACT_ASSERT), "ASSERT");
    EXPECT_STREQ(dialogue_act_to_string(DIALOGUE_ACT_QUESTION), "QUESTION");
    EXPECT_STREQ(dialogue_act_to_string(DIALOGUE_ACT_CHALLENGE), "CHALLENGE");
    EXPECT_STREQ(dialogue_act_to_string(DIALOGUE_ACT_ELABORATE), "ELABORATE");
    EXPECT_STREQ(dialogue_act_to_string(DIALOGUE_ACT_SYNTHESIZE), "SYNTHESIZE");
    EXPECT_STREQ(dialogue_act_to_string(DIALOGUE_ACT_CONCLUDE), "CONCLUDE");
    EXPECT_STREQ(dialogue_act_to_string(DIALOGUE_ACT_DEFER), "DEFER");
    EXPECT_STREQ(dialogue_act_to_string(DIALOGUE_ACT_INTROSPECT), "INTROSPECT");
    EXPECT_STREQ(dialogue_act_to_string(DIALOGUE_ACT_REFRAME), "REFRAME");
    EXPECT_STREQ(dialogue_act_to_string(DIALOGUE_ACT_WARN), "WARN");
}

TEST(StringConversionRegressionTest, PerspectiveTypeStrings) {
    EXPECT_STREQ(perspective_type_to_string(PERSPECTIVE_ANALYTICAL), "ANALYTICAL");
    EXPECT_STREQ(perspective_type_to_string(PERSPECTIVE_EMOTIONAL), "EMOTIONAL");
    EXPECT_STREQ(perspective_type_to_string(PERSPECTIVE_CRITICAL), "CRITICAL");
    EXPECT_STREQ(perspective_type_to_string(PERSPECTIVE_CREATIVE), "CREATIVE");
    EXPECT_STREQ(perspective_type_to_string(PERSPECTIVE_MEMORY), "MEMORY");
    EXPECT_STREQ(perspective_type_to_string(PERSPECTIVE_ETHICAL), "ETHICAL");
    EXPECT_STREQ(perspective_type_to_string(PERSPECTIVE_METACOGNITIVE), "METACOGNITIVE");
}

TEST(StringConversionRegressionTest, EngineStateStrings) {
    EXPECT_STREQ(inner_dialogue_state_to_string(DIALOGUE_STATE_IDLE), "IDLE");
    EXPECT_STREQ(inner_dialogue_state_to_string(DIALOGUE_STATE_INITIATED), "INITIATED");
    EXPECT_STREQ(inner_dialogue_state_to_string(DIALOGUE_STATE_DELIBERATING), "DELIBERATING");
    EXPECT_STREQ(inner_dialogue_state_to_string(DIALOGUE_STATE_CONVERGING), "CONVERGING");
    EXPECT_STREQ(inner_dialogue_state_to_string(DIALOGUE_STATE_DEADLOCKED), "DEADLOCKED");
    EXPECT_STREQ(inner_dialogue_state_to_string(DIALOGUE_STATE_RUMINATING), "RUMINATING");
    EXPECT_STREQ(inner_dialogue_state_to_string(DIALOGUE_STATE_ESCALATED), "ESCALATED");
    EXPECT_STREQ(inner_dialogue_state_to_string(DIALOGUE_STATE_CONCLUDED), "CONCLUDED");
    EXPECT_STREQ(inner_dialogue_state_to_string(DIALOGUE_STATE_CANCELLED), "CANCELLED");
}

TEST(StringConversionRegressionTest, TerminationReasonStrings) {
    EXPECT_STREQ(termination_reason_to_string(TERMINATION_NONE), "NONE");
    EXPECT_STREQ(termination_reason_to_string(TERMINATION_CONVERGED), "CONVERGED");
    EXPECT_STREQ(termination_reason_to_string(TERMINATION_MAX_TURNS), "MAX_TURNS");
    EXPECT_STREQ(termination_reason_to_string(TERMINATION_DEADLOCKED), "DEADLOCKED");
    EXPECT_STREQ(termination_reason_to_string(TERMINATION_RUMINATING), "RUMINATING");
    EXPECT_STREQ(termination_reason_to_string(TERMINATION_EMOTIONAL_SPIRAL), "EMOTIONAL_SPIRAL");
    EXPECT_STREQ(termination_reason_to_string(TERMINATION_SUBSTRATE_SUPPRESSED), "SUBSTRATE_SUPPRESSED");
    EXPECT_STREQ(termination_reason_to_string(TERMINATION_CANCELLED), "CANCELLED");
    EXPECT_STREQ(termination_reason_to_string(TERMINATION_ESCALATED), "ESCALATED");
}

/* ============================================================================
 * TEST SUITE: Constants Regression (values should never change)
 * ============================================================================ */

TEST(ConstantsRegressionTest, TurnConstants) {
    EXPECT_EQ(INNER_DIALOGUE_TURN_MAX_CONTENT, 512);
    EXPECT_EQ(INNER_DIALOGUE_MAX_HISTORY, 128);
    EXPECT_EQ(INNER_DIALOGUE_TURN_MAX_TAGS, 4);
    EXPECT_EQ(INNER_DIALOGUE_TURN_TAG_LEN, 32);
}

TEST(ConstantsRegressionTest, PerspectiveConstants) {
    EXPECT_EQ(INNER_DIALOGUE_MAX_PERSPECTIVES, 16);
    EXPECT_EQ(INNER_DIALOGUE_PERSPECTIVE_NAME_LEN, 64);
    EXPECT_EQ(PERSPECTIVE_CUSTOM_START, 32);
}

TEST(ConstantsRegressionTest, ConvergenceConstants) {
    EXPECT_EQ(INNER_DIALOGUE_CONVERGENCE_MIN_TURNS, 3);
    EXPECT_FLOAT_EQ(INNER_DIALOGUE_DEFAULT_AGREEMENT_THRESHOLD, 0.75f);
    EXPECT_FLOAT_EQ(INNER_DIALOGUE_DEFAULT_DEADLOCK_THRESHOLD, 0.70f);
    EXPECT_FLOAT_EQ(INNER_DIALOGUE_DEFAULT_RUMINATION_THRESHOLD, 0.65f);
    EXPECT_FLOAT_EQ(INNER_DIALOGUE_DEFAULT_EMOTIONAL_SPIRAL_THRESHOLD, 0.80f);
    EXPECT_EQ(INNER_DIALOGUE_CONVERGENCE_TREND_WINDOW, 8);
}

TEST(ConstantsRegressionTest, EngineConstants) {
    EXPECT_STREQ(INNER_DIALOGUE_VERSION, "1.0.0");
    EXPECT_EQ(INNER_DIALOGUE_MAGIC, 0x494E444Cu);
    EXPECT_EQ(INNER_DIALOGUE_MAX_TOPIC_LEN, 256);
    EXPECT_EQ(INNER_DIALOGUE_DEFAULT_MAX_TURNS, 32);
    EXPECT_FLOAT_EQ(INNER_DIALOGUE_DEFAULT_URGENCY, 0.5f);
    EXPECT_FLOAT_EQ(INNER_DIALOGUE_MIN_RELEVANCE_THRESHOLD, 0.1f);
}

TEST(ConstantsRegressionTest, EnumCounts) {
    EXPECT_EQ(DIALOGUE_ACT_COUNT, 10);
    EXPECT_EQ(DIALOGUE_STATE_COUNT, 9);
    EXPECT_EQ(PERSPECTIVE_BUILTIN_COUNT, 7);
    EXPECT_EQ(TERMINATION_COUNT, 9);
}
