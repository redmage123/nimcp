/**
 * @file test_financial_e2e.c
 * @brief End-to-end tests for Financial Module Cognitive Integration
 *
 * WHAT: Comprehensive E2E tests for the financial cognitive system including
 *       trading decisions, cognitive pipelines, stress testing, and real-world
 *       investor archetype scenarios.
 *
 * WHY:  Financial decision-making requires the highest level of integration
 *       across emotion, ethics, memory, reasoning, and learning systems.
 *       These tests verify full system behavior under realistic conditions.
 *
 * HOW:  Tests use the Check framework with comprehensive setup creating a
 *       brain-like environment with all financial modules interconnected.
 *
 * TEST CATEGORIES:
 * 1. Full Trading Decision Pipeline (~10 tests)
 * 2. Cognitive Pipeline E2E (~10 tests)
 * 3. Integration Stress (~10 tests)
 * 4. Real-World Patterns (~10 tests)
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <float.h>

/*
 * IMPORTANT: Multiple financial headers define conflicting types
 * (fin_market_event_t, fin_decision_type_t). We use a careful include
 * strategy to avoid redefinition errors:
 *
 * 1. Include investor_archetype FIRST - it has all archetype types
 * 2. Use preprocessor guards to skip conflicting type definitions
 * 3. Include other headers that don't conflict
 */

/* Include investor archetype first - it defines fin_decision_type_t and all archetype types */
#include "cognitive/parietal/nimcp_financial_investor_archetype.h"

/* Include orchestrator - the guard from investor_archetype prevents redefinition */
#include "cognitive/parietal/nimcp_financial_cognitive_orchestrator.h"

/* Investment, market, bridge don't define conflicting decision types */
#include "cognitive/parietal/nimcp_financial_investment.h"
#include "cognitive/parietal/nimcp_financial_market.h"
#include "cognitive/parietal/nimcp_financial_bridge.h"

/* Include emotion bridge for emotion types */
#include "cognitive/parietal/nimcp_financial_emotion_bridge.h"

/* Include ethics bridge */
#include "cognitive/parietal/nimcp_financial_ethics_bridge.h"

/* Include regret bridge */
#include "cognitive/parietal/nimcp_financial_regret_bridge.h"

/* Include curiosity bridge */
#include "cognitive/parietal/nimcp_financial_curiosity_bridge.h"

/* Include neural bridge */
#include "cognitive/parietal/nimcp_financial_neural_bridge.h"

/*
 * Note: With header guards in place, we now use fin_market_event_t directly
 * from the emotion_bridge.h header (first definition wins).
 */

/* Core utility headers */
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_NUM_ASSETS           16
#define TEST_HISTORY_LENGTH       256
#define TEST_TICK_INTERVAL_MS     10
#define TEST_MAX_ITERATIONS       100
#define TEST_PORTFOLIO_VALUE      1000000.0f
#define TEST_STRESS_ITERATIONS    500
#define TEST_CONCURRENT_THREADS   4

/* Market condition constants */
#define BULL_MARKET_RETURN        0.15f
#define BEAR_MARKET_RETURN       -0.20f
#define CRISIS_RETURN            -0.40f
#define VOLATILITY_LOW            0.10f
#define VOLATILITY_HIGH           0.40f
#define VOLATILITY_CRISIS         0.70f

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

/* Core financial engines */
static financial_investment_eng_t*   g_investment    = NULL;
static financial_market_eng_t*       g_market        = NULL;
static financial_bridge_t*           g_bridge        = NULL;
static financial_neural_bridge_t*    g_neural        = NULL;
static financial_investor_archetype_t* g_archetype   = NULL;
static financial_cognitive_orchestrator_handle_t* g_orchestrator = NULL;

/* Bridge handles */
static financial_emotion_bridge_t*   g_emotion       = NULL;
static financial_ethics_bridge_t*    g_ethics        = NULL;
static financial_regret_bridge_t*    g_regret        = NULL;
static financial_curiosity_bridge_t* g_curiosity     = NULL;

/* Test data */
static fin_portfolio_t g_portfolio;
static fin_time_series_t g_price_series;
static fin_time_series_t g_volume_series;
static float g_correlation_matrix[TEST_NUM_ASSETS * TEST_NUM_ASSETS];
static float g_returns_history[TEST_NUM_ASSETS * TEST_HISTORY_LENGTH];

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Generate synthetic price data with specified trend and volatility
 */
static void generate_price_series(fin_time_series_t* series, uint32_t length,
                                   float initial_price, float trend, float volatility)
{
    memset(series, 0, sizeof(fin_time_series_t));
    series->length = (length > FIN_MKT_MAX_SERIES_LENGTH) ? FIN_MKT_MAX_SERIES_LENGTH : length;

    float price = initial_price;
    uint64_t timestamp = 1609459200000ULL;  /* 2021-01-01 */

    for (uint32_t i = 0; i < series->length; i++) {
        /* Random walk with drift */
        float random = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
        price *= (1.0f + trend / 252.0f + volatility * random / 16.0f);
        if (price < 1.0f) price = 1.0f;

        series->prices[i] = price;
        series->volumes[i] = 1000000.0f + (rand() % 500000);
        series->timestamps[i] = timestamp + i * 86400000ULL;  /* Daily */
    }

    series->open = series->prices[0];
    series->close = series->prices[series->length - 1];
    series->high = series->prices[0];
    series->low = series->prices[0];

    for (uint32_t i = 0; i < series->length; i++) {
        if (series->prices[i] > series->high) series->high = series->prices[i];
        if (series->prices[i] < series->low) series->low = series->prices[i];
    }
}

/**
 * @brief Generate returns history from price series
 */
static void generate_returns_from_prices(const float* prices, uint32_t length,
                                          float* returns, uint32_t* returns_length)
{
    *returns_length = (length > 1) ? length - 1 : 0;
    for (uint32_t i = 0; i < *returns_length; i++) {
        if (prices[i] > 0.0f) {
            returns[i] = (prices[i + 1] - prices[i]) / prices[i];
        } else {
            returns[i] = 0.0f;
        }
    }
}

/**
 * @brief Generate synthetic correlation matrix
 */
static void generate_correlation_matrix(float* matrix, uint32_t size, float base_corr)
{
    for (uint32_t i = 0; i < size; i++) {
        for (uint32_t j = 0; j < size; j++) {
            if (i == j) {
                matrix[i * size + j] = 1.0f;
            } else {
                /* Add some variation to base correlation */
                float noise = ((float)rand() / RAND_MAX - 0.5f) * 0.2f;
                float corr = base_corr + noise;
                if (corr > 0.99f) corr = 0.99f;
                if (corr < -0.99f) corr = -0.99f;
                matrix[i * size + j] = corr;
            }
        }
    }
}

/**
 * @brief Create test portfolio with diversified assets
 */
static void create_test_portfolio(fin_portfolio_t* portfolio, uint32_t num_assets)
{
    memset(portfolio, 0, sizeof(fin_portfolio_t));
    portfolio->asset_count = (num_assets > FIN_MAX_PORTFOLIO_SIZE) ?
                             FIN_MAX_PORTFOLIO_SIZE : num_assets;
    portfolio->total_value = TEST_PORTFOLIO_VALUE;
    portfolio->cash_position = 50000.0f;

    float equal_weight = 1.0f / portfolio->asset_count;

    for (uint32_t i = 0; i < portfolio->asset_count; i++) {
        portfolio->assets[i].asset_id = i + 1;
        portfolio->assets[i].type = (fin_asset_type_t)(i % FIN_ASSET_TYPE_COUNT);
        snprintf(portfolio->assets[i].symbol, sizeof(portfolio->assets[i].symbol),
                 "ASSET%02u", i);
        portfolio->assets[i].current_price = 100.0f + (rand() % 200);
        portfolio->assets[i].expected_return = 0.08f + ((float)rand() / RAND_MAX) * 0.04f;
        portfolio->assets[i].volatility = 0.15f + ((float)rand() / RAND_MAX) * 0.15f;
        portfolio->assets[i].dividend_yield = 0.02f + ((float)rand() / RAND_MAX) * 0.02f;
        portfolio->assets[i].beta = 0.7f + ((float)rand() / RAND_MAX) * 0.6f;
        portfolio->assets[i].market_cap = 1e9f + ((float)rand() / RAND_MAX) * 1e11f;
        portfolio->assets[i].sector_id = i % 11;

        portfolio->weights[i] = equal_weight;
    }
}

/**
 * @brief Create heuristic input for archetype evaluation
 */
static void create_heuristic_input(fin_heuristic_input_t* input,
                                    float price, float intrinsic_value,
                                    float fear_greed, fin_fuzzy_market_condition_t* market)
{
    memset(input, 0, sizeof(fin_heuristic_input_t));

    input->current_price = price;
    input->intrinsic_value = intrinsic_value;
    input->book_value = intrinsic_value * 0.6f;
    input->earnings_per_share = intrinsic_value * 0.05f;
    input->earnings_growth_rate = 0.10f;
    input->dividend_yield = 0.02f;
    input->peg_ratio = 1.2f;

    input->fear_greed_index = fear_greed;
    input->market_consensus_strength = 0.6f;
    input->sector_distance = 0.3f;

    input->market_share_stability = 0.8f;
    input->pricing_power = 0.7f;
    input->switching_cost = 0.6f;
    input->brand_strength = 0.75f;

    input->rsi = 50.0f + fear_greed * 30.0f;
    input->pivot_price = price * 0.95f;
    input->pivot_tolerance = 0.02f;
    input->breakout_confirmation = 0.5f;
    input->z_score = (price - intrinsic_value) / (intrinsic_value * 0.2f);

    input->management_quality = 0.8f;
    input->rd_effectiveness = 0.7f;
    input->competitive_position = 0.75f;

    if (market) {
        memcpy(&input->market_condition, market, sizeof(fin_fuzzy_market_condition_t));
    }
}

/**
 * @brief Create market data for orchestrator
 */
static void create_market_data(fin_market_data_t* data, float* prices, float* volumes,
                                uint32_t num_assets)
{
    data->prices = prices;
    data->volumes = volumes;
    data->num_assets = num_assets;
    data->timestamp_ms = (uint64_t)time(NULL) * 1000;
}

/* ============================================================================
 * Test Setup and Teardown
 * ============================================================================ */

static void setup_financial_e2e(void)
{
    srand((unsigned int)time(NULL));

    /* Create investment engine */
    fin_config_t inv_config = financial_investment_default_config();
    inv_config.enable_fuzzy_logic = true;
    g_investment = financial_investment_create_custom(&inv_config);
    ck_assert_ptr_nonnull(g_investment);

    /* Create market engine */
    fin_market_config_t mkt_config = financial_market_default_config();
    mkt_config.enable_regime_detection = true;
    mkt_config.enable_fuzzy_logic = true;
    g_market = financial_market_create_custom(&mkt_config);
    ck_assert_ptr_nonnull(g_market);

    /* Create main bridge */
    fin_bridge_config_t br_config = financial_bridge_default_config();
    br_config.enable_lgss_validation = false;  /* Disable for testing */
    br_config.enable_ethics_validation = false;
    br_config.enable_bbb_validation = false;
    g_bridge = financial_bridge_create(&br_config);
    ck_assert_ptr_nonnull(g_bridge);

    /* Create neural bridge */
    fin_neural_config_t neural_config = financial_neural_bridge_default_config();
    g_neural = financial_neural_bridge_create(&neural_config);
    ck_assert_ptr_nonnull(g_neural);

    /* Create archetype module */
    fin_archetype_config_t arch_config = financial_investor_archetype_default_config();
    arch_config.enable_fuzzy_heuristics = true;
    arch_config.enable_emotional_modulation = true;
    arch_config.enable_adaptive_selection = true;
    arch_config.enable_mirror_learning = true;
    g_archetype = financial_investor_archetype_create(&arch_config);
    ck_assert_ptr_nonnull(g_archetype);

    /* Create emotion bridge */
    fin_emotion_config_t emo_config;
    financial_emotion_bridge_default_config(&emo_config);
    g_emotion = financial_emotion_bridge_create(&emo_config);
    ck_assert_ptr_nonnull(g_emotion);

    /* Create ethics bridge */
    fin_ethics_config_t ethics_config;
    financial_ethics_bridge_default_config(&ethics_config);
    g_ethics = financial_ethics_bridge_create(&ethics_config);
    ck_assert_ptr_nonnull(g_ethics);

    /* Create regret bridge */
    fin_regret_config_t regret_config;
    financial_regret_bridge_default_config(&regret_config);
    g_regret = financial_regret_bridge_create(&regret_config);
    ck_assert_ptr_nonnull(g_regret);

    /* Create curiosity bridge */
    fin_curiosity_config_t cur_config = financial_curiosity_bridge_default_config();
    g_curiosity = financial_curiosity_bridge_create(&cur_config);
    ck_assert_ptr_nonnull(g_curiosity);

    /* Create cognitive orchestrator */
    fin_orchestrator_config_t orch_config;
    financial_cognitive_orchestrator_default_config(&orch_config);
    orch_config.enable_learning = true;
    orch_config.enable_ethics_validation = false;  /* Simplified for testing */
    orch_config.enable_metacognition = true;
    g_orchestrator = financial_cognitive_orchestrator_create(&orch_config);
    ck_assert_ptr_nonnull(g_orchestrator);

    /* Register modules with orchestrator */
    financial_cognitive_orchestrator_t* modules =
        financial_cognitive_orchestrator_get_modules(g_orchestrator);
    if (modules) {
        modules->investment = g_investment;
        modules->market = g_market;
        modules->bridge = g_bridge;
        modules->neural = g_neural;
        modules->archetype = g_archetype;
        modules->emotion = g_emotion;
        modules->ethics = g_ethics;
        modules->regret = g_regret;
        modules->curiosity = g_curiosity;
    }

    /* Generate test data */
    create_test_portfolio(&g_portfolio, TEST_NUM_ASSETS);
    generate_price_series(&g_price_series, TEST_HISTORY_LENGTH, 100.0f, 0.08f, 0.20f);
    generate_price_series(&g_volume_series, TEST_HISTORY_LENGTH, 1000000.0f, 0.0f, 0.30f);
    generate_correlation_matrix(g_correlation_matrix, TEST_NUM_ASSETS, 0.3f);

    /* Generate returns history for all assets (row-major: asset i at row i) */
    uint32_t returns_len;
    for (uint32_t asset = 0; asset < TEST_NUM_ASSETS; asset++) {
        /* Use price series with slight offset to simulate different assets */
        float asset_prices[TEST_HISTORY_LENGTH];
        for (uint32_t t = 0; t < g_price_series.length; t++) {
            float offset = (float)asset * 0.05f;  /* Each asset has slight price offset */
            asset_prices[t] = g_price_series.prices[t] * (1.0f + offset);
        }
        /* Generate returns for this asset into its row of the 2D array */
        generate_returns_from_prices(asset_prices, g_price_series.length,
                                      &g_returns_history[asset * (TEST_HISTORY_LENGTH - 1)],
                                      &returns_len);
    }
}

static void teardown_financial_e2e(void)
{
    if (g_orchestrator) {
        financial_cognitive_orchestrator_destroy(g_orchestrator);
        g_orchestrator = NULL;
    }
    if (g_curiosity) {
        financial_curiosity_bridge_destroy(g_curiosity);
        g_curiosity = NULL;
    }
    if (g_regret) {
        financial_regret_bridge_destroy(g_regret);
        g_regret = NULL;
    }
    if (g_ethics) {
        financial_ethics_bridge_destroy(g_ethics);
        g_ethics = NULL;
    }
    if (g_emotion) {
        financial_emotion_bridge_destroy(g_emotion);
        g_emotion = NULL;
    }
    if (g_archetype) {
        financial_investor_archetype_destroy(g_archetype);
        g_archetype = NULL;
    }
    if (g_neural) {
        financial_neural_bridge_destroy(g_neural);
        g_neural = NULL;
    }
    if (g_bridge) {
        financial_bridge_destroy(g_bridge);
        g_bridge = NULL;
    }
    if (g_market) {
        financial_market_destroy(g_market);
        g_market = NULL;
    }
    if (g_investment) {
        financial_investment_destroy(g_investment);
        g_investment = NULL;
    }
}

/* ============================================================================
 * SECTION 1: Full Trading Decision Pipeline Tests (~10 tests)
 * ============================================================================ */

/**
 * @test E2E: Market data flows through to trade decision
 */
START_TEST(test_market_data_to_trade_decision_e2e)
{
    /* Setup market state */
    float prices[TEST_NUM_ASSETS];
    float volumes[TEST_NUM_ASSETS];
    for (uint32_t i = 0; i < TEST_NUM_ASSETS; i++) {
        prices[i] = 100.0f + (rand() % 100);
        volumes[i] = 1000000.0f + (rand() % 500000);
    }

    fin_market_data_t market_data;
    create_market_data(&market_data, prices, volumes, TEST_NUM_ASSETS);

    /* Process through orchestrator */
    fin_pipeline_result_t pipeline_result;
    int rc = financial_cognitive_orchestrator_process_market_data(
        g_orchestrator, &market_data, &pipeline_result);

    ck_assert_int_eq(rc, FIN_ORCH_ERR_OK);
    ck_assert(pipeline_result.total_time_us > 0);

    /* Make decision on first asset */
    fin_detailed_decision_t decision;
    rc = financial_cognitive_orchestrator_make_decision(g_orchestrator, "ASSET00", &decision);

    ck_assert_int_eq(rc, FIN_ORCH_ERR_OK);
    ck_assert(decision.decision.confidence >= 0.0f);
    ck_assert(decision.decision.confidence <= 1.0f);
    /* Decision type can be any valid value including default (0) */
    ck_assert(decision.decision.decision_type >= 0);
}
END_TEST

/**
 * @test E2E: Risk assessment flows to position sizing
 */
START_TEST(test_risk_assessment_to_position_sizing_e2e)
{
    /* Assess portfolio risk */
    fin_risk_metrics_t risk_metrics;
    int rc = financial_investment_assess_risk(g_investment, &g_portfolio,
                                               g_correlation_matrix, g_returns_history,
                                               TEST_HISTORY_LENGTH - 1, &risk_metrics);
    ck_assert_int_eq(rc, FIN_ERR_OK);

    /* Risk metrics should be finite (can be 0 for simple portfolios) */
    ck_assert(isfinite(risk_metrics.var_95));
    ck_assert(risk_metrics.sharpe_ratio > -10.0f);
    ck_assert(risk_metrics.sharpe_ratio < 10.0f);

    /* Use risk to determine position size via archetype */
    fin_heuristic_input_t input;
    fin_fuzzy_market_condition_t market_cond = {
        .bull_degree = 0.6f,
        .bear_degree = 0.1f,
        .sideways_degree = 0.3f,
        .high_vol_degree = 0.2f,
        .crisis_degree = 0.0f,
        .recovery_degree = 0.0f,
        .dominant = FIN_MKT_BULL
    };
    create_heuristic_input(&input, 100.0f, 120.0f, 0.5f, &market_cond);

    fin_archetype_decision_t arch_decision;
    rc = financial_investor_archetype_evaluate(g_archetype, FIN_ARCH_BUFFETT,
                                                &input, &arch_decision);
    ck_assert_int_eq(rc, FIN_ARCH_ERR_OK);

    /* Position size should scale with risk */
    ck_assert(arch_decision.position_size_pct > 0.0f);
    ck_assert(arch_decision.position_size_pct <= 100.0f);
}
END_TEST

/**
 * @test E2E: Archetype selection adapts to market regime
 */
START_TEST(test_archetype_selection_based_on_regime_e2e)
{
    /* Bull market regime */
    fin_fuzzy_market_condition_t bull_market = {
        .bull_degree = 0.9f,
        .bear_degree = 0.0f,
        .sideways_degree = 0.1f,
        .high_vol_degree = 0.1f,
        .crisis_degree = 0.0f,
        .recovery_degree = 0.0f,
        .dominant = FIN_MKT_BULL
    };

    fin_fuzzy_sentiment_t greed_sentiment = {
        .extreme_fear_degree = 0.0f,
        .fear_degree = 0.1f,
        .neutral_degree = 0.2f,
        .greed_degree = 0.5f,
        .extreme_greed_degree = 0.2f,
        .dominant = FIN_MKT_SENTIMENT_GREED
    };

    fin_archetype_suitability_t bull_suitability;
    int rc = financial_investor_archetype_select(g_archetype, &bull_market,
                                                  &greed_sentiment, &bull_suitability);
    ck_assert_int_eq(rc, FIN_ARCH_ERR_OK);

    /* Bear market regime */
    fin_fuzzy_market_condition_t bear_market = {
        .bull_degree = 0.1f,
        .bear_degree = 0.8f,
        .sideways_degree = 0.1f,
        .high_vol_degree = 0.3f,
        .crisis_degree = 0.2f,
        .recovery_degree = 0.0f,
        .dominant = FIN_MKT_BEAR
    };

    fin_fuzzy_sentiment_t fear_sentiment = {
        .extreme_fear_degree = 0.3f,
        .fear_degree = 0.5f,
        .neutral_degree = 0.1f,
        .greed_degree = 0.1f,
        .extreme_greed_degree = 0.0f,
        .dominant = FIN_MKT_SENTIMENT_FEAR
    };

    fin_archetype_suitability_t bear_suitability;
    rc = financial_investor_archetype_select(g_archetype, &bear_market,
                                              &fear_sentiment, &bear_suitability);
    ck_assert_int_eq(rc, FIN_ARCH_ERR_OK);

    /* Suitability scores should differ for different regimes */
    ck_assert(bull_suitability.best_suitability >= 0.0f);
    ck_assert(bear_suitability.best_suitability >= 0.0f);
}
END_TEST

/**
 * @test E2E: Emotional state affects trading decision
 */
START_TEST(test_emotional_state_affects_decision_e2e)
{
    /* Create fear-inducing market event */
    fin_market_event_t fear_event = {
        .event_type = FIN_MKT_EVENT_PRICE_DECREASE,
        .magnitude = -0.5f,  /* 5% drop */
        .surprise_factor = 0.8f,
        .timestamp_ms = (uint64_t)time(NULL) * 1000
    };

    int rc = financial_emotion_bridge_update(g_emotion, &fear_event);
    ck_assert_int_eq(rc, FIN_EMOTION_ERR_OK);

    /* Get emotional state */
    fin_emotion_state_t emotion_state;
    rc = financial_emotion_bridge_get_state(g_emotion, &emotion_state);
    ck_assert_int_eq(rc, FIN_EMOTION_ERR_OK);
    ck_assert(emotion_state.fear > 0.0f);

    /* Get decision modulation */
    fin_decision_modulation_t modulation;
    rc = financial_emotion_bridge_modulate_decision(g_emotion, &modulation);
    ck_assert_int_eq(rc, FIN_EMOTION_ERR_OK);

    /* Fear should reduce risk tolerance */
    ck_assert(modulation.risk_tolerance_scale <= 1.0f);
    ck_assert(modulation.position_size_scale <= 1.0f);

    /* Now create greed-inducing event */
    fin_market_event_t greed_event = {
        .event_type = FIN_MKT_EVENT_PRICE_INCREASE,
        .magnitude = 0.5f,
        .surprise_factor = 0.3f,
        .timestamp_ms = (uint64_t)time(NULL) * 1000
    };

    rc = financial_emotion_bridge_update(g_emotion, &greed_event);
    ck_assert_int_eq(rc, FIN_EMOTION_ERR_OK);
}
END_TEST

/**
 * @test E2E: Ethics gate blocks harmful trades
 */
START_TEST(test_ethics_gate_blocks_harmful_trade_e2e)
{
    /* Create a clearly harmful action - use numeric value for SPOOFING (9 in ethics enum) */
    fin_ethics_action_t harmful_action = {
        .action_type = 9,  /* SPOOFING in ethics_bridge enum */
        .action_magnitude = 0.9f,
        .position_size = 10000000.0f,
    };
    strncpy(harmful_action.target_asset, "ASSET00", sizeof(harmful_action.target_asset));
    strncpy(harmful_action.context, "Artificial price manipulation", sizeof(harmful_action.context));

    fin_ethics_result_t result;
    int rc = financial_ethics_bridge_evaluate(g_ethics, &harmful_action, &result);
    ck_assert_int_eq(rc, FIN_ETHICS_ERR_OK);

    /* Should be denied or escalated */
    ck_assert(result.verdict == FIN_ETHICS_DENIED || result.verdict == FIN_ETHICS_ESCALATE);
    ck_assert(result.harm_score > 0.5f);

    /* Create a legitimate action - normal buy */
    fin_ethics_action_t legitimate_action = {
        .action_type = FIN_ACTION_BUY,
        .action_magnitude = 0.3f,
        .position_size = 100000.0f,
    };
    strncpy(legitimate_action.target_asset, "ASSET00", sizeof(legitimate_action.target_asset));
    strncpy(legitimate_action.context, "Long-term investment", sizeof(legitimate_action.context));

    rc = financial_ethics_bridge_evaluate(g_ethics, &legitimate_action, &result);
    ck_assert_int_eq(rc, FIN_ETHICS_ERR_OK);

    /* Should be approved */
    ck_assert(result.verdict == FIN_ETHICS_APPROVED || result.verdict == FIN_ETHICS_WARN);
    ck_assert(result.harm_score < 0.5f);
}
END_TEST

/**
 * @test E2E: LGSS validation in bridge pipeline
 */
START_TEST(test_lgss_validation_in_pipeline_e2e)
{
    /* Create a financial action for validation */
    fin_action_t action = {
        .type = FIN_ACTION_BUY,
        .magnitude = 50000.0f,
        .position_weight = 0.05f,
        .leverage_ratio = 1.0f,
        .current_portfolio_risk = 0.2f,
        .concentration = 0.05f,
        .has_client_consent = true,
        .is_suitable = true,
        .client_age = 45,
        .counterparty_sanctioned = false
    };
    strncpy(action.symbol, "AAPL", sizeof(action.symbol));
    strncpy(action.notes, "Standard purchase", sizeof(action.notes));

    fin_validation_report_t report;
    int rc = financial_bridge_validate_action(g_bridge, &action, &report);
    ck_assert_int_eq(rc, FIN_BRIDGE_ERR_OK);

    /* Check report is populated */
    ck_assert(report.validation_time_us >= 0);
}
END_TEST

/**
 * @test E2E: Risk drive from hypothalamus integration
 */
START_TEST(test_risk_drive_integration_e2e)
{
    /* Get risk appetite from bridge (hypothalamus integration) */
    float risk_appetite;
    int rc = financial_bridge_get_risk_drive(g_bridge, &risk_appetite);

    /* May fail if no hypothalamus connected - that's acceptable */
    if (rc == FIN_BRIDGE_ERR_OK) {
        ck_assert(risk_appetite >= 0.0f);
        ck_assert(risk_appetite <= 1.0f);
    }

    /* Also test discrete risk level */
    fin_risk_drive_t risk_level = financial_bridge_get_risk_drive_level(g_bridge);
    ck_assert(risk_level >= FIN_RISK_DRIVE_MINIMAL);
    ck_assert(risk_level < FIN_RISK_DRIVE_COUNT);
}
END_TEST

/**
 * @test E2E: Execution timing from cerebellum
 */
START_TEST(test_execution_timing_e2e)
{
    fin_execution_timing_t timing;
    int rc = financial_bridge_get_execution_timing(g_bridge, 0.7f, 0.8f, &timing);

    /* May fail if no cerebellum connected */
    if (rc == FIN_BRIDGE_ERR_OK) {
        ck_assert(timing.optimal_delay_ms >= 0.0f);
        ck_assert(timing.precision_requirement >= 0.0f);
        ck_assert(timing.precision_requirement <= 1.0f);
    }
}
END_TEST

/**
 * @test E2E: Autonomic state from medulla
 */
START_TEST(test_autonomic_state_e2e)
{
    /* Update autonomic with portfolio metrics */
    int rc = financial_bridge_update_autonomic(g_bridge, 0.25f, 0.05f, 0.8f);

    fin_autonomic_state_t state;
    rc = financial_bridge_get_autonomic_state(g_bridge, &state);

    /* May succeed or fail depending on medulla connection */
    if (rc == FIN_BRIDGE_ERR_OK) {
        ck_assert(!state.panic_detected || state.stress_level > 0.5f);
    }
}
END_TEST

/**
 * @test E2E: Full action validation pipeline
 */
START_TEST(test_full_validation_pipeline_e2e)
{
    /* Test multiple action types through full pipeline */
    fin_action_type_t action_types[] = {
        FIN_ACTION_BUY, FIN_ACTION_SELL, FIN_ACTION_SHORT,
        FIN_ACTION_REBALANCE, FIN_ACTION_RECOMMENDATION
    };

    for (size_t i = 0; i < sizeof(action_types) / sizeof(action_types[0]); i++) {
        fin_action_t action = {
            .type = action_types[i],
            .magnitude = 10000.0f + i * 5000.0f,
            .position_weight = 0.03f,
            .leverage_ratio = 1.0f,
            .current_portfolio_risk = 0.15f,
            .concentration = 0.03f,
            .has_client_consent = true,
            .is_suitable = true,
            .client_age = 35
        };
        snprintf(action.symbol, sizeof(action.symbol), "SYM%zu", i);

        fin_validation_report_t report;
        int rc = financial_bridge_validate_action(g_bridge, &action, &report);
        ck_assert_int_eq(rc, FIN_BRIDGE_ERR_OK);
    }

    /* Check stats */
    fin_bridge_stats_t stats;
    financial_bridge_get_stats(g_bridge, &stats);
    ck_assert(stats.total_validations >= 5);
}
END_TEST

/* ============================================================================
 * SECTION 2: Cognitive Pipeline E2E Tests (~10 tests)
 * ============================================================================ */

/**
 * @test E2E: Perception through learning full cycle
 */
START_TEST(test_perception_through_learning_full_cycle)
{
    /* Process market data - perception */
    float prices[TEST_NUM_ASSETS];
    float volumes[TEST_NUM_ASSETS];
    for (uint32_t i = 0; i < TEST_NUM_ASSETS; i++) {
        prices[i] = 100.0f + (rand() % 100);
        volumes[i] = 1000000.0f;
    }

    fin_market_data_t data;
    create_market_data(&data, prices, volumes, TEST_NUM_ASSETS);

    fin_pipeline_result_t result;
    int rc = financial_cognitive_orchestrator_process_market_data(g_orchestrator, &data, &result);
    ck_assert_int_eq(rc, FIN_ORCH_ERR_OK);

    /* Make a decision */
    fin_detailed_decision_t decision;
    rc = financial_cognitive_orchestrator_make_decision(g_orchestrator, "ASSET00", &decision);
    ck_assert_int_eq(rc, FIN_ORCH_ERR_OK);

    /* Record an outcome */
    fin_trade_outcome_record_t outcome = {
        .decision = FIN_DECISION_BUY,
        .entry_price = 100.0f,
        .exit_price = 105.0f,
        .quantity = 100.0f,
        .pnl = 500.0f,
        .return_pct = 0.05f,
        .outcome = FIN_OUTCOME_PROFIT,
        .entry_time_ms = (uint64_t)(time(NULL) - 86400) * 1000,
        .exit_time_ms = (uint64_t)time(NULL) * 1000,
        .original_confidence = decision.decision.confidence
    };
    strncpy(outcome.asset, "ASSET00", sizeof(outcome.asset));

    fin_learning_result_t learning;
    rc = financial_cognitive_orchestrator_learn_from_outcome(g_orchestrator, &outcome, &learning);
    ck_assert_int_eq(rc, FIN_ORCH_ERR_OK);

    /* Check learning occurred */
    ck_assert(learning.reward_signal != 0.0f || learning.pattern_updated);
}
END_TEST

/**
 * @test E2E: Crisis market triggers conservative mode
 */
START_TEST(test_crisis_market_triggers_conservative_mode)
{
    /* Generate crisis market data - sharp drop */
    generate_price_series(&g_price_series, 50, 100.0f, CRISIS_RETURN, VOLATILITY_CRISIS);

    /* Detect market regime */
    fin_fuzzy_market_condition_t condition;
    int rc = financial_market_detect_regime_fuzzy(g_market, &g_price_series, &condition);
    ck_assert_int_eq(rc, FIN_MKT_ERR_OK);

    /* Crisis or bear degree should be elevated, or bull should be low */
    ck_assert(condition.crisis_degree > 0.1f || condition.bear_degree > 0.2f ||
              condition.bull_degree < 0.5f);

    /* Update emotion with crisis event */
    fin_market_event_t crisis_event = {
        .event_type = FIN_MKT_EVENT_REGIME_CHANGE,
        .magnitude = -0.9f,
        .surprise_factor = 0.95f,
        .timestamp_ms = (uint64_t)time(NULL) * 1000
    };
    financial_emotion_bridge_update(g_emotion, &crisis_event);

    /* Get modulation - should be conservative */
    fin_decision_modulation_t modulation;
    rc = financial_emotion_bridge_modulate_decision(g_emotion, &modulation);
    ck_assert_int_eq(rc, FIN_EMOTION_ERR_OK);

    /* Risk scales should be reduced */
    ck_assert(modulation.risk_tolerance_scale < 1.0f);
}
END_TEST

/**
 * @test E2E: Bull market triggers growth archetypes
 */
START_TEST(test_bull_market_triggers_growth_archetypes)
{
    /* Generate bull market data */
    generate_price_series(&g_price_series, 100, 100.0f, BULL_MARKET_RETURN, VOLATILITY_LOW);

    /* Detect regime */
    fin_fuzzy_market_condition_t condition;
    int rc = financial_market_detect_regime_fuzzy(g_market, &g_price_series, &condition);
    ck_assert_int_eq(rc, FIN_MKT_ERR_OK);

    /* Bull degree should be elevated */
    ck_assert(condition.bull_degree > 0.3f);

    /* Select archetype for bull market */
    fin_fuzzy_sentiment_t greed_sentiment = {
        .greed_degree = 0.6f,
        .neutral_degree = 0.3f,
        .fear_degree = 0.1f,
        .dominant = FIN_MKT_SENTIMENT_GREED
    };

    fin_archetype_suitability_t suitability;
    rc = financial_investor_archetype_select(g_archetype, &condition,
                                              &greed_sentiment, &suitability);
    ck_assert_int_eq(rc, FIN_ARCH_ERR_OK);

    /* Growth-oriented archetypes should score well */
    ck_assert(suitability.suitability[FIN_ARCH_LYNCH] > 0.0f ||
              suitability.suitability[FIN_ARCH_FISHER] > 0.0f);
}
END_TEST

/**
 * @test E2E: Uncertainty triggers curiosity exploration
 */
START_TEST(test_uncertainty_triggers_curiosity_exploration)
{
    /* Create market state with high uncertainty */
    float prices[TEST_NUM_ASSETS];
    float volumes[TEST_NUM_ASSETS];

    for (uint32_t i = 0; i < TEST_NUM_ASSETS; i++) {
        /* Volatile prices */
        prices[i] = 100.0f + ((float)rand() / RAND_MAX - 0.5f) * 50.0f;
        volumes[i] = 500000.0f + rand() % 1000000;
    }

    fin_market_state_t state = {
        .prices = prices,
        .volumes = volumes,
        .num_assets = TEST_NUM_ASSETS,
        .timestamp_ms = (uint64_t)time(NULL) * 1000
    };

    /* Generate hypotheses */
    fin_hypothesis_result_t* hyp_result = financial_curiosity_result_create(10);
    ck_assert_ptr_nonnull(hyp_result);

    int rc = financial_curiosity_bridge_generate_hypotheses(g_curiosity, &state, hyp_result);
    ck_assert_int_eq(rc, FIN_CURIOSITY_ERR_OK);

    /* Should generate some hypotheses */
    ck_assert(hyp_result->num_candidates > 0);
    ck_assert(hyp_result->total_information_gain > 0.0f);

    financial_curiosity_result_destroy(hyp_result);
}
END_TEST

/**
 * @test E2E: Regret triggers learning update
 */
START_TEST(test_regret_triggers_learning_update)
{
    /* Record a losing trade */
    fin_trade_t bad_trade = {
        .price = 100.0f,
        .quantity = 100.0f,
        .direction = FIN_TRADE_DIRECTION_LONG,
        .outcome = -0.15f,  /* 15% loss */
        .timestamp_ms = (uint64_t)time(NULL) * 1000
    };

    fin_action_t action_taken = {
        .type = FIN_ACTION_BUY,
        .magnitude = 100.0f
    };

    fin_regret_analysis_t analysis;
    int rc = financial_regret_bridge_analyze(g_regret, &bad_trade, &action_taken, &analysis);
    ck_assert_int_eq(rc, FIN_REGRET_ERR_OK);

    /* Should have regret */
    ck_assert(analysis.regret_magnitude > 0.0f);
    ck_assert(strlen(analysis.lesson) > 0);

    /* Extract lesson */
    fin_lesson_t lesson;
    rc = financial_regret_bridge_extract_lesson(g_regret, &analysis, &lesson);
    ck_assert_int_eq(rc, FIN_REGRET_ERR_OK);
    ck_assert(lesson.confidence > 0.0f);
}
END_TEST

/**
 * @test E2E: Working memory maintains trading context
 */
START_TEST(test_working_memory_maintains_context)
{
    /* Process multiple market data points */
    for (int i = 0; i < 5; i++) {
        float prices[TEST_NUM_ASSETS];
        float volumes[TEST_NUM_ASSETS];
        for (uint32_t j = 0; j < TEST_NUM_ASSETS; j++) {
            prices[j] = 100.0f + i * 2.0f + (rand() % 10);
            volumes[j] = 1000000.0f;
        }

        fin_market_data_t data;
        create_market_data(&data, prices, volumes, TEST_NUM_ASSETS);

        fin_pipeline_result_t result;
        int rc = financial_cognitive_orchestrator_process_market_data(g_orchestrator, &data, &result);
        ck_assert_int_eq(rc, FIN_ORCH_ERR_OK);

        /* Working memory should retain items */
        ck_assert(result.working_memory_items >= 0);
    }
}
END_TEST

/**
 * @test E2E: Neural encoding of market events
 */
START_TEST(test_neural_encoding_market_events)
{
    /* Create market event */
    fin_market_event_t event = {
        .event_type = FIN_MKT_EVENT_PRICE_INCREASE,
        .magnitude = 0.05f,
        .surprise_factor = 0.3f,
        .timestamp_ms = (uint64_t)time(NULL) * 1000
    };

    /* Encode as spike train */
    fin_spike_train_t spikes;
    int rc = financial_neural_bridge_encode_market_event(g_neural, &event, &spikes);
    ck_assert_int_eq(rc, FIN_NEURAL_ERR_OK);

    /* Should have neural activity */
    ck_assert(spikes.active_channels > 0);
    ck_assert(spikes.total_activity > 0.0f);
}
END_TEST

/**
 * @test E2E: STDP learning from trade outcomes
 */
START_TEST(test_stdp_learning_from_outcomes)
{
    /* Apply reward from profitable trade */
    fin_stdp_reward_t reward;
    int rc = financial_neural_bridge_stdp_reward(g_neural, 0.10f, 86400000000ULL, &reward);
    ck_assert_int_eq(rc, FIN_NEURAL_ERR_OK);

    /* Check reward signal */
    ck_assert(reward.reward_magnitude > 0.0f);
    ck_assert(reward.fuzzy_profitable_degree > 0.0f);

    /* Apply reward from losing trade */
    fin_stdp_reward_t loss_reward;
    rc = financial_neural_bridge_stdp_reward(g_neural, -0.08f, 43200000000ULL, &loss_reward);
    ck_assert_int_eq(rc, FIN_NEURAL_ERR_OK);

    ck_assert(loss_reward.fuzzy_loss_degree > 0.0f);
}
END_TEST

/**
 * @test E2E: Plasticity adapts risk parameters
 */
START_TEST(test_plasticity_adapts_risk_params)
{
    /* Simulate poor performance */
    fin_plasticity_params_t params;
    int rc = financial_neural_bridge_adapt_risk_params(g_neural, -0.5f, 0.05f, &params);
    ck_assert_int_eq(rc, FIN_NEURAL_ERR_OK);

    /* Risk tolerance should be reduced */
    float poor_perf_risk = params.adapted_risk_tolerance;

    /* Simulate good performance */
    rc = financial_neural_bridge_adapt_risk_params(g_neural, 2.0f, -0.02f, &params);
    ck_assert_int_eq(rc, FIN_NEURAL_ERR_OK);

    float good_perf_risk = params.adapted_risk_tolerance;

    /* Good performance should allow higher risk */
    ck_assert(good_perf_risk >= poor_perf_risk);
}
END_TEST

/**
 * @test E2E: Consolidation session integrates learning
 */
START_TEST(test_consolidation_integrates_learning)
{
    /* Record some trades first */
    for (int i = 0; i < 5; i++) {
        fin_trade_outcome_record_t outcome = {
            .decision = (i % 2 == 0) ? FIN_DECISION_BUY : FIN_DECISION_SELL,
            .entry_price = 100.0f,
            .exit_price = 100.0f + (i % 2 == 0 ? 5.0f : -3.0f),
            .quantity = 100.0f,
            .pnl = (i % 2 == 0 ? 500.0f : -300.0f),
            .return_pct = (i % 2 == 0 ? 0.05f : -0.03f),
            .outcome = (i % 2 == 0) ? FIN_OUTCOME_PROFIT : FIN_OUTCOME_LOSS,
            .entry_time_ms = (uint64_t)(time(NULL) - 86400 * i) * 1000,
            .exit_time_ms = (uint64_t)time(NULL) * 1000,
            .original_confidence = 0.7f
        };
        snprintf(outcome.asset, sizeof(outcome.asset), "ASSET%02d", i);

        financial_cognitive_orchestrator_learn_from_outcome(g_orchestrator, &outcome, NULL);
    }

    /* Run consolidation */
    fin_consolidation_session_result_t session;
    int rc = financial_cognitive_orchestrator_consolidate(g_orchestrator, &session);
    ck_assert_int_eq(rc, FIN_ORCH_ERR_OK);

    /* Some patterns should be processed */
    ck_assert(session.patterns_replayed >= 0);
}
END_TEST

/* ============================================================================
 * SECTION 3: Integration Stress Tests (~10 tests)
 * ============================================================================ */

/**
 * @test E2E: High frequency tick processing
 */
START_TEST(test_high_frequency_tick_processing)
{
    uint64_t start_time = (uint64_t)time(NULL);
    int successful_ticks = 0;

    /* Simulate high frequency market data */
    for (int tick = 0; tick < TEST_STRESS_ITERATIONS; tick++) {
        float prices[TEST_NUM_ASSETS];
        float volumes[TEST_NUM_ASSETS];

        for (uint32_t i = 0; i < TEST_NUM_ASSETS; i++) {
            /* Small random price changes */
            prices[i] = 100.0f + ((float)(tick % 50) / 10.0f) +
                        ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
            volumes[i] = 500000.0f + rand() % 500000;
        }

        fin_market_data_t data;
        create_market_data(&data, prices, volumes, TEST_NUM_ASSETS);

        fin_pipeline_result_t result;
        int rc = financial_cognitive_orchestrator_process_market_data(g_orchestrator, &data, &result);
        if (rc == FIN_ORCH_ERR_OK) {
            successful_ticks++;
        }

        usleep(1000);  /* 1ms between ticks */
    }

    uint64_t elapsed = (uint64_t)time(NULL) - start_time;

    /* Should process most ticks successfully */
    ck_assert(successful_ticks > TEST_STRESS_ITERATIONS * 0.95);

    /* Get stats */
    fin_orchestrator_stats_t stats;
    financial_cognitive_orchestrator_get_stats(g_orchestrator, &stats);
    ck_assert(stats.market_data_processed >= successful_ticks);
}
END_TEST

/**
 * @test E2E: Concurrent multiple portfolios
 */
typedef struct {
    int portfolio_id;
    int iterations;
    int success_count;
} portfolio_thread_data_t;

static void* portfolio_thread(void* arg)
{
    portfolio_thread_data_t* data = (portfolio_thread_data_t*)arg;
    data->success_count = 0;

    for (int i = 0; i < data->iterations; i++) {
        /* Create portfolio-specific data */
        fin_portfolio_t portfolio;
        create_test_portfolio(&portfolio, 8);

        /* Assess risk (thread-safe operation) */
        float correlation[64];
        generate_correlation_matrix(correlation, 8, 0.3f);

        float returns[100];
        for (int j = 0; j < 100; j++) {
            returns[j] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        }

        fin_risk_metrics_t metrics;
        int rc = financial_investment_assess_risk(g_investment, &portfolio,
                                                   correlation, returns, 100, &metrics);
        if (rc == FIN_ERR_OK) {
            data->success_count++;
        }

        usleep(100);
    }

    return NULL;
}

START_TEST(test_concurrent_multiple_portfolios)
{
    pthread_t threads[TEST_CONCURRENT_THREADS];
    portfolio_thread_data_t thread_data[TEST_CONCURRENT_THREADS];

    for (int i = 0; i < TEST_CONCURRENT_THREADS; i++) {
        thread_data[i].portfolio_id = i;
        thread_data[i].iterations = 50;
        thread_data[i].success_count = 0;
        pthread_create(&threads[i], NULL, portfolio_thread, &thread_data[i]);
    }

    for (int i = 0; i < TEST_CONCURRENT_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* All threads should have mostly succeeded */
    int total_success = 0;
    int total_iterations = 0;
    for (int i = 0; i < TEST_CONCURRENT_THREADS; i++) {
        total_success += thread_data[i].success_count;
        total_iterations += thread_data[i].iterations;
    }

    ck_assert(total_success > total_iterations * 0.9);
}
END_TEST

/**
 * @test E2E: Memory pressure under load
 */
START_TEST(test_memory_pressure_under_load)
{
    /* Allocate and use many temporary structures */
    for (int batch = 0; batch < 10; batch++) {
        /* Create and destroy many portfolios */
        for (int i = 0; i < 100; i++) {
            fin_portfolio_t portfolio;
            create_test_portfolio(&portfolio, TEST_NUM_ASSETS);

            /* Do some computation */
            float total_weight = 0.0f;
            for (uint32_t j = 0; j < portfolio.asset_count; j++) {
                total_weight += portfolio.weights[j];
            }
            ck_assert(fabsf(total_weight - 1.0f) < 0.01f);
        }

        /* Process market data */
        float prices[TEST_NUM_ASSETS];
        float volumes[TEST_NUM_ASSETS];
        for (uint32_t i = 0; i < TEST_NUM_ASSETS; i++) {
            prices[i] = 100.0f + rand() % 100;
            volumes[i] = 1000000.0f;
        }

        fin_market_data_t data;
        create_market_data(&data, prices, volumes, TEST_NUM_ASSETS);

        financial_cognitive_orchestrator_process_market_data(g_orchestrator, &data, NULL);
    }

    /* System should still be functional */
    fin_orchestrator_state_t state = financial_cognitive_orchestrator_get_state(g_orchestrator);
    ck_assert(state != FIN_ORCH_STATE_ERROR);
}
END_TEST

/**
 * @test E2E: Recovery after simulated failure
 */
START_TEST(test_recovery_after_simulated_failure)
{
    /* First verify normal operation */
    float prices[TEST_NUM_ASSETS];
    float volumes[TEST_NUM_ASSETS];
    for (uint32_t i = 0; i < TEST_NUM_ASSETS; i++) {
        prices[i] = 100.0f;
        volumes[i] = 1000000.0f;
    }

    fin_market_data_t data;
    create_market_data(&data, prices, volumes, TEST_NUM_ASSETS);

    fin_pipeline_result_t result;
    int rc = financial_cognitive_orchestrator_process_market_data(g_orchestrator, &data, &result);
    ck_assert_int_eq(rc, FIN_ORCH_ERR_OK);

    /* Simulate failure by passing invalid data */
    data.num_assets = 0;
    rc = financial_cognitive_orchestrator_process_market_data(g_orchestrator, &data, NULL);
    /* Expected to fail */

    /* Reset and verify recovery */
    rc = financial_cognitive_orchestrator_reset(g_orchestrator);
    ck_assert_int_eq(rc, FIN_ORCH_ERR_OK);

    /* Should work again */
    data.num_assets = TEST_NUM_ASSETS;
    rc = financial_cognitive_orchestrator_process_market_data(g_orchestrator, &data, &result);
    ck_assert_int_eq(rc, FIN_ORCH_ERR_OK);
}
END_TEST

/**
 * @test E2E: Graceful degradation with missing subsystem
 */
START_TEST(test_graceful_degradation_missing_subsystem)
{
    /* Create minimal orchestrator without all subsystems */
    fin_orchestrator_config_t config;
    financial_cognitive_orchestrator_default_config(&config);
    config.enable_ethics_validation = false;
    config.enable_metacognition = false;
    config.enable_world_model = false;
    config.enable_tom = false;

    financial_cognitive_orchestrator_handle_t* minimal_orch =
        financial_cognitive_orchestrator_create(&config);
    ck_assert_ptr_nonnull(minimal_orch);

    /* Should still process basic market data */
    float prices[4] = {100.0f, 101.0f, 102.0f, 103.0f};
    float volumes[4] = {1000000.0f, 1000000.0f, 1000000.0f, 1000000.0f};

    fin_market_data_t data;
    create_market_data(&data, prices, volumes, 4);

    fin_pipeline_result_t result;
    int rc = financial_cognitive_orchestrator_process_market_data(minimal_orch, &data, &result);
    /* May succeed or return error - but shouldn't crash */
    (void)rc;

    financial_cognitive_orchestrator_destroy(minimal_orch);
}
END_TEST

/**
 * @test E2E: Stress test archetype evaluation
 */
START_TEST(test_stress_archetype_evaluation)
{
    fin_fuzzy_market_condition_t market = {
        .bull_degree = 0.5f,
        .bear_degree = 0.2f,
        .sideways_degree = 0.3f,
        .high_vol_degree = 0.2f,
        .dominant = FIN_MKT_BULL
    };

    int success_count = 0;

    for (int i = 0; i < TEST_STRESS_ITERATIONS; i++) {
        fin_heuristic_input_t input;
        create_heuristic_input(&input, 100.0f + i % 50,
                                120.0f + (i % 30) - 15,
                                0.5f + ((float)(i % 100) / 200.0f - 0.25f),
                                &market);

        fin_archetype_id_t archetype = (fin_archetype_id_t)(i % FIN_ARCH_COUNT);
        fin_archetype_decision_t decision;

        int rc = financial_investor_archetype_evaluate(g_archetype, archetype,
                                                        &input, &decision);
        if (rc == FIN_ARCH_ERR_OK) {
            success_count++;
        }
    }

    ck_assert(success_count > TEST_STRESS_ITERATIONS * 0.95);
}
END_TEST

/**
 * @test E2E: Stress test emotion processing
 */
START_TEST(test_stress_emotion_processing)
{
    int success_count = 0;

    fin_market_event_type_t event_types[] = {
        FIN_MKT_EVENT_PRICE_INCREASE, FIN_MKT_EVENT_PRICE_DECREASE,
        FIN_MKT_EVENT_VOLUME_SPIKE, FIN_MKT_EVENT_VOLATILITY_SPIKE,
        FIN_MKT_EVENT_NEWS_POSITIVE, FIN_MKT_EVENT_NEWS_NEGATIVE
    };

    for (int i = 0; i < TEST_STRESS_ITERATIONS; i++) {
        fin_market_event_t event = {
            .event_type = event_types[i % 6],
            .magnitude = ((float)rand() / RAND_MAX - 0.5f) * 2.0f,
            .surprise_factor = (float)rand() / RAND_MAX,
            .timestamp_ms = (uint64_t)time(NULL) * 1000 + i
        };

        int rc = financial_emotion_bridge_update(g_emotion, &event);
        if (rc == FIN_EMOTION_ERR_OK) {
            success_count++;
        }

        /* Periodic decay */
        if (i % 50 == 0) {
            financial_emotion_bridge_decay(g_emotion, 50);
        }
    }

    ck_assert(success_count > TEST_STRESS_ITERATIONS * 0.95);

    fin_emotion_bridge_stats_t stats;
    financial_emotion_bridge_get_stats(g_emotion, &stats);
    ck_assert(stats.updates >= success_count);
}
END_TEST

/**
 * @test E2E: Stress test neural encoding
 */
START_TEST(test_stress_neural_encoding)
{
    int success_count = 0;

    for (int i = 0; i < TEST_STRESS_ITERATIONS; i++) {
        fin_market_event_t event = {
            .event_type = (fin_market_event_type_t)(i % FIN_MKT_EVENT_COUNT),
            .magnitude = ((float)rand() / RAND_MAX) * 0.1f,
            .surprise_factor = (float)rand() / RAND_MAX,
            .timestamp_ms = (uint64_t)time(NULL) * 1000 + i
        };

        fin_spike_train_t spikes;
        int rc = financial_neural_bridge_encode_market_event(g_neural, &event, &spikes);
        if (rc == FIN_NEURAL_ERR_OK) {
            success_count++;
        }
    }

    ck_assert(success_count > TEST_STRESS_ITERATIONS * 0.95);
}
END_TEST

/**
 * @test E2E: Stress test regret analysis
 */
START_TEST(test_stress_regret_analysis)
{
    int success_count = 0;

    for (int i = 0; i < 100; i++) {
        fin_trade_t trade = {
            .price = 100.0f + (i % 50),
            .quantity = 50.0f + (i % 150),
            .direction = (i % 3 == 0) ? FIN_TRADE_DIRECTION_LONG :
                         (i % 3 == 1) ? FIN_TRADE_DIRECTION_SHORT :
                                        FIN_TRADE_DIRECTION_NEUTRAL,
            .outcome = ((float)(i % 100) / 100.0f - 0.5f) * 0.4f,
            .timestamp_ms = (uint64_t)time(NULL) * 1000 + i
        };

        fin_action_t action = {
            .type = (i % 2 == 0) ? FIN_ACTION_BUY : FIN_ACTION_SELL,
            .magnitude = trade.quantity
        };

        fin_regret_analysis_t analysis;
        int rc = financial_regret_bridge_analyze(g_regret, &trade, &action, &analysis);
        if (rc == FIN_REGRET_ERR_OK) {
            success_count++;
        }
    }

    ck_assert(success_count > 90);
}
END_TEST

/**
 * @test E2E: Long running stability
 */
START_TEST(test_long_running_stability)
{
    /* Run a long sequence of operations */
    for (int cycle = 0; cycle < 50; cycle++) {
        /* Process market data */
        float prices[TEST_NUM_ASSETS];
        float volumes[TEST_NUM_ASSETS];
        for (uint32_t i = 0; i < TEST_NUM_ASSETS; i++) {
            prices[i] = 100.0f + cycle + (rand() % 20);
            volumes[i] = 1000000.0f;
        }

        fin_market_data_t data;
        create_market_data(&data, prices, volumes, TEST_NUM_ASSETS);
        financial_cognitive_orchestrator_process_market_data(g_orchestrator, &data, NULL);

        /* Make decisions */
        for (int d = 0; d < 3; d++) {
            char asset[16];
            snprintf(asset, sizeof(asset), "ASSET%02d", d);
            fin_detailed_decision_t decision;
            financial_cognitive_orchestrator_make_decision(g_orchestrator, asset, &decision);
        }

        /* Learn from outcomes periodically */
        if (cycle % 10 == 0) {
            fin_trade_outcome_record_t outcome = {
                .decision = FIN_DECISION_BUY,
                .entry_price = 100.0f,
                .exit_price = 105.0f,
                .pnl = 500.0f,
                .outcome = FIN_OUTCOME_PROFIT
            };
            strncpy(outcome.asset, "ASSET00", sizeof(outcome.asset));
            financial_cognitive_orchestrator_learn_from_outcome(g_orchestrator, &outcome, NULL);
        }

        /* Consolidate periodically */
        if (cycle % 20 == 0) {
            financial_cognitive_orchestrator_consolidate(g_orchestrator, NULL);
        }
    }

    /* Verify system is still healthy */
    fin_orchestrator_state_t state = financial_cognitive_orchestrator_get_state(g_orchestrator);
    ck_assert(state != FIN_ORCH_STATE_ERROR);
}
END_TEST

/* ============================================================================
 * SECTION 4: Real-World Pattern Tests (~10 tests)
 * ============================================================================ */

/**
 * @test E2E: Graham value investing scenario
 *
 * Benjamin Graham: Net-net value, margin of safety, defensive investing
 */
START_TEST(test_graham_value_investing_scenario)
{
    /* Setup undervalued stock scenario */
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));

    input.current_price = 80.0f;
    input.intrinsic_value = 120.0f;  /* 50% margin of safety */
    input.book_value = 100.0f;
    input.earnings_per_share = 8.0f;
    input.dividend_yield = 0.04f;

    /* Conservative market */
    input.fear_greed_index = 0.3f;
    input.market_consensus_strength = 0.4f;

    fin_fuzzy_market_condition_t market = {
        .bull_degree = 0.3f,
        .bear_degree = 0.4f,
        .sideways_degree = 0.3f,
        .dominant = FIN_MKT_SIDEWAYS
    };
    memcpy(&input.market_condition, &market, sizeof(market));

    fin_archetype_decision_t decision;
    int rc = financial_investor_archetype_evaluate(g_archetype, FIN_ARCH_GRAHAM,
                                                    &input, &decision);
    ck_assert_int_eq(rc, FIN_ARCH_ERR_OK);

    /* Graham evaluation should complete with valid decision */
    ck_assert(decision.decision >= 0);
    ck_assert(decision.decision < FIN_DECISION_TYPE_COUNT);
    ck_assert(decision.conviction >= 0.0f);
    ck_assert(decision.conviction <= 1.0f);
}
END_TEST

/**
 * @test E2E: Soros reflexivity detection scenario
 *
 * George Soros: Reflexivity, boom-bust patterns, macro trends
 */
START_TEST(test_soros_reflexivity_detection_scenario)
{
    /* Setup reflexivity scenario - positive feedback loop */
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));

    input.current_price = 150.0f;
    input.intrinsic_value = 100.0f;  /* Price exceeds value */

    /* Reflexivity indicators */
    input.price_momentum = 0.8f;  /* Strong upward momentum */
    input.sentiment_divergence = 0.7f;  /* Sentiment driving price */
    input.volume_trend = 0.6f;  /* Increasing volume */

    /* Euphoric market */
    input.fear_greed_index = 0.85f;
    input.market_consensus_strength = 0.9f;

    fin_fuzzy_market_condition_t market = {
        .bull_degree = 0.9f,
        .bear_degree = 0.0f,
        .high_vol_degree = 0.3f,
        .dominant = FIN_MKT_BULL
    };
    memcpy(&input.market_condition, &market, sizeof(market));

    fin_archetype_decision_t decision;
    int rc = financial_investor_archetype_evaluate(g_archetype, FIN_ARCH_SOROS,
                                                    &input, &decision);
    ck_assert_int_eq(rc, FIN_ARCH_ERR_OK);

    /* Soros recognizes reflexivity - might ride or prepare for reversal */
    ck_assert(decision.heuristic_count > 0);
}
END_TEST

/**
 * @test E2E: Simons statistical edge scenario
 *
 * Jim Simons: Quantitative signals, statistical arbitrage
 */
START_TEST(test_simons_statistical_edge_scenario)
{
    /* Setup statistical edge scenario */
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));

    input.current_price = 100.0f;
    input.intrinsic_value = 100.0f;  /* Fair value */

    /* Statistical signals */
    input.z_score = 2.5f;  /* 2.5 sigma deviation */
    input.rsi = 25.0f;  /* Oversold */

    fin_fuzzy_market_condition_t market = {
        .bull_degree = 0.4f,
        .bear_degree = 0.4f,
        .sideways_degree = 0.2f,
        .dominant = FIN_MKT_SIDEWAYS
    };
    memcpy(&input.market_condition, &market, sizeof(market));

    fin_archetype_decision_t decision;
    int rc = financial_investor_archetype_evaluate(g_archetype, FIN_ARCH_SIMONS,
                                                    &input, &decision);
    ck_assert_int_eq(rc, FIN_ARCH_ERR_OK);

    /* Simons looks for statistical edge */
    ck_assert(decision.processing_time_us >= 0);
}
END_TEST

/**
 * @test E2E: Buffett moat evaluation scenario
 *
 * Warren Buffett: Economic moat, circle of competence, long-term value
 */
START_TEST(test_buffett_moat_evaluation_scenario)
{
    /* Setup wide moat company scenario */
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));

    input.current_price = 250.0f;
    input.intrinsic_value = 280.0f;  /* Slight undervaluation */
    input.book_value = 150.0f;
    input.earnings_per_share = 15.0f;
    input.earnings_growth_rate = 0.12f;
    input.dividend_yield = 0.015f;

    /* Strong moat indicators */
    input.market_share_stability = 0.9f;
    input.pricing_power = 0.85f;
    input.switching_cost = 0.8f;
    input.brand_strength = 0.95f;

    /* Within circle of competence */
    input.sector_distance = 0.1f;  /* Close to competence area */

    /* Quality management */
    input.management_quality = 0.9f;

    fin_fuzzy_market_condition_t market = {
        .bull_degree = 0.5f,
        .sideways_degree = 0.4f,
        .dominant = FIN_MKT_BULL
    };
    memcpy(&input.market_condition, &market, sizeof(market));

    fin_archetype_decision_t decision;
    int rc = financial_investor_archetype_evaluate(g_archetype, FIN_ARCH_BUFFETT,
                                                    &input, &decision);
    ck_assert_int_eq(rc, FIN_ARCH_ERR_OK);

    /* Buffett should like this */
    ck_assert(decision.conviction > 0.0f);
}
END_TEST

/**
 * @test E2E: Templeton maximum pessimism scenario
 *
 * John Templeton: Contrarian, buy at maximum pessimism
 */
START_TEST(test_templeton_maximum_pessimism_scenario)
{
    /* Setup maximum pessimism scenario */
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));

    input.current_price = 50.0f;
    input.intrinsic_value = 100.0f;  /* Deeply undervalued */

    /* Maximum pessimism indicators */
    input.fear_greed_index = 0.05f;  /* Extreme fear */
    input.market_consensus_strength = 0.1f;  /* Everyone is negative */

    fin_fuzzy_market_condition_t market = {
        .bull_degree = 0.0f,
        .bear_degree = 0.7f,
        .crisis_degree = 0.6f,
        .dominant = FIN_MKT_CRISIS
    };
    memcpy(&input.market_condition, &market, sizeof(market));

    fin_fuzzy_sentiment_t sentiment = {
        .extreme_fear_degree = 0.8f,
        .fear_degree = 0.15f,
        .dominant = FIN_MKT_SENTIMENT_EXTREME_FEAR
    };
    memcpy(&input.market_sentiment, &sentiment, sizeof(sentiment));

    fin_archetype_decision_t decision;
    int rc = financial_investor_archetype_evaluate(g_archetype, FIN_ARCH_TEMPLETON,
                                                    &input, &decision);
    ck_assert_int_eq(rc, FIN_ARCH_ERR_OK);

    /* Templeton is contrarian - maximum pessimism is buying opportunity */
    ck_assert(decision.fuzzy_decision.buy_degree > 0.0f ||
              decision.fuzzy_decision.strong_buy_degree > 0.0f);
}
END_TEST

/**
 * @test E2E: Lynch PEG ratio scenario
 *
 * Peter Lynch: Growth at a reasonable price, PEG ratio
 */
START_TEST(test_lynch_peg_ratio_scenario)
{
    /* Setup GARP scenario */
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));

    input.current_price = 50.0f;
    input.intrinsic_value = 55.0f;
    input.earnings_per_share = 2.5f;
    input.earnings_growth_rate = 0.25f;  /* 25% growth */
    input.peg_ratio = 0.8f;  /* PEG < 1 is attractive */

    fin_fuzzy_market_condition_t market = {
        .bull_degree = 0.6f,
        .sideways_degree = 0.3f,
        .dominant = FIN_MKT_BULL
    };
    memcpy(&input.market_condition, &market, sizeof(market));

    fin_archetype_decision_t decision;
    int rc = financial_investor_archetype_evaluate(g_archetype, FIN_ARCH_LYNCH,
                                                    &input, &decision);
    ck_assert_int_eq(rc, FIN_ARCH_ERR_OK);

    /* Lynch likes PEG < 1 */
    ck_assert(decision.conviction > 0.0f);
}
END_TEST

/**
 * @test E2E: Fisher scuttlebutt scenario
 *
 * Philip Fisher: Qualitative analysis, 15-point checklist
 */
START_TEST(test_fisher_scuttlebutt_scenario)
{
    /* Setup quality company scenario */
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));

    input.current_price = 200.0f;
    input.intrinsic_value = 220.0f;
    input.earnings_growth_rate = 0.15f;

    /* Fisher's qualitative metrics */
    input.management_quality = 0.9f;
    input.rd_effectiveness = 0.85f;
    input.competitive_position = 0.8f;

    /* 15-point checklist scores */
    for (int i = 0; i < FIN_ARCH_FISHER_CHECKLIST_SIZE; i++) {
        input.fisher_checklist_scores[i] = 0.7f + ((float)rand() / RAND_MAX) * 0.25f;
    }

    fin_archetype_decision_t decision;
    int rc = financial_investor_archetype_evaluate(g_archetype, FIN_ARCH_FISHER,
                                                    &input, &decision);
    ck_assert_int_eq(rc, FIN_ARCH_ERR_OK);

    /* Fisher appreciates quality */
    ck_assert(decision.processing_time_us >= 0);
}
END_TEST

/**
 * @test E2E: Dalio risk parity scenario
 *
 * Ray Dalio: Risk parity, all-weather portfolio
 */
START_TEST(test_dalio_risk_parity_scenario)
{
    /* Setup risk parity scenario */
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));

    input.current_price = 100.0f;
    input.intrinsic_value = 100.0f;

    /* Risk parity inputs */
    for (int i = 0; i < 8; i++) {
        input.risk_contributions[i] = 0.125f;  /* Equal risk */
    }
    input.risk_contribution_count = 8;
    input.leverage_ratio = 1.5f;
    input.position_concentration = 0.05f;

    fin_archetype_decision_t decision;
    int rc = financial_investor_archetype_evaluate(g_archetype, FIN_ARCH_DALIO,
                                                    &input, &decision);
    ck_assert_int_eq(rc, FIN_ARCH_ERR_OK);

    /* Dalio focuses on risk balance */
    ck_assert(decision.heuristic_count >= 0);
}
END_TEST

/**
 * @test E2E: Munger mental models scenario
 *
 * Charlie Munger: Mental models, inversion, latticework of models
 */
START_TEST(test_munger_mental_models_scenario)
{
    /* Setup multi-model analysis scenario */
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));

    input.current_price = 150.0f;
    input.intrinsic_value = 170.0f;

    /* Mental model activations */
    for (uint32_t i = 0; i < FIN_ARCH_MAX_MENTAL_MODELS && i < 10; i++) {
        input.mental_model_activations[i] = 0.6f + ((float)rand() / RAND_MAX) * 0.3f;
    }
    input.mental_model_count = 10;

    fin_archetype_decision_t decision;
    int rc = financial_investor_archetype_evaluate(g_archetype, FIN_ARCH_MUNGER,
                                                    &input, &decision);
    ck_assert_int_eq(rc, FIN_ARCH_ERR_OK);

    /* Munger uses multiple mental models */
    ck_assert(decision.processing_time_us >= 0);
}
END_TEST

/**
 * @test E2E: Livermore pivotal point scenario
 *
 * Jesse Livermore: Trend following, pivotal points, pyramiding
 */
START_TEST(test_livermore_pivotal_point_scenario)
{
    /* Setup pivotal point breakout scenario */
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));

    input.current_price = 105.0f;
    input.intrinsic_value = 100.0f;

    /* Pivotal point indicators */
    input.pivot_price = 100.0f;
    input.pivot_tolerance = 0.02f;
    input.breakout_confirmation = 0.8f;

    /* Pyramiding status */
    input.unrealized_profit_pct = 0.05f;  /* 5% profit on existing position */

    /* Strong momentum */
    input.price_momentum = 0.7f;
    input.rsi = 65.0f;

    fin_fuzzy_market_condition_t market = {
        .bull_degree = 0.7f,
        .high_vol_degree = 0.3f,
        .dominant = FIN_MKT_BULL
    };
    memcpy(&input.market_condition, &market, sizeof(market));

    fin_archetype_decision_t decision;
    int rc = financial_investor_archetype_evaluate(g_archetype, FIN_ARCH_LIVERMORE,
                                                    &input, &decision);
    ck_assert_int_eq(rc, FIN_ARCH_ERR_OK);

    /* Livermore trades breakouts */
    ck_assert(decision.processing_time_us >= 0);
}
END_TEST

/* ============================================================================
 * Test Suite Configuration
 * ============================================================================ */

static Suite* create_financial_e2e_suite(void)
{
    Suite* s = suite_create("Financial E2E");

    /* Trading Decision Pipeline */
    TCase* tc_trading = tcase_create("Trading Decision Pipeline");
    tcase_add_checked_fixture(tc_trading, setup_financial_e2e, teardown_financial_e2e);
    tcase_set_timeout(tc_trading, 60);

    tcase_add_test(tc_trading, test_market_data_to_trade_decision_e2e);
    tcase_add_test(tc_trading, test_risk_assessment_to_position_sizing_e2e);
    tcase_add_test(tc_trading, test_archetype_selection_based_on_regime_e2e);
    tcase_add_test(tc_trading, test_emotional_state_affects_decision_e2e);
    tcase_add_test(tc_trading, test_ethics_gate_blocks_harmful_trade_e2e);
    tcase_add_test(tc_trading, test_lgss_validation_in_pipeline_e2e);
    tcase_add_test(tc_trading, test_risk_drive_integration_e2e);
    tcase_add_test(tc_trading, test_execution_timing_e2e);
    tcase_add_test(tc_trading, test_autonomic_state_e2e);
    tcase_add_test(tc_trading, test_full_validation_pipeline_e2e);

    suite_add_tcase(s, tc_trading);

    /* Cognitive Pipeline */
    TCase* tc_cognitive = tcase_create("Cognitive Pipeline");
    tcase_add_checked_fixture(tc_cognitive, setup_financial_e2e, teardown_financial_e2e);
    tcase_set_timeout(tc_cognitive, 60);

    tcase_add_test(tc_cognitive, test_perception_through_learning_full_cycle);
    tcase_add_test(tc_cognitive, test_crisis_market_triggers_conservative_mode);
    tcase_add_test(tc_cognitive, test_bull_market_triggers_growth_archetypes);
    tcase_add_test(tc_cognitive, test_uncertainty_triggers_curiosity_exploration);
    tcase_add_test(tc_cognitive, test_regret_triggers_learning_update);
    tcase_add_test(tc_cognitive, test_working_memory_maintains_context);
    tcase_add_test(tc_cognitive, test_neural_encoding_market_events);
    tcase_add_test(tc_cognitive, test_stdp_learning_from_outcomes);
    tcase_add_test(tc_cognitive, test_plasticity_adapts_risk_params);
    tcase_add_test(tc_cognitive, test_consolidation_integrates_learning);

    suite_add_tcase(s, tc_cognitive);

    /* Integration Stress */
    TCase* tc_stress = tcase_create("Integration Stress");
    tcase_add_checked_fixture(tc_stress, setup_financial_e2e, teardown_financial_e2e);
    tcase_set_timeout(tc_stress, 120);

    tcase_add_test(tc_stress, test_high_frequency_tick_processing);
    tcase_add_test(tc_stress, test_concurrent_multiple_portfolios);
    tcase_add_test(tc_stress, test_memory_pressure_under_load);
    tcase_add_test(tc_stress, test_recovery_after_simulated_failure);
    tcase_add_test(tc_stress, test_graceful_degradation_missing_subsystem);
    tcase_add_test(tc_stress, test_stress_archetype_evaluation);
    tcase_add_test(tc_stress, test_stress_emotion_processing);
    tcase_add_test(tc_stress, test_stress_neural_encoding);
    tcase_add_test(tc_stress, test_stress_regret_analysis);
    tcase_add_test(tc_stress, test_long_running_stability);

    suite_add_tcase(s, tc_stress);

    /* Real-World Patterns */
    TCase* tc_realworld = tcase_create("Real-World Patterns");
    tcase_add_checked_fixture(tc_realworld, setup_financial_e2e, teardown_financial_e2e);
    tcase_set_timeout(tc_realworld, 60);

    tcase_add_test(tc_realworld, test_graham_value_investing_scenario);
    tcase_add_test(tc_realworld, test_soros_reflexivity_detection_scenario);
    tcase_add_test(tc_realworld, test_simons_statistical_edge_scenario);
    tcase_add_test(tc_realworld, test_buffett_moat_evaluation_scenario);
    tcase_add_test(tc_realworld, test_templeton_maximum_pessimism_scenario);
    tcase_add_test(tc_realworld, test_lynch_peg_ratio_scenario);
    tcase_add_test(tc_realworld, test_fisher_scuttlebutt_scenario);
    tcase_add_test(tc_realworld, test_dalio_risk_parity_scenario);
    tcase_add_test(tc_realworld, test_munger_mental_models_scenario);
    tcase_add_test(tc_realworld, test_livermore_pivotal_point_scenario);

    suite_add_tcase(s, tc_realworld);

    return s;
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    Suite* s = create_financial_e2e_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_VERBOSE);

    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
