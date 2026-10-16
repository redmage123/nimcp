/* ============================================================================
 * Unit Tests: GPU Financial Portfolio Optimization
 * ============================================================================
 * WHAT: Unit tests for GPU-accelerated portfolio optimization
 * WHY:  Validate mean-variance, min-variance, max-Sharpe on GPU
 * HOW:  Compare GPU results against known analytical solutions
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>
#include <random>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/financial/nimcp_financial_gpu.h"
#include "gpu/financial/nimcp_financial_optimization_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-4f;
constexpr float RELAXED_TOLERANCE = 1e-3f;
constexpr float OPTIMIZATION_TOLERANCE = 1e-2f;  // Optimization may not be exact

class FinancialOptimizationGPUTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        ctx_ = nimcp_gpu_context_create(0);
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_context_destroy(ctx_);
            ctx_ = NULL;
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx_ = NULL;

    // Helper: Compute portfolio return
    float portfolio_return(const std::vector<float>& weights,
                          const std::vector<float>& returns) {
        float ret = 0.0f;
        for (size_t i = 0; i < weights.size(); i++) {
            ret += weights[i] * returns[i];
        }
        return ret;
    }

    // Helper: Compute portfolio variance
    float portfolio_variance(const std::vector<float>& weights,
                            const std::vector<float>& covariance,
                            size_t n) {
        float var = 0.0f;
        for (size_t i = 0; i < n; i++) {
            for (size_t j = 0; j < n; j++) {
                var += weights[i] * weights[j] * covariance[i * n + j];
            }
        }
        return var;
    }

    // Helper: Compute Sharpe ratio
    float sharpe_ratio(float port_ret, float port_var, float rf) {
        float vol = std::sqrt(port_var);
        if (vol < 1e-10f) return 0.0f;
        return (port_ret - rf) / vol;
    }
#endif
};

/* ============================================================================
 * Test: Mean-Variance Optimization - Simple Case
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, MeanVarianceSimple) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 2;

    // Simple 2-asset case
    std::vector<float> expected_returns = {0.10f, 0.05f};  // 10% and 5%
    std::vector<float> covariance = {
        0.04f, 0.01f,   // Asset 1 variance = 0.04 (20% vol)
        0.01f, 0.01f    // Asset 2 variance = 0.01 (10% vol)
    };

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.strategy = FIN_OPT_STRATEGY_MEAN_VARIANCE;
    params.target_return = 0.08f;  // Target 8% return
    params.n_assets = n_assets;
    params.long_only = true;

    fin_optimization_gpu_result_t result = {};
    result.optimal_weights = new float[n_assets];

    ASSERT_TRUE(fin_optimization_gpu_mean_variance(
        ctx_, expected_returns.data(), covariance.data(),
        &params, &result));

    // Verify weights sum to 1
    float weight_sum = 0.0f;
    for (uint32_t i = 0; i < n_assets; i++) {
        EXPECT_GE(result.optimal_weights[i], -TOLERANCE) << "Weight " << i << " is negative";
        weight_sum += result.optimal_weights[i];
    }
    EXPECT_NEAR(weight_sum, 1.0f, RELAXED_TOLERANCE);

    // Verify portfolio achieves target return
    float port_ret = portfolio_return(
        std::vector<float>(result.optimal_weights, result.optimal_weights + n_assets),
        expected_returns);
    EXPECT_NEAR(port_ret, params.target_return, OPTIMIZATION_TOLERANCE);

    delete[] result.optimal_weights;
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Minimum Variance Portfolio
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, MinimumVariance) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 3;

    std::vector<float> expected_returns = {0.12f, 0.08f, 0.05f};
    std::vector<float> covariance = {
        0.09f, 0.02f, 0.01f,
        0.02f, 0.04f, 0.01f,
        0.01f, 0.01f, 0.01f
    };

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.strategy = FIN_OPT_STRATEGY_MIN_VARIANCE;
    params.n_assets = n_assets;
    params.long_only = true;

    fin_optimization_gpu_result_t result = {};
    result.optimal_weights = new float[n_assets];

    ASSERT_TRUE(fin_optimization_gpu_mean_variance(
        ctx_, expected_returns.data(), covariance.data(),
        &params, &result));

    // Min-variance should weight lowest-volatility asset heavily
    // Asset 2 has lowest variance (0.01)
    EXPECT_GT(result.optimal_weights[2], result.optimal_weights[0])
        << "Min-variance should favor low-vol assets";

    // Verify convergence
    EXPECT_TRUE(result.converged);

    delete[] result.optimal_weights;
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Maximum Sharpe Ratio Portfolio
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, MaximumSharpe) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 3;

    std::vector<float> expected_returns = {0.15f, 0.10f, 0.05f};
    std::vector<float> covariance = {
        0.09f, 0.03f, 0.01f,
        0.03f, 0.04f, 0.02f,
        0.01f, 0.02f, 0.01f
    };

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.strategy = FIN_OPT_STRATEGY_MAX_SHARPE;
    params.risk_free_rate = 0.02f;
    params.n_assets = n_assets;
    params.long_only = true;
    params.max_iterations = 2000;

    fin_optimization_gpu_result_t result = {};
    result.optimal_weights = new float[n_assets];

    ASSERT_TRUE(fin_optimization_gpu_mean_variance(
        ctx_, expected_returns.data(), covariance.data(),
        &params, &result));

    // Verify Sharpe ratio is positive
    EXPECT_GT(result.sharpe_ratio, 0.0f);

    // Verify reported Sharpe matches computed
    float computed_sharpe = sharpe_ratio(
        result.expected_return, result.portfolio_variance, params.risk_free_rate);
    EXPECT_NEAR(result.sharpe_ratio, computed_sharpe, RELAXED_TOLERANCE);

    delete[] result.optimal_weights;
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Long-Only Constraint
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, LongOnlyConstraint) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 4;

    std::vector<float> expected_returns = {0.08f, 0.12f, 0.05f, 0.10f};
    std::vector<float> covariance(n_assets * n_assets, 0.0f);

    // Diagonal covariance (uncorrelated)
    for (uint32_t i = 0; i < n_assets; i++) {
        covariance[i * n_assets + i] = 0.04f;  // 20% vol
    }

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.strategy = FIN_OPT_STRATEGY_MAX_SHARPE;
    params.risk_free_rate = 0.02f;
    params.n_assets = n_assets;
    params.long_only = true;

    fin_optimization_gpu_result_t result = {};
    result.optimal_weights = new float[n_assets];

    ASSERT_TRUE(fin_optimization_gpu_mean_variance(
        ctx_, expected_returns.data(), covariance.data(),
        &params, &result));

    // All weights should be non-negative
    for (uint32_t i = 0; i < n_assets; i++) {
        EXPECT_GE(result.optimal_weights[i], -TOLERANCE)
            << "Long-only violated at asset " << i;
    }

    delete[] result.optimal_weights;
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Weight Bounds
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, WeightBounds) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 3;

    std::vector<float> expected_returns = {0.08f, 0.12f, 0.06f};
    std::vector<float> covariance = {
        0.04f, 0.01f, 0.005f,
        0.01f, 0.09f, 0.02f,
        0.005f, 0.02f, 0.01f
    };

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.strategy = FIN_OPT_STRATEGY_MAX_SHARPE;
    params.n_assets = n_assets;
    params.min_weight = 0.1f;   // At least 10% per asset
    params.max_weight = 0.5f;   // At most 50% per asset

    fin_optimization_gpu_result_t result = {};
    result.optimal_weights = new float[n_assets];

    ASSERT_TRUE(fin_optimization_gpu_mean_variance(
        ctx_, expected_returns.data(), covariance.data(),
        &params, &result));

    // Verify bounds
    for (uint32_t i = 0; i < n_assets; i++) {
        EXPECT_GE(result.optimal_weights[i], params.min_weight - TOLERANCE)
            << "Min weight violated at asset " << i;
        EXPECT_LE(result.optimal_weights[i], params.max_weight + TOLERANCE)
            << "Max weight violated at asset " << i;
    }

    delete[] result.optimal_weights;
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Efficient Frontier Computation
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, EfficientFrontier) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 4;
    const uint32_t num_points = 20;

    std::vector<float> expected_returns = {0.12f, 0.08f, 0.05f, 0.10f};
    std::vector<float> covariance = {
        0.09f, 0.02f, 0.01f, 0.03f,
        0.02f, 0.04f, 0.01f, 0.02f,
        0.01f, 0.01f, 0.01f, 0.005f,
        0.03f, 0.02f, 0.005f, 0.06f
    };

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.n_assets = n_assets;
    params.long_only = true;

    fin_efficient_frontier_result_t frontier = {};
    frontier.returns = new float[num_points];
    frontier.volatilities = new float[num_points];
    frontier.sharpe_ratios = new float[num_points];
    frontier.weights = new float[num_points * n_assets];

    ASSERT_TRUE(fin_optimization_gpu_efficient_frontier(
        ctx_, expected_returns.data(), covariance.data(),
        &params, num_points, &frontier));

    EXPECT_EQ(frontier.num_points, num_points);

    // Frontier should be monotonically increasing in return-risk space
    for (uint32_t i = 1; i < num_points; i++) {
        EXPECT_GE(frontier.returns[i], frontier.returns[i-1] - TOLERANCE)
            << "Frontier not monotonic in returns at point " << i;
        // Higher return generally means higher volatility on frontier
    }

    // Verify all frontier portfolios have valid weights
    for (uint32_t p = 0; p < num_points; p++) {
        float sum = 0.0f;
        for (uint32_t a = 0; a < n_assets; a++) {
            sum += frontier.weights[p * n_assets + a];
        }
        EXPECT_NEAR(sum, 1.0f, RELAXED_TOLERANCE)
            << "Weights don't sum to 1 at frontier point " << p;
    }

    delete[] frontier.returns;
    delete[] frontier.volatilities;
    delete[] frontier.sharpe_ratios;
    delete[] frontier.weights;
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Large Portfolio (Many Assets)
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, LargePortfolio) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 50;

    // Generate random returns and positive definite covariance
    std::vector<float> expected_returns(n_assets);
    std::vector<float> covariance(n_assets * n_assets);

    std::mt19937 gen(42);
    std::normal_distribution<float> ret_dist(0.08f, 0.03f);
    std::uniform_real_distribution<float> cov_dist(0.0f, 0.02f);

    for (uint32_t i = 0; i < n_assets; i++) {
        expected_returns[i] = std::max(0.01f, ret_dist(gen));
        for (uint32_t j = 0; j < n_assets; j++) {
            if (i == j) {
                covariance[i * n_assets + j] = 0.04f + cov_dist(gen);  // Diagonal
            } else {
                float cov_val = cov_dist(gen);
                covariance[i * n_assets + j] = cov_val;
                covariance[j * n_assets + i] = cov_val;  // Symmetric
            }
        }
    }

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.strategy = FIN_OPT_STRATEGY_MAX_SHARPE;
    params.n_assets = n_assets;
    params.long_only = true;
    params.max_iterations = 5000;

    fin_optimization_gpu_result_t result = {};
    result.optimal_weights = new float[n_assets];

    ASSERT_TRUE(fin_optimization_gpu_mean_variance(
        ctx_, expected_returns.data(), covariance.data(),
        &params, &result));

    // Verify basic properties
    float weight_sum = 0.0f;
    for (uint32_t i = 0; i < n_assets; i++) {
        weight_sum += result.optimal_weights[i];
    }
    EXPECT_NEAR(weight_sum, 1.0f, RELAXED_TOLERANCE);

    // Should complete in reasonable time (no specific assertion, just not hang)
    EXPECT_LT(result.kernel_time_ms, 5000.0f);

    delete[] result.optimal_weights;
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Convergence Detection
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, ConvergenceDetection) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 3;

    std::vector<float> expected_returns = {0.10f, 0.08f, 0.06f};
    std::vector<float> covariance = {
        0.04f, 0.01f, 0.005f,
        0.01f, 0.04f, 0.01f,
        0.005f, 0.01f, 0.04f
    };

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.strategy = FIN_OPT_STRATEGY_MAX_SHARPE;
    params.n_assets = n_assets;
    params.convergence_tolerance = 1e-8f;  // Tight tolerance
    params.max_iterations = 10000;

    fin_optimization_gpu_result_t result = {};
    result.optimal_weights = new float[n_assets];

    ASSERT_TRUE(fin_optimization_gpu_mean_variance(
        ctx_, expected_returns.data(), covariance.data(),
        &params, &result));

    EXPECT_TRUE(result.converged) << "Should converge with tight tolerance";
    EXPECT_LT(result.iterations, params.max_iterations)
        << "Should converge before max iterations";

    delete[] result.optimal_weights;
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Equal Returns Case
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, EqualReturns) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 3;

    // All assets have same expected return
    std::vector<float> expected_returns = {0.08f, 0.08f, 0.08f};
    std::vector<float> covariance = {
        0.04f, 0.01f, 0.005f,
        0.01f, 0.02f, 0.01f,
        0.005f, 0.01f, 0.01f
    };

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.strategy = FIN_OPT_STRATEGY_MIN_VARIANCE;
    params.n_assets = n_assets;
    params.long_only = true;

    fin_optimization_gpu_result_t result = {};
    result.optimal_weights = new float[n_assets];

    ASSERT_TRUE(fin_optimization_gpu_mean_variance(
        ctx_, expected_returns.data(), covariance.data(),
        &params, &result));

    // All returns equal, so min-variance should dominate
    // Asset 2 has lowest variance
    EXPECT_GT(result.optimal_weights[2], 0.3f)
        << "Min-variance should favor low-vol asset";

    delete[] result.optimal_weights;
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Risk Aversion Parameter
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, RiskAversionParameter) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 3;

    std::vector<float> expected_returns = {0.15f, 0.08f, 0.04f};
    std::vector<float> covariance = {
        0.16f, 0.04f, 0.02f,
        0.04f, 0.04f, 0.01f,
        0.02f, 0.01f, 0.01f
    };

    fin_optimization_gpu_result_t result_low = {};
    fin_optimization_gpu_result_t result_high = {};
    result_low.optimal_weights = new float[n_assets];
    result_high.optimal_weights = new float[n_assets];

    // Low risk aversion
    fin_optimization_gpu_params_t params_low = fin_optimization_gpu_params_default();
    params_low.strategy = FIN_OPT_STRATEGY_MEAN_VARIANCE;
    params_low.n_assets = n_assets;
    params_low.risk_aversion = 0.5f;  // Low risk aversion
    params_low.long_only = true;

    ASSERT_TRUE(fin_optimization_gpu_mean_variance(
        ctx_, expected_returns.data(), covariance.data(),
        &params_low, &result_low));

    // High risk aversion
    fin_optimization_gpu_params_t params_high = fin_optimization_gpu_params_default();
    params_high.strategy = FIN_OPT_STRATEGY_MEAN_VARIANCE;
    params_high.n_assets = n_assets;
    params_high.risk_aversion = 5.0f;  // High risk aversion
    params_high.long_only = true;

    ASSERT_TRUE(fin_optimization_gpu_mean_variance(
        ctx_, expected_returns.data(), covariance.data(),
        &params_high, &result_high));

    // High risk aversion should result in lower volatility portfolio
    EXPECT_LT(result_high.portfolio_volatility, result_low.portfolio_volatility + TOLERANCE)
        << "Higher risk aversion should yield lower volatility";

    delete[] result_low.optimal_weights;
    delete[] result_high.optimal_weights;
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Portfolio Variance Computation
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, PortfolioVariance) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 3;

    std::vector<float> weights = {0.4f, 0.35f, 0.25f};
    std::vector<float> covariance = {
        0.04f, 0.01f, 0.005f,
        0.01f, 0.09f, 0.02f,
        0.005f, 0.02f, 0.01f
    };

    float gpu_variance = fin_opt_gpu_portfolio_variance(ctx_, weights.data(),
                                                         covariance.data(), n_assets);

    // CPU reference calculation
    float cpu_variance = portfolio_variance(weights, covariance, n_assets);

    EXPECT_NEAR(gpu_variance, cpu_variance, TOLERANCE)
        << "Portfolio variance mismatch";
    EXPECT_GT(gpu_variance, 0.0f) << "Variance should be positive";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Portfolio Return Computation
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, PortfolioReturn) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 4;

    std::vector<float> weights = {0.25f, 0.25f, 0.30f, 0.20f};
    std::vector<float> expected_returns = {0.08f, 0.12f, 0.05f, 0.10f};

    float gpu_return = fin_opt_gpu_portfolio_return(ctx_, weights.data(),
                                                     expected_returns.data(), n_assets);

    // CPU reference calculation
    float cpu_return = portfolio_return(weights, expected_returns);

    EXPECT_NEAR(gpu_return, cpu_return, TOLERANCE)
        << "Portfolio return mismatch";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Portfolio Variance - Zero Weights
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, PortfolioVarianceZeroWeights) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 3;

    std::vector<float> weights = {0.0f, 0.0f, 0.0f};
    std::vector<float> covariance = {
        0.04f, 0.01f, 0.005f,
        0.01f, 0.09f, 0.02f,
        0.005f, 0.02f, 0.01f
    };

    float variance = fin_opt_gpu_portfolio_variance(ctx_, weights.data(),
                                                     covariance.data(), n_assets);

    EXPECT_NEAR(variance, 0.0f, TOLERANCE) << "Zero weights should give zero variance";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Portfolio Return - Equal Weights
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, PortfolioReturnEqualWeights) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 4;

    std::vector<float> weights = {0.25f, 0.25f, 0.25f, 0.25f};
    std::vector<float> expected_returns = {0.08f, 0.08f, 0.08f, 0.08f};

    float port_return = fin_opt_gpu_portfolio_return(ctx_, weights.data(),
                                                      expected_returns.data(), n_assets);

    // Equal weights and equal returns should give same return
    EXPECT_NEAR(port_return, 0.08f, TOLERANCE)
        << "Equal weighted portfolio return mismatch";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Black-Litterman Model
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, BlackLittermanBasic) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 3;

    // Market covariance matrix
    std::vector<float> market_covariance = {
        0.04f, 0.01f, 0.005f,
        0.01f, 0.09f, 0.02f,
        0.005f, 0.02f, 0.01f
    };

    // Market cap weights for equilibrium
    std::vector<float> market_weights = {0.5f, 0.3f, 0.2f};

    // Investor view: Asset 0 will outperform Asset 2 by 2%
    std::vector<float> views_matrix = {1.0f, 0.0f, -1.0f};  // P matrix: 1 view x 3 assets
    std::vector<float> views_returns = {0.02f};  // Q vector: expected excess return
    std::vector<float> views_confidence = {0.01f};  // Omega diagonal: confidence

    fin_opt_extended_params_t params = fin_opt_extended_params_default();
    params.base.strategy = FIN_OPT_STRATEGY_MAX_SHARPE;
    params.base.n_assets = n_assets;
    params.base.long_only = true;
    params.views_matrix = views_matrix.data();
    params.views_returns = views_returns.data();
    params.views_confidence = views_confidence.data();
    params.num_views = 1;
    params.tau = 0.05f;
    params.market_weights = market_weights.data();

    // Don't pre-allocate - let the function handle allocation
    fin_opt_extended_result_t result = {};

    bool ok = fin_opt_gpu_black_litterman(ctx_, market_covariance.data(),
                                           n_assets, &params, &result);
    ASSERT_TRUE(ok);

    // Verify weights sum to 1
    float weight_sum = 0.0f;
    for (uint32_t i = 0; i < n_assets; i++) {
        weight_sum += result.base.optimal_weights[i];
    }
    EXPECT_NEAR(weight_sum, 1.0f, RELAXED_TOLERANCE);

    // Posterior returns should be computed
    for (uint32_t i = 0; i < n_assets; i++) {
        EXPECT_FALSE(std::isnan(result.posterior_returns[i]))
            << "Posterior return should not be NaN at " << i;
    }

    // Given our view that Asset 0 outperforms Asset 2,
    // Asset 0 should have higher weight or return
    EXPECT_GT(result.posterior_returns[0], result.posterior_returns[2] - 0.01f)
        << "View should be reflected in posterior returns";

    // Use the proper free function (handles malloc'd memory)
    fin_opt_extended_result_free(&result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Black-Litterman - Multiple Views
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, BlackLittermanMultipleViews) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 4;
    const uint32_t num_views = 2;

    std::vector<float> market_covariance = {
        0.04f, 0.01f, 0.005f, 0.008f,
        0.01f, 0.09f, 0.02f, 0.015f,
        0.005f, 0.02f, 0.01f, 0.006f,
        0.008f, 0.015f, 0.006f, 0.0225f
    };

    std::vector<float> market_weights = {0.4f, 0.25f, 0.2f, 0.15f};

    // View 1: Asset 0 will return 8%
    // View 2: Asset 1 will outperform Asset 3 by 3%
    std::vector<float> views_matrix = {
        1.0f, 0.0f, 0.0f, 0.0f,   // View 1: absolute view on Asset 0
        0.0f, 1.0f, 0.0f, -1.0f   // View 2: relative view Asset 1 vs Asset 3
    };
    std::vector<float> views_returns = {0.08f, 0.03f};
    std::vector<float> views_confidence = {0.01f, 0.02f};

    fin_opt_extended_params_t params = fin_opt_extended_params_default();
    params.base.strategy = FIN_OPT_STRATEGY_MAX_SHARPE;
    params.base.n_assets = n_assets;
    params.base.long_only = true;
    params.views_matrix = views_matrix.data();
    params.views_returns = views_returns.data();
    params.views_confidence = views_confidence.data();
    params.num_views = num_views;
    params.tau = 0.05f;
    params.market_weights = market_weights.data();

    // Don't pre-allocate - let the function handle allocation
    fin_opt_extended_result_t result = {};

    bool ok = fin_opt_gpu_black_litterman(ctx_, market_covariance.data(),
                                           n_assets, &params, &result);
    ASSERT_TRUE(ok);

    // Verify convergence
    EXPECT_TRUE(result.base.converged);

    // Verify all weights are valid
    for (uint32_t i = 0; i < n_assets; i++) {
        EXPECT_GE(result.base.optimal_weights[i], -TOLERANCE);
        EXPECT_LE(result.base.optimal_weights[i], 1.0f + TOLERANCE);
    }

    // Use the proper free function
    fin_opt_extended_result_free(&result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Risk Parity Optimization
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, RiskParity) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 3;

    std::vector<float> covariance = {
        0.04f, 0.01f, 0.005f,
        0.01f, 0.09f, 0.02f,
        0.005f, 0.02f, 0.01f
    };

    fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
    params.n_assets = n_assets;
    params.max_iterations = 2000;
    params.convergence_tolerance = 1e-6f;

    fin_optimization_gpu_result_t result = {};
    result.optimal_weights = new float[n_assets];

    bool ok = fin_opt_gpu_risk_parity(ctx_, covariance.data(), n_assets,
                                       nullptr,  // Equal risk contribution
                                       &params, &result);
    ASSERT_TRUE(ok);

    // Verify weights sum to 1
    float weight_sum = 0.0f;
    for (uint32_t i = 0; i < n_assets; i++) {
        EXPECT_GE(result.optimal_weights[i], 0.0f)
            << "Risk parity weights should be positive";
        weight_sum += result.optimal_weights[i];
    }
    EXPECT_NEAR(weight_sum, 1.0f, RELAXED_TOLERANCE);

    // Lower variance assets should have higher weights in risk parity
    // Asset 2 has lowest variance (0.01)
    EXPECT_GT(result.optimal_weights[2], result.optimal_weights[1])
        << "Low-vol asset should have higher weight in risk parity";

    delete[] result.optimal_weights;
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Risk Contribution Computation
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, RiskContribution) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n_assets = 3;

    std::vector<float> weights = {0.4f, 0.35f, 0.25f};
    std::vector<float> covariance = {
        0.04f, 0.01f, 0.005f,
        0.01f, 0.09f, 0.02f,
        0.005f, 0.02f, 0.01f
    };

    std::vector<float> marginal_risk(n_assets);
    std::vector<float> risk_contribution(n_assets);

    bool ok = fin_opt_gpu_risk_contribution(ctx_, weights.data(), covariance.data(),
                                             n_assets, marginal_risk.data(),
                                             risk_contribution.data());
    ASSERT_TRUE(ok);

    // Risk contributions should sum to total portfolio risk
    float total_contribution = 0.0f;
    for (uint32_t i = 0; i < n_assets; i++) {
        EXPECT_GE(risk_contribution[i], 0.0f)
            << "Risk contribution should be non-negative";
        total_contribution += risk_contribution[i];
    }

    float total_variance = portfolio_variance(weights, covariance, n_assets);
    float total_vol = std::sqrt(total_variance);

    // Risk contributions should approximately sum to portfolio volatility
    EXPECT_NEAR(total_contribution, total_vol, RELAXED_TOLERANCE)
        << "Risk contributions should sum to total risk";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Covariance Matrix Inversion
 * ============================================================================ */
TEST_F(FinancialOptimizationGPUTest, CovarianceInversion) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n = 3;

    // Positive definite covariance matrix
    std::vector<float> covariance = {
        0.04f, 0.01f, 0.005f,
        0.01f, 0.09f, 0.02f,
        0.005f, 0.02f, 0.01f
    };

    std::vector<float> inverse(n * n);

    bool ok = fin_opt_gpu_invert_covariance(ctx_, covariance.data(), n, inverse.data());
    ASSERT_TRUE(ok);

    // Verify: Cov * Cov^-1 should be approximately identity
    std::vector<float> product(n * n, 0.0f);
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            for (uint32_t k = 0; k < n; k++) {
                product[i * n + j] += covariance[i * n + k] * inverse[k * n + j];
            }
        }
    }

    // Check if result is close to identity matrix
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            float expected = (i == j) ? 1.0f : 0.0f;
            EXPECT_NEAR(product[i * n + j], expected, RELAXED_TOLERANCE)
                << "Inverse verification failed at (" << i << "," << j << ")";
        }
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  // namespace
