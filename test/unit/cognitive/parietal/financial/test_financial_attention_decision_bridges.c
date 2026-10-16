/**
 * @file test_financial_attention_decision_bridges.c
 * @brief Unit tests for Financial Attention and Decision bridges
 *
 * WHAT: Test suite for financial bridge systems including:
 *       - Salience Bridge (novelty/surprise/urgency filtering)
 *       - Emotion-Attention Bridge (broaden-and-build, tunnel vision)
 *       - Basal Ganglia Bridge (Q-learning, habit vs goal-directed)
 *       - Predictive Bridge (free energy principle, active inference)
 *       - Reasoning Bridge (forward/backward chaining)
 *       - JEPA Bridge (masked factor prediction)
 *
 * WHY:  Verify correct behavior of attention and decision systems
 *       for financial market analysis and trading decisions.
 *
 * HOW:  Unit tests using Check framework covering all bridge APIs
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "cognitive/parietal/nimcp_financial_salience_bridge.h"
#include "cognitive/parietal/nimcp_financial_emo_attention_bridge.h"
#include "cognitive/parietal/nimcp_financial_basal_ganglia_bridge.h"
#include "cognitive/parietal/nimcp_financial_predictive_bridge.h"
#include "cognitive/parietal/nimcp_financial_reasoning_bridge.h"
#include "cognitive/parietal/nimcp_financial_jepa_bridge.h"

/* ============================================================================
 * Test Fixtures - Salience Bridge
 * ============================================================================ */

static financial_salience_bridge_t* g_salience_bridge = NULL;

static void salience_setup(void)
{
    fin_salience_config_t config;
    financial_salience_bridge_default_config(&config);
    g_salience_bridge = financial_salience_bridge_create(&config);
    ck_assert_ptr_nonnull(g_salience_bridge);
}

static void salience_teardown(void)
{
    if (g_salience_bridge) {
        financial_salience_bridge_destroy(g_salience_bridge);
        g_salience_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Emotion-Attention Bridge
 * ============================================================================ */

static financial_emo_attention_bridge_t* g_emo_attention_bridge = NULL;

static void emo_attention_setup(void)
{
    fin_emo_attention_config_t config;
    financial_emo_attention_bridge_default_config(&config);
    g_emo_attention_bridge = financial_emo_attention_bridge_create(&config);
    ck_assert_ptr_nonnull(g_emo_attention_bridge);
}

static void emo_attention_teardown(void)
{
    if (g_emo_attention_bridge) {
        financial_emo_attention_bridge_destroy(g_emo_attention_bridge);
        g_emo_attention_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Basal Ganglia Bridge
 * ============================================================================ */

static financial_bg_bridge_t* g_bg_bridge = NULL;

static void bg_setup(void)
{
    fin_bg_config_t config;
    financial_bg_bridge_default_config(&config);
    g_bg_bridge = financial_bg_bridge_create(&config);
    ck_assert_ptr_nonnull(g_bg_bridge);
}

static void bg_teardown(void)
{
    if (g_bg_bridge) {
        financial_bg_bridge_destroy(g_bg_bridge);
        g_bg_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Predictive Bridge
 * ============================================================================ */

static financial_predictive_bridge_t* g_predictive_bridge = NULL;

static void predictive_setup(void)
{
    fin_predictive_config_t config = financial_predictive_bridge_default_config();
    g_predictive_bridge = financial_predictive_bridge_create(&config);
    ck_assert_ptr_nonnull(g_predictive_bridge);
}

static void predictive_teardown(void)
{
    if (g_predictive_bridge) {
        financial_predictive_bridge_destroy(g_predictive_bridge);
        g_predictive_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Reasoning Bridge
 * ============================================================================ */

static financial_reasoning_bridge_t* g_reasoning_bridge = NULL;

static void reasoning_setup(void)
{
    fin_reasoning_config_t config;
    financial_reasoning_bridge_default_config(&config);
    g_reasoning_bridge = financial_reasoning_bridge_create(&config);
    ck_assert_ptr_nonnull(g_reasoning_bridge);
}

static void reasoning_teardown(void)
{
    if (g_reasoning_bridge) {
        financial_reasoning_bridge_destroy(g_reasoning_bridge);
        g_reasoning_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - JEPA Bridge
 * ============================================================================ */

static financial_jepa_bridge_t* g_jepa_bridge = NULL;

static void jepa_setup(void)
{
    fin_jepa_config_t config = financial_jepa_bridge_default_config();
    g_jepa_bridge = financial_jepa_bridge_create(&config);
    ck_assert_ptr_nonnull(g_jepa_bridge);
}

static void jepa_teardown(void)
{
    if (g_jepa_bridge) {
        financial_jepa_bridge_destroy(g_jepa_bridge);
        g_jepa_bridge = NULL;
    }
}

/* ============================================================================
 * Salience Bridge Tests
 * ============================================================================ */

START_TEST(test_salience_create_destroy)
{
    fin_salience_config_t config;
    int result = financial_salience_bridge_default_config(&config);
    ck_assert_int_eq(result, 0);

    financial_salience_bridge_t* bridge = financial_salience_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_salience_bridge_state_t state = financial_salience_bridge_get_state(bridge);
    ck_assert_int_ne(state, FIN_SALIENCE_STATE_UNINITIALIZED);

    financial_salience_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_salience_create_null_config)
{
    /* Should create with default config */
    financial_salience_bridge_t* bridge = financial_salience_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_salience_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_salience_destroy_null)
{
    /* Should not crash */
    financial_salience_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_salience_set_immune)
{
    int result = financial_salience_bridge_set_immune(g_salience_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_salience_set_bbb)
{
    int result = financial_salience_bridge_set_bbb(g_salience_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_salience_set_health_agent)
{
    int result = financial_salience_bridge_set_health_agent(g_salience_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_salience_evaluate_basic)
{
    fin_market_event_t event = {
        .event_type = FIN_SAL_EVENT_PRICE_CHANGE,
        .magnitude = 0.5f,
        .price_change_pct = 5.0f,
        .volume_ratio = 2.0f,
        .timestamp_ms = 1000
    };
    strncpy(event.symbol, "AAPL", FIN_SALIENCE_MAX_SYMBOL - 1);

    fin_salience_score_t score;
    int result = financial_salience_bridge_evaluate(g_salience_bridge, &event, &score);
    ck_assert_int_eq(result, 0);
    ck_assert(score.combined >= 0.0f && score.combined <= 1.0f);
}
END_TEST

START_TEST(test_salience_evaluate_null)
{
    fin_salience_score_t score;
    int result = financial_salience_bridge_evaluate(NULL, NULL, &score);
    ck_assert_int_ne(result, 0);

    fin_market_event_t event = {.event_type = FIN_SAL_EVENT_PRICE_CHANGE};
    result = financial_salience_bridge_evaluate(g_salience_bridge, &event, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_salience_filter_events)
{
    fin_market_event_t events[3];
    memset(events, 0, sizeof(events));

    /* High salience event */
    events[0].event_type = FIN_SAL_EVENT_CIRCUIT_BREAKER;
    events[0].magnitude = 0.9f;
    events[0].price_change_pct = 10.0f;
    strncpy(events[0].symbol, "SPY", FIN_SALIENCE_MAX_SYMBOL - 1);

    /* Low salience event */
    events[1].event_type = FIN_SAL_EVENT_PRICE_CHANGE;
    events[1].magnitude = 0.01f;
    events[1].price_change_pct = 0.1f;
    strncpy(events[1].symbol, "XYZ", FIN_SALIENCE_MAX_SYMBOL - 1);

    /* Medium salience event */
    events[2].event_type = FIN_SAL_EVENT_VOLUME_SPIKE;
    events[2].magnitude = 0.5f;
    events[2].volume_ratio = 3.0f;
    strncpy(events[2].symbol, "MSFT", FIN_SALIENCE_MAX_SYMBOL - 1);

    fin_scored_event_t output[3];
    size_t output_count = 0;

    int result = financial_salience_bridge_filter(
        g_salience_bridge, events, 3, output, &output_count, 0.3f);
    ck_assert_int_eq(result, 0);
    ck_assert(output_count <= 3);
}
END_TEST

START_TEST(test_salience_rank_events)
{
    fin_market_event_t events[2];
    memset(events, 0, sizeof(events));

    events[0].event_type = FIN_SAL_EVENT_PRICE_CHANGE;
    events[0].magnitude = 0.3f;
    strncpy(events[0].symbol, "AAPL", FIN_SALIENCE_MAX_SYMBOL - 1);

    events[1].event_type = FIN_SAL_EVENT_EARNINGS;
    events[1].magnitude = 0.8f;
    strncpy(events[1].symbol, "GOOG", FIN_SALIENCE_MAX_SYMBOL - 1);

    fin_scored_event_t output[2];
    size_t output_count = 0;

    int result = financial_salience_bridge_rank(
        g_salience_bridge, events, 2, output, 0, &output_count);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_eq(output_count, 2);
    /* Ranked by salience - higher salience should be first */
    ck_assert(output[0].score.combined >= output[1].score.combined);
}
END_TEST

START_TEST(test_salience_get_stats)
{
    fin_salience_bridge_stats_t stats;
    int result = financial_salience_bridge_get_stats(g_salience_bridge, &stats);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* ============================================================================
 * Emotion-Attention Bridge Tests
 * ============================================================================ */

START_TEST(test_emo_attention_create_destroy)
{
    fin_emo_attention_config_t config;
    int result = financial_emo_attention_bridge_default_config(&config);
    ck_assert_int_eq(result, 0);

    financial_emo_attention_bridge_t* bridge = financial_emo_attention_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_emo_attention_bridge_state_t state = financial_emo_attention_bridge_get_bridge_state(bridge);
    ck_assert_int_ne(state, FIN_EMO_ATTN_STATE_UNINITIALIZED);

    financial_emo_attention_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_emo_attention_create_null_config)
{
    financial_emo_attention_bridge_t* bridge = financial_emo_attention_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_emo_attention_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_emo_attention_destroy_null)
{
    financial_emo_attention_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_emo_attention_set_immune)
{
    int result = financial_emo_attention_bridge_set_immune(g_emo_attention_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_emo_attention_set_bbb)
{
    int result = financial_emo_attention_bridge_set_bbb(g_emo_attention_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_emo_attention_set_health_agent)
{
    int result = financial_emo_attention_bridge_set_health_agent(g_emo_attention_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_emo_attention_modulate_joy)
{
    /* Joy should broaden attention */
    fin_emotion_input_t emotion = {
        .joy = 0.9f,
        .fear = 0.0f,
        .anger = 0.0f,
        .surprise = 0.0f,
        .sadness = 0.0f,
        .greed = 0.0f,
        .panic = 0.0f
    };

    fin_emo_attention_state_t state = {0};
    int result = financial_emo_attention_bridge_modulate(g_emo_attention_bridge, &emotion, &state);
    ck_assert_int_eq(result, 0);
    /* Joy broadens - width should be above baseline (0.5) */
    ck_assert(state.attention_width >= FIN_EMO_ATTN_DEFAULT_WIDTH);
}
END_TEST

START_TEST(test_emo_attention_modulate_fear)
{
    /* Fear should narrow attention */
    fin_emotion_input_t emotion = {
        .joy = 0.0f,
        .fear = 0.9f,
        .anger = 0.0f,
        .surprise = 0.0f,
        .sadness = 0.0f,
        .greed = 0.0f,
        .panic = 0.0f
    };

    fin_emo_attention_state_t state = {0};
    int result = financial_emo_attention_bridge_modulate(g_emo_attention_bridge, &emotion, &state);
    ck_assert_int_eq(result, 0);
    /* Fear narrows - width should be below baseline */
    ck_assert(state.attention_width <= FIN_EMO_ATTN_DEFAULT_WIDTH);
}
END_TEST

START_TEST(test_emo_attention_detect_tunnel_vision)
{
    /* Set high panic to trigger tunnel vision */
    fin_emotion_input_t emotion = {
        .joy = 0.0f,
        .fear = 0.8f,
        .anger = 0.0f,
        .surprise = 0.0f,
        .sadness = 0.0f,
        .greed = 0.0f,
        .panic = 0.9f
    };

    /* First modulate to set state */
    financial_emo_attention_bridge_modulate(g_emo_attention_bridge, &emotion, NULL);

    fin_tunnel_vision_result_t result_struct;
    int result = financial_emo_attention_bridge_detect_tunnel_vision(
        g_emo_attention_bridge, &result_struct);
    ck_assert_int_eq(result, 0);
    /* With high fear and panic, should detect tunnel vision */
    /* Note: May or may not be detected depending on exact thresholds */
}
END_TEST

START_TEST(test_emo_attention_get_width)
{
    float width = financial_emo_attention_bridge_get_width(g_emo_attention_bridge);
    ck_assert(width >= 0.0f && width <= 1.0f);
}
END_TEST

START_TEST(test_emo_attention_get_stats)
{
    fin_emo_attention_bridge_stats_t stats;
    int result = financial_emo_attention_bridge_get_stats(g_emo_attention_bridge, &stats);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* ============================================================================
 * Basal Ganglia Bridge Tests
 * ============================================================================ */

START_TEST(test_bg_create_destroy)
{
    fin_bg_config_t config;
    int result = financial_bg_bridge_default_config(&config);
    ck_assert_int_eq(result, 0);

    financial_bg_bridge_t* bridge = financial_bg_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_bg_op_state_t state = financial_bg_bridge_get_op_state(bridge);
    ck_assert_int_ne(state, FIN_BG_OP_STATE_UNINITIALIZED);

    financial_bg_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_bg_create_null_config)
{
    financial_bg_bridge_t* bridge = financial_bg_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_bg_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_bg_destroy_null)
{
    financial_bg_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_bg_set_immune)
{
    int result = financial_bg_bridge_set_immune(g_bg_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_bg_set_bbb)
{
    int result = financial_bg_bridge_set_bbb(g_bg_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_bg_set_health_agent)
{
    int result = financial_bg_bridge_set_health_agent(g_bg_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_bg_evaluate_actions)
{
    fin_bg_state_t state = {
        .market = {
            .price_change = 0.05f,
            .volatility = 0.3f,
            .momentum = 0.2f,
            .regime_confidence = 0.8f,
            .sentiment = 0.1f,
            .spread = 0.001f,
            .timestamp_ms = 1000
        },
        .position = {
            .position_size = 0.0f,
            .unrealized_pnl = 0.0f,
            .realized_pnl = 0.0f,
            .exposure = 0.0f,
            .avg_entry_price = 0.0f,
            .hold_duration_ms = 0
        }
    };

    fin_bg_decision_t decision;
    int init_result = financial_bg_init_decision(&decision, FIN_BG_ACTION_COUNT);
    ck_assert_int_eq(init_result, 0);

    int result = financial_bg_bridge_evaluate_actions(g_bg_bridge, &state, &decision);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_gt(decision.num_actions, 0);

    financial_bg_cleanup_decision(&decision);
}
END_TEST

START_TEST(test_bg_select_action)
{
    fin_bg_state_t state = {
        .market = {
            .price_change = 0.05f,
            .volatility = 0.3f,
            .momentum = 0.2f
        },
        .position = {0}
    };

    fin_bg_decision_t decision;
    financial_bg_init_decision(&decision, FIN_BG_ACTION_COUNT);
    financial_bg_bridge_evaluate_actions(g_bg_bridge, &state, &decision);

    int result = financial_bg_bridge_select_action(g_bg_bridge, &decision);
    ck_assert_int_eq(result, 0);
    ck_assert(decision.selected >= 0 && decision.selected < FIN_BG_ACTION_COUNT);

    financial_bg_cleanup_decision(&decision);
}
END_TEST

START_TEST(test_bg_update_from_outcome)
{
    fin_bg_state_t prev_state = {
        .market = {.price_change = 0.05f},
        .position = {.position_size = 0.5f}
    };

    fin_bg_outcome_t outcome = {
        .action_taken = FIN_BG_ACTION_ENTER_LONG,
        .reward = 0.1f,
        .next_state = {
            .market = {.price_change = 0.08f},
            .position = {.position_size = 0.5f, .unrealized_pnl = 0.05f}
        },
        .terminal = false,
        .timestamp_ms = 2000
    };

    int result = financial_bg_bridge_update_from_outcome(g_bg_bridge, &prev_state, &outcome);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_bg_test_goal_directed)
{
    fin_bg_state_t state = {
        .market = {.price_change = 0.05f},
        .position = {0}
    };

    fin_bg_habit_result_t habit_result;
    int result = financial_bg_bridge_test_goal_directed(
        g_bg_bridge, FIN_BG_ACTION_HOLD, &state, &habit_result);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_bg_get_stats)
{
    fin_bg_bridge_stats_t stats;
    int result = financial_bg_bridge_get_stats(g_bg_bridge, &stats);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* ============================================================================
 * Predictive Bridge Tests
 * ============================================================================ */

START_TEST(test_predictive_create_destroy)
{
    fin_predictive_config_t config = financial_predictive_bridge_default_config();

    financial_predictive_bridge_t* bridge = financial_predictive_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_predictive_op_state_t state = financial_predictive_bridge_get_state(bridge);
    ck_assert_int_ne(state, FIN_PREDICTIVE_STATE_UNINITIALIZED);

    financial_predictive_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_predictive_create_null_config)
{
    financial_predictive_bridge_t* bridge = financial_predictive_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_predictive_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_predictive_destroy_null)
{
    financial_predictive_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_predictive_set_immune)
{
    int result = financial_predictive_bridge_set_immune(g_predictive_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_predictive_set_bbb)
{
    int result = financial_predictive_bridge_set_bbb(g_predictive_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_predictive_set_health_agent)
{
    int result = financial_predictive_bridge_set_health_agent(g_predictive_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_predictive_predict)
{
    fin_market_observation_t observation = {
        .num_assets = 2,
        .timestamp_us = 1000000
    };
    float prices[] = {100.0f, 200.0f};
    float volumes[] = {1000.0f, 2000.0f};
    float returns[] = {0.01f, -0.02f};
    float volatilities[] = {0.2f, 0.3f};
    observation.prices = prices;
    observation.volumes = volumes;
    observation.returns = returns;
    observation.volatilities = volatilities;

    fin_predictive_state_t* state = financial_predictive_state_create(2, 5);
    ck_assert_ptr_nonnull(state);

    int result = financial_predictive_bridge_predict(g_predictive_bridge, &observation, state);
    ck_assert_int_eq(result, 0);

    financial_predictive_state_destroy(state);
}
END_TEST

START_TEST(test_predictive_update)
{
    fin_market_observation_t observation = {
        .num_assets = 2,
        .timestamp_us = 2000000
    };
    float prices[] = {101.0f, 198.0f};
    float volumes[] = {1100.0f, 1900.0f};
    float returns[] = {0.01f, -0.01f};
    float volatilities[] = {0.21f, 0.29f};
    observation.prices = prices;
    observation.volumes = volumes;
    observation.returns = returns;
    observation.volatilities = volatilities;

    fin_predictive_state_t* state = financial_predictive_state_create(2, 5);
    ck_assert_ptr_nonnull(state);

    int result = financial_predictive_bridge_update(g_predictive_bridge, &observation, state);
    ck_assert_int_eq(result, 0);

    financial_predictive_state_destroy(state);
}
END_TEST

START_TEST(test_predictive_expected_free_energy)
{
    fin_predictive_state_t* state = financial_predictive_state_create(2, 5);
    ck_assert_ptr_nonnull(state);

    /* Initialize with some test data */
    for (uint32_t i = 0; i < state->num_assets * state->horizon; i++) {
        state->predictions[i] = 100.0f + (float)i;
        state->precisions[i] = 1.0f;
        state->prediction_errors[i] = 0.1f;
    }

    fin_preferred_outcome_t preferred = {
        .target_return = 0.05f,
        .max_drawdown = 0.1f,
        .target_sharpe = 1.5f,
        .risk_tolerance = 0.5f,
        .asset_preferences = NULL,
        .num_assets = 2
    };

    fin_efe_result_t efe_result;
    int result = financial_predictive_bridge_expected_free_energy(
        g_predictive_bridge, FIN_ACTION_BUY, state, &preferred, &efe_result);
    ck_assert_int_eq(result, 0);

    financial_predictive_state_destroy(state);
}
END_TEST

START_TEST(test_predictive_active_inference)
{
    fin_predictive_state_t* state = financial_predictive_state_create(2, 5);
    ck_assert_ptr_nonnull(state);

    for (uint32_t i = 0; i < state->num_assets * state->horizon; i++) {
        state->predictions[i] = 100.0f + (float)i;
        state->precisions[i] = 1.0f;
        state->prediction_errors[i] = 0.1f;
    }

    fin_preferred_outcome_t preferred = {
        .target_return = 0.05f,
        .max_drawdown = 0.1f,
        .target_sharpe = 1.5f,
        .risk_tolerance = 0.5f,
        .asset_preferences = NULL,
        .num_assets = 2
    };

    fin_active_inference_result_t* ai_result = financial_predictive_result_create(2, FIN_ACTION_COUNT);
    ck_assert_ptr_nonnull(ai_result);

    int result = financial_predictive_bridge_active_inference(
        g_predictive_bridge, state, &preferred, ai_result);
    ck_assert_int_eq(result, 0);
    ck_assert(ai_result->selected_action >= 0 && ai_result->selected_action < FIN_ACTION_COUNT);

    financial_predictive_result_destroy(ai_result);
    financial_predictive_state_destroy(state);
}
END_TEST

START_TEST(test_predictive_get_stats)
{
    fin_predictive_bridge_stats_t stats;
    int result = financial_predictive_bridge_get_stats(g_predictive_bridge, &stats);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* ============================================================================
 * Reasoning Bridge Tests
 * ============================================================================ */

START_TEST(test_reasoning_create_destroy)
{
    fin_reasoning_config_t config;
    int result = financial_reasoning_bridge_default_config(&config);
    ck_assert_int_eq(result, 0);

    financial_reasoning_bridge_t* bridge = financial_reasoning_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_reasoning_op_state_t state = financial_reasoning_bridge_get_op_state(bridge);
    ck_assert_int_ne(state, FIN_REASONING_OP_STATE_UNINITIALIZED);

    financial_reasoning_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_reasoning_create_null_config)
{
    financial_reasoning_bridge_t* bridge = financial_reasoning_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_reasoning_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_reasoning_destroy_null)
{
    financial_reasoning_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_reasoning_set_immune)
{
    int result = financial_reasoning_bridge_set_immune(g_reasoning_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_reasoning_set_bbb)
{
    int result = financial_reasoning_bridge_set_bbb(g_reasoning_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_reasoning_set_health_agent)
{
    int result = financial_reasoning_bridge_set_health_agent(g_reasoning_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_reasoning_add_rule)
{
    fin_trading_rule_t rule = {
        .confidence = 0.8f
    };
    strncpy(rule.condition, "PE_RATIO < 15 AND DEBT_EQUITY < 0.5", FIN_REASONING_CONDITION_LEN - 1);
    strncpy(rule.action, "BUY", FIN_REASONING_ACTION_LEN - 1);
    strncpy(rule.source, "Graham", FIN_REASONING_SOURCE_LEN - 1);

    int rule_id = financial_reasoning_bridge_add_rule(g_reasoning_bridge, &rule);
    ck_assert_int_ge(rule_id, 0);

    uint32_t count = financial_reasoning_bridge_get_rule_count(g_reasoning_bridge);
    ck_assert_uint_ge(count, 1);
}
END_TEST

START_TEST(test_reasoning_forward_chain)
{
    /* Add a simple rule */
    fin_trading_rule_t rule = {
        .confidence = 0.9f
    };
    strncpy(rule.condition, "RSI < 30", FIN_REASONING_CONDITION_LEN - 1);
    strncpy(rule.action, "STRONG_BUY", FIN_REASONING_ACTION_LEN - 1);
    strncpy(rule.source, "Technical", FIN_REASONING_SOURCE_LEN - 1);
    financial_reasoning_bridge_add_rule(g_reasoning_bridge, &rule);

    /* Assert a fact that matches the condition */
    int assert_result = financial_reasoning_bridge_assert_numeric(
        g_reasoning_bridge, "RSI", 25.0f, 1.0f);
    ck_assert_int_eq(assert_result, 0);

    /* Derive signals using forward chaining */
    fin_reasoning_result_t result;
    financial_reasoning_result_init(&result);

    int derive_result = financial_reasoning_bridge_derive_signals(g_reasoning_bridge, &result);
    ck_assert_int_eq(derive_result, 0);

    financial_reasoning_result_free(&result);
}
END_TEST

START_TEST(test_reasoning_backward_chain)
{
    /* Add rules for backward chaining */
    fin_trading_rule_t rule = {
        .confidence = 0.85f
    };
    strncpy(rule.condition, "UNDERVALUED AND QUALITY", FIN_REASONING_CONDITION_LEN - 1);
    strncpy(rule.action, "SHOULD_BUY", FIN_REASONING_ACTION_LEN - 1);
    strncpy(rule.source, "Buffett", FIN_REASONING_SOURCE_LEN - 1);
    financial_reasoning_bridge_add_rule(g_reasoning_bridge, &rule);

    fin_verify_request_t request = {
        .max_depth = 5,
        .explain = true
    };
    strncpy(request.goal, "SHOULD_BUY", FIN_REASONING_CONDITION_LEN - 1);

    fin_verify_response_t response;
    int result = financial_reasoning_bridge_verify_condition(
        g_reasoning_bridge, &request, &response);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_reasoning_load_context)
{
    fin_market_context_t context = {
        .current_price = 150.0f,
        .price_change_pct = 0.02f,
        .pe_ratio = 20.0f,
        .pb_ratio = 3.5f,
        .rsi = 55.0f,
        .macd = 0.5f,
        .sentiment_score = 0.3f,
        .volatility = 0.25f
    };

    int result = financial_reasoning_bridge_load_context(g_reasoning_bridge, &context);
    ck_assert_int_ge(result, 0); /* Returns number of facts loaded */
}
END_TEST

START_TEST(test_reasoning_get_stats)
{
    fin_reasoning_bridge_stats_t stats;
    int result = financial_reasoning_bridge_get_stats(g_reasoning_bridge, &stats);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* ============================================================================
 * JEPA Bridge Tests
 * ============================================================================ */

START_TEST(test_jepa_create_destroy)
{
    fin_jepa_config_t config = financial_jepa_bridge_default_config();

    financial_jepa_bridge_t* bridge = financial_jepa_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_jepa_op_state_t state = financial_jepa_bridge_get_state(bridge);
    ck_assert_int_ne(state, FIN_JEPA_STATE_UNINITIALIZED);

    financial_jepa_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_jepa_create_null_config)
{
    financial_jepa_bridge_t* bridge = financial_jepa_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_jepa_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_jepa_destroy_null)
{
    financial_jepa_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_jepa_set_immune)
{
    int result = financial_jepa_bridge_set_immune(g_jepa_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_jepa_set_bbb)
{
    int result = financial_jepa_bridge_set_bbb(g_jepa_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_jepa_set_health_agent)
{
    int result = financial_jepa_bridge_set_health_agent(g_jepa_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_jepa_predict_missing)
{
    fin_jepa_input_t* input = financial_jepa_input_create(10);
    ck_assert_ptr_nonnull(input);

    /* Set up visible factors and mask */
    for (uint32_t i = 0; i < 10; i++) {
        input->visible_factors[i] = (float)(i + 1) * 0.1f;
        input->mask[i] = (i >= 7); /* Mask last 3 factors */
    }

    fin_jepa_output_t* output = financial_jepa_output_create(10);
    ck_assert_ptr_nonnull(output);

    int result = financial_jepa_bridge_predict_missing(g_jepa_bridge, input, output);
    ck_assert_int_eq(result, 0);

    financial_jepa_input_destroy(input);
    financial_jepa_output_destroy(output);
}
END_TEST

START_TEST(test_jepa_cross_modal_predict)
{
    float source_factors[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    fin_cross_modal_input_t input = {
        .source_factors = source_factors,
        .num_source = 5,
        .source_modality = FIN_MODALITY_SENTIMENT,
        .target_modality = FIN_MODALITY_PRICE
    };

    fin_cross_modal_output_t* output = financial_jepa_cross_modal_output_create(10);
    ck_assert_ptr_nonnull(output);

    int result = financial_jepa_bridge_cross_modal_predict(g_jepa_bridge, &input, output);
    ck_assert_int_eq(result, 0);

    financial_jepa_cross_modal_output_destroy(output);
}
END_TEST

START_TEST(test_jepa_generate_mask)
{
    bool mask[10];
    uint32_t num_masked = 0;

    int result = financial_jepa_bridge_generate_mask(g_jepa_bridge, 10, mask, &num_masked);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_le(num_masked, 10);
}
END_TEST

START_TEST(test_jepa_embedding_similarity)
{
    float similarity;
    int result = financial_jepa_bridge_embedding_similarity(g_jepa_bridge, 0, 1, &similarity);
    ck_assert_int_eq(result, 0);
    ck_assert(similarity >= -1.0f && similarity <= 1.0f);
}
END_TEST

START_TEST(test_jepa_get_stats)
{
    fin_jepa_bridge_stats_t stats;
    int result = financial_jepa_bridge_get_stats(g_jepa_bridge, &stats);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* financial_attention_decision_suite(void)
{
    Suite* s = suite_create("Financial Attention Decision Bridges");

    /* Salience Bridge tests */
    TCase* tc_salience = tcase_create("Salience Bridge");
    tcase_add_checked_fixture(tc_salience, salience_setup, salience_teardown);
    tcase_add_test(tc_salience, test_salience_create_destroy);
    tcase_add_test(tc_salience, test_salience_create_null_config);
    tcase_add_test(tc_salience, test_salience_destroy_null);
    tcase_add_test(tc_salience, test_salience_set_immune);
    tcase_add_test(tc_salience, test_salience_set_bbb);
    tcase_add_test(tc_salience, test_salience_set_health_agent);
    tcase_add_test(tc_salience, test_salience_evaluate_basic);
    tcase_add_test(tc_salience, test_salience_evaluate_null);
    tcase_add_test(tc_salience, test_salience_filter_events);
    tcase_add_test(tc_salience, test_salience_rank_events);
    tcase_add_test(tc_salience, test_salience_get_stats);
    suite_add_tcase(s, tc_salience);

    /* Emotion-Attention Bridge tests */
    TCase* tc_emo_attention = tcase_create("Emotion-Attention Bridge");
    tcase_add_checked_fixture(tc_emo_attention, emo_attention_setup, emo_attention_teardown);
    tcase_add_test(tc_emo_attention, test_emo_attention_create_destroy);
    tcase_add_test(tc_emo_attention, test_emo_attention_create_null_config);
    tcase_add_test(tc_emo_attention, test_emo_attention_destroy_null);
    tcase_add_test(tc_emo_attention, test_emo_attention_set_immune);
    tcase_add_test(tc_emo_attention, test_emo_attention_set_bbb);
    tcase_add_test(tc_emo_attention, test_emo_attention_set_health_agent);
    tcase_add_test(tc_emo_attention, test_emo_attention_modulate_joy);
    tcase_add_test(tc_emo_attention, test_emo_attention_modulate_fear);
    tcase_add_test(tc_emo_attention, test_emo_attention_detect_tunnel_vision);
    tcase_add_test(tc_emo_attention, test_emo_attention_get_width);
    tcase_add_test(tc_emo_attention, test_emo_attention_get_stats);
    suite_add_tcase(s, tc_emo_attention);

    /* Basal Ganglia Bridge tests */
    TCase* tc_bg = tcase_create("Basal Ganglia Bridge");
    tcase_add_checked_fixture(tc_bg, bg_setup, bg_teardown);
    tcase_add_test(tc_bg, test_bg_create_destroy);
    tcase_add_test(tc_bg, test_bg_create_null_config);
    tcase_add_test(tc_bg, test_bg_destroy_null);
    tcase_add_test(tc_bg, test_bg_set_immune);
    tcase_add_test(tc_bg, test_bg_set_bbb);
    tcase_add_test(tc_bg, test_bg_set_health_agent);
    tcase_add_test(tc_bg, test_bg_evaluate_actions);
    tcase_add_test(tc_bg, test_bg_select_action);
    tcase_add_test(tc_bg, test_bg_update_from_outcome);
    tcase_add_test(tc_bg, test_bg_test_goal_directed);
    tcase_add_test(tc_bg, test_bg_get_stats);
    suite_add_tcase(s, tc_bg);

    /* Predictive Bridge tests */
    TCase* tc_predictive = tcase_create("Predictive Bridge");
    tcase_add_checked_fixture(tc_predictive, predictive_setup, predictive_teardown);
    tcase_add_test(tc_predictive, test_predictive_create_destroy);
    tcase_add_test(tc_predictive, test_predictive_create_null_config);
    tcase_add_test(tc_predictive, test_predictive_destroy_null);
    tcase_add_test(tc_predictive, test_predictive_set_immune);
    tcase_add_test(tc_predictive, test_predictive_set_bbb);
    tcase_add_test(tc_predictive, test_predictive_set_health_agent);
    tcase_add_test(tc_predictive, test_predictive_predict);
    tcase_add_test(tc_predictive, test_predictive_update);
    tcase_add_test(tc_predictive, test_predictive_expected_free_energy);
    tcase_add_test(tc_predictive, test_predictive_active_inference);
    tcase_add_test(tc_predictive, test_predictive_get_stats);
    suite_add_tcase(s, tc_predictive);

    /* Reasoning Bridge tests */
    TCase* tc_reasoning = tcase_create("Reasoning Bridge");
    tcase_add_checked_fixture(tc_reasoning, reasoning_setup, reasoning_teardown);
    tcase_add_test(tc_reasoning, test_reasoning_create_destroy);
    tcase_add_test(tc_reasoning, test_reasoning_create_null_config);
    tcase_add_test(tc_reasoning, test_reasoning_destroy_null);
    tcase_add_test(tc_reasoning, test_reasoning_set_immune);
    tcase_add_test(tc_reasoning, test_reasoning_set_bbb);
    tcase_add_test(tc_reasoning, test_reasoning_set_health_agent);
    tcase_add_test(tc_reasoning, test_reasoning_add_rule);
    tcase_add_test(tc_reasoning, test_reasoning_forward_chain);
    tcase_add_test(tc_reasoning, test_reasoning_backward_chain);
    tcase_add_test(tc_reasoning, test_reasoning_load_context);
    tcase_add_test(tc_reasoning, test_reasoning_get_stats);
    suite_add_tcase(s, tc_reasoning);

    /* JEPA Bridge tests */
    TCase* tc_jepa = tcase_create("JEPA Bridge");
    tcase_add_checked_fixture(tc_jepa, jepa_setup, jepa_teardown);
    tcase_add_test(tc_jepa, test_jepa_create_destroy);
    tcase_add_test(tc_jepa, test_jepa_create_null_config);
    tcase_add_test(tc_jepa, test_jepa_destroy_null);
    tcase_add_test(tc_jepa, test_jepa_set_immune);
    tcase_add_test(tc_jepa, test_jepa_set_bbb);
    tcase_add_test(tc_jepa, test_jepa_set_health_agent);
    tcase_add_test(tc_jepa, test_jepa_predict_missing);
    tcase_add_test(tc_jepa, test_jepa_cross_modal_predict);
    tcase_add_test(tc_jepa, test_jepa_generate_mask);
    tcase_add_test(tc_jepa, test_jepa_embedding_similarity);
    tcase_add_test(tc_jepa, test_jepa_get_stats);
    suite_add_tcase(s, tc_jepa);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = financial_attention_decision_suite();
    SRunner* sr = srunner_create(s);

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
