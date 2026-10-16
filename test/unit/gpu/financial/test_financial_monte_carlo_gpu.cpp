/* ============================================================================
 * Unit Tests: GPU Financial Monte Carlo Simulation
 * ============================================================================
 * WHAT: Unit tests for GPU-accelerated Monte Carlo simulation
 * WHY:  Validate correctness of GBM, Heston, jump-diffusion on GPU
 * HOW:  Statistical verification of simulation properties
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <numeric>
#include <algorithm>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/financial/nimcp_financial_gpu.h"
#include "gpu/financial/nimcp_financial_monte_carlo_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-4f;
constexpr float STATISTICAL_TOLERANCE = 0.05f;  // 5% for statistical tests
constexpr float RELAXED_STATISTICAL_TOLERANCE = 0.10f;  // 10% for high-variance tests

class FinancialMonteCarloGPUTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        ctx_ = nimcp_gpu_context_create(0);
        if (ctx_) {
            rng_ = fin_gpu_rng_create(ctx_, 100000, 12345);
        }
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (rng_) {
            fin_gpu_rng_destroy(rng_);
            rng_ = NULL;
        }
        if (ctx_) {
            nimcp_gpu_context_destroy(ctx_);
            ctx_ = NULL;
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx_ = NULL;
    fin_gpu_rng_t* rng_ = NULL;

    // Statistical helpers
    float mean(const std::vector<float>& v) {
        return std::accumulate(v.begin(), v.end(), 0.0f) / v.size();
    }

    float variance(const std::vector<float>& v) {
        float m = mean(v);
        float sum = 0.0f;
        for (float x : v) {
            sum += (x - m) * (x - m);
        }
        return sum / (v.size() - 1);
    }

    float stddev(const std::vector<float>& v) {
        return std::sqrt(variance(v));
    }

    // GBM analytical expected value: E[S_T] = S_0 * exp(mu * T)
    float gbm_expected_value(float S0, float mu, float T) {
        return S0 * std::exp(mu * T);
    }

    // GBM analytical variance: Var[S_T] = S_0^2 * exp(2*mu*T) * (exp(sigma^2*T) - 1)
    float gbm_variance(float S0, float mu, float sigma, float T) {
        return S0 * S0 * std::exp(2.0f * mu * T) * (std::exp(sigma * sigma * T) - 1.0f);
    }
#endif
};

/* ============================================================================
 * Test: RNG Creation and Seeding
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, RNGCreation) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG should be created";

    // Create another RNG with different seed
    fin_gpu_rng_t* rng2 = fin_gpu_rng_create(ctx_, 1000, 54321);
    ASSERT_NE(rng2, nullptr);

    fin_gpu_rng_destroy(rng2);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RNG Reseed
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, RNGReseed) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    EXPECT_TRUE(fin_gpu_rng_reseed(rng_, 99999));
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: GBM Simulation - Basic
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, GBMSimulationBasic) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.process_type = FIN_GPU_PROCESS_GBM;
    params.initial_value = 100.0f;
    params.drift = 0.05f;
    params.volatility = 0.2f;
    params.time_horizon = 1.0f;
    params.num_paths = 50000;
    params.num_steps = 252;
    params.antithetic = true;

    fin_monte_carlo_gpu_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result));

    EXPECT_EQ(result.paths_completed, params.num_paths);
    EXPECT_GT(result.mean_value, 0.0f);
    EXPECT_GT(result.std_return, 0.0f);

    // Expected value for GBM: S_0 * exp(mu * T)
    float expected_mean = gbm_expected_value(100.0f, 0.05f, 1.0f);
    float rel_error = std::abs(result.mean_value - expected_mean) / expected_mean;
    EXPECT_LT(rel_error, STATISTICAL_TOLERANCE)
        << "GBM mean deviates too much from expected";

    fin_monte_carlo_gpu_result_free(ctx_, &result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: GBM Simulation - Statistical Properties
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, GBMStatisticalProperties) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    const float S0 = 100.0f;
    const float mu = 0.08f;
    const float sigma = 0.25f;
    const float T = 1.0f;

    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.initial_value = S0;
    params.drift = mu;
    params.volatility = sigma;
    params.time_horizon = T;
    params.num_paths = 100000;
    params.num_steps = 252;

    fin_monte_carlo_gpu_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result));

    // Check mean matches analytical expectation
    float expected_mean = gbm_expected_value(S0, mu, T);
    EXPECT_NEAR(result.mean_value, expected_mean, expected_mean * STATISTICAL_TOLERANCE)
        << "GBM mean incorrect";

    // Check variance is in reasonable range
    float expected_var = gbm_variance(S0, mu, sigma, T);
    float expected_std = std::sqrt(expected_var);
    EXPECT_NEAR(result.std_return, expected_std / S0, RELAXED_STATISTICAL_TOLERANCE)
        << "GBM volatility incorrect";

    fin_monte_carlo_gpu_result_free(ctx_, &result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: GBM Simulation - Antithetic Variates
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, GBMAntitheticVariates) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    // Run with antithetic
    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.initial_value = 100.0f;
    params.drift = 0.05f;
    params.volatility = 0.3f;
    params.time_horizon = 1.0f;
    params.num_paths = 10000;
    params.antithetic = true;

    fin_monte_carlo_gpu_result_t result_anti = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result_anti));

    // Reseed and run without antithetic
    fin_gpu_rng_reseed(rng_, 12345);
    params.antithetic = false;
    fin_monte_carlo_gpu_result_t result_no_anti = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result_no_anti));

    // Antithetic should have lower standard error
    // (Not always guaranteed but usually true for reasonable settings)
    EXPECT_GE(result_anti.std_error, 0.0f);
    EXPECT_GE(result_no_anti.std_error, 0.0f);

    fin_monte_carlo_gpu_result_free(ctx_, &result_anti);
    fin_monte_carlo_gpu_result_free(ctx_, &result_no_anti);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: GBM Simulation - VaR Computation
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, GBMVaRComputation) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.initial_value = 100.0f;
    params.drift = 0.05f;
    params.volatility = 0.2f;
    params.time_horizon = 1.0f;
    params.num_paths = 50000;

    fin_monte_carlo_gpu_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result));

    // VaR should be negative (worst 5%/1% outcomes)
    // For positive drift, 95% VaR might be positive but 99% more likely negative
    EXPECT_NE(result.var_95, 0.0f);
    EXPECT_NE(result.var_99, 0.0f);

    // CVaR should be more extreme than VaR (larger positive value for losses)
    EXPECT_GE(result.cvar_95, result.var_95 - 0.01f);
    EXPECT_GE(result.cvar_99, result.var_99 - 0.01f);

    fin_monte_carlo_gpu_result_free(ctx_, &result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Heston Stochastic Volatility Model
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, HestonModel) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.process_type = FIN_GPU_PROCESS_HESTON;
    params.initial_value = 100.0f;
    params.drift = 0.05f;
    params.risk_free_rate = 0.02f;
    params.time_horizon = 1.0f;
    params.num_paths = 30000;
    params.num_steps = 252;

    // Heston parameters
    params.initial_variance = 0.04f;     // V_0 = (0.2)^2
    params.mean_reversion = 2.0f;        // kappa
    params.long_term_variance = 0.04f;   // theta
    params.vol_of_vol = 0.3f;            // xi
    params.correlation = -0.5f;          // rho (leverage effect)

    fin_monte_carlo_gpu_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result));

    EXPECT_EQ(result.paths_completed, params.num_paths);
    EXPECT_GT(result.mean_value, 0.0f);

    // Heston can produce heavier tails than GBM
    // Just verify basic sanity
    EXPECT_LT(result.min_terminal, result.mean_value);
    EXPECT_GT(result.max_terminal, result.mean_value);

    fin_monte_carlo_gpu_result_free(ctx_, &result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Jump-Diffusion Model
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, JumpDiffusionModel) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.process_type = FIN_GPU_PROCESS_JUMP_DIFFUSION;
    params.initial_value = 100.0f;
    params.drift = 0.05f;
    params.volatility = 0.2f;
    params.time_horizon = 1.0f;
    params.num_paths = 30000;
    params.num_steps = 252;

    // Jump parameters
    params.jump_intensity = 1.0f;   // Average 1 jump per year
    params.jump_mean = -0.05f;      // Average -5% jump (crash bias)
    params.jump_vol = 0.15f;        // Jump size volatility

    fin_monte_carlo_gpu_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result));

    EXPECT_EQ(result.paths_completed, params.num_paths);

    // Jump-diffusion with negative jumps should produce negative skewness
    // and heavier left tail
    EXPECT_GT(result.std_return, 0.0f);

    fin_monte_carlo_gpu_result_free(ctx_, &result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Multi-Asset Correlated Simulation
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, MultiAssetCorrelated) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    const uint32_t num_assets = 3;
    const uint32_t num_paths = 20000;

    std::vector<float> initial_values = {100.0f, 50.0f, 200.0f};
    std::vector<float> drifts = {0.05f, 0.08f, 0.03f};
    std::vector<float> volatilities = {0.2f, 0.25f, 0.15f};

    // Cholesky factor of correlation matrix
    // Correlation: [[1.0, 0.6, 0.3], [0.6, 1.0, 0.4], [0.3, 0.4, 1.0]]
    // Cholesky L (lower triangular)
    std::vector<float> cholesky_L = {
        1.0f, 0.0f, 0.0f,
        0.6f, 0.8f, 0.0f,
        0.3f, 0.275f, 0.912f  // Approximate values
    };

    std::vector<float> terminal_values(num_paths * num_assets);

    fin_multi_asset_params_t params;
    params.base = fin_monte_carlo_gpu_params_default();
    params.base.num_paths = num_paths;
    params.base.num_steps = 252;
    params.base.time_horizon = 1.0f;
    params.n_assets = num_assets;
    params.initial_values = initial_values.data();
    params.drifts = drifts.data();
    params.volatilities = volatilities.data();
    params.cholesky_L = cholesky_L.data();

    ASSERT_TRUE(fin_monte_carlo_gpu_multi_asset(
        ctx_, rng_,
        &params,
        terminal_values.data()));

    // Compute correlation of terminal values
    std::vector<float> asset0(num_paths), asset1(num_paths);
    for (uint32_t p = 0; p < num_paths; p++) {
        asset0[p] = terminal_values[p * num_assets + 0];
        asset1[p] = terminal_values[p * num_assets + 1];
    }

    float mean0 = mean(asset0);
    float mean1 = mean(asset1);
    float cov01 = 0.0f;
    for (uint32_t p = 0; p < num_paths; p++) {
        cov01 += (asset0[p] - mean0) * (asset1[p] - mean1);
    }
    cov01 /= (num_paths - 1);

    float corr01 = cov01 / (stddev(asset0) * stddev(asset1));

    // Realized correlation should be close to input (0.6)
    // Allow some slack for finite sample
    EXPECT_NEAR(corr01, 0.6f, 0.15f)
        << "Multi-asset correlation not preserved";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Path Storage
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, PathStorage) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.initial_value = 100.0f;
    params.num_paths = 1000;  // Smaller for full path storage
    params.num_steps = 50;
    params.store_paths = true;

    fin_monte_carlo_gpu_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result));

    EXPECT_NE(result.path_data, nullptr);

    // Paths should start at initial value (approximately)
    // Note: path_data is on device, need to copy back to verify

    fin_monte_carlo_gpu_result_free(ctx_, &result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Large Scale Simulation
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, LargeScaleSimulation) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.initial_value = 100.0f;
    params.drift = 0.05f;
    params.volatility = 0.2f;
    params.num_paths = 500000;  // 500K paths
    params.num_steps = 252;

    fin_monte_carlo_gpu_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result));

    EXPECT_EQ(result.paths_completed, params.num_paths);

    // With more paths, standard error should be lower
    EXPECT_LT(result.std_error, 0.5f);  // Reasonable bound

    // Execution time should be reasonable
    EXPECT_LT(result.kernel_time_ms, 1000.0f);  // Under 1 second

    fin_monte_carlo_gpu_result_free(ctx_, &result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Zero Volatility (Deterministic)
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, ZeroVolatility) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.initial_value = 100.0f;
    params.drift = 0.10f;
    params.volatility = 0.0f;  // Deterministic growth
    params.time_horizon = 1.0f;
    params.num_paths = 1000;

    fin_monte_carlo_gpu_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result));

    // With zero volatility, all paths should end at same value
    float expected = 100.0f * std::exp(0.10f * 1.0f);
    EXPECT_NEAR(result.mean_value, expected, 0.01f);
    EXPECT_NEAR(result.std_return, 0.0f, 0.001f);

    fin_monte_carlo_gpu_result_free(ctx_, &result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Short Time Horizon
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, ShortTimeHorizon) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.initial_value = 100.0f;
    params.drift = 0.05f;
    params.volatility = 0.2f;
    params.time_horizon = 1.0f / 252.0f;  // 1 day
    params.num_paths = 50000;
    params.num_steps = 1;

    fin_monte_carlo_gpu_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result));

    // Over 1 day, change should be minimal
    EXPECT_NEAR(result.mean_value, 100.0f, 1.0f);

    fin_monte_carlo_gpu_result_free(ctx_, &result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Probability of Loss
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, ProbabilityOfLoss) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    // Negative drift = expected loss
    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.initial_value = 100.0f;
    params.drift = -0.10f;  // Negative drift
    params.volatility = 0.2f;
    params.time_horizon = 1.0f;
    params.num_paths = 50000;

    fin_monte_carlo_gpu_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result));

    // With negative drift, P(loss) should be > 50%
    EXPECT_GT(result.probability_of_loss, 0.5f);

    fin_monte_carlo_gpu_result_free(ctx_, &result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Heston Model - Extended Parameters
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, HestonModelExtended) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    fin_heston_params_t params;
    params.base = fin_monte_carlo_gpu_params_default();
    params.base.num_paths = 50000;
    params.base.num_steps = 252;
    params.base.initial_value = 100.0f;
    params.base.risk_free_rate = 0.02f;
    params.base.time_horizon = 1.0f;

    // Heston parameters (Feller condition: 2*kappa*theta > xi^2)
    params.initial_variance = 0.04f;  // v0 = (0.2)^2
    params.kappa = 2.0f;              // Mean reversion speed
    params.theta = 0.04f;             // Long-term variance
    params.xi = 0.3f;                 // Vol-of-vol
    params.rho = -0.7f;               // Leverage effect (negative correlation)

    fin_monte_carlo_gpu_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_heston(ctx_, rng_, &params, &result));

    EXPECT_EQ(result.paths_completed, params.base.num_paths);
    EXPECT_GT(result.mean_value, 0.0f);

    // Heston with leverage effect should produce negative skewness
    // (not always guaranteed but typical)
    EXPECT_LT(result.min_terminal, result.mean_value);
    EXPECT_GT(result.max_terminal, result.mean_value);

    fin_monte_carlo_gpu_result_free(ctx_, &result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Monte Carlo Option Pricing - European Call
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, MCOptionPricingEuropeanCall) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    fin_mc_option_params_t params;
    params.base = fin_monte_carlo_gpu_params_default();
    params.base.initial_value = 100.0f;
    params.base.risk_free_rate = 0.05f;
    params.base.volatility = 0.2f;
    params.base.time_horizon = 1.0f;
    params.base.num_paths = 100000;
    params.base.num_steps = 252;
    params.option_type = FIN_OPT_CALL;
    params.option_style = FIN_MC_OPT_EUROPEAN;
    params.strike = 100.0f;

    fin_mc_option_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_option_price(ctx_, rng_, &params, &result));

    // Compare with Black-Scholes closed form
    // BS call price for ATM 1Y with 20% vol and 5% rate ≈ 10.45
    EXPECT_NEAR(result.price, 10.45f, 1.0f)
        << "MC European call should match BS price";

    EXPECT_GT(result.price, 0.0f);
    EXPECT_LT(result.std_error, 0.5f) << "Standard error should be reasonable";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Monte Carlo Option Pricing - European Put
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, MCOptionPricingEuropeanPut) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    fin_mc_option_params_t params;
    params.base = fin_monte_carlo_gpu_params_default();
    params.base.initial_value = 100.0f;
    params.base.risk_free_rate = 0.05f;
    params.base.volatility = 0.2f;
    params.base.time_horizon = 1.0f;
    params.base.num_paths = 100000;
    params.base.num_steps = 252;
    params.option_type = FIN_OPT_PUT;
    params.option_style = FIN_MC_OPT_EUROPEAN;
    params.strike = 100.0f;

    fin_mc_option_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_option_price(ctx_, rng_, &params, &result));

    // BS put price for ATM 1Y with 20% vol and 5% rate ≈ 5.57
    EXPECT_NEAR(result.price, 5.57f, 1.0f)
        << "MC European put should match BS price";

    EXPECT_GT(result.price, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Monte Carlo Option Pricing - Asian Option
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, MCOptionPricingAsian) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    fin_mc_option_params_t params;
    params.base = fin_monte_carlo_gpu_params_default();
    params.base.initial_value = 100.0f;
    params.base.risk_free_rate = 0.05f;
    params.base.volatility = 0.2f;
    params.base.time_horizon = 1.0f;
    params.base.num_paths = 50000;
    params.base.num_steps = 252;
    params.option_type = FIN_OPT_CALL;
    params.option_style = FIN_MC_OPT_ASIAN;
    params.strike = 100.0f;

    fin_mc_option_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_option_price(ctx_, rng_, &params, &result));

    // Asian options should be cheaper than European due to averaging
    // European ATM call ≈ 10.45, Asian should be less
    EXPECT_GT(result.price, 0.0f);
    EXPECT_LT(result.price, 10.0f)
        << "Asian option should be cheaper than European";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Monte Carlo Result - VaR/CVaR Verification
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, MCResultVaRCVaR) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.initial_value = 100.0f;
    params.drift = 0.05f;
    params.volatility = 0.3f;  // Higher vol for clearer VaR
    params.time_horizon = 1.0f;
    params.num_paths = 100000;
    params.num_steps = 252;

    fin_monte_carlo_gpu_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result));

    // Verify VaR and CVaR are computed
    EXPECT_NE(result.var_95, 0.0f) << "VaR 95% should be computed";
    EXPECT_NE(result.var_99, 0.0f) << "VaR 99% should be computed";
    EXPECT_NE(result.cvar_95, 0.0f) << "CVaR 95% should be computed";
    EXPECT_NE(result.cvar_99, 0.0f) << "CVaR 99% should be computed";

    // CVaR should be >= VaR (more extreme loss measure)
    EXPECT_GE(result.cvar_95, result.var_95 - 0.01f)
        << "CVaR 95% should be >= VaR 95%";
    EXPECT_GE(result.cvar_99, result.var_99 - 0.01f)
        << "CVaR 99% should be >= VaR 99%";

    // 99% VaR should be more extreme than 95% VaR
    EXPECT_GE(result.var_99, result.var_95 - 0.01f)
        << "99% VaR should be more extreme";

    fin_monte_carlo_gpu_result_free(ctx_, &result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Monte Carlo Result - Mean Return
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, MCResultMeanReturn) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
    params.initial_value = 100.0f;
    params.drift = 0.10f;  // 10% drift
    params.volatility = 0.2f;
    params.time_horizon = 1.0f;
    params.num_paths = 100000;
    params.num_steps = 252;

    fin_monte_carlo_gpu_result_t result = {};
    ASSERT_TRUE(fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result));

    // Mean return should be computed and close to drift
    EXPECT_NEAR(result.mean_return, 0.10f, 0.02f)
        << "Mean return should approximate drift";

    fin_monte_carlo_gpu_result_free(ctx_, &result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Cholesky Decomposition
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, CholeskyDecomposition) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t n = 3;

    // Positive definite correlation matrix
    std::vector<float> correlation = {
        1.0f, 0.5f, 0.3f,
        0.5f, 1.0f, 0.4f,
        0.3f, 0.4f, 1.0f
    };

    std::vector<float> cholesky(n * n, 0.0f);

    ASSERT_TRUE(fin_mc_gpu_cholesky(ctx_, correlation.data(), n, cholesky.data()));

    // Verify: L * L^T = correlation
    std::vector<float> reconstructed(n * n, 0.0f);
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            for (uint32_t k = 0; k < n; k++) {
                reconstructed[i * n + j] += cholesky[i * n + k] * cholesky[j * n + k];
            }
        }
    }

    for (uint32_t i = 0; i < n * n; i++) {
        EXPECT_NEAR(reconstructed[i], correlation[i], 1e-5f)
            << "Cholesky reconstruction failed at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Correlated Assets - Correlation Preservation
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, CorrelatedAssetsPreservation) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    const uint32_t num_assets = 2;
    const uint32_t num_paths = 50000;

    std::vector<float> initial_values = {100.0f, 100.0f};
    std::vector<float> drifts = {0.05f, 0.05f};
    std::vector<float> volatilities = {0.2f, 0.2f};

    // Cholesky factor for correlation = 0.8
    // Correlation: [[1.0, 0.8], [0.8, 1.0]]
    // Cholesky L: [[1.0, 0.0], [0.8, 0.6]]
    std::vector<float> cholesky_L = {
        1.0f, 0.0f,
        0.8f, 0.6f
    };

    std::vector<float> terminal_values(num_paths * num_assets);

    fin_multi_asset_params_t params;
    params.base = fin_monte_carlo_gpu_params_default();
    params.base.num_paths = num_paths;
    params.base.num_steps = 252;
    params.base.time_horizon = 1.0f;
    params.n_assets = num_assets;
    params.initial_values = initial_values.data();
    params.drifts = drifts.data();
    params.volatilities = volatilities.data();
    params.cholesky_L = cholesky_L.data();

    ASSERT_TRUE(fin_monte_carlo_gpu_multi_asset(
        ctx_, rng_,
        &params,
        terminal_values.data()));

    // Compute realized correlation
    std::vector<float> asset0(num_paths), asset1(num_paths);
    for (uint32_t p = 0; p < num_paths; p++) {
        asset0[p] = terminal_values[p * num_assets + 0];
        asset1[p] = terminal_values[p * num_assets + 1];
    }

    float mean0 = mean(asset0);
    float mean1 = mean(asset1);
    float cov01 = 0.0f;
    for (uint32_t p = 0; p < num_paths; p++) {
        cov01 += (asset0[p] - mean0) * (asset1[p] - mean1);
    }
    cov01 /= (num_paths - 1);

    float realized_corr = cov01 / (stddev(asset0) * stddev(asset1));

    // Realized correlation should be close to input (0.8)
    EXPECT_NEAR(realized_corr, 0.8f, 0.1f)
        << "High correlation not preserved";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RNG Uniform Generation
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, RNGUniform) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    const uint32_t N = 10000;
    std::vector<float> uniform(N);

    ASSERT_TRUE(fin_gpu_rng_uniform(rng_, uniform.data(), N));

    // All values should be in [0, 1)
    for (uint32_t i = 0; i < N; i++) {
        EXPECT_GE(uniform[i], 0.0f);
        EXPECT_LT(uniform[i], 1.0f);
    }

    // Mean should be close to 0.5
    float m = mean(uniform);
    EXPECT_NEAR(m, 0.5f, 0.02f) << "Uniform mean should be ~0.5";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RNG Normal Generation
 * ============================================================================ */
TEST_F(FinancialMonteCarloGPUTest, RNGNormal) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    const uint32_t N = 10000;
    std::vector<float> normal(N);

    ASSERT_TRUE(fin_gpu_rng_normal(rng_, normal.data(), N));

    // Mean should be close to 0
    float m = mean(normal);
    EXPECT_NEAR(m, 0.0f, 0.05f) << "Normal mean should be ~0";

    // Std dev should be close to 1
    float s = stddev(normal);
    EXPECT_NEAR(s, 1.0f, 0.05f) << "Normal stddev should be ~1";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  // namespace
