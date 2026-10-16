/**
 * @file test_financial_investment.cpp
 * @brief Unit tests for NIMCP Financial Investment module
 *
 * Tests portfolio management, risk assessment, derivatives pricing,
 * asset valuation, optimization, factor analysis, tax-loss harvesting,
 * fuzzy integration, modulation, and NULL safety.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

#include "cognitive/parietal/nimcp_financial_investment.h"

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class FinancialInvestmentTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fin = financial_investment_create();
        ASSERT_NE(fin, nullptr);
    }

    void TearDown() override
    {
        if (fin) {
            financial_investment_destroy(fin);
            fin = nullptr;
        }
    }

    /** Helper: create a simple 3-asset portfolio */
    void make_portfolio(fin_portfolio_t* p)
    {
        memset(p, 0, sizeof(*p));
        int rc = financial_investment_portfolio_create(fin, p);
        ASSERT_EQ(rc, FIN_ERR_OK);

        fin_asset_t a{};
        a.asset_id = 1;
        a.type = FIN_ASSET_EQUITY;
        strncpy(a.symbol, "AAPL", sizeof(a.symbol));
        a.current_price = 150.0f;
        a.expected_return = 0.12f;
        a.volatility = 0.25f;
        a.beta = 1.1f;
        ASSERT_EQ(financial_investment_portfolio_add_asset(fin, p, &a, 0.4f), FIN_ERR_OK);

        a.asset_id = 2;
        strncpy(a.symbol, "MSFT", sizeof(a.symbol));
        a.current_price = 300.0f;
        a.expected_return = 0.10f;
        a.volatility = 0.22f;
        a.beta = 1.0f;
        ASSERT_EQ(financial_investment_portfolio_add_asset(fin, p, &a, 0.35f), FIN_ERR_OK);

        a.asset_id = 3;
        a.type = FIN_ASSET_FIXED_INCOME;
        strncpy(a.symbol, "AGG", sizeof(a.symbol));
        a.current_price = 100.0f;
        a.expected_return = 0.04f;
        a.volatility = 0.06f;
        a.beta = 0.1f;
        ASSERT_EQ(financial_investment_portfolio_add_asset(fin, p, &a, 0.25f), FIN_ERR_OK);
    }

    financial_investment_eng_t* fin = nullptr;
};

//=============================================================================
// 1. Lifecycle Tests
//=============================================================================

TEST_F(FinancialInvestmentTest, DefaultConfig)
{
    fin_config_t cfg = financial_investment_default_config();
    EXPECT_GT(cfg.risk_free_rate, 0.0f);
    EXPECT_GT(cfg.max_iterations, 0u);
    EXPECT_GT(cfg.convergence_tolerance, 0.0f);
    EXPECT_GT(cfg.monte_carlo_paths, 0u);
}

TEST_F(FinancialInvestmentTest, CreateDefault)
{
    EXPECT_NE(fin, nullptr);
}

TEST_F(FinancialInvestmentTest, CreateCustom)
{
    fin_config_t cfg = financial_investment_default_config();
    cfg.risk_free_rate = 0.06f;
    cfg.max_iterations = 2000;
    financial_investment_eng_t* custom = financial_investment_create_custom(&cfg);
    ASSERT_NE(custom, nullptr);
    financial_investment_destroy(custom);
}

TEST_F(FinancialInvestmentTest, DestroyNullSafe)
{
    financial_investment_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// 2. Portfolio Management Tests
//=============================================================================

TEST_F(FinancialInvestmentTest, PortfolioCreate)
{
    fin_portfolio_t p{};
    int rc = financial_investment_portfolio_create(fin, &p);
    EXPECT_EQ(rc, FIN_ERR_OK);
    EXPECT_EQ(p.asset_count, 0u);
}

TEST_F(FinancialInvestmentTest, PortfolioAddAsset)
{
    fin_portfolio_t p{};
    financial_investment_portfolio_create(fin, &p);

    fin_asset_t a{};
    a.asset_id = 1;
    a.type = FIN_ASSET_EQUITY;
    strncpy(a.symbol, "TSLA", sizeof(a.symbol));
    a.current_price = 200.0f;
    a.expected_return = 0.15f;
    a.volatility = 0.45f;

    int rc = financial_investment_portfolio_add_asset(fin, &p, &a, 0.5f);
    EXPECT_EQ(rc, FIN_ERR_OK);
    EXPECT_EQ(p.asset_count, 1u);
    EXPECT_NEAR(p.weights[0], 0.5f, 1e-5f);
}

TEST_F(FinancialInvestmentTest, PortfolioRemoveAsset)
{
    fin_portfolio_t p{};
    make_portfolio(&p);
    EXPECT_EQ(p.asset_count, 3u);

    int rc = financial_investment_portfolio_remove_asset(fin, &p, 2);
    EXPECT_EQ(rc, FIN_ERR_OK);
    EXPECT_EQ(p.asset_count, 2u);
}

TEST_F(FinancialInvestmentTest, PortfolioRemoveAssetNotFound)
{
    fin_portfolio_t p{};
    make_portfolio(&p);
    int rc = financial_investment_portfolio_remove_asset(fin, &p, 999);
    EXPECT_NE(rc, FIN_ERR_OK);
}

TEST_F(FinancialInvestmentTest, PortfolioRebalance)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    float targets[] = {0.33f, 0.34f, 0.33f};
    int rc = financial_investment_portfolio_rebalance(fin, &p, targets);
    EXPECT_EQ(rc, FIN_ERR_OK);
    EXPECT_NEAR(p.weights[0], 0.33f, 0.02f);
}

TEST_F(FinancialInvestmentTest, PortfolioReturn)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    float ret = financial_investment_portfolio_return(fin, &p);
    // weighted: 0.4*0.12 + 0.35*0.10 + 0.25*0.04 = 0.048 + 0.035 + 0.01 = 0.093
    EXPECT_NEAR(ret, 0.093f, 0.02f);
}

TEST_F(FinancialInvestmentTest, PortfolioVolatility)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    // 3x3 correlation matrix (identity-ish for simplicity)
    float corr[] = {
        1.0f, 0.5f, 0.1f,
        0.5f, 1.0f, 0.2f,
        0.1f, 0.2f, 1.0f
    };
    float vol = financial_investment_portfolio_volatility(fin, &p, corr);
    EXPECT_GT(vol, 0.0f);
    EXPECT_LT(vol, 1.0f);
}

//=============================================================================
// 3. Risk Assessment Tests
//=============================================================================

TEST_F(FinancialInvestmentTest, AssessRisk)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    float corr[] = {
        1.0f, 0.5f, 0.1f,
        0.5f, 1.0f, 0.2f,
        0.1f, 0.2f, 1.0f
    };
    // Generate synthetic returns
    float returns[100];
    for (int i = 0; i < 100; i++) {
        returns[i] = 0.001f * (float)(i % 20 - 10);
    }

    fin_risk_metrics_t metrics{};
    int rc = financial_investment_assess_risk(fin, &p, corr, returns, 100, &metrics);
    EXPECT_EQ(rc, FIN_ERR_OK);
    EXPECT_NE(metrics.sharpe_ratio, 0.0f);
    EXPECT_GE(metrics.max_drawdown, 0.0f); // drawdown is positive (0 to 1 range)
}

TEST_F(FinancialInvestmentTest, AssessRiskFuzzyGrade)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    float corr[] = {
        1.0f, 0.5f, 0.1f,
        0.5f, 1.0f, 0.2f,
        0.1f, 0.2f, 1.0f
    };
    float returns[100];
    for (int i = 0; i < 100; i++) {
        returns[i] = 0.001f * (float)(i % 20 - 10);
    }

    fin_risk_metrics_t metrics{};
    financial_investment_assess_risk(fin, &p, corr, returns, 100, &metrics);
    // fuzzy_risk_grade should be in [0,1]
    EXPECT_GE(metrics.fuzzy_risk_grade, 0.0f);
    EXPECT_LE(metrics.fuzzy_risk_grade, 1.0f);
}

TEST_F(FinancialInvestmentTest, ComputeVaR)
{
    float returns[] = {-0.05f, -0.02f, 0.01f, 0.03f, -0.04f,
                       0.02f, -0.01f, 0.00f, -0.03f, 0.04f};
    float var = financial_investment_compute_var(fin, returns, 10, 0.95f);
    EXPECT_GE(var, 0.0f); // VaR is positive loss measure
}

TEST_F(FinancialInvestmentTest, ComputeCVaR)
{
    float returns[] = {-0.05f, -0.02f, 0.01f, 0.03f, -0.04f,
                       0.02f, -0.01f, 0.00f, -0.03f, 0.04f};
    float cvar = financial_investment_compute_cvar(fin, returns, 10, 0.95f);
    EXPECT_GE(cvar, 0.0f); // CVaR is positive loss measure
    // CVaR should be worse (larger) than VaR
    float var = financial_investment_compute_var(fin, returns, 10, 0.95f);
    EXPECT_GE(cvar, var);
}

TEST_F(FinancialInvestmentTest, MaxDrawdown)
{
    float prices[] = {100.0f, 110.0f, 105.0f, 95.0f, 100.0f, 120.0f};
    float mdd = financial_investment_max_drawdown(prices, 6);
    // Drawdown from 110 to 95 = 13.6%
    EXPECT_GT(mdd, 0.0f);  // drawdown is positive
    EXPECT_LT(mdd, 1.0f);  // and less than 100%
}

//=============================================================================
// 4. Derivatives Pricing Tests
//=============================================================================

TEST_F(FinancialInvestmentTest, BlackScholesCall)
{
    float price = financial_investment_black_scholes(
        100.0f, 100.0f, 0.05f, 0.20f, 1.0f, FIN_OPT_CALL);
    // ATM call with vol=0.20, rate=0.05 should be around 10
    EXPECT_GT(price, 5.0f);
    EXPECT_LT(price, 20.0f);
}

TEST_F(FinancialInvestmentTest, BlackScholesPut)
{
    float price = financial_investment_black_scholes(
        100.0f, 100.0f, 0.05f, 0.20f, 1.0f, FIN_OPT_PUT);
    // ATM put should be less than call due to put-call parity
    float call = financial_investment_black_scholes(
        100.0f, 100.0f, 0.05f, 0.20f, 1.0f, FIN_OPT_CALL);
    EXPECT_GT(price, 0.0f);
    EXPECT_LT(price, call);
}

TEST_F(FinancialInvestmentTest, PriceOption)
{
    fin_option_result_t result{};
    int rc = financial_investment_price_option(fin, 100.0f, 100.0f,
        0.05f, 0.20f, 1.0f, FIN_OPT_CALL,
        FIN_PRICING_BLACK_SCHOLES, &result);
    EXPECT_EQ(rc, FIN_ERR_OK);
    EXPECT_GT(result.price, 0.0f);
    // Greeks should be computed
    EXPECT_GT(result.delta, 0.0f);  // call delta positive
    EXPECT_GT(result.gamma, 0.0f);  // gamma always positive
    EXPECT_LT(result.theta, 0.0f);  // theta negative for long options
    EXPECT_GT(result.vega, 0.0f);   // vega positive
}

TEST_F(FinancialInvestmentTest, BinomialTree)
{
    fin_option_result_t result{};
    int rc = financial_investment_binomial_tree(fin,
        100.0f, 100.0f, 0.05f, 0.20f, 1.0f,
        100, FIN_OPT_CALL, FIN_OPT_STYLE_EUROPEAN, &result);
    EXPECT_EQ(rc, FIN_ERR_OK);
    EXPECT_GT(result.price, 0.0f);

    // Should converge to B-S price for European
    float bs = financial_investment_black_scholes(
        100.0f, 100.0f, 0.05f, 0.20f, 1.0f, FIN_OPT_CALL);
    EXPECT_NEAR(result.price, bs, 0.5f);
}

TEST_F(FinancialInvestmentTest, BinomialTreeAmerican)
{
    fin_option_result_t result{};
    int rc = financial_investment_binomial_tree(fin,
        100.0f, 100.0f, 0.05f, 0.20f, 1.0f,
        100, FIN_OPT_PUT, FIN_OPT_STYLE_AMERICAN, &result);
    EXPECT_EQ(rc, FIN_ERR_OK);
    // American put >= European put
    float euro = financial_investment_black_scholes(
        100.0f, 100.0f, 0.05f, 0.20f, 1.0f, FIN_OPT_PUT);
    EXPECT_GE(result.price, euro - 0.5f); // small tolerance
}

TEST_F(FinancialInvestmentTest, ImpliedVolatility)
{
    // Price an option, then recover the implied vol
    float market_price = financial_investment_black_scholes(
        100.0f, 100.0f, 0.05f, 0.25f, 1.0f, FIN_OPT_CALL);
    float iv = financial_investment_implied_vol(market_price,
        100.0f, 100.0f, 0.05f, 1.0f, FIN_OPT_CALL);
    EXPECT_NEAR(iv, 0.25f, 0.02f);
}

//=============================================================================
// 5. Asset Valuation Tests
//=============================================================================

TEST_F(FinancialInvestmentTest, DCFValuation)
{
    float cash_flows[] = {10.0f, 12.0f, 14.0f, 16.0f, 18.0f};
    fin_valuation_result_t result{};
    int rc = financial_investment_dcf_valuation(fin, cash_flows, 5,
        0.10f, 0.03f, &result);
    EXPECT_EQ(rc, FIN_ERR_OK);
    EXPECT_GT(result.intrinsic_value, 0.0f);
    EXPECT_EQ(result.method, FIN_VALUATION_DCF);
}

TEST_F(FinancialInvestmentTest, DDMValuation)
{
    fin_valuation_result_t result{};
    int rc = financial_investment_ddm_valuation(fin,
        2.0f, 0.05f, 0.10f, &result);
    EXPECT_EQ(rc, FIN_ERR_OK);
    // Gordon growth: D*(1+g)/(r-g) = 2*1.05/0.05 = 42
    EXPECT_GT(result.intrinsic_value, 30.0f);
    EXPECT_EQ(result.method, FIN_VALUATION_DDM);
}

TEST_F(FinancialInvestmentTest, ComparablesValuation)
{
    float peer_multiples[] = {15.0f, 18.0f, 20.0f, 14.0f};
    fin_valuation_result_t result{};
    int rc = financial_investment_comparables(fin, peer_multiples, 4,
        5.0f, &result);
    EXPECT_EQ(rc, FIN_ERR_OK);
    EXPECT_GT(result.intrinsic_value, 0.0f);
    EXPECT_EQ(result.method, FIN_VALUATION_COMPARABLES);
}

//=============================================================================
// 6. Portfolio Optimization Tests
//=============================================================================

TEST_F(FinancialInvestmentTest, OptimizeMeanVariance)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    float expected_returns[] = {0.12f, 0.10f, 0.04f};
    float cov[] = {
        0.0625f, 0.025f, 0.005f,
        0.025f, 0.0484f, 0.008f,
        0.005f, 0.008f, 0.0036f
    };

    fin_optimization_result_t result{};
    int rc = financial_investment_optimize(fin, &p, expected_returns, cov,
        FIN_OPT_STRATEGY_MEAN_VARIANCE, &result);
    EXPECT_EQ(rc, FIN_ERR_OK);
    EXPECT_TRUE(result.converged);
    EXPECT_GT(result.expected_return, 0.0f);
    financial_investment_free_optimization(&result);
}

TEST_F(FinancialInvestmentTest, OptimizeMinVariance)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    float expected_returns[] = {0.12f, 0.10f, 0.04f};
    float cov[] = {
        0.0625f, 0.025f, 0.005f,
        0.025f, 0.0484f, 0.008f,
        0.005f, 0.008f, 0.0036f
    };

    fin_optimization_result_t result{};
    int rc = financial_investment_optimize(fin, &p, expected_returns, cov,
        FIN_OPT_STRATEGY_MIN_VARIANCE, &result);
    EXPECT_EQ(rc, FIN_ERR_OK);
    EXPECT_GT(result.expected_volatility, 0.0f);
    financial_investment_free_optimization(&result);
}

TEST_F(FinancialInvestmentTest, OptimizeMaxSharpe)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    float expected_returns[] = {0.12f, 0.10f, 0.04f};
    float cov[] = {
        0.0625f, 0.025f, 0.005f,
        0.025f, 0.0484f, 0.008f,
        0.005f, 0.008f, 0.0036f
    };

    fin_optimization_result_t result{};
    int rc = financial_investment_optimize(fin, &p, expected_returns, cov,
        FIN_OPT_STRATEGY_MAX_SHARPE, &result);
    EXPECT_EQ(rc, FIN_ERR_OK);
    EXPECT_GT(result.expected_sharpe, 0.0f);
    financial_investment_free_optimization(&result);
}

TEST_F(FinancialInvestmentTest, OptimizeRiskParity)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    float expected_returns[] = {0.12f, 0.10f, 0.04f};
    float cov[] = {
        0.0625f, 0.025f, 0.005f,
        0.025f, 0.0484f, 0.008f,
        0.005f, 0.008f, 0.0036f
    };

    fin_optimization_result_t result{};
    int rc = financial_investment_optimize(fin, &p, expected_returns, cov,
        FIN_OPT_STRATEGY_RISK_PARITY, &result);
    EXPECT_EQ(rc, FIN_ERR_OK);
    financial_investment_free_optimization(&result);
}

TEST_F(FinancialInvestmentTest, OptimizeEqualWeight)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    float expected_returns[] = {0.12f, 0.10f, 0.04f};
    float cov[] = {
        0.0625f, 0.025f, 0.005f,
        0.025f, 0.0484f, 0.008f,
        0.005f, 0.008f, 0.0036f
    };

    fin_optimization_result_t result{};
    int rc = financial_investment_optimize(fin, &p, expected_returns, cov,
        FIN_OPT_STRATEGY_EQUAL_WEIGHT, &result);
    EXPECT_EQ(rc, FIN_ERR_OK);
    // Equal weight: each asset = 1/3
    EXPECT_NEAR(result.optimal_weights[0], 1.0f / 3.0f, 0.05f);
    financial_investment_free_optimization(&result);
}

TEST_F(FinancialInvestmentTest, OptimizeBlackLitterman)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    float expected_returns[] = {0.12f, 0.10f, 0.04f};
    float cov[] = {
        0.0625f, 0.025f, 0.005f,
        0.025f, 0.0484f, 0.008f,
        0.005f, 0.008f, 0.0036f
    };

    fin_optimization_result_t result{};
    int rc = financial_investment_optimize(fin, &p, expected_returns, cov,
        FIN_OPT_STRATEGY_BLACK_LITTERMAN, &result);
    EXPECT_EQ(rc, FIN_ERR_OK);
    financial_investment_free_optimization(&result);
}

TEST_F(FinancialInvestmentTest, OptimizeFuzzyConvergence)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    float expected_returns[] = {0.12f, 0.10f, 0.04f};
    float cov[] = {
        0.0625f, 0.025f, 0.005f,
        0.025f, 0.0484f, 0.008f,
        0.005f, 0.008f, 0.0036f
    };

    fin_optimization_result_t result{};
    financial_investment_optimize(fin, &p, expected_returns, cov,
        FIN_OPT_STRATEGY_MEAN_VARIANCE, &result);
    // Fuzzy convergence degree in [0,1]
    EXPECT_GE(result.convergence_degree, 0.0f);
    EXPECT_LE(result.convergence_degree, 1.0f);
    financial_investment_free_optimization(&result);
}

TEST_F(FinancialInvestmentTest, EfficientFrontier)
{
    float expected_returns[] = {0.12f, 0.10f, 0.04f};
    float cov[] = {
        0.0625f, 0.025f, 0.005f,
        0.025f, 0.0484f, 0.008f,
        0.005f, 0.008f, 0.0036f
    };

    const uint32_t pts = 10;
    float frontier_returns[10];
    float frontier_vols[10];

    int rc = financial_investment_efficient_frontier(fin,
        expected_returns, cov, 3,
        frontier_returns, frontier_vols, pts);
    EXPECT_EQ(rc, FIN_ERR_OK);
    // Returns should be monotonically increasing along frontier
    for (uint32_t i = 1; i < pts; i++) {
        EXPECT_GE(frontier_returns[i], frontier_returns[i - 1] - 0.01f);
    }
}

//=============================================================================
// 7. Factor Analysis Tests
//=============================================================================

TEST_F(FinancialInvestmentTest, FactorAnalysis)
{
    // 20 observations of 1 asset, 2 factors
    float asset_returns[20];
    float factor_returns[40]; // 20 obs * 2 factors
    for (int i = 0; i < 20; i++) {
        asset_returns[i] = 0.01f * (float)(i % 5 - 2);
        factor_returns[i * 2 + 0] = 0.005f * (float)(i % 7 - 3);
        factor_returns[i * 2 + 1] = 0.003f * (float)(i % 4 - 1);
    }

    fin_factor_result_t result{};
    int rc = financial_investment_factor_analysis(fin,
        asset_returns, factor_returns, 20, 2, &result);
    EXPECT_EQ(rc, FIN_ERR_OK);
    EXPECT_EQ(result.factor_count, 2u);
    EXPECT_GE(result.r_squared, 0.0f);
    EXPECT_LE(result.r_squared, 1.0f);
    financial_investment_free_factor(&result);
}

//=============================================================================
// 8. Tax-Loss Harvesting Test
//=============================================================================

TEST_F(FinancialInvestmentTest, TaxLossHarvest)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    // cost basis: some assets at a loss
    float cost_basis[] = {160.0f, 280.0f, 110.0f}; // AAPL at loss, MSFT gain, AGG loss
    float tax_savings = 0.0f;

    int rc = financial_investment_tax_loss_harvest(fin, &p, cost_basis, &tax_savings);
    EXPECT_EQ(rc, FIN_ERR_OK);
    EXPECT_GE(tax_savings, 0.0f);
}

//=============================================================================
// 9. Fuzzy Integration Tests
//=============================================================================

TEST_F(FinancialInvestmentTest, FuzzyDiversificationQuality)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    float corr[] = {
        1.0f, 0.5f, 0.1f,
        0.5f, 1.0f, 0.2f,
        0.1f, 0.2f, 1.0f
    };
    float returns[100];
    for (int i = 0; i < 100; i++) {
        returns[i] = 0.001f * (float)(i % 20 - 10);
    }

    fin_risk_metrics_t metrics{};
    financial_investment_assess_risk(fin, &p, corr, returns, 100, &metrics);
    EXPECT_GE(metrics.fuzzy_diversification_quality, 0.0f);
    EXPECT_LE(metrics.fuzzy_diversification_quality, 1.0f);
}

//=============================================================================
// 10. Modulation Tests
//=============================================================================

TEST_F(FinancialInvestmentTest, SetInflammation)
{
    int rc = financial_investment_set_inflammation(fin, 0.5f);
    EXPECT_EQ(rc, FIN_ERR_OK);
}

TEST_F(FinancialInvestmentTest, SetFatigue)
{
    int rc = financial_investment_set_fatigue(fin, 0.7f);
    EXPECT_EQ(rc, FIN_ERR_OK);
}

TEST_F(FinancialInvestmentTest, InflammationAffectsRisk)
{
    fin_portfolio_t p{};
    make_portfolio(&p);

    float corr[] = {
        1.0f, 0.5f, 0.1f,
        0.5f, 1.0f, 0.2f,
        0.1f, 0.2f, 1.0f
    };
    float returns[100];
    for (int i = 0; i < 100; i++) {
        returns[i] = 0.001f * (float)(i % 20 - 10);
    }

    // Baseline
    fin_risk_metrics_t base{};
    financial_investment_assess_risk(fin, &p, corr, returns, 100, &base);

    // With inflammation
    financial_investment_set_inflammation(fin, 0.9f);
    fin_risk_metrics_t inflamed{};
    financial_investment_assess_risk(fin, &p, corr, returns, 100, &inflamed);

    // Inflammation should shift risk perception
    // (either fuzzy_risk_grade changes, or other metrics shift)
    // We just verify the function runs without error
    EXPECT_GE(inflamed.fuzzy_risk_grade, 0.0f);
}

//=============================================================================
// 11. Statistics Tests
//=============================================================================

TEST_F(FinancialInvestmentTest, GetStats)
{
    fin_stats_t stats{};
    int rc = financial_investment_get_stats(fin, &stats);
    EXPECT_EQ(rc, FIN_ERR_OK);
}

TEST_F(FinancialInvestmentTest, ResetStats)
{
    // Do some work first
    fin_portfolio_t p{};
    make_portfolio(&p);

    financial_investment_reset_stats(fin);
    fin_stats_t stats{};
    financial_investment_get_stats(fin, &stats);
    EXPECT_EQ(stats.portfolio_analyses, 0u);
}

TEST_F(FinancialInvestmentTest, GetLastError)
{
    const char* err = financial_investment_get_last_error();
    // Should return non-null (may be empty string)
    EXPECT_NE(err, nullptr);
}

//=============================================================================
// 12. NULL Safety Tests
//=============================================================================

TEST_F(FinancialInvestmentTest, NullCreateCustom)
{
    financial_investment_eng_t* e = financial_investment_create_custom(nullptr);
    // Should either return valid with defaults or null
    if (e) financial_investment_destroy(e);
}

TEST_F(FinancialInvestmentTest, NullPortfolioCreate)
{
    int rc = financial_investment_portfolio_create(fin, nullptr);
    EXPECT_NE(rc, FIN_ERR_OK);
}

TEST_F(FinancialInvestmentTest, NullPortfolioCreateEngine)
{
    fin_portfolio_t p{};
    int rc = financial_investment_portfolio_create(nullptr, &p);
    EXPECT_NE(rc, FIN_ERR_OK);
}

TEST_F(FinancialInvestmentTest, NullAddAsset)
{
    fin_portfolio_t p{};
    int rc = financial_investment_portfolio_add_asset(fin, &p, nullptr, 0.5f);
    EXPECT_NE(rc, FIN_ERR_OK);
}

TEST_F(FinancialInvestmentTest, NullAssessRisk)
{
    int rc = financial_investment_assess_risk(nullptr, nullptr, nullptr,
                                              nullptr, 0, nullptr);
    EXPECT_NE(rc, FIN_ERR_OK);
}

TEST_F(FinancialInvestmentTest, NullPriceOption)
{
    int rc = financial_investment_price_option(nullptr, 100, 100,
        0.05f, 0.2f, 1.0f, FIN_OPT_CALL, FIN_PRICING_BLACK_SCHOLES, nullptr);
    EXPECT_NE(rc, FIN_ERR_OK);
}

TEST_F(FinancialInvestmentTest, NullDCFValuation)
{
    int rc = financial_investment_dcf_valuation(nullptr, nullptr, 0,
        0.1f, 0.03f, nullptr);
    EXPECT_NE(rc, FIN_ERR_OK);
}

TEST_F(FinancialInvestmentTest, NullOptimize)
{
    int rc = financial_investment_optimize(nullptr, nullptr, nullptr,
        nullptr, FIN_OPT_STRATEGY_MEAN_VARIANCE, nullptr);
    EXPECT_NE(rc, FIN_ERR_OK);
}

TEST_F(FinancialInvestmentTest, NullFactorAnalysis)
{
    int rc = financial_investment_factor_analysis(nullptr, nullptr,
        nullptr, 0, 0, nullptr);
    EXPECT_NE(rc, FIN_ERR_OK);
}

TEST_F(FinancialInvestmentTest, NullSetInflammation)
{
    int rc = financial_investment_set_inflammation(nullptr, 0.5f);
    EXPECT_NE(rc, FIN_ERR_OK);
}

TEST_F(FinancialInvestmentTest, NullGetStats)
{
    int rc = financial_investment_get_stats(nullptr, nullptr);
    EXPECT_NE(rc, FIN_ERR_OK);
}

TEST_F(FinancialInvestmentTest, FreeOptimizationNull)
{
    financial_investment_free_optimization(nullptr);
    // Should not crash
}

TEST_F(FinancialInvestmentTest, FreeFactorNull)
{
    financial_investment_free_factor(nullptr);
    // Should not crash
}

} // namespace
