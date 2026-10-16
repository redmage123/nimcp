/**
 * @file test_financial_cognitive_bridges.c
 * @brief Unit tests for Financial World Model and Theory of Mind bridges
 *
 * WHAT: Test suite for financial cognitive model bridges
 * WHY:  Verify correct behavior of world model prediction, counterfactual analysis,
 *       policy rollout, investor modeling, and false belief detection
 * HOW:  Unit tests using Check framework covering all bridge API functions
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "cognitive/parietal/nimcp_financial_world_model_bridge.h"
#include "cognitive/parietal/nimcp_financial_tom_bridge.h"

/* ============================================================================
 * World Model Bridge Test Fixtures
 * ============================================================================ */

static financial_world_model_bridge_t* g_world_bridge = NULL;

static void setup_world_model(void)
{
    fin_world_model_config_t config;
    financial_world_model_bridge_default_config(&config);
    config.max_assets = 16;
    config.default_trajectory_len = 10;
    config.enable_immune_integration = false;  /* Isolated tests */
    config.enable_bbb_validation = false;
    config.enable_kg_messaging = false;
    config.enable_health_monitoring = false;
    config.verbose_logging = false;

    g_world_bridge = financial_world_model_bridge_create(&config);
    ck_assert_ptr_nonnull(g_world_bridge);
}

static void teardown_world_model(void)
{
    if (g_world_bridge) {
        financial_world_model_bridge_destroy(g_world_bridge);
        g_world_bridge = NULL;
    }
}

/* ============================================================================
 * ToM Bridge Test Fixtures
 * ============================================================================ */

static financial_tom_bridge_t* g_tom_bridge = NULL;

static void setup_tom(void)
{
    fin_tom_config_t config;
    financial_tom_bridge_default_config(&config);
    config.max_models = 32;
    config.default_confidence = 0.7f;
    config.enable_rationale_generation = true;
    config.enable_immune_integration = false;  /* Isolated tests */
    config.enable_bbb_validation = false;
    config.enable_kg_messaging = false;
    config.enable_health_monitoring = false;
    config.verbose_logging = false;

    g_tom_bridge = financial_tom_bridge_create(&config);
    ck_assert_ptr_nonnull(g_tom_bridge);
}

static void teardown_tom(void)
{
    if (g_tom_bridge) {
        financial_tom_bridge_destroy(g_tom_bridge);
        g_tom_bridge = NULL;
    }
}

/* ============================================================================
 * World Model Bridge: Lifecycle Tests
 * ============================================================================ */

START_TEST(test_world_model_create_default)
{
    financial_world_model_bridge_t* bridge = financial_world_model_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);

    fin_world_bridge_state_t state = financial_world_model_bridge_get_bridge_state(bridge);
    ck_assert_int_eq(state, FIN_WORLD_STATE_INITIALIZED);

    financial_world_model_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_world_model_create_with_config)
{
    fin_world_model_config_t config;
    financial_world_model_bridge_default_config(&config);
    config.max_assets = 64;
    config.monte_carlo_samples = 100;

    financial_world_model_bridge_t* bridge = financial_world_model_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);
    financial_world_model_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_world_model_destroy_null)
{
    /* Should not crash */
    financial_world_model_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_world_model_reset)
{
    int result = financial_world_model_bridge_reset(g_world_bridge);
    ck_assert_int_eq(result, 0);

    fin_world_bridge_state_t state = financial_world_model_bridge_get_bridge_state(g_world_bridge);
    ck_assert_int_eq(state, FIN_WORLD_STATE_INITIALIZED);
}
END_TEST

START_TEST(test_world_model_default_config)
{
    fin_world_model_config_t config;
    int result = financial_world_model_bridge_default_config(&config);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_gt(config.max_assets, 0);
    ck_assert_uint_gt(config.default_trajectory_len, 0);
}
END_TEST

START_TEST(test_world_model_default_config_null)
{
    int result = financial_world_model_bridge_default_config(NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * World Model Bridge: Subsystem Setter Tests
 * ============================================================================ */

START_TEST(test_world_model_set_immune)
{
    /* Test with NULL immune (disconnect) */
    int result = financial_world_model_bridge_set_immune(g_world_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_world_model_set_bbb)
{
    int result = financial_world_model_bridge_set_bbb(g_world_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_world_model_set_health_agent)
{
    int result = financial_world_model_bridge_set_health_agent(g_world_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_world_model_set_kg_wiring)
{
    int result = financial_world_model_bridge_set_kg_wiring(g_world_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_world_model_set_logger)
{
    int result = financial_world_model_bridge_set_logger(g_world_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_world_model_subsystem_null_bridge)
{
    /* All setters should return non-zero error for NULL bridge */
    ck_assert_int_ne(financial_world_model_bridge_set_immune(NULL, NULL), 0);
    ck_assert_int_ne(financial_world_model_bridge_set_bbb(NULL, NULL), 0);
    ck_assert_int_ne(financial_world_model_bridge_set_health_agent(NULL, NULL), 0);
    ck_assert_int_ne(financial_world_model_bridge_set_kg_wiring(NULL, NULL), 0);
    ck_assert_int_ne(financial_world_model_bridge_set_logger(NULL, NULL), 0);
}
END_TEST

/* ============================================================================
 * World Model Bridge: State Management Tests
 * ============================================================================ */

START_TEST(test_world_model_set_state)
{
    fin_world_state_t state;
    memset(&state, 0, sizeof(state));
    ck_assert_int_eq(fin_world_state_alloc(&state, 4), 0);

    state.num_assets = 4;
    state.regime = FIN_REGIME_BULL;
    state.timestamp_ms = 1234567890ULL;
    for (uint32_t i = 0; i < 4; i++) {
        state.asset_prices[i] = 100.0f + (float)i * 10.0f;
        state.volatilities[i] = 0.2f;
    }

    int result = financial_world_model_bridge_set_state(g_world_bridge, &state);
    ck_assert_int_eq(result, 0);

    fin_world_state_free(&state);
}
END_TEST

START_TEST(test_world_model_get_state)
{
    /* First set some state */
    fin_world_state_t input_state;
    memset(&input_state, 0, sizeof(input_state));
    ck_assert_int_eq(fin_world_state_alloc(&input_state, 4), 0);

    input_state.num_assets = 4;
    input_state.regime = FIN_REGIME_SIDEWAYS;
    input_state.timestamp_ms = 999999ULL;
    for (uint32_t i = 0; i < 4; i++) {
        input_state.asset_prices[i] = 50.0f + (float)i;
        input_state.volatilities[i] = 0.15f;
    }

    int result = financial_world_model_bridge_set_state(g_world_bridge, &input_state);
    ck_assert_int_eq(result, 0);

    /* Now retrieve it */
    fin_world_state_t output_state;
    memset(&output_state, 0, sizeof(output_state));
    ck_assert_int_eq(fin_world_state_alloc(&output_state, 4), 0);

    result = financial_world_model_bridge_get_state(g_world_bridge, &output_state);
    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(output_state.regime, FIN_REGIME_SIDEWAYS);

    fin_world_state_free(&input_state);
    fin_world_state_free(&output_state);
}
END_TEST

START_TEST(test_world_model_update_asset)
{
    /* Initialize state first */
    fin_world_state_t state;
    memset(&state, 0, sizeof(state));
    ck_assert_int_eq(fin_world_state_alloc(&state, 4), 0);
    state.num_assets = 4;
    for (uint32_t i = 0; i < 4; i++) {
        state.asset_prices[i] = 100.0f;
        state.volatilities[i] = 0.2f;
    }
    financial_world_model_bridge_set_state(g_world_bridge, &state);
    fin_world_state_free(&state);

    /* Update specific asset */
    int result = financial_world_model_bridge_update_asset(g_world_bridge, 2, 150.0f, 0.25f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_world_model_set_regime)
{
    int result = financial_world_model_bridge_set_regime(g_world_bridge, FIN_REGIME_CRISIS);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_world_model_state_null_params)
{
    ck_assert_int_ne(financial_world_model_bridge_set_state(NULL, NULL), 0);
    ck_assert_int_ne(financial_world_model_bridge_set_state(g_world_bridge, NULL), 0);
    ck_assert_int_ne(financial_world_model_bridge_get_state(NULL, NULL), 0);
}
END_TEST

/* ============================================================================
 * World Model Bridge: Prediction Tests
 * ============================================================================ */

START_TEST(test_world_model_predict_forward)
{
    /* Set up initial state */
    fin_world_state_t state;
    memset(&state, 0, sizeof(state));
    ck_assert_int_eq(fin_world_state_alloc(&state, 4), 0);
    state.num_assets = 4;
    state.regime = FIN_REGIME_BULL;
    for (uint32_t i = 0; i < 4; i++) {
        state.asset_prices[i] = 100.0f;
        state.volatilities[i] = 0.2f;
    }
    financial_world_model_bridge_set_state(g_world_bridge, &state);
    fin_world_state_free(&state);

    /* Allocate trajectory */
    uint32_t horizon = 5;
    fin_world_state_t* trajectory = calloc(horizon, sizeof(fin_world_state_t));
    ck_assert_ptr_nonnull(trajectory);
    for (uint32_t i = 0; i < horizon; i++) {
        ck_assert_int_eq(fin_world_state_alloc(&trajectory[i], 4), 0);
    }

    int result = financial_world_model_bridge_predict_forward(
        g_world_bridge,
        horizon,
        FIN_PRED_MODEL_RANDOM_WALK,
        trajectory
    );
    ck_assert_int_eq(result, 0);

    /* Verify some prediction was made */
    ck_assert(trajectory[0].asset_prices[0] > 0.0f);

    for (uint32_t i = 0; i < horizon; i++) {
        fin_world_state_free(&trajectory[i]);
    }
    free(trajectory);
}
END_TEST

START_TEST(test_world_model_predict_null_params)
{
    ck_assert_int_ne(financial_world_model_bridge_predict_forward(NULL, 5, FIN_PRED_MODEL_RANDOM_WALK, NULL), 0);
    ck_assert_int_ne(financial_world_model_bridge_predict_forward(g_world_bridge, 5, FIN_PRED_MODEL_RANDOM_WALK, NULL), 0);
}
END_TEST

/* ============================================================================
 * World Model Bridge: Counterfactual Tests
 * ============================================================================ */

START_TEST(test_world_model_counterfactual)
{
    /* Set up initial state */
    fin_world_state_t state;
    memset(&state, 0, sizeof(state));
    ck_assert_int_eq(fin_world_state_alloc(&state, 4), 0);
    state.num_assets = 4;
    state.regime = FIN_REGIME_BULL;
    for (uint32_t i = 0; i < 4; i++) {
        state.asset_prices[i] = 100.0f;
        state.volatilities[i] = 0.2f;
    }
    financial_world_model_bridge_set_state(g_world_bridge, &state);

    /* Create perturbation - "what if price jumped 20%?" */
    fin_world_state_t perturbation;
    memset(&perturbation, 0, sizeof(perturbation));
    ck_assert_int_eq(fin_world_state_alloc(&perturbation, 4), 0);
    perturbation.num_assets = 4;
    perturbation.regime = FIN_REGIME_BULL;
    for (uint32_t i = 0; i < 4; i++) {
        perturbation.asset_prices[i] = 120.0f;  /* 20% jump */
        perturbation.volatilities[i] = 0.3f;    /* Higher vol */
    }

    fin_counterfactual_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = financial_world_model_bridge_counterfactual(
        g_world_bridge,
        &perturbation,
        10,
        &result
    );
    ck_assert_int_eq(ret, 0);

    /* Check result populated */
    ck_assert_uint_gt(result.trajectory_len, 0);
    ck_assert(result.probability >= 0.0f && result.probability <= 1.0f);

    financial_world_model_bridge_free_counterfactual(&result);
    fin_world_state_free(&state);
    fin_world_state_free(&perturbation);
}
END_TEST

START_TEST(test_world_model_counterfactual_null)
{
    fin_counterfactual_result_t result;
    ck_assert_int_ne(financial_world_model_bridge_counterfactual(NULL, NULL, 10, &result), 0);
    ck_assert_int_ne(financial_world_model_bridge_counterfactual(g_world_bridge, NULL, 10, &result), 0);
}
END_TEST

/* ============================================================================
 * World Model Bridge: Policy Rollout Tests
 * ============================================================================ */

START_TEST(test_world_model_rollout_policy)
{
    /* Set up initial state */
    fin_world_state_t state;
    memset(&state, 0, sizeof(state));
    ck_assert_int_eq(fin_world_state_alloc(&state, 4), 0);
    state.num_assets = 4;
    state.regime = FIN_REGIME_BULL;
    for (uint32_t i = 0; i < 4; i++) {
        state.asset_prices[i] = 100.0f;
        state.volatilities[i] = 0.2f;
    }
    financial_world_model_bridge_set_state(g_world_bridge, &state);
    fin_world_state_free(&state);

    /* Define policy actions */
    fin_policy_action_t actions[2];
    memset(actions, 0, sizeof(actions));

    actions[0].asset_index = 0;
    actions[0].position_delta = 0.5f;   /* Buy 50% */
    actions[0].stop_loss = 0.95f;
    actions[0].take_profit = 1.10f;

    actions[1].asset_index = 1;
    actions[1].position_delta = -0.3f;  /* Short 30% */
    actions[1].stop_loss = 1.05f;
    actions[1].take_profit = 0.90f;

    fin_rollout_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = financial_world_model_bridge_rollout_policy(
        g_world_bridge,
        actions,
        2,
        100000.0f,  /* Initial capital */
        &result
    );
    ck_assert_int_eq(ret, 0);

    /* Check result is populated */
    ck_assert_uint_gt(result.trajectory_len, 0);

    financial_world_model_bridge_free_rollout(&result);
}
END_TEST

START_TEST(test_world_model_rollout_null)
{
    fin_rollout_result_t result;
    ck_assert_int_ne(financial_world_model_bridge_rollout_policy(NULL, NULL, 0, 100000.0f, &result), 0);
}
END_TEST

/* ============================================================================
 * World Model Bridge: Statistics Tests
 * ============================================================================ */

START_TEST(test_world_model_get_stats)
{
    fin_world_model_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = financial_world_model_bridge_get_stats(g_world_bridge, &stats);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_world_model_reset_stats)
{
    financial_world_model_bridge_reset_stats(g_world_bridge);

    fin_world_model_bridge_stats_t stats;
    int result = financial_world_model_bridge_get_stats(g_world_bridge, &stats);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_eq(stats.predictions, 0);
    ck_assert_uint_eq(stats.counterfactuals, 0);
}
END_TEST

START_TEST(test_world_model_stats_null)
{
    ck_assert_int_ne(financial_world_model_bridge_get_stats(NULL, NULL), 0);
    ck_assert_int_ne(financial_world_model_bridge_get_stats(g_world_bridge, NULL), 0);
}
END_TEST

/* ============================================================================
 * World Model Bridge: Utility Tests
 * ============================================================================ */

START_TEST(test_world_model_regime_names)
{
    ck_assert_str_ne(fin_world_regime_name(FIN_REGIME_BULL), "");
    ck_assert_str_ne(fin_world_regime_name(FIN_REGIME_BEAR), "");
    ck_assert_str_ne(fin_world_regime_name(FIN_REGIME_SIDEWAYS), "");
    ck_assert_str_ne(fin_world_regime_name(FIN_REGIME_CRISIS), "");
}
END_TEST

START_TEST(test_world_model_version)
{
    const char* version = financial_world_model_bridge_version();
    ck_assert_ptr_nonnull(version);
    ck_assert_str_eq(version, FINANCIAL_WORLD_MODEL_BRIDGE_VERSION);
}
END_TEST

/* ============================================================================
 * ToM Bridge: Lifecycle Tests
 * ============================================================================ */

START_TEST(test_tom_create_default)
{
    financial_tom_bridge_t* bridge = financial_tom_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);

    fin_tom_state_t state = financial_tom_bridge_get_state(bridge);
    ck_assert_int_eq(state, FIN_TOM_STATE_INITIALIZED);

    financial_tom_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_tom_create_with_config)
{
    fin_tom_config_t config;
    financial_tom_bridge_default_config(&config);
    config.max_models = 128;
    config.enable_rationale_generation = true;

    financial_tom_bridge_t* bridge = financial_tom_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);
    financial_tom_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_tom_destroy_null)
{
    /* Should not crash */
    financial_tom_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_tom_reset)
{
    int result = financial_tom_bridge_reset(g_tom_bridge);
    ck_assert_int_eq(result, 0);

    uint32_t count = financial_tom_bridge_get_model_count(g_tom_bridge);
    ck_assert_uint_eq(count, 0);
}
END_TEST

START_TEST(test_tom_default_config)
{
    fin_tom_config_t config;
    int result = financial_tom_bridge_default_config(&config);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_gt(config.max_models, 0);
    ck_assert(config.default_confidence > 0.0f && config.default_confidence <= 1.0f);
}
END_TEST

START_TEST(test_tom_default_config_null)
{
    int result = financial_tom_bridge_default_config(NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * ToM Bridge: Subsystem Setter Tests
 * ============================================================================ */

START_TEST(test_tom_set_immune)
{
    int result = financial_tom_bridge_set_immune(g_tom_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_tom_set_bbb)
{
    int result = financial_tom_bridge_set_bbb(g_tom_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_tom_set_health_agent)
{
    int result = financial_tom_bridge_set_health_agent(g_tom_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_tom_set_kg_wiring)
{
    int result = financial_tom_bridge_set_kg_wiring(g_tom_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_tom_set_logger)
{
    int result = financial_tom_bridge_set_logger(g_tom_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_tom_subsystem_null_bridge)
{
    ck_assert_int_ne(financial_tom_bridge_set_immune(NULL, NULL), 0);
    ck_assert_int_ne(financial_tom_bridge_set_bbb(NULL, NULL), 0);
    ck_assert_int_ne(financial_tom_bridge_set_health_agent(NULL, NULL), 0);
    ck_assert_int_ne(financial_tom_bridge_set_kg_wiring(NULL, NULL), 0);
    ck_assert_int_ne(financial_tom_bridge_set_logger(NULL, NULL), 0);
}
END_TEST

/* ============================================================================
 * ToM Bridge: Investor Model Tests
 * ============================================================================ */

START_TEST(test_tom_model_investor)
{
    fin_investor_model_t model;
    memset(&model, 0, sizeof(model));

    int result = financial_tom_bridge_model_investor(
        g_tom_bridge,
        "investor_buffett_001",
        FIN_TOM_ARCHETYPE_BUFFETT,
        0.8f,
        &model
    );
    ck_assert_int_eq(result, 0);
    ck_assert_str_eq(model.investor_id, "investor_buffett_001");
    ck_assert_int_eq(model.archetype, FIN_TOM_ARCHETYPE_BUFFETT);
    ck_assert(fabsf(model.confidence - 0.8f) < 0.01f);

    uint32_t count = financial_tom_bridge_get_model_count(g_tom_bridge);
    ck_assert_uint_eq(count, 1);
}
END_TEST

START_TEST(test_tom_model_multiple_archetypes)
{
    fin_investor_model_t model;

    /* Create models for different archetypes */
    ck_assert_int_eq(financial_tom_bridge_model_investor(
        g_tom_bridge, "graham_01", FIN_TOM_ARCHETYPE_GRAHAM, 0.7f, &model), 0);
    ck_assert_int_eq(financial_tom_bridge_model_investor(
        g_tom_bridge, "soros_01", FIN_TOM_ARCHETYPE_SOROS, 0.9f, &model), 0);
    ck_assert_int_eq(financial_tom_bridge_model_investor(
        g_tom_bridge, "simons_01", FIN_TOM_ARCHETYPE_SIMONS, 0.85f, &model), 0);

    uint32_t count = financial_tom_bridge_get_model_count(g_tom_bridge);
    ck_assert_uint_eq(count, 3);
}
END_TEST

START_TEST(test_tom_get_model)
{
    /* Create a model first */
    financial_tom_bridge_model_investor(
        g_tom_bridge, "test_get", FIN_TOM_ARCHETYPE_LYNCH, 0.75f, NULL);

    fin_investor_model_t retrieved;
    memset(&retrieved, 0, sizeof(retrieved));

    int result = financial_tom_bridge_get_model(g_tom_bridge, "test_get", &retrieved);
    ck_assert_int_eq(result, 0);
    ck_assert_str_eq(retrieved.investor_id, "test_get");
    ck_assert_int_eq(retrieved.archetype, FIN_TOM_ARCHETYPE_LYNCH);
}
END_TEST

START_TEST(test_tom_update_beliefs)
{
    /* Create model */
    financial_tom_bridge_model_investor(
        g_tom_bridge, "update_beliefs_test", FIN_TOM_ARCHETYPE_TEMPLETON, 0.6f, NULL);

    /* Update beliefs */
    float beliefs[4] = {0.5f, -0.3f, 0.8f, 0.1f};
    int result = financial_tom_bridge_update_beliefs(
        g_tom_bridge, "update_beliefs_test", beliefs, 4);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_tom_update_emotion)
{
    /* Create model */
    financial_tom_bridge_model_investor(
        g_tom_bridge, "emotion_test", FIN_TOM_ARCHETYPE_DALIO, 0.7f, NULL);

    int result = financial_tom_bridge_update_emotion(
        g_tom_bridge, "emotion_test", FIN_TOM_EMOTION_FEAR, 0.4f);
    ck_assert_int_eq(result, 0);

    /* Verify emotion was updated */
    fin_investor_model_t model;
    financial_tom_bridge_get_model(g_tom_bridge, "emotion_test", &model);
    ck_assert_int_eq(model.emotional_state, FIN_TOM_EMOTION_FEAR);
}
END_TEST

START_TEST(test_tom_remove_model)
{
    /* Create then remove */
    financial_tom_bridge_model_investor(
        g_tom_bridge, "to_remove", FIN_TOM_ARCHETYPE_MUNGER, 0.5f, NULL);

    uint32_t before = financial_tom_bridge_get_model_count(g_tom_bridge);

    int result = financial_tom_bridge_remove_model(g_tom_bridge, "to_remove");
    ck_assert_int_eq(result, 0);

    uint32_t after = financial_tom_bridge_get_model_count(g_tom_bridge);
    ck_assert_uint_eq(after, before - 1);
}
END_TEST

START_TEST(test_tom_model_null_params)
{
    ck_assert_int_ne(financial_tom_bridge_model_investor(NULL, "id", FIN_TOM_ARCHETYPE_BUFFETT, 0.5f, NULL), 0);
    ck_assert_int_ne(financial_tom_bridge_model_investor(g_tom_bridge, NULL, FIN_TOM_ARCHETYPE_BUFFETT, 0.5f, NULL), 0);
}
END_TEST

/* ============================================================================
 * ToM Bridge: Action Prediction Tests
 * ============================================================================ */

START_TEST(test_tom_predict_action)
{
    /* Create investor model */
    financial_tom_bridge_model_investor(
        g_tom_bridge, "predictor", FIN_TOM_ARCHETYPE_LIVERMORE, 0.8f, NULL);

    /* Set up market context */
    fin_tom_market_context_t context;
    memset(&context, 0, sizeof(context));
    context.current_price = 100.0f;
    context.price_change_pct = 5.0f;  /* Up 5% */
    context.volatility = 0.2f;
    context.volume_ratio = 1.5f;
    context.market_fear_greed = 0.7f;  /* Greedy */
    context.is_trending = true;
    context.is_ranging = false;

    fin_tom_action_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    int result = financial_tom_bridge_predict_action(
        g_tom_bridge, "predictor", &context, &prediction);
    ck_assert_int_eq(result, 0);

    /* Verify prediction is valid */
    ck_assert(prediction.action >= FIN_TOM_ACTION_STRONG_BUY &&
              prediction.action < FIN_TOM_ACTION_COUNT);
    ck_assert(prediction.probability >= 0.0f && prediction.probability <= 1.0f);
    ck_assert(prediction.conviction >= 0.0f && prediction.conviction <= 1.0f);
}
END_TEST

START_TEST(test_tom_predict_action_null)
{
    fin_tom_action_prediction_t prediction;
    fin_tom_market_context_t context;
    memset(&context, 0, sizeof(context));

    ck_assert_int_ne(financial_tom_bridge_predict_action(NULL, "id", &context, &prediction), 0);
    ck_assert_int_ne(financial_tom_bridge_predict_action(g_tom_bridge, NULL, &context, &prediction), 0);
    ck_assert_int_ne(financial_tom_bridge_predict_action(g_tom_bridge, "id", NULL, &prediction), 0);
}
END_TEST

/* ============================================================================
 * ToM Bridge: False Belief Detection Tests
 * ============================================================================ */

START_TEST(test_tom_detect_false_belief)
{
    /* Create investor with specific beliefs */
    financial_tom_bridge_model_investor(
        g_tom_bridge, "biased_investor", FIN_TOM_ARCHETYPE_FISHER, 0.9f, NULL);

    /* Set beliefs that may be false given market context */
    float beliefs[4] = {0.9f, 0.8f, 0.7f, 0.6f};  /* Very bullish beliefs */
    financial_tom_bridge_update_beliefs(g_tom_bridge, "biased_investor", beliefs, 4);

    /* Market context showing bearish reality */
    fin_tom_market_context_t context;
    memset(&context, 0, sizeof(context));
    context.current_price = 80.0f;
    context.price_change_pct = -15.0f;  /* Down 15% */
    context.volatility = 0.4f;
    context.market_fear_greed = 0.2f;  /* Fearful */
    context.is_trending = true;

    fin_tom_false_belief_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = financial_tom_bridge_detect_false_belief(
        g_tom_bridge, "biased_investor", &context, &result);
    ck_assert_int_eq(ret, 0);

    /* Should detect some false beliefs (mismatch between bullish beliefs and bearish market) */
    ck_assert_uint_ge(result.total_analyzed, 1);

    financial_tom_bridge_free_false_belief_result(&result);
}
END_TEST

START_TEST(test_tom_detect_false_beliefs_all)
{
    /* Create multiple investors */
    financial_tom_bridge_model_investor(
        g_tom_bridge, "inv1", FIN_TOM_ARCHETYPE_GRAHAM, 0.7f, NULL);
    financial_tom_bridge_model_investor(
        g_tom_bridge, "inv2", FIN_TOM_ARCHETYPE_SOROS, 0.8f, NULL);

    fin_tom_market_context_t context;
    memset(&context, 0, sizeof(context));
    context.current_price = 100.0f;
    context.volatility = 0.25f;

    fin_tom_false_belief_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = financial_tom_bridge_detect_false_beliefs_all(
        g_tom_bridge, &context, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_uint_ge(result.total_analyzed, 2);

    financial_tom_bridge_free_false_belief_result(&result);
}
END_TEST

START_TEST(test_tom_false_belief_null)
{
    fin_tom_market_context_t context;
    fin_tom_false_belief_result_t result;
    memset(&context, 0, sizeof(context));

    ck_assert_int_ne(financial_tom_bridge_detect_false_belief(NULL, "id", &context, &result), 0);
    ck_assert_int_ne(financial_tom_bridge_detect_false_belief(g_tom_bridge, NULL, &context, &result), 0);
}
END_TEST

/* ============================================================================
 * ToM Bridge: Sentiment Aggregation Tests
 * ============================================================================ */

START_TEST(test_tom_aggregate_sentiment)
{
    /* Create multiple investors with different states */
    financial_tom_bridge_model_investor(
        g_tom_bridge, "bull1", FIN_TOM_ARCHETYPE_BUFFETT, 0.9f, NULL);
    financial_tom_bridge_model_investor(
        g_tom_bridge, "bear1", FIN_TOM_ARCHETYPE_TEMPLETON, 0.8f, NULL);

    /* Make one bullish, one bearish */
    float bull_beliefs[4] = {0.8f, 0.7f, 0.6f, 0.5f};
    float bear_beliefs[4] = {-0.6f, -0.5f, -0.4f, -0.3f};
    financial_tom_bridge_update_beliefs(g_tom_bridge, "bull1", bull_beliefs, 4);
    financial_tom_bridge_update_beliefs(g_tom_bridge, "bear1", bear_beliefs, 4);

    fin_tom_sentiment_t sentiment;
    memset(&sentiment, 0, sizeof(sentiment));

    int result = financial_tom_bridge_aggregate_sentiment(
        g_tom_bridge, NULL, 0, &sentiment);
    ck_assert_int_eq(result, 0);

    /* Verify sentiment structure */
    ck_assert(sentiment.bullish_pct >= 0.0f && sentiment.bullish_pct <= 1.0f);
    ck_assert(sentiment.bearish_pct >= 0.0f && sentiment.bearish_pct <= 1.0f);
    ck_assert_uint_eq(sentiment.models_included, 2);
}
END_TEST

START_TEST(test_tom_sentiment_by_archetype)
{
    /* Create several investors of same archetype */
    financial_tom_bridge_model_investor(
        g_tom_bridge, "simons1", FIN_TOM_ARCHETYPE_SIMONS, 0.85f, NULL);
    financial_tom_bridge_model_investor(
        g_tom_bridge, "simons2", FIN_TOM_ARCHETYPE_SIMONS, 0.9f, NULL);
    financial_tom_bridge_model_investor(
        g_tom_bridge, "other", FIN_TOM_ARCHETYPE_LYNCH, 0.7f, NULL);

    fin_tom_sentiment_t sentiment;
    memset(&sentiment, 0, sizeof(sentiment));

    int result = financial_tom_bridge_sentiment_by_archetype(
        g_tom_bridge, FIN_TOM_ARCHETYPE_SIMONS, &sentiment);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_eq(sentiment.models_included, 2);  /* Only Simons archetypes */
}
END_TEST

START_TEST(test_tom_sentiment_null)
{
    ck_assert_int_ne(financial_tom_bridge_aggregate_sentiment(NULL, NULL, 0, NULL), 0);
    ck_assert_int_ne(financial_tom_bridge_aggregate_sentiment(g_tom_bridge, NULL, 0, NULL), 0);
}
END_TEST

/* ============================================================================
 * ToM Bridge: Statistics Tests
 * ============================================================================ */

START_TEST(test_tom_get_stats)
{
    fin_tom_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = financial_tom_bridge_get_stats(g_tom_bridge, &stats);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_tom_reset_stats)
{
    /* Create some models to generate stats */
    financial_tom_bridge_model_investor(
        g_tom_bridge, "stat_test", FIN_TOM_ARCHETYPE_MUNGER, 0.5f, NULL);

    financial_tom_bridge_reset_stats(g_tom_bridge);

    fin_tom_bridge_stats_t stats;
    int result = financial_tom_bridge_get_stats(g_tom_bridge, &stats);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_tom_stats_null)
{
    ck_assert_int_ne(financial_tom_bridge_get_stats(NULL, NULL), 0);
    ck_assert_int_ne(financial_tom_bridge_get_stats(g_tom_bridge, NULL), 0);
}
END_TEST

/* ============================================================================
 * ToM Bridge: Utility Tests
 * ============================================================================ */

START_TEST(test_tom_archetype_names)
{
    ck_assert_str_ne(fin_tom_archetype_name(FIN_TOM_ARCHETYPE_GRAHAM), "");
    ck_assert_str_ne(fin_tom_archetype_name(FIN_TOM_ARCHETYPE_BUFFETT), "");
    ck_assert_str_ne(fin_tom_archetype_name(FIN_TOM_ARCHETYPE_SOROS), "");
    ck_assert_str_ne(fin_tom_archetype_name(FIN_TOM_ARCHETYPE_SIMONS), "");
}
END_TEST

START_TEST(test_tom_emotion_names)
{
    ck_assert_str_ne(fin_tom_emotion_name(FIN_TOM_EMOTION_NEUTRAL), "");
    ck_assert_str_ne(fin_tom_emotion_name(FIN_TOM_EMOTION_FEAR), "");
    ck_assert_str_ne(fin_tom_emotion_name(FIN_TOM_EMOTION_GREED), "");
    ck_assert_str_ne(fin_tom_emotion_name(FIN_TOM_EMOTION_PANIC), "");
}
END_TEST

START_TEST(test_tom_action_names)
{
    ck_assert_str_ne(fin_tom_action_name(FIN_TOM_ACTION_STRONG_BUY), "");
    ck_assert_str_ne(fin_tom_action_name(FIN_TOM_ACTION_HOLD), "");
    ck_assert_str_ne(fin_tom_action_name(FIN_TOM_ACTION_SELL), "");
}
END_TEST

START_TEST(test_tom_version)
{
    const char* version = financial_tom_bridge_version();
    ck_assert_ptr_nonnull(version);
    ck_assert_str_eq(version, FINANCIAL_TOM_BRIDGE_VERSION);
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* financial_world_model_suite(void)
{
    Suite* s = suite_create("Financial World Model Bridge");

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_world_model_create_default);
    tcase_add_test(tc_lifecycle, test_world_model_create_with_config);
    tcase_add_test(tc_lifecycle, test_world_model_destroy_null);
    tcase_add_checked_fixture(tc_lifecycle, setup_world_model, teardown_world_model);
    tcase_add_test(tc_lifecycle, test_world_model_reset);
    tcase_add_test(tc_lifecycle, test_world_model_default_config);
    tcase_add_test(tc_lifecycle, test_world_model_default_config_null);
    suite_add_tcase(s, tc_lifecycle);

    /* Subsystem setter tests */
    TCase* tc_setters = tcase_create("Subsystem Setters");
    tcase_add_checked_fixture(tc_setters, setup_world_model, teardown_world_model);
    tcase_add_test(tc_setters, test_world_model_set_immune);
    tcase_add_test(tc_setters, test_world_model_set_bbb);
    tcase_add_test(tc_setters, test_world_model_set_health_agent);
    tcase_add_test(tc_setters, test_world_model_set_kg_wiring);
    tcase_add_test(tc_setters, test_world_model_set_logger);
    tcase_add_test(tc_setters, test_world_model_subsystem_null_bridge);
    suite_add_tcase(s, tc_setters);

    /* State management tests */
    TCase* tc_state = tcase_create("State Management");
    tcase_add_checked_fixture(tc_state, setup_world_model, teardown_world_model);
    tcase_add_test(tc_state, test_world_model_set_state);
    tcase_add_test(tc_state, test_world_model_get_state);
    tcase_add_test(tc_state, test_world_model_update_asset);
    tcase_add_test(tc_state, test_world_model_set_regime);
    tcase_add_test(tc_state, test_world_model_state_null_params);
    suite_add_tcase(s, tc_state);

    /* Prediction tests */
    TCase* tc_predict = tcase_create("Prediction");
    tcase_add_checked_fixture(tc_predict, setup_world_model, teardown_world_model);
    tcase_add_test(tc_predict, test_world_model_predict_forward);
    tcase_add_test(tc_predict, test_world_model_predict_null_params);
    suite_add_tcase(s, tc_predict);

    /* Counterfactual tests */
    TCase* tc_counterfactual = tcase_create("Counterfactual");
    tcase_add_checked_fixture(tc_counterfactual, setup_world_model, teardown_world_model);
    tcase_add_test(tc_counterfactual, test_world_model_counterfactual);
    tcase_add_test(tc_counterfactual, test_world_model_counterfactual_null);
    suite_add_tcase(s, tc_counterfactual);

    /* Policy rollout tests */
    TCase* tc_rollout = tcase_create("Policy Rollout");
    tcase_add_checked_fixture(tc_rollout, setup_world_model, teardown_world_model);
    tcase_add_test(tc_rollout, test_world_model_rollout_policy);
    tcase_add_test(tc_rollout, test_world_model_rollout_null);
    suite_add_tcase(s, tc_rollout);

    /* Statistics tests */
    TCase* tc_stats = tcase_create("Statistics");
    tcase_add_checked_fixture(tc_stats, setup_world_model, teardown_world_model);
    tcase_add_test(tc_stats, test_world_model_get_stats);
    tcase_add_test(tc_stats, test_world_model_reset_stats);
    tcase_add_test(tc_stats, test_world_model_stats_null);
    suite_add_tcase(s, tc_stats);

    /* Utility tests */
    TCase* tc_util = tcase_create("Utilities");
    tcase_add_test(tc_util, test_world_model_regime_names);
    tcase_add_test(tc_util, test_world_model_version);
    suite_add_tcase(s, tc_util);

    return s;
}

Suite* financial_tom_suite(void)
{
    Suite* s = suite_create("Financial Theory of Mind Bridge");

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_tom_create_default);
    tcase_add_test(tc_lifecycle, test_tom_create_with_config);
    tcase_add_test(tc_lifecycle, test_tom_destroy_null);
    tcase_add_checked_fixture(tc_lifecycle, setup_tom, teardown_tom);
    tcase_add_test(tc_lifecycle, test_tom_reset);
    tcase_add_test(tc_lifecycle, test_tom_default_config);
    tcase_add_test(tc_lifecycle, test_tom_default_config_null);
    suite_add_tcase(s, tc_lifecycle);

    /* Subsystem setter tests */
    TCase* tc_setters = tcase_create("Subsystem Setters");
    tcase_add_checked_fixture(tc_setters, setup_tom, teardown_tom);
    tcase_add_test(tc_setters, test_tom_set_immune);
    tcase_add_test(tc_setters, test_tom_set_bbb);
    tcase_add_test(tc_setters, test_tom_set_health_agent);
    tcase_add_test(tc_setters, test_tom_set_kg_wiring);
    tcase_add_test(tc_setters, test_tom_set_logger);
    tcase_add_test(tc_setters, test_tom_subsystem_null_bridge);
    suite_add_tcase(s, tc_setters);

    /* Investor model tests */
    TCase* tc_model = tcase_create("Investor Modeling");
    tcase_add_checked_fixture(tc_model, setup_tom, teardown_tom);
    tcase_add_test(tc_model, test_tom_model_investor);
    tcase_add_test(tc_model, test_tom_model_multiple_archetypes);
    tcase_add_test(tc_model, test_tom_get_model);
    tcase_add_test(tc_model, test_tom_update_beliefs);
    tcase_add_test(tc_model, test_tom_update_emotion);
    tcase_add_test(tc_model, test_tom_remove_model);
    tcase_add_test(tc_model, test_tom_model_null_params);
    suite_add_tcase(s, tc_model);

    /* Action prediction tests */
    TCase* tc_predict = tcase_create("Action Prediction");
    tcase_add_checked_fixture(tc_predict, setup_tom, teardown_tom);
    tcase_add_test(tc_predict, test_tom_predict_action);
    tcase_add_test(tc_predict, test_tom_predict_action_null);
    suite_add_tcase(s, tc_predict);

    /* False belief detection tests */
    TCase* tc_false_belief = tcase_create("False Belief Detection");
    tcase_add_checked_fixture(tc_false_belief, setup_tom, teardown_tom);
    tcase_add_test(tc_false_belief, test_tom_detect_false_belief);
    tcase_add_test(tc_false_belief, test_tom_detect_false_beliefs_all);
    tcase_add_test(tc_false_belief, test_tom_false_belief_null);
    suite_add_tcase(s, tc_false_belief);

    /* Sentiment aggregation tests */
    TCase* tc_sentiment = tcase_create("Sentiment Aggregation");
    tcase_add_checked_fixture(tc_sentiment, setup_tom, teardown_tom);
    tcase_add_test(tc_sentiment, test_tom_aggregate_sentiment);
    tcase_add_test(tc_sentiment, test_tom_sentiment_by_archetype);
    tcase_add_test(tc_sentiment, test_tom_sentiment_null);
    suite_add_tcase(s, tc_sentiment);

    /* Statistics tests */
    TCase* tc_stats = tcase_create("Statistics");
    tcase_add_checked_fixture(tc_stats, setup_tom, teardown_tom);
    tcase_add_test(tc_stats, test_tom_get_stats);
    tcase_add_test(tc_stats, test_tom_reset_stats);
    tcase_add_test(tc_stats, test_tom_stats_null);
    suite_add_tcase(s, tc_stats);

    /* Utility tests */
    TCase* tc_util = tcase_create("Utilities");
    tcase_add_test(tc_util, test_tom_archetype_names);
    tcase_add_test(tc_util, test_tom_emotion_names);
    tcase_add_test(tc_util, test_tom_action_names);
    tcase_add_test(tc_util, test_tom_version);
    suite_add_tcase(s, tc_util);

    return s;
}

int main(void)
{
    int number_failed = 0;
    SRunner* sr = srunner_create(financial_world_model_suite());
    srunner_add_suite(sr, financial_tom_suite());

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
