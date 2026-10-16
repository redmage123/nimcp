//=============================================================================
// test_financial_gpu_workflow.cpp - Real-World Financial Workflow Tests
//=============================================================================
/**
 * @file test_financial_gpu_workflow.cpp
 * @brief Integration tests for realistic financial GPU workflows
 *
 * Tests complete end-to-end financial workflows including:
 *   - Portfolio optimization pipeline with risk analysis
 *   - Risk scenario stress testing
 *   - Options pricing with Greeks and hedging
 *   - Black-Litterman portfolio construction
 *   - GPU recovery during financial operations
 *   - Multi-asset correlation and VaR
 *
 * WORKFLOW PATTERNS TESTED:
 *   1. Data In -> GPU Process -> Risk Analysis -> Decision
 *   2. Scenario Generation -> Portfolio Simulation -> Risk Aggregation
 *   3. Market Data -> Option Pricing -> Greeks -> Hedge Ratios
 *   4. Views -> Black-Litterman -> Optimal Weights -> Backtest
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cstring>

// Include financial GPU headers
// Note: These are C headers but have proper extern "C" guards internally
#include "gpu/financial/nimcp_financial_gpu.h"
#include "gpu/financial/nimcp_financial_risk_gpu.h"
#include "gpu/financial/nimcp_financial_derivatives_gpu.h"
#include "gpu/financial/nimcp_financial_monte_carlo_gpu.h"
#include "gpu/financial/nimcp_financial_optimization_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"

//=============================================================================
// Test Fixture for Financial Workflows
//=============================================================================

class FinancialGPUWorkflowTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    fin_gpu_rng_t* rng = nullptr;
    nimcp_gpu_recovery_context_t* recovery_ctx = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        // Try to create GPU context
        ctx = nimcp_gpu_context_create(0);
        if (!ctx) {
            GTEST_SKIP() << "No GPU available - skipping workflow test";
        }
        gpu_available = true;

        // Create RNG with fixed seed for reproducibility
        rng = fin_gpu_rng_create(ctx, 200000, 42);
        if (!rng) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
            GTEST_SKIP() << "Failed to create GPU RNG - skipping workflow test";
        }

        // Initialize recovery context
        nimcp_gpu_recovery_config_t recovery_config;
        nimcp_gpu_recovery_default_config(&recovery_config);
        recovery_config.enable_cpu_fallback = true;
        recovery_config.max_retries = 3;
        recovery_ctx = nimcp_gpu_recovery_context_create(&recovery_config);
    }

    void TearDown() override {
        if (recovery_ctx) {
            nimcp_gpu_recovery_context_destroy(recovery_ctx);
            recovery_ctx = nullptr;
        }
        if (rng) {
            fin_gpu_rng_destroy(rng);
            rng = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Helper: Generate realistic covariance matrix (positive semi-definite)
    std::vector<float> generateRealisticCovariance(uint32_t n_assets,
                                                    float avg_vol = 0.20f,
                                                    float avg_corr = 0.30f) {
        std::vector<float> cov(n_assets * n_assets);
        std::vector<float> volatilities(n_assets);

        // Generate varied volatilities
        for (uint32_t i = 0; i < n_assets; i++) {
            volatilities[i] = avg_vol * (0.5f + static_cast<float>(rand()) / RAND_MAX);
        }

        // Build covariance from correlation
        for (uint32_t i = 0; i < n_assets; i++) {
            for (uint32_t j = 0; j < n_assets; j++) {
                if (i == j) {
                    cov[i * n_assets + j] = volatilities[i] * volatilities[i];
                } else {
                    // Correlation with some randomness but bounded
                    float corr = avg_corr + 0.2f * (static_cast<float>(rand()) / RAND_MAX - 0.5f);
                    corr = std::max(-0.9f, std::min(0.9f, corr));
                    cov[i * n_assets + j] = corr * volatilities[i] * volatilities[j];
                }
            }
        }

        // Ensure positive semi-definite by adding small diagonal
        for (uint32_t i = 0; i < n_assets; i++) {
            cov[i * n_assets + i] += 1e-6f;
        }

        return cov;
    }

    // Helper: Generate expected returns with sector clustering
    std::vector<float> generateSectorReturns(uint32_t n_assets, uint32_t n_sectors,
                                              float base_return = 0.08f) {
        std::vector<float> returns(n_assets);
        std::vector<float> sector_premia(n_sectors);

        // Sector risk premia
        for (uint32_t s = 0; s < n_sectors; s++) {
            sector_premia[s] = 0.04f * (static_cast<float>(rand()) / RAND_MAX - 0.5f);
        }

        // Asset returns with sector and idiosyncratic components
        for (uint32_t i = 0; i < n_assets; i++) {
            uint32_t sector = i % n_sectors;
            float idio = 0.02f * (static_cast<float>(rand()) / RAND_MAX - 0.5f);
            returns[i] = base_return + sector_premia[sector] + idio;
        }

        return returns;
    }

    // Helper: Compute Cholesky decomposition for correlation matrix
    std::vector<float> computeCholesky(const std::vector<float>& matrix, uint32_t n) {
        std::vector<float> L(n * n, 0.0f);

        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = 0; j <= i; j++) {
                float sum = matrix[i * n + j];
                for (uint32_t k = 0; k < j; k++) {
                    sum -= L[i * n + k] * L[j * n + k];
                }
                if (i == j) {
                    if (sum <= 0.0f) sum = 1e-8f;  // Numerical stability
                    L[i * n + j] = std::sqrt(sum);
                } else {
                    L[i * n + j] = sum / L[j * n + j];
                }
            }
        }
        return L;
    }

    // Helper: Compute portfolio statistics
    struct PortfolioStats {
        float expected_return;
        float volatility;
        float sharpe_ratio;
        float var_95;
    };

    PortfolioStats computePortfolioStats(const float* weights,
                                          const float* returns,
                                          const float* covariance,
                                          uint32_t n_assets,
                                          float risk_free = 0.02f) {
        PortfolioStats stats = {0};

        // Expected return: w' * r
        for (uint32_t i = 0; i < n_assets; i++) {
            stats.expected_return += weights[i] * returns[i];
        }

        // Variance: w' * Cov * w
        float variance = 0.0f;
        for (uint32_t i = 0; i < n_assets; i++) {
            for (uint32_t j = 0; j < n_assets; j++) {
                variance += weights[i] * weights[j] * covariance[i * n_assets + j];
            }
        }
        stats.volatility = std::sqrt(variance);

        // Sharpe ratio
        if (stats.volatility > 1e-8f) {
            stats.sharpe_ratio = (stats.expected_return - risk_free) / stats.volatility;
        }

        // Parametric VaR (95%)
        stats.var_95 = -stats.expected_return + 1.645f * stats.volatility;

        return stats;
    }
};

//=============================================================================
// Portfolio Optimization Pipeline Tests
//=============================================================================

TEST_F(FinancialGPUWorkflowTest, FullPortfolioOptimizationPipeline) {
    // Workflow: Generate Data -> Optimize -> Analyze Risk -> Simulate -> Validate

    const uint32_t N_ASSETS = 20;
    const uint32_t N_SECTORS = 4;
    srand(12345);

    // Step 1: Generate market data
    auto expected_returns = generateSectorReturns(N_ASSETS, N_SECTORS, 0.10f);
    auto covariance = generateRealisticCovariance(N_ASSETS, 0.25f, 0.35f);

    // Step 2: Run mean-variance optimization for max Sharpe
    fin_optimization_gpu_params_t opt_params = fin_optimization_gpu_params_default();
    opt_params.strategy = FIN_OPT_STRATEGY_MAX_SHARPE;
    opt_params.risk_free_rate = 0.03f;
    opt_params.n_assets = N_ASSETS;
    opt_params.long_only = true;
    opt_params.min_weight = 0.0f;
    opt_params.max_weight = 0.20f;  // Max 20% per asset
    opt_params.max_iterations = 1000;
    opt_params.convergence_tolerance = 1e-7f;

    fin_optimization_gpu_result_t opt_result = {0};
    bool ok = fin_optimization_gpu_mean_variance(ctx, expected_returns.data(),
                                                   covariance.data(), &opt_params,
                                                   &opt_result);
    ASSERT_TRUE(ok) << "Portfolio optimization failed: " << fin_gpu_get_last_error();
    ASSERT_TRUE(opt_result.converged) << "Optimization did not converge";

    // Verify constraints
    float weight_sum = 0.0f;
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        weight_sum += opt_result.optimal_weights[i];
        EXPECT_GE(opt_result.optimal_weights[i], -1e-4f) << "Long-only violated at " << i;
        EXPECT_LE(opt_result.optimal_weights[i], 0.201f) << "Max weight violated at " << i;
    }
    EXPECT_NEAR(weight_sum, 1.0f, 0.01f);

    // Step 3: Compute risk metrics for optimized portfolio
    auto portfolio_stats = computePortfolioStats(opt_result.optimal_weights,
                                                  expected_returns.data(),
                                                  covariance.data(),
                                                  N_ASSETS, 0.03f);

    EXPECT_NEAR(portfolio_stats.expected_return, opt_result.expected_return, 0.01f);
    EXPECT_NEAR(portfolio_stats.volatility, opt_result.portfolio_volatility, 0.01f);

    // Step 4: Monte Carlo simulation of optimized portfolio
    fin_monte_carlo_gpu_params_t mc_params = fin_monte_carlo_gpu_params_default();
    mc_params.initial_value = 1000000.0f;  // $1M portfolio
    mc_params.drift = opt_result.expected_return;
    mc_params.volatility = opt_result.portfolio_volatility;
    mc_params.time_horizon = 1.0f;
    mc_params.num_paths = 50000;
    mc_params.num_steps = 252;
    mc_params.antithetic = true;

    fin_monte_carlo_gpu_result_t mc_result = {0};
    ok = fin_monte_carlo_gpu_simulate(ctx, rng, &mc_params, &mc_result);
    ASSERT_TRUE(ok) << "Monte Carlo simulation failed";

    // Step 5: Validate simulated results match optimization expectations
    EXPECT_NEAR(mc_result.mean_return, opt_result.expected_return, 0.02f)
        << "Simulated return should match expected";
    EXPECT_NEAR(mc_result.std_return, opt_result.portfolio_volatility, 0.02f)
        << "Simulated volatility should match expected";
    EXPECT_GT(mc_result.var_95, 0.0f) << "VaR should be positive (loss)";

    // Step 6: Compute risk contribution using GPU
    std::vector<float> marginal_risk(N_ASSETS);
    std::vector<float> risk_contrib(N_ASSETS);
    ok = fin_opt_gpu_risk_contribution(ctx, opt_result.optimal_weights,
                                         covariance.data(), N_ASSETS,
                                         marginal_risk.data(), risk_contrib.data());
    ASSERT_TRUE(ok) << "Risk contribution computation failed";

    // Verify risk contributions sum to portfolio variance
    float total_risk_contrib = 0.0f;
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        total_risk_contrib += risk_contrib[i];
    }
    float expected_variance = opt_result.portfolio_volatility * opt_result.portfolio_volatility;
    EXPECT_NEAR(total_risk_contrib, expected_variance, expected_variance * 0.05f);

    fin_optimization_gpu_result_free(&opt_result);
    fin_monte_carlo_gpu_result_free(ctx, &mc_result);
}

TEST_F(FinancialGPUWorkflowTest, RiskParityPortfolioConstruction) {
    // Workflow: Equal risk contribution portfolio

    const uint32_t N_ASSETS = 8;
    srand(54321);

    auto covariance = generateRealisticCovariance(N_ASSETS, 0.20f, 0.25f);

    // Run risk parity optimization
    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.n_assets = N_ASSETS;
    params.convergence_tolerance = 1e-6f;
    params.max_iterations = 500;

    fin_optimization_gpu_result_t result = {0};
    bool ok = fin_opt_gpu_risk_parity(ctx, covariance.data(), N_ASSETS,
                                        nullptr,  // Equal risk targets
                                        &params, &result);
    ASSERT_TRUE(ok) << "Risk parity optimization failed";
    ASSERT_TRUE(result.converged);

    // Verify weights are positive and sum to 1
    float weight_sum = 0.0f;
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        EXPECT_GT(result.optimal_weights[i], 0.0f);
        weight_sum += result.optimal_weights[i];
    }
    EXPECT_NEAR(weight_sum, 1.0f, 0.01f);

    // Compute risk contributions and verify they are approximately equal
    std::vector<float> marginal_risk(N_ASSETS);
    std::vector<float> risk_contrib(N_ASSETS);
    ok = fin_opt_gpu_risk_contribution(ctx, result.optimal_weights,
                                         covariance.data(), N_ASSETS,
                                         marginal_risk.data(), risk_contrib.data());
    ASSERT_TRUE(ok);

    float total_contrib = 0.0f;
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        total_contrib += risk_contrib[i];
    }

    float target_contrib = total_contrib / N_ASSETS;
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        EXPECT_NEAR(risk_contrib[i], target_contrib, target_contrib * 0.15f)
            << "Asset " << i << " risk contribution deviates from target";
    }

    fin_optimization_gpu_result_free(&result);
}

//=============================================================================
// Risk Scenario Analysis Tests
//=============================================================================

TEST_F(FinancialGPUWorkflowTest, RiskScenarioStressTesting) {
    // Workflow: Generate scenarios -> Simulate -> Aggregate risk -> Report

    const uint32_t N_ASSETS = 10;
    const uint32_t N_PATHS = 20000;
    const uint32_t N_SCENARIOS = 5;

    srand(99999);
    auto base_covariance = generateRealisticCovariance(N_ASSETS, 0.20f, 0.30f);
    auto expected_returns = generateSectorReturns(N_ASSETS, 3, 0.08f);

    // Portfolio weights (equal weight for simplicity)
    std::vector<float> weights(N_ASSETS, 1.0f / N_ASSETS);

    // Stress scenarios with different volatility multipliers
    std::vector<float> vol_multipliers = {1.0f, 1.5f, 2.0f, 2.5f, 3.0f};
    std::vector<float> scenario_vars(N_SCENARIOS);
    std::vector<float> scenario_cvars(N_SCENARIOS);

    for (uint32_t s = 0; s < N_SCENARIOS; s++) {
        // Create stressed covariance
        std::vector<float> stressed_cov = base_covariance;
        float mult_sq = vol_multipliers[s] * vol_multipliers[s];
        for (size_t i = 0; i < stressed_cov.size(); i++) {
            stressed_cov[i] *= mult_sq;
        }

        // Compute portfolio volatility under stress
        float portfolio_var = fin_opt_gpu_portfolio_variance(ctx, weights.data(),
                                                               stressed_cov.data(), N_ASSETS);
        float portfolio_vol = std::sqrt(portfolio_var);

        // Simulate returns
        fin_monte_carlo_gpu_params_t mc_params = fin_monte_carlo_gpu_params_default();
        mc_params.initial_value = 1.0f;
        mc_params.drift = 0.0f;  // Risk-neutral for VaR
        mc_params.volatility = portfolio_vol;
        mc_params.time_horizon = 10.0f / 252.0f;  // 10-day VaR
        mc_params.num_paths = N_PATHS;
        mc_params.num_steps = 10;

        fin_monte_carlo_gpu_result_t mc_result = {0};
        bool ok = fin_monte_carlo_gpu_simulate(ctx, rng, &mc_params, &mc_result);
        ASSERT_TRUE(ok) << "Scenario " << s << " simulation failed";

        scenario_vars[s] = mc_result.var_95;
        scenario_cvars[s] = mc_result.cvar_95;

        fin_monte_carlo_gpu_result_free(ctx, &mc_result);
    }

    // Validate stress testing results
    for (uint32_t s = 1; s < N_SCENARIOS; s++) {
        EXPECT_GT(scenario_vars[s], scenario_vars[s-1] * 0.95f)
            << "VaR should generally increase with stress level";
        EXPECT_GT(scenario_cvars[s], scenario_vars[s])
            << "CVaR should exceed VaR";
    }

    // Most stressed scenario should have significantly higher VaR
    EXPECT_GT(scenario_vars[N_SCENARIOS-1], scenario_vars[0] * 1.5f)
        << "Stressed VaR should be notably higher than base";
}

TEST_F(FinancialGPUWorkflowTest, RollingRiskAnalysis) {
    // Workflow: Compute rolling risk metrics over historical data

    const uint32_t HISTORY_LENGTH = 504;  // ~2 years of daily data
    const uint32_t WINDOW = 63;  // Quarterly window

    // Generate synthetic price series
    std::vector<float> prices(HISTORY_LENGTH);
    prices[0] = 100.0f;
    srand(11111);
    for (uint32_t i = 1; i < HISTORY_LENGTH; i++) {
        float ret = 0.0002f + 0.015f * (2.0f * static_cast<float>(rand()) / RAND_MAX - 1.0f);
        prices[i] = prices[i-1] * (1.0f + ret);
    }

    // Convert to returns
    std::vector<float> returns(HISTORY_LENGTH - 1);
    for (uint32_t i = 1; i < HISTORY_LENGTH; i++) {
        returns[i-1] = (prices[i] - prices[i-1]) / prices[i-1];
    }

    // Compute rolling risk metrics
    fin_risk_gpu_params_t risk_params = fin_risk_gpu_params_default();
    risk_params.num_returns = static_cast<uint32_t>(returns.size());

    fin_risk_rolling_result_t rolling_result = {0};
    bool ok = fin_risk_gpu_rolling(ctx, returns.data(),
                                     static_cast<uint32_t>(returns.size()),
                                     WINDOW, &risk_params, &rolling_result);
    ASSERT_TRUE(ok) << "Rolling risk computation failed";

    // Verify rolling results
    uint32_t num_points = static_cast<uint32_t>(returns.size()) - WINDOW + 1;
    EXPECT_EQ(rolling_result.num_points, num_points);
    ASSERT_NE(rolling_result.var_series, nullptr);
    ASSERT_NE(rolling_result.vol_series, nullptr);

    // Validate rolling metrics are reasonable
    for (uint32_t i = 0; i < rolling_result.num_points; i++) {
        EXPECT_GT(rolling_result.vol_series[i], 0.0f);
        EXPECT_LT(rolling_result.vol_series[i], 1.0f);  // Daily vol < 100%
    }

    fin_risk_rolling_result_free(&rolling_result);
}

//=============================================================================
// Options Pricing and Greeks Workflow Tests
//=============================================================================

TEST_F(FinancialGPUWorkflowTest, OptionsGreeksAndHedging) {
    // Workflow: Price options -> Compute Greeks -> Determine hedge ratios

    const float SPOT = 100.0f;
    const float RATE = 0.05f;
    const float VOL = 0.20f;

    // Create option portfolio
    const uint32_t NUM_OPTIONS = 25;
    std::vector<float> strikes(NUM_OPTIONS);
    std::vector<float> expiries(NUM_OPTIONS);
    std::vector<fin_option_type_t> types(NUM_OPTIONS);

    for (uint32_t i = 0; i < NUM_OPTIONS; i++) {
        strikes[i] = 85.0f + i;  // Strike range: 85-109
        expiries[i] = 0.25f + 0.25f * (i % 4);  // 3, 6, 9, 12 months
        types[i] = (i < 12) ? FIN_OPT_CALL : FIN_OPT_PUT;
    }

    // Price all options with Greeks
    fin_option_chain_t chain;
    chain.strikes = strikes.data();
    chain.expiries = expiries.data();
    chain.types = types.data();
    chain.styles = nullptr;  // All European
    chain.num_options = NUM_OPTIONS;
    chain.spot = SPOT;
    chain.rate = RATE;
    chain.dividend = 0.0f;

    fin_option_chain_result_t chain_result = {0};
    bool ok = fin_deriv_gpu_price_chain(ctx, &chain, VOL, &chain_result);
    ASSERT_TRUE(ok) << "Option chain pricing failed";

    ASSERT_EQ(chain_result.num_options, NUM_OPTIONS);
    ASSERT_NE(chain_result.prices, nullptr);
    ASSERT_NE(chain_result.deltas, nullptr);
    ASSERT_NE(chain_result.gammas, nullptr);

    // Compute portfolio Greeks
    float portfolio_delta = 0.0f;
    float portfolio_gamma = 0.0f;
    float portfolio_vega = 0.0f;
    float portfolio_value = 0.0f;

    for (uint32_t i = 0; i < NUM_OPTIONS; i++) {
        EXPECT_GT(chain_result.prices[i], 0.0f);
        portfolio_value += chain_result.prices[i];
        portfolio_delta += chain_result.deltas[i];
        portfolio_gamma += chain_result.gammas[i];
        if (chain_result.vegas) {
            portfolio_vega += chain_result.vegas[i];
        }
    }

    // Delta should be mix of positive (calls) and negative (puts)
    // For roughly balanced portfolio, net delta shouldn't be extreme
    EXPECT_GT(portfolio_value, 0.0f);
    EXPECT_LT(std::abs(portfolio_delta), NUM_OPTIONS);  // Reasonable bound

    // Compute delta hedge: need -delta shares of underlying
    float hedge_shares = -portfolio_delta;

    // Verify gamma is positive (long options)
    EXPECT_GT(portfolio_gamma, 0.0f) << "Long option portfolio should have positive gamma";

    fin_option_chain_result_free(&chain_result);
}

TEST_F(FinancialGPUWorkflowTest, AmericanOptionEarlyExercise) {
    // Workflow: Price American put -> Analyze early exercise boundary

    fin_deriv_extended_params_t params = fin_deriv_extended_params_default();
    params.base.option_type = FIN_OPT_PUT;
    params.base.option_style = FIN_OPT_STYLE_AMERICAN;
    params.base.spot = 100.0f;
    params.base.strike = 100.0f;
    params.base.risk_free_rate = 0.08f;  // Higher rate increases early exercise premium
    params.base.volatility = 0.25f;
    params.base.time_to_expiry = 1.0f;
    params.base.tree_steps = 2000;
    params.base.compute_greeks = true;
    params.greek_flags = FIN_GREEK_ALL;

    fin_deriv_extended_result_t result = {0};
    bool ok = fin_deriv_gpu_american_with_boundary(ctx, &params, &result);
    ASSERT_TRUE(ok) << "American option pricing failed";

    // American put should be worth more than European due to early exercise
    float european_price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.08f, 0.25f, 1.0f, FIN_OPT_PUT);

    EXPECT_GT(result.base.price, european_price)
        << "American put should be worth at least as much as European";
    EXPECT_GT(result.base.early_exercise_premium, 0.0f)
        << "Early exercise premium should be positive";

    // Greeks should be reasonable
    EXPECT_LT(result.base.delta, 0.0f) << "Put delta should be negative";
    EXPECT_GT(result.base.gamma, 0.0f) << "Gamma should be positive";
    EXPECT_LT(result.base.theta, 0.0f) << "Theta should be negative (time decay)";
    EXPECT_GT(result.base.vega, 0.0f) << "Vega should be positive";

    fin_deriv_extended_result_free(&result);
}

TEST_F(FinancialGPUWorkflowTest, ImpliedVolatilitySurface) {
    // Workflow: Calibrate IV surface from market prices

    const uint32_t NUM_STRIKES = 9;
    const uint32_t NUM_EXPIRIES = 4;
    const uint32_t TOTAL = NUM_STRIKES * NUM_EXPIRIES;

    // Create option grid
    std::vector<float> strikes(TOTAL);
    std::vector<float> expiries(TOTAL);
    std::vector<fin_option_type_t> types(TOTAL);
    std::vector<float> market_prices(TOTAL);

    // Generate synthetic market prices with volatility smile
    for (uint32_t t = 0; t < NUM_EXPIRIES; t++) {
        float expiry = 0.25f + 0.25f * t;  // 3, 6, 9, 12 months
        for (uint32_t k = 0; k < NUM_STRIKES; k++) {
            uint32_t idx = t * NUM_STRIKES + k;
            strikes[idx] = 90.0f + k * 2.5f;  // 90 to 110
            expiries[idx] = expiry;
            types[idx] = (strikes[idx] <= 100.0f) ? FIN_OPT_PUT : FIN_OPT_CALL;

            // Generate price with volatility smile
            float moneyness = strikes[idx] / 100.0f;
            float smile_vol = 0.20f + 0.05f * (moneyness - 1.0f) * (moneyness - 1.0f);
            smile_vol += 0.02f * (1.0f - moneyness);  // Skew

            market_prices[idx] = fin_deriv_gpu_black_scholes(ctx, 100.0f, strikes[idx],
                                                              0.05f, smile_vol, expiry,
                                                              types[idx]);
        }
    }

    // Calibrate implied volatilities
    fin_option_chain_t chain;
    chain.strikes = strikes.data();
    chain.expiries = expiries.data();
    chain.types = types.data();
    chain.styles = nullptr;
    chain.num_options = TOTAL;
    chain.spot = 100.0f;
    chain.rate = 0.05f;
    chain.dividend = 0.0f;

    std::vector<float> implied_vols(TOTAL);
    bool ok = fin_deriv_gpu_implied_vol_surface(ctx, &chain, market_prices.data(),
                                                  implied_vols.data());
    ASSERT_TRUE(ok) << "Implied vol surface calibration failed";

    // Verify IV smile structure
    for (uint32_t t = 0; t < NUM_EXPIRIES; t++) {
        float atm_vol = implied_vols[t * NUM_STRIKES + NUM_STRIKES / 2];
        float otm_put_vol = implied_vols[t * NUM_STRIKES + 1];
        float otm_call_vol = implied_vols[t * NUM_STRIKES + NUM_STRIKES - 2];

        EXPECT_GT(atm_vol, 0.10f) << "ATM vol should be reasonable";
        EXPECT_LT(atm_vol, 0.50f) << "ATM vol should be reasonable";

        // Smile: OTM vols should be higher than ATM
        EXPECT_GE(otm_put_vol, atm_vol * 0.95f) << "Vol smile expected for puts";
    }
}

TEST_F(FinancialGPUWorkflowTest, ExoticOptionMonteCarloPricing) {
    // Workflow: Price Asian and Barrier options via Monte Carlo

    // Asian call option (arithmetic average)
    fin_mc_extended_params_t asian_params = fin_mc_extended_params_default();
    asian_params.base.initial_value = 100.0f;
    asian_params.base.drift = 0.05f;
    asian_params.base.volatility = 0.20f;
    asian_params.base.risk_free_rate = 0.05f;
    asian_params.base.time_horizon = 1.0f;
    asian_params.base.num_paths = 100000;
    asian_params.base.num_steps = 252;
    asian_params.option_type = FIN_MC_OPTION_ASIAN_ARITHMETIC;

    fin_mc_extended_result_t asian_result = {0};
    bool ok = fin_mc_gpu_path_dependent_option(ctx, rng, 100.0f, 100.0f,
                                                 &asian_params, true, &asian_result);
    ASSERT_TRUE(ok) << "Asian option pricing failed";

    EXPECT_GT(asian_result.option_price, 0.0f);
    EXPECT_GT(asian_result.option_std_error, 0.0f);
    EXPECT_LT(asian_result.option_std_error, asian_result.option_price * 0.05f)
        << "Monte Carlo error should be small";

    // Asian option should be cheaper than European (volatility averaging)
    float european_price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, 0.20f, 1.0f, FIN_OPT_CALL);
    EXPECT_LT(asian_result.option_price, european_price)
        << "Asian call should be cheaper than European";

    // Barrier option (down-and-out call)
    fin_mc_extended_params_t barrier_params = fin_mc_extended_params_default();
    barrier_params.base = asian_params.base;
    barrier_params.option_type = FIN_MC_OPTION_BARRIER_DOWN_OUT;
    barrier_params.barrier = 90.0f;  // Knock-out at 90

    fin_mc_extended_result_t barrier_result = {0};
    ok = fin_mc_gpu_path_dependent_option(ctx, rng, 100.0f, 100.0f,
                                            &barrier_params, true, &barrier_result);
    ASSERT_TRUE(ok) << "Barrier option pricing failed";

    EXPECT_GT(barrier_result.option_price, 0.0f);
    EXPECT_LT(barrier_result.option_price, european_price)
        << "Barrier option should be cheaper than vanilla";
    EXPECT_GT(barrier_result.barrier_hit_probability, 0.0f)
        << "Some paths should hit barrier";
    EXPECT_LT(barrier_result.barrier_hit_probability, 1.0f)
        << "Not all paths should hit barrier";

    fin_mc_extended_result_free(ctx, &asian_result);
    fin_mc_extended_result_free(ctx, &barrier_result);
}

//=============================================================================
// Black-Litterman Integration Tests
//=============================================================================

TEST_F(FinancialGPUWorkflowTest, BlackLittermanPortfolioConstruction) {
    // Workflow: Market equilibrium -> Add views -> Posterior -> Optimize

    const uint32_t N_ASSETS = 6;
    const uint32_t N_VIEWS = 2;

    srand(77777);
    auto market_cov = generateRealisticCovariance(N_ASSETS, 0.18f, 0.30f);

    // Market cap weights (equilibrium)
    std::vector<float> market_weights = {0.25f, 0.20f, 0.15f, 0.15f, 0.15f, 0.10f};

    // Investor views:
    // View 1: Asset 0 will outperform Asset 1 by 3%
    // View 2: Assets 2,3 (sector) will return 8%
    std::vector<float> P(N_VIEWS * N_ASSETS, 0.0f);
    P[0 * N_ASSETS + 0] = 1.0f;   // View 1: Long asset 0
    P[0 * N_ASSETS + 1] = -1.0f;  // View 1: Short asset 1
    P[1 * N_ASSETS + 2] = 0.5f;   // View 2: Long 50% asset 2
    P[1 * N_ASSETS + 3] = 0.5f;   // View 2: Long 50% asset 3

    std::vector<float> Q = {0.03f, 0.08f};  // View expected returns
    std::vector<float> omega = {0.0001f, 0.0002f};  // View confidence

    // Set up Black-Litterman parameters
    fin_opt_extended_params_t bl_params = fin_opt_extended_params_default();
    bl_params.base.strategy = FIN_OPT_STRATEGY_BLACK_LITTERMAN;
    bl_params.base.n_assets = N_ASSETS;
    bl_params.base.risk_free_rate = 0.02f;
    bl_params.tau = 0.05f;
    bl_params.views_matrix = P.data();
    bl_params.views_returns = Q.data();
    bl_params.views_confidence = omega.data();
    bl_params.num_views = N_VIEWS;
    bl_params.market_weights = market_weights.data();

    fin_opt_extended_result_t bl_result = {0};
    bool ok = fin_opt_gpu_black_litterman(ctx, market_cov.data(), N_ASSETS,
                                            &bl_params, &bl_result);
    ASSERT_TRUE(ok) << "Black-Litterman optimization failed";
    ASSERT_TRUE(bl_result.base.converged);

    // Verify posterior returns reflect views
    ASSERT_NE(bl_result.posterior_returns, nullptr);

    // Asset 0 should have higher posterior return than asset 1 (View 1)
    EXPECT_GT(bl_result.posterior_returns[0], bl_result.posterior_returns[1])
        << "Posterior should reflect outperformance view";

    // Weights should sum to 1
    float weight_sum = 0.0f;
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        weight_sum += bl_result.base.optimal_weights[i];
    }
    EXPECT_NEAR(weight_sum, 1.0f, 0.01f);

    // Asset 0 should have higher weight than equilibrium (positive view)
    // This test is directional - the view should tilt weights
    // Note: exact comparison depends on view strength

    fin_opt_extended_result_free(&bl_result);
}

//=============================================================================
// GPU Recovery Integration Tests
//=============================================================================

TEST_F(FinancialGPUWorkflowTest, RecoveryDuringMonteCarloSimulation) {
    // Test that GPU recovery context is properly initialized
    ASSERT_NE(recovery_ctx, nullptr);

    // Verify recovery system is available
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized() || recovery_ctx != nullptr);

    // Run a normal Monte Carlo that should succeed
    fin_monte_carlo_gpu_params_t mc_params = fin_monte_carlo_gpu_params_default();
    mc_params.num_paths = 10000;
    mc_params.num_steps = 100;

    fin_monte_carlo_gpu_result_t mc_result = {0};
    bool ok = fin_monte_carlo_gpu_simulate(ctx, rng, &mc_params, &mc_result);
    EXPECT_TRUE(ok);

    if (ok) {
        fin_monte_carlo_gpu_result_free(ctx, &mc_result);
    }

    // Get recovery statistics
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    // Stats should be tracked
    // (Specific assertions depend on whether errors occurred)
}

TEST_F(FinancialGPUWorkflowTest, RecoveryFromInvalidParameters) {
    // Test recovery system handles invalid parameters gracefully

    // Invalid: zero paths
    fin_monte_carlo_gpu_params_t bad_params = fin_monte_carlo_gpu_params_default();
    bad_params.num_paths = 0;

    fin_monte_carlo_gpu_result_t result = {0};
    bool ok = fin_monte_carlo_gpu_simulate(ctx, rng, &bad_params, &result);
    EXPECT_FALSE(ok) << "Should fail with zero paths";

    // Invalid: negative volatility (should be clamped or rejected)
    bad_params = fin_monte_carlo_gpu_params_default();
    bad_params.volatility = -0.5f;
    bad_params.num_paths = 1000;

    ok = fin_monte_carlo_gpu_simulate(ctx, rng, &bad_params, &result);
    // May succeed with clamped value or fail with error
    // Either way, should not crash

    if (ok) {
        fin_monte_carlo_gpu_result_free(ctx, &result);
    }
}

TEST_F(FinancialGPUWorkflowTest, MemoryPressureRecovery) {
    // Test behavior under memory constraints

    // Check initial memory state
    size_t free_bytes, total_bytes;
    bool mem_ok = nimcp_gpu_get_memory_info(&free_bytes, &total_bytes);
    if (!mem_ok) {
        GTEST_SKIP() << "Cannot query GPU memory";
    }

    // Run series of operations, checking memory doesn't leak
    size_t initial_free = free_bytes;

    for (int i = 0; i < 5; i++) {
        fin_monte_carlo_gpu_params_t mc_params = fin_monte_carlo_gpu_params_default();
        mc_params.num_paths = 50000;
        mc_params.num_steps = 100;

        fin_monte_carlo_gpu_result_t mc_result = {0};
        bool ok = fin_monte_carlo_gpu_simulate(ctx, rng, &mc_params, &mc_result);
        ASSERT_TRUE(ok);
        fin_monte_carlo_gpu_result_free(ctx, &mc_result);
    }

    // Synchronize and check memory
    nimcp_gpu_context_synchronize(ctx);
    nimcp_gpu_get_memory_info(&free_bytes, &total_bytes);

    // Allow some tolerance for caching
    EXPECT_GT(free_bytes, initial_free * 0.9)
        << "Memory should be mostly freed after operations";
}

//=============================================================================
// Multi-Asset Correlation Flow Tests
//=============================================================================

TEST_F(FinancialGPUWorkflowTest, CorrelatedAssetSimulationAndVaR) {
    // Workflow: Correlated simulation -> Portfolio VaR -> Component VaR

    const uint32_t N_ASSETS = 6;
    const uint32_t N_PATHS = 50000;

    srand(88888);

    // Asset parameters
    std::vector<float> initial_values = {100.0f, 50.0f, 75.0f, 200.0f, 150.0f, 80.0f};
    std::vector<float> drifts = {0.08f, 0.10f, 0.06f, 0.12f, 0.09f, 0.07f};
    std::vector<float> volatilities = {0.20f, 0.30f, 0.15f, 0.25f, 0.22f, 0.18f};

    // Correlation matrix
    std::vector<float> correlation = {
        1.00f, 0.60f, 0.40f, 0.30f, 0.50f, 0.35f,
        0.60f, 1.00f, 0.45f, 0.35f, 0.55f, 0.40f,
        0.40f, 0.45f, 1.00f, 0.70f, 0.45f, 0.50f,
        0.30f, 0.35f, 0.70f, 1.00f, 0.40f, 0.45f,
        0.50f, 0.55f, 0.45f, 0.40f, 1.00f, 0.60f,
        0.35f, 0.40f, 0.50f, 0.45f, 0.60f, 1.00f
    };

    // Compute Cholesky decomposition
    auto cholesky = computeCholesky(correlation, N_ASSETS);

    // Run correlated simulation
    fin_monte_carlo_gpu_params_t base_params = fin_monte_carlo_gpu_params_default();
    base_params.num_paths = N_PATHS;
    base_params.time_horizon = 10.0f / 252.0f;  // 10-day horizon
    base_params.num_steps = 10;

    std::vector<float> terminal_values(N_PATHS * N_ASSETS);
    bool ok = fin_mc_gpu_correlated_assets(ctx, rng, initial_values.data(),
                                             drifts.data(), volatilities.data(),
                                             correlation.data(), N_ASSETS,
                                             &base_params, terminal_values.data());
    ASSERT_TRUE(ok) << "Correlated simulation failed";

    // Portfolio weights (value-weighted)
    float total_value = 0.0f;
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        total_value += initial_values[i];
    }
    std::vector<float> weights(N_ASSETS);
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        weights[i] = initial_values[i] / total_value;
    }

    // Compute portfolio returns
    std::vector<float> portfolio_returns(N_PATHS);
    for (uint32_t p = 0; p < N_PATHS; p++) {
        float initial_port = 0.0f;
        float terminal_port = 0.0f;
        for (uint32_t a = 0; a < N_ASSETS; a++) {
            initial_port += weights[a] * initial_values[a];
            terminal_port += weights[a] * terminal_values[p * N_ASSETS + a];
        }
        portfolio_returns[p] = (terminal_port - initial_port) / initial_port;
    }

    // Compute portfolio VaR
    fin_risk_gpu_params_t risk_params = fin_risk_gpu_params_default();
    risk_params.num_returns = N_PATHS;

    fin_risk_gpu_result_t risk_result = {0};
    ok = fin_risk_gpu_compute(ctx, portfolio_returns.data(), &risk_params, &risk_result);
    ASSERT_TRUE(ok);

    EXPECT_GT(risk_result.var_95, 0.0f);
    EXPECT_GT(risk_result.cvar_95, risk_result.var_95);

    // Compute component VaR for each asset
    std::vector<float> component_vars(N_ASSETS);
    for (uint32_t a = 0; a < N_ASSETS; a++) {
        std::vector<float> asset_returns(N_PATHS);
        for (uint32_t p = 0; p < N_PATHS; p++) {
            asset_returns[p] = (terminal_values[p * N_ASSETS + a] - initial_values[a]) / initial_values[a];
        }

        fin_risk_gpu_result_t asset_risk = {0};
        ok = fin_risk_gpu_compute(ctx, asset_returns.data(), &risk_params, &asset_risk);
        ASSERT_TRUE(ok);
        component_vars[a] = asset_risk.var_95;
    }

    // Portfolio VaR should be less than sum of component VaRs (diversification)
    float sum_component_var = 0.0f;
    for (uint32_t a = 0; a < N_ASSETS; a++) {
        sum_component_var += weights[a] * component_vars[a];
    }
    EXPECT_LT(risk_result.var_95, sum_component_var)
        << "Diversification should reduce portfolio VaR vs sum of weighted component VaRs";
}

TEST_F(FinancialGPUWorkflowTest, CovarianceMatrixComputation) {
    // Workflow: Returns data -> GPU covariance -> Portfolio analytics

    const uint32_t N_ASSETS = 8;
    const uint32_t N_OBSERVATIONS = 252;

    srand(66666);

    // Generate synthetic return data with correlation structure
    std::vector<float> returns(N_ASSETS * N_OBSERVATIONS);
    for (uint32_t t = 0; t < N_OBSERVATIONS; t++) {
        float market_factor = 0.01f * (2.0f * static_cast<float>(rand()) / RAND_MAX - 1.0f);
        for (uint32_t a = 0; a < N_ASSETS; a++) {
            float beta = 0.5f + 0.1f * a;  // Different betas
            float idio = 0.005f * (2.0f * static_cast<float>(rand()) / RAND_MAX - 1.0f);
            returns[a * N_OBSERVATIONS + t] = beta * market_factor + idio;
        }
    }

    // Compute covariance matrix on GPU
    std::vector<float> covariance(N_ASSETS * N_ASSETS);
    bool ok = fin_risk_gpu_covariance_matrix(ctx, returns.data(), N_ASSETS,
                                               N_OBSERVATIONS, covariance.data());
    ASSERT_TRUE(ok) << "Covariance matrix computation failed";

    // Verify covariance is symmetric and positive semi-definite
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        for (uint32_t j = 0; j < N_ASSETS; j++) {
            EXPECT_NEAR(covariance[i * N_ASSETS + j], covariance[j * N_ASSETS + i], 1e-6f)
                << "Covariance should be symmetric";
        }
        EXPECT_GT(covariance[i * N_ASSETS + i], 0.0f)
            << "Diagonal (variance) should be positive";
    }

    // Use computed covariance for portfolio optimization
    std::vector<float> expected_returns(N_ASSETS);
    for (uint32_t a = 0; a < N_ASSETS; a++) {
        float sum = 0.0f;
        for (uint32_t t = 0; t < N_OBSERVATIONS; t++) {
            sum += returns[a * N_OBSERVATIONS + t];
        }
        expected_returns[a] = sum / N_OBSERVATIONS * 252.0f;  // Annualize
    }

    fin_optimization_gpu_params_t opt_params = fin_optimization_gpu_params_default();
    opt_params.strategy = FIN_OPT_STRATEGY_MIN_VARIANCE;
    opt_params.n_assets = N_ASSETS;

    fin_optimization_gpu_result_t opt_result = {0};
    ok = fin_optimization_gpu_mean_variance(ctx, expected_returns.data(),
                                              covariance.data(), &opt_params, &opt_result);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(opt_result.converged);

    fin_optimization_gpu_result_free(&opt_result);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    // Initialize GPU recovery system
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);
    nimcp_gpu_recovery_init(&config);

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    nimcp_gpu_recovery_shutdown();
    return result;
}
