//=============================================================================
// test_financial_gpu_integration.cpp - Integration Tests for GPU Financial
//=============================================================================
/**
 * @file test_financial_gpu_integration.cpp
 * @brief Integration tests for GPU-accelerated financial computations
 *
 * Tests complete financial workflows combining Monte Carlo, optimization,
 * risk metrics, and derivatives pricing with CPU-GPU equivalence validation.
 *
 * COVERAGE:
 *   - Complete Monte Carlo simulation workflow
 *   - Portfolio optimization pipeline
 *   - Risk-adjusted portfolio analysis
 *   - Efficient frontier computation
 *   - Option pricing with Greeks
 *   - CPU vs GPU result equivalence
 *   - Memory management across operations
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
// Test Fixture
//=============================================================================

class FinancialGPUIntegrationTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    fin_gpu_rng_t* rng = nullptr;

    void SetUp() override {
        ctx = nimcp_gpu_context_create(0);
        if (!ctx) {
            GTEST_SKIP() << "No GPU available - skipping test";
        }

        rng = fin_gpu_rng_create(ctx, 100000, 42);
        if (!rng) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
            GTEST_SKIP() << "Failed to create GPU RNG - skipping test";
        }
    }

    void TearDown() override {
        if (rng) {
            fin_gpu_rng_destroy(rng);
            rng = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Generate covariance matrix for testing
    std::vector<float> generateCovarianceMatrix(uint32_t n_assets, float base_vol = 0.2f) {
        std::vector<float> cov(n_assets * n_assets);

        for (uint32_t i = 0; i < n_assets; i++) {
            for (uint32_t j = 0; j < n_assets; j++) {
                if (i == j) {
                    // Variance on diagonal
                    cov[i * n_assets + j] = base_vol * base_vol * (1.0f + 0.1f * i);
                } else {
                    // Correlation-based covariance (positive correlation)
                    float corr = 0.3f + 0.1f * std::min(i, j) / n_assets;
                    float vol_i = base_vol * std::sqrt(1.0f + 0.1f * i);
                    float vol_j = base_vol * std::sqrt(1.0f + 0.1f * j);
                    cov[i * n_assets + j] = corr * vol_i * vol_j;
                }
            }
        }
        return cov;
    }

    // Generate expected returns for testing
    std::vector<float> generateExpectedReturns(uint32_t n_assets, float base_return = 0.08f) {
        std::vector<float> returns(n_assets);
        for (uint32_t i = 0; i < n_assets; i++) {
            returns[i] = base_return + 0.02f * (static_cast<float>(i) / n_assets - 0.5f);
        }
        return returns;
    }
};

//=============================================================================
// Complete Workflow Tests
//=============================================================================

TEST_F(FinancialGPUIntegrationTest, MonteCarloToRiskAnalysis) {
    // Step 1: Run Monte Carlo simulation
    fin_monte_carlo_gpu_params_t mc_params = fin_monte_carlo_gpu_params_default();
    mc_params.initial_value = 100000.0f;
    mc_params.drift = 0.08f;
    mc_params.volatility = 0.20f;
    mc_params.time_horizon = 1.0f;
    mc_params.num_paths = 50000;
    mc_params.num_steps = 252;
    mc_params.antithetic = true;

    fin_monte_carlo_gpu_result_t mc_result = {0};
    bool ok = fin_monte_carlo_gpu_simulate(ctx, rng, &mc_params, &mc_result);
    ASSERT_TRUE(ok) << "Monte Carlo simulation failed";

    // Verify Monte Carlo results
    EXPECT_GT(mc_result.mean_value, 100000.0f) << "Expected growth over 1 year";
    EXPECT_GT(mc_result.std_return, 0.0f) << "Should have volatility";
    EXPECT_EQ(mc_result.paths_completed, mc_params.num_paths);

    // Step 2: Compute additional risk metrics from terminal values
    ASSERT_NE(mc_result.terminal_values, nullptr);

    // Download terminal values for CPU analysis
    std::vector<float> terminal_values(mc_params.num_paths);
    cudaMemcpy(terminal_values.data(), mc_result.terminal_values,
               mc_params.num_paths * sizeof(float), cudaMemcpyDeviceToHost);

    // Convert to returns
    std::vector<float> returns(mc_params.num_paths);
    for (uint32_t i = 0; i < mc_params.num_paths; i++) {
        returns[i] = (terminal_values[i] - mc_params.initial_value) / mc_params.initial_value;
    }

    // Step 3: Use GPU risk metrics on returns
    fin_risk_gpu_params_t risk_params = fin_risk_gpu_params_default();
    risk_params.num_returns = mc_params.num_paths;
    risk_params.compute_var = true;
    risk_params.compute_cvar = true;

    fin_risk_gpu_result_t risk_result = {0};
    ok = fin_risk_gpu_compute(ctx, returns.data(), &risk_params, &risk_result);
    ASSERT_TRUE(ok) << "Risk computation failed";

    // Verify risk metrics are consistent with MC output
    EXPECT_NEAR(risk_result.var_95, mc_result.var_95, 0.02f) << "VaR should be consistent";
    EXPECT_GT(risk_result.cvar_95, risk_result.var_95) << "CVaR >= VaR";

    fin_monte_carlo_gpu_result_free(ctx, &mc_result);
}

TEST_F(FinancialGPUIntegrationTest, PortfolioOptimizationToRisk) {
    const uint32_t N_ASSETS = 10;

    // Step 1: Generate market data
    auto expected_returns = generateExpectedReturns(N_ASSETS, 0.10f);
    auto covariance = generateCovarianceMatrix(N_ASSETS, 0.20f);

    // Step 2: Run portfolio optimization
    fin_optimization_gpu_params_t opt_params = fin_optimization_gpu_params_default();
    opt_params.strategy = FIN_OPT_STRATEGY_MAX_SHARPE;
    opt_params.risk_free_rate = 0.03f;
    opt_params.n_assets = N_ASSETS;
    opt_params.long_only = true;
    opt_params.max_iterations = 500;

    fin_optimization_gpu_result_t opt_result = {0};
    bool ok = fin_optimization_gpu_mean_variance(ctx, expected_returns.data(),
                                                   covariance.data(), &opt_params, &opt_result);
    ASSERT_TRUE(ok) << "Portfolio optimization failed";

    EXPECT_TRUE(opt_result.converged) << "Optimization should converge";
    EXPECT_GT(opt_result.sharpe_ratio, 0.0f) << "Sharpe should be positive";

    // Verify weights sum to 1
    float weight_sum = 0.0f;
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        weight_sum += opt_result.optimal_weights[i];
        EXPECT_GE(opt_result.optimal_weights[i], -0.001f) << "Long-only violated at " << i;
    }
    EXPECT_NEAR(weight_sum, 1.0f, 0.01f) << "Weights should sum to 1";

    // Step 3: Simulate portfolio with Monte Carlo
    fin_monte_carlo_gpu_params_t mc_params = fin_monte_carlo_gpu_params_default();
    mc_params.initial_value = 1000000.0f;
    mc_params.drift = opt_result.expected_return;
    mc_params.volatility = opt_result.portfolio_volatility;
    mc_params.time_horizon = 1.0f;
    mc_params.num_paths = 10000;

    fin_monte_carlo_gpu_result_t mc_result = {0};
    ok = fin_monte_carlo_gpu_simulate(ctx, rng, &mc_params, &mc_result);
    ASSERT_TRUE(ok);

    // Verify simulated returns match expected
    EXPECT_NEAR(mc_result.mean_return, opt_result.expected_return, 0.02f);

    fin_optimization_gpu_result_free(&opt_result);
    fin_monte_carlo_gpu_result_free(ctx, &mc_result);
}

TEST_F(FinancialGPUIntegrationTest, EfficientFrontierAnalysis) {
    const uint32_t N_ASSETS = 8;
    const uint32_t N_POINTS = 20;

    auto expected_returns = generateExpectedReturns(N_ASSETS, 0.12f);
    auto covariance = generateCovarianceMatrix(N_ASSETS, 0.25f);

    fin_optimization_gpu_params_t opt_params = fin_optimization_gpu_params_default();
    opt_params.n_assets = N_ASSETS;
    opt_params.long_only = true;
    opt_params.risk_free_rate = 0.02f;

    fin_efficient_frontier_result_t frontier_result = {0};
    bool ok = fin_optimization_gpu_efficient_frontier(ctx, expected_returns.data(),
                                                        covariance.data(), &opt_params,
                                                        N_POINTS, &frontier_result);
    ASSERT_TRUE(ok) << "Efficient frontier computation failed";

    ASSERT_EQ(frontier_result.num_points, N_POINTS);
    ASSERT_NE(frontier_result.returns, nullptr);
    ASSERT_NE(frontier_result.volatilities, nullptr);
    ASSERT_NE(frontier_result.sharpe_ratios, nullptr);
    ASSERT_NE(frontier_result.weights, nullptr);

    // Verify frontier is monotonically increasing in return
    for (uint32_t i = 1; i < N_POINTS; i++) {
        EXPECT_GE(frontier_result.returns[i], frontier_result.returns[i-1] - 0.001f)
            << "Frontier should be sorted by return";
    }

    // Find max Sharpe portfolio
    float max_sharpe = 0.0f;
    uint32_t max_sharpe_idx = 0;
    for (uint32_t i = 0; i < N_POINTS; i++) {
        if (frontier_result.sharpe_ratios[i] > max_sharpe) {
            max_sharpe = frontier_result.sharpe_ratios[i];
            max_sharpe_idx = i;
        }
    }

    EXPECT_GT(max_sharpe, 0.0f) << "Max Sharpe should be positive";

    // Verify weights for max Sharpe portfolio
    float weight_sum = 0.0f;
    for (uint32_t j = 0; j < N_ASSETS; j++) {
        weight_sum += frontier_result.weights[max_sharpe_idx * N_ASSETS + j];
    }
    EXPECT_NEAR(weight_sum, 1.0f, 0.01f);

    // Free result (need to manually free arrays)
    free(frontier_result.returns);
    free(frontier_result.volatilities);
    free(frontier_result.sharpe_ratios);
    free(frontier_result.weights);
}

// NOTE: OptionPricingWithRiskManagement test removed
// fin_derivatives_gpu_batch_price not yet implemented

TEST_F(FinancialGPUIntegrationTest, MultiAssetMonteCarloPortfolio) {
    const uint32_t N_ASSETS = 5;
    const uint32_t N_PATHS = 10000;

    // Generate correlated asset simulation parameters
    std::vector<float> initial_values = {100.0f, 50.0f, 75.0f, 120.0f, 80.0f};
    std::vector<float> drifts = {0.08f, 0.10f, 0.06f, 0.12f, 0.09f};
    std::vector<float> volatilities = {0.20f, 0.30f, 0.15f, 0.25f, 0.22f};

    // Correlation matrix -> Cholesky factor
    std::vector<float> correlation = {
        1.0f, 0.5f, 0.3f, 0.2f, 0.4f,
        0.5f, 1.0f, 0.4f, 0.3f, 0.5f,
        0.3f, 0.4f, 1.0f, 0.6f, 0.3f,
        0.2f, 0.3f, 0.6f, 1.0f, 0.4f,
        0.4f, 0.5f, 0.3f, 0.4f, 1.0f
    };

    // Cholesky decomposition (lower triangular)
    std::vector<float> cholesky_L(N_ASSETS * N_ASSETS, 0.0f);
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        for (uint32_t j = 0; j <= i; j++) {
            float sum = correlation[i * N_ASSETS + j];
            for (uint32_t k = 0; k < j; k++) {
                sum -= cholesky_L[i * N_ASSETS + k] * cholesky_L[j * N_ASSETS + k];
            }
            if (i == j) {
                cholesky_L[i * N_ASSETS + j] = std::sqrt(sum);
            } else {
                cholesky_L[i * N_ASSETS + j] = sum / cholesky_L[j * N_ASSETS + j];
            }
        }
    }

    fin_multi_asset_params_t params;
    params.base = fin_monte_carlo_gpu_params_default();
    params.base.num_paths = N_PATHS;
    params.base.time_horizon = 1.0f;
    params.base.num_steps = 252;
    params.n_assets = N_ASSETS;
    params.initial_values = initial_values.data();
    params.drifts = drifts.data();
    params.volatilities = volatilities.data();
    params.cholesky_L = cholesky_L.data();

    std::vector<float> terminal_values(N_PATHS * N_ASSETS);

    bool ok = fin_monte_carlo_gpu_multi_asset(ctx, rng, &params, terminal_values.data());
    ASSERT_TRUE(ok) << "Multi-asset Monte Carlo failed";

    // Verify each asset has reasonable terminal values
    for (uint32_t a = 0; a < N_ASSETS; a++) {
        float sum = 0.0f;
        float min_val = 1e10f, max_val = 0.0f;
        for (uint32_t p = 0; p < N_PATHS; p++) {
            float val = terminal_values[p * N_ASSETS + a];
            sum += val;
            min_val = std::min(min_val, val);
            max_val = std::max(max_val, val);
        }
        float mean = sum / N_PATHS;

        // Mean should be close to drift-adjusted initial value
        float expected_mean = initial_values[a] * std::exp(drifts[a]);
        EXPECT_NEAR(mean, expected_mean, expected_mean * 0.15f)
            << "Asset " << a << " mean terminal value unexpected";
    }

    // Compute portfolio returns for different weight vectors
    std::vector<float> weights = {0.2f, 0.2f, 0.2f, 0.2f, 0.2f};  // Equal weight
    float initial_portfolio = 0.0f;
    for (uint32_t a = 0; a < N_ASSETS; a++) {
        initial_portfolio += weights[a] * initial_values[a];
    }

    std::vector<float> portfolio_returns(N_PATHS);
    for (uint32_t p = 0; p < N_PATHS; p++) {
        float terminal_portfolio = 0.0f;
        for (uint32_t a = 0; a < N_ASSETS; a++) {
            terminal_portfolio += weights[a] * terminal_values[p * N_ASSETS + a];
        }
        portfolio_returns[p] = (terminal_portfolio - initial_portfolio) / initial_portfolio;
    }

    // Compute portfolio VaR
    fin_risk_gpu_params_t risk_params = fin_risk_gpu_params_default();
    risk_params.num_returns = N_PATHS;

    fin_risk_gpu_result_t risk_result = {0};
    ok = fin_risk_gpu_compute(ctx, portfolio_returns.data(), &risk_params, &risk_result);
    ASSERT_TRUE(ok);

    EXPECT_GT(risk_result.var_95, 0.0f);
    EXPECT_GT(risk_result.sharpe_ratio, -10.0f);  // Reasonable Sharpe
}

TEST_F(FinancialGPUIntegrationTest, HestonModelOptionPricing) {
    // Price option using Heston stochastic volatility model
    fin_heston_params_t heston_params;
    heston_params.base = fin_monte_carlo_gpu_params_default();
    heston_params.base.process_type = FIN_GPU_PROCESS_HESTON;
    heston_params.base.initial_value = 100.0f;
    heston_params.base.risk_free_rate = 0.05f;
    heston_params.base.time_horizon = 1.0f;
    heston_params.base.num_paths = 50000;
    heston_params.base.num_steps = 500;

    // Heston parameters
    heston_params.initial_variance = 0.04f;  // v0 = 0.04 (vol = 20%)
    heston_params.kappa = 2.0f;              // Mean reversion speed
    heston_params.theta = 0.04f;             // Long-term variance
    heston_params.xi = 0.3f;                 // Vol of vol
    heston_params.rho = -0.7f;               // Correlation (typically negative)

    fin_monte_carlo_gpu_result_t mc_result = {0};
    bool ok = fin_monte_carlo_gpu_heston(ctx, rng, &heston_params, &mc_result);
    ASSERT_TRUE(ok) << "Heston simulation failed";

    // Download terminal values
    std::vector<float> terminal_values(heston_params.base.num_paths);
    cudaMemcpy(terminal_values.data(), mc_result.terminal_values,
               heston_params.base.num_paths * sizeof(float), cudaMemcpyDeviceToHost);

    // Price European call with strike 100
    float strike = 100.0f;
    float discount = std::exp(-heston_params.base.risk_free_rate * heston_params.base.time_horizon);
    float sum_payoff = 0.0f;
    for (uint32_t i = 0; i < heston_params.base.num_paths; i++) {
        float payoff = std::max(terminal_values[i] - strike, 0.0f);
        sum_payoff += payoff;
    }
    float mc_price = discount * sum_payoff / heston_params.base.num_paths;

    // Compare with Black-Scholes (should be different due to stochastic vol)
    float bs_price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, 0.2f, 1.0f, FIN_OPT_CALL);

    // Prices should be somewhat similar but not identical
    EXPECT_GT(mc_price, 0.0f) << "MC price should be positive";
    EXPECT_GT(bs_price, 0.0f) << "BS price should be positive";
    EXPECT_NEAR(mc_price, bs_price, bs_price * 0.25f) << "Heston and BS should be roughly similar";

    fin_monte_carlo_gpu_result_free(ctx, &mc_result);
}

//=============================================================================
// CPU vs GPU Equivalence Tests
//=============================================================================

TEST_F(FinancialGPUIntegrationTest, RiskMetricsConsistency) {
    // Generate returns
    const uint32_t N = 1000;
    std::vector<float> returns(N);
    srand(12345);
    for (uint32_t i = 0; i < N; i++) {
        // Simple normal approximation
        float u1 = (static_cast<float>(rand()) / RAND_MAX);
        float u2 = (static_cast<float>(rand()) / RAND_MAX);
        if (u1 < 1e-10f) u1 = 1e-10f;
        float z = std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * M_PI * u2);
        returns[i] = 0.0001f + 0.02f * z;  // Small positive drift
    }

    // GPU risk metrics
    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = N;

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
    ASSERT_TRUE(ok);

    // CPU reference calculations
    float cpu_mean = 0.0f;
    for (float r : returns) cpu_mean += r;
    cpu_mean /= N;

    float cpu_var = 0.0f;
    for (float r : returns) {
        float diff = r - cpu_mean;
        cpu_var += diff * diff;
    }
    cpu_var /= (N - 1);
    float cpu_vol = std::sqrt(cpu_var);

    // Verify
    EXPECT_NEAR(result.mean_return, cpu_mean, 1e-5f) << "Mean mismatch";
    EXPECT_NEAR(result.volatility, cpu_vol, 1e-4f) << "Volatility mismatch";
}

TEST_F(FinancialGPUIntegrationTest, BinomialVsBlackScholes) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.option_style = FIN_OPT_STYLE_EUROPEAN;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.20f;
    params.time_to_expiry = 1.0f;
    params.tree_steps = 2000;  // High precision

    fin_derivatives_gpu_result_t tree_result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &tree_result);
    ASSERT_TRUE(ok);

    float bs_price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, 0.20f, 1.0f, FIN_OPT_CALL);

    // Should converge closely
    EXPECT_NEAR(tree_result.price, bs_price, 0.05f)
        << "Binomial tree should converge to Black-Scholes";
}

// NOTE: StatisticsCollection test removed
// fin_gpu_reset_stats and fin_gpu_get_stats not yet implemented

//=============================================================================
// Memory Management Tests
//=============================================================================

TEST_F(FinancialGPUIntegrationTest, RepeatedOperationsMemoryStability) {
    // Run multiple operations to check for memory leaks
    for (int iter = 0; iter < 10; iter++) {
        // Monte Carlo
        fin_monte_carlo_gpu_params_t mc_params = fin_monte_carlo_gpu_params_default();
        mc_params.num_paths = 1000;
        fin_monte_carlo_gpu_result_t mc_result = {0};
        bool ok = fin_monte_carlo_gpu_simulate(ctx, rng, &mc_params, &mc_result);
        ASSERT_TRUE(ok);
        fin_monte_carlo_gpu_result_free(ctx, &mc_result);

        // Optimization
        auto returns = generateExpectedReturns(4);
        auto cov = generateCovarianceMatrix(4);
        fin_optimization_gpu_params_t opt_params = fin_optimization_gpu_params_default();
        opt_params.n_assets = 4;
        fin_optimization_gpu_result_t opt_result = {0};
        ok = fin_optimization_gpu_mean_variance(ctx, returns.data(), cov.data(), &opt_params, &opt_result);
        ASSERT_TRUE(ok);
        fin_optimization_gpu_result_free(&opt_result);

        // Derivatives
        fin_derivatives_gpu_params_t deriv_params = fin_derivatives_gpu_params_default();
        fin_derivatives_gpu_result_t deriv_result = {0};
        ok = fin_derivatives_gpu_binomial_tree(ctx, &deriv_params, &deriv_result);
        ASSERT_TRUE(ok);
    }

    // If we got here without crashing, memory management is working
    SUCCEED();
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(FinancialGPUIntegrationTest, InvalidParameterHandling) {
    // Null context
    fin_monte_carlo_gpu_params_t mc_params = fin_monte_carlo_gpu_params_default();
    fin_monte_carlo_gpu_result_t mc_result = {0};
    bool ok = fin_monte_carlo_gpu_simulate(nullptr, rng, &mc_params, &mc_result);
    EXPECT_FALSE(ok);

    // Zero paths
    mc_params.num_paths = 0;
    ok = fin_monte_carlo_gpu_simulate(ctx, rng, &mc_params, &mc_result);
    EXPECT_FALSE(ok);
}

// NOTE: Availability test removed
// fin_gpu_is_available, fin_gpu_recommended_mc_paths, fin_gpu_max_optimization_assets not yet implemented

//=============================================================================
// Recovery Integration Tests
//=============================================================================

TEST_F(FinancialGPUIntegrationTest, RecoveryContextInitialization) {
    // Initialize GPU recovery system
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);

    // Verify default configuration
    EXPECT_TRUE(config.enable_cpu_fallback);
    EXPECT_TRUE(config.enable_param_correction);
    EXPECT_TRUE(config.enable_batch_reduction);
    EXPECT_TRUE(config.enable_retry);
    EXPECT_EQ(config.max_retries, 3u);

    // Create recovery context
    nimcp_gpu_recovery_context_t* recovery_ctx = nimcp_gpu_recovery_context_create(&config);
    ASSERT_NE(recovery_ctx, nullptr);

    // Verify context is usable
    nimcp_gpu_recovery_context_reset(recovery_ctx);

    nimcp_gpu_recovery_context_destroy(recovery_ctx);
}

TEST_F(FinancialGPUIntegrationTest, RecoveryFromParameterErrors) {
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);
    config.enable_param_correction = true;

    nimcp_gpu_recovery_context_t* recovery_ctx = nimcp_gpu_recovery_context_create(&config);
    ASSERT_NE(recovery_ctx, nullptr);

    // Test parameter correction
    float vol = -0.5f;  // Invalid negative volatility
    nimcp_gpu_param_range_t range = {0.0001f, 5.0f, 0.2f, true};
    bool corrected = nimcp_gpu_correct_param_float(&vol, &range, "volatility");

    EXPECT_TRUE(corrected);
    EXPECT_GE(vol, 0.0001f);
    EXPECT_LE(vol, 5.0f);

    // Test integer parameter correction
    int batch_size = -100;  // Invalid
    corrected = nimcp_gpu_correct_param_int(&batch_size, 1, 1000000, 1000, "batch_size");
    EXPECT_TRUE(corrected);
    EXPECT_GE(batch_size, 1);

    nimcp_gpu_recovery_context_destroy(recovery_ctx);
}

TEST_F(FinancialGPUIntegrationTest, RecoveryStrategySelection) {
    // Test recovery strategy selection for different error types
    nimcp_gpu_recovery_action_t action;

    // Invalid parameters should suggest clamping
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_INVALID_PARAMS, cudaSuccess, 0);
    EXPECT_EQ(action, GPU_RECOVERY_CLAMP_PARAMS);

    // Out of memory should suggest batch reduction
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 0);
    EXPECT_EQ(action, GPU_RECOVERY_REDUCE_BATCH);

    // On retry, may escalate to CPU fallback
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 2);
    // After multiple retries, should escalate
    EXPECT_NE(action, GPU_RECOVERY_NONE);
}

TEST_F(FinancialGPUIntegrationTest, RecoveryDuringMonteCarloSimulation) {
    // Run Monte Carlo with recovery context
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);
    nimcp_gpu_recovery_context_t* recovery_ctx = nimcp_gpu_recovery_context_create(&config);

    // Run successful simulation
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

    // Success rate should be tracked
    EXPECT_GE(stats.success_rate, 0.0f);
    EXPECT_LE(stats.success_rate, 1.0f);

    nimcp_gpu_recovery_context_destroy(recovery_ctx);
}

TEST_F(FinancialGPUIntegrationTest, MemoryInfoAndRecovery) {
    // Check memory info availability
    size_t free_bytes, total_bytes;
    bool mem_ok = nimcp_gpu_get_memory_info(&free_bytes, &total_bytes);

    if (mem_ok) {
        EXPECT_GT(total_bytes, 0u);
        EXPECT_LE(free_bytes, total_bytes);

        // Check if memory is critical
        bool critical = nimcp_gpu_memory_critical(0.99f);  // Very high threshold
        // Most likely not critical unless GPU is heavily loaded
        (void)critical;  // Just check it doesn't crash
    }
}

//=============================================================================
// Cross-Module Data Flow Tests
//=============================================================================

TEST_F(FinancialGPUIntegrationTest, DerivativesPricingToMonteCarlo) {
    // Step 1: Price European option using closed-form
    float bs_price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, 0.20f, 1.0f, FIN_OPT_CALL);
    EXPECT_GT(bs_price, 0.0f);

    // Step 2: Verify with Monte Carlo option pricing
    fin_mc_option_params_t mc_opt_params;
    mc_opt_params.base = fin_monte_carlo_gpu_params_default();
    mc_opt_params.base.initial_value = 100.0f;
    mc_opt_params.base.drift = 0.05f;
    mc_opt_params.base.volatility = 0.20f;
    mc_opt_params.base.time_horizon = 1.0f;
    mc_opt_params.base.num_paths = 100000;
    mc_opt_params.base.num_steps = 252;
    mc_opt_params.base.antithetic = true;
    mc_opt_params.option_type = FIN_OPT_CALL;
    mc_opt_params.option_style = FIN_MC_OPT_EUROPEAN;
    mc_opt_params.strike = 100.0f;

    fin_mc_option_result_t mc_result = {0};
    bool ok = fin_monte_carlo_gpu_option_price(ctx, rng, &mc_opt_params, &mc_result);
    ASSERT_TRUE(ok) << "Monte Carlo option pricing failed";

    // MC price should be close to Black-Scholes
    EXPECT_NEAR(mc_result.price, bs_price, bs_price * 0.03f)
        << "MC should converge to BS for European option";
    EXPECT_GT(mc_result.std_error, 0.0f);
    EXPECT_LT(mc_result.std_error, bs_price * 0.02f)
        << "Standard error should be small";
}

TEST_F(FinancialGPUIntegrationTest, OptimizationToRiskContribution) {
    const uint32_t N_ASSETS = 12;

    auto expected_returns = generateExpectedReturns(N_ASSETS, 0.10f);
    auto covariance = generateCovarianceMatrix(N_ASSETS, 0.22f);

    // Step 1: Optimize portfolio
    fin_optimization_gpu_params_t opt_params = fin_optimization_gpu_params_default();
    opt_params.strategy = FIN_OPT_STRATEGY_MIN_VARIANCE;
    opt_params.n_assets = N_ASSETS;
    opt_params.long_only = true;

    fin_optimization_gpu_result_t opt_result = {0};
    bool ok = fin_optimization_gpu_mean_variance(ctx, expected_returns.data(),
                                                   covariance.data(), &opt_params, &opt_result);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(opt_result.converged);

    // Step 2: Compute risk contributions
    std::vector<float> marginal_risk(N_ASSETS);
    std::vector<float> risk_contrib(N_ASSETS);
    ok = fin_opt_gpu_risk_contribution(ctx, opt_result.optimal_weights,
                                         covariance.data(), N_ASSETS,
                                         marginal_risk.data(), risk_contrib.data());
    ASSERT_TRUE(ok);

    // Verify risk contributions
    float total_contrib = 0.0f;
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        total_contrib += risk_contrib[i];
        EXPECT_GE(risk_contrib[i], 0.0f) << "Risk contrib should be non-negative for long-only";
    }

    // Total should equal portfolio variance
    float portfolio_var = opt_result.portfolio_volatility * opt_result.portfolio_volatility;
    EXPECT_NEAR(total_contrib, portfolio_var, portfolio_var * 0.05f);

    fin_optimization_gpu_result_free(&opt_result);
}

TEST_F(FinancialGPUIntegrationTest, VolatilityEstimationFlow) {
    // Generate price series
    const uint32_t N = 252;  // 1 year of daily prices
    std::vector<float> prices(N);
    prices[0] = 100.0f;

    srand(55555);
    for (uint32_t i = 1; i < N; i++) {
        float ret = 0.0002f + 0.015f * (2.0f * static_cast<float>(rand()) / RAND_MAX - 1.0f);
        prices[i] = prices[i-1] * (1.0f + ret);
    }

    // Step 1: Compute volatility using simple method
    float vol_simple = 0.0f;
    bool ok = fin_risk_gpu_volatility(ctx, prices.data(), N, FIN_VOL_SIMPLE, &vol_simple);
    ASSERT_TRUE(ok);
    EXPECT_GT(vol_simple, 0.0f);
    EXPECT_LT(vol_simple, 1.0f);  // Should be reasonable

    // Step 2: Compute volatility using EWMA
    float vol_ewma = 0.0f;
    ok = fin_risk_gpu_volatility(ctx, prices.data(), N, FIN_VOL_EWMA, &vol_ewma);
    ASSERT_TRUE(ok);
    EXPECT_GT(vol_ewma, 0.0f);

    // Both methods should give similar order of magnitude
    EXPECT_NEAR(vol_simple, vol_ewma, vol_simple * 0.5f);

    // Step 3: Use volatility in option pricing
    fin_derivatives_gpu_params_t deriv_params = fin_derivatives_gpu_params_default();
    deriv_params.spot = prices[N-1];
    deriv_params.strike = prices[N-1];
    deriv_params.volatility = vol_simple * std::sqrt(252.0f);  // Annualize
    deriv_params.time_to_expiry = 0.25f;

    fin_derivatives_gpu_result_t deriv_result = {0};
    ok = fin_derivatives_gpu_binomial_tree(ctx, &deriv_params, &deriv_result);
    ASSERT_TRUE(ok);
    EXPECT_GT(deriv_result.price, 0.0f);
}

// NOTE: BatchVaRComputation test removed
// fin_risk_gpu_var_batch not yet implemented

TEST_F(FinancialGPUIntegrationTest, CorrelationAndCovarianceMatrixFlow) {
    const uint32_t N_ASSETS = 6;
    const uint32_t N_OBS = 252;

    // Generate correlated returns
    std::vector<float> returns(N_ASSETS * N_OBS);
    srand(77777);

    for (uint32_t t = 0; t < N_OBS; t++) {
        float market = 0.01f * (2.0f * static_cast<float>(rand()) / RAND_MAX - 1.0f);
        for (uint32_t a = 0; a < N_ASSETS; a++) {
            float beta = 0.5f + 0.2f * a;
            float idio = 0.005f * (2.0f * static_cast<float>(rand()) / RAND_MAX - 1.0f);
            returns[a * N_OBS + t] = beta * market + idio;
        }
    }

    // Compute correlation matrix
    std::vector<float> correlation(N_ASSETS * N_ASSETS);
    bool ok = fin_risk_gpu_correlation_matrix(ctx, returns.data(), N_ASSETS, N_OBS,
                                                correlation.data());
    ASSERT_TRUE(ok);

    // Verify correlation properties
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        EXPECT_NEAR(correlation[i * N_ASSETS + i], 1.0f, 0.01f)
            << "Diagonal should be 1.0";
        for (uint32_t j = 0; j < N_ASSETS; j++) {
            EXPECT_GE(correlation[i * N_ASSETS + j], -1.0f);
            EXPECT_LE(correlation[i * N_ASSETS + j], 1.0f);
            EXPECT_NEAR(correlation[i * N_ASSETS + j], correlation[j * N_ASSETS + i], 1e-6f)
                << "Correlation should be symmetric";
        }
    }

    // Compute covariance matrix
    std::vector<float> covariance(N_ASSETS * N_ASSETS);
    ok = fin_risk_gpu_covariance_matrix(ctx, returns.data(), N_ASSETS, N_OBS,
                                          covariance.data());
    ASSERT_TRUE(ok);

    // Verify covariance properties
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        EXPECT_GT(covariance[i * N_ASSETS + i], 0.0f)
            << "Variance should be positive";
    }

    // Use covariance for optimization
    std::vector<float> exp_ret(N_ASSETS, 0.08f);
    fin_optimization_gpu_params_t opt_params = fin_optimization_gpu_params_default();
    opt_params.n_assets = N_ASSETS;

    fin_optimization_gpu_result_t opt_result = {0};
    ok = fin_optimization_gpu_mean_variance(ctx, exp_ret.data(), covariance.data(),
                                              &opt_params, &opt_result);
    ASSERT_TRUE(ok);

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
