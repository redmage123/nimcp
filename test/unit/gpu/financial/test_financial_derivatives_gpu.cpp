//=============================================================================
// test_financial_derivatives_gpu.cpp - Unit Tests for GPU Derivatives Pricing
//=============================================================================
/**
 * @file test_financial_derivatives_gpu.cpp
 * @brief Unit tests for GPU-accelerated derivatives pricing
 *
 * Tests binomial tree pricing, Black-Scholes, Greeks computation,
 * implied volatility, option chains, and exotic options.
 *
 * COVERAGE:
 *   - Black-Scholes GPU computation
 *   - Binomial tree for European and American options
 *   - Greeks (Delta, Gamma, Theta, Vega, Rho)
 *   - Second-order Greeks (Vanna, Charm, Volga)
 *   - Implied volatility computation
 *   - Batch option pricing
 *   - Option chain pricing
 *   - American vs European option premium
 *   - Bermudan options
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "gpu/financial/nimcp_financial_gpu.h"
#include "gpu/financial/nimcp_financial_derivatives_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FinancialDerivativesGPUTest : public ::testing::Test {
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

    // CPU reference for Black-Scholes call
    float cpuBlackScholesCall(float S, float K, float r, float sigma, float T) {
        float d1 = (std::log(S / K) + (r + 0.5f * sigma * sigma) * T) / (sigma * std::sqrt(T));
        float d2 = d1 - sigma * std::sqrt(T);
        return S * normCDF(d1) - K * std::exp(-r * T) * normCDF(d2);
    }

    // CPU reference for Black-Scholes put
    float cpuBlackScholesPut(float S, float K, float r, float sigma, float T) {
        float d1 = (std::log(S / K) + (r + 0.5f * sigma * sigma) * T) / (sigma * std::sqrt(T));
        float d2 = d1 - sigma * std::sqrt(T);
        return K * std::exp(-r * T) * normCDF(-d2) - S * normCDF(-d1);
    }

    // CPU reference for Delta
    float cpuDelta(float S, float K, float r, float sigma, float T, bool is_call) {
        float d1 = (std::log(S / K) + (r + 0.5f * sigma * sigma) * T) / (sigma * std::sqrt(T));
        return is_call ? normCDF(d1) : normCDF(d1) - 1.0f;
    }

    // CPU reference for Gamma
    float cpuGamma(float S, float K, float r, float sigma, float T) {
        float d1 = (std::log(S / K) + (r + 0.5f * sigma * sigma) * T) / (sigma * std::sqrt(T));
        return normPDF(d1) / (S * sigma * std::sqrt(T));
    }

    // CPU reference for Vega
    float cpuVega(float S, float K, float r, float sigma, float T) {
        float d1 = (std::log(S / K) + (r + 0.5f * sigma * sigma) * T) / (sigma * std::sqrt(T));
        return S * std::sqrt(T) * normPDF(d1) / 100.0f;  // Per 1% change
    }

    // CPU reference for Theta (call)
    float cpuThetaCall(float S, float K, float r, float sigma, float T) {
        float d1 = (std::log(S / K) + (r + 0.5f * sigma * sigma) * T) / (sigma * std::sqrt(T));
        float d2 = d1 - sigma * std::sqrt(T);
        float term1 = -(S * normPDF(d1) * sigma) / (2.0f * std::sqrt(T));
        float term2 = r * K * std::exp(-r * T) * normCDF(d2);
        return (term1 - term2) / 365.0f;  // Daily theta
    }

private:
    float normCDF(float x) {
        return 0.5f * (1.0f + std::erf(x / std::sqrt(2.0f)));
    }

    float normPDF(float x) {
        return std::exp(-0.5f * x * x) / std::sqrt(2.0f * M_PI);
    }
};

//=============================================================================
// Black-Scholes Tests
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, BlackScholesCall) {
    float price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, 0.2f, 1.0f, FIN_OPT_CALL);

    float cpu_price = cpuBlackScholesCall(100.0f, 100.0f, 0.05f, 0.2f, 1.0f);
    EXPECT_NEAR(price, cpu_price, 0.01f) << "Black-Scholes call price mismatch";
}

TEST_F(FinancialDerivativesGPUTest, BlackScholesPut) {
    float price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.05f, 0.2f, 1.0f, FIN_OPT_PUT);

    float cpu_price = cpuBlackScholesPut(100.0f, 100.0f, 0.05f, 0.2f, 1.0f);
    EXPECT_NEAR(price, cpu_price, 0.01f) << "Black-Scholes put price mismatch";
}

TEST_F(FinancialDerivativesGPUTest, BlackScholesPutCallParity) {
    float S = 100.0f, K = 105.0f, r = 0.05f, sigma = 0.25f, T = 0.5f;

    float call = fin_deriv_gpu_black_scholes(ctx, S, K, r, sigma, T, FIN_OPT_CALL);
    float put = fin_deriv_gpu_black_scholes(ctx, S, K, r, sigma, T, FIN_OPT_PUT);

    // Put-Call Parity: C - P = S - K*exp(-rT)
    float parity_diff = call - put;
    float expected_diff = S - K * std::exp(-r * T);
    EXPECT_NEAR(parity_diff, expected_diff, 0.01f) << "Put-call parity violated";
}

TEST_F(FinancialDerivativesGPUTest, BlackScholesBatch) {
    const uint32_t N = 100;
    std::vector<float> spots(N), strikes(N), rates(N), vols(N), times(N);
    std::vector<fin_option_type_t> types(N);
    std::vector<float> prices(N);

    for (uint32_t i = 0; i < N; i++) {
        spots[i] = 100.0f;
        strikes[i] = 90.0f + 0.2f * i;  // 90 to 110
        rates[i] = 0.05f;
        vols[i] = 0.2f;
        times[i] = 1.0f;
        types[i] = FIN_OPT_CALL;
    }

    bool ok = fin_deriv_gpu_black_scholes_batch(ctx, spots.data(), strikes.data(), rates.data(),
                                                  vols.data(), times.data(), types.data(), N, prices.data());
    ASSERT_TRUE(ok);

    // Verify a few prices
    for (uint32_t i = 0; i < N; i += 10) {
        float cpu_price = cpuBlackScholesCall(spots[i], strikes[i], rates[i], vols[i], times[i]);
        EXPECT_NEAR(prices[i], cpu_price, 0.05f) << "Batch BS price mismatch at " << i;
    }

    // Higher strike -> lower call price
    for (uint32_t i = 1; i < N; i++) {
        EXPECT_LE(prices[i], prices[i-1] + 0.01f) << "Call price should decrease with strike";
    }
}

//=============================================================================
// Binomial Tree Tests
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, BinomialTreeEuropeanCall) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.option_style = FIN_OPT_STYLE_EUROPEAN;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;
    params.tree_steps = 1000;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    float cpu_price = cpuBlackScholesCall(100.0f, 100.0f, 0.05f, 0.2f, 1.0f);
    EXPECT_NEAR(result.price, cpu_price, 0.1f) << "Binomial tree should converge to BS";
}

TEST_F(FinancialDerivativesGPUTest, BinomialTreeEuropeanPut) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_PUT;
    params.option_style = FIN_OPT_STYLE_EUROPEAN;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;
    params.tree_steps = 1000;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    float cpu_price = cpuBlackScholesPut(100.0f, 100.0f, 0.05f, 0.2f, 1.0f);
    EXPECT_NEAR(result.price, cpu_price, 0.1f) << "Binomial tree put should converge to BS";
}

TEST_F(FinancialDerivativesGPUTest, BinomialTreeAmericanPut) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_PUT;
    params.option_style = FIN_OPT_STYLE_AMERICAN;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;
    params.tree_steps = 1000;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    // American put should be >= European put
    float eu_price = cpuBlackScholesPut(100.0f, 100.0f, 0.05f, 0.2f, 1.0f);
    EXPECT_GE(result.price, eu_price - 0.01f) << "American put should be >= European put";

    // Should have early exercise premium
    EXPECT_GE(result.early_exercise_premium, 0.0f) << "Early exercise premium should be non-negative";
}

TEST_F(FinancialDerivativesGPUTest, BinomialTreeAmericanCall) {
    // American call without dividends should equal European call
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.option_style = FIN_OPT_STYLE_AMERICAN;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;
    params.dividend_yield = 0.0f;
    params.tree_steps = 1000;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    float eu_price = cpuBlackScholesCall(100.0f, 100.0f, 0.05f, 0.2f, 1.0f);
    EXPECT_NEAR(result.price, eu_price, 0.15f) << "American call (no div) ≈ European call";
}

TEST_F(FinancialDerivativesGPUTest, BinomialTreeConvergence) {
    // Test that more steps gives more accurate results
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.option_style = FIN_OPT_STYLE_EUROPEAN;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;

    float cpu_price = cpuBlackScholesCall(100.0f, 100.0f, 0.05f, 0.2f, 1.0f);

    float prev_error = 1000.0f;
    for (uint32_t steps : {100, 500, 1000, 2000}) {
        params.tree_steps = steps;
        fin_derivatives_gpu_result_t result = {0};
        fin_derivatives_gpu_binomial_tree(ctx, &params, &result);

        float error = std::abs(result.price - cpu_price);
        EXPECT_LT(error, prev_error + 0.01f) << "Error should decrease with more steps";
        prev_error = error;
    }
}

//=============================================================================
// Greeks Tests
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, GreeksDelta) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;
    params.compute_greeks = true;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    float cpu_delta = cpuDelta(100.0f, 100.0f, 0.05f, 0.2f, 1.0f, true);
    EXPECT_NEAR(result.delta, cpu_delta, 0.02f) << "Delta mismatch";

    // ATM call delta should be around 0.5-0.6
    EXPECT_GT(result.delta, 0.4f);
    EXPECT_LT(result.delta, 0.7f);
}

TEST_F(FinancialDerivativesGPUTest, GreeksGamma) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;
    params.compute_greeks = true;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    float cpu_gamma = cpuGamma(100.0f, 100.0f, 0.05f, 0.2f, 1.0f);
    EXPECT_NEAR(result.gamma, cpu_gamma, 0.005f) << "Gamma mismatch";

    // Gamma should be positive
    EXPECT_GT(result.gamma, 0.0f);
}

TEST_F(FinancialDerivativesGPUTest, GreeksVega) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;
    params.compute_greeks = true;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    // Vega should be positive
    EXPECT_GT(result.vega, 0.0f);
}

TEST_F(FinancialDerivativesGPUTest, GreeksTheta) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;
    params.compute_greeks = true;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    // Theta for long options should be negative (time decay)
    EXPECT_LT(result.theta, 0.0f);
}

TEST_F(FinancialDerivativesGPUTest, GreeksRho) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;
    params.compute_greeks = true;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    // Rho for call should be positive (higher rates increase call value)
    EXPECT_GT(result.rho, 0.0f);
}

TEST_F(FinancialDerivativesGPUTest, GreeksPutDelta) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_PUT;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;
    params.compute_greeks = true;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    float cpu_delta = cpuDelta(100.0f, 100.0f, 0.05f, 0.2f, 1.0f, false);
    EXPECT_NEAR(result.delta, cpu_delta, 0.02f) << "Put delta mismatch";

    // Put delta should be negative
    EXPECT_LT(result.delta, 0.0f);
    EXPECT_GT(result.delta, -1.0f);
}

TEST_F(FinancialDerivativesGPUTest, SingleGreekComputation) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;

    float delta = fin_deriv_gpu_greek(ctx, &params, FIN_GREEK_DELTA, 0.01f);
    float gamma = fin_deriv_gpu_greek(ctx, &params, FIN_GREEK_GAMMA, 0.01f);
    float vega = fin_deriv_gpu_greek(ctx, &params, FIN_GREEK_VEGA, 0.01f);

    EXPECT_GT(delta, 0.0f) << "Call delta should be positive";
    EXPECT_GT(gamma, 0.0f) << "Gamma should be positive";
    EXPECT_GT(vega, 0.0f) << "Vega should be positive";
}

//=============================================================================
// Extended Greeks Tests
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, ExtendedGreeks) {
    fin_deriv_extended_params_t params = fin_deriv_extended_params_default();
    params.greek_flags = FIN_GREEK_ALL;

    fin_deriv_extended_result_t result = {0};
    bool ok = fin_deriv_gpu_price_extended(ctx, &params, &result);
    ASSERT_TRUE(ok);

    // Vanna: d(Delta)/d(Vol) - can be positive or negative
    // Charm: d(Delta)/d(Time) - typically negative for calls
    // Volga: d^2(Price)/d(Vol)^2 - typically positive

    EXPECT_NE(result.vanna, 0.0f) << "Vanna should be non-zero for ATM";

    fin_deriv_extended_result_free(&result);
}

//=============================================================================
// Implied Volatility Tests
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, ImpliedVolatility) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.time_to_expiry = 1.0f;

    // Known price from vol = 0.2
    float true_vol = 0.2f;
    float market_price = cpuBlackScholesCall(100.0f, 100.0f, 0.05f, true_vol, 1.0f);

    float iv = fin_deriv_gpu_implied_vol(ctx, market_price, &params);
    EXPECT_NEAR(iv, true_vol, 0.005f) << "Implied vol should recover true vol";
}

TEST_F(FinancialDerivativesGPUTest, ImpliedVolatilityBatch) {
    const uint32_t N = 10;
    std::vector<float> strikes(N);
    std::vector<float> expiries(N);
    std::vector<fin_option_type_t> types(N);
    std::vector<fin_option_style_t> styles(N);
    std::vector<float> market_prices(N);
    std::vector<float> implied_vols(N);

    float true_vol = 0.2f;
    for (uint32_t i = 0; i < N; i++) {
        strikes[i] = 95.0f + i;
        expiries[i] = 1.0f;
        types[i] = FIN_OPT_CALL;
        styles[i] = FIN_OPT_STYLE_EUROPEAN;
        market_prices[i] = cpuBlackScholesCall(100.0f, strikes[i], 0.05f, true_vol, 1.0f);
    }

    fin_option_chain_t chain = {
        .strikes = strikes.data(),
        .expiries = expiries.data(),
        .types = types.data(),
        .styles = styles.data(),
        .num_options = N,
        .spot = 100.0f,
        .rate = 0.05f,
        .dividend = 0.0f
    };

    bool ok = fin_deriv_gpu_implied_vol_batch(ctx, market_prices.data(), &chain, implied_vols.data());
    ASSERT_TRUE(ok);

    for (uint32_t i = 0; i < N; i++) {
        EXPECT_NEAR(implied_vols[i], true_vol, 0.01f) << "Batch IV mismatch at " << i;
    }
}

//=============================================================================
// Option Chain Tests
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, OptionChainPricing) {
    const uint32_t N = 20;
    std::vector<float> strikes(N);
    std::vector<float> expiries(N);
    std::vector<fin_option_type_t> types(N);
    std::vector<fin_option_style_t> styles(N);

    for (uint32_t i = 0; i < N; i++) {
        strikes[i] = 90.0f + i;
        expiries[i] = 0.5f;
        types[i] = (i % 2 == 0) ? FIN_OPT_CALL : FIN_OPT_PUT;
        styles[i] = FIN_OPT_STYLE_EUROPEAN;
    }

    fin_option_chain_t chain = {
        .strikes = strikes.data(),
        .expiries = expiries.data(),
        .types = types.data(),
        .styles = styles.data(),
        .num_options = N,
        .spot = 100.0f,
        .rate = 0.05f,
        .dividend = 0.0f
    };

    fin_option_chain_result_t result = {0};
    bool ok = fin_deriv_gpu_price_chain(ctx, &chain, 0.2f, &result);
    ASSERT_TRUE(ok);

    ASSERT_NE(result.prices, nullptr);
    ASSERT_EQ(result.num_options, N);

    // All prices should be positive
    for (uint32_t i = 0; i < N; i++) {
        EXPECT_GT(result.prices[i], 0.0f) << "Option price should be positive";
    }

    fin_option_chain_result_free(&result);
}

TEST_F(FinancialDerivativesGPUTest, OptionChainGreeks) {
    const uint32_t N = 10;
    std::vector<float> strikes(N);
    std::vector<float> expiries(N);
    std::vector<fin_option_type_t> types(N);
    std::vector<fin_option_style_t> styles(N);

    for (uint32_t i = 0; i < N; i++) {
        strikes[i] = 95.0f + i;
        expiries[i] = 1.0f;
        types[i] = FIN_OPT_CALL;
        styles[i] = FIN_OPT_STYLE_EUROPEAN;
    }

    fin_option_chain_t chain = {
        .strikes = strikes.data(),
        .expiries = expiries.data(),
        .types = types.data(),
        .styles = styles.data(),
        .num_options = N,
        .spot = 100.0f,
        .rate = 0.05f,
        .dividend = 0.0f
    };

    fin_option_chain_result_t result = {0};
    uint32_t greek_flags = FIN_GREEK_DELTA | FIN_GREEK_GAMMA | FIN_GREEK_VEGA;
    bool ok = fin_deriv_gpu_greeks_chain(ctx, &chain, 0.2f, greek_flags, &result);
    ASSERT_TRUE(ok);

    ASSERT_NE(result.deltas, nullptr);
    ASSERT_NE(result.gammas, nullptr);
    ASSERT_NE(result.vegas, nullptr);

    // Delta should decrease as strike increases (for calls)
    for (uint32_t i = 1; i < N; i++) {
        EXPECT_LE(result.deltas[i], result.deltas[i-1] + 0.01f)
            << "Call delta should decrease with strike";
    }

    fin_option_chain_result_free(&result);
}

//=============================================================================
// American Option Tests
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, AmericanWithExerciseBoundary) {
    fin_deriv_extended_params_t params = fin_deriv_extended_params_default();
    params.base.option_type = FIN_OPT_PUT;
    params.base.option_style = FIN_OPT_STYLE_AMERICAN;
    params.base.spot = 100.0f;
    params.base.strike = 100.0f;
    params.base.tree_steps = 500;

    fin_deriv_extended_result_t result = {0};
    bool ok = fin_deriv_gpu_american_with_boundary(ctx, &params, &result);
    ASSERT_TRUE(ok);

    // Should have exercise boundary
    ASSERT_NE(result.exercise_boundary, nullptr);

    // Exercise boundary should be monotonically increasing for put
    for (uint32_t i = 1; i < params.base.tree_steps; i++) {
        EXPECT_GE(result.exercise_boundary[i], result.exercise_boundary[i-1] - 0.01f)
            << "Put exercise boundary should be non-decreasing";
    }

    fin_deriv_extended_result_free(&result);
}

TEST_F(FinancialDerivativesGPUTest, AmericanEarlyExercisePremium) {
    // Deep ITM put should have significant early exercise premium
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_PUT;
    params.option_style = FIN_OPT_STYLE_AMERICAN;
    params.spot = 80.0f;  // Deep ITM
    params.strike = 100.0f;
    params.risk_free_rate = 0.1f;  // Higher rate increases premium
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;
    params.tree_steps = 1000;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    EXPECT_GT(result.early_exercise_premium, 0.5f)
        << "Deep ITM put should have significant early exercise premium";
}

//=============================================================================
// Bermudan Option Tests
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, BermudanOption) {
    fin_deriv_extended_params_t params = fin_deriv_extended_params_default();
    params.base.option_type = FIN_OPT_PUT;
    params.base.spot = 100.0f;
    params.base.strike = 100.0f;
    params.base.time_to_expiry = 1.0f;
    params.base.tree_steps = 252;

    // Exercise dates: quarterly (every 63 days)
    std::vector<uint32_t> exercise_dates = {63, 126, 189, 252};
    params.exercise_dates = exercise_dates.data();
    params.num_exercise_dates = exercise_dates.size();

    fin_deriv_extended_result_t result = {0};
    bool ok = fin_deriv_gpu_bermudan(ctx, &params, &result);
    ASSERT_TRUE(ok);

    // Bermudan price should be between European and American
    float eu_price = cpuBlackScholesPut(100.0f, 100.0f, 0.02f, 0.2f, 1.0f);

    params.base.option_style = FIN_OPT_STYLE_AMERICAN;
    fin_derivatives_gpu_result_t am_result = {0};
    fin_derivatives_gpu_binomial_tree(ctx, &params.base, &am_result);

    EXPECT_GE(result.base.price, eu_price - 0.1f) << "Bermudan >= European";
    EXPECT_LE(result.base.price, am_result.price + 0.1f) << "Bermudan <= American";

    fin_deriv_extended_result_free(&result);
}

// NOTE: Monte Carlo option pricing and batch pricing tests removed
// The functions fin_derivatives_gpu_monte_carlo and fin_derivatives_gpu_batch_price
// are not yet implemented in the production code

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, ShortExpiry) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.time_to_expiry = 1.0f / 365.0f;  // 1 day
    params.tree_steps = 100;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    // Short expiry ATM should have small value
    EXPECT_LT(result.price, 2.0f) << "1-day ATM option should have small value";
    EXPECT_GT(result.price, 0.0f) << "Price should be positive";
}

TEST_F(FinancialDerivativesGPUTest, DeepITMCall) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.spot = 150.0f;
    params.strike = 100.0f;
    params.time_to_expiry = 1.0f;
    params.tree_steps = 500;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    // Deep ITM call should have delta close to 1
    EXPECT_GT(result.delta, 0.9f) << "Deep ITM call delta should be near 1";

    // Price should be close to intrinsic value
    float intrinsic = 150.0f - 100.0f;
    EXPECT_GT(result.price, intrinsic) << "Price should exceed intrinsic value";
}

TEST_F(FinancialDerivativesGPUTest, DeepOTMPut) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_PUT;
    params.spot = 150.0f;
    params.strike = 100.0f;
    params.time_to_expiry = 0.25f;
    params.tree_steps = 500;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    // Deep OTM put should have near-zero value
    EXPECT_LT(result.price, 1.0f) << "Deep OTM put should have small value";
    EXPECT_GT(result.price, 0.0f) << "Price should be positive";
}

TEST_F(FinancialDerivativesGPUTest, HighVolatility) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.volatility = 1.0f;  // 100% vol
    params.time_to_expiry = 1.0f;
    params.tree_steps = 1000;

    fin_derivatives_gpu_result_t result = {0};
    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);
    ASSERT_TRUE(ok);

    // High vol should give higher option price
    float low_vol_price = cpuBlackScholesCall(100.0f, 100.0f, 0.02f, 0.2f, 1.0f);
    EXPECT_GT(result.price, low_vol_price * 2.0f) << "High vol should give much higher price";
}

TEST_F(FinancialDerivativesGPUTest, CumNormCDF) {
    // Test the CDF helper function
    float n0 = fin_deriv_gpu_norm_cdf(0.0f);
    EXPECT_NEAR(n0, 0.5f, 0.001f) << "N(0) should be 0.5";

    float n_neg = fin_deriv_gpu_norm_cdf(-2.0f);
    EXPECT_LT(n_neg, 0.05f) << "N(-2) should be small";

    float n_pos = fin_deriv_gpu_norm_cdf(2.0f);
    EXPECT_GT(n_pos, 0.95f) << "N(2) should be large";

    // Symmetry
    EXPECT_NEAR(n_neg + n_pos, 1.0f, 0.001f) << "N(-x) + N(x) = 1";
}

//=============================================================================
// Implied Volatility Surface Tests
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, ImpliedVolSurface) {
    const uint32_t N = 9;  // 3x3 grid
    std::vector<float> strikes = {95, 100, 105, 95, 100, 105, 95, 100, 105};
    std::vector<float> expiries = {0.25, 0.25, 0.25, 0.5, 0.5, 0.5, 1.0, 1.0, 1.0};
    std::vector<fin_option_type_t> types(N, FIN_OPT_CALL);
    std::vector<fin_option_style_t> styles(N, FIN_OPT_STYLE_EUROPEAN);

    // Generate market prices with vol smile
    std::vector<float> market_prices(N);
    std::vector<float> true_vols = {0.22, 0.20, 0.22, 0.21, 0.20, 0.21, 0.20, 0.20, 0.20};
    for (uint32_t i = 0; i < N; i++) {
        market_prices[i] = cpuBlackScholesCall(100.0f, strikes[i], 0.05f, true_vols[i], expiries[i]);
    }

    fin_option_chain_t chain = {
        .strikes = strikes.data(),
        .expiries = expiries.data(),
        .types = types.data(),
        .styles = styles.data(),
        .num_options = N,
        .spot = 100.0f,
        .rate = 0.05f,
        .dividend = 0.0f
    };

    std::vector<float> implied_vols(N);
    bool ok = fin_deriv_gpu_implied_vol_surface(ctx, &chain, market_prices.data(), implied_vols.data());
    ASSERT_TRUE(ok);

    for (uint32_t i = 0; i < N; i++) {
        EXPECT_NEAR(implied_vols[i], true_vols[i], 0.01f)
            << "IV surface mismatch at " << i;
    }
}

//=============================================================================
// Result Cleanup Tests
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, ExtendedResultCleanup) {
    fin_deriv_extended_params_t params = fin_deriv_extended_params_default();
    params.greek_flags = FIN_GREEK_ALL;
    params.base.option_style = FIN_OPT_STYLE_AMERICAN;
    params.base.option_type = FIN_OPT_PUT;
    params.base.tree_steps = 100;

    fin_deriv_extended_result_t result = {0};
    bool ok = fin_deriv_gpu_american_with_boundary(ctx, &params, &result);
    ASSERT_TRUE(ok);

    // Verify result has allocated data
    EXPECT_NE(result.exercise_boundary, nullptr);

    // Cleanup should not crash
    fin_deriv_extended_result_free(&result);

    // After cleanup, should be safe (no double-free)
    EXPECT_EQ(result.exercise_boundary, nullptr);
}

TEST_F(FinancialDerivativesGPUTest, OptionChainResultCleanup) {
    const uint32_t N = 5;
    std::vector<float> strikes = {95.0f, 97.5f, 100.0f, 102.5f, 105.0f};
    std::vector<float> expiries(N, 0.5f);
    std::vector<fin_option_type_t> types(N, FIN_OPT_CALL);
    std::vector<fin_option_style_t> styles(N, FIN_OPT_STYLE_EUROPEAN);

    fin_option_chain_t chain = {
        .strikes = strikes.data(),
        .expiries = expiries.data(),
        .types = types.data(),
        .styles = styles.data(),
        .num_options = N,
        .spot = 100.0f,
        .rate = 0.05f,
        .dividend = 0.0f
    };

    fin_option_chain_result_t result = {0};
    bool ok = fin_deriv_gpu_price_chain(ctx, &chain, 0.2f, &result);
    ASSERT_TRUE(ok);

    EXPECT_NE(result.prices, nullptr);
    EXPECT_EQ(result.num_options, N);

    // Cleanup should work correctly
    fin_option_chain_result_free(&result);

    // After cleanup
    EXPECT_EQ(result.prices, nullptr);
}

//=============================================================================
// Additional Black-Scholes Tests
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, BlackScholesDeepITM) {
    // Deep ITM call
    float call_price = fin_deriv_gpu_black_scholes(ctx, 150.0f, 100.0f, 0.05f, 0.2f, 1.0f, FIN_OPT_CALL);

    // Deep ITM call should be close to intrinsic value discounted
    float intrinsic = 150.0f - 100.0f * std::exp(-0.05f * 1.0f);
    EXPECT_GT(call_price, intrinsic - 1.0f) << "Deep ITM call should exceed PV of intrinsic";
}

TEST_F(FinancialDerivativesGPUTest, BlackScholesDeepOTM) {
    // Deep OTM call
    float call_price = fin_deriv_gpu_black_scholes(ctx, 50.0f, 100.0f, 0.05f, 0.2f, 1.0f, FIN_OPT_CALL);

    // Deep OTM call should have small value
    EXPECT_LT(call_price, 1.0f) << "Deep OTM call should have small value";
    EXPECT_GT(call_price, 0.0f) << "Price should still be positive";
}

TEST_F(FinancialDerivativesGPUTest, BlackScholesZeroRate) {
    float call_price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.0f, 0.2f, 1.0f, FIN_OPT_CALL);
    float put_price = fin_deriv_gpu_black_scholes(ctx, 100.0f, 100.0f, 0.0f, 0.2f, 1.0f, FIN_OPT_PUT);

    // With zero rate, ATM call and put should be equal (no drift)
    EXPECT_NEAR(call_price, put_price, 0.1f) << "ATM call=put when r=0";
}

//=============================================================================
// Additional Greeks Tests
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, AllGreeksComputation) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;

    // Test all Greeks individually
    float delta = fin_deriv_gpu_greek(ctx, &params, FIN_GREEK_DELTA, 0.01f);
    float gamma = fin_deriv_gpu_greek(ctx, &params, FIN_GREEK_GAMMA, 0.01f);
    float theta = fin_deriv_gpu_greek(ctx, &params, FIN_GREEK_THETA, 1.0f/252.0f);
    float vega = fin_deriv_gpu_greek(ctx, &params, FIN_GREEK_VEGA, 0.01f);
    float rho = fin_deriv_gpu_greek(ctx, &params, FIN_GREEK_RHO, 0.0001f);

    // ATM call Greeks should have expected signs
    EXPECT_GT(delta, 0.4f) << "ATM call delta should be ~0.5-0.6";
    EXPECT_LT(delta, 0.7f);
    EXPECT_GT(gamma, 0.0f) << "Gamma should be positive";
    EXPECT_LT(theta, 0.0f) << "Theta should be negative (time decay)";
    EXPECT_GT(vega, 0.0f) << "Vega should be positive";
    EXPECT_GT(rho, 0.0f) << "Call rho should be positive";
}

TEST_F(FinancialDerivativesGPUTest, SecondOrderGreeks) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;

    // Test second-order Greeks
    float vanna = fin_deriv_gpu_greek(ctx, &params, FIN_GREEK_VANNA, 0.01f);
    float charm = fin_deriv_gpu_greek(ctx, &params, FIN_GREEK_CHARM, 1.0f/252.0f);
    float volga = fin_deriv_gpu_greek(ctx, &params, FIN_GREEK_VOLGA, 0.01f);

    // Just verify they compute without error and are finite
    EXPECT_FALSE(std::isnan(vanna)) << "Vanna should be finite";
    EXPECT_FALSE(std::isnan(charm)) << "Charm should be finite";
    EXPECT_FALSE(std::isnan(volga)) << "Volga should be finite";
}

//=============================================================================
// Implied Volatility Edge Cases
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, ImpliedVolatilityOTM) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.spot = 100.0f;
    params.strike = 120.0f;  // OTM
    params.risk_free_rate = 0.05f;
    params.time_to_expiry = 0.5f;

    // Get price with known vol
    float true_vol = 0.3f;
    float market_price = cpuBlackScholesCall(100.0f, 120.0f, 0.05f, true_vol, 0.5f);

    float iv = fin_deriv_gpu_implied_vol(ctx, market_price, &params);
    EXPECT_NEAR(iv, true_vol, 0.01f) << "OTM implied vol should recover true vol";
}

TEST_F(FinancialDerivativesGPUTest, ImpliedVolatilityITM) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.spot = 100.0f;
    params.strike = 80.0f;  // ITM
    params.risk_free_rate = 0.05f;
    params.time_to_expiry = 0.5f;

    // Get price with known vol
    float true_vol = 0.25f;
    float market_price = cpuBlackScholesCall(100.0f, 80.0f, 0.05f, true_vol, 0.5f);

    float iv = fin_deriv_gpu_implied_vol(ctx, market_price, &params);
    EXPECT_NEAR(iv, true_vol, 0.01f) << "ITM implied vol should recover true vol";
}

TEST_F(FinancialDerivativesGPUTest, ImpliedVolatilityPut) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_PUT;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.time_to_expiry = 1.0f;

    float true_vol = 0.2f;
    float market_price = cpuBlackScholesPut(100.0f, 100.0f, 0.05f, true_vol, 1.0f);

    float iv = fin_deriv_gpu_implied_vol(ctx, market_price, &params);
    EXPECT_NEAR(iv, true_vol, 0.01f) << "Put implied vol should recover true vol";
}

//=============================================================================
// Cumulative Normal CDF Extended Tests
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, NormCDFExtended) {
    // Test various values
    EXPECT_NEAR(fin_deriv_gpu_norm_cdf(-3.0f), 0.00135f, 0.001f);
    EXPECT_NEAR(fin_deriv_gpu_norm_cdf(-1.0f), 0.15866f, 0.001f);
    EXPECT_NEAR(fin_deriv_gpu_norm_cdf(0.0f), 0.5f, 0.001f);
    EXPECT_NEAR(fin_deriv_gpu_norm_cdf(1.0f), 0.84134f, 0.001f);
    EXPECT_NEAR(fin_deriv_gpu_norm_cdf(3.0f), 0.99865f, 0.001f);
}

//=============================================================================
// Tree Convergence Detailed Tests
//=============================================================================

TEST_F(FinancialDerivativesGPUTest, BinomialTreeDividendYield) {
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_CALL;
    params.option_style = FIN_OPT_STYLE_EUROPEAN;
    params.spot = 100.0f;
    params.strike = 100.0f;
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;
    params.tree_steps = 1000;

    // Without dividend
    params.dividend_yield = 0.0f;
    fin_derivatives_gpu_result_t result_no_div = {0};
    fin_derivatives_gpu_binomial_tree(ctx, &params, &result_no_div);

    // With dividend
    params.dividend_yield = 0.03f;
    fin_derivatives_gpu_result_t result_with_div = {0};
    fin_derivatives_gpu_binomial_tree(ctx, &params, &result_with_div);

    // Dividend should reduce call value
    EXPECT_LT(result_with_div.price, result_no_div.price)
        << "Call with dividend should be worth less";
}

TEST_F(FinancialDerivativesGPUTest, AmericanVsEuropeanPremium) {
    // For put options, American premium should be positive
    fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
    params.option_type = FIN_OPT_PUT;
    params.spot = 100.0f;
    params.strike = 110.0f;  // ITM put
    params.risk_free_rate = 0.05f;
    params.volatility = 0.2f;
    params.time_to_expiry = 1.0f;
    params.tree_steps = 1000;

    // European
    params.option_style = FIN_OPT_STYLE_EUROPEAN;
    fin_derivatives_gpu_result_t euro_result = {0};
    fin_derivatives_gpu_binomial_tree(ctx, &params, &euro_result);

    // American
    params.option_style = FIN_OPT_STYLE_AMERICAN;
    fin_derivatives_gpu_result_t amer_result = {0};
    fin_derivatives_gpu_binomial_tree(ctx, &params, &amer_result);

    EXPECT_GE(amer_result.price, euro_result.price)
        << "American option should be worth at least as much as European";
    EXPECT_GT(amer_result.early_exercise_premium, 0.0f)
        << "ITM put should have positive early exercise premium";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
