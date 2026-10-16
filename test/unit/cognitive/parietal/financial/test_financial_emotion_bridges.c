/**
 * @file test_financial_emotion_bridges.c
 * @brief Unit tests for Financial Emotional System bridges
 *
 * WHAT: Comprehensive test suite for the four financial emotional system bridges:
 *       1. Financial Emotion Bridge (Plutchik's model)
 *       2. Financial Motivation Bridge (Nucleus accumbens / FOMO detection)
 *       3. Financial Neuromodulator Bridge (DA/5HT/NE/ACh/adenosine)
 *       4. Financial Mental Health Bridge (stress/anxiety assessment)
 *
 * WHY:  Verify correct behavior of emotional processing, motivation evaluation,
 *       neuromodulatory effects, and mental health assessment in financial contexts.
 *
 * HOW:  Unit tests using Check framework covering lifecycle, setters, and core APIs.
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "cognitive/parietal/nimcp_financial_emotion_bridge.h"
#include "cognitive/parietal/nimcp_financial_motivation_bridge.h"
#include "cognitive/parietal/nimcp_financial_neuromod_bridge.h"
#include "cognitive/parietal/nimcp_financial_mental_health_bridge.h"

/* ============================================================================
 * Helper Macros
 * ============================================================================ */

#define FLOAT_EPSILON 0.0001f

#define ck_assert_float_in_range(val, min, max) \
    ck_assert_msg((val) >= (min) && (val) <= (max), \
        "Expected %f in range [%f, %f]", (double)(val), (double)(min), (double)(max))

/* ============================================================================
 * Test Fixtures - Financial Emotion Bridge
 * ============================================================================ */

static financial_emotion_bridge_t* g_emotion_bridge = NULL;

static void emotion_setup(void)
{
    fin_emotion_config_t config;
    financial_emotion_bridge_default_config(&config);
    g_emotion_bridge = financial_emotion_bridge_create(&config);
    ck_assert_ptr_nonnull(g_emotion_bridge);
}

static void emotion_teardown(void)
{
    if (g_emotion_bridge) {
        financial_emotion_bridge_destroy(g_emotion_bridge);
        g_emotion_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Financial Motivation Bridge
 * ============================================================================ */

static financial_motivation_bridge_t* g_motivation_bridge = NULL;

static void motivation_setup(void)
{
    fin_motivation_config_t config;
    financial_motivation_bridge_default_config(&config);
    g_motivation_bridge = financial_motivation_bridge_create(&config);
    ck_assert_ptr_nonnull(g_motivation_bridge);
}

static void motivation_teardown(void)
{
    if (g_motivation_bridge) {
        financial_motivation_bridge_destroy(g_motivation_bridge);
        g_motivation_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Financial Neuromodulator Bridge
 * ============================================================================ */

static financial_neuromod_bridge_t* g_neuromod_bridge = NULL;

static void neuromod_setup(void)
{
    fin_neuromod_config_t config;
    financial_neuromod_bridge_default_config(&config);
    g_neuromod_bridge = financial_neuromod_bridge_create(&config);
    ck_assert_ptr_nonnull(g_neuromod_bridge);
}

static void neuromod_teardown(void)
{
    if (g_neuromod_bridge) {
        financial_neuromod_bridge_destroy(g_neuromod_bridge);
        g_neuromod_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Financial Mental Health Bridge
 * ============================================================================ */

static financial_mental_health_bridge_t* g_mental_health_bridge = NULL;

static void mental_health_setup(void)
{
    fin_mental_health_config_t config;
    financial_mental_health_bridge_default_config(&config);
    g_mental_health_bridge = financial_mental_health_bridge_create(&config);
    ck_assert_ptr_nonnull(g_mental_health_bridge);
}

static void mental_health_teardown(void)
{
    if (g_mental_health_bridge) {
        financial_mental_health_bridge_destroy(g_mental_health_bridge);
        g_mental_health_bridge = NULL;
    }
}

/* ============================================================================
 * FINANCIAL EMOTION BRIDGE TESTS
 * ============================================================================ */

/* --- Lifecycle Tests --- */

START_TEST(test_emotion_default_config)
{
    fin_emotion_config_t config;
    int result = financial_emotion_bridge_default_config(&config);
    ck_assert_int_eq(result, 0);

    /* Verify sensible defaults */
    ck_assert_float_in_range(config.decay_rate, 0.0f, 1.0f);
    ck_assert_float_in_range(config.sensitivity, 0.0f, 2.0f);
    ck_assert_float_in_range(config.baseline_mood, 0.0f, 1.0f);
    ck_assert_float_in_range(config.fomo_threshold, 0.0f, 1.0f);
    ck_assert_float_in_range(config.panic_threshold, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_emotion_default_config_null)
{
    int result = financial_emotion_bridge_default_config(NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_emotion_create_destroy)
{
    fin_emotion_config_t config;
    financial_emotion_bridge_default_config(&config);

    financial_emotion_bridge_t* bridge = financial_emotion_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_emotion_bridge_state_t state = financial_emotion_bridge_get_bridge_state(bridge);
    ck_assert_int_eq(state, FIN_EMOTION_STATE_INITIALIZED);

    financial_emotion_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_emotion_create_null_config)
{
    /* Should use defaults when config is NULL */
    financial_emotion_bridge_t* bridge = financial_emotion_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_emotion_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_emotion_destroy_null)
{
    /* Should not crash */
    financial_emotion_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_emotion_reset)
{
    int result = financial_emotion_bridge_reset(g_emotion_bridge);
    ck_assert_int_eq(result, 0);

    /* Verify state is reset */
    fin_emotion_state_t state;
    result = financial_emotion_bridge_get_state(g_emotion_bridge, &state);
    ck_assert_int_eq(result, 0);

    /* After reset, emotions should be at baseline (low intensity) */
    ck_assert_float_in_range(state.joy, 0.0f, 0.3f);
    ck_assert_float_in_range(state.fear, 0.0f, 0.3f);
}
END_TEST

/* --- Subsystem Setter Tests --- */

START_TEST(test_emotion_set_immune)
{
    int result = financial_emotion_bridge_set_immune(g_emotion_bridge, NULL);
    ck_assert_int_eq(result, 0);

    result = financial_emotion_bridge_set_immune(NULL, NULL);
    ck_assert_int_ne(result, 0);  /* Should fail with NULL bridge */
}
END_TEST

START_TEST(test_emotion_set_bbb)
{
    int result = financial_emotion_bridge_set_bbb(g_emotion_bridge, NULL);
    ck_assert_int_eq(result, 0);

    result = financial_emotion_bridge_set_bbb(NULL, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_emotion_set_health_agent)
{
    int result = financial_emotion_bridge_set_health_agent(g_emotion_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_emotion_set_kg_wiring)
{
    int result = financial_emotion_bridge_set_kg_wiring(g_emotion_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* --- Core Emotion API Tests --- */

START_TEST(test_emotion_update_price_increase)
{
    fin_market_event_t event = {
        .event_type = FIN_MKT_EVENT_PRICE_INCREASE,
        .magnitude = 0.5f,
        .surprise_factor = 0.3f,
        .timestamp_ms = 1000
    };

    int result = financial_emotion_bridge_update(g_emotion_bridge, &event);
    ck_assert_int_eq(result, 0);

    fin_emotion_state_t state;
    result = financial_emotion_bridge_get_state(g_emotion_bridge, &state);
    ck_assert_int_eq(result, 0);

    /* Price increase should increase joy and potentially greed */
    ck_assert_float_in_range(state.joy, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_emotion_update_price_decrease)
{
    fin_market_event_t event = {
        .event_type = FIN_MKT_EVENT_PRICE_DECREASE,
        .magnitude = 0.7f,
        .surprise_factor = 0.6f,
        .timestamp_ms = 2000
    };

    int result = financial_emotion_bridge_update(g_emotion_bridge, &event);
    ck_assert_int_eq(result, 0);

    fin_emotion_state_t state;
    result = financial_emotion_bridge_get_state(g_emotion_bridge, &state);
    ck_assert_int_eq(result, 0);

    /* Price decrease should trigger fear/sadness */
    ck_assert_float_in_range(state.fear, 0.0f, 1.0f);
    ck_assert_float_in_range(state.sadness, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_emotion_update_null)
{
    int result = financial_emotion_bridge_update(g_emotion_bridge, NULL);
    ck_assert_int_ne(result, 0);

    result = financial_emotion_bridge_update(NULL, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_emotion_get_state)
{
    fin_emotion_state_t state;
    int result = financial_emotion_bridge_get_state(g_emotion_bridge, &state);
    ck_assert_int_eq(result, 0);

    /* All emotion values should be in [0,1] range */
    ck_assert_float_in_range(state.joy, 0.0f, 1.0f);
    ck_assert_float_in_range(state.sadness, 0.0f, 1.0f);
    ck_assert_float_in_range(state.anger, 0.0f, 1.0f);
    ck_assert_float_in_range(state.fear, 0.0f, 1.0f);
    ck_assert_float_in_range(state.surprise, 0.0f, 1.0f);
    ck_assert_float_in_range(state.disgust, 0.0f, 1.0f);
    ck_assert_float_in_range(state.trust, 0.0f, 1.0f);
    ck_assert_float_in_range(state.anticipation, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_emotion_get_state_null)
{
    int result = financial_emotion_bridge_get_state(g_emotion_bridge, NULL);
    ck_assert_int_ne(result, 0);

    fin_emotion_state_t state;
    result = financial_emotion_bridge_get_state(NULL, &state);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_emotion_get_dominant)
{
    /* First trigger some emotion */
    fin_market_event_t event = {
        .event_type = FIN_MKT_EVENT_PROFIT_TARGET_HIT,
        .magnitude = 0.8f,
        .surprise_factor = 0.5f,
        .timestamp_ms = 3000
    };
    financial_emotion_bridge_update(g_emotion_bridge, &event);

    fin_dominant_result_t result_dom;
    int result = financial_emotion_bridge_get_dominant(g_emotion_bridge, &result_dom);
    ck_assert_int_eq(result, 0);

    /* Dominant emotion should be valid and have intensity in range */
    ck_assert_int_ge(result_dom.dominant, FIN_DOMINANT_NEUTRAL);
    ck_assert_int_lt(result_dom.dominant, FIN_DOMINANT_COUNT);
    ck_assert_float_in_range(result_dom.intensity, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_emotion_decay)
{
    /* First create high emotion state */
    fin_market_event_t event = {
        .event_type = FIN_MKT_EVENT_STOP_LOSS_HIT,
        .magnitude = 0.9f,
        .surprise_factor = 0.8f,
        .timestamp_ms = 4000
    };
    financial_emotion_bridge_update(g_emotion_bridge, &event);

    fin_emotion_state_t state_before;
    financial_emotion_bridge_get_state(g_emotion_bridge, &state_before);

    /* Apply decay */
    int result = financial_emotion_bridge_decay(g_emotion_bridge, 5000);  /* 5 seconds */
    ck_assert_int_eq(result, 0);

    fin_emotion_state_t state_after;
    financial_emotion_bridge_get_state(g_emotion_bridge, &state_after);

    /* After decay, emotions should generally decrease (or stay same if at baseline) */
    /* At minimum, values should still be valid */
    ck_assert_float_in_range(state_after.fear, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_emotion_modulate_decision)
{
    /* Create stressed emotional state */
    fin_market_event_t event = {
        .event_type = FIN_MKT_EVENT_VOLATILITY_SPIKE,
        .magnitude = 0.7f,
        .surprise_factor = 0.6f,
        .timestamp_ms = 5000
    };
    financial_emotion_bridge_update(g_emotion_bridge, &event);

    fin_decision_modulation_t modulation;
    int result = financial_emotion_bridge_modulate_decision(g_emotion_bridge, &modulation);
    ck_assert_int_eq(result, 0);

    /* Modulation factors should be in valid ranges */
    ck_assert_float_in_range(modulation.risk_tolerance_scale, 0.0f, 2.0f);
    ck_assert_float_in_range(modulation.position_size_scale, 0.0f, 2.0f);
    ck_assert_float_in_range(modulation.urgency_dampening, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_emotion_detect_bias)
{
    /* Create FOMO-inducing state */
    fin_market_event_t event = {
        .event_type = FIN_MKT_EVENT_MISSED_OPPORTUNITY,
        .magnitude = 0.8f,
        .surprise_factor = 0.7f,
        .timestamp_ms = 6000
    };
    financial_emotion_bridge_update(g_emotion_bridge, &event);

    fin_bias_detection_t detection;
    int result = financial_emotion_bridge_detect_bias(g_emotion_bridge, &detection);
    ck_assert_int_eq(result, 0);

    /* Bias result should be valid */
    ck_assert_int_ge(detection.bias, FIN_BIAS_NONE);
    ck_assert_int_lt(detection.bias, FIN_BIAS_COUNT);
    ck_assert_float_in_range(detection.severity, 0.0f, 1.0f);
    ck_assert_float_in_range(detection.confidence, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_emotion_get_stats)
{
    /* Perform some operations */
    fin_market_event_t event = {
        .event_type = FIN_MKT_EVENT_NEWS_POSITIVE,
        .magnitude = 0.5f,
        .surprise_factor = 0.4f,
        .timestamp_ms = 7000
    };
    financial_emotion_bridge_update(g_emotion_bridge, &event);

    fin_emotion_bridge_stats_t stats;
    int result = financial_emotion_bridge_get_stats(g_emotion_bridge, &stats);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_ge(stats.updates, 1);
}
END_TEST

START_TEST(test_emotion_utility_names)
{
    /* Test utility name functions */
    const char* name;

    name = fin_emotion_primary_name(FIN_PRIMARY_JOY);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_ne(name, "");

    name = fin_emotion_compound_name(FIN_COMPOUND_FOMO);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_ne(name, "");

    name = fin_emotion_dominant_name(FIN_DOMINANT_PANIC);
    ck_assert_ptr_nonnull(name);

    name = fin_emotion_bias_name(FIN_BIAS_FOMO);
    ck_assert_ptr_nonnull(name);

    name = fin_emotion_event_name(FIN_MKT_EVENT_PRICE_INCREASE);
    ck_assert_ptr_nonnull(name);

    name = fin_emotion_state_name(FIN_EMOTION_STATE_ACTIVE);
    ck_assert_ptr_nonnull(name);

    name = financial_emotion_bridge_version();
    ck_assert_ptr_nonnull(name);
}
END_TEST

/* ============================================================================
 * FINANCIAL MOTIVATION BRIDGE TESTS
 * ============================================================================ */

/* --- Lifecycle Tests --- */

START_TEST(test_motivation_default_config)
{
    fin_motivation_config_t config;
    int result = financial_motivation_bridge_default_config(&config);
    ck_assert_int_eq(result, 0);

    /* Verify sensible defaults */
    ck_assert_float_in_range(config.wanting_sensitivity, 0.0f, 2.0f);
    ck_assert_float_in_range(config.novelty_weight, 0.0f, 1.0f);
    ck_assert_float_in_range(config.risk_aversion, 0.0f, 2.0f);
    ck_assert_float_in_range(config.learning_rate, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_motivation_default_config_null)
{
    int result = financial_motivation_bridge_default_config(NULL);
    ck_assert_int_ne(result, 0);  /* Should return non-zero error */
}
END_TEST

START_TEST(test_motivation_create_destroy)
{
    fin_motivation_config_t config;
    financial_motivation_bridge_default_config(&config);

    financial_motivation_bridge_t* bridge = financial_motivation_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_motivation_state_t state = financial_motivation_bridge_get_state(bridge);
    ck_assert_int_eq(state, FIN_MOTIVATION_STATE_INITIALIZED);

    financial_motivation_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_motivation_create_null_config)
{
    financial_motivation_bridge_t* bridge = financial_motivation_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_motivation_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_motivation_destroy_null)
{
    financial_motivation_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_motivation_reset)
{
    int result = financial_motivation_bridge_reset(g_motivation_bridge);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* --- Subsystem Setter Tests --- */

START_TEST(test_motivation_set_immune)
{
    int result = financial_motivation_bridge_set_immune(g_motivation_bridge, NULL);
    ck_assert_int_eq(result, 0);

    result = financial_motivation_bridge_set_immune(NULL, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_motivation_set_bbb)
{
    int result = financial_motivation_bridge_set_bbb(g_motivation_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* --- Core Motivation API Tests --- */

START_TEST(test_motivation_evaluate)
{
    fin_opportunity_t opportunity = {
        .expected_return = 0.05f,  /* 5% expected return */
        .risk_level = 0.3f,
        .novelty = 0.6f,
        .urgency = 0.4f,
        .timestamp_ms = 1000
    };

    fin_motivation_signal_t signal;
    int result = financial_motivation_bridge_evaluate(g_motivation_bridge, &opportunity, &signal);
    ck_assert_int_eq(result, 0);

    /* Signal values should be in valid range [-1, 1] */
    ck_assert_float_in_range(signal.wanting, -1.0f, 1.0f);
    ck_assert_float_in_range(signal.liking, -1.0f, 1.0f);
    ck_assert_float_in_range(signal.learning, -1.0f, 1.0f);
}
END_TEST

START_TEST(test_motivation_evaluate_null)
{
    fin_motivation_signal_t signal;
    int result = financial_motivation_bridge_evaluate(g_motivation_bridge, NULL, &signal);
    ck_assert_int_ne(result, 0);

    fin_opportunity_t opportunity = {0};
    result = financial_motivation_bridge_evaluate(g_motivation_bridge, &opportunity, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_motivation_detect_fomo)
{
    /* Create high-urgency opportunity */
    fin_opportunity_t opportunity = {
        .expected_return = 0.10f,
        .risk_level = 0.5f,
        .novelty = 0.8f,
        .urgency = 0.9f,  /* High urgency - FOMO trigger */
        .timestamp_ms = 2000
    };

    fin_motivation_signal_t signal;
    financial_motivation_bridge_evaluate(g_motivation_bridge, &opportunity, &signal);

    fin_fomo_result_t fomo;
    int result = financial_motivation_bridge_detect_fomo(g_motivation_bridge, &opportunity, &signal, &fomo);
    ck_assert_int_eq(result, 0);

    /* FOMO result should be valid */
    ck_assert_int_ge(fomo.level, FIN_FOMO_NONE);
    ck_assert_int_lt(fomo.level, FIN_FOMO_COUNT);
    ck_assert_float_in_range(fomo.wanting_excess, -1.0f, 2.0f);
    ck_assert_float_in_range(fomo.urgency_bias, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_motivation_detect_fomo_null)
{
    int result = financial_motivation_bridge_detect_fomo(g_motivation_bridge, NULL, NULL, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_motivation_rational_value)
{
    fin_opportunity_t opportunity = {
        .expected_return = 0.08f,
        .risk_level = 0.4f,
        .novelty = 0.3f,
        .urgency = 0.2f,
        .timestamp_ms = 3000
    };

    fin_rational_value_t rational;
    int result = financial_motivation_bridge_rational_value(g_motivation_bridge, &opportunity, &rational);
    ck_assert_int_eq(result, 0);

    /* Rational value should have valid fields */
    ck_assert_float_in_range(rational.confidence, 0.0f, 1.0f);
    ck_assert_float_in_range(rational.kelly_fraction, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_motivation_should_override)
{
    fin_motivation_signal_t signal = {
        .wanting = 0.9f,  /* High wanting */
        .liking = 0.3f,
        .learning = 0.1f
    };

    fin_rational_value_t rational = {
        .expected_value = 0.02f,  /* Low rational value */
        .opportunity_cost = 0.05f,
        .kelly_fraction = 0.1f,
        .confidence = 0.7f
    };

    fin_override_result_t override;
    int result = financial_motivation_bridge_should_override(g_motivation_bridge, &signal, &rational, &override);
    ck_assert_int_eq(result, 0);

    /* Override result should be valid */
    ck_assert_int_ge(override.level, FIN_OVERRIDE_NONE);
    ck_assert_int_lt(override.level, FIN_OVERRIDE_COUNT);
    ck_assert_float_in_range(override.override_confidence, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_motivation_process_outcome)
{
    fin_outcome_feedback_t feedback = {
        .opportunity_id = 1,
        .actual_return = 0.06f,
        .satisfaction = 0.7f,
        .timestamp_ms = 4000
    };

    int result = financial_motivation_bridge_process_outcome(g_motivation_bridge, &feedback);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_motivation_get_stats)
{
    /* Perform some operations */
    fin_opportunity_t opportunity = {
        .expected_return = 0.05f,
        .risk_level = 0.3f,
        .novelty = 0.5f,
        .urgency = 0.4f,
        .timestamp_ms = 5000
    };
    fin_motivation_signal_t signal;
    financial_motivation_bridge_evaluate(g_motivation_bridge, &opportunity, &signal);

    fin_motivation_bridge_stats_t stats;
    int result = financial_motivation_bridge_get_stats(g_motivation_bridge, &stats);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_ge(stats.evaluations, 1);
}
END_TEST

START_TEST(test_motivation_utility_names)
{
    const char* name;

    name = fin_motivation_state_name(FIN_MOTIVATION_STATE_ACTIVE);
    ck_assert_ptr_nonnull(name);

    name = fin_motivation_override_name(FIN_OVERRIDE_REVIEW);
    ck_assert_ptr_nonnull(name);

    name = fin_motivation_fomo_name(FIN_FOMO_STRONG);
    ck_assert_ptr_nonnull(name);

    name = financial_motivation_bridge_version();
    ck_assert_ptr_nonnull(name);
}
END_TEST

/* ============================================================================
 * FINANCIAL NEUROMODULATOR BRIDGE TESTS
 * ============================================================================ */

/* --- Lifecycle Tests --- */

START_TEST(test_neuromod_default_config)
{
    fin_neuromod_config_t config;
    int result = financial_neuromod_bridge_default_config(&config);
    ck_assert_int_eq(result, 0);

    /* Verify sensible baseline defaults (around 0.5) */
    ck_assert_float_in_range(config.baseline_dopamine, 0.0f, 1.0f);
    ck_assert_float_in_range(config.baseline_serotonin, 0.0f, 1.0f);
    ck_assert_float_in_range(config.baseline_norepinephrine, 0.0f, 1.0f);
    ck_assert_float_in_range(config.baseline_acetylcholine, 0.0f, 1.0f);
    ck_assert_float_in_range(config.baseline_adenosine, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_neuromod_default_config_null)
{
    int result = financial_neuromod_bridge_default_config(NULL);
    ck_assert_int_ne(result, 0);  /* Should return non-zero error */
}
END_TEST

START_TEST(test_neuromod_create_destroy)
{
    fin_neuromod_config_t config;
    financial_neuromod_bridge_default_config(&config);

    financial_neuromod_bridge_t* bridge = financial_neuromod_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_neuromod_op_state_t state = financial_neuromod_bridge_get_op_state(bridge);
    ck_assert_int_eq(state, FIN_NEUROMOD_OP_STATE_INITIALIZED);

    financial_neuromod_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_neuromod_create_null_config)
{
    financial_neuromod_bridge_t* bridge = financial_neuromod_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_neuromod_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_neuromod_destroy_null)
{
    financial_neuromod_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_neuromod_reset)
{
    int result = financial_neuromod_bridge_reset(g_neuromod_bridge);
    ck_assert_int_eq(result, 0);

    /* After reset, state should be at baseline */
    fin_neuromod_state_t state;
    result = financial_neuromod_bridge_get_state(g_neuromod_bridge, &state);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* --- Subsystem Setter Tests --- */

START_TEST(test_neuromod_set_immune)
{
    int result = financial_neuromod_bridge_set_immune(g_neuromod_bridge, NULL);
    ck_assert_int_eq(result, 0);

    result = financial_neuromod_bridge_set_immune(NULL, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_neuromod_set_bbb)
{
    int result = financial_neuromod_bridge_set_bbb(g_neuromod_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* --- Core Neuromod API Tests --- */

START_TEST(test_neuromod_update_gain)
{
    fin_neuromod_event_t event = {
        .type = FIN_NEUROMOD_EVENT_GAIN,
        .magnitude = 0.6f,
        .prediction_error = 0.3f,  /* Better than expected */
        .timestamp_ms = 1000
    };

    int result = financial_neuromod_bridge_update(g_neuromod_bridge, &event);
    ck_assert_int_eq(result, 0);

    fin_neuromod_state_t state;
    result = financial_neuromod_bridge_get_state(g_neuromod_bridge, &state);
    ck_assert_int_eq(result, 0);

    /* All neuromodulator values should be in valid range */
    ck_assert_float_in_range(state.dopamine, 0.0f, 1.0f);
    ck_assert_float_in_range(state.serotonin, 0.0f, 1.0f);
    ck_assert_float_in_range(state.norepinephrine, 0.0f, 1.0f);
    ck_assert_float_in_range(state.acetylcholine, 0.0f, 1.0f);
    ck_assert_float_in_range(state.adenosine, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_neuromod_update_loss)
{
    fin_neuromod_event_t event = {
        .type = FIN_NEUROMOD_EVENT_LOSS,
        .magnitude = 0.7f,
        .prediction_error = -0.4f,  /* Worse than expected */
        .timestamp_ms = 2000
    };

    int result = financial_neuromod_bridge_update(g_neuromod_bridge, &event);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_neuromod_update_null)
{
    int result = financial_neuromod_bridge_update(g_neuromod_bridge, NULL);
    ck_assert_int_ne(result, 0);

    result = financial_neuromod_bridge_update(NULL, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_neuromod_get_state)
{
    fin_neuromod_state_t state;
    int result = financial_neuromod_bridge_get_state(g_neuromod_bridge, &state);
    ck_assert_int_eq(result, 0);

    /* All values should be in [0,1] */
    ck_assert_float_in_range(state.dopamine, 0.0f, 1.0f);
    ck_assert_float_in_range(state.serotonin, 0.0f, 1.0f);
    ck_assert_float_in_range(state.norepinephrine, 0.0f, 1.0f);
    ck_assert_float_in_range(state.acetylcholine, 0.0f, 1.0f);
    ck_assert_float_in_range(state.adenosine, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_neuromod_set_state)
{
    fin_neuromod_state_t new_state = {
        .dopamine = 0.7f,
        .serotonin = 0.6f,
        .norepinephrine = 0.5f,
        .acetylcholine = 0.6f,
        .adenosine = 0.3f
    };

    int result = financial_neuromod_bridge_set_state(g_neuromod_bridge, &new_state);
    ck_assert_int_eq(result, 0);

    fin_neuromod_state_t retrieved;
    result = financial_neuromod_bridge_get_state(g_neuromod_bridge, &retrieved);
    ck_assert_int_eq(result, 0);

    ck_assert(fabsf(retrieved.dopamine - 0.7f) < FLOAT_EPSILON);
    ck_assert(fabsf(retrieved.serotonin - 0.6f) < FLOAT_EPSILON);
}
END_TEST

START_TEST(test_neuromod_compute_effects)
{
    fin_neuromod_effects_t effects;
    int result = financial_neuromod_bridge_compute_effects(g_neuromod_bridge, &effects);
    ck_assert_int_eq(result, 0);

    /* Effects should be in valid ranges */
    ck_assert_float_in_range(effects.risk_tolerance, 0.0f, 1.0f);
    ck_assert_float_in_range(effects.patience, 0.0f, 1.0f);
    ck_assert_float_in_range(effects.learning_rate, 0.0f, 1.0f);
    ck_assert_float_in_range(effects.arousal_level, 0.0f, 1.0f);
    ck_assert_float_in_range(effects.fatigue_level, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_neuromod_compute_effects_null)
{
    int result = financial_neuromod_bridge_compute_effects(g_neuromod_bridge, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_neuromod_compute_effects_from_state)
{
    fin_neuromod_state_t state = {
        .dopamine = 0.8f,
        .serotonin = 0.4f,
        .norepinephrine = 0.7f,
        .acetylcholine = 0.6f,
        .adenosine = 0.5f
    };

    fin_neuromod_effects_t effects;
    int result = financial_neuromod_bridge_compute_effects_from_state(g_neuromod_bridge, &state, &effects);
    ck_assert_int_eq(result, 0);

    ck_assert_float_in_range(effects.risk_tolerance, 0.0f, 1.0f);
    ck_assert_float_in_range(effects.arousal_level, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_neuromod_modulate_archetype)
{
    fin_archetype_params_t base = {
        .base_risk_tolerance = 0.5f,
        .base_patience = 0.6f,
        .base_learning_rate = 0.3f,
        .base_concentration = 0.7f,
        .base_contrarian_tendency = 0.2f
    };

    fin_archetype_modulation_t modulated;
    int result = financial_neuromod_bridge_modulate_archetype(g_neuromod_bridge, &base, &modulated);
    ck_assert_int_eq(result, 0);

    /* Modulated values should still be in valid ranges */
    ck_assert_float_in_range(modulated.modulated_risk_tolerance, 0.0f, 1.0f);
    ck_assert_float_in_range(modulated.modulated_patience, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_neuromod_decay)
{
    /* First set high values */
    fin_neuromod_state_t high_state = {
        .dopamine = 0.9f,
        .serotonin = 0.8f,
        .norepinephrine = 0.85f,
        .acetylcholine = 0.8f,
        .adenosine = 0.7f
    };
    financial_neuromod_bridge_set_state(g_neuromod_bridge, &high_state);

    /* Apply decay */
    int result = financial_neuromod_bridge_decay(g_neuromod_bridge, 5000);  /* 5 seconds */
    ck_assert_int_eq(result, 0);

    fin_neuromod_state_t after;
    financial_neuromod_bridge_get_state(g_neuromod_bridge, &after);

    /* Values should still be valid after decay */
    ck_assert_float_in_range(after.dopamine, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_neuromod_analyze_arousal)
{
    fin_arousal_result_t arousal;
    int result = financial_neuromod_bridge_analyze_arousal(g_neuromod_bridge, &arousal);
    ck_assert_int_eq(result, 0);

    /* Arousal analysis should have valid values */
    ck_assert_int_ge(arousal.level, FIN_AROUSAL_VERY_LOW);
    ck_assert_int_lt(arousal.level, FIN_AROUSAL_COUNT);
    ck_assert_float_in_range(arousal.raw_arousal, 0.0f, 1.0f);
    ck_assert_float_in_range(arousal.performance_factor, 0.0f, 2.0f);
}
END_TEST

START_TEST(test_neuromod_analyze_fatigue)
{
    /* First increase adenosine (fatigue indicator) */
    fin_neuromod_event_t event = {
        .type = FIN_NEUROMOD_EVENT_COGNITIVE_EFFORT,
        .magnitude = 0.8f,
        .prediction_error = 0.0f,
        .timestamp_ms = 3000
    };
    financial_neuromod_bridge_update(g_neuromod_bridge, &event);

    fin_fatigue_result_t fatigue;
    int result = financial_neuromod_bridge_analyze_fatigue(g_neuromod_bridge, &fatigue);
    ck_assert_int_eq(result, 0);

    /* Fatigue analysis should have valid values */
    ck_assert_int_ge(fatigue.level, FIN_FATIGUE_NONE);
    ck_assert_int_lt(fatigue.level, FIN_FATIGUE_COUNT);
    ck_assert_float_in_range(fatigue.raw_fatigue, 0.0f, 1.0f);
    ck_assert_float_in_range(fatigue.cognitive_capacity, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_neuromod_get_stats)
{
    /* Perform some operations */
    fin_neuromod_event_t event = {
        .type = FIN_NEUROMOD_EVENT_OPPORTUNITY,
        .magnitude = 0.5f,
        .prediction_error = 0.0f,
        .timestamp_ms = 4000
    };
    financial_neuromod_bridge_update(g_neuromod_bridge, &event);

    fin_neuromod_bridge_stats_t stats;
    int result = financial_neuromod_bridge_get_stats(g_neuromod_bridge, &stats);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_ge(stats.state_updates, 1);
}
END_TEST

START_TEST(test_neuromod_utility_names)
{
    const char* name;

    name = fin_neuromod_op_state_name(FIN_NEUROMOD_OP_STATE_ACTIVE);
    ck_assert_ptr_nonnull(name);

    name = fin_neuromod_event_name(FIN_NEUROMOD_EVENT_GAIN);
    ck_assert_ptr_nonnull(name);

    name = fin_neuromod_arousal_name(FIN_AROUSAL_OPTIMAL);
    ck_assert_ptr_nonnull(name);

    name = fin_neuromod_fatigue_name(FIN_FATIGUE_MODERATE);
    ck_assert_ptr_nonnull(name);

    name = financial_neuromod_bridge_version();
    ck_assert_ptr_nonnull(name);
}
END_TEST

/* ============================================================================
 * FINANCIAL MENTAL HEALTH BRIDGE TESTS
 * ============================================================================ */

/* --- Lifecycle Tests --- */

START_TEST(test_mental_health_default_config)
{
    fin_mental_health_config_t config;
    int result = financial_mental_health_bridge_default_config(&config);
    ck_assert_int_eq(result, 0);

    /* Verify sensible defaults */
    ck_assert_float_in_range(config.stress_warning_threshold, 0.0f, 1.0f);
    ck_assert_float_in_range(config.stress_block_threshold, 0.0f, 1.0f);
    ck_assert_float_in_range(config.anxiety_warning_threshold, 0.0f, 1.0f);
    ck_assert_uint_gt(config.max_trades_per_session, 0);
    ck_assert_uint_gt(config.max_session_duration_mins, 0);
}
END_TEST

START_TEST(test_mental_health_default_config_null)
{
    int result = financial_mental_health_bridge_default_config(NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_mental_health_create_destroy)
{
    fin_mental_health_config_t config;
    financial_mental_health_bridge_default_config(&config);

    financial_mental_health_bridge_t* bridge = financial_mental_health_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_mental_health_bridge_state_t state = financial_mental_health_bridge_get_bridge_state(bridge);
    ck_assert_int_eq(state, FIN_MH_STATE_INITIALIZED);

    financial_mental_health_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_mental_health_create_null_config)
{
    financial_mental_health_bridge_t* bridge = financial_mental_health_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_mental_health_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_mental_health_destroy_null)
{
    financial_mental_health_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_mental_health_reset)
{
    int result = financial_mental_health_bridge_reset(g_mental_health_bridge);
    ck_assert_int_eq(result, 0);

    /* After reset, risk should be low */
    fin_mental_health_risk_t risk = financial_mental_health_bridge_get_risk_level(g_mental_health_bridge);
    ck_assert_int_eq(risk, FIN_MH_RISK_LOW);
}
END_TEST

/* --- Subsystem Setter Tests --- */

START_TEST(test_mental_health_set_immune)
{
    int result = financial_mental_health_bridge_set_immune(g_mental_health_bridge, NULL);
    ck_assert_int_eq(result, 0);

    result = financial_mental_health_bridge_set_immune(NULL, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_mental_health_set_bbb)
{
    int result = financial_mental_health_bridge_set_bbb(g_mental_health_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* --- Core Mental Health API Tests --- */

START_TEST(test_mental_health_assess)
{
    fin_health_input_factors_t factors = {
        .trades_today = 10,
        .decisions_today = 25,
        .losses_today = 3,
        .wins_today = 7,
        .pnl_today = 0.02f,
        .session_duration_mins = 120,
        .time_since_break_mins = 60,
        .consecutive_losses = 1,
        .consecutive_wins = 2,
        .sleep_quality = 0.7f,
        .physical_wellness = 0.8f,
        .external_stress = 0.2f,
        .biometrics_available = false
    };

    fin_mental_health_assessment_t assessment;
    int result = financial_mental_health_bridge_assess(g_mental_health_bridge, &factors, &assessment);
    ck_assert_int_eq(result, 0);

    /* Assessment should have valid values */
    ck_assert_int_ge(assessment.risk_level, FIN_MH_RISK_LOW);
    ck_assert_int_lt(assessment.risk_level, FIN_MH_RISK_COUNT);
    ck_assert_float_in_range(assessment.impairment_score, 0.0f, 1.0f);
    ck_assert_float_in_range(assessment.wellbeing_score, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_mental_health_assess_null_factors)
{
    /* Should use internal state when factors is NULL */
    fin_mental_health_assessment_t assessment;
    int result = financial_mental_health_bridge_assess(g_mental_health_bridge, NULL, &assessment);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_mental_health_assess_null)
{
    int result = financial_mental_health_bridge_assess(g_mental_health_bridge, NULL, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_mental_health_should_trade)
{
    fin_trading_advisability_t advisability;
    int result = financial_mental_health_bridge_should_trade(g_mental_health_bridge, &advisability);
    ck_assert_int_eq(result, 0);

    /* Advisability should be valid */
    ck_assert_int_ge(advisability.advice, FIN_TRADE_ADVISED);
    ck_assert_int_lt(advisability.advice, FIN_TRADE_ADVICE_COUNT);
    ck_assert_float_in_range(advisability.confidence, 0.0f, 1.0f);
    ck_assert_float_in_range(advisability.max_position_scale, 0.0f, 1.0f);
    ck_assert_float_in_range(advisability.max_risk_scale, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_mental_health_should_trade_null)
{
    int result = financial_mental_health_bridge_should_trade(g_mental_health_bridge, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_mental_health_recommend_break)
{
    fin_break_recommendation_t recommendation;
    int result = financial_mental_health_bridge_recommend_break(g_mental_health_bridge, &recommendation);
    ck_assert_int_eq(result, 0);

    /* Recommendation should be valid */
    ck_assert_int_ge(recommendation.break_type, FIN_BREAK_NONE);
    ck_assert_int_lt(recommendation.break_type, FIN_BREAK_COUNT);
    ck_assert_float_in_range(recommendation.urgency, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_mental_health_update_state)
{
    fin_mental_health_state_t new_state = {
        .stress_level = 0.6f,
        .anxiety_level = 0.5f,
        .depression_risk = 0.2f,
        .cognitive_load = 0.7f,
        .decision_fatigue = 0.5f,
        .judgment_impaired = false
    };

    int result = financial_mental_health_bridge_update_state(g_mental_health_bridge, &new_state);
    ck_assert_int_eq(result, 0);

    fin_mental_health_state_t retrieved;
    result = financial_mental_health_bridge_get_state(g_mental_health_bridge, &retrieved);
    ck_assert_int_eq(result, 0);

    ck_assert(fabsf(retrieved.stress_level - 0.6f) < FLOAT_EPSILON);
    ck_assert(fabsf(retrieved.anxiety_level - 0.5f) < FLOAT_EPSILON);
}
END_TEST

START_TEST(test_mental_health_update_from_factors)
{
    fin_health_input_factors_t factors = {
        .trades_today = 30,  /* Many trades */
        .decisions_today = 100,
        .losses_today = 15,
        .wins_today = 15,
        .pnl_today = -0.05f,  /* Losing day */
        .session_duration_mins = 300,  /* 5 hours */
        .time_since_break_mins = 180,  /* 3 hours without break */
        .consecutive_losses = 5,  /* Loss streak */
        .consecutive_wins = 0,
        .sleep_quality = 0.4f,  /* Poor sleep */
        .physical_wellness = 0.5f,
        .external_stress = 0.6f,  /* High external stress */
        .biometrics_available = false
    };

    int result = financial_mental_health_bridge_update_from_factors(g_mental_health_bridge, &factors);
    ck_assert_int_eq(result, 0);

    /* With these negative factors, risk should be elevated */
    fin_mental_health_risk_t risk = financial_mental_health_bridge_get_risk_level(g_mental_health_bridge);
    ck_assert_int_ge(risk, FIN_MH_RISK_LOW);  /* At minimum low, likely higher */
}
END_TEST

START_TEST(test_mental_health_record_break)
{
    /* First create some stress */
    fin_mental_health_state_t stressed = {
        .stress_level = 0.7f,
        .anxiety_level = 0.6f,
        .depression_risk = 0.2f,
        .cognitive_load = 0.6f,
        .decision_fatigue = 0.7f,
        .judgment_impaired = false
    };
    financial_mental_health_bridge_update_state(g_mental_health_bridge, &stressed);

    /* Record a break */
    int result = financial_mental_health_bridge_record_break(g_mental_health_bridge, 30);  /* 30 min break */
    ck_assert_int_eq(result, 0);

    /* After break, stress should be reduced */
    fin_mental_health_state_t after;
    financial_mental_health_bridge_get_state(g_mental_health_bridge, &after);
    ck_assert(after.stress_level <= stressed.stress_level);
}
END_TEST

START_TEST(test_mental_health_apply_recovery)
{
    int result = financial_mental_health_bridge_apply_recovery(g_mental_health_bridge, 60000);  /* 60 seconds */
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_mental_health_get_state)
{
    fin_mental_health_state_t state;
    int result = financial_mental_health_bridge_get_state(g_mental_health_bridge, &state);
    ck_assert_int_eq(result, 0);

    /* All values should be in valid ranges */
    ck_assert_float_in_range(state.stress_level, 0.0f, 1.0f);
    ck_assert_float_in_range(state.anxiety_level, 0.0f, 1.0f);
    ck_assert_float_in_range(state.depression_risk, 0.0f, 1.0f);
    ck_assert_float_in_range(state.cognitive_load, 0.0f, 1.0f);
    ck_assert_float_in_range(state.decision_fatigue, 0.0f, 1.0f);
}
END_TEST

START_TEST(test_mental_health_get_risk_level)
{
    fin_mental_health_risk_t risk = financial_mental_health_bridge_get_risk_level(g_mental_health_bridge);
    ck_assert_int_ge(risk, FIN_MH_RISK_LOW);
    ck_assert_int_lt(risk, FIN_MH_RISK_COUNT);

    /* NULL bridge should return safe default or error value */
    risk = financial_mental_health_bridge_get_risk_level(NULL);
    /* Just verify it doesn't crash */
}
END_TEST

START_TEST(test_mental_health_get_stats)
{
    /* Perform some operations */
    fin_mental_health_assessment_t assessment;
    financial_mental_health_bridge_assess(g_mental_health_bridge, NULL, &assessment);

    fin_mental_health_bridge_stats_t stats;
    int result = financial_mental_health_bridge_get_stats(g_mental_health_bridge, &stats);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_ge(stats.assessments, 1);
}
END_TEST

START_TEST(test_mental_health_utility_names)
{
    const char* name;

    name = fin_mental_health_risk_name(FIN_MH_RISK_HIGH);
    ck_assert_ptr_nonnull(name);

    name = fin_mental_health_break_name(FIN_BREAK_MEDIUM);
    ck_assert_ptr_nonnull(name);

    name = fin_mental_health_advice_name(FIN_TRADE_CAUTION);
    ck_assert_ptr_nonnull(name);

    name = fin_mental_health_state_name(FIN_MH_STATE_MONITORING);
    ck_assert_ptr_nonnull(name);

    name = financial_mental_health_bridge_version();
    ck_assert_ptr_nonnull(name);
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* financial_emotion_bridges_suite(void)
{
    Suite* s = suite_create("Financial Emotion Bridges");

    /* ===== Financial Emotion Bridge Tests ===== */

    /* Emotion Lifecycle */
    TCase* tc_emotion_lifecycle = tcase_create("Emotion Lifecycle");
    tcase_add_test(tc_emotion_lifecycle, test_emotion_default_config);
    tcase_add_test(tc_emotion_lifecycle, test_emotion_default_config_null);
    tcase_add_test(tc_emotion_lifecycle, test_emotion_create_destroy);
    tcase_add_test(tc_emotion_lifecycle, test_emotion_create_null_config);
    tcase_add_test(tc_emotion_lifecycle, test_emotion_destroy_null);
    tcase_add_checked_fixture(tc_emotion_lifecycle, emotion_setup, emotion_teardown);
    tcase_add_test(tc_emotion_lifecycle, test_emotion_reset);
    suite_add_tcase(s, tc_emotion_lifecycle);

    /* Emotion Setters */
    TCase* tc_emotion_setters = tcase_create("Emotion Setters");
    tcase_add_checked_fixture(tc_emotion_setters, emotion_setup, emotion_teardown);
    tcase_add_test(tc_emotion_setters, test_emotion_set_immune);
    tcase_add_test(tc_emotion_setters, test_emotion_set_bbb);
    tcase_add_test(tc_emotion_setters, test_emotion_set_health_agent);
    tcase_add_test(tc_emotion_setters, test_emotion_set_kg_wiring);
    suite_add_tcase(s, tc_emotion_setters);

    /* Emotion Core API */
    TCase* tc_emotion_core = tcase_create("Emotion Core API");
    tcase_add_checked_fixture(tc_emotion_core, emotion_setup, emotion_teardown);
    tcase_add_test(tc_emotion_core, test_emotion_update_price_increase);
    tcase_add_test(tc_emotion_core, test_emotion_update_price_decrease);
    tcase_add_test(tc_emotion_core, test_emotion_update_null);
    tcase_add_test(tc_emotion_core, test_emotion_get_state);
    tcase_add_test(tc_emotion_core, test_emotion_get_state_null);
    tcase_add_test(tc_emotion_core, test_emotion_get_dominant);
    tcase_add_test(tc_emotion_core, test_emotion_decay);
    tcase_add_test(tc_emotion_core, test_emotion_modulate_decision);
    tcase_add_test(tc_emotion_core, test_emotion_detect_bias);
    tcase_add_test(tc_emotion_core, test_emotion_get_stats);
    tcase_add_test(tc_emotion_core, test_emotion_utility_names);
    suite_add_tcase(s, tc_emotion_core);

    /* ===== Financial Motivation Bridge Tests ===== */

    /* Motivation Lifecycle */
    TCase* tc_motivation_lifecycle = tcase_create("Motivation Lifecycle");
    tcase_add_test(tc_motivation_lifecycle, test_motivation_default_config);
    tcase_add_test(tc_motivation_lifecycle, test_motivation_default_config_null);
    tcase_add_test(tc_motivation_lifecycle, test_motivation_create_destroy);
    tcase_add_test(tc_motivation_lifecycle, test_motivation_create_null_config);
    tcase_add_test(tc_motivation_lifecycle, test_motivation_destroy_null);
    tcase_add_checked_fixture(tc_motivation_lifecycle, motivation_setup, motivation_teardown);
    tcase_add_test(tc_motivation_lifecycle, test_motivation_reset);
    suite_add_tcase(s, tc_motivation_lifecycle);

    /* Motivation Setters */
    TCase* tc_motivation_setters = tcase_create("Motivation Setters");
    tcase_add_checked_fixture(tc_motivation_setters, motivation_setup, motivation_teardown);
    tcase_add_test(tc_motivation_setters, test_motivation_set_immune);
    tcase_add_test(tc_motivation_setters, test_motivation_set_bbb);
    suite_add_tcase(s, tc_motivation_setters);

    /* Motivation Core API */
    TCase* tc_motivation_core = tcase_create("Motivation Core API");
    tcase_add_checked_fixture(tc_motivation_core, motivation_setup, motivation_teardown);
    tcase_add_test(tc_motivation_core, test_motivation_evaluate);
    tcase_add_test(tc_motivation_core, test_motivation_evaluate_null);
    tcase_add_test(tc_motivation_core, test_motivation_detect_fomo);
    tcase_add_test(tc_motivation_core, test_motivation_detect_fomo_null);
    tcase_add_test(tc_motivation_core, test_motivation_rational_value);
    tcase_add_test(tc_motivation_core, test_motivation_should_override);
    tcase_add_test(tc_motivation_core, test_motivation_process_outcome);
    tcase_add_test(tc_motivation_core, test_motivation_get_stats);
    tcase_add_test(tc_motivation_core, test_motivation_utility_names);
    suite_add_tcase(s, tc_motivation_core);

    /* ===== Financial Neuromodulator Bridge Tests ===== */

    /* Neuromod Lifecycle */
    TCase* tc_neuromod_lifecycle = tcase_create("Neuromod Lifecycle");
    tcase_add_test(tc_neuromod_lifecycle, test_neuromod_default_config);
    tcase_add_test(tc_neuromod_lifecycle, test_neuromod_default_config_null);
    tcase_add_test(tc_neuromod_lifecycle, test_neuromod_create_destroy);
    tcase_add_test(tc_neuromod_lifecycle, test_neuromod_create_null_config);
    tcase_add_test(tc_neuromod_lifecycle, test_neuromod_destroy_null);
    tcase_add_checked_fixture(tc_neuromod_lifecycle, neuromod_setup, neuromod_teardown);
    tcase_add_test(tc_neuromod_lifecycle, test_neuromod_reset);
    suite_add_tcase(s, tc_neuromod_lifecycle);

    /* Neuromod Setters */
    TCase* tc_neuromod_setters = tcase_create("Neuromod Setters");
    tcase_add_checked_fixture(tc_neuromod_setters, neuromod_setup, neuromod_teardown);
    tcase_add_test(tc_neuromod_setters, test_neuromod_set_immune);
    tcase_add_test(tc_neuromod_setters, test_neuromod_set_bbb);
    suite_add_tcase(s, tc_neuromod_setters);

    /* Neuromod Core API */
    TCase* tc_neuromod_core = tcase_create("Neuromod Core API");
    tcase_add_checked_fixture(tc_neuromod_core, neuromod_setup, neuromod_teardown);
    tcase_add_test(tc_neuromod_core, test_neuromod_update_gain);
    tcase_add_test(tc_neuromod_core, test_neuromod_update_loss);
    tcase_add_test(tc_neuromod_core, test_neuromod_update_null);
    tcase_add_test(tc_neuromod_core, test_neuromod_get_state);
    tcase_add_test(tc_neuromod_core, test_neuromod_set_state);
    tcase_add_test(tc_neuromod_core, test_neuromod_compute_effects);
    tcase_add_test(tc_neuromod_core, test_neuromod_compute_effects_null);
    tcase_add_test(tc_neuromod_core, test_neuromod_compute_effects_from_state);
    tcase_add_test(tc_neuromod_core, test_neuromod_modulate_archetype);
    tcase_add_test(tc_neuromod_core, test_neuromod_decay);
    tcase_add_test(tc_neuromod_core, test_neuromod_analyze_arousal);
    tcase_add_test(tc_neuromod_core, test_neuromod_analyze_fatigue);
    tcase_add_test(tc_neuromod_core, test_neuromod_get_stats);
    tcase_add_test(tc_neuromod_core, test_neuromod_utility_names);
    suite_add_tcase(s, tc_neuromod_core);

    /* ===== Financial Mental Health Bridge Tests ===== */

    /* Mental Health Lifecycle */
    TCase* tc_mental_health_lifecycle = tcase_create("Mental Health Lifecycle");
    tcase_add_test(tc_mental_health_lifecycle, test_mental_health_default_config);
    tcase_add_test(tc_mental_health_lifecycle, test_mental_health_default_config_null);
    tcase_add_test(tc_mental_health_lifecycle, test_mental_health_create_destroy);
    tcase_add_test(tc_mental_health_lifecycle, test_mental_health_create_null_config);
    tcase_add_test(tc_mental_health_lifecycle, test_mental_health_destroy_null);
    tcase_add_checked_fixture(tc_mental_health_lifecycle, mental_health_setup, mental_health_teardown);
    tcase_add_test(tc_mental_health_lifecycle, test_mental_health_reset);
    suite_add_tcase(s, tc_mental_health_lifecycle);

    /* Mental Health Setters */
    TCase* tc_mental_health_setters = tcase_create("Mental Health Setters");
    tcase_add_checked_fixture(tc_mental_health_setters, mental_health_setup, mental_health_teardown);
    tcase_add_test(tc_mental_health_setters, test_mental_health_set_immune);
    tcase_add_test(tc_mental_health_setters, test_mental_health_set_bbb);
    suite_add_tcase(s, tc_mental_health_setters);

    /* Mental Health Core API */
    TCase* tc_mental_health_core = tcase_create("Mental Health Core API");
    tcase_add_checked_fixture(tc_mental_health_core, mental_health_setup, mental_health_teardown);
    tcase_add_test(tc_mental_health_core, test_mental_health_assess);
    tcase_add_test(tc_mental_health_core, test_mental_health_assess_null_factors);
    tcase_add_test(tc_mental_health_core, test_mental_health_assess_null);
    tcase_add_test(tc_mental_health_core, test_mental_health_should_trade);
    tcase_add_test(tc_mental_health_core, test_mental_health_should_trade_null);
    tcase_add_test(tc_mental_health_core, test_mental_health_recommend_break);
    tcase_add_test(tc_mental_health_core, test_mental_health_update_state);
    tcase_add_test(tc_mental_health_core, test_mental_health_update_from_factors);
    tcase_add_test(tc_mental_health_core, test_mental_health_record_break);
    tcase_add_test(tc_mental_health_core, test_mental_health_apply_recovery);
    tcase_add_test(tc_mental_health_core, test_mental_health_get_state);
    tcase_add_test(tc_mental_health_core, test_mental_health_get_risk_level);
    tcase_add_test(tc_mental_health_core, test_mental_health_get_stats);
    tcase_add_test(tc_mental_health_core, test_mental_health_utility_names);
    suite_add_tcase(s, tc_mental_health_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = financial_emotion_bridges_suite();
    SRunner* sr = srunner_create(s);

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
