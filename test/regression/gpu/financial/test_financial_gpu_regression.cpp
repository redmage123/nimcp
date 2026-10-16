//=============================================================================
// test_financial_gpu_regression.cpp - GPU Financial Module Regression Tests
//=============================================================================
/**
 * @file test_financial_gpu_regression.cpp
 * @brief Comprehensive regression tests for GPU-accelerated financial module
 *
 * WHAT: Regression tests ensuring financial GPU computations remain stable
 * WHY:  Financial applications require absolute precision and consistency
 * HOW:  Test numerical stability, edge cases, known values, performance baselines
 *
 * COVERAGE:
 *   - Numerical stability tests (extreme values, edge cases)
 *   - Memory stress tests (large portfolios, many paths)
 *   - Consistency tests (same input = same output)
 *   - Performance regression (timing baselines)
 *   - Error handling regression
 *   - Known bug regression tests
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <numeric>
#include <limits>
#include <chrono>
#include <random>

// Include financial GPU headers
// Note: These are C headers but have proper extern "C" guards internally
#include "gpu/financial/nimcp_financial_gpu.h"
#include "gpu/financial/nimcp_financial_monte_carlo_gpu.h"
#include "gpu/financial/nimcp_financial_optimization_gpu.h"
#include "gpu/financial/nimcp_financial_risk_gpu.h"
#include "gpu/financial/nimcp_financial_derivatives_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FinancialGPURegressionTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    fin_gpu_rng_t* rng = nullptr;

    void SetUp() override {
        ctx = nimcp_gpu_context_create(0);
        if (!ctx) {
            GTEST_SKIP() << "No GPU available - skipping GPU regression test";
        }

        rng = fin_gpu_rng_create(ctx, 500000, 42);
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

    // CPU Black-Scholes reference
    float normCDF(float x) {
        return 0.5f * (1.0f + std::erf(x / std::sqrt(2.0f)));
    }

    float blackScholesCall(float S, float K, float r, float sigma, float T) {
        if (T < 1e-10f || sigma < 1e-10f) {
            return std::max(0.0f, S - K * std::exp(-r * T));
        }
        float d1 = (std::log(S / K) + (r + 0.5f * sigma * sigma) * T) / (sigma * std::sqrt(T));
        float d2 = d1 - sigma * std::sqrt(T);
        return S * normCDF(d1) - K * std::exp(-r * T) * normCDF(d2);
    }

    float blackScholesPut(float S, float K, float r, float sigma, float T) {
        if (T < 1e-10f || sigma < 1e-10f) {
            return std::max(0.0f, K * std::exp(-r * T) - S);
        }
        float d1 = (std::log(S / K) + (r + 0.5f * sigma * sigma) * T) / (sigma * std::sqrt(T));
        float d2 = d1 - sigma * std::sqrt(T);
        return K * std::exp(-r * T) * normCDF(-d2) - S * normCDF(-d1);
    }

    // Generate Cholesky decomposition for correlation matrix
    std::vector<float> choleskyDecompose(const std::vector<float>& correlation, int n) {
        std::vector<float> L(n * n, 0.0f);
        for (int i = 0; i < n; i++) {
            for (int j = 0; j <= i; j++) {
                float sum = correlation[i * n + j];
                for (int k = 0; k < j; k++) {
                    sum -= L[i * n + k] * L[j * n + k];
                }
                if (i == j) {
                    L[i * n + j] = std::sqrt(std::max(0.0f, sum));
                } else if (L[j * n + j] > 1e-10f) {
                    L[i * n + j] = sum / L[j * n + j];
                }
            }
        }
        return L;
    }
};

//=============================================================================
// SECTION 1: Numerical Stability Tests (Extreme Values)
//=============================================================================

TEST_F(FinancialGPURegressionTest, NumericalStability_ExtremeSpotPrices) {
    // Test with extreme spot prices
    std::vector<float> extreme_spots = {
        0.001f, 0.01f, 0.1f, 1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f, 100000.0f
    };

    for (float spot : extreme_spots) {
        float strike = spot;  // ATM
        float price = fin_deriv_gpu_black_scholes(ctx, spot, strike, 0.05f, 0.2f, 1.0f, FIN_OPT_CALL);

        EXPECT_FALSE(std::isnan(price)) << "NaN for spot=" << spot;
        EXPECT_FALSE(std::isinf(price)) << "Inf for spot=" << spot;
        EXPECT_GE(price, 0.0f) << "Negative price for spot=" << spot;
        EXPECT_LE(price, spot) << "Price exceeds spot for spot=" << spot;
    }
}

TEST_F(FinancialGPURegressionTest, NumericalStability_ExtremeVolatility) {
    // Test with extreme volatility values
    std::vector<float> extreme_vols = {
        0.001f, 0.01f, 0.05f, 0.10f, 0.50f, 1.0f, 2.0f, 5.0f
    };

    for (float vol : extreme_vols) {
        float price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, vol, 1.0f, FIN_OPT_CALL);

        EXPECT_FALSE(std::isnan(price)) << "NaN for vol=" << vol;
        EXPECT_FALSE(std::isinf(price)) << "Inf for vol=" << vol;
        EXPECT_GE(price, 0.0f) << "Negative price for vol=" << vol;

        // Higher vol should generally give higher option price
        if (vol > 0.1f) {
            float low_vol_price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, 0.1f, 1.0f, FIN_OPT_CALL);
            EXPECT_GE(price, low_vol_price * 0.8f) << "High vol price should be >= low vol";
        }
    }
}

TEST_F(FinancialGPURegressionTest, NumericalStability_ExtremeTimeToExpiry) {
    // Test with extreme expiry times
    std::vector<float> extreme_times = {
        0.0001f,    // ~1 hour
        0.004f,     // ~1 day
        0.02f,      // ~1 week
        0.25f,      // 3 months
        1.0f,       // 1 year
        5.0f,       // 5 years
        10.0f,      // 10 years
        30.0f       // 30 years
    };

    for (float T : extreme_times) {
        float price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, 0.2f, T, FIN_OPT_CALL);

        EXPECT_FALSE(std::isnan(price)) << "NaN for T=" << T;
        EXPECT_FALSE(std::isinf(price)) << "Inf for T=" << T;
        EXPECT_GE(price, 0.0f) << "Negative price for T=" << T;
        EXPECT_LE(price, 100.0f) << "Price exceeds spot for T=" << T;
    }
}

TEST_F(FinancialGPURegressionTest, NumericalStability_ExtremeInterestRates) {
    // Test with extreme interest rates
    std::vector<float> extreme_rates = {
        -0.05f, -0.01f, 0.0f, 0.001f, 0.05f, 0.10f, 0.20f, 0.50f
    };

    for (float r : extreme_rates) {
        float price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, r, 0.2f, 1.0f, FIN_OPT_CALL);

        EXPECT_FALSE(std::isnan(price)) << "NaN for r=" << r;
        EXPECT_FALSE(std::isinf(price)) << "Inf for r=" << r;
        EXPECT_GE(price, 0.0f) << "Negative price for r=" << r;
    }
}

TEST_F(FinancialGPURegressionTest, NumericalStability_DeepITMAndOTM) {
    // Deep in-the-money and out-of-the-money options
    struct TestCase {
        float spot;
        float strike;
        const char* desc;
    };

    std::vector<TestCase> cases = {
        {200.0f, 100.0f, "Deep ITM call (2x)"},
        {500.0f, 100.0f, "Very deep ITM call (5x)"},
        {1000.0f, 100.0f, "Extreme ITM call (10x)"},
        {100.0f, 200.0f, "Deep OTM call (0.5x)"},
        {100.0f, 500.0f, "Very deep OTM call (0.2x)"},
        {100.0f, 1000.0f, "Extreme OTM call (0.1x)"},
    };

    for (const auto& tc : cases) {
        float call = fin_deriv_gpu_black_scholes(ctx, tc.spot, tc.strike, 0.05f, 0.2f, 1.0f, FIN_OPT_CALL);
        float put = fin_deriv_gpu_black_scholes(ctx, tc.spot, tc.strike, 0.05f, 0.2f, 1.0f, FIN_OPT_PUT);

        EXPECT_FALSE(std::isnan(call)) << tc.desc << " call is NaN";
        EXPECT_FALSE(std::isnan(put)) << tc.desc << " put is NaN";
        EXPECT_GE(call, 0.0f) << tc.desc << " call is negative";
        EXPECT_GE(put, 0.0f) << tc.desc << " put is negative";

        // Verify put-call parity
        float parity = call - put - (tc.spot - tc.strike * std::exp(-0.05f));
        EXPECT_NEAR(parity, 0.0f, 0.1f) << tc.desc << " violates put-call parity";
    }
}

TEST_F(FinancialGPURegressionTest, NumericalStability_MonteCarloExtremePaths) {
    // Test Monte Carlo with extreme parameters
    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.num_paths = 10000;

    // High volatility scenario
    params.initial_value = 100.0f;
    params.drift = 0.0f;
    params.volatility = 1.0f;  // 100% volatility
    params.time_horizon = 1.0f;

    fin_monte_carlo_gpu_result_t result = {0};
    bool ok = fin_monte_carlo_gpu_simulate(ctx, rng, &params, &result);
    ASSERT_TRUE(ok);

    EXPECT_FALSE(std::isnan(result.mean_return));
    EXPECT_FALSE(std::isnan(result.std_return));
    EXPECT_FALSE(std::isnan(result.var_95));
    EXPECT_GT(result.std_return, 0.0f) << "High vol should produce high std";

    fin_monte_carlo_gpu_result_free(ctx, &result);
}

TEST_F(FinancialGPURegressionTest, NumericalStability_RiskMetricsEdgeCases) {
    // Test risk metrics with edge case return distributions

    // All zeros
    std::vector<float> zeros(100, 0.0f);
    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = 100;

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, zeros.data(), &params, &result);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(result.var_95, 0.0f, 0.001f) << "VaR of zeros should be zero";

    // All same value
    std::vector<float> constant(100, -0.01f);
    ok = fin_risk_gpu_compute(ctx, constant.data(), &params, &result);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(result.var_95, 0.01f, 0.001f) << "VaR of constant should be that value";

    // Extreme outliers
    std::vector<float> outliers(100);
    for (int i = 0; i < 99; i++) outliers[i] = 0.01f;
    outliers[99] = -10.0f;  // Extreme outlier
    ok = fin_risk_gpu_compute(ctx, outliers.data(), &params, &result);
    ASSERT_TRUE(ok);
    EXPECT_FALSE(std::isnan(result.var_95));
    EXPECT_FALSE(std::isnan(result.cvar_95));
}

//=============================================================================
// SECTION 2: Memory Stress Tests (Large Portfolios, Many Paths)
//=============================================================================

TEST_F(FinancialGPURegressionTest, MemoryStress_LargeMonteCarlo) {
    // Test with large number of paths
    std::vector<uint32_t> path_counts = {100000, 250000, 500000};

    for (uint32_t num_paths : path_counts) {
        fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
        params.num_paths = num_paths;
        params.num_steps = 252;
        params.initial_value = 100.0f;
        params.drift = 0.05f;
        params.volatility = 0.2f;

        fin_monte_carlo_gpu_result_t result = {0};
        bool ok = fin_monte_carlo_gpu_simulate(ctx, rng, &params, &result);

        ASSERT_TRUE(ok) << "Failed with " << num_paths << " paths";
        EXPECT_GT(result.mean_value, 0.0f) << "Invalid mean with " << num_paths << " paths";
        EXPECT_EQ(result.paths_completed, num_paths) << "Not all paths completed";

        fin_monte_carlo_gpu_result_free(ctx, &result);
    }
}

TEST_F(FinancialGPURegressionTest, MemoryStress_LargePortfolio) {
    // Test optimization with large number of assets
    std::vector<uint32_t> asset_counts = {50, 100, 200};

    for (uint32_t n : asset_counts) {
        std::vector<float> returns(n);
        std::vector<float> covariance(n * n);

        // Generate returns and positive-definite covariance
        std::mt19937 gen(42);
        std::normal_distribution<float> ret_dist(0.08f, 0.04f);

        for (uint32_t i = 0; i < n; i++) {
            returns[i] = ret_dist(gen);
            for (uint32_t j = 0; j < n; j++) {
                if (i == j) {
                    covariance[i * n + j] = 0.04f + 0.01f * (i % 5);
                } else {
                    covariance[i * n + j] = 0.01f * std::exp(-std::abs((int)i - (int)j) / 10.0f);
                }
            }
        }

        fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
        params.n_assets = n;
        params.max_iterations = 500;
        params.convergence_tolerance = 1e-5f;

        fin_optimization_gpu_result_t result = {0};
        bool ok = fin_optimization_gpu_mean_variance(ctx, returns.data(), covariance.data(),
                                                       &params, &result);

        ASSERT_TRUE(ok) << "Optimization failed with " << n << " assets";

        // Verify weights
        float weight_sum = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            EXPECT_FALSE(std::isnan(result.optimal_weights[i])) << "NaN weight at " << i;
            weight_sum += result.optimal_weights[i];
        }
        EXPECT_NEAR(weight_sum, 1.0f, 0.05f) << "Weights should sum to 1 with " << n << " assets";

        fin_optimization_gpu_result_free(&result);
    }
}

TEST_F(FinancialGPURegressionTest, MemoryStress_LargeRiskComputation) {
    // Test risk computation with large return datasets
    std::vector<uint32_t> sizes = {10000, 50000, 100000};

    std::mt19937 gen(42);
    std::normal_distribution<float> dist(0.0f, 0.02f);

    for (uint32_t n : sizes) {
        std::vector<float> returns(n);
        for (uint32_t i = 0; i < n; i++) {
            returns[i] = dist(gen);
        }

        fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
        params.num_returns = n;

        fin_risk_gpu_result_t result = {0};
        bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);

        ASSERT_TRUE(ok) << "Risk computation failed with " << n << " returns";
        EXPECT_FALSE(std::isnan(result.var_95)) << "NaN VaR with " << n << " returns";
        EXPECT_FALSE(std::isnan(result.cvar_95)) << "NaN CVaR with " << n << " returns";
    }
}

// NOTE: MemoryStress_BatchOptionPricing test removed
// fin_derivatives_gpu_batch_price not yet implemented

//=============================================================================
// SECTION 3: Consistency Tests (Same Input = Same Output)
//=============================================================================

TEST_F(FinancialGPURegressionTest, Consistency_DeterministicBlackScholes) {
    // Black-Scholes should always produce identical results
    for (int trial = 0; trial < 10; trial++) {
        float price1 = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, 0.2f, 1.0f, FIN_OPT_CALL);
        float price2 = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, 0.2f, 1.0f, FIN_OPT_CALL);

        EXPECT_EQ(price1, price2) << "Black-Scholes not deterministic on trial " << trial;
    }
}

TEST_F(FinancialGPURegressionTest, Consistency_DeterministicBinomialTree) {
    // Binomial tree should be deterministic
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.tree_steps = 500;
    params.compute_greeks = true;

    fin_derivatives_gpu_result_t result1 = {0}, result2 = {0};

    fin_derivatives_gpu_binomial_tree(ctx, &params, &result1);
    fin_derivatives_gpu_binomial_tree(ctx, &params, &result2);

    EXPECT_EQ(result1.price, result2.price) << "Binomial tree not deterministic";
    EXPECT_EQ(result1.delta, result2.delta) << "Delta not deterministic";
    EXPECT_EQ(result1.gamma, result2.gamma) << "Gamma not deterministic";
}

TEST_F(FinancialGPURegressionTest, Consistency_DeterministicOptimization) {
    // Optimization should be deterministic (no randomness)
    const uint32_t N = 10;
    std::vector<float> returns = {0.08f, 0.10f, 0.12f, 0.09f, 0.11f,
                                   0.07f, 0.13f, 0.06f, 0.14f, 0.10f};
    std::vector<float> cov(N * N);
    for (uint32_t i = 0; i < N; i++) {
        for (uint32_t j = 0; j < N; j++) {
            cov[i * N + j] = (i == j) ? 0.04f : 0.01f;
        }
    }

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.n_assets = N;

    fin_optimization_gpu_result_t result1 = {0}, result2 = {0};

    fin_optimization_gpu_mean_variance(ctx, returns.data(), cov.data(), &params, &result1);
    fin_optimization_gpu_mean_variance(ctx, returns.data(), cov.data(), &params, &result2);

    for (uint32_t i = 0; i < N; i++) {
        EXPECT_NEAR(result1.optimal_weights[i], result2.optimal_weights[i], 1e-6f)
            << "Optimization not deterministic at weight " << i;
    }

    fin_optimization_gpu_result_free(&result1);
    fin_optimization_gpu_result_free(&result2);
}

TEST_F(FinancialGPURegressionTest, Consistency_SeededMonteCarlo) {
    // Same seed should give same results
    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.num_paths = 50000;
    params.seed = 12345;

    // First run
    fin_gpu_rng_reseed(rng, 12345);
    fin_monte_carlo_gpu_result_t result1 = {0};
    fin_monte_carlo_gpu_simulate(ctx, rng, &params, &result1);

    // Second run with same seed
    fin_gpu_rng_reseed(rng, 12345);
    fin_monte_carlo_gpu_result_t result2 = {0};
    fin_monte_carlo_gpu_simulate(ctx, rng, &params, &result2);

    EXPECT_NEAR(result1.mean_return, result2.mean_return, 1e-5f)
        << "Same seed should give same mean return";
    EXPECT_NEAR(result1.std_return, result2.std_return, 1e-5f)
        << "Same seed should give same std return";

    fin_monte_carlo_gpu_result_free(ctx, &result1);
    fin_monte_carlo_gpu_result_free(ctx, &result2);
}

//=============================================================================
// SECTION 4: Performance Regression (Timing Baselines)
//=============================================================================

TEST_F(FinancialGPURegressionTest, Performance_MonteCarloBaseline) {
    // Baseline: 100K paths should complete in reasonable time
    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.num_paths = 100000;
    params.num_steps = 252;

    auto start = std::chrono::high_resolution_clock::now();

    fin_monte_carlo_gpu_result_t result = {0};
    bool ok = fin_monte_carlo_gpu_simulate(ctx, rng, &params, &result);
    ASSERT_TRUE(ok);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in under 2 seconds on any reasonable GPU
    EXPECT_LT(elapsed.count(), 2000) << "100K path MC took too long: " << elapsed.count() << "ms";

    std::cout << "  Monte Carlo (100K paths): " << elapsed.count() << " ms" << std::endl;

    fin_monte_carlo_gpu_result_free(ctx, &result);
}

// NOTE: Performance_BatchPricingBaseline test removed
// fin_derivatives_gpu_batch_price not yet implemented

TEST_F(FinancialGPURegressionTest, Performance_OptimizationBaseline) {
    // Baseline: 100 asset optimization
    const uint32_t N = 100;
    std::vector<float> returns(N);
    std::vector<float> cov(N * N);

    std::mt19937 gen(42);
    std::normal_distribution<float> dist(0.08f, 0.03f);

    for (uint32_t i = 0; i < N; i++) {
        returns[i] = dist(gen);
        for (uint32_t j = 0; j < N; j++) {
            cov[i * N + j] = (i == j) ? 0.04f : 0.01f;
        }
    }

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.n_assets = N;
    params.max_iterations = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    fin_optimization_gpu_result_t result = {0};
    bool ok = fin_optimization_gpu_mean_variance(ctx, returns.data(), cov.data(), &params, &result);
    ASSERT_TRUE(ok);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in under 3 seconds
    EXPECT_LT(elapsed.count(), 3000) << "Optimization took too long: " << elapsed.count() << "ms";

    std::cout << "  Optimization (100 assets): " << elapsed.count() << " ms" << std::endl;

    fin_optimization_gpu_result_free(&result);
}

TEST_F(FinancialGPURegressionTest, Performance_RiskMetricsBaseline) {
    // Baseline: Risk computation on 100K returns
    const uint32_t N = 100000;
    std::vector<float> returns(N);

    std::mt19937 gen(42);
    std::normal_distribution<float> dist(0.0f, 0.02f);
    for (uint32_t i = 0; i < N; i++) {
        returns[i] = dist(gen);
    }

    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = N;

    auto start = std::chrono::high_resolution_clock::now();

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
    ASSERT_TRUE(ok);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in under 500ms
    EXPECT_LT(elapsed.count(), 500) << "Risk computation took too long: " << elapsed.count() << "ms";

    std::cout << "  Risk metrics (100K returns): " << elapsed.count() << " ms" << std::endl;
}

//=============================================================================
// SECTION 5: Error Handling Regression
//=============================================================================

TEST_F(FinancialGPURegressionTest, ErrorHandling_NullContext) {
    // Operations with null context should fail gracefully
    float price = fin_deriv_gpu_black_scholes(nullptr, 100.0f, 100.0f, 0.05f, 0.2f, 1.0f, FIN_OPT_CALL);
    EXPECT_TRUE(std::isnan(price) || price == 0.0f) << "Should handle null context";
}

TEST_F(FinancialGPURegressionTest, ErrorHandling_InvalidParameters) {
    // Test with invalid parameters
    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.num_paths = 0;  // Invalid

    fin_monte_carlo_gpu_result_t result = {0};
    bool ok = fin_monte_carlo_gpu_simulate(ctx, rng, &params, &result);
    // Should either fail or handle gracefully
    if (ok) {
        EXPECT_EQ(result.paths_completed, 0u);
    }
}

TEST_F(FinancialGPURegressionTest, ErrorHandling_NegativeVolatility) {
    // Negative volatility should be handled
    float price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, -0.2f, 1.0f, FIN_OPT_CALL);
    // Negative vol is undefined behavior - just verify it doesn't crash
    EXPECT_TRUE(std::isnan(price) || std::isfinite(price));
}

TEST_F(FinancialGPURegressionTest, ErrorHandling_NegativeExpiry) {
    // Negative time to expiry
    float price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, 0.2f, -1.0f, FIN_OPT_CALL);
    EXPECT_TRUE(std::isnan(price) || price >= 0.0f);
}

TEST_F(FinancialGPURegressionTest, ErrorHandling_ZeroStrike) {
    // Zero strike price
    float price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 0.0f, 0.05f, 0.2f, 1.0f, FIN_OPT_CALL);
    // Call with zero strike is worth the spot
    EXPECT_TRUE(std::isnan(price) || price >= 99.0f);
}

//=============================================================================
// SECTION 6: Known Bug Regression Tests
//=============================================================================

TEST_F(FinancialGPURegressionTest, KnownBug_ZeroVolatilityDivision) {
    // Regression: Zero volatility used to cause division by zero
    float price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 95.0f, 0.05f, 0.0f, 1.0f, FIN_OPT_CALL);

    EXPECT_FALSE(std::isnan(price)) << "Zero vol should not cause NaN";
    EXPECT_FALSE(std::isinf(price)) << "Zero vol should not cause Inf";
    // ITM call with zero vol is worth max(S - K*exp(-rT), 0)
    float expected = std::max(0.0f, 100.0f - 95.0f * std::exp(-0.05f));
    EXPECT_NEAR(price, expected, 1.0f);
}

TEST_F(FinancialGPURegressionTest, KnownBug_VeryShortExpiry) {
    // Regression: Very short expiry used to underflow
    float price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, 0.2f, 1e-6f, FIN_OPT_CALL);

    EXPECT_FALSE(std::isnan(price)) << "Very short expiry should not cause NaN";
    EXPECT_FALSE(std::isinf(price)) << "Very short expiry should not cause Inf";
    EXPECT_GE(price, 0.0f);
    EXPECT_LT(price, 1.0f) << "ATM with near-zero expiry should have tiny value";
}

TEST_F(FinancialGPURegressionTest, KnownBug_HighCorrelationOptimization) {
    // Regression: High correlation matrix used to cause singular matrix issues
    const uint32_t N = 5;
    std::vector<float> returns(N, 0.10f);
    std::vector<float> cov(N * N);

    // Very high correlation (near singular)
    for (uint32_t i = 0; i < N; i++) {
        for (uint32_t j = 0; j < N; j++) {
            if (i == j) {
                cov[i * N + j] = 0.04f;
            } else {
                cov[i * N + j] = 0.0399f;  // Correlation ≈ 0.9975
            }
        }
    }

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.n_assets = N;
    params.regularization = 1e-4f;  // Use regularization

    fin_optimization_gpu_result_t result = {0};
    bool ok = fin_optimization_gpu_mean_variance(ctx, returns.data(), cov.data(), &params, &result);

    // Should complete (may warn but not crash)
    if (ok) {
        for (uint32_t i = 0; i < N; i++) {
            EXPECT_FALSE(std::isnan(result.optimal_weights[i]));
            EXPECT_FALSE(std::isinf(result.optimal_weights[i]));
        }
        fin_optimization_gpu_result_free(&result);
    }
}

TEST_F(FinancialGPURegressionTest, KnownBug_SingleAssetPortfolio) {
    // Regression: Single asset portfolio used to fail
    const uint32_t N = 1;
    std::vector<float> returns = {0.10f};
    std::vector<float> cov = {0.04f};

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.n_assets = N;

    fin_optimization_gpu_result_t result = {0};
    bool ok = fin_optimization_gpu_mean_variance(ctx, returns.data(), cov.data(), &params, &result);

    if (ok) {
        EXPECT_NEAR(result.optimal_weights[0], 1.0f, 0.01f)
            << "Single asset should have 100% weight";
        fin_optimization_gpu_result_free(&result);
    }
}

TEST_F(FinancialGPURegressionTest, KnownBug_VaRWithIdenticalReturns) {
    // Regression: Identical returns used to cause issues in sorting
    std::vector<float> returns(100, 0.01f);

    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = 100;

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
    ASSERT_TRUE(ok);

    EXPECT_NEAR(result.var_95, -0.01f, 0.001f) << "VaR of constant should be that constant";
}

TEST_F(FinancialGPURegressionTest, KnownBug_AmericanCallNoDividend) {
    // Regression: American call with no dividend should equal European
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.option_style = FIN_OPT_STYLE_AMERICAN;
    params.dividend_yield = 0.0f;
    params.tree_steps = 1000;

    fin_derivatives_gpu_result_t am_result = {0};
    fin_derivatives_gpu_binomial_tree(ctx, &params, &am_result);

    float bs_price = blackScholesCall(params.spot, params.strike, params.risk_free_rate,
                                       params.volatility, params.time_to_expiry);

    // American call without dividends should be very close to European
    EXPECT_NEAR(am_result.price, bs_price, 0.2f)
        << "American call (no div) should equal European";
    EXPECT_NEAR(am_result.early_exercise_premium, 0.0f, 0.1f)
        << "No early exercise premium without dividends";
}

TEST_F(FinancialGPURegressionTest, KnownBug_RiskParityEqualWeights) {
    // Regression: Risk parity with identical assets should give equal weights
    const uint32_t N = 4;
    std::vector<float> cov(N * N);

    // Identical variances, no correlation
    for (uint32_t i = 0; i < N; i++) {
        for (uint32_t j = 0; j < N; j++) {
            cov[i * N + j] = (i == j) ? 0.04f : 0.0f;
        }
    }

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.n_assets = N;
    params.strategy = FIN_OPT_STRATEGY_RISK_PARITY;

    fin_optimization_gpu_result_t result = {0};
    bool ok = fin_opt_gpu_risk_parity(ctx, cov.data(), N, nullptr, &params, &result);

    if (ok) {
        // All weights should be equal (1/N)
        float expected_weight = 1.0f / N;
        for (uint32_t i = 0; i < N; i++) {
            EXPECT_NEAR(result.optimal_weights[i], expected_weight, 0.05f)
                << "Risk parity with identical assets should give equal weights";
        }
        fin_optimization_gpu_result_free(&result);
    }
}

//=============================================================================
// SECTION 7: Mathematical Property Tests
//=============================================================================

TEST_F(FinancialGPURegressionTest, MathProperty_PutCallParity) {
    // C - P = S - K*exp(-rT)
    std::vector<std::tuple<float, float, float, float, float>> test_cases = {
        {100.0f, 100.0f, 0.05f, 0.20f, 1.0f},
        {100.0f, 90.0f, 0.03f, 0.30f, 0.5f},
        {100.0f, 110.0f, 0.08f, 0.15f, 2.0f},
        {50.0f, 55.0f, 0.02f, 0.25f, 0.25f},
        {200.0f, 180.0f, 0.04f, 0.18f, 3.0f},
    };

    for (const auto& [S, K, r, sigma, T] : test_cases) {
        float call = fin_deriv_gpu_black_scholes(ctx, S, K, r, sigma, T, FIN_OPT_CALL);
        float put = fin_deriv_gpu_black_scholes(ctx, S, K, r, sigma, T, FIN_OPT_PUT);

        float lhs = call - put;
        float rhs = S - K * std::exp(-r * T);

        EXPECT_NEAR(lhs, rhs, 0.01f)
            << "Put-call parity violated for S=" << S << ", K=" << K;
    }
}

TEST_F(FinancialGPURegressionTest, MathProperty_MonotonicityInStrike) {
    // Call price decreases with strike, Put price increases with strike
    float prev_call = 1000.0f;
    float prev_put = 0.0f;

    for (float K = 80.0f; K <= 120.0f; K += 2.0f) {
        float call = fin_deriv_gpu_black_scholes(ctx, 100.0f, K, 0.05f, 0.2f, 1.0f, FIN_OPT_CALL);
        float put = fin_deriv_gpu_black_scholes(ctx, 100.0f, K, 0.05f, 0.2f, 1.0f, FIN_OPT_PUT);

        EXPECT_LE(call, prev_call + 0.01f) << "Call not decreasing at K=" << K;
        EXPECT_GE(put, prev_put - 0.01f) << "Put not increasing at K=" << K;

        prev_call = call;
        prev_put = put;
    }
}

TEST_F(FinancialGPURegressionTest, MathProperty_MonotonicityInExpiry) {
    // Option value increases with time to expiry
    float prev_price = 0.0f;

    for (float T = 0.1f; T <= 2.0f; T += 0.1f) {
        float price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, 0.2f, T, FIN_OPT_CALL);

        EXPECT_GE(price, prev_price - 0.01f) << "Value not increasing at T=" << T;
        prev_price = price;
    }
}

TEST_F(FinancialGPURegressionTest, MathProperty_CVaRGreaterThanVaR) {
    // CVaR >= VaR always
    std::mt19937 gen(42);
    std::normal_distribution<float> dist(0.0f, 0.02f);

    for (int trial = 0; trial < 10; trial++) {
        std::vector<float> returns(1000);
        for (int i = 0; i < 1000; i++) {
            returns[i] = dist(gen);
        }

        fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
        params.num_returns = 1000;

        fin_risk_gpu_result_t result = {0};
        bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
        ASSERT_TRUE(ok);

        EXPECT_GE(result.cvar_95, result.var_95 - 0.001f)
            << "CVaR(95%) < VaR(95%) on trial " << trial;
        EXPECT_GE(result.cvar_99, result.var_99 - 0.001f)
            << "CVaR(99%) < VaR(99%) on trial " << trial;
    }
}

TEST_F(FinancialGPURegressionTest, MathProperty_GreeksSignConventions) {
    // Verify Greek sign conventions
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.compute_greeks = true;
    params.tree_steps = 500;

    // Call options
    params.option_type = FIN_OPT_CALL;
    fin_derivatives_gpu_result_t call_result = {0};
    fin_derivatives_gpu_binomial_tree(ctx, &params, &call_result);

    EXPECT_GT(call_result.delta, 0.0f) << "Call delta should be positive";
    EXPECT_GT(call_result.gamma, 0.0f) << "Call gamma should be positive";
    EXPECT_LT(call_result.theta, 0.0f) << "Call theta should be negative (time decay)";
    EXPECT_GT(call_result.vega, 0.0f) << "Call vega should be positive";
    EXPECT_GT(call_result.rho, 0.0f) << "Call rho should be positive";

    // Put options
    params.option_type = FIN_OPT_PUT;
    fin_derivatives_gpu_result_t put_result = {0};
    fin_derivatives_gpu_binomial_tree(ctx, &params, &put_result);

    EXPECT_LT(put_result.delta, 0.0f) << "Put delta should be negative";
    EXPECT_GT(put_result.gamma, 0.0f) << "Put gamma should be positive";
    EXPECT_LT(put_result.theta, 0.0f) << "Put theta should be negative";
    EXPECT_GT(put_result.vega, 0.0f) << "Put vega should be positive";
    EXPECT_LT(put_result.rho, 0.0f) << "Put rho should be negative";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
