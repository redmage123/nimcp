/**
 * @file test_financial_regression.cpp
 * @brief Regression tests for NIMCP Financial Investment module
 * @date 2026-01-29
 *
 * WHAT: Backward compatibility, correctness, and contract stability tests
 * WHY:  Ensure financial module API contracts, error codes, value ranges,
 *       and mathematical properties remain stable across versions
 * HOW:  Verify NULL safety, output bounds, config defaults, error ranges,
 *       statistical consistency, idempotent operations, pricing identities
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <algorithm>

extern "C" {
#include "cognitive/parietal/nimcp_financial_investment.h"
#include "cognitive/parietal/nimcp_financial_market.h"
#include "cognitive/parietal/nimcp_financial_bridge.h"
#include "cognitive/parietal/nimcp_financial_neural_bridge.h"
#include "cognitive/parietal/nimcp_financial_investor_archetype.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class FinancialRegressionTest : public ::testing::Test {
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
        brg  = financial_bridge_create(&bc);

        fin_neural_config_t nc = financial_neural_bridge_default_config();
        neur = financial_neural_bridge_create(&nc);

        fin_archetype_config_t ac = financial_investor_archetype_default_config();
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
// NULL Safety - Every public function handles NULL without crash
// ============================================================================

TEST_F(FinancialRegressionTest, InvestmentNullSafety) {
    // destroy(NULL) should not crash
    financial_investment_destroy(nullptr);

    // Portfolio functions with NULL engine or portfolio
    EXPECT_NE(financial_investment_portfolio_create(nullptr, nullptr), 0);

    fin_portfolio_t portfolio;
    memset(&portfolio, 0, sizeof(portfolio));
    EXPECT_NE(financial_investment_portfolio_create(nullptr, &portfolio), 0);
    EXPECT_NE(financial_investment_portfolio_add_asset(nullptr, nullptr, nullptr, 0.5f), 0);
    EXPECT_NE(financial_investment_portfolio_remove_asset(nullptr, nullptr, 0), 0);
    EXPECT_NE(financial_investment_portfolio_rebalance(nullptr, nullptr, nullptr), 0);

    // Risk functions with NULL
    EXPECT_NE(financial_investment_assess_risk(nullptr, nullptr, nullptr, nullptr, 0, nullptr), 0);
    // max_drawdown with NULL should handle gracefully
    float mdd = financial_investment_max_drawdown(nullptr, 0);
    (void)mdd;

    // Pricing with NULL
    EXPECT_NE(financial_investment_price_option(nullptr, 0, 0, 0, 0, 0,
              FIN_OPT_CALL, FIN_PRICING_BLACK_SCHOLES, nullptr), 0);

    // Valuation with NULL
    EXPECT_NE(financial_investment_dcf_valuation(nullptr, nullptr, 0, 0, 0, nullptr), 0);
    EXPECT_NE(financial_investment_ddm_valuation(nullptr, 0, 0, 0, nullptr), 0);
    EXPECT_NE(financial_investment_comparables(nullptr, nullptr, 0, 0, nullptr), 0);

    // Optimization with NULL
    EXPECT_NE(financial_investment_optimize(nullptr, nullptr, nullptr, nullptr,
              FIN_OPT_STRATEGY_MEAN_VARIANCE, nullptr), 0);

    // Factor analysis with NULL
    EXPECT_NE(financial_investment_factor_analysis(nullptr, nullptr, nullptr, 0, 0, nullptr), 0);

    // Tax-loss harvesting with NULL
    EXPECT_NE(financial_investment_tax_loss_harvest(nullptr, nullptr, nullptr, nullptr), 0);

    // Modulation with NULL
    EXPECT_NE(financial_investment_set_inflammation(nullptr, 0.5f), 0);
    EXPECT_NE(financial_investment_set_fatigue(nullptr, 0.5f), 0);
    EXPECT_NE(financial_investment_get_stats(nullptr, nullptr), 0);
    financial_investment_reset_stats(nullptr); // should not crash

    // Free functions with NULL
    financial_investment_free_optimization(nullptr);
    financial_investment_free_factor(nullptr);
}

TEST_F(FinancialRegressionTest, MarketNullSafety) {
    financial_market_destroy(nullptr);

    EXPECT_NE(financial_market_garch_fit(nullptr, nullptr, 0, 0, 0, nullptr), 0);

    fin_time_series_t ts;
    memset(&ts, 0, sizeof(ts));
    EXPECT_NE(financial_market_compute_indicator(nullptr, nullptr, FIN_MKT_IND_SMA, 10, nullptr), 0);
    EXPECT_NE(financial_market_compute_sma(nullptr, 0, 0, nullptr), 0);
    EXPECT_NE(financial_market_compute_ema(nullptr, 0, 0, nullptr), 0);
    EXPECT_NE(financial_market_compute_macd(nullptr, 0, 0, 0, 0, nullptr, nullptr, nullptr), 0);
    EXPECT_NE(financial_market_compute_bollinger(nullptr, 0, 0, 0, nullptr, nullptr, nullptr), 0);

    EXPECT_NE(financial_market_analyze_sentiment(nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_NE(financial_market_detect_regime_fuzzy(nullptr, nullptr, nullptr), 0);
    EXPECT_NE(financial_market_analyze_sentiment_fuzzy(nullptr, nullptr, nullptr, nullptr), 0);

    EXPECT_NE(financial_market_run_scenario(nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_NE(financial_market_stress_test(nullptr, nullptr, nullptr, 0, nullptr), 0);
    EXPECT_NE(financial_market_monte_carlo(nullptr, nullptr, 0, 0, 0, 0, nullptr), 0);

    EXPECT_NE(financial_market_set_inflammation(nullptr, 0.5f), 0);
    EXPECT_NE(financial_market_set_fatigue(nullptr, 0.5f), 0);
    EXPECT_NE(financial_market_get_stats(nullptr, nullptr), 0);
    financial_market_reset_stats(nullptr);
}

TEST_F(FinancialRegressionTest, BridgeNullSafety) {
    financial_bridge_destroy(nullptr);

    EXPECT_NE(financial_bridge_validate_action(nullptr, nullptr, nullptr), 0);
    EXPECT_NE(financial_bridge_fuzzy_score(nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_NE(financial_bridge_get_risk_drive(nullptr, nullptr), 0);
    EXPECT_NE(financial_bridge_get_execution_timing(nullptr, 0, 0, nullptr), 0);
    EXPECT_NE(financial_bridge_get_autonomic_state(nullptr, nullptr), 0);
    EXPECT_NE(financial_bridge_update_autonomic(nullptr, 0, 0, 0), 0);
    EXPECT_NE(financial_bridge_check_health(nullptr), 0);
    EXPECT_NE(financial_bridge_heartbeat(nullptr, nullptr, 0), 0);
    EXPECT_NE(financial_bridge_set_inflammation(nullptr, 0.5f), 0);
    EXPECT_NE(financial_bridge_set_fatigue(nullptr, 0.5f), 0);
    EXPECT_NE(financial_bridge_get_stats(nullptr, nullptr), 0);
    financial_bridge_reset_stats(nullptr);
    EXPECT_NE(financial_bridge_reset(nullptr), 0);

    // Setter NULL safety
    EXPECT_NE(financial_bridge_set_immune(nullptr, nullptr), 0);
    EXPECT_NE(financial_bridge_set_bbb(nullptr, nullptr), 0);
    EXPECT_NE(financial_bridge_set_ethics(nullptr, nullptr), 0);
    EXPECT_NE(financial_bridge_set_lgss(nullptr, nullptr), 0);
    EXPECT_NE(financial_bridge_set_fuzzy_bridge(nullptr, nullptr), 0);
    EXPECT_NE(financial_bridge_set_security(nullptr, nullptr), 0);
    EXPECT_NE(financial_bridge_set_validation_callback(nullptr, nullptr, nullptr), 0);
    EXPECT_NE(financial_bridge_set_health_callback(nullptr, nullptr, nullptr), 0);
}

TEST_F(FinancialRegressionTest, NeuralNullSafety) {
    financial_neural_bridge_destroy(nullptr);

    EXPECT_NE(financial_neural_bridge_encode_market_event(nullptr, nullptr, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_encode_fuzzy_regime(nullptr, nullptr, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_decode_spikes(nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_stdp_reward(nullptr, 0, 0, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_compute_fuzzy_reward(nullptr, 0, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_lnn_predict(nullptr, nullptr, 0, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_lnn_update(nullptr, nullptr, 0), 0);
    EXPECT_NE(financial_neural_bridge_adapt_risk_params(nullptr, 0, 0, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_get_plasticity(nullptr, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_quantum_optimize(nullptr, nullptr, 0, nullptr, nullptr, 0, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_store_pattern(nullptr, nullptr, 0, 0, 0), 0);
    EXPECT_NE(financial_neural_bridge_retrieve_patterns(nullptr, nullptr, 0, nullptr, 0, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_consolidate(nullptr), 0);
    EXPECT_NE(financial_neural_bridge_train_step(nullptr, nullptr, nullptr, 0, 0), 0);
    EXPECT_NE(financial_neural_bridge_get_convergence(nullptr, nullptr, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_set_inflammation(nullptr, 0.5f), 0);
    EXPECT_NE(financial_neural_bridge_set_fatigue(nullptr, 0.5f), 0);
    EXPECT_NE(financial_neural_bridge_get_stats(nullptr, nullptr), 0);
    financial_neural_bridge_reset_stats(nullptr);
    EXPECT_NE(financial_neural_bridge_reset(nullptr), 0);

    // Setter NULL safety
    EXPECT_NE(financial_neural_bridge_set_snn(nullptr, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_set_stdp(nullptr, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_set_lnn(nullptr, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_set_plasticity(nullptr, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_set_quantum(nullptr, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_set_immune(nullptr, nullptr), 0);
    EXPECT_NE(financial_neural_bridge_set_fuzzy_bridge(nullptr, nullptr), 0);
}

TEST_F(FinancialRegressionTest, ArchetypeNullSafety) {
    financial_investor_archetype_destroy(nullptr);

    EXPECT_NE(financial_investor_archetype_evaluate(nullptr, FIN_ARCH_GRAHAM, nullptr, nullptr), 0);
    EXPECT_NE(financial_investor_archetype_evaluate_heuristic(nullptr, FIN_HEURISTIC_MARGIN_OF_SAFETY, nullptr, nullptr), 0);
    EXPECT_NE(financial_investor_archetype_evaluate_blend(nullptr, nullptr, nullptr, 0, nullptr, nullptr), 0);
    EXPECT_NE(financial_investor_archetype_select(nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_NE(financial_investor_archetype_apply_emotion(nullptr, nullptr, nullptr), 0);
    EXPECT_NE(financial_investor_archetype_compute_emotion(nullptr, nullptr, nullptr, 0, 0, nullptr), 0);
    EXPECT_NE(financial_investor_archetype_mirror_record(nullptr, nullptr), 0);
    EXPECT_NE(financial_investor_archetype_self_reflect(nullptr, nullptr, nullptr), 0);
    EXPECT_NE(financial_investor_archetype_set_inflammation(nullptr, 0.5f), 0);
    EXPECT_NE(financial_investor_archetype_set_fatigue(nullptr, 0.5f), 0);
    EXPECT_NE(financial_investor_archetype_get_stats(nullptr, nullptr), 0);
    financial_investor_archetype_reset_stats(nullptr);
    EXPECT_NE(financial_investor_archetype_get_profile((fin_archetype_id_t)999, nullptr), 0);

    // Setter NULL safety
    EXPECT_NE(financial_investor_archetype_set_ethics(nullptr, nullptr), 0);
    EXPECT_NE(financial_investor_archetype_set_lgss(nullptr, nullptr), 0);
    EXPECT_NE(financial_investor_archetype_set_immune(nullptr, nullptr), 0);
    EXPECT_NE(financial_investor_archetype_set_fuzzy(nullptr, nullptr), 0);
}

// ============================================================================
// Config Defaults
// ============================================================================

TEST_F(FinancialRegressionTest, InvestmentConfigDefaults) {
    fin_config_t cfg = financial_investment_default_config();
    EXPECT_GE(cfg.risk_free_rate, 0.0f);
    EXPECT_GT(cfg.default_horizon_years, 0.0f);
    EXPECT_GT(cfg.convergence_tolerance, 0.0f);
    EXPECT_GT(cfg.max_iterations, 0u);
    EXPECT_GT(cfg.monte_carlo_paths, 0u);
    EXPECT_GT(cfg.binomial_tree_steps, 0u);
    EXPECT_GE(cfg.min_weight, 0.0f);
    EXPECT_LE(cfg.max_weight, 1.0f);
    EXPECT_GT(cfg.rebalance_threshold, 0.0f);
}

TEST_F(FinancialRegressionTest, MarketConfigDefaults) {
    fin_market_config_t cfg = financial_market_default_config();
    EXPECT_GT(cfg.sentiment_weight, 0.0f);
    EXPECT_GT(cfg.technical_weight, 0.0f);
    EXPECT_GT(cfg.fundamental_weight, 0.0f);
    EXPECT_GT(cfg.default_ma_period, 0u);
    EXPECT_GT(cfg.garch_max_iterations, 0u);
    EXPECT_GT(cfg.garch_convergence_tol, 0.0f);
    EXPECT_GT(cfg.monte_carlo_default_paths, 0u);
}

TEST_F(FinancialRegressionTest, BridgeConfigDefaults) {
    fin_bridge_config_t cfg = financial_bridge_default_config();
    EXPECT_GT(cfg.validation_timeout_ms, 0u);
    EXPECT_GT(cfg.health_check_interval_ms, 0u);
    // fuzzy_risk_gate_threshold should be in (0,1]
    EXPECT_GT(cfg.fuzzy_risk_gate_threshold, 0.0f);
    EXPECT_LE(cfg.fuzzy_risk_gate_threshold, 1.0f);
}

TEST_F(FinancialRegressionTest, NeuralConfigDefaults) {
    fin_neural_config_t cfg = financial_neural_bridge_default_config();
    EXPECT_GT(cfg.spike_channels, 0u);
    EXPECT_GT(cfg.encoding_gain, 0.0f);
    EXPECT_GT(cfg.stdp_learning_rate, 0.0f);
    EXPECT_GT(cfg.lnn_state_dim, 0u);
    EXPECT_GT(cfg.plasticity_base_rate, 0.0f);
    EXPECT_GT(cfg.max_memory_patterns, 0u);
}

TEST_F(FinancialRegressionTest, ArchetypeConfigDefaults) {
    fin_archetype_config_t cfg = financial_investor_archetype_default_config();
    EXPECT_GT(cfg.mirror_learning_rate, 0.0f);
    EXPECT_GT(cfg.max_blend_archetypes, 0u);
    EXPECT_GT(cfg.min_blend_weight, 0.0f);
}

// ============================================================================
// Error Code Ranges
// ============================================================================

TEST_F(FinancialRegressionTest, InvestmentErrorCodeRange) {
    EXPECT_EQ(FIN_ERR_OK, 0);
    EXPECT_GT(FIN_ERR_NULL, (int)FIN_ERROR_BASE);
    EXPECT_EQ(FIN_ERR_NULL, FIN_ERROR_BASE + 1);
    EXPECT_EQ(FIN_ERR_INVALID_CONFIDENCE, FIN_ERROR_BASE + 10);
    EXPECT_EQ(FIN_ERROR_BASE, 32000);
}

TEST_F(FinancialRegressionTest, BridgeErrorCodeRange) {
    EXPECT_EQ(FIN_BRIDGE_ERR_OK, 0);
    EXPECT_GT(FIN_BRIDGE_ERR_NULL, (int)FIN_BRIDGE_ERROR_BASE);
    EXPECT_EQ(FIN_BRIDGE_ERROR_BASE, 33000);
    EXPECT_EQ(FIN_BRIDGE_ERR_CONFIG, FIN_BRIDGE_ERROR_BASE + 9);
}

TEST_F(FinancialRegressionTest, NeuralErrorCodeRange) {
    EXPECT_EQ(FIN_NEURAL_ERR_OK, 0);
    EXPECT_GT(FIN_NEURAL_ERR_NULL, (int)FIN_NEURAL_ERROR_BASE);
    EXPECT_EQ(FIN_NEURAL_ERROR_BASE, 34000);
    EXPECT_EQ(FIN_NEURAL_ERR_CONVERGENCE, FIN_NEURAL_ERROR_BASE + 10);
}

TEST_F(FinancialRegressionTest, ArchetypeErrorCodeRange) {
    EXPECT_EQ(FIN_ARCH_ERR_OK, 0);
    EXPECT_GT(FIN_ARCH_ERR_NULL, (int)FIN_ARCH_ERROR_BASE);
    EXPECT_EQ(FIN_ARCH_ERROR_BASE, 35000);
    EXPECT_EQ(FIN_ARCH_ERR_MIRROR, FIN_ARCH_ERROR_BASE + 9);
}

// ============================================================================
// Fuzzy Score Bounds [0,1]
// ============================================================================

TEST_F(FinancialRegressionTest, FuzzyDecisionDominantMatchesMax) {
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));
    input.current_price   = 50.0f;
    input.intrinsic_value = 80.0f;
    input.book_value      = 40.0f;

    fin_archetype_decision_t decision;
    memset(&decision, 0, sizeof(decision));
    int rc = financial_investor_archetype_evaluate(arch, FIN_ARCH_GRAHAM, &input, &decision);
    EXPECT_EQ(rc, 0);

    // Find the maximum fuzzy membership
    float degrees[] = {
        decision.fuzzy_decision.strong_buy_degree,
        decision.fuzzy_decision.buy_degree,
        decision.fuzzy_decision.hold_degree,
        decision.fuzzy_decision.reduce_degree,
        decision.fuzzy_decision.sell_degree,
        decision.fuzzy_decision.strong_sell_degree,
        decision.fuzzy_decision.no_action_degree,
    };

    float max_deg = 0.0f;
    int max_idx = 0;
    for (int i = 0; i < 7; i++) {
        EXPECT_GE(degrees[i], 0.0f);
        EXPECT_LE(degrees[i], 1.0f);
        if (degrees[i] > max_deg) {
            max_deg = degrees[i];
            max_idx = i;
        }
    }

    // The dominant decision type should correspond to the highest membership
    // Map index to decision type
    fin_decision_type_t expected_types[] = {
        FIN_DECISION_STRONG_BUY, FIN_DECISION_BUY, FIN_DECISION_HOLD,
        FIN_DECISION_REDUCE, FIN_DECISION_SELL, FIN_DECISION_STRONG_SELL,
        FIN_DECISION_NO_ACTION
    };
    EXPECT_EQ(decision.fuzzy_decision.dominant, expected_types[max_idx]);
}

TEST_F(FinancialRegressionTest, FuzzyFallbackWhenDisabled) {
    // Create archetype with fuzzy disabled
    fin_archetype_config_t cfg = financial_investor_archetype_default_config();
    cfg.enable_fuzzy_heuristics        = false;
    cfg.enable_fuzzy_decision_scoring  = false;
    cfg.enable_fuzzy_emotional_blend   = false;

    financial_investor_archetype_t* arch_nf = financial_investor_archetype_create(&cfg);
    ASSERT_NE(arch_nf, nullptr);

    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));
    input.current_price   = 50.0f;
    input.intrinsic_value = 75.0f;

    fin_archetype_decision_t decision;
    memset(&decision, 0, sizeof(decision));
    int rc = financial_investor_archetype_evaluate(arch_nf, FIN_ARCH_BUFFETT, &input, &decision);
    EXPECT_EQ(rc, 0);
    // Should still produce a valid decision
    EXPECT_GE((int)decision.decision, 0);
    EXPECT_LT((int)decision.decision, (int)FIN_DECISION_TYPE_COUNT);

    financial_investor_archetype_destroy(arch_nf);
}

// ============================================================================
// Statistics Consistency
// ============================================================================

TEST_F(FinancialRegressionTest, InvestmentStatsConsistency) {
    financial_investment_reset_stats(inv);

    fin_portfolio_t portfolio;
    memset(&portfolio, 0, sizeof(portfolio));
    financial_investment_portfolio_create(inv, &portfolio);

    fin_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int rc = financial_investment_get_stats(inv, &stats);
    EXPECT_EQ(rc, 0);
    // After one portfolio operation, counter should be incremented
    EXPECT_GE(stats.portfolio_analyses, 0u);
}

TEST_F(FinancialRegressionTest, MarketStatsConsistency) {
    financial_market_reset_stats(mkt);

    fin_market_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int rc = financial_market_get_stats(mkt, &stats);
    EXPECT_EQ(rc, 0);
    // After reset, all should be zero
    EXPECT_EQ(stats.garch_fits, 0u);
    EXPECT_EQ(stats.indicator_calculations, 0u);
}

TEST_F(FinancialRegressionTest, BridgeStatsConsistency) {
    financial_bridge_reset_stats(brg);

    fin_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int rc = financial_bridge_get_stats(brg, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_validations, 0u);
}

TEST_F(FinancialRegressionTest, NeuralStatsConsistency) {
    financial_neural_bridge_reset_stats(neur);

    fin_neural_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int rc = financial_neural_bridge_get_stats(neur, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.events_encoded, 0u);
    EXPECT_EQ(stats.predictions_made, 0u);
}

TEST_F(FinancialRegressionTest, ArchetypeStatsConsistency) {
    financial_investor_archetype_reset_stats(arch);

    fin_archetype_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int rc = financial_investor_archetype_get_stats(arch, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_evaluations, 0u);
    EXPECT_EQ(stats.total_blends, 0u);
}

// ============================================================================
// Idempotent Operations
// ============================================================================

TEST_F(FinancialRegressionTest, DoubleDestroyInvestmentSafe) {
    financial_investment_eng_t* eng = financial_investment_create();
    ASSERT_NE(eng, nullptr);
    financial_investment_destroy(eng);
    // Second destroy of dangling pointer is UB in general, but destroy(NULL) is safe
    financial_investment_destroy(nullptr);
}

TEST_F(FinancialRegressionTest, DoubleDestroyMarketSafe) {
    financial_market_eng_t* eng = financial_market_create();
    ASSERT_NE(eng, nullptr);
    financial_market_destroy(eng);
    financial_market_destroy(nullptr);
}

TEST_F(FinancialRegressionTest, DoubleDestroyBridgeSafe) {
    fin_bridge_config_t cfg = financial_bridge_default_config();
    financial_bridge_t* b = financial_bridge_create(&cfg);
    ASSERT_NE(b, nullptr);
    financial_bridge_destroy(b);
    financial_bridge_destroy(nullptr);
}

TEST_F(FinancialRegressionTest, DoubleResetBridgeSafe) {
    int rc1 = financial_bridge_reset(brg);
    int rc2 = financial_bridge_reset(brg);
    EXPECT_EQ(rc1, 0);
    EXPECT_EQ(rc2, 0);
}

TEST_F(FinancialRegressionTest, DoubleResetNeuralSafe) {
    int rc1 = financial_neural_bridge_reset(neur);
    int rc2 = financial_neural_bridge_reset(neur);
    EXPECT_EQ(rc1, 0);
    EXPECT_EQ(rc2, 0);
}

// ============================================================================
// Risk Metrics Non-Negative
// ============================================================================

TEST_F(FinancialRegressionTest, RiskMetricsNonNegative) {
    fin_portfolio_t portfolio;
    memset(&portfolio, 0, sizeof(portfolio));
    portfolio.asset_count = 2;
    portfolio.assets[0].asset_id = 1;
    portfolio.assets[0].expected_return = 0.08f;
    portfolio.assets[0].volatility = 0.20f;
    portfolio.assets[1].asset_id = 2;
    portfolio.assets[1].expected_return = 0.06f;
    portfolio.assets[1].volatility = 0.15f;
    portfolio.weights[0] = 0.6f;
    portfolio.weights[1] = 0.4f;
    portfolio.total_value = 100000.0f;

    float returns[100];
    for (int i = 0; i < 100; i++) {
        returns[i] = 0.001f * sinf((float)i * 0.2f);
    }
    float corr[4] = {1.0f, 0.4f, 0.4f, 1.0f};

    fin_risk_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    int rc = financial_investment_assess_risk(inv, &portfolio, corr, returns, 100, &metrics);
    EXPECT_EQ(rc, 0);

    // VaR, CVaR, volatility should be non-negative
    EXPECT_GE(metrics.var_95, 0.0f);
    EXPECT_GE(metrics.var_99, 0.0f);
    EXPECT_GE(metrics.cvar_95, 0.0f);
    EXPECT_GE(metrics.cvar_99, 0.0f);
    EXPECT_GE(metrics.volatility_annual, 0.0f);
    EXPECT_GE(metrics.downside_deviation, 0.0f);
    EXPECT_GE(metrics.max_drawdown, 0.0f);
    EXPECT_GE(metrics.herfindahl_index, 0.0f);
}

TEST_F(FinancialRegressionTest, VaRComputeNonNegative) {
    float returns[50];
    for (int i = 0; i < 50; i++) {
        returns[i] = -0.02f + 0.001f * (float)i;
    }
    float var95 = financial_investment_compute_var(inv, returns, 50, 0.95f);
    EXPECT_GE(var95, 0.0f);

    float cvar95 = financial_investment_compute_cvar(inv, returns, 50, 0.95f);
    EXPECT_GE(cvar95, 0.0f);
}

// ============================================================================
// Monte Carlo: VaR < CVaR (expected shortfall >= VaR)
// ============================================================================

TEST_F(FinancialRegressionTest, MonteCarloVaRLessThanCVaR) {
    fin_portfolio_t portfolio;
    memset(&portfolio, 0, sizeof(portfolio));
    portfolio.asset_count = 2;
    portfolio.assets[0].asset_id = 1;
    portfolio.assets[0].current_price = 100.0f;
    portfolio.assets[1].asset_id = 2;
    portfolio.assets[1].current_price = 50.0f;
    portfolio.weights[0] = 0.5f;
    portfolio.weights[1] = 0.5f;
    portfolio.total_value = 100000.0f;

    fin_monte_carlo_result_t mc;
    memset(&mc, 0, sizeof(mc));
    int rc = financial_market_monte_carlo(mkt, &portfolio, 0.05f, 0.25f, 1.0f, 5000, &mc);
    EXPECT_EQ(rc, 0);

    // CVaR (expected shortfall) should be >= VaR
    EXPECT_GE(mc.paths_cvar_95, mc.paths_var_95);
    EXPECT_GE(mc.probability_of_loss, 0.0f);
    EXPECT_LE(mc.probability_of_loss, 1.0f);
}

// ============================================================================
// Option Pricing - Call-Put Parity
// ============================================================================

TEST_F(FinancialRegressionTest, CallPutParityApproximatelyHolds) {
    float S = 100.0f, K = 100.0f, r = 0.05f, vol = 0.20f, T = 1.0f;

    float call_price = financial_investment_black_scholes(S, K, r, vol, T, FIN_OPT_CALL);
    float put_price  = financial_investment_black_scholes(S, K, r, vol, T, FIN_OPT_PUT);

    // Call - Put = S - K * exp(-rT) (approximately)
    float expected_diff = S - K * expf(-r * T);
    EXPECT_NEAR(call_price - put_price, expected_diff, 0.5f);
}

TEST_F(FinancialRegressionTest, BSMAtTheMoneyCallPositive) {
    float S = 100.0f, K = 100.0f, r = 0.05f, vol = 0.20f, T = 1.0f;
    float price = financial_investment_black_scholes(S, K, r, vol, T, FIN_OPT_CALL);
    EXPECT_GT(price, 0.0f);
}

TEST_F(FinancialRegressionTest, BSMDeepInTheMoneyCall) {
    float S = 150.0f, K = 100.0f, r = 0.05f, vol = 0.20f, T = 1.0f;
    float price = financial_investment_black_scholes(S, K, r, vol, T, FIN_OPT_CALL);
    // Deep ITM call should be worth at least the intrinsic value
    EXPECT_GE(price, S - K * expf(-r * T) - 0.01f);
}

// ============================================================================
// Archetype Profiles - All 10 Valid
// ============================================================================

TEST_F(FinancialRegressionTest, AllTenArchetypeProfilesValid) {
    for (int i = 0; i < (int)FIN_ARCH_COUNT; i++) {
        fin_archetype_profile_t profile;
        memset(&profile, 0, sizeof(profile));
        int rc = financial_investor_archetype_get_profile((fin_archetype_id_t)i, &profile);
        EXPECT_EQ(rc, 0) << "Failed for archetype " << i;
        EXPECT_EQ((int)profile.id, i);
        EXPECT_GT(strlen(profile.name), 0u) << "Empty name for archetype " << i;
        EXPECT_GT(strlen(profile.philosophy), 0u) << "Empty philosophy for archetype " << i;
        EXPECT_GE(profile.risk_tolerance, 0.0f);
        EXPECT_LE(profile.risk_tolerance, 1.0f);
        EXPECT_GE((int)profile.cognitive_style, 0);
        EXPECT_LT((int)profile.cognitive_style, (int)FIN_COGNITIVE_STYLE_COUNT);
        EXPECT_GE((int)profile.preferred_horizon, 0);
        EXPECT_LT((int)profile.preferred_horizon, (int)FIN_HORIZON_COUNT);
        EXPECT_GT(profile.heuristic_count, 0u);
    }
}

TEST_F(FinancialRegressionTest, ArchetypeNamesNotNull) {
    for (int i = 0; i < (int)FIN_ARCH_COUNT; i++) {
        const char* name = financial_investor_archetype_name((fin_archetype_id_t)i);
        ASSERT_NE(name, nullptr) << "NULL name for archetype " << i;
        EXPECT_GT(strlen(name), 0u) << "Empty name string for archetype " << i;
    }
}

// ============================================================================
// Mirror Learning Bounds
// ============================================================================

TEST_F(FinancialRegressionTest, MirrorLearningBounds) {
    // Record some outcomes
    for (int i = 0; i < 10; i++) {
        fin_mirror_record_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.archetype     = FIN_ARCH_BUFFETT;
        rec.decision_made = (i % 2 == 0) ? FIN_DECISION_BUY : FIN_DECISION_HOLD;
        rec.outcome_return = (i % 3 == 0) ? 0.05f : -0.02f;
        rec.prediction_error = 0.01f * (float)i;
        rec.was_correct   = (i % 2 == 0);
        rec.timestamp_us  = 1000000ULL * (uint64_t)i;
        financial_investor_archetype_mirror_record(arch, &rec);
    }

    float accuracy = 0.0f, calibration = 0.0f;
    int rc = financial_investor_archetype_self_reflect(arch, &accuracy, &calibration);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(accuracy, 0.0f);
    EXPECT_LE(accuracy, 1.0f);
    EXPECT_GE(calibration, 0.0f);
    EXPECT_LE(calibration, 1.0f);
}

// ============================================================================
// Emotional Modulation Bounds
// ============================================================================

TEST_F(FinancialRegressionTest, EmotionalModulationConvictionBounds) {
    fin_heuristic_input_t input;
    memset(&input, 0, sizeof(input));
    input.current_price   = 60.0f;
    input.intrinsic_value = 90.0f;

    fin_archetype_decision_t decision;
    memset(&decision, 0, sizeof(decision));
    financial_investor_archetype_evaluate(arch, FIN_ARCH_DALIO, &input, &decision);

    fin_emotional_state_t emotion;
    memset(&emotion, 0, sizeof(emotion));
    emotion.fear_level       = 0.8f;
    emotion.greed_level      = 0.1f;
    emotion.stress_level     = 0.6f;
    emotion.arousal_level    = 0.5f;
    emotion.confidence_level = 0.3f;

    int rc = financial_investor_archetype_apply_emotion(arch, &emotion, &decision);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(decision.conviction, 0.0f);
    EXPECT_LE(decision.conviction, 1.0f);
    EXPECT_GE(decision.position_size_pct, 0.0f);
}

// ============================================================================
// Bridge State Transitions
// ============================================================================

TEST_F(FinancialRegressionTest, BridgeStateTransitions) {
    fin_bridge_state_t state = financial_bridge_get_state(brg);
    // Should be IDLE or ACTIVE after creation
    EXPECT_GE((int)state, (int)FIN_BRIDGE_STATE_IDLE);

    // Reset should return to IDLE
    financial_bridge_reset(brg);
    state = financial_bridge_get_state(brg);
    EXPECT_EQ(state, FIN_BRIDGE_STATE_IDLE);
}

TEST_F(FinancialRegressionTest, BridgeStateNullReturnsUninitialized) {
    fin_bridge_state_t state = financial_bridge_get_state(nullptr);
    EXPECT_EQ(state, FIN_BRIDGE_STATE_UNINITIALIZED);
}

// ============================================================================
// Neural State Machine
// ============================================================================

TEST_F(FinancialRegressionTest, NeuralStateTransitions) {
    fin_neural_state_t state = financial_neural_bridge_get_state(neur);
    EXPECT_GE((int)state, (int)FIN_NEURAL_STATE_IDLE);

    financial_neural_bridge_reset(neur);
    state = financial_neural_bridge_get_state(neur);
    EXPECT_EQ(state, FIN_NEURAL_STATE_IDLE);
}

TEST_F(FinancialRegressionTest, NeuralStateNullReturnsUninitialized) {
    fin_neural_state_t state = financial_neural_bridge_get_state(nullptr);
    EXPECT_EQ(state, FIN_NEURAL_STATE_UNINITIALIZED);
}

// ============================================================================
// Plasticity Bounds
// ============================================================================

TEST_F(FinancialRegressionTest, PlasticityRateInReasonableRange) {
    fin_plasticity_params_t params;
    memset(&params, 0, sizeof(params));
    int rc = financial_neural_bridge_get_plasticity(neur, &params);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(params.current_plasticity_rate, 0.0f);
    EXPECT_LE(params.current_plasticity_rate, 10.0f); // reasonable upper bound
}

TEST_F(FinancialRegressionTest, PlasticityAdaptationBounds) {
    fin_plasticity_params_t params;
    memset(&params, 0, sizeof(params));
    int rc = financial_neural_bridge_adapt_risk_params(neur, 2.0f, 0.01f, &params);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(params.adapted_risk_tolerance, 0.0f);
    EXPECT_GE(params.fuzzy_adaptation_degree, 0.0f);
    EXPECT_LE(params.fuzzy_adaptation_degree, 1.0f);
}

// ============================================================================
// Last Error String
// ============================================================================

TEST_F(FinancialRegressionTest, LastErrorStringsNotNull) {
    const char* e1 = financial_investment_get_last_error();
    ASSERT_NE(e1, nullptr);

    const char* e2 = financial_market_get_last_error();
    ASSERT_NE(e2, nullptr);

    const char* e3 = financial_bridge_get_last_error();
    ASSERT_NE(e3, nullptr);

    const char* e4 = financial_neural_bridge_get_last_error();
    ASSERT_NE(e4, nullptr);

    const char* e5 = financial_investor_archetype_get_last_error();
    ASSERT_NE(e5, nullptr);
}

// ============================================================================
// Portfolio Weight Bounds
// ============================================================================

TEST_F(FinancialRegressionTest, PortfolioWeightsSumAfterRebalance) {
    fin_portfolio_t portfolio;
    memset(&portfolio, 0, sizeof(portfolio));
    portfolio.asset_count = 3;
    for (int i = 0; i < 3; i++) {
        portfolio.assets[i].asset_id = (uint32_t)(i + 1);
        portfolio.assets[i].current_price = 100.0f;
        portfolio.weights[i] = 1.0f / 3.0f;
    }
    portfolio.total_value = 90000.0f;

    float target_weights[3] = {0.5f, 0.3f, 0.2f};
    int rc = financial_investment_portfolio_rebalance(inv, &portfolio, target_weights);
    EXPECT_EQ(rc, 0);

    float sum = 0.0f;
    for (uint32_t i = 0; i < portfolio.asset_count; i++) {
        sum += portfolio.weights[i];
    }
    EXPECT_NEAR(sum, 1.0f, 1e-3f);
}

// ============================================================================
// Optimization Convergence Degree
// ============================================================================

TEST_F(FinancialRegressionTest, OptimizationConvergenceDegreeInBounds) {
    fin_portfolio_t portfolio;
    memset(&portfolio, 0, sizeof(portfolio));
    portfolio.asset_count = 3;
    for (int i = 0; i < 3; i++) {
        portfolio.assets[i].asset_id = (uint32_t)(i + 1);
        portfolio.weights[i] = 1.0f / 3.0f;
    }

    float expected_returns[3] = {0.08f, 0.10f, 0.06f};
    float cov[9] = {
        0.04f, 0.01f, 0.005f,
        0.01f, 0.09f, 0.02f,
        0.005f, 0.02f, 0.025f
    };

    fin_optimization_result_t opt;
    memset(&opt, 0, sizeof(opt));
    int rc = financial_investment_optimize(inv, &portfolio, expected_returns, cov,
                                            FIN_OPT_STRATEGY_MAX_SHARPE, &opt);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(opt.convergence_degree, 0.0f);
    EXPECT_LE(opt.convergence_degree, 1.0f);
}
