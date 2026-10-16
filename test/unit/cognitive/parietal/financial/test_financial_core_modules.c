/**
 * @file test_financial_core_modules.c
 * @brief Comprehensive unit tests for NIMCP financial core modules
 *
 * WHAT: Test suite covering 5 core financial modules:
 *       1. nimcp_financial_investment - Portfolio management, risk, optimization
 *       2. nimcp_financial_market - GARCH, regime detection, sentiment
 *       3. nimcp_financial_bridge - 14 subsystem setters, validation pipeline
 *       4. nimcp_financial_neural_bridge - SNN encoding, STDP, LNN prediction
 *       5. nimcp_financial_investor_archetype - 10 archetypes, heuristics, decisions
 *
 * WHY:  Verify correct behavior of all financial module lifecycle, configuration,
 *       setter functions, core operations, fuzzy integration, and error handling
 *
 * HOW:  Unit tests using Check framework with setup/teardown fixtures,
 *       covering ~90 tests total (~18 per module)
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "cognitive/parietal/nimcp_financial_investment.h"
#include "cognitive/parietal/nimcp_financial_market.h"
#include "cognitive/parietal/nimcp_financial_bridge.h"
#include "cognitive/parietal/nimcp_financial_neural_bridge.h"
#include "cognitive/parietal/nimcp_financial_investor_archetype.h"

/* ============================================================================
 * Module 1: Financial Investment - Test Fixtures
 * ============================================================================ */

static financial_investment_eng_t* g_investment = NULL;

static void setup_investment(void)
{
    g_investment = financial_investment_create();
    ck_assert_ptr_nonnull(g_investment);
}

static void teardown_investment(void)
{
    if (g_investment) {
        financial_investment_destroy(g_investment);
        g_investment = NULL;
    }
}

/* ============================================================================
 * Module 1: Financial Investment - Lifecycle Tests
 * ============================================================================ */

START_TEST(test_investment_create_default)
{
    financial_investment_eng_t* fin = financial_investment_create();
    ck_assert_ptr_nonnull(fin);
    financial_investment_destroy(fin);
}
END_TEST

START_TEST(test_investment_create_custom)
{
    fin_config_t config = financial_investment_default_config();
    config.risk_free_rate = 0.05f;
    config.max_iterations = 500;
    config.enable_fuzzy_logic = true;

    financial_investment_eng_t* fin = financial_investment_create_custom(&config);
    ck_assert_ptr_nonnull(fin);
    financial_investment_destroy(fin);
}
END_TEST

START_TEST(test_investment_create_null_config)
{
    financial_investment_eng_t* fin = financial_investment_create_custom(NULL);
    /* Implementation may return NULL for NULL config - that's acceptable */
    if (fin != NULL) {
        financial_investment_destroy(fin);
    }
    /* Test passes - verifying no crash on NULL config */
}
END_TEST

START_TEST(test_investment_destroy_null)
{
    /* Should not crash */
    financial_investment_destroy(NULL);
}
END_TEST

START_TEST(test_investment_default_config)
{
    fin_config_t config = financial_investment_default_config();
    ck_assert(config.risk_free_rate >= 0.0f);
    ck_assert(config.max_iterations > 0);
    ck_assert(config.monte_carlo_paths > 0);
    ck_assert(config.convergence_tolerance > 0.0f);
}
END_TEST

/* ============================================================================
 * Module 1: Financial Investment - Setter Tests
 * ============================================================================ */

START_TEST(test_investment_set_immune)
{
    int result = financial_investment_set_instance_immune(g_investment, NULL);
    /* Setting NULL should be valid (disconnect) */
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_investment_set_immune_null_engine)
{
    int result = financial_investment_set_instance_immune(NULL, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_investment_set_bbb)
{
    int result = financial_investment_set_instance_bbb(g_investment, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_investment_set_kg_wiring)
{
    int result = financial_investment_set_kg_wiring(g_investment, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_investment_set_health_agent)
{
    int result = financial_investment_set_health_agent(g_investment, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_investment_set_logger)
{
    int result = financial_investment_set_logger(g_investment, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_investment_set_bio_async)
{
    int result = financial_investment_set_bio_async(g_investment, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_investment_enable_immune_validation)
{
    int result = financial_investment_enable_immune_validation(g_investment, true);
    ck_assert_int_eq(result, 0);
    result = financial_investment_enable_immune_validation(g_investment, false);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_investment_enable_bbb_validation)
{
    int result = financial_investment_enable_bbb_validation(g_investment, true);
    ck_assert_int_eq(result, 0);
    result = financial_investment_enable_bbb_validation(g_investment, false);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* ============================================================================
 * Module 1: Financial Investment - Portfolio Tests
 * ============================================================================ */

START_TEST(test_investment_portfolio_create)
{
    fin_portfolio_t portfolio;
    memset(&portfolio, 0, sizeof(portfolio));

    int result = financial_investment_portfolio_create(g_investment, &portfolio);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_eq(portfolio.asset_count, 0);
}
END_TEST

START_TEST(test_investment_portfolio_add_asset)
{
    fin_portfolio_t portfolio;
    memset(&portfolio, 0, sizeof(portfolio));
    financial_investment_portfolio_create(g_investment, &portfolio);

    fin_asset_t asset = {
        .asset_id = 1,
        .type = FIN_ASSET_EQUITY,
        .current_price = 100.0f,
        .expected_return = 0.08f,
        .volatility = 0.20f,
        .beta = 1.0f
    };
    strcpy(asset.symbol, "TEST");

    int result = financial_investment_portfolio_add_asset(g_investment, &portfolio, &asset, 0.5f);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_eq(portfolio.asset_count, 1);
}
END_TEST

START_TEST(test_investment_portfolio_null_params)
{
    fin_portfolio_t portfolio;
    fin_asset_t asset = {0};

    int result = financial_investment_portfolio_create(NULL, &portfolio);
    ck_assert_int_ne(result, 0);

    result = financial_investment_portfolio_create(g_investment, NULL);
    ck_assert_int_ne(result, 0);

    result = financial_investment_portfolio_add_asset(g_investment, NULL, &asset, 0.5f);
    ck_assert_int_ne(result, 0);
}
END_TEST

/* ============================================================================
 * Module 1: Financial Investment - Risk Assessment Tests
 * ============================================================================ */

START_TEST(test_investment_assess_risk)
{
    fin_portfolio_t portfolio;
    memset(&portfolio, 0, sizeof(portfolio));
    financial_investment_portfolio_create(g_investment, &portfolio);

    fin_asset_t asset = {
        .asset_id = 1,
        .type = FIN_ASSET_EQUITY,
        .current_price = 100.0f,
        .expected_return = 0.08f,
        .volatility = 0.20f,
        .beta = 1.0f
    };
    financial_investment_portfolio_add_asset(g_investment, &portfolio, &asset, 1.0f);

    float correlation_matrix[1] = {1.0f};
    float returns_history[100];
    for (int i = 0; i < 100; i++) {
        returns_history[i] = 0.001f * (float)(i % 10 - 5);
    }

    fin_risk_metrics_t metrics;
    int result = financial_investment_assess_risk(g_investment, &portfolio,
                                                   correlation_matrix, returns_history,
                                                   100, &metrics);
    ck_assert_int_eq(result, 0);
    ck_assert(isfinite(metrics.var_95));
    ck_assert(isfinite(metrics.sharpe_ratio));
}
END_TEST

START_TEST(test_investment_compute_var)
{
    float returns[100];
    for (int i = 0; i < 100; i++) {
        returns[i] = 0.01f * (float)(i % 20 - 10);
    }

    float var = financial_investment_compute_var(g_investment, returns, 100, 0.95f);
    ck_assert(isfinite(var));
    /* VaR can be returned as positive (loss amount) or negative (return) depending on convention */
}
END_TEST

START_TEST(test_investment_max_drawdown)
{
    float prices[100];
    for (int i = 0; i < 50; i++) prices[i] = 100.0f + (float)i;
    for (int i = 50; i < 100; i++) prices[i] = 150.0f - (float)(i - 50) * 0.5f;

    float mdd = financial_investment_max_drawdown(prices, 100);
    ck_assert(isfinite(mdd));
    ck_assert(mdd >= 0.0f);
}
END_TEST

/* ============================================================================
 * Module 1: Financial Investment - Stats Tests
 * ============================================================================ */

START_TEST(test_investment_get_stats)
{
    fin_stats_t stats;
    int result = financial_investment_get_stats(g_investment, &stats);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_investment_reset_stats)
{
    financial_investment_reset_stats(g_investment);
    fin_stats_t stats;
    financial_investment_get_stats(g_investment, &stats);
    ck_assert_uint_eq(stats.portfolio_analyses, 0);
}
END_TEST

START_TEST(test_investment_set_inflammation)
{
    int result = financial_investment_set_inflammation(g_investment, 0.5f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_investment_set_fatigue)
{
    int result = financial_investment_set_fatigue(g_investment, 0.3f);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* ============================================================================
 * Module 2: Financial Market - Test Fixtures
 * ============================================================================ */

static financial_market_eng_t* g_market = NULL;

static void setup_market(void)
{
    g_market = financial_market_create();
    ck_assert_ptr_nonnull(g_market);
}

static void teardown_market(void)
{
    if (g_market) {
        financial_market_destroy(g_market);
        g_market = NULL;
    }
}

/* ============================================================================
 * Module 2: Financial Market - Lifecycle Tests
 * ============================================================================ */

START_TEST(test_market_create_default)
{
    financial_market_eng_t* mkt = financial_market_create();
    ck_assert_ptr_nonnull(mkt);
    financial_market_destroy(mkt);
}
END_TEST

START_TEST(test_market_create_custom)
{
    fin_market_config_t config = financial_market_default_config();
    config.enable_regime_detection = true;
    config.enable_fuzzy_logic = true;
    config.monte_carlo_default_paths = 5000;

    financial_market_eng_t* mkt = financial_market_create_custom(&config);
    ck_assert_ptr_nonnull(mkt);
    financial_market_destroy(mkt);
}
END_TEST

START_TEST(test_market_destroy_null)
{
    financial_market_destroy(NULL);
}
END_TEST

START_TEST(test_market_default_config)
{
    fin_market_config_t config = financial_market_default_config();
    ck_assert(config.garch_max_iterations > 0);
    ck_assert(config.monte_carlo_default_paths > 0);
}
END_TEST

/* ============================================================================
 * Module 2: Financial Market - Setter Tests
 * ============================================================================ */

START_TEST(test_market_set_immune)
{
    financial_market_eng_set_immune(g_market, NULL);
    /* No return value to check, just verify no crash */
}
END_TEST

START_TEST(test_market_set_bbb)
{
    financial_market_eng_set_bbb(g_market, NULL);
}
END_TEST

START_TEST(test_market_enable_bbb_validation)
{
    financial_market_eng_enable_bbb_validation(g_market, true);
    financial_market_eng_enable_bbb_validation(g_market, false);
}
END_TEST

START_TEST(test_market_enable_immune_validation)
{
    financial_market_eng_enable_immune_validation(g_market, true);
    financial_market_eng_enable_immune_validation(g_market, false);
}
END_TEST

START_TEST(test_market_set_bio_async)
{
    int result = financial_market_set_bio_async(g_market, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_market_set_kg_wiring)
{
    int result = financial_market_set_kg_wiring(g_market, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_market_set_health_agent)
{
    int result = financial_market_set_health_agent(g_market, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_market_set_logger)
{
    int result = financial_market_set_logger(g_market, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* ============================================================================
 * Module 2: Financial Market - GARCH Tests
 * ============================================================================ */

START_TEST(test_market_garch_fit)
{
    float returns[200];
    for (int i = 0; i < 200; i++) {
        returns[i] = 0.01f * sinf((float)i * 0.1f);
    }

    fin_garch_result_t result;
    int ret = financial_market_garch_fit(g_market, returns, 200, 1, 1, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert(isfinite(result.omega));
    ck_assert(isfinite(result.current_variance));
}
END_TEST

START_TEST(test_market_garch_forecast)
{
    fin_garch_result_t garch = {
        .omega = 0.0001f,
        .alpha = {0.1f},
        .beta = {0.8f},
        .p = 1, .q = 1,
        .current_variance = 0.0004f,
        .converged = true
    };

    float forecast = financial_market_garch_forecast(&garch, 5);
    ck_assert(isfinite(forecast));
    ck_assert(forecast > 0.0f);
}
END_TEST

START_TEST(test_market_garch_null_params)
{
    float returns[100] = {0};
    fin_garch_result_t result;

    int ret = financial_market_garch_fit(NULL, returns, 100, 1, 1, &result);
    ck_assert_int_ne(ret, 0);

    ret = financial_market_garch_fit(g_market, NULL, 100, 1, 1, &result);
    ck_assert_int_ne(ret, 0);
}
END_TEST

/* ============================================================================
 * Module 2: Financial Market - Regime Detection Tests
 * ============================================================================ */

START_TEST(test_market_detect_regime)
{
    fin_time_series_t series;
    memset(&series, 0, sizeof(series));
    series.length = 100;
    for (uint32_t i = 0; i < 100; i++) {
        series.prices[i] = 100.0f + (float)i * 0.5f;
        series.volumes[i] = 1000000.0f;
    }

    fin_market_condition_t regime = financial_market_detect_regime(g_market, &series);
    ck_assert(regime >= FIN_MKT_BULL && regime < FIN_MKT_CONDITION_COUNT);
}
END_TEST

START_TEST(test_market_detect_regime_fuzzy)
{
    fin_time_series_t series;
    memset(&series, 0, sizeof(series));
    series.length = 100;
    for (uint32_t i = 0; i < 100; i++) {
        series.prices[i] = 100.0f + (float)i * 0.5f;
    }

    fin_fuzzy_market_condition_t condition;
    int ret = financial_market_detect_regime_fuzzy(g_market, &series, &condition);
    ck_assert_int_eq(ret, 0);
    ck_assert(condition.bull_degree >= 0.0f && condition.bull_degree <= 1.0f);
    ck_assert(condition.bear_degree >= 0.0f && condition.bear_degree <= 1.0f);
}
END_TEST

/* ============================================================================
 * Module 2: Financial Market - Sentiment Tests
 * ============================================================================ */

START_TEST(test_market_analyze_sentiment)
{
    fin_time_series_t price_series, volume_series;
    memset(&price_series, 0, sizeof(price_series));
    memset(&volume_series, 0, sizeof(volume_series));

    price_series.length = 50;
    volume_series.length = 50;
    for (uint32_t i = 0; i < 50; i++) {
        price_series.prices[i] = 100.0f + (float)i;
        volume_series.volumes[i] = 1000000.0f;
    }

    fin_sentiment_t sentiment;
    int ret = financial_market_analyze_sentiment(g_market, &price_series, &volume_series, &sentiment);
    ck_assert_int_eq(ret, 0);
    ck_assert(sentiment.level >= FIN_MKT_SENTIMENT_EXTREME_FEAR &&
              sentiment.level < FIN_MKT_SENTIMENT_COUNT);
}
END_TEST

START_TEST(test_market_analyze_sentiment_fuzzy)
{
    fin_time_series_t price_series, volume_series;
    memset(&price_series, 0, sizeof(price_series));
    memset(&volume_series, 0, sizeof(volume_series));

    price_series.length = 50;
    volume_series.length = 50;
    for (uint32_t i = 0; i < 50; i++) {
        price_series.prices[i] = 100.0f - (float)i * 0.5f;  /* Declining */
        volume_series.volumes[i] = 1500000.0f;  /* High volume */
    }

    fin_fuzzy_sentiment_t sentiment;
    int ret = financial_market_analyze_sentiment_fuzzy(g_market, &price_series,
                                                        &volume_series, &sentiment);
    ck_assert_int_eq(ret, 0);
    ck_assert(sentiment.fear_degree >= 0.0f && sentiment.fear_degree <= 1.0f);
}
END_TEST

/* ============================================================================
 * Module 2: Financial Market - Stats Tests
 * ============================================================================ */

START_TEST(test_market_get_stats)
{
    fin_market_stats_t stats;
    int ret = financial_market_get_stats(g_market, &stats);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_market_reset_stats)
{
    financial_market_reset_stats(g_market);
    fin_market_stats_t stats;
    financial_market_get_stats(g_market, &stats);
    ck_assert_uint_eq(stats.garch_fits, 0);
}
END_TEST

START_TEST(test_market_set_inflammation)
{
    int ret = financial_market_set_inflammation(g_market, 0.4f);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_market_set_fatigue)
{
    int ret = financial_market_set_fatigue(g_market, 0.2f);
    ck_assert_int_eq(ret, 0);
}
END_TEST

/* ============================================================================
 * Module 3: Financial Bridge - Test Fixtures
 * ============================================================================ */

static financial_bridge_t* g_bridge = NULL;

static void setup_bridge(void)
{
    fin_bridge_config_t config = financial_bridge_default_config();
    g_bridge = financial_bridge_create(&config);
    ck_assert_ptr_nonnull(g_bridge);
}

static void teardown_bridge(void)
{
    if (g_bridge) {
        financial_bridge_destroy(g_bridge);
        g_bridge = NULL;
    }
}

/* ============================================================================
 * Module 3: Financial Bridge - Lifecycle Tests
 * ============================================================================ */

START_TEST(test_bridge_create)
{
    fin_bridge_config_t config = financial_bridge_default_config();
    financial_bridge_t* bridge = financial_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);
    financial_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_bridge_create_null_config)
{
    financial_bridge_t* bridge = financial_bridge_create(NULL);
    /* Should handle NULL gracefully */
    if (bridge) {
        financial_bridge_destroy(bridge);
    }
}
END_TEST

START_TEST(test_bridge_destroy_null)
{
    financial_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_bridge_default_config)
{
    fin_bridge_config_t config = financial_bridge_default_config();
    ck_assert(config.validation_timeout_ms > 0);
    ck_assert(config.fuzzy_risk_gate_threshold >= 0.0f);
}
END_TEST

START_TEST(test_bridge_get_state)
{
    fin_bridge_state_t state = financial_bridge_get_state(g_bridge);
    ck_assert(state >= FIN_BRIDGE_STATE_UNINITIALIZED && state <= FIN_BRIDGE_STATE_ERROR);
}
END_TEST

START_TEST(test_bridge_reset)
{
    int ret = financial_bridge_reset(g_bridge);
    ck_assert_int_eq(ret, 0);
}
END_TEST

/* ============================================================================
 * Module 3: Financial Bridge - 14 Subsystem Setter Tests
 * ============================================================================ */

START_TEST(test_bridge_set_immune)
{
    int ret = financial_bridge_set_immune(g_bridge, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_bbb)
{
    int ret = financial_bridge_set_bbb(g_bridge, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_health_agent)
{
    int ret = financial_bridge_set_health_agent(g_bridge, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_kg_wiring)
{
    int ret = financial_bridge_set_kg_wiring(g_bridge, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_logger)
{
    int ret = financial_bridge_set_logger(g_bridge, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_security)
{
    int ret = financial_bridge_set_security(g_bridge, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_ethics)
{
    int ret = financial_bridge_set_ethics(g_bridge, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_lgss)
{
    int ret = financial_bridge_set_lgss(g_bridge, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_cycle)
{
    int ret = financial_bridge_set_cycle(g_bridge, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_bio_router)
{
    int ret = financial_bridge_set_bio_router(g_bridge, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_hypothalamus)
{
    int ret = financial_bridge_set_hypothalamus(g_bridge, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_medulla)
{
    int ret = financial_bridge_set_medulla(g_bridge, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_cerebellum)
{
    int ret = financial_bridge_set_cerebellum(g_bridge, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_fuzzy_bridge)
{
    int ret = financial_bridge_set_fuzzy_bridge(g_bridge, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_setters_null_bridge)
{
    ck_assert_int_ne(financial_bridge_set_immune(NULL, NULL), 0);
    ck_assert_int_ne(financial_bridge_set_bbb(NULL, NULL), 0);
    ck_assert_int_ne(financial_bridge_set_health_agent(NULL, NULL), 0);
}
END_TEST

/* ============================================================================
 * Module 3: Financial Bridge - Validation Pipeline Tests
 * ============================================================================ */

START_TEST(test_bridge_validate_action)
{
    fin_action_t action = {
        .type = FIN_ACTION_BUY,
        .magnitude = 1000.0f,
        .position_weight = 0.05f,
        .leverage_ratio = 1.0f,
        .has_client_consent = true,
        .is_suitable = true
    };
    strcpy(action.symbol, "TEST");

    fin_validation_report_t report;
    int ret = financial_bridge_validate_action(g_bridge, &action, &report);
    ck_assert_int_eq(ret, 0);
    ck_assert(report.result >= FIN_VALIDATION_PASS && report.result <= FIN_VALIDATION_ERROR);
}
END_TEST

START_TEST(test_bridge_lgss_check)
{
    fin_action_t action = {
        .type = FIN_ACTION_SELL,
        .magnitude = 500.0f,
        .leverage_ratio = 1.0f
    };

    fin_validation_result_t result = financial_bridge_lgss_check(g_bridge, &action);
    ck_assert(result >= FIN_VALIDATION_PASS && result <= FIN_VALIDATION_ERROR);
}
END_TEST

START_TEST(test_bridge_fuzzy_score)
{
    fin_action_t action = {
        .type = FIN_ACTION_REBALANCE,
        .magnitude = 10000.0f,
        .position_weight = 0.10f
    };

    float safety_score, risk_score;
    int ret = financial_bridge_fuzzy_score(g_bridge, &action, &safety_score, &risk_score);
    ck_assert_int_eq(ret, 0);
    ck_assert(safety_score >= 0.0f && safety_score <= 1.0f);
    ck_assert(risk_score >= 0.0f && risk_score <= 1.0f);
}
END_TEST

/* ============================================================================
 * Module 3: Financial Bridge - Stats Tests
 * ============================================================================ */

START_TEST(test_bridge_get_stats)
{
    fin_bridge_stats_t stats;
    int ret = financial_bridge_get_stats(g_bridge, &stats);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_reset_stats)
{
    financial_bridge_reset_stats(g_bridge);
    fin_bridge_stats_t stats;
    financial_bridge_get_stats(g_bridge, &stats);
    ck_assert_uint_eq(stats.total_validations, 0);
}
END_TEST

START_TEST(test_bridge_check_health)
{
    int ret = financial_bridge_check_health(g_bridge);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_heartbeat)
{
    int ret = financial_bridge_heartbeat(g_bridge, "test_operation", 0.5f);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_inflammation)
{
    int ret = financial_bridge_set_inflammation(g_bridge, 0.3f);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_bridge_set_fatigue)
{
    int ret = financial_bridge_set_fatigue(g_bridge, 0.4f);
    ck_assert_int_eq(ret, 0);
}
END_TEST

/* ============================================================================
 * Module 4: Financial Neural Bridge - Test Fixtures
 * ============================================================================ */

static financial_neural_bridge_t* g_neural = NULL;

static void setup_neural(void)
{
    fin_neural_config_t config = financial_neural_bridge_default_config();
    g_neural = financial_neural_bridge_create(&config);
    ck_assert_ptr_nonnull(g_neural);
}

static void teardown_neural(void)
{
    if (g_neural) {
        financial_neural_bridge_destroy(g_neural);
        g_neural = NULL;
    }
}

/* ============================================================================
 * Module 4: Financial Neural Bridge - Lifecycle Tests
 * ============================================================================ */

START_TEST(test_neural_create)
{
    fin_neural_config_t config = financial_neural_bridge_default_config();
    financial_neural_bridge_t* neural = financial_neural_bridge_create(&config);
    ck_assert_ptr_nonnull(neural);
    financial_neural_bridge_destroy(neural);
}
END_TEST

START_TEST(test_neural_create_null_config)
{
    financial_neural_bridge_t* neural = financial_neural_bridge_create(NULL);
    if (neural) {
        financial_neural_bridge_destroy(neural);
    }
}
END_TEST

START_TEST(test_neural_destroy_null)
{
    financial_neural_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_neural_default_config)
{
    fin_neural_config_t config = financial_neural_bridge_default_config();
    ck_assert(config.spike_channels > 0);
    ck_assert(config.lnn_state_dim > 0);
    ck_assert(config.stdp_learning_rate > 0.0f);
}
END_TEST

START_TEST(test_neural_get_state)
{
    fin_neural_state_t state = financial_neural_bridge_get_state(g_neural);
    ck_assert(state >= FIN_NEURAL_STATE_UNINITIALIZED && state <= FIN_NEURAL_STATE_ERROR);
}
END_TEST

START_TEST(test_neural_reset)
{
    int ret = financial_neural_bridge_reset(g_neural);
    ck_assert_int_eq(ret, 0);
}
END_TEST

/* ============================================================================
 * Module 4: Financial Neural Bridge - Setter Tests
 * ============================================================================ */

START_TEST(test_neural_set_snn)
{
    int ret = financial_neural_bridge_set_snn(g_neural, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_set_stdp)
{
    int ret = financial_neural_bridge_set_stdp(g_neural, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_set_lnn)
{
    int ret = financial_neural_bridge_set_lnn(g_neural, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_set_plasticity)
{
    int ret = financial_neural_bridge_set_plasticity(g_neural, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_set_quantum)
{
    int ret = financial_neural_bridge_set_quantum(g_neural, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_set_immune)
{
    int ret = financial_neural_bridge_set_immune(g_neural, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_set_health_agent)
{
    int ret = financial_neural_bridge_set_health_agent(g_neural, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_set_logger)
{
    int ret = financial_neural_bridge_set_logger(g_neural, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_set_fuzzy_bridge)
{
    int ret = financial_neural_bridge_set_fuzzy_bridge(g_neural, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_set_kg_wiring)
{
    int ret = financial_neural_bridge_set_kg_wiring(g_neural, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_set_instance_bbb)
{
    int ret = financial_neural_bridge_set_instance_bbb(g_neural, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_enable_bbb_validation)
{
    int ret = financial_neural_bridge_enable_bbb_validation(g_neural, true);
    ck_assert_int_eq(ret, 0);
    ret = financial_neural_bridge_enable_bbb_validation(g_neural, false);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_enable_immune_validation)
{
    int ret = financial_neural_bridge_enable_immune_validation(g_neural, true);
    ck_assert_int_eq(ret, 0);
    ret = financial_neural_bridge_enable_immune_validation(g_neural, false);
    ck_assert_int_eq(ret, 0);
}
END_TEST

/* ============================================================================
 * Module 4: Financial Neural Bridge - SNN Encoding Tests
 * ============================================================================ */

START_TEST(test_neural_encode_market_event)
{
    fin_market_event_t event = {
        .type = FIN_EVENT_PRICE_CHANGE,
        .magnitude = 0.05f,
        .direction = 1.0f,
        .timestamp_us = 1000000
    };

    fin_spike_train_t spikes;
    int ret = financial_neural_bridge_encode_market_event(g_neural, &event, &spikes);
    ck_assert_int_eq(ret, 0);
    ck_assert(spikes.active_channels > 0 || spikes.total_activity >= 0.0f);
}
END_TEST

START_TEST(test_neural_encode_fuzzy_regime)
{
    fin_fuzzy_market_condition_t condition = {
        .bull_degree = 0.7f,
        .bear_degree = 0.2f,
        .sideways_degree = 0.1f,
        .dominant = FIN_MKT_BULL
    };

    fin_spike_train_t spikes;
    int ret = financial_neural_bridge_encode_fuzzy_regime(g_neural, &condition, &spikes);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_decode_spikes)
{
    fin_spike_train_t spikes;
    memset(&spikes, 0, sizeof(spikes));
    spikes.active_channels = 5;
    spikes.total_activity = 0.5f;
    for (int i = 0; i < 5; i++) {
        spikes.spike_rates[i] = 0.1f;
    }

    float signal, confidence;
    int ret = financial_neural_bridge_decode_spikes(g_neural, &spikes, &signal, &confidence);
    ck_assert_int_eq(ret, 0);
    ck_assert(isfinite(signal));
}
END_TEST

/* ============================================================================
 * Module 4: Financial Neural Bridge - STDP Tests
 * ============================================================================ */

START_TEST(test_neural_stdp_reward)
{
    fin_stdp_reward_t reward;
    int ret = financial_neural_bridge_stdp_reward(g_neural, 0.05f, 3600000000ULL, &reward);
    ck_assert_int_eq(ret, 0);
    ck_assert(isfinite(reward.reward_magnitude));
}
END_TEST

START_TEST(test_neural_compute_fuzzy_reward)
{
    fin_stdp_reward_t reward;
    int ret = financial_neural_bridge_compute_fuzzy_reward(g_neural, 0.03f, &reward);
    ck_assert_int_eq(ret, 0);
    ck_assert(reward.fuzzy_profitable_degree >= 0.0f);
    ck_assert(reward.fuzzy_loss_degree >= 0.0f);
}
END_TEST

/* ============================================================================
 * Module 4: Financial Neural Bridge - Stats Tests
 * ============================================================================ */

START_TEST(test_neural_get_stats)
{
    fin_neural_stats_t stats;
    int ret = financial_neural_bridge_get_stats(g_neural, &stats);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_reset_stats)
{
    financial_neural_bridge_reset_stats(g_neural);
    fin_neural_stats_t stats;
    financial_neural_bridge_get_stats(g_neural, &stats);
    ck_assert_uint_eq(stats.events_encoded, 0);
}
END_TEST

START_TEST(test_neural_set_inflammation)
{
    int ret = financial_neural_bridge_set_inflammation(g_neural, 0.25f);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_neural_set_fatigue)
{
    int ret = financial_neural_bridge_set_fatigue(g_neural, 0.15f);
    ck_assert_int_eq(ret, 0);
}
END_TEST

/* ============================================================================
 * Module 5: Investor Archetype - Test Fixtures
 * ============================================================================ */

static financial_investor_archetype_t* g_archetype = NULL;

static void setup_archetype(void)
{
    fin_archetype_config_t config = financial_investor_archetype_default_config();
    g_archetype = financial_investor_archetype_create(&config);
    ck_assert_ptr_nonnull(g_archetype);
}

static void teardown_archetype(void)
{
    if (g_archetype) {
        financial_investor_archetype_destroy(g_archetype);
        g_archetype = NULL;
    }
}

/* ============================================================================
 * Module 5: Investor Archetype - Lifecycle Tests
 * ============================================================================ */

START_TEST(test_archetype_create)
{
    fin_archetype_config_t config = financial_investor_archetype_default_config();
    financial_investor_archetype_t* arch = financial_investor_archetype_create(&config);
    ck_assert_ptr_nonnull(arch);
    financial_investor_archetype_destroy(arch);
}
END_TEST

START_TEST(test_archetype_create_null_config)
{
    financial_investor_archetype_t* arch = financial_investor_archetype_create(NULL);
    if (arch) {
        financial_investor_archetype_destroy(arch);
    }
}
END_TEST

START_TEST(test_archetype_destroy_null)
{
    financial_investor_archetype_destroy(NULL);
}
END_TEST

START_TEST(test_archetype_default_config)
{
    fin_archetype_config_t config = financial_investor_archetype_default_config();
    ck_assert(config.max_blend_archetypes > 0);
    ck_assert(config.mirror_learning_rate >= 0.0f);
}
END_TEST

/* ============================================================================
 * Module 5: Investor Archetype - Setter Tests
 * ============================================================================ */

START_TEST(test_archetype_set_ethics)
{
    int ret = financial_investor_archetype_set_ethics(g_archetype, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_archetype_set_lgss)
{
    int ret = financial_investor_archetype_set_lgss(g_archetype, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_archetype_set_immune)
{
    int ret = financial_investor_archetype_set_immune(g_archetype, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_archetype_set_health_agent)
{
    int ret = financial_investor_archetype_set_health_agent(g_archetype, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_archetype_set_logger)
{
    int ret = financial_investor_archetype_set_logger(g_archetype, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_archetype_set_fuzzy)
{
    int ret = financial_investor_archetype_set_fuzzy(g_archetype, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_archetype_set_bbb)
{
    int ret = financial_investor_archetype_set_bbb(g_archetype, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_archetype_set_kg_wiring)
{
    int ret = financial_investor_archetype_set_kg_wiring(g_archetype, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_archetype_set_bio_async)
{
    int ret = financial_investor_archetype_set_bio_async(g_archetype, NULL);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_archetype_enable_bbb_validation)
{
    int ret = financial_investor_archetype_enable_bbb_validation(g_archetype, true);
    ck_assert_int_eq(ret, 0);
    ret = financial_investor_archetype_enable_bbb_validation(g_archetype, false);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_archetype_enable_immune_validation)
{
    int ret = financial_investor_archetype_enable_immune_validation(g_archetype, true);
    ck_assert_int_eq(ret, 0);
    ret = financial_investor_archetype_enable_immune_validation(g_archetype, false);
    ck_assert_int_eq(ret, 0);
}
END_TEST

/* ============================================================================
 * Module 5: Investor Archetype - Profile Tests
 * ============================================================================ */

START_TEST(test_archetype_get_profile)
{
    fin_archetype_profile_t profile;
    int ret = financial_investor_archetype_get_profile(FIN_ARCH_BUFFETT, &profile);
    ck_assert_int_eq(ret, 0);
    ck_assert_str_ne(profile.name, "");
    ck_assert(profile.heuristic_count > 0);
}
END_TEST

START_TEST(test_archetype_get_profile_all)
{
    for (int i = 0; i < FIN_ARCH_COUNT; i++) {
        fin_archetype_profile_t profile;
        int ret = financial_investor_archetype_get_profile((fin_archetype_id_t)i, &profile);
        ck_assert_int_eq(ret, 0);
        ck_assert(profile.id == (fin_archetype_id_t)i);
    }
}
END_TEST

START_TEST(test_archetype_name)
{
    const char* name = financial_investor_archetype_name(FIN_ARCH_GRAHAM);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_ne(name, "");

    name = financial_investor_archetype_name(FIN_ARCH_SOROS);
    ck_assert_ptr_nonnull(name);
}
END_TEST

START_TEST(test_archetype_get_profile_invalid)
{
    fin_archetype_profile_t profile;
    int ret = financial_investor_archetype_get_profile((fin_archetype_id_t)99, &profile);
    ck_assert_int_ne(ret, 0);
}
END_TEST

/* ============================================================================
 * Module 5: Investor Archetype - Evaluation Tests
 * ============================================================================ */

START_TEST(test_archetype_evaluate)
{
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));
    input.current_price = 100.0f;
    input.intrinsic_value = 120.0f;
    input.book_value = 80.0f;
    input.earnings_per_share = 5.0f;
    input.peg_ratio = 1.2f;
    input.market_share_stability = 0.9f;
    input.pricing_power = 0.8f;

    fin_archetype_decision_t decision;
    int ret = financial_investor_archetype_evaluate(g_archetype, FIN_ARCH_GRAHAM, &input, &decision);
    ck_assert_int_eq(ret, 0);
    ck_assert(decision.decision >= FIN_DECISION_STRONG_BUY &&
              decision.decision <= FIN_DECISION_NO_ACTION);
}
END_TEST

START_TEST(test_archetype_evaluate_heuristic)
{
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));
    input.current_price = 100.0f;
    input.intrinsic_value = 150.0f;

    fin_heuristic_result_t result;
    int ret = financial_investor_archetype_evaluate_heuristic(g_archetype,
                                                               FIN_HEURISTIC_MARGIN_OF_SAFETY,
                                                               &input, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert(result.fuzzy_membership >= 0.0f && result.fuzzy_membership <= 1.0f);
}
END_TEST

START_TEST(test_archetype_evaluate_blend)
{
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));
    input.current_price = 100.0f;
    input.intrinsic_value = 110.0f;
    input.peg_ratio = 1.5f;

    fin_archetype_id_t archetypes[] = {FIN_ARCH_GRAHAM, FIN_ARCH_BUFFETT, FIN_ARCH_LYNCH};
    float weights[] = {0.4f, 0.4f, 0.2f};

    fin_blend_result_t blend;
    int ret = financial_investor_archetype_evaluate_blend(g_archetype, archetypes, weights,
                                                          3, &input, &blend);
    ck_assert_int_eq(ret, 0);
    ck_assert_uint_eq(blend.archetype_count, 3);
}
END_TEST

/* ============================================================================
 * Module 5: Investor Archetype - Adaptive Selection Tests
 * ============================================================================ */

START_TEST(test_archetype_select)
{
    fin_fuzzy_market_condition_t market = {
        .bull_degree = 0.3f,
        .bear_degree = 0.6f,
        .sideways_degree = 0.1f,
        .dominant = FIN_MKT_BEAR
    };

    fin_fuzzy_sentiment_t sentiment = {
        .extreme_fear_degree = 0.4f,
        .fear_degree = 0.4f,
        .neutral_degree = 0.2f,
        .dominant = FIN_MKT_SENTIMENT_FEAR
    };

    fin_archetype_suitability_t suitability;
    int ret = financial_investor_archetype_select(g_archetype, &market, &sentiment, &suitability);
    ck_assert_int_eq(ret, 0);
    ck_assert(suitability.best_archetype >= FIN_ARCH_GRAHAM &&
              suitability.best_archetype < FIN_ARCH_COUNT);
}
END_TEST

/* ============================================================================
 * Module 5: Investor Archetype - Emotional Modulation Tests
 * ============================================================================ */

START_TEST(test_archetype_compute_emotion)
{
    fin_fuzzy_market_condition_t market = {
        .bull_degree = 0.2f,
        .bear_degree = 0.7f,
        .crisis_degree = 0.1f,
        .dominant = FIN_MKT_BEAR
    };

    fin_fuzzy_sentiment_t sentiment = {
        .extreme_fear_degree = 0.5f,
        .fear_degree = 0.3f,
        .dominant = FIN_MKT_SENTIMENT_EXTREME_FEAR
    };

    fin_emotional_state_t emotion;
    int ret = financial_investor_archetype_compute_emotion(g_archetype, &market, &sentiment,
                                                            0.7f, 0.8f, &emotion);
    ck_assert_int_eq(ret, 0);
    ck_assert(emotion.fear_level >= 0.0f);
    ck_assert(emotion.fuzzy_conservatism >= 0.0f);
}
END_TEST

START_TEST(test_archetype_apply_emotion)
{
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));
    input.current_price = 100.0f;
    input.intrinsic_value = 130.0f;

    fin_archetype_decision_t decision;
    financial_investor_archetype_evaluate(g_archetype, FIN_ARCH_GRAHAM, &input, &decision);

    fin_emotional_state_t emotion = {
        .fear_level = 0.6f,
        .stress_level = 0.5f,
        .confidence_level = 0.4f
    };

    int ret = financial_investor_archetype_apply_emotion(g_archetype, &emotion, &decision);
    ck_assert_int_eq(ret, 0);
}
END_TEST

/* ============================================================================
 * Module 5: Investor Archetype - Stats Tests
 * ============================================================================ */

START_TEST(test_archetype_get_stats)
{
    fin_archetype_stats_t stats;
    int ret = financial_investor_archetype_get_stats(g_archetype, &stats);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_archetype_reset_stats)
{
    financial_investor_archetype_reset_stats(g_archetype);
    fin_archetype_stats_t stats;
    financial_investor_archetype_get_stats(g_archetype, &stats);
    ck_assert_uint_eq(stats.total_evaluations, 0);
}
END_TEST

START_TEST(test_archetype_set_inflammation)
{
    int ret = financial_investor_archetype_set_inflammation(g_archetype, 0.35f);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_archetype_set_fatigue)
{
    int ret = financial_investor_archetype_set_fatigue(g_archetype, 0.25f);
    ck_assert_int_eq(ret, 0);
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* financial_investment_suite(void)
{
    Suite* s = suite_create("Financial Investment");

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_investment_create_default);
    tcase_add_test(tc_lifecycle, test_investment_create_custom);
    tcase_add_test(tc_lifecycle, test_investment_create_null_config);
    tcase_add_test(tc_lifecycle, test_investment_destroy_null);
    tcase_add_test(tc_lifecycle, test_investment_default_config);
    suite_add_tcase(s, tc_lifecycle);

    /* Setter tests */
    TCase* tc_setters = tcase_create("Setters");
    tcase_add_checked_fixture(tc_setters, setup_investment, teardown_investment);
    tcase_add_test(tc_setters, test_investment_set_immune);
    tcase_add_test(tc_setters, test_investment_set_immune_null_engine);
    tcase_add_test(tc_setters, test_investment_set_bbb);
    tcase_add_test(tc_setters, test_investment_set_kg_wiring);
    tcase_add_test(tc_setters, test_investment_set_health_agent);
    tcase_add_test(tc_setters, test_investment_set_logger);
    tcase_add_test(tc_setters, test_investment_set_bio_async);
    tcase_add_test(tc_setters, test_investment_enable_immune_validation);
    tcase_add_test(tc_setters, test_investment_enable_bbb_validation);
    suite_add_tcase(s, tc_setters);

    /* Portfolio tests */
    TCase* tc_portfolio = tcase_create("Portfolio");
    tcase_add_checked_fixture(tc_portfolio, setup_investment, teardown_investment);
    tcase_add_test(tc_portfolio, test_investment_portfolio_create);
    tcase_add_test(tc_portfolio, test_investment_portfolio_add_asset);
    tcase_add_test(tc_portfolio, test_investment_portfolio_null_params);
    suite_add_tcase(s, tc_portfolio);

    /* Risk assessment tests */
    TCase* tc_risk = tcase_create("Risk Assessment");
    tcase_add_checked_fixture(tc_risk, setup_investment, teardown_investment);
    tcase_add_test(tc_risk, test_investment_assess_risk);
    tcase_add_test(tc_risk, test_investment_compute_var);
    tcase_add_test(tc_risk, test_investment_max_drawdown);
    suite_add_tcase(s, tc_risk);

    /* Stats tests */
    TCase* tc_stats = tcase_create("Stats");
    tcase_add_checked_fixture(tc_stats, setup_investment, teardown_investment);
    tcase_add_test(tc_stats, test_investment_get_stats);
    tcase_add_test(tc_stats, test_investment_reset_stats);
    tcase_add_test(tc_stats, test_investment_set_inflammation);
    tcase_add_test(tc_stats, test_investment_set_fatigue);
    suite_add_tcase(s, tc_stats);

    return s;
}

Suite* financial_market_suite(void)
{
    Suite* s = suite_create("Financial Market");

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_market_create_default);
    tcase_add_test(tc_lifecycle, test_market_create_custom);
    tcase_add_test(tc_lifecycle, test_market_destroy_null);
    tcase_add_test(tc_lifecycle, test_market_default_config);
    suite_add_tcase(s, tc_lifecycle);

    /* Setter tests */
    TCase* tc_setters = tcase_create("Setters");
    tcase_add_checked_fixture(tc_setters, setup_market, teardown_market);
    tcase_add_test(tc_setters, test_market_set_immune);
    tcase_add_test(tc_setters, test_market_set_bbb);
    tcase_add_test(tc_setters, test_market_enable_bbb_validation);
    tcase_add_test(tc_setters, test_market_enable_immune_validation);
    tcase_add_test(tc_setters, test_market_set_bio_async);
    tcase_add_test(tc_setters, test_market_set_kg_wiring);
    tcase_add_test(tc_setters, test_market_set_health_agent);
    tcase_add_test(tc_setters, test_market_set_logger);
    suite_add_tcase(s, tc_setters);

    /* GARCH tests */
    TCase* tc_garch = tcase_create("GARCH");
    tcase_add_checked_fixture(tc_garch, setup_market, teardown_market);
    tcase_add_test(tc_garch, test_market_garch_fit);
    tcase_add_test(tc_garch, test_market_garch_forecast);
    tcase_add_test(tc_garch, test_market_garch_null_params);
    suite_add_tcase(s, tc_garch);

    /* Regime detection tests */
    TCase* tc_regime = tcase_create("Regime Detection");
    tcase_add_checked_fixture(tc_regime, setup_market, teardown_market);
    tcase_add_test(tc_regime, test_market_detect_regime);
    tcase_add_test(tc_regime, test_market_detect_regime_fuzzy);
    suite_add_tcase(s, tc_regime);

    /* Sentiment tests */
    TCase* tc_sentiment = tcase_create("Sentiment");
    tcase_add_checked_fixture(tc_sentiment, setup_market, teardown_market);
    tcase_add_test(tc_sentiment, test_market_analyze_sentiment);
    tcase_add_test(tc_sentiment, test_market_analyze_sentiment_fuzzy);
    suite_add_tcase(s, tc_sentiment);

    /* Stats tests */
    TCase* tc_stats = tcase_create("Stats");
    tcase_add_checked_fixture(tc_stats, setup_market, teardown_market);
    tcase_add_test(tc_stats, test_market_get_stats);
    tcase_add_test(tc_stats, test_market_reset_stats);
    tcase_add_test(tc_stats, test_market_set_inflammation);
    tcase_add_test(tc_stats, test_market_set_fatigue);
    suite_add_tcase(s, tc_stats);

    return s;
}

Suite* financial_bridge_suite(void)
{
    Suite* s = suite_create("Financial Bridge");

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_bridge_create);
    tcase_add_test(tc_lifecycle, test_bridge_create_null_config);
    tcase_add_test(tc_lifecycle, test_bridge_destroy_null);
    tcase_add_test(tc_lifecycle, test_bridge_default_config);
    tcase_add_checked_fixture(tc_lifecycle, setup_bridge, teardown_bridge);
    tcase_add_test(tc_lifecycle, test_bridge_get_state);
    tcase_add_test(tc_lifecycle, test_bridge_reset);
    suite_add_tcase(s, tc_lifecycle);

    /* Setter tests - all 14 subsystems */
    TCase* tc_setters = tcase_create("Subsystem Setters");
    tcase_add_checked_fixture(tc_setters, setup_bridge, teardown_bridge);
    tcase_add_test(tc_setters, test_bridge_set_immune);
    tcase_add_test(tc_setters, test_bridge_set_bbb);
    tcase_add_test(tc_setters, test_bridge_set_health_agent);
    tcase_add_test(tc_setters, test_bridge_set_kg_wiring);
    tcase_add_test(tc_setters, test_bridge_set_logger);
    tcase_add_test(tc_setters, test_bridge_set_security);
    tcase_add_test(tc_setters, test_bridge_set_ethics);
    tcase_add_test(tc_setters, test_bridge_set_lgss);
    tcase_add_test(tc_setters, test_bridge_set_cycle);
    tcase_add_test(tc_setters, test_bridge_set_bio_router);
    tcase_add_test(tc_setters, test_bridge_set_hypothalamus);
    tcase_add_test(tc_setters, test_bridge_set_medulla);
    tcase_add_test(tc_setters, test_bridge_set_cerebellum);
    tcase_add_test(tc_setters, test_bridge_set_fuzzy_bridge);
    tcase_add_test(tc_setters, test_bridge_setters_null_bridge);
    suite_add_tcase(s, tc_setters);

    /* Validation pipeline tests */
    TCase* tc_validation = tcase_create("Validation Pipeline");
    tcase_add_checked_fixture(tc_validation, setup_bridge, teardown_bridge);
    tcase_add_test(tc_validation, test_bridge_validate_action);
    tcase_add_test(tc_validation, test_bridge_lgss_check);
    tcase_add_test(tc_validation, test_bridge_fuzzy_score);
    suite_add_tcase(s, tc_validation);

    /* Stats tests */
    TCase* tc_stats = tcase_create("Stats");
    tcase_add_checked_fixture(tc_stats, setup_bridge, teardown_bridge);
    tcase_add_test(tc_stats, test_bridge_get_stats);
    tcase_add_test(tc_stats, test_bridge_reset_stats);
    tcase_add_test(tc_stats, test_bridge_check_health);
    tcase_add_test(tc_stats, test_bridge_heartbeat);
    tcase_add_test(tc_stats, test_bridge_set_inflammation);
    tcase_add_test(tc_stats, test_bridge_set_fatigue);
    suite_add_tcase(s, tc_stats);

    return s;
}

Suite* financial_neural_bridge_suite(void)
{
    Suite* s = suite_create("Financial Neural Bridge");

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_neural_create);
    tcase_add_test(tc_lifecycle, test_neural_create_null_config);
    tcase_add_test(tc_lifecycle, test_neural_destroy_null);
    tcase_add_test(tc_lifecycle, test_neural_default_config);
    tcase_add_checked_fixture(tc_lifecycle, setup_neural, teardown_neural);
    tcase_add_test(tc_lifecycle, test_neural_get_state);
    tcase_add_test(tc_lifecycle, test_neural_reset);
    suite_add_tcase(s, tc_lifecycle);

    /* Setter tests */
    TCase* tc_setters = tcase_create("Setters");
    tcase_add_checked_fixture(tc_setters, setup_neural, teardown_neural);
    tcase_add_test(tc_setters, test_neural_set_snn);
    tcase_add_test(tc_setters, test_neural_set_stdp);
    tcase_add_test(tc_setters, test_neural_set_lnn);
    tcase_add_test(tc_setters, test_neural_set_plasticity);
    tcase_add_test(tc_setters, test_neural_set_quantum);
    tcase_add_test(tc_setters, test_neural_set_immune);
    tcase_add_test(tc_setters, test_neural_set_health_agent);
    tcase_add_test(tc_setters, test_neural_set_logger);
    tcase_add_test(tc_setters, test_neural_set_fuzzy_bridge);
    tcase_add_test(tc_setters, test_neural_set_kg_wiring);
    tcase_add_test(tc_setters, test_neural_set_instance_bbb);
    tcase_add_test(tc_setters, test_neural_enable_bbb_validation);
    tcase_add_test(tc_setters, test_neural_enable_immune_validation);
    suite_add_tcase(s, tc_setters);

    /* SNN encoding tests */
    TCase* tc_snn = tcase_create("SNN Encoding");
    tcase_add_checked_fixture(tc_snn, setup_neural, teardown_neural);
    tcase_add_test(tc_snn, test_neural_encode_market_event);
    tcase_add_test(tc_snn, test_neural_encode_fuzzy_regime);
    tcase_add_test(tc_snn, test_neural_decode_spikes);
    suite_add_tcase(s, tc_snn);

    /* STDP tests */
    TCase* tc_stdp = tcase_create("STDP");
    tcase_add_checked_fixture(tc_stdp, setup_neural, teardown_neural);
    tcase_add_test(tc_stdp, test_neural_stdp_reward);
    tcase_add_test(tc_stdp, test_neural_compute_fuzzy_reward);
    suite_add_tcase(s, tc_stdp);

    /* Stats tests */
    TCase* tc_stats = tcase_create("Stats");
    tcase_add_checked_fixture(tc_stats, setup_neural, teardown_neural);
    tcase_add_test(tc_stats, test_neural_get_stats);
    tcase_add_test(tc_stats, test_neural_reset_stats);
    tcase_add_test(tc_stats, test_neural_set_inflammation);
    tcase_add_test(tc_stats, test_neural_set_fatigue);
    suite_add_tcase(s, tc_stats);

    return s;
}

Suite* financial_archetype_suite(void)
{
    Suite* s = suite_create("Financial Investor Archetype");

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_archetype_create);
    tcase_add_test(tc_lifecycle, test_archetype_create_null_config);
    tcase_add_test(tc_lifecycle, test_archetype_destroy_null);
    tcase_add_test(tc_lifecycle, test_archetype_default_config);
    suite_add_tcase(s, tc_lifecycle);

    /* Setter tests */
    TCase* tc_setters = tcase_create("Setters");
    tcase_add_checked_fixture(tc_setters, setup_archetype, teardown_archetype);
    tcase_add_test(tc_setters, test_archetype_set_ethics);
    tcase_add_test(tc_setters, test_archetype_set_lgss);
    tcase_add_test(tc_setters, test_archetype_set_immune);
    tcase_add_test(tc_setters, test_archetype_set_health_agent);
    tcase_add_test(tc_setters, test_archetype_set_logger);
    tcase_add_test(tc_setters, test_archetype_set_fuzzy);
    tcase_add_test(tc_setters, test_archetype_set_bbb);
    tcase_add_test(tc_setters, test_archetype_set_kg_wiring);
    tcase_add_test(tc_setters, test_archetype_set_bio_async);
    tcase_add_test(tc_setters, test_archetype_enable_bbb_validation);
    tcase_add_test(tc_setters, test_archetype_enable_immune_validation);
    suite_add_tcase(s, tc_setters);

    /* Profile tests */
    TCase* tc_profile = tcase_create("Profile");
    tcase_add_checked_fixture(tc_profile, setup_archetype, teardown_archetype);
    tcase_add_test(tc_profile, test_archetype_get_profile);
    tcase_add_test(tc_profile, test_archetype_get_profile_all);
    tcase_add_test(tc_profile, test_archetype_name);
    tcase_add_test(tc_profile, test_archetype_get_profile_invalid);
    suite_add_tcase(s, tc_profile);

    /* Evaluation tests */
    TCase* tc_eval = tcase_create("Evaluation");
    tcase_add_checked_fixture(tc_eval, setup_archetype, teardown_archetype);
    tcase_add_test(tc_eval, test_archetype_evaluate);
    tcase_add_test(tc_eval, test_archetype_evaluate_heuristic);
    tcase_add_test(tc_eval, test_archetype_evaluate_blend);
    suite_add_tcase(s, tc_eval);

    /* Adaptive selection tests */
    TCase* tc_adaptive = tcase_create("Adaptive Selection");
    tcase_add_checked_fixture(tc_adaptive, setup_archetype, teardown_archetype);
    tcase_add_test(tc_adaptive, test_archetype_select);
    suite_add_tcase(s, tc_adaptive);

    /* Emotional modulation tests */
    TCase* tc_emotion = tcase_create("Emotional Modulation");
    tcase_add_checked_fixture(tc_emotion, setup_archetype, teardown_archetype);
    tcase_add_test(tc_emotion, test_archetype_compute_emotion);
    tcase_add_test(tc_emotion, test_archetype_apply_emotion);
    suite_add_tcase(s, tc_emotion);

    /* Stats tests */
    TCase* tc_stats = tcase_create("Stats");
    tcase_add_checked_fixture(tc_stats, setup_archetype, teardown_archetype);
    tcase_add_test(tc_stats, test_archetype_get_stats);
    tcase_add_test(tc_stats, test_archetype_reset_stats);
    tcase_add_test(tc_stats, test_archetype_set_inflammation);
    tcase_add_test(tc_stats, test_archetype_set_fatigue);
    suite_add_tcase(s, tc_stats);

    return s;
}

int main(void)
{
    int number_failed = 0;
    SRunner* sr = srunner_create(financial_investment_suite());

    /* Add all module suites */
    srunner_add_suite(sr, financial_market_suite());
    srunner_add_suite(sr, financial_bridge_suite());
    srunner_add_suite(sr, financial_neural_bridge_suite());
    srunner_add_suite(sr, financial_archetype_suite());

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);

    printf("\n========================================\n");
    printf("Financial Core Modules Test Summary\n");
    printf("========================================\n");
    printf("Total tests run: %d\n", srunner_ntests_run(sr));
    printf("Tests passed: %d\n", srunner_ntests_run(sr) - number_failed);
    printf("Tests failed: %d\n", number_failed);
    printf("========================================\n");

    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
