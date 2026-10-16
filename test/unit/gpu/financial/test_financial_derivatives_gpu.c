/* ============================================================================
 * Financial GPU Derivatives Unit Tests
 * ============================================================================
 * WHAT: Unit tests for GPU derivatives pricing kernels
 * WHY:  Validate binomial trees, Black-Scholes, Greeks, implied volatility
 * HOW:  Test GPU kernels against known analytical solutions
 * ============================================================================
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

/* Include GPU financial headers if available */
#ifdef NIMCP_CUDA_ENABLED
#include "gpu/financial/nimcp_financial_derivatives_gpu.h"
#include "gpu/nimcp_gpu_context.h"
#endif

/* ============================================================================
 * Test Constants
 * ============================================================================ */
#define TOLERANCE 1e-4
#define PRICE_TOLERANCE 0.01  /* $0.01 for option prices */
#define GREEK_TOLERANCE 1e-3

/* Black-Scholes analytical formula for validation */
static double norm_cdf(double x) {
    return 0.5 * (1.0 + erf(x / sqrt(2.0)));
}

static double black_scholes_call(double S, double K, double r, double sigma, double T) {
    double d1 = (log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrt(T));
    double d2 = d1 - sigma * sqrt(T);
    return S * norm_cdf(d1) - K * exp(-r * T) * norm_cdf(d2);
}

static double black_scholes_put(double S, double K, double r, double sigma, double T) {
    double d1 = (log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrt(T));
    double d2 = d1 - sigma * sqrt(T);
    return K * exp(-r * T) * norm_cdf(-d2) - S * norm_cdf(-d1);
}

/* ============================================================================
 * European Call Option Tests
 * ============================================================================ */

START_TEST(test_binomial_european_call_atm)
{
#ifdef NIMCP_CUDA_ENABLED
    /* ATM European call option */
    fin_derivatives_gpu_params_t params = {
        .spot_price = 100.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = true,
        .is_american = false
    };

    fin_derivatives_gpu_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        /* No GPU available - skip test */
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);

    if (success) {
        /* Compare with Black-Scholes analytical price */
        double bs_price = black_scholes_call(100.0, 100.0, 0.05, 0.2, 1.0);
        ck_assert_msg(fabs(result.price - bs_price) < PRICE_TOLERANCE,
            "ATM call price mismatch: got %.4f, expected %.4f",
            result.price, bs_price);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

START_TEST(test_binomial_european_call_itm)
{
#ifdef NIMCP_CUDA_ENABLED
    /* ITM European call option (S=110, K=100) */
    fin_derivatives_gpu_params_t params = {
        .spot_price = 110.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = true,
        .is_american = false
    };

    fin_derivatives_gpu_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);

    if (success) {
        double bs_price = black_scholes_call(110.0, 100.0, 0.05, 0.2, 1.0);
        ck_assert_msg(fabs(result.price - bs_price) < PRICE_TOLERANCE,
            "ITM call price mismatch: got %.4f, expected %.4f",
            result.price, bs_price);
        /* ITM option should have positive intrinsic value */
        ck_assert_msg(result.price > 10.0, "ITM call should be worth > intrinsic");
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

START_TEST(test_binomial_european_call_otm)
{
#ifdef NIMCP_CUDA_ENABLED
    /* OTM European call option (S=90, K=100) */
    fin_derivatives_gpu_params_t params = {
        .spot_price = 90.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = true,
        .is_american = false
    };

    fin_derivatives_gpu_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);

    if (success) {
        double bs_price = black_scholes_call(90.0, 100.0, 0.05, 0.2, 1.0);
        ck_assert_msg(fabs(result.price - bs_price) < PRICE_TOLERANCE,
            "OTM call price mismatch: got %.4f, expected %.4f",
            result.price, bs_price);
        /* OTM option should have only time value */
        ck_assert_msg(result.price > 0.0 && result.price < 10.0,
            "OTM call should have positive time value but no intrinsic");
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

/* ============================================================================
 * European Put Option Tests
 * ============================================================================ */

START_TEST(test_binomial_european_put_atm)
{
#ifdef NIMCP_CUDA_ENABLED
    /* ATM European put option */
    fin_derivatives_gpu_params_t params = {
        .spot_price = 100.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = false,
        .is_american = false
    };

    fin_derivatives_gpu_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);

    if (success) {
        double bs_price = black_scholes_put(100.0, 100.0, 0.05, 0.2, 1.0);
        ck_assert_msg(fabs(result.price - bs_price) < PRICE_TOLERANCE,
            "ATM put price mismatch: got %.4f, expected %.4f",
            result.price, bs_price);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

START_TEST(test_binomial_european_put_itm)
{
#ifdef NIMCP_CUDA_ENABLED
    /* ITM European put option (S=90, K=100) */
    fin_derivatives_gpu_params_t params = {
        .spot_price = 90.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = false,
        .is_american = false
    };

    fin_derivatives_gpu_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);

    if (success) {
        double bs_price = black_scholes_put(90.0, 100.0, 0.05, 0.2, 1.0);
        ck_assert_msg(fabs(result.price - bs_price) < PRICE_TOLERANCE,
            "ITM put price mismatch: got %.4f, expected %.4f",
            result.price, bs_price);
        /* ITM put should have positive intrinsic value */
        ck_assert_msg(result.price > 10.0, "ITM put should be worth > intrinsic");
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

/* ============================================================================
 * American Option Tests
 * ============================================================================ */

START_TEST(test_binomial_american_call)
{
#ifdef NIMCP_CUDA_ENABLED
    /* American call on non-dividend stock = European call */
    fin_derivatives_gpu_params_t params = {
        .spot_price = 100.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = true,
        .is_american = true
    };

    fin_derivatives_gpu_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);

    if (success) {
        /* American call (no dividends) = European call */
        double bs_price = black_scholes_call(100.0, 100.0, 0.05, 0.2, 1.0);
        ck_assert_msg(fabs(result.price - bs_price) < PRICE_TOLERANCE,
            "American call should equal European call (no dividends): got %.4f, expected %.4f",
            result.price, bs_price);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

START_TEST(test_binomial_american_put_premium)
{
#ifdef NIMCP_CUDA_ENABLED
    /* American put should be worth >= European put */
    fin_derivatives_gpu_params_t params_euro = {
        .spot_price = 100.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = false,
        .is_american = false
    };

    fin_derivatives_gpu_params_t params_amer = {
        .spot_price = 100.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = false,
        .is_american = true
    };

    fin_derivatives_gpu_result_t result_euro, result_amer;
    memset(&result_euro, 0, sizeof(result_euro));
    memset(&result_amer, 0, sizeof(result_amer));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success_euro = fin_derivatives_gpu_binomial_tree(ctx, &params_euro, &result_euro);
    bool success_amer = fin_derivatives_gpu_binomial_tree(ctx, &params_amer, &result_amer);

    if (success_euro && success_amer) {
        ck_assert_msg(result_amer.price >= result_euro.price - TOLERANCE,
            "American put (%.4f) should be >= European put (%.4f)",
            result_amer.price, result_euro.price);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

/* ============================================================================
 * Black-Scholes Batch Tests
 * ============================================================================ */

START_TEST(test_black_scholes_batch_calls)
{
#ifdef NIMCP_CUDA_ENABLED
    /* Batch pricing of multiple call options */
    const int n_options = 5;
    float spots[] = {90.0f, 95.0f, 100.0f, 105.0f, 110.0f};
    float strikes[] = {100.0f, 100.0f, 100.0f, 100.0f, 100.0f};
    float rates[] = {0.05f, 0.05f, 0.05f, 0.05f, 0.05f};
    float vols[] = {0.2f, 0.2f, 0.2f, 0.2f, 0.2f};
    float times[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    bool is_call[] = {true, true, true, true, true};
    float prices[5];

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success = fin_derivatives_gpu_black_scholes_batch(
        ctx, spots, strikes, rates, vols, times, is_call, prices, n_options);

    if (success) {
        /* Verify each price against analytical solution */
        for (int i = 0; i < n_options; i++) {
            double expected = black_scholes_call(spots[i], strikes[i], rates[i], vols[i], times[i]);
            ck_assert_msg(fabs(prices[i] - expected) < PRICE_TOLERANCE,
                "BS batch call %d: got %.4f, expected %.4f", i, prices[i], expected);
        }

        /* Prices should increase monotonically with spot */
        for (int i = 1; i < n_options; i++) {
            ck_assert_msg(prices[i] > prices[i-1],
                "Call prices should increase with spot price");
        }
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

START_TEST(test_black_scholes_batch_puts)
{
#ifdef NIMCP_CUDA_ENABLED
    /* Batch pricing of multiple put options */
    const int n_options = 5;
    float spots[] = {90.0f, 95.0f, 100.0f, 105.0f, 110.0f};
    float strikes[] = {100.0f, 100.0f, 100.0f, 100.0f, 100.0f};
    float rates[] = {0.05f, 0.05f, 0.05f, 0.05f, 0.05f};
    float vols[] = {0.2f, 0.2f, 0.2f, 0.2f, 0.2f};
    float times[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    bool is_call[] = {false, false, false, false, false};
    float prices[5];

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success = fin_derivatives_gpu_black_scholes_batch(
        ctx, spots, strikes, rates, vols, times, is_call, prices, n_options);

    if (success) {
        /* Verify each price against analytical solution */
        for (int i = 0; i < n_options; i++) {
            double expected = black_scholes_put(spots[i], strikes[i], rates[i], vols[i], times[i]);
            ck_assert_msg(fabs(prices[i] - expected) < PRICE_TOLERANCE,
                "BS batch put %d: got %.4f, expected %.4f", i, prices[i], expected);
        }

        /* Put prices should decrease monotonically with spot */
        for (int i = 1; i < n_options; i++) {
            ck_assert_msg(prices[i] < prices[i-1],
                "Put prices should decrease with spot price");
        }
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

/* ============================================================================
 * Greeks Tests
 * ============================================================================ */

START_TEST(test_greeks_delta_call)
{
#ifdef NIMCP_CUDA_ENABLED
    /* Delta should be between 0 and 1 for calls */
    fin_derivatives_gpu_params_t params = {
        .spot_price = 100.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = true,
        .is_american = false
    };

    fin_derivatives_gpu_greeks_t greeks;
    memset(&greeks, 0, sizeof(greeks));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success = fin_derivatives_gpu_compute_greeks(ctx, &params, &greeks);

    if (success) {
        /* ATM call delta should be around 0.5-0.6 */
        ck_assert_msg(greeks.delta > 0.0 && greeks.delta < 1.0,
            "Call delta should be in (0, 1): got %.4f", greeks.delta);
        ck_assert_msg(greeks.delta > 0.4 && greeks.delta < 0.7,
            "ATM call delta should be around 0.5-0.6: got %.4f", greeks.delta);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

START_TEST(test_greeks_delta_put)
{
#ifdef NIMCP_CUDA_ENABLED
    /* Delta should be between -1 and 0 for puts */
    fin_derivatives_gpu_params_t params = {
        .spot_price = 100.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = false,
        .is_american = false
    };

    fin_derivatives_gpu_greeks_t greeks;
    memset(&greeks, 0, sizeof(greeks));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success = fin_derivatives_gpu_compute_greeks(ctx, &params, &greeks);

    if (success) {
        /* ATM put delta should be around -0.4 to -0.5 */
        ck_assert_msg(greeks.delta > -1.0 && greeks.delta < 0.0,
            "Put delta should be in (-1, 0): got %.4f", greeks.delta);
        ck_assert_msg(greeks.delta > -0.6 && greeks.delta < -0.3,
            "ATM put delta should be around -0.4 to -0.5: got %.4f", greeks.delta);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

START_TEST(test_greeks_gamma)
{
#ifdef NIMCP_CUDA_ENABLED
    /* Gamma should be positive and peak at ATM */
    fin_derivatives_gpu_params_t params_atm = {
        .spot_price = 100.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = true,
        .is_american = false
    };

    fin_derivatives_gpu_params_t params_itm = {
        .spot_price = 110.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = true,
        .is_american = false
    };

    fin_derivatives_gpu_greeks_t greeks_atm, greeks_itm;
    memset(&greeks_atm, 0, sizeof(greeks_atm));
    memset(&greeks_itm, 0, sizeof(greeks_itm));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success_atm = fin_derivatives_gpu_compute_greeks(ctx, &params_atm, &greeks_atm);
    bool success_itm = fin_derivatives_gpu_compute_greeks(ctx, &params_itm, &greeks_itm);

    if (success_atm && success_itm) {
        /* Gamma should be positive */
        ck_assert_msg(greeks_atm.gamma > 0.0,
            "Gamma should be positive: got %.6f", greeks_atm.gamma);

        /* ATM gamma should be higher than ITM gamma */
        ck_assert_msg(greeks_atm.gamma > greeks_itm.gamma,
            "ATM gamma (%.6f) should be > ITM gamma (%.6f)",
            greeks_atm.gamma, greeks_itm.gamma);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

START_TEST(test_greeks_theta)
{
#ifdef NIMCP_CUDA_ENABLED
    /* Theta should be negative (time decay) for long options */
    fin_derivatives_gpu_params_t params = {
        .spot_price = 100.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = true,
        .is_american = false
    };

    fin_derivatives_gpu_greeks_t greeks;
    memset(&greeks, 0, sizeof(greeks));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success = fin_derivatives_gpu_compute_greeks(ctx, &params, &greeks);

    if (success) {
        /* Theta should be negative for long options */
        ck_assert_msg(greeks.theta < 0.0,
            "Theta should be negative (time decay): got %.6f", greeks.theta);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

START_TEST(test_greeks_vega)
{
#ifdef NIMCP_CUDA_ENABLED
    /* Vega should be positive and peak at ATM */
    fin_derivatives_gpu_params_t params = {
        .spot_price = 100.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = true,
        .is_american = false
    };

    fin_derivatives_gpu_greeks_t greeks;
    memset(&greeks, 0, sizeof(greeks));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success = fin_derivatives_gpu_compute_greeks(ctx, &params, &greeks);

    if (success) {
        /* Vega should be positive */
        ck_assert_msg(greeks.vega > 0.0,
            "Vega should be positive: got %.6f", greeks.vega);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

START_TEST(test_greeks_rho)
{
#ifdef NIMCP_CUDA_ENABLED
    /* Rho: positive for calls, negative for puts */
    fin_derivatives_gpu_params_t params_call = {
        .spot_price = 100.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = true,
        .is_american = false
    };

    fin_derivatives_gpu_params_t params_put = params_call;
    params_put.is_call = false;

    fin_derivatives_gpu_greeks_t greeks_call, greeks_put;
    memset(&greeks_call, 0, sizeof(greeks_call));
    memset(&greeks_put, 0, sizeof(greeks_put));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success_call = fin_derivatives_gpu_compute_greeks(ctx, &params_call, &greeks_call);
    bool success_put = fin_derivatives_gpu_compute_greeks(ctx, &params_put, &greeks_put);

    if (success_call && success_put) {
        /* Call rho should be positive */
        ck_assert_msg(greeks_call.rho > 0.0,
            "Call rho should be positive: got %.6f", greeks_call.rho);
        /* Put rho should be negative */
        ck_assert_msg(greeks_put.rho < 0.0,
            "Put rho should be negative: got %.6f", greeks_put.rho);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

/* ============================================================================
 * Implied Volatility Tests
 * ============================================================================ */

START_TEST(test_implied_volatility_call)
{
#ifdef NIMCP_CUDA_ENABLED
    /* Compute IV from a known price */
    double true_vol = 0.20;
    double S = 100.0, K = 100.0, r = 0.05, T = 1.0;
    double market_price = black_scholes_call(S, K, r, true_vol, T);

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    float implied_vol = 0.0f;
    bool success = fin_derivatives_gpu_implied_vol(
        ctx, (float)market_price, (float)S, (float)K, (float)r, (float)T,
        true, &implied_vol);

    if (success) {
        ck_assert_msg(fabs(implied_vol - true_vol) < 0.001,
            "Implied vol should match: got %.4f, expected %.4f",
            implied_vol, true_vol);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

START_TEST(test_implied_volatility_put)
{
#ifdef NIMCP_CUDA_ENABLED
    /* Compute IV from a known put price */
    double true_vol = 0.25;
    double S = 100.0, K = 100.0, r = 0.05, T = 1.0;
    double market_price = black_scholes_put(S, K, r, true_vol, T);

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    float implied_vol = 0.0f;
    bool success = fin_derivatives_gpu_implied_vol(
        ctx, (float)market_price, (float)S, (float)K, (float)r, (float)T,
        false, &implied_vol);

    if (success) {
        ck_assert_msg(fabs(implied_vol - true_vol) < 0.001,
            "Implied vol should match: got %.4f, expected %.4f",
            implied_vol, true_vol);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

START_TEST(test_implied_volatility_batch)
{
#ifdef NIMCP_CUDA_ENABLED
    /* Batch implied volatility computation */
    const int n = 3;
    float true_vols[] = {0.15f, 0.20f, 0.30f};
    float prices[3];
    float spots[] = {100.0f, 100.0f, 100.0f};
    float strikes[] = {100.0f, 100.0f, 100.0f};
    float rates[] = {0.05f, 0.05f, 0.05f};
    float times[] = {1.0f, 1.0f, 1.0f};
    bool is_call[] = {true, true, true};

    /* Compute market prices from true vols */
    for (int i = 0; i < n; i++) {
        prices[i] = (float)black_scholes_call(spots[i], strikes[i], rates[i], true_vols[i], times[i]);
    }

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    float implied_vols[3];
    bool success = fin_derivatives_gpu_implied_vol_batch(
        ctx, prices, spots, strikes, rates, times, is_call, implied_vols, n);

    if (success) {
        for (int i = 0; i < n; i++) {
            ck_assert_msg(fabs(implied_vols[i] - true_vols[i]) < 0.002,
                "Batch IV %d: got %.4f, expected %.4f",
                i, implied_vols[i], true_vols[i]);
        }
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

/* ============================================================================
 * Edge Cases and Error Handling Tests
 * ============================================================================ */

START_TEST(test_derivatives_null_context)
{
#ifdef NIMCP_CUDA_ENABLED
    fin_derivatives_gpu_params_t params = {
        .spot_price = 100.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 1.0,
        .num_steps = 100,
        .is_call = true,
        .is_american = false
    };

    fin_derivatives_gpu_result_t result;

    /* Null context should fail gracefully */
    bool success = fin_derivatives_gpu_binomial_tree(NULL, &params, &result);
    ck_assert_msg(!success, "Should fail with null context");
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

START_TEST(test_derivatives_zero_time)
{
#ifdef NIMCP_CUDA_ENABLED
    /* Zero time to maturity - should return intrinsic value */
    fin_derivatives_gpu_params_t params = {
        .spot_price = 110.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.2,
        .time_to_maturity = 0.0,
        .num_steps = 100,
        .is_call = true,
        .is_american = false
    };

    fin_derivatives_gpu_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);

    if (success) {
        /* At expiry, ITM call should be worth intrinsic value */
        ck_assert_msg(fabs(result.price - 10.0) < PRICE_TOLERANCE,
            "Zero time ITM call should equal intrinsic: got %.4f, expected 10.0",
            result.price);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

START_TEST(test_derivatives_high_volatility)
{
#ifdef NIMCP_CUDA_ENABLED
    /* High volatility should increase option price */
    fin_derivatives_gpu_params_t params_low = {
        .spot_price = 100.0,
        .strike_price = 100.0,
        .risk_free_rate = 0.05,
        .volatility = 0.1,
        .time_to_maturity = 1.0,
        .num_steps = 200,
        .is_call = true,
        .is_american = false
    };

    fin_derivatives_gpu_params_t params_high = params_low;
    params_high.volatility = 0.5;

    fin_derivatives_gpu_result_t result_low, result_high;
    memset(&result_low, 0, sizeof(result_low));
    memset(&result_high, 0, sizeof(result_high));

    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    bool success_low = fin_derivatives_gpu_binomial_tree(ctx, &params_low, &result_low);
    bool success_high = fin_derivatives_gpu_binomial_tree(ctx, &params_high, &result_high);

    if (success_low && success_high) {
        ck_assert_msg(result_high.price > result_low.price,
            "Higher volatility should increase option price: low=%.4f, high=%.4f",
            result_low.price, result_high.price);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping GPU test");
#endif
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* financial_derivatives_gpu_suite(void)
{
    Suite* s = suite_create("Financial GPU Derivatives");

    /* European Call Tests */
    TCase* tc_euro_call = tcase_create("European Call");
    tcase_add_test(tc_euro_call, test_binomial_european_call_atm);
    tcase_add_test(tc_euro_call, test_binomial_european_call_itm);
    tcase_add_test(tc_euro_call, test_binomial_european_call_otm);
    tcase_set_timeout(tc_euro_call, 30);
    suite_add_tcase(s, tc_euro_call);

    /* European Put Tests */
    TCase* tc_euro_put = tcase_create("European Put");
    tcase_add_test(tc_euro_put, test_binomial_european_put_atm);
    tcase_add_test(tc_euro_put, test_binomial_european_put_itm);
    tcase_set_timeout(tc_euro_put, 30);
    suite_add_tcase(s, tc_euro_put);

    /* American Option Tests */
    TCase* tc_american = tcase_create("American Options");
    tcase_add_test(tc_american, test_binomial_american_call);
    tcase_add_test(tc_american, test_binomial_american_put_premium);
    tcase_set_timeout(tc_american, 30);
    suite_add_tcase(s, tc_american);

    /* Black-Scholes Batch Tests */
    TCase* tc_bs_batch = tcase_create("Black-Scholes Batch");
    tcase_add_test(tc_bs_batch, test_black_scholes_batch_calls);
    tcase_add_test(tc_bs_batch, test_black_scholes_batch_puts);
    tcase_set_timeout(tc_bs_batch, 30);
    suite_add_tcase(s, tc_bs_batch);

    /* Greeks Tests */
    TCase* tc_greeks = tcase_create("Greeks");
    tcase_add_test(tc_greeks, test_greeks_delta_call);
    tcase_add_test(tc_greeks, test_greeks_delta_put);
    tcase_add_test(tc_greeks, test_greeks_gamma);
    tcase_add_test(tc_greeks, test_greeks_theta);
    tcase_add_test(tc_greeks, test_greeks_vega);
    tcase_add_test(tc_greeks, test_greeks_rho);
    tcase_set_timeout(tc_greeks, 30);
    suite_add_tcase(s, tc_greeks);

    /* Implied Volatility Tests */
    TCase* tc_iv = tcase_create("Implied Volatility");
    tcase_add_test(tc_iv, test_implied_volatility_call);
    tcase_add_test(tc_iv, test_implied_volatility_put);
    tcase_add_test(tc_iv, test_implied_volatility_batch);
    tcase_set_timeout(tc_iv, 30);
    suite_add_tcase(s, tc_iv);

    /* Edge Cases and Errors */
    TCase* tc_edge = tcase_create("Edge Cases");
    tcase_add_test(tc_edge, test_derivatives_null_context);
    tcase_add_test(tc_edge, test_derivatives_zero_time);
    tcase_add_test(tc_edge, test_derivatives_high_volatility);
    tcase_set_timeout(tc_edge, 30);
    suite_add_tcase(s, tc_edge);

    return s;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    Suite* s = financial_derivatives_gpu_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
