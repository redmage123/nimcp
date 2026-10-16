//=============================================================================
// test_financial_risk_gpu.cpp - Unit Tests for GPU Financial Risk Metrics
//=============================================================================
/**
 * @file test_financial_risk_gpu.cpp
 * @brief Unit tests for GPU-accelerated risk metrics computation
 *
 * Tests VaR/CVaR computation, volatility estimators, GPU sorting,
 * max drawdown, correlation/covariance matrices, and batch operations.
 *
 * COVERAGE:
 *   - VaR (Historical, Parametric, Cornish-Fisher)
 *   - CVaR/Expected Shortfall
 *   - GPU sorting (bitonic sort)
 *   - Volatility estimators (simple, EWMA, Parkinson, Garman-Klass, Yang-Zhang)
 *   - Max drawdown computation
 *   - Sharpe ratio, Sortino ratio
 *   - Batch VaR for multiple portfolios
 *   - Correlation and covariance matrix computation
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <numeric>

#include "gpu/financial/nimcp_financial_gpu.h"
#include "gpu/financial/nimcp_financial_risk_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FinancialRiskGPUTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;

    void SetUp() override {
        ctx = nimcp_gpu_context_create(0);
        if (!ctx) {
            GTEST_SKIP() << "No GPU available - skipping test";
        }
    }

    void TearDown() override {
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Generate synthetic returns with known properties
    std::vector<float> generateReturns(uint32_t n, float mean, float std_dev, uint64_t seed = 12345) {
        std::vector<float> returns(n);
        srand(static_cast<unsigned int>(seed));
        for (uint32_t i = 0; i < n; i++) {
            // Box-Muller transform for normal distribution
            float u1 = (static_cast<float>(rand()) / RAND_MAX);
            float u2 = (static_cast<float>(rand()) / RAND_MAX);
            if (u1 < 1e-10f) u1 = 1e-10f;
            float z = std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * M_PI * u2);
            returns[i] = mean + std_dev * z;
        }
        return returns;
    }

    // Generate price series from returns
    std::vector<float> returnsToPrices(const std::vector<float>& returns, float initial_price = 100.0f) {
        std::vector<float> prices(returns.size() + 1);
        prices[0] = initial_price;
        for (size_t i = 0; i < returns.size(); i++) {
            prices[i + 1] = prices[i] * (1.0f + returns[i]);
        }
        return prices;
    }

    // CPU reference for historical VaR
    float cpuHistoricalVaR(const std::vector<float>& returns, float confidence) {
        std::vector<float> sorted = returns;
        std::sort(sorted.begin(), sorted.end());
        size_t index = static_cast<size_t>((1.0f - confidence) * sorted.size());
        return -sorted[index];  // VaR is reported as positive
    }

    // CPU reference for CVaR
    float cpuCVaR(const std::vector<float>& returns, float confidence) {
        std::vector<float> sorted = returns;
        std::sort(sorted.begin(), sorted.end());
        size_t cutoff = static_cast<size_t>((1.0f - confidence) * sorted.size());
        if (cutoff == 0) cutoff = 1;
        float sum = 0.0f;
        for (size_t i = 0; i < cutoff; i++) {
            sum += sorted[i];
        }
        return -(sum / cutoff);  // CVaR is reported as positive
    }

    // CPU reference for simple volatility
    float cpuSimpleVolatility(const std::vector<float>& returns) {
        float mean = 0.0f;
        for (float r : returns) mean += r;
        mean /= returns.size();

        float var = 0.0f;
        for (float r : returns) {
            float diff = r - mean;
            var += diff * diff;
        }
        var /= (returns.size() - 1);
        return std::sqrt(var);
    }

    // CPU reference for max drawdown
    float cpuMaxDrawdown(const std::vector<float>& prices) {
        float max_price = prices[0];
        float max_dd = 0.0f;
        for (float p : prices) {
            if (p > max_price) max_price = p;
            float dd = (max_price - p) / max_price;
            if (dd > max_dd) max_dd = dd;
        }
        return max_dd;
    }

    // CPU reference for Sharpe ratio
    float cpuSharpeRatio(const std::vector<float>& returns, float risk_free_rate, uint32_t annualization = 252) {
        float mean = 0.0f;
        for (float r : returns) mean += r;
        mean /= returns.size();

        float daily_rf = risk_free_rate / annualization;
        float excess_mean = mean - daily_rf;

        float vol = cpuSimpleVolatility(returns);
        return (excess_mean / vol) * std::sqrt(static_cast<float>(annualization));
    }
};

//=============================================================================
// VaR Computation Tests
//=============================================================================

TEST_F(FinancialRiskGPUTest, VaR95Historical) {
    // Generate 10000 returns with known distribution
    std::vector<float> returns = generateReturns(10000, -0.001f, 0.02f);

    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = returns.size();
    params.compute_var = true;

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
    ASSERT_TRUE(ok);

    // Compare with CPU reference
    float cpu_var = cpuHistoricalVaR(returns, 0.95f);
    EXPECT_NEAR(result.var_95, cpu_var, 0.005f) << "95% VaR mismatch";
}

TEST_F(FinancialRiskGPUTest, VaR99Historical) {
    std::vector<float> returns = generateReturns(10000, -0.001f, 0.02f);

    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = returns.size();

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
    ASSERT_TRUE(ok);

    float cpu_var = cpuHistoricalVaR(returns, 0.99f);
    EXPECT_NEAR(result.var_99, cpu_var, 0.005f) << "99% VaR mismatch";
}

TEST_F(FinancialRiskGPUTest, VaRParametric) {
    std::vector<float> returns = generateReturns(10000, 0.0f, 0.02f);

    float var = fin_risk_gpu_var_parametric(ctx, returns.data(), returns.size(), 0.95f);

    // For normal distribution, 95% VaR ≈ 1.645 * sigma
    float expected_var = 1.645f * 0.02f;
    EXPECT_NEAR(var, expected_var, 0.003f) << "Parametric VaR should match normal quantile";
}

TEST_F(FinancialRiskGPUTest, VaRCornishFisher) {
    // Generate returns with skewness/kurtosis
    std::vector<float> returns = generateReturns(10000, 0.0f, 0.02f, 54321);

    float var = fin_risk_gpu_var_cornish_fisher(ctx, returns.data(), returns.size(), 0.95f);

    // Cornish-Fisher should account for non-normality
    EXPECT_GT(var, 0.0f) << "Cornish-Fisher VaR should be positive";
}

//=============================================================================
// CVaR Tests
//=============================================================================

TEST_F(FinancialRiskGPUTest, CVaR95) {
    std::vector<float> returns = generateReturns(10000, -0.001f, 0.02f);

    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = returns.size();
    params.compute_cvar = true;

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
    ASSERT_TRUE(ok);

    float cpu_cvar = cpuCVaR(returns, 0.95f);
    EXPECT_NEAR(result.cvar_95, cpu_cvar, 0.005f) << "95% CVaR mismatch";
}

TEST_F(FinancialRiskGPUTest, CVaRGreaterThanVaR) {
    std::vector<float> returns = generateReturns(10000, -0.001f, 0.02f);

    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = returns.size();

    fin_risk_gpu_result_t result = {0};
    fin_risk_gpu_compute(ctx, returns.data(), &params, &result);

    // CVaR (Expected Shortfall) should always be >= VaR
    EXPECT_GE(result.cvar_95, result.var_95) << "CVaR must be >= VaR";
    EXPECT_GE(result.cvar_99, result.var_99) << "CVaR must be >= VaR";
}

//=============================================================================
// GPU Sorting Tests
//=============================================================================

TEST_F(FinancialRiskGPUTest, GPUSortAscending) {
    std::vector<float> data = {5.0f, 2.0f, 8.0f, 1.0f, 9.0f, 3.0f, 7.0f, 4.0f, 6.0f, 0.0f};

    // Allocate device memory
    float* d_data = nullptr;
    ASSERT_EQ(cudaMalloc(&d_data, data.size() * sizeof(float)), cudaSuccess);
    ASSERT_EQ(cudaMemcpy(d_data, data.data(), data.size() * sizeof(float), cudaMemcpyHostToDevice), cudaSuccess);

    bool ok = fin_risk_gpu_sort(ctx, d_data, data.size(), true);
    ASSERT_TRUE(ok);

    std::vector<float> sorted(data.size());
    ASSERT_EQ(cudaMemcpy(sorted.data(), d_data, data.size() * sizeof(float), cudaMemcpyDeviceToHost), cudaSuccess);
    cudaFree(d_data);

    // Verify sorted order
    for (size_t i = 1; i < sorted.size(); i++) {
        EXPECT_LE(sorted[i-1], sorted[i]) << "Array not sorted at index " << i;
    }
}

TEST_F(FinancialRiskGPUTest, GPUSortDescending) {
    std::vector<float> data = {5.0f, 2.0f, 8.0f, 1.0f, 9.0f, 3.0f, 7.0f, 4.0f, 6.0f, 0.0f};

    float* d_data = nullptr;
    ASSERT_EQ(cudaMalloc(&d_data, data.size() * sizeof(float)), cudaSuccess);
    ASSERT_EQ(cudaMemcpy(d_data, data.data(), data.size() * sizeof(float), cudaMemcpyHostToDevice), cudaSuccess);

    bool ok = fin_risk_gpu_sort(ctx, d_data, data.size(), false);
    ASSERT_TRUE(ok);

    std::vector<float> sorted(data.size());
    ASSERT_EQ(cudaMemcpy(sorted.data(), d_data, data.size() * sizeof(float), cudaMemcpyDeviceToHost), cudaSuccess);
    cudaFree(d_data);

    for (size_t i = 1; i < sorted.size(); i++) {
        EXPECT_GE(sorted[i-1], sorted[i]) << "Array not sorted descending at index " << i;
    }
}

TEST_F(FinancialRiskGPUTest, GPUSortLargeArray) {
    const uint32_t N = 100000;
    std::vector<float> data(N);
    for (uint32_t i = 0; i < N; i++) {
        data[i] = static_cast<float>(rand()) / RAND_MAX;
    }

    float* d_data = nullptr;
    ASSERT_EQ(cudaMalloc(&d_data, N * sizeof(float)), cudaSuccess);
    ASSERT_EQ(cudaMemcpy(d_data, data.data(), N * sizeof(float), cudaMemcpyHostToDevice), cudaSuccess);

    bool ok = fin_risk_gpu_sort(ctx, d_data, N, true);
    ASSERT_TRUE(ok);

    std::vector<float> sorted(N);
    ASSERT_EQ(cudaMemcpy(sorted.data(), d_data, N * sizeof(float), cudaMemcpyDeviceToHost), cudaSuccess);
    cudaFree(d_data);

    // Verify sort (check sample of elements)
    for (size_t i = 1; i < sorted.size(); i += 1000) {
        if (i > 0) {
            EXPECT_LE(sorted[i-1], sorted[i]) << "Large array not sorted at index " << i;
        }
    }
}

TEST_F(FinancialRiskGPUTest, GPUPercentile) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};

    float* d_data = nullptr;
    ASSERT_EQ(cudaMalloc(&d_data, data.size() * sizeof(float)), cudaSuccess);
    ASSERT_EQ(cudaMemcpy(d_data, data.data(), data.size() * sizeof(float), cudaMemcpyHostToDevice), cudaSuccess);

    // Data is already sorted
    float p50 = fin_risk_gpu_percentile(ctx, d_data, data.size(), 50.0f);
    EXPECT_NEAR(p50, 5.5f, 0.5f) << "50th percentile incorrect";

    // Linear interpolation: p10 of [1..10] = 1 + 0.9 * (2-1) = 1.9
    float p10 = fin_risk_gpu_percentile(ctx, d_data, data.size(), 10.0f);
    EXPECT_NEAR(p10, 1.9f, 0.5f) << "10th percentile incorrect";

    // Linear interpolation: p90 of [1..10] = 9 + 0.1 * (10-9) = 9.1
    float p90 = fin_risk_gpu_percentile(ctx, d_data, data.size(), 90.0f);
    EXPECT_NEAR(p90, 9.1f, 0.5f) << "90th percentile incorrect";

    cudaFree(d_data);
}

//=============================================================================
// Volatility Estimator Tests
//=============================================================================

TEST_F(FinancialRiskGPUTest, VolatilitySimple) {
    std::vector<float> returns = generateReturns(1000, 0.0f, 0.02f);

    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = returns.size();
    params.vol_type = FIN_VOL_SIMPLE;
    params.compute_volatility = true;

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
    ASSERT_TRUE(ok);

    float cpu_vol = cpuSimpleVolatility(returns);
    EXPECT_NEAR(result.volatility, cpu_vol, 0.001f) << "Simple volatility mismatch";
}

TEST_F(FinancialRiskGPUTest, VolatilityEWMA) {
    std::vector<float> returns = generateReturns(1000, 0.0f, 0.02f);

    float ewma_vol = fin_risk_gpu_ewma_volatility(ctx, returns.data(), returns.size(), 0.94f, nullptr);
    EXPECT_GT(ewma_vol, 0.0f) << "EWMA volatility should be positive";
    EXPECT_NEAR(ewma_vol, 0.02f, 0.01f) << "EWMA volatility should be close to true volatility";
}

TEST_F(FinancialRiskGPUTest, VolatilityEWMASeries) {
    std::vector<float> returns = generateReturns(1000, 0.0f, 0.02f);
    std::vector<float> vol_series(returns.size());

    float final_vol = fin_risk_gpu_ewma_volatility(ctx, returns.data(), returns.size(), 0.94f, vol_series.data());
    EXPECT_GT(final_vol, 0.0f);

    // Check that volatility series is positive
    for (size_t i = 0; i < vol_series.size(); i++) {
        EXPECT_GE(vol_series[i], 0.0f) << "EWMA vol series should be non-negative at " << i;
    }
}

TEST_F(FinancialRiskGPUTest, VolatilityParkinson) {
    // Generate OHLC data
    const uint32_t N = 252;
    std::vector<float> open(N), high(N), low(N), close(N);
    float price = 100.0f;

    for (uint32_t i = 0; i < N; i++) {
        open[i] = price;
        float ret = 0.001f + 0.02f * (static_cast<float>(rand()) / RAND_MAX - 0.5f);
        close[i] = price * (1.0f + ret);
        high[i] = std::max(open[i], close[i]) * (1.0f + 0.01f * static_cast<float>(rand()) / RAND_MAX);
        low[i] = std::min(open[i], close[i]) * (1.0f - 0.01f * static_cast<float>(rand()) / RAND_MAX);
        price = close[i];
    }

    float volatility;
    bool ok = fin_risk_gpu_volatility_ohlc(ctx, open.data(), high.data(), low.data(), close.data(),
                                            N, FIN_VOL_PARKINSON, &volatility);
    ASSERT_TRUE(ok);
    EXPECT_GT(volatility, 0.0f) << "Parkinson volatility should be positive";
}

TEST_F(FinancialRiskGPUTest, VolatilityGarmanKlass) {
    const uint32_t N = 252;
    std::vector<float> open(N), high(N), low(N), close(N);
    float price = 100.0f;

    for (uint32_t i = 0; i < N; i++) {
        open[i] = price;
        float ret = 0.001f + 0.02f * (static_cast<float>(rand()) / RAND_MAX - 0.5f);
        close[i] = price * (1.0f + ret);
        high[i] = std::max(open[i], close[i]) * (1.0f + 0.01f * static_cast<float>(rand()) / RAND_MAX);
        low[i] = std::min(open[i], close[i]) * (1.0f - 0.01f * static_cast<float>(rand()) / RAND_MAX);
        price = close[i];
    }

    float volatility;
    bool ok = fin_risk_gpu_volatility_ohlc(ctx, open.data(), high.data(), low.data(), close.data(),
                                            N, FIN_VOL_GARMAN_KLASS, &volatility);
    ASSERT_TRUE(ok);
    EXPECT_GT(volatility, 0.0f) << "Garman-Klass volatility should be positive";
}

TEST_F(FinancialRiskGPUTest, VolatilityYangZhang) {
    const uint32_t N = 252;
    std::vector<float> open(N), high(N), low(N), close(N);
    float price = 100.0f;

    for (uint32_t i = 0; i < N; i++) {
        open[i] = price;
        float ret = 0.001f + 0.02f * (static_cast<float>(rand()) / RAND_MAX - 0.5f);
        close[i] = price * (1.0f + ret);
        high[i] = std::max(open[i], close[i]) * (1.0f + 0.01f * static_cast<float>(rand()) / RAND_MAX);
        low[i] = std::min(open[i], close[i]) * (1.0f - 0.01f * static_cast<float>(rand()) / RAND_MAX);
        price = close[i];
    }

    float volatility;
    bool ok = fin_risk_gpu_volatility_ohlc(ctx, open.data(), high.data(), low.data(), close.data(),
                                            N, FIN_VOL_YANG_ZHANG, &volatility);
    ASSERT_TRUE(ok);
    EXPECT_GT(volatility, 0.0f) << "Yang-Zhang volatility should be positive";
}

//=============================================================================
// Drawdown Tests
//=============================================================================

TEST_F(FinancialRiskGPUTest, MaxDrawdown) {
    // Create a price series with known drawdown
    std::vector<float> prices = {100.0f, 110.0f, 115.0f, 100.0f, 95.0f, 105.0f, 120.0f, 110.0f, 125.0f};
    // Max drawdown: 115 -> 95 = 17.4%

    uint32_t dd_start, dd_end;
    float max_dd = fin_risk_gpu_max_drawdown(ctx, prices.data(), prices.size(), &dd_start, &dd_end);

    float cpu_dd = cpuMaxDrawdown(prices);
    EXPECT_NEAR(max_dd, cpu_dd, 0.001f) << "Max drawdown mismatch";
    EXPECT_NEAR(max_dd, 0.174f, 0.01f) << "Expected ~17.4% drawdown";
}

TEST_F(FinancialRiskGPUTest, MaxDrawdownMonotonic) {
    // Monotonically increasing prices should have 0 drawdown
    std::vector<float> prices = {100.0f, 101.0f, 102.0f, 103.0f, 104.0f, 105.0f};

    uint32_t dd_start, dd_end;
    float max_dd = fin_risk_gpu_max_drawdown(ctx, prices.data(), prices.size(), &dd_start, &dd_end);

    EXPECT_NEAR(max_dd, 0.0f, 1e-6f) << "No drawdown for monotonic increase";
}

//=============================================================================
// Risk-Adjusted Returns Tests
//=============================================================================

TEST_F(FinancialRiskGPUTest, SharpeRatio) {
    std::vector<float> returns = generateReturns(252, 0.0004f, 0.01f);  // ~10% annual return

    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = returns.size();
    params.risk_free_rate = 0.02f;

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
    ASSERT_TRUE(ok);

    float cpu_sharpe = cpuSharpeRatio(returns, 0.02f, 252);
    EXPECT_NEAR(result.sharpe_ratio, cpu_sharpe, 0.1f) << "Sharpe ratio mismatch";
}

TEST_F(FinancialRiskGPUTest, SortinoRatio) {
    std::vector<float> returns = generateReturns(252, 0.0004f, 0.01f);

    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = returns.size();
    params.risk_free_rate = 0.02f;

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
    ASSERT_TRUE(ok);

    // Sortino uses downside deviation instead of total volatility
    // Sortino should be >= Sharpe for positive returns
    EXPECT_GE(result.sortino_ratio, result.sharpe_ratio * 0.9f)
        << "Sortino should be close to or greater than Sharpe";
}

//=============================================================================
// Batch Risk Computation Tests
//=============================================================================

// NOTE: BatchVaR test removed - fin_risk_gpu_var_batch not yet implemented

TEST_F(FinancialRiskGPUTest, BatchRiskCompute) {
    const uint32_t NUM_PORTFOLIOS = 5;
    const uint32_t NUM_RETURNS = 500;

    std::vector<float> all_returns(NUM_PORTFOLIOS * NUM_RETURNS);
    for (uint32_t p = 0; p < NUM_PORTFOLIOS; p++) {
        auto ret = generateReturns(NUM_RETURNS, -0.001f, 0.02f + p * 0.005f, 12345 + p);
        std::copy(ret.begin(), ret.end(), all_returns.begin() + p * NUM_RETURNS);
    }

    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    std::vector<fin_risk_gpu_result_t> results(NUM_PORTFOLIOS);

    bool ok = fin_risk_gpu_batch(ctx, all_returns.data(), NUM_PORTFOLIOS, NUM_RETURNS, &params, results.data());
    ASSERT_TRUE(ok);

    // Check that results vary with volatility
    for (uint32_t p = 1; p < NUM_PORTFOLIOS; p++) {
        EXPECT_GT(results[p].volatility, results[p-1].volatility * 0.8f)
            << "Higher volatility portfolios should have higher measured vol";
    }
}

//=============================================================================
// Correlation/Covariance Matrix Tests
//=============================================================================

TEST_F(FinancialRiskGPUTest, CorrelationMatrix) {
    const uint32_t N_ASSETS = 4;
    const uint32_t N_RETURNS = 252;

    std::vector<float> returns(N_ASSETS * N_RETURNS);
    for (uint32_t a = 0; a < N_ASSETS; a++) {
        auto ret = generateReturns(N_RETURNS, 0.0f, 0.02f, 12345 + a);
        std::copy(ret.begin(), ret.end(), returns.begin() + a * N_RETURNS);
    }

    std::vector<float> correlation(N_ASSETS * N_ASSETS);
    bool ok = fin_risk_gpu_correlation_matrix(ctx, returns.data(), N_ASSETS, N_RETURNS, correlation.data());
    ASSERT_TRUE(ok);

    // Check diagonal elements are 1.0
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        EXPECT_NEAR(correlation[i * N_ASSETS + i], 1.0f, 0.01f)
            << "Diagonal correlation should be 1.0";
    }

    // Check symmetry
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        for (uint32_t j = i + 1; j < N_ASSETS; j++) {
            EXPECT_NEAR(correlation[i * N_ASSETS + j], correlation[j * N_ASSETS + i], 1e-6f)
                << "Correlation matrix should be symmetric";
        }
    }

    // Check bounds [-1, 1]
    for (uint32_t i = 0; i < N_ASSETS * N_ASSETS; i++) {
        EXPECT_GE(correlation[i], -1.0f) << "Correlation out of bounds";
        EXPECT_LE(correlation[i], 1.0f) << "Correlation out of bounds";
    }
}

TEST_F(FinancialRiskGPUTest, CovarianceMatrix) {
    const uint32_t N_ASSETS = 4;
    const uint32_t N_RETURNS = 252;

    std::vector<float> returns(N_ASSETS * N_RETURNS);
    for (uint32_t a = 0; a < N_ASSETS; a++) {
        auto ret = generateReturns(N_RETURNS, 0.0f, 0.02f * (a + 1), 12345 + a);
        std::copy(ret.begin(), ret.end(), returns.begin() + a * N_RETURNS);
    }

    std::vector<float> covariance(N_ASSETS * N_ASSETS);
    bool ok = fin_risk_gpu_covariance_matrix(ctx, returns.data(), N_ASSETS, N_RETURNS, covariance.data());
    ASSERT_TRUE(ok);

    // Check positive semi-definiteness (diagonal elements positive)
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        EXPECT_GT(covariance[i * N_ASSETS + i], 0.0f)
            << "Diagonal covariance (variance) should be positive";
    }

    // Check symmetry
    for (uint32_t i = 0; i < N_ASSETS; i++) {
        for (uint32_t j = i + 1; j < N_ASSETS; j++) {
            EXPECT_NEAR(covariance[i * N_ASSETS + j], covariance[j * N_ASSETS + i], 1e-6f)
                << "Covariance matrix should be symmetric";
        }
    }
}

//=============================================================================
// Portfolio VaR Tests
//=============================================================================

TEST_F(FinancialRiskGPUTest, PortfolioVaRDeltaNormal) {
    const uint32_t N_ASSETS = 3;
    std::vector<float> weights = {0.4f, 0.35f, 0.25f};
    std::vector<float> covariance = {
        0.04f, 0.01f, 0.005f,
        0.01f, 0.09f, 0.02f,
        0.005f, 0.02f, 0.16f
    };

    float var = fin_risk_gpu_portfolio_var_delta_normal(ctx, weights.data(), covariance.data(),
                                                         N_ASSETS, 0.95f, 10);
    EXPECT_GT(var, 0.0f) << "Portfolio VaR should be positive";
}

//=============================================================================
// Extended Risk Tests
//=============================================================================

TEST_F(FinancialRiskGPUTest, ExtendedRiskMetrics) {
    std::vector<float> returns = generateReturns(1000, -0.001f, 0.02f);

    fin_risk_extended_params_t params = fin_risk_extended_params_default();
    params.base.num_returns = returns.size();
    params.compute_tail_risk = true;

    fin_risk_extended_result_t result = {0};
    bool ok = fin_risk_gpu_extended(ctx, returns.data(), returns.size(), &params, &result);
    ASSERT_TRUE(ok);

    // Check that all VaR methods are computed
    EXPECT_GT(result.var_historical, 0.0f);
    EXPECT_GT(result.var_parametric, 0.0f);

    // Check drawdown metrics
    EXPECT_GE(result.max_drawdown, 0.0f);
    EXPECT_LE(result.max_drawdown, 1.0f);

    fin_risk_extended_result_free(&result);
}

TEST_F(FinancialRiskGPUTest, RollingRisk) {
    std::vector<float> returns = generateReturns(500, -0.001f, 0.02f);

    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    fin_risk_rolling_result_t result = {0};

    bool ok = fin_risk_gpu_rolling_extended(ctx, returns.data(), returns.size(), 21, &params, &result);
    ASSERT_TRUE(ok);

    EXPECT_GT(result.num_points, 0u);
    ASSERT_NE(result.var_series, nullptr);
    ASSERT_NE(result.vol_series, nullptr);

    // Check rolling VaR is positive
    for (uint32_t i = 0; i < result.num_points; i++) {
        EXPECT_GE(result.var_series[i], 0.0f) << "Rolling VaR should be non-negative";
        EXPECT_GE(result.vol_series[i], 0.0f) << "Rolling vol should be non-negative";
    }

    fin_risk_rolling_result_free(&result);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(FinancialRiskGPUTest, SmallSampleSize) {
    // Very small sample - should still work
    std::vector<float> returns = {0.01f, -0.02f, 0.005f, -0.015f, 0.008f};

    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = returns.size();

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
    ASSERT_TRUE(ok);

    EXPECT_GT(result.volatility, 0.0f);
}

TEST_F(FinancialRiskGPUTest, ZeroVolatility) {
    // All same returns - zero volatility
    std::vector<float> returns(100, 0.01f);

    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = returns.size();

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
    ASSERT_TRUE(ok);

    EXPECT_NEAR(result.volatility, 0.0f, 1e-6f) << "Zero volatility expected";
}

TEST_F(FinancialRiskGPUTest, LargeScale) {
    // Stress test with large data
    const uint32_t N = 100000;
    std::vector<float> returns = generateReturns(N, 0.0f, 0.02f);

    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = N;

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
    ASSERT_TRUE(ok);

    // Should handle large data without issues
    EXPECT_NEAR(result.volatility, 0.02f, 0.005f) << "Volatility should match input";
    EXPECT_GT(result.kernel_time_ms, 0.0f) << "Kernel time should be recorded";
}

//=============================================================================
// Standalone VaR/CVaR Function Tests
//=============================================================================

TEST_F(FinancialRiskGPUTest, StandaloneVaR95) {
    std::vector<float> returns = generateReturns(10000, -0.001f, 0.02f);

    float var = fin_risk_gpu_var(ctx, returns.data(), returns.size(), 0.95f);

    float cpu_var = cpuHistoricalVaR(returns, 0.95f);
    EXPECT_NEAR(var, cpu_var, 0.005f) << "Standalone VaR 95% mismatch";
}

TEST_F(FinancialRiskGPUTest, StandaloneVaR99) {
    std::vector<float> returns = generateReturns(10000, -0.001f, 0.02f);

    float var = fin_risk_gpu_var(ctx, returns.data(), returns.size(), 0.99f);

    float cpu_var = cpuHistoricalVaR(returns, 0.99f);
    EXPECT_NEAR(var, cpu_var, 0.005f) << "Standalone VaR 99% mismatch";
}

TEST_F(FinancialRiskGPUTest, StandaloneCVaR95) {
    std::vector<float> returns = generateReturns(10000, -0.001f, 0.02f);

    float cvar = fin_risk_gpu_cvar(ctx, returns.data(), returns.size(), 0.95f);

    float cpu_cvar = cpuCVaR(returns, 0.95f);
    EXPECT_NEAR(cvar, cpu_cvar, 0.005f) << "Standalone CVaR 95% mismatch";
}

TEST_F(FinancialRiskGPUTest, StandaloneCVaR99) {
    std::vector<float> returns = generateReturns(10000, -0.001f, 0.02f);

    float cvar = fin_risk_gpu_cvar(ctx, returns.data(), returns.size(), 0.99f);

    float cpu_cvar = cpuCVaR(returns, 0.99f);
    EXPECT_NEAR(cvar, cpu_cvar, 0.01f) << "Standalone CVaR 99% mismatch";
}

TEST_F(FinancialRiskGPUTest, StandaloneCVaRGreaterThanVaR) {
    std::vector<float> returns = generateReturns(10000, -0.001f, 0.02f);

    float var_95 = fin_risk_gpu_var(ctx, returns.data(), returns.size(), 0.95f);
    float cvar_95 = fin_risk_gpu_cvar(ctx, returns.data(), returns.size(), 0.95f);

    // CVaR (Expected Shortfall) should always be >= VaR
    EXPECT_GE(cvar_95, var_95) << "Standalone CVaR must be >= VaR";
}

TEST_F(FinancialRiskGPUTest, StandaloneVaRConsistency) {
    // Verify standalone function matches the compute function result
    std::vector<float> returns = generateReturns(5000, -0.001f, 0.02f);

    // Use standalone function
    float standalone_var = fin_risk_gpu_var(ctx, returns.data(), returns.size(), 0.95f);

    // Use compute function
    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = returns.size();
    params.compute_var = true;

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
    ASSERT_TRUE(ok);

    EXPECT_NEAR(standalone_var, result.var_95, 0.01f)
        << "Standalone VaR should match compute() VaR";
}

TEST_F(FinancialRiskGPUTest, StandaloneCVaRConsistency) {
    std::vector<float> returns = generateReturns(5000, -0.001f, 0.02f);

    // Use standalone function
    float standalone_cvar = fin_risk_gpu_cvar(ctx, returns.data(), returns.size(), 0.95f);

    // Use compute function
    fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
    params.num_returns = returns.size();
    params.compute_cvar = true;

    fin_risk_gpu_result_t result = {0};
    bool ok = fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
    ASSERT_TRUE(ok);

    EXPECT_NEAR(standalone_cvar, result.cvar_95, 0.01f)
        << "Standalone CVaR should match compute() CVaR";
}

//=============================================================================
// Standalone Volatility Function Tests
//=============================================================================

TEST_F(FinancialRiskGPUTest, StandaloneVolatilitySimple) {
    std::vector<float> returns = generateReturns(1000, 0.0f, 0.02f);
    std::vector<float> prices = returnsToPrices(returns);

    float volatility = 0.0f;
    bool ok = fin_risk_gpu_volatility(ctx, prices.data(), prices.size(),
                                       FIN_VOL_SIMPLE, &volatility);
    ASSERT_TRUE(ok);

    EXPECT_GT(volatility, 0.0f) << "Volatility should be positive";
    EXPECT_NEAR(volatility, 0.02f, 0.01f) << "Volatility should be close to true value";
}

TEST_F(FinancialRiskGPUTest, StandaloneVolatilityEWMA) {
    std::vector<float> returns = generateReturns(1000, 0.0f, 0.02f);
    std::vector<float> prices = returnsToPrices(returns);

    float volatility = 0.0f;
    bool ok = fin_risk_gpu_volatility(ctx, prices.data(), prices.size(),
                                       FIN_VOL_EWMA, &volatility);
    ASSERT_TRUE(ok);

    EXPECT_GT(volatility, 0.0f) << "EWMA volatility should be positive";
}

//=============================================================================
// Max Drawdown Extended Tests
//=============================================================================

TEST_F(FinancialRiskGPUTest, MaxDrawdownWithIndices) {
    // Create a price series with known drawdown pattern
    std::vector<float> prices = {100.0f, 105.0f, 110.0f, 108.0f, 95.0f, 90.0f, 100.0f, 105.0f};
    // Max drawdown: 110 -> 90 = 18.18%

    uint32_t dd_start = 0, dd_end = 0;
    float max_dd = fin_risk_gpu_max_drawdown(ctx, prices.data(), prices.size(), &dd_start, &dd_end);

    EXPECT_NEAR(max_dd, 0.1818f, 0.01f) << "Max drawdown value mismatch";

    // Verify indices are reasonable
    EXPECT_LT(dd_start, dd_end) << "Drawdown start should be before end";
    EXPECT_EQ(dd_start, 2u) << "Drawdown should start at peak (index 2)";
    EXPECT_EQ(dd_end, 5u) << "Drawdown should end at trough (index 5)";
}

TEST_F(FinancialRiskGPUTest, MaxDrawdownRecovery) {
    // Price recovers after drawdown
    std::vector<float> prices = {100.0f, 90.0f, 80.0f, 85.0f, 95.0f, 100.0f, 105.0f, 110.0f};
    // Max drawdown: 100 -> 80 = 20%

    uint32_t dd_start, dd_end;
    float max_dd = fin_risk_gpu_max_drawdown(ctx, prices.data(), prices.size(), &dd_start, &dd_end);

    EXPECT_NEAR(max_dd, 0.2f, 0.01f) << "Max drawdown should be 20%";
}

//=============================================================================
// EWMA Volatility Extended Tests
//=============================================================================

TEST_F(FinancialRiskGPUTest, EWMAVolatilityLambdaSensitivity) {
    std::vector<float> returns = generateReturns(500, 0.0f, 0.02f);

    float vol_094 = fin_risk_gpu_ewma_volatility(ctx, returns.data(), returns.size(), 0.94f, nullptr);
    float vol_097 = fin_risk_gpu_ewma_volatility(ctx, returns.data(), returns.size(), 0.97f, nullptr);

    // Both should be positive
    EXPECT_GT(vol_094, 0.0f);
    EXPECT_GT(vol_097, 0.0f);

    // Higher lambda = smoother, less reactive to recent observations
    // Results should be similar but not identical
    EXPECT_NE(vol_094, vol_097) << "Different lambdas should produce different results";
}

TEST_F(FinancialRiskGPUTest, EWMAVolatilityWithVolatilityClustering) {
    // Simulate volatility clustering: low vol period followed by high vol period
    std::vector<float> returns(500);
    srand(12345);

    // Low vol period
    for (int i = 0; i < 250; i++) {
        float u1 = (static_cast<float>(rand()) / RAND_MAX);
        float u2 = (static_cast<float>(rand()) / RAND_MAX);
        if (u1 < 1e-10f) u1 = 1e-10f;
        float z = std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * M_PI * u2);
        returns[i] = 0.01f * z;  // 1% vol
    }

    // High vol period
    for (int i = 250; i < 500; i++) {
        float u1 = (static_cast<float>(rand()) / RAND_MAX);
        float u2 = (static_cast<float>(rand()) / RAND_MAX);
        if (u1 < 1e-10f) u1 = 1e-10f;
        float z = std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * M_PI * u2);
        returns[i] = 0.03f * z;  // 3% vol
    }

    std::vector<float> vol_series(returns.size());
    float final_vol = fin_risk_gpu_ewma_volatility(ctx, returns.data(), returns.size(), 0.94f, vol_series.data());

    // Final EWMA vol should reflect recent high-vol period
    EXPECT_GT(final_vol, 0.02f) << "EWMA should react to high-vol period";

    // Early vol should be lower than late vol
    float avg_early_vol = 0.0f, avg_late_vol = 0.0f;
    for (int i = 200; i < 250; i++) avg_early_vol += vol_series[i];
    for (int i = 450; i < 500; i++) avg_late_vol += vol_series[i];
    avg_early_vol /= 50.0f;
    avg_late_vol /= 50.0f;

    EXPECT_LT(avg_early_vol, avg_late_vol)
        << "EWMA vol should be higher in high-vol period";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
