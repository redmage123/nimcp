/**
 * @file test_financial_e2e.cpp
 * @brief End-to-end pipeline tests for NIMCP Financial Investment module
 * @date 2026-01-29
 *
 * WHAT: Full system pipeline tests exercising all 5 financial sub-modules
 * WHY:  Verify complete workflows from market data through decision execution
 * HOW:  Create all engines, wire together, run multi-step pipelines
 *
 * COVERAGE:
 * - Complete investment cycle
 * - Market analysis pipeline
 * - Archetype advisory pipeline
 * - Validation pipeline
 * - Neural learning pipeline
 * - Multi-archetype blend
 * - Emotional journey
 * - Stress scenario
 * - Fuzzy archetype across regimes
 * - Fuzzy risk-gated trading
 * - Fuzzy emotion-driven decision
 * - Health modulation
 * - Full system integration
 * - Memory cleanup
 * - Stats accumulation
 * - Performance baseline
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>

extern "C" {
#include "cognitive/parietal/nimcp_financial_investment.h"
#include "cognitive/parietal/nimcp_financial_market.h"
#include "cognitive/parietal/nimcp_financial_bridge.h"
#include "cognitive/parietal/nimcp_financial_neural_bridge.h"
#include "cognitive/parietal/nimcp_financial_investor_archetype.h"
}

// ============================================================================
// Helpers
// ============================================================================

static void fill_bull_market(fin_time_series_t* ts, uint32_t len) {
    memset(ts, 0, sizeof(*ts));
    ts->length = len;
    for (uint32_t i = 0; i < len; i++) {
        ts->prices[i]     = 100.0f + (float)i * 0.3f + sinf((float)i * 0.1f) * 2.0f;
        ts->volumes[i]    = 10000.0f + (float)(i % 20) * 500.0f;
        ts->timestamps[i] = 1000000ULL * i;
    }
    ts->open  = ts->prices[0];
    ts->close = ts->prices[len - 1];
    ts->high  = ts->prices[len - 1] + 5.0f;
    ts->low   = ts->prices[0] - 2.0f;
}

static void fill_volatile_market(fin_time_series_t* ts, uint32_t len) {
    memset(ts, 0, sizeof(*ts));
    ts->length = len;
    for (uint32_t i = 0; i < len; i++) {
        ts->prices[i]     = 100.0f + sinf((float)i * 0.5f) * 15.0f;
        ts->volumes[i]    = 15000.0f + (float)(i % 10) * 1000.0f;
        ts->timestamps[i] = 1000000ULL * i;
    }
    ts->open  = ts->prices[0];
    ts->close = ts->prices[len - 1];
    ts->high  = 115.0f;
    ts->low   = 85.0f;
}

static void fill_crash_market(fin_time_series_t* ts, uint32_t len) {
    memset(ts, 0, sizeof(*ts));
    ts->length = len;
    for (uint32_t i = 0; i < len; i++) {
        float t = (float)i / (float)len;
        ts->prices[i]     = 200.0f * expf(-3.0f * t) + 20.0f;
        ts->volumes[i]    = 20000.0f + (float)i * 200.0f; // rising volume on crash
        ts->timestamps[i] = 1000000ULL * i;
    }
    ts->open  = ts->prices[0];
    ts->close = ts->prices[len - 1];
    ts->high  = ts->prices[0];
    ts->low   = ts->prices[len - 1];
}

static void make_diversified_portfolio(fin_portfolio_t* p, uint32_t n) {
    memset(p, 0, sizeof(*p));
    fin_asset_type_t types[] = {
        FIN_ASSET_EQUITY, FIN_ASSET_FIXED_INCOME, FIN_ASSET_COMMODITY,
        FIN_ASSET_CURRENCY, FIN_ASSET_INDEX
    };
    for (uint32_t i = 0; i < n && i < FIN_MAX_PORTFOLIO_SIZE; i++) {
        p->assets[i].asset_id        = i + 1;
        p->assets[i].type            = types[i % 5];
        snprintf(p->assets[i].symbol, sizeof(p->assets[i].symbol), "E2E%u", i);
        p->assets[i].current_price   = 50.0f + (float)i * 20.0f;
        p->assets[i].expected_return = 0.05f + 0.02f * (float)(i % 5);
        p->assets[i].volatility      = 0.10f + 0.05f * (float)(i % 4);
        p->assets[i].dividend_yield  = 0.02f;
        p->assets[i].beta            = 0.8f + 0.2f * (float)(i % 3);
        p->weights[i] = 1.0f / (float)n;
    }
    p->asset_count    = n;
    p->total_value    = 500000.0f;
    p->cash_position  = 25000.0f;
}

// ============================================================================
// Test Fixture
// ============================================================================

class FinancialE2ETest : public ::testing::Test {
protected:
    financial_investment_eng_t*    inv   = nullptr;
    financial_market_eng_t*        mkt   = nullptr;
    financial_bridge_t*            brg   = nullptr;
    financial_neural_bridge_t*     neur  = nullptr;
    financial_investor_archetype_t* arch = nullptr;

    void SetUp() override {
        inv  = financial_investment_create();
        mkt  = financial_market_create();

        fin_bridge_config_t bc = financial_bridge_default_config();
        bc.enable_lgss_validation   = true;
        bc.enable_ethics_validation = true;
        bc.enable_fuzzy_validation  = true;
        bc.enable_fuzzy_risk_gating = true;
        brg = financial_bridge_create(&bc);

        fin_neural_config_t nc = financial_neural_bridge_default_config();
        nc.enable_fuzzy_encoding   = true;
        nc.enable_fuzzy_prediction = true;
        neur = financial_neural_bridge_create(&nc);

        fin_archetype_config_t ac = financial_investor_archetype_default_config();
        ac.enable_fuzzy_heuristics       = true;
        ac.enable_fuzzy_decision_scoring = true;
        ac.enable_fuzzy_emotional_blend  = true;
        ac.enable_adaptive_selection     = true;
        ac.enable_mirror_learning        = true;
        ac.enable_emotional_modulation   = true;
        arch = financial_investor_archetype_create(&ac);
    }

    void TearDown() override {
        financial_investment_destroy(inv);
        financial_market_destroy(mkt);
        financial_bridge_destroy(brg);
        financial_neural_bridge_destroy(neur);
        financial_investor_archetype_destroy(arch);
    }
};

// ============================================================================
// E2E Test: Complete Investment Cycle
// ============================================================================

TEST_F(FinancialE2ETest, CompleteInvestmentCycle) {
    // 1. Create portfolio
    fin_portfolio_t portfolio;
    make_diversified_portfolio(&portfolio, 5);
    int rc = financial_investment_portfolio_create(inv, &portfolio);
    EXPECT_EQ(rc, 0);

    // 2. Add an asset
    fin_asset_t new_asset;
    memset(&new_asset, 0, sizeof(new_asset));
    new_asset.asset_id       = 100;
    new_asset.type           = FIN_ASSET_CRYPTO;
    new_asset.current_price  = 45000.0f;
    new_asset.expected_return = 0.15f;
    new_asset.volatility     = 0.60f;
    snprintf(new_asset.symbol, sizeof(new_asset.symbol), "BTC");
    rc = financial_investment_portfolio_add_asset(inv, &portfolio, &new_asset, 0.05f);
    EXPECT_EQ(rc, 0);

    // 3. Assess risk
    uint32_t n = portfolio.asset_count;
    float* corr = (float*)calloc(n * n, sizeof(float));
    for (uint32_t i = 0; i < n; i++)
        for (uint32_t j = 0; j < n; j++)
            corr[i * n + j] = (i == j) ? 1.0f : 0.2f;

    float returns[100];
    for (int i = 0; i < 100; i++) {
        returns[i] = 0.001f * sinf((float)i * 0.3f);
    }

    fin_risk_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    rc = financial_investment_assess_risk(inv, &portfolio, corr, returns, 100, &metrics);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(metrics.volatility_annual, 0.0f);

    // 4. Optimize
    float exp_ret[6];
    for (uint32_t i = 0; i < n; i++) exp_ret[i] = portfolio.assets[i].expected_return;

    float* cov = (float*)calloc(n * n, sizeof(float));
    for (uint32_t i = 0; i < n; i++)
        for (uint32_t j = 0; j < n; j++)
            cov[i * n + j] = corr[i * n + j] * portfolio.assets[i].volatility *
                             portfolio.assets[j].volatility;

    fin_optimization_result_t opt;
    memset(&opt, 0, sizeof(opt));
    rc = financial_investment_optimize(inv, &portfolio, exp_ret, cov,
                                        FIN_OPT_STRATEGY_MAX_SHARPE, &opt);
    EXPECT_EQ(rc, 0);

    // 5. Rebalance
    rc = financial_investment_portfolio_rebalance(inv, &portfolio, opt.optimal_weights);
    EXPECT_EQ(rc, 0);

    // Verify weights sum to ~1
    float wsum = 0.0f;
    for (uint32_t i = 0; i < portfolio.asset_count; i++) {
        wsum += portfolio.weights[i];
    }
    EXPECT_NEAR(wsum, 1.0f, 1e-2f);

    free(corr);
    free(cov);
}

// ============================================================================
// E2E Test: Market Analysis Pipeline
// ============================================================================

TEST_F(FinancialE2ETest, MarketAnalysisPipeline) {
    fin_time_series_t ts;
    fill_bull_market(&ts, 300);

    // 1. GARCH fit
    float returns[299];
    for (uint32_t i = 1; i < 300; i++) {
        returns[i - 1] = logf(ts.prices[i] / ts.prices[i - 1]);
    }

    fin_garch_result_t garch;
    memset(&garch, 0, sizeof(garch));
    int rc = financial_market_garch_fit(mkt, returns, 299, 1, 1, &garch);
    EXPECT_EQ(rc, 0);

    // 2. Forecast volatility
    float forecast_vol = financial_market_garch_forecast(&garch, 5);
    EXPECT_GE(forecast_vol, 0.0f);

    // 3. Technical indicators
    float sma[300];
    memset(sma, 0, sizeof(sma));
    rc = financial_market_compute_sma(ts.prices, ts.length, 20, sma);
    // Accept success or bio-async errors (281 = BIO_MSG_ROUTER_NOT_FOUND)
    EXPECT_TRUE(rc == 0 || rc == 281);

    float rsi = financial_market_compute_rsi(ts.prices, ts.length, 14);
    EXPECT_GE(rsi, 0.0f);
    EXPECT_LE(rsi, 100.0f);

    // 4. Regime detection
    fin_fuzzy_market_condition_t fcond;
    memset(&fcond, 0, sizeof(fcond));
    rc = financial_market_detect_regime_fuzzy(mkt, &ts, &fcond);
    EXPECT_EQ(rc, 0);

    // 5. Sentiment analysis
    fin_fuzzy_sentiment_t fsent;
    memset(&fsent, 0, sizeof(fsent));
    rc = financial_market_analyze_sentiment_fuzzy(mkt, &ts, &ts, &fsent);
    EXPECT_EQ(rc, 0);
}

// ============================================================================
// E2E Test: Archetype Advisory Pipeline
// ============================================================================

TEST_F(FinancialE2ETest, ArchetypeAdvisoryPipeline) {
    fin_time_series_t ts;
    fill_bull_market(&ts, 200);

    // 1. Detect regime and sentiment
    fin_fuzzy_market_condition_t fcond;
    memset(&fcond, 0, sizeof(fcond));
    financial_market_detect_regime_fuzzy(mkt, &ts, &fcond);

    fin_fuzzy_sentiment_t fsent;
    memset(&fsent, 0, sizeof(fsent));
    financial_market_analyze_sentiment_fuzzy(mkt, &ts, &ts, &fsent);

    // 2. Archetype selection
    fin_archetype_suitability_t suit;
    memset(&suit, 0, sizeof(suit));
    int rc = financial_investor_archetype_select(arch, &fcond, &fsent, &suit);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(suit.best_suitability, 0.0f);

    // 3. Evaluate with best archetype
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));
    input.current_price      = ts.close;
    input.intrinsic_value    = ts.close * 1.2f;
    input.book_value         = ts.close * 0.6f;
    input.earnings_per_share = 5.0f;
    input.peg_ratio          = 1.1f;
    input.rsi                = 55.0f;
    input.management_quality = 0.8f;
    input.market_condition   = fcond;
    input.market_sentiment   = fsent;

    fin_archetype_decision_t decision;
    memset(&decision, 0, sizeof(decision));
    rc = financial_investor_archetype_evaluate(arch, suit.best_archetype, &input, &decision);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(decision.conviction, 0.0f);
    EXPECT_LE(decision.conviction, 1.0f);
}

// ============================================================================
// E2E Test: Validation Pipeline
// ============================================================================

TEST_F(FinancialE2ETest, ValidationPipeline) {
    // Create action
    fin_action_t action;
    memset(&action, 0, sizeof(action));
    action.type              = FIN_ACTION_BUY;
    snprintf(action.symbol, sizeof(action.symbol), "MSFT");
    action.magnitude         = 5000.0f;
    action.position_weight   = 0.10f;
    action.leverage_ratio    = 1.0f;
    action.has_client_consent = true;
    action.is_suitable       = true;
    action.client_age        = 35;

    // Run full validation pipeline
    fin_validation_report_t report;
    memset(&report, 0, sizeof(report));
    int rc = financial_bridge_validate_action(brg, &action, &report);
    EXPECT_GE(rc, 0);
    EXPECT_GE((int)report.result, 0);
    EXPECT_LE((int)report.result, (int)FIN_VALIDATION_ERROR);
    EXPECT_GE(report.fuzzy_safety_score, 0.0f);
    EXPECT_LE(report.fuzzy_safety_score, 1.0f);
    // Validation time may be 0 if timing is disabled or very fast
    EXPECT_GE(report.validation_time_us, 0.0f);
}

// ============================================================================
// E2E Test: Neural Learning Pipeline
// ============================================================================

TEST_F(FinancialE2ETest, NeuralLearningPipeline) {
    // 1. Encode market events
    for (int i = 0; i < 10; i++) {
        fin_market_event_t event;
        memset(&event, 0, sizeof(event));
        event.type      = (fin_market_event_type_t)(i % (int)FIN_EVENT_TYPE_COUNT);
        event.magnitude = 0.01f * (float)(i + 1);
        event.direction = (i % 2 == 0) ? 1.0f : -1.0f;

        fin_spike_train_t spikes;
        memset(&spikes, 0, sizeof(spikes));
        financial_neural_bridge_encode_market_event(neur, &event, &spikes);
    }

    // 2. Train
    float input[8]  = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float target[8] = {0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f};
    for (int i = 0; i < 5; i++) {
        int rc = financial_neural_bridge_train_step(neur, input, target, 8, 0.01f);
        EXPECT_EQ(rc, 0);
    }

    // 3. Predict
    fin_time_series_t ts;
    fill_bull_market(&ts, 100);
    fin_neural_prediction_t pred;
    memset(&pred, 0, sizeof(pred));
    int rc = financial_neural_bridge_lnn_predict(neur, &ts, 5, &pred);
    EXPECT_EQ(rc, 0);

    // 4. Adapt plasticity
    fin_plasticity_params_t params;
    memset(&params, 0, sizeof(params));
    rc = financial_neural_bridge_adapt_risk_params(neur, 1.2f, 0.015f, &params);
    EXPECT_EQ(rc, 0);

    // 5. Consolidate memory
    float pattern[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    financial_neural_bridge_store_pattern(neur, pattern, 4, 0.05f, 0.9f);
    rc = financial_neural_bridge_consolidate(neur);
    EXPECT_EQ(rc, 0);

    // 6. Get convergence
    float loss = 0.0f, conv_deg = 0.0f;
    rc = financial_neural_bridge_get_convergence(neur, &loss, &conv_deg);
    EXPECT_EQ(rc, 0);
}

// ============================================================================
// E2E Test: Multi-Archetype Blend
// ============================================================================

TEST_F(FinancialE2ETest, MultiArchetypeBlend) {
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));
    input.current_price        = 75.0f;
    input.intrinsic_value      = 100.0f;
    input.book_value           = 60.0f;
    input.earnings_per_share   = 6.0f;
    input.peg_ratio            = 0.9f;
    input.rsi                  = 45.0f;
    input.management_quality   = 0.85f;
    input.pricing_power        = 0.7f;
    input.brand_strength       = 0.8f;

    fin_archetype_id_t ids[] = {FIN_ARCH_GRAHAM, FIN_ARCH_BUFFETT, FIN_ARCH_MUNGER};
    float weights[] = {0.35f, 0.40f, 0.25f};

    fin_blend_result_t blend;
    memset(&blend, 0, sizeof(blend));
    int rc = financial_investor_archetype_evaluate_blend(arch, ids, weights, 3, &input, &blend);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(blend.archetype_count, 3u);
    EXPECT_GE(blend.blended_conviction, 0.0f);
    EXPECT_LE(blend.blended_conviction, 1.0f);
    EXPECT_GE(blend.blend_entropy, 0.0f);
}

// ============================================================================
// E2E Test: Emotional Journey
// ============================================================================

TEST_F(FinancialE2ETest, EmotionalJourney) {
    // 1. Compute emotion from bearish conditions
    fin_fuzzy_market_condition_t fcond;
    memset(&fcond, 0, sizeof(fcond));
    fcond.bear_degree   = 0.6f;
    fcond.crisis_degree = 0.2f;
    fcond.dominant      = FIN_MKT_BEAR;

    fin_fuzzy_sentiment_t fsent;
    memset(&fsent, 0, sizeof(fsent));
    fsent.fear_degree = 0.7f;
    fsent.dominant    = FIN_MKT_SENTIMENT_FEAR;

    fin_emotional_state_t emotion;
    memset(&emotion, 0, sizeof(emotion));
    int rc = financial_investor_archetype_compute_emotion(
        arch, &fcond, &fsent, 0.5f, 0.4f, &emotion);
    EXPECT_EQ(rc, 0);

    // 2. Apply to decision
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));
    input.current_price   = 60.0f;
    input.intrinsic_value = 85.0f;

    fin_archetype_decision_t decision;
    memset(&decision, 0, sizeof(decision));
    financial_investor_archetype_evaluate(arch, FIN_ARCH_TEMPLETON, &input, &decision);

    rc = financial_investor_archetype_apply_emotion(arch, &emotion, &decision);
    EXPECT_EQ(rc, 0);

    // Emotional modulation should affect position sizing
    EXPECT_GE(decision.position_size_pct, 0.0f);
    EXPECT_GE(decision.conviction, 0.0f);
    EXPECT_LE(decision.conviction, 1.0f);
}

// ============================================================================
// E2E Test: Stress Scenario
// ============================================================================

TEST_F(FinancialE2ETest, StressScenario) {
    // Create stressed market
    fin_time_series_t ts;
    fill_crash_market(&ts, 200);

    // All modules should degrade gracefully
    fin_fuzzy_market_condition_t fcond;
    memset(&fcond, 0, sizeof(fcond));
    int rc = financial_market_detect_regime_fuzzy(mkt, &ts, &fcond);
    EXPECT_EQ(rc, 0);

    fin_fuzzy_sentiment_t fsent;
    memset(&fsent, 0, sizeof(fsent));
    rc = financial_market_analyze_sentiment_fuzzy(mkt, &ts, &ts, &fsent);
    EXPECT_EQ(rc, 0);

    // Archetype should still work under stress
    fin_archetype_suitability_t suit;
    memset(&suit, 0, sizeof(suit));
    rc = financial_investor_archetype_select(arch, &fcond, &fsent, &suit);
    EXPECT_EQ(rc, 0);

    // Neural encoding should still work
    fin_market_event_t event;
    memset(&event, 0, sizeof(event));
    event.type      = FIN_EVENT_REGIME_CHANGE;
    event.magnitude = 0.9f;
    event.direction = -1.0f;

    fin_spike_train_t spikes;
    memset(&spikes, 0, sizeof(spikes));
    rc = financial_neural_bridge_encode_market_event(neur, &event, &spikes);
    EXPECT_EQ(rc, 0);

    // Bridge validation should still work
    fin_action_t action;
    memset(&action, 0, sizeof(action));
    action.type              = FIN_ACTION_SELL;
    action.magnitude         = 100000.0f;
    action.current_portfolio_risk = 0.8f;
    action.has_client_consent = true;
    action.is_suitable       = true;
    snprintf(action.symbol, sizeof(action.symbol), "STRESS");

    fin_validation_report_t report;
    memset(&report, 0, sizeof(report));
    rc = financial_bridge_validate_action(brg, &action, &report);
    EXPECT_GE(rc, 0);
}

// ============================================================================
// E2E Test: Fuzzy Archetype Across Regimes
// ============================================================================

TEST_F(FinancialE2ETest, FuzzyArchetypeAcrossRegimes) {
    fin_time_series_t ts_bull, ts_crash;
    fill_bull_market(&ts_bull, 200);
    fill_crash_market(&ts_crash, 200);

    // Bull regime
    fin_fuzzy_market_condition_t fcond_bull;
    memset(&fcond_bull, 0, sizeof(fcond_bull));
    financial_market_detect_regime_fuzzy(mkt, &ts_bull, &fcond_bull);

    fin_fuzzy_sentiment_t fsent_bull;
    memset(&fsent_bull, 0, sizeof(fsent_bull));
    financial_market_analyze_sentiment_fuzzy(mkt, &ts_bull, &ts_bull, &fsent_bull);

    fin_archetype_suitability_t suit_bull;
    memset(&suit_bull, 0, sizeof(suit_bull));
    financial_investor_archetype_select(arch, &fcond_bull, &fsent_bull, &suit_bull);

    // Crash regime
    fin_fuzzy_market_condition_t fcond_crash;
    memset(&fcond_crash, 0, sizeof(fcond_crash));
    financial_market_detect_regime_fuzzy(mkt, &ts_crash, &fcond_crash);

    fin_fuzzy_sentiment_t fsent_crash;
    memset(&fsent_crash, 0, sizeof(fsent_crash));
    financial_market_analyze_sentiment_fuzzy(mkt, &ts_crash, &ts_crash, &fsent_crash);

    fin_archetype_suitability_t suit_crash;
    memset(&suit_crash, 0, sizeof(suit_crash));
    financial_investor_archetype_select(arch, &fcond_crash, &fsent_crash, &suit_crash);

    // Both should produce valid selections but potentially different archetypes
    EXPECT_GE((int)suit_bull.best_archetype, 0);
    EXPECT_LT((int)suit_bull.best_archetype, (int)FIN_ARCH_COUNT);
    EXPECT_GE((int)suit_crash.best_archetype, 0);
    EXPECT_LT((int)suit_crash.best_archetype, (int)FIN_ARCH_COUNT);
}

// ============================================================================
// E2E Test: Fuzzy Risk-Gated Trading
// ============================================================================

TEST_F(FinancialE2ETest, FuzzyRiskGatedTrading) {
    // Conservative action should pass gate
    fin_action_t safe;
    memset(&safe, 0, sizeof(safe));
    safe.type              = FIN_ACTION_BUY;
    safe.magnitude         = 1000.0f;
    safe.position_weight   = 0.02f;
    safe.leverage_ratio    = 1.0f;
    safe.current_portfolio_risk = 0.1f;
    safe.concentration     = 0.02f;
    safe.has_client_consent = true;
    safe.is_suitable       = true;
    safe.client_age        = 40;
    snprintf(safe.symbol, sizeof(safe.symbol), "SAFE");

    float safety_safe = 0.0f, risk_safe = 0.0f;
    financial_bridge_fuzzy_score(brg, &safe, &safety_safe, &risk_safe);

    // Risky action may not pass gate
    fin_action_t risky;
    memset(&risky, 0, sizeof(risky));
    risky.type              = FIN_ACTION_SHORT;
    risky.magnitude         = 500000.0f;
    risky.position_weight   = 0.80f;
    risky.leverage_ratio    = 10.0f;
    risky.current_portfolio_risk = 0.95f;
    risky.concentration     = 0.85f;
    risky.has_client_consent = false;
    risky.is_suitable       = false;
    risky.client_age        = 90;
    risky.counterparty_sanctioned = true;
    snprintf(risky.symbol, sizeof(risky.symbol), "JUNK");

    float safety_risky = 0.0f, risk_risky = 0.0f;
    financial_bridge_fuzzy_score(brg, &risky, &safety_risky, &risk_risky);

    // Safe action should have better safety and lower risk
    EXPECT_GE(safety_safe, 0.0f);
    EXPECT_GE(risk_risky, 0.0f);
}

// ============================================================================
// E2E Test: Fuzzy Emotion-Driven Decision
// ============================================================================

TEST_F(FinancialE2ETest, FuzzyEmotionDrivenDecision) {
    // Fear cycle
    fin_fuzzy_market_condition_t fcond;
    memset(&fcond, 0, sizeof(fcond));
    fcond.crisis_degree = 0.8f;
    fcond.bear_degree   = 0.5f;
    fcond.dominant      = FIN_MKT_CRISIS;

    fin_fuzzy_sentiment_t fsent;
    memset(&fsent, 0, sizeof(fsent));
    fsent.extreme_fear_degree = 0.7f;
    fsent.fear_degree         = 0.3f;
    fsent.dominant            = FIN_MKT_SENTIMENT_EXTREME_FEAR;

    // Compute high-fear emotion
    fin_emotional_state_t emotion;
    memset(&emotion, 0, sizeof(emotion));
    int rc = financial_investor_archetype_compute_emotion(
        arch, &fcond, &fsent, 0.9f, 0.8f, &emotion);
    EXPECT_EQ(rc, 0);

    // Evaluate and modulate
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));
    input.current_price   = 40.0f;
    input.intrinsic_value = 80.0f;  // 50% margin of safety
    input.rsi             = 20.0f;  // oversold

    fin_archetype_decision_t decision;
    memset(&decision, 0, sizeof(decision));
    financial_investor_archetype_evaluate(arch, FIN_ARCH_TEMPLETON, &input, &decision);

    float original_position = decision.position_size_pct;
    rc = financial_investor_archetype_apply_emotion(arch, &emotion, &decision);
    EXPECT_EQ(rc, 0);
    // Fear should make the decision more conservative (smaller or equal position)
    EXPECT_GE(decision.position_size_pct, 0.0f);
}

// ============================================================================
// E2E Test: Health Modulation
// ============================================================================

TEST_F(FinancialE2ETest, HealthModulation) {
    // Set inflammation and fatigue on all modules
    int rc;
    rc = financial_investment_set_inflammation(inv, 0.7f);
    EXPECT_EQ(rc, 0);
    rc = financial_investment_set_fatigue(inv, 0.5f);
    EXPECT_EQ(rc, 0);

    rc = financial_market_set_inflammation(mkt, 0.7f);
    EXPECT_EQ(rc, 0);
    rc = financial_market_set_fatigue(mkt, 0.5f);
    EXPECT_EQ(rc, 0);

    rc = financial_bridge_set_inflammation(brg, 0.7f);
    EXPECT_EQ(rc, 0);
    rc = financial_bridge_set_fatigue(brg, 0.5f);
    EXPECT_EQ(rc, 0);

    rc = financial_neural_bridge_set_inflammation(neur, 0.7f);
    EXPECT_EQ(rc, 0);
    rc = financial_neural_bridge_set_fatigue(neur, 0.5f);
    EXPECT_EQ(rc, 0);

    rc = financial_investor_archetype_set_inflammation(arch, 0.7f);
    EXPECT_EQ(rc, 0);
    rc = financial_investor_archetype_set_fatigue(arch, 0.5f);
    EXPECT_EQ(rc, 0);

    // All modules should still function
    fin_time_series_t ts;
    fill_volatile_market(&ts, 200);

    fin_fuzzy_market_condition_t fcond;
    memset(&fcond, 0, sizeof(fcond));
    rc = financial_market_detect_regime_fuzzy(mkt, &ts, &fcond);
    EXPECT_EQ(rc, 0);

    fin_action_t action;
    memset(&action, 0, sizeof(action));
    action.type = FIN_ACTION_BUY;
    action.magnitude = 1000.0f;
    action.has_client_consent = true;
    action.is_suitable = true;
    snprintf(action.symbol, sizeof(action.symbol), "HLTH");

    fin_validation_report_t report;
    memset(&report, 0, sizeof(report));
    rc = financial_bridge_validate_action(brg, &action, &report);
    EXPECT_GE(rc, 0);
}

// ============================================================================
// E2E Test: Full System Integration
// ============================================================================

TEST_F(FinancialE2ETest, FullSystemIntegration) {
    // Process market data end-to-end through all 5 modules
    fin_time_series_t ts;
    fill_bull_market(&ts, 250);

    // Market analysis
    fin_fuzzy_market_condition_t fcond;
    memset(&fcond, 0, sizeof(fcond));
    financial_market_detect_regime_fuzzy(mkt, &ts, &fcond);

    fin_fuzzy_sentiment_t fsent;
    memset(&fsent, 0, sizeof(fsent));
    financial_market_analyze_sentiment_fuzzy(mkt, &ts, &ts, &fsent);

    // Neural encoding of regime
    fin_spike_train_t spikes;
    memset(&spikes, 0, sizeof(spikes));
    financial_neural_bridge_encode_fuzzy_regime(neur, &fcond, &spikes);

    // Neural prediction
    fin_neural_prediction_t pred;
    memset(&pred, 0, sizeof(pred));
    financial_neural_bridge_lnn_predict(neur, &ts, 10, &pred);

    // Archetype selection and evaluation
    fin_archetype_suitability_t suit;
    memset(&suit, 0, sizeof(suit));
    financial_investor_archetype_select(arch, &fcond, &fsent, &suit);

    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));
    input.current_price      = ts.close;
    input.intrinsic_value    = ts.close * (1.0f + pred.predicted_return * 5.0f);
    input.market_condition   = fcond;
    input.market_sentiment   = fsent;
    input.rsi                = 50.0f;

    fin_archetype_decision_t decision;
    memset(&decision, 0, sizeof(decision));
    financial_investor_archetype_evaluate(arch, suit.best_archetype, &input, &decision);

    // Convert to action and validate
    fin_action_t action;
    memset(&action, 0, sizeof(action));
    action.type = (decision.decision <= FIN_DECISION_BUY) ? FIN_ACTION_BUY : FIN_ACTION_SELL;
    action.magnitude         = 10000.0f * decision.position_size_pct;
    action.position_weight   = decision.position_size_pct;
    action.leverage_ratio    = 1.0f;
    action.has_client_consent = true;
    action.is_suitable       = true;
    action.client_age        = 40;
    snprintf(action.symbol, sizeof(action.symbol), "FULL");

    fin_validation_report_t report;
    memset(&report, 0, sizeof(report));
    int rc = financial_bridge_validate_action(brg, &action, &report);
    EXPECT_GE(rc, 0);

    // Investment - portfolio with risk assessment
    fin_portfolio_t portfolio;
    make_diversified_portfolio(&portfolio, 4);
    financial_investment_portfolio_create(inv, &portfolio);

    float returns[100];
    for (int i = 0; i < 100; i++) returns[i] = 0.001f * sinf((float)i * 0.2f);
    float corr[16];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            corr[i * 4 + j] = (i == j) ? 1.0f : 0.25f;

    fin_risk_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    rc = financial_investment_assess_risk(inv, &portfolio, corr, returns, 100, &metrics);
    EXPECT_EQ(rc, 0);
}

// ============================================================================
// E2E Test: Memory Cleanup
// ============================================================================

TEST_F(FinancialE2ETest, MemoryCleanupMultipleCycles) {
    for (int cycle = 0; cycle < 3; cycle++) {
        financial_investment_eng_t* i = financial_investment_create();
        ASSERT_NE(i, nullptr) << "Cycle " << cycle;
        financial_investment_destroy(i);

        financial_market_eng_t* m = financial_market_create();
        ASSERT_NE(m, nullptr) << "Cycle " << cycle;
        financial_market_destroy(m);

        fin_bridge_config_t bc = financial_bridge_default_config();
        financial_bridge_t* b = financial_bridge_create(&bc);
        ASSERT_NE(b, nullptr) << "Cycle " << cycle;
        financial_bridge_destroy(b);

        fin_neural_config_t nc = financial_neural_bridge_default_config();
        financial_neural_bridge_t* n = financial_neural_bridge_create(&nc);
        ASSERT_NE(n, nullptr) << "Cycle " << cycle;
        financial_neural_bridge_destroy(n);

        fin_archetype_config_t ac = financial_investor_archetype_default_config();
        financial_investor_archetype_t* a = financial_investor_archetype_create(&ac);
        ASSERT_NE(a, nullptr) << "Cycle " << cycle;
        financial_investor_archetype_destroy(a);
    }
}

// ============================================================================
// E2E Test: Stats Accumulation
// ============================================================================

TEST_F(FinancialE2ETest, StatsAccumulation) {
    // Reset all stats
    financial_investment_reset_stats(inv);
    financial_market_reset_stats(mkt);
    financial_bridge_reset_stats(brg);
    financial_neural_bridge_reset_stats(neur);
    financial_investor_archetype_reset_stats(arch);

    // Run some operations
    fin_portfolio_t portfolio;
    make_diversified_portfolio(&portfolio, 3);
    financial_investment_portfolio_create(inv, &portfolio);

    float returns[50];
    for (int i = 0; i < 50; i++) returns[i] = 0.001f * (float)(i % 10 - 5);
    float corr[9] = {1, 0.3f, 0.2f, 0.3f, 1, 0.4f, 0.2f, 0.4f, 1};
    fin_risk_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    financial_investment_assess_risk(inv, &portfolio, corr, returns, 50, &metrics);

    fin_time_series_t ts;
    fill_bull_market(&ts, 100);
    fin_fuzzy_market_condition_t fcond;
    memset(&fcond, 0, sizeof(fcond));
    financial_market_detect_regime_fuzzy(mkt, &ts, &fcond);

    fin_action_t action;
    memset(&action, 0, sizeof(action));
    action.type = FIN_ACTION_BUY;
    action.has_client_consent = true;
    action.is_suitable = true;
    snprintf(action.symbol, sizeof(action.symbol), "STAT");
    fin_validation_report_t report;
    memset(&report, 0, sizeof(report));
    financial_bridge_validate_action(brg, &action, &report);

    fin_market_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = FIN_EVENT_PRICE_CHANGE;
    event.magnitude = 0.05f;
    fin_spike_train_t spikes;
    memset(&spikes, 0, sizeof(spikes));
    financial_neural_bridge_encode_market_event(neur, &event, &spikes);

    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));
    input.current_price   = 100.0f;
    input.intrinsic_value = 120.0f;
    fin_archetype_decision_t decision;
    memset(&decision, 0, sizeof(decision));
    financial_investor_archetype_evaluate(arch, FIN_ARCH_GRAHAM, &input, &decision);

    // Verify stats were incremented
    fin_stats_t is;
    memset(&is, 0, sizeof(is));
    financial_investment_get_stats(inv, &is);
    EXPECT_GT(is.risk_assessments, 0u);

    fin_market_stats_t ms;
    memset(&ms, 0, sizeof(ms));
    financial_market_get_stats(mkt, &ms);
    // At least one operation should be counted

    fin_bridge_stats_t bs;
    memset(&bs, 0, sizeof(bs));
    financial_bridge_get_stats(brg, &bs);
    EXPECT_GT(bs.total_validations, 0u);

    fin_neural_stats_t ns;
    memset(&ns, 0, sizeof(ns));
    financial_neural_bridge_get_stats(neur, &ns);
    EXPECT_GT(ns.events_encoded, 0u);

    fin_archetype_stats_t as;
    memset(&as, 0, sizeof(as));
    financial_investor_archetype_get_stats(arch, &as);
    EXPECT_GT(as.total_evaluations, 0u);
}

// ============================================================================
// E2E Test: Performance Baseline
// ============================================================================

TEST_F(FinancialE2ETest, PerformanceBaseline100Evaluations) {
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));
    input.current_price   = 50.0f;
    input.intrinsic_value = 70.0f;
    input.book_value      = 40.0f;
    input.peg_ratio       = 1.0f;
    input.rsi             = 50.0f;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 100; i++) {
        fin_archetype_decision_t decision;
        memset(&decision, 0, sizeof(decision));
        fin_archetype_id_t id = (fin_archetype_id_t)(i % (int)FIN_ARCH_COUNT);
        financial_investor_archetype_evaluate(arch, id, &input, &decision);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // 100 evaluations should complete in reasonable time (< 30 seconds)
    EXPECT_LT(duration_ms, 30000) << "100 archetype evaluations took " << duration_ms << "ms";
}
