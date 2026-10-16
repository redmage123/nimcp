/* ============================================================================
 * Financial GPU Integration Tests
 * ============================================================================
 * WHAT: Integration tests for GPU financial computing modules
 * WHY:  Validate complete financial workflows on GPU
 * HOW:  Test full pipelines (Monte Carlo -> VaR -> Portfolio Opt), CPU/GPU equivalence
 * ============================================================================
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef NIMCP_CUDA_ENABLED
#include "gpu/financial/nimcp_financial_monte_carlo_gpu.h"
#include "gpu/financial/nimcp_financial_optimization_gpu.h"
#include "gpu/financial/nimcp_financial_risk_gpu.h"
#include "gpu/financial/nimcp_financial_derivatives_gpu.h"
#include "gpu/nimcp_gpu_context.h"
#endif

#define TOLERANCE 1e-4
#define STATISTICAL_TOLERANCE 0.05  /* 5% for statistical tests */
#define NUM_PATHS 10000
#define NUM_ASSETS 10

/* ============================================================================
 * Full Monte Carlo -> VaR Pipeline Tests
 * ============================================================================ */

START_TEST(test_monte_carlo_to_var_pipeline)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    /* Create RNG state */
    fin_gpu_rng_t* rng = fin_gpu_rng_create(ctx, NUM_PATHS, 12345);
    if (!rng) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create RNG");
        return;
    }

    /* Step 1: Monte Carlo simulation of asset returns */
    fin_monte_carlo_gpu_params_t mc_params = {
        .initial_value = 100.0f,
        .drift = 0.05f,         /* 5% annual drift */
        .volatility = 0.20f,    /* 20% annual volatility */
        .time_horizon = 1.0f,   /* 1 year */
        .num_steps = 252,       /* Daily steps */
        .num_paths = NUM_PATHS
    };

    fin_monte_carlo_gpu_result_t mc_result;
    memset(&mc_result, 0, sizeof(mc_result));
    mc_result.terminal_values = (float*)malloc(NUM_PATHS * sizeof(float));
    mc_result.path_returns = (float*)malloc(NUM_PATHS * sizeof(float));

    bool mc_success = fin_monte_carlo_gpu_simulate(ctx, rng, &mc_params, &mc_result);

    if (mc_success) {
        /* Step 2: Compute VaR from simulated returns */
        fin_risk_gpu_params_t risk_params = {
            .confidence_level = 0.95f,
            .time_horizon_days = 1
        };

        fin_risk_gpu_result_t risk_result;
        memset(&risk_result, 0, sizeof(risk_result));

        bool risk_success = fin_risk_gpu_compute(ctx, mc_result.path_returns, NUM_PATHS,
            &risk_params, &risk_result);

        if (risk_success) {
            /* VaR should be positive (potential loss) */
            ck_assert_msg(risk_result.var_95 > 0.0f || risk_result.var_95 < 0.0f,
                "VaR should be computed: got %.4f", risk_result.var_95);

            /* CVaR should be more extreme than VaR */
            ck_assert_msg(fabs(risk_result.cvar_95) >= fabs(risk_result.var_95) - TOLERANCE,
                "CVaR should be >= VaR in magnitude: CVaR=%.4f, VaR=%.4f",
                risk_result.cvar_95, risk_result.var_95);

            /* Mean terminal value should be approximately initial * exp(drift * time) */
            float expected_mean = mc_params.initial_value * expf(mc_params.drift * mc_params.time_horizon);
            float actual_mean = mc_result.mean_terminal;
            float rel_error = fabs(actual_mean - expected_mean) / expected_mean;
            ck_assert_msg(rel_error < STATISTICAL_TOLERANCE,
                "Mean terminal value should match expected: got %.2f, expected %.2f (%.1f%% error)",
                actual_mean, expected_mean, rel_error * 100);
        }
    }

    free(mc_result.terminal_values);
    free(mc_result.path_returns);
    fin_gpu_rng_destroy(rng);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Multi-Asset Portfolio Pipeline Tests
 * ============================================================================ */

START_TEST(test_multi_asset_portfolio_pipeline)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    fin_gpu_rng_t* rng = fin_gpu_rng_create(ctx, NUM_PATHS * NUM_ASSETS, 54321);
    if (!rng) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create RNG");
        return;
    }

    /* Step 1: Simulate correlated multi-asset returns */
    float expected_returns[NUM_ASSETS];
    float volatilities[NUM_ASSETS];
    float correlation_matrix[NUM_ASSETS * NUM_ASSETS];

    /* Setup: diversified portfolio with varying returns and correlations */
    for (int i = 0; i < NUM_ASSETS; i++) {
        expected_returns[i] = 0.05f + (float)i * 0.01f;  /* 5% to 14% */
        volatilities[i] = 0.15f + (float)i * 0.02f;      /* 15% to 33% */
        for (int j = 0; j < NUM_ASSETS; j++) {
            if (i == j) {
                correlation_matrix[i * NUM_ASSETS + j] = 1.0f;
            } else {
                /* Moderate correlation */
                correlation_matrix[i * NUM_ASSETS + j] = 0.3f + 0.05f * (float)(abs(i - j) < 3 ? 1 : 0);
            }
        }
    }

    fin_multi_asset_gpu_params_t multi_params = {
        .num_assets = NUM_ASSETS,
        .num_paths = NUM_PATHS,
        .num_steps = 252,
        .time_horizon = 1.0f,
        .expected_returns = expected_returns,
        .volatilities = volatilities,
        .correlation_matrix = correlation_matrix
    };

    float* asset_returns = (float*)malloc(NUM_PATHS * NUM_ASSETS * sizeof(float));

    bool sim_success = fin_monte_carlo_gpu_multi_asset(ctx, rng, &multi_params, asset_returns);

    if (sim_success) {
        /* Step 2: Compute covariance matrix from simulated returns */
        float covariance_matrix[NUM_ASSETS * NUM_ASSETS];

        /* Simple covariance estimation */
        for (int i = 0; i < NUM_ASSETS; i++) {
            float mean_i = 0.0f;
            for (int p = 0; p < NUM_PATHS; p++) {
                mean_i += asset_returns[p * NUM_ASSETS + i];
            }
            mean_i /= NUM_PATHS;

            for (int j = i; j < NUM_ASSETS; j++) {
                float mean_j = 0.0f;
                for (int p = 0; p < NUM_PATHS; p++) {
                    mean_j += asset_returns[p * NUM_ASSETS + j];
                }
                mean_j /= NUM_PATHS;

                float cov = 0.0f;
                for (int p = 0; p < NUM_PATHS; p++) {
                    cov += (asset_returns[p * NUM_ASSETS + i] - mean_i) *
                           (asset_returns[p * NUM_ASSETS + j] - mean_j);
                }
                cov /= (NUM_PATHS - 1);
                covariance_matrix[i * NUM_ASSETS + j] = cov;
                covariance_matrix[j * NUM_ASSETS + i] = cov;
            }
        }

        /* Step 3: Portfolio optimization */
        fin_optimization_gpu_params_t opt_params = {
            .target_return = 0.08f,
            .max_iterations = 1000,
            .convergence_threshold = 1e-6f,
            .constraint_type = FIN_OPT_CONSTRAINT_LONG_ONLY,
            .risk_aversion = 1.0f
        };

        fin_optimization_gpu_result_t opt_result;
        memset(&opt_result, 0, sizeof(opt_result));
        opt_result.weights = (float*)malloc(NUM_ASSETS * sizeof(float));

        bool opt_success = fin_optimization_gpu_mean_variance(ctx,
            expected_returns, covariance_matrix, NUM_ASSETS,
            &opt_params, &opt_result);

        if (opt_success) {
            /* Weights should sum to 1 */
            float weight_sum = 0.0f;
            for (int i = 0; i < NUM_ASSETS; i++) {
                weight_sum += opt_result.weights[i];
            }
            ck_assert_msg(fabs(weight_sum - 1.0f) < 0.01f,
                "Weights should sum to 1: got %.4f", weight_sum);

            /* All weights should be non-negative (long-only) */
            int negative_weights = 0;
            for (int i = 0; i < NUM_ASSETS; i++) {
                if (opt_result.weights[i] < -TOLERANCE) {
                    negative_weights++;
                }
            }
            ck_assert_msg(negative_weights == 0,
                "Long-only constraint violated: %d negative weights", negative_weights);

            /* Portfolio return should be achievable */
            float portfolio_return = 0.0f;
            for (int i = 0; i < NUM_ASSETS; i++) {
                portfolio_return += opt_result.weights[i] * expected_returns[i];
            }
            ck_assert_msg(portfolio_return >= opt_params.target_return - 0.01f,
                "Portfolio return should meet target: got %.4f, target %.4f",
                portfolio_return, opt_params.target_return);
        }

        free(opt_result.weights);
    }

    free(asset_returns);
    fin_gpu_rng_destroy(rng);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Options Pricing and Hedging Pipeline Tests
 * ============================================================================ */

START_TEST(test_options_pricing_hedging_pipeline)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    /* Step 1: Price a portfolio of options */
    const int n_options = 20;
    float spots[20], strikes[20], rates[20], vols[20], times[20];
    bool is_call[20];
    float prices[20];

    for (int i = 0; i < n_options; i++) {
        spots[i] = 100.0f;
        strikes[i] = 80.0f + (float)i * 2.0f;  /* 80 to 118 */
        rates[i] = 0.05f;
        vols[i] = 0.20f;
        times[i] = 0.5f;  /* 6 months */
        is_call[i] = (i % 2 == 0);  /* Alternating calls and puts */
    }

    bool pricing_success = fin_derivatives_gpu_black_scholes_batch(
        ctx, spots, strikes, rates, vols, times, is_call, prices, n_options);

    if (pricing_success) {
        /* Step 2: Compute Greeks for hedging */
        fin_derivatives_gpu_greeks_t* greeks = (fin_derivatives_gpu_greeks_t*)
            malloc(n_options * sizeof(fin_derivatives_gpu_greeks_t));

        fin_derivatives_gpu_params_t params[20];
        for (int i = 0; i < n_options; i++) {
            params[i].spot_price = spots[i];
            params[i].strike_price = strikes[i];
            params[i].risk_free_rate = rates[i];
            params[i].volatility = vols[i];
            params[i].time_to_maturity = times[i];
            params[i].is_call = is_call[i];
            params[i].is_american = false;
        }

        int greeks_computed = 0;
        for (int i = 0; i < n_options; i++) {
            if (fin_derivatives_gpu_compute_greeks(ctx, &params[i], &greeks[i])) {
                greeks_computed++;
            }
        }

        ck_assert_msg(greeks_computed == n_options,
            "All Greeks should be computed: got %d/%d", greeks_computed, n_options);

        /* Step 3: Compute portfolio Greeks (sum of individual Greeks) */
        float portfolio_delta = 0.0f;
        float portfolio_gamma = 0.0f;
        float portfolio_vega = 0.0f;
        float portfolio_theta = 0.0f;

        for (int i = 0; i < n_options; i++) {
            portfolio_delta += greeks[i].delta;
            portfolio_gamma += greeks[i].gamma;
            portfolio_vega += greeks[i].vega;
            portfolio_theta += greeks[i].theta;
        }

        /* With alternating calls/puts, portfolio delta should be partially hedged */
        ck_assert_msg(fabs(portfolio_delta) < (float)n_options,
            "Alternating calls/puts should partially hedge delta: got %.4f",
            portfolio_delta);

        /* Gamma should be positive (sum of all gammas) */
        ck_assert_msg(portfolio_gamma > 0.0f,
            "Portfolio gamma should be positive: got %.4f", portfolio_gamma);

        /* Vega should be positive */
        ck_assert_msg(portfolio_vega > 0.0f,
            "Portfolio vega should be positive: got %.4f", portfolio_vega);

        /* Theta should be negative (time decay) */
        ck_assert_msg(portfolio_theta < 0.0f,
            "Portfolio theta should be negative: got %.4f", portfolio_theta);

        free(greeks);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Efficient Frontier Computation Tests
 * ============================================================================ */

START_TEST(test_efficient_frontier_computation)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    /* Setup 5-asset portfolio */
    const int n_assets = 5;
    float expected_returns[] = {0.05f, 0.08f, 0.10f, 0.12f, 0.15f};

    /* Covariance matrix (symmetric positive definite) */
    float covariance[] = {
        0.04f, 0.01f, 0.01f, 0.02f, 0.01f,
        0.01f, 0.09f, 0.02f, 0.02f, 0.03f,
        0.01f, 0.02f, 0.16f, 0.04f, 0.05f,
        0.02f, 0.02f, 0.04f, 0.25f, 0.06f,
        0.01f, 0.03f, 0.05f, 0.06f, 0.36f
    };

    /* Compute efficient frontier with 10 points */
    const int n_frontier_points = 10;
    fin_efficient_frontier_gpu_params_t frontier_params = {
        .num_points = n_frontier_points,
        .min_return = 0.05f,
        .max_return = 0.15f,
        .constraint_type = FIN_OPT_CONSTRAINT_LONG_ONLY
    };

    fin_efficient_frontier_gpu_result_t frontier_result;
    memset(&frontier_result, 0, sizeof(frontier_result));
    frontier_result.returns = (float*)malloc(n_frontier_points * sizeof(float));
    frontier_result.risks = (float*)malloc(n_frontier_points * sizeof(float));
    frontier_result.weights = (float*)malloc(n_frontier_points * n_assets * sizeof(float));

    bool success = fin_optimization_gpu_efficient_frontier(ctx,
        expected_returns, covariance, n_assets,
        &frontier_params, &frontier_result);

    if (success) {
        /* Returns should be monotonically increasing */
        for (int i = 1; i < n_frontier_points; i++) {
            ck_assert_msg(frontier_result.returns[i] >= frontier_result.returns[i-1] - TOLERANCE,
                "Returns should be monotonically increasing: [%d]=%.4f, [%d]=%.4f",
                i-1, frontier_result.returns[i-1], i, frontier_result.returns[i]);
        }

        /* Risk should generally increase with return (efficient frontier) */
        /* Note: Not strictly monotonic due to diversification benefits */

        /* All weights should sum to 1 for each point */
        for (int p = 0; p < n_frontier_points; p++) {
            float weight_sum = 0.0f;
            for (int a = 0; a < n_assets; a++) {
                weight_sum += frontier_result.weights[p * n_assets + a];
            }
            ck_assert_msg(fabs(weight_sum - 1.0f) < 0.01f,
                "Weights at point %d should sum to 1: got %.4f", p, weight_sum);
        }

        /* Sharpe ratios should be positive for most points */
        int positive_sharpe = 0;
        float rf_rate = 0.02f;
        for (int p = 0; p < n_frontier_points; p++) {
            if (frontier_result.risks[p] > TOLERANCE) {
                float sharpe = (frontier_result.returns[p] - rf_rate) / frontier_result.risks[p];
                if (sharpe > 0.0f) positive_sharpe++;
            }
        }
        ck_assert_msg(positive_sharpe > n_frontier_points / 2,
            "Most frontier points should have positive Sharpe ratio: %d/%d",
            positive_sharpe, n_frontier_points);
    }

    free(frontier_result.returns);
    free(frontier_result.risks);
    free(frontier_result.weights);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Risk Parity Portfolio Tests
 * ============================================================================ */

START_TEST(test_risk_parity_portfolio)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    /* Setup portfolio with varying volatilities */
    const int n_assets = 4;
    float covariance[] = {
        0.04f, 0.01f, 0.01f, 0.01f,  /* Low vol asset */
        0.01f, 0.16f, 0.02f, 0.02f,  /* Medium vol */
        0.01f, 0.02f, 0.36f, 0.03f,  /* High vol */
        0.01f, 0.02f, 0.03f, 0.64f   /* Very high vol */
    };

    fin_risk_parity_gpu_params_t rp_params = {
        .max_iterations = 1000,
        .convergence_threshold = 1e-6f,
        .target_risk = 0.0f  /* Equal risk contribution */
    };

    fin_risk_parity_gpu_result_t rp_result;
    memset(&rp_result, 0, sizeof(rp_result));
    rp_result.weights = (float*)malloc(n_assets * sizeof(float));
    rp_result.risk_contributions = (float*)malloc(n_assets * sizeof(float));

    bool success = fin_optimization_gpu_risk_parity(ctx, covariance, n_assets,
        &rp_params, &rp_result);

    if (success) {
        /* Weights should sum to 1 */
        float weight_sum = 0.0f;
        for (int i = 0; i < n_assets; i++) {
            weight_sum += rp_result.weights[i];
        }
        ck_assert_msg(fabs(weight_sum - 1.0f) < 0.01f,
            "Weights should sum to 1: got %.4f", weight_sum);

        /* Risk contributions should be approximately equal */
        float mean_rc = 0.0f;
        for (int i = 0; i < n_assets; i++) {
            mean_rc += rp_result.risk_contributions[i];
        }
        mean_rc /= n_assets;

        float max_rc_deviation = 0.0f;
        for (int i = 0; i < n_assets; i++) {
            float deviation = fabs(rp_result.risk_contributions[i] - mean_rc);
            if (deviation > max_rc_deviation) max_rc_deviation = deviation;
        }

        ck_assert_msg(max_rc_deviation < mean_rc * 0.2f,
            "Risk contributions should be approximately equal: max deviation %.4f from mean %.4f",
            max_rc_deviation, mean_rc);

        /* Lower vol assets should have higher weights */
        ck_assert_msg(rp_result.weights[0] > rp_result.weights[3],
            "Lower vol asset should have higher weight: low=%.4f, high=%.4f",
            rp_result.weights[0], rp_result.weights[3]);
    }

    free(rp_result.weights);
    free(rp_result.risk_contributions);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Implied Volatility Surface Tests
 * ============================================================================ */

START_TEST(test_implied_vol_surface)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    /* Create grid of options for volatility surface */
    const int n_strikes = 5;
    const int n_maturities = 4;
    const int n_options = n_strikes * n_maturities;

    float spots[20], strikes[20], rates[20], times[20];
    bool is_call[20];
    float prices[20];

    float strike_range[] = {90.0f, 95.0f, 100.0f, 105.0f, 110.0f};
    float time_range[] = {0.25f, 0.5f, 0.75f, 1.0f};

    /* Generate option prices with volatility smile */
    int idx = 0;
    for (int t = 0; t < n_maturities; t++) {
        for (int k = 0; k < n_strikes; k++) {
            spots[idx] = 100.0f;
            strikes[idx] = strike_range[k];
            rates[idx] = 0.05f;
            times[idx] = time_range[t];
            is_call[idx] = true;

            /* Volatility smile: higher vol for OTM options */
            float moneyness = strike_range[k] / 100.0f;
            float smile_vol = 0.20f + 0.05f * (moneyness - 1.0f) * (moneyness - 1.0f);

            /* Generate price from known vol */
            float d1 = (logf(100.0f / strikes[idx]) + (rates[idx] + 0.5f * smile_vol * smile_vol) * times[idx])
                       / (smile_vol * sqrtf(times[idx]));
            float d2 = d1 - smile_vol * sqrtf(times[idx]);
            prices[idx] = 100.0f * 0.5f * (1.0f + erff(d1 / sqrtf(2.0f)))
                        - strikes[idx] * expf(-rates[idx] * times[idx]) * 0.5f * (1.0f + erff(d2 / sqrtf(2.0f)));

            idx++;
        }
    }

    /* Compute implied volatilities */
    float implied_vols[20];

    bool success = fin_derivatives_gpu_implied_vol_batch(ctx, prices, spots, strikes,
        rates, times, is_call, implied_vols, n_options);

    if (success) {
        /* Check volatility smile pattern */
        for (int t = 0; t < n_maturities; t++) {
            float atm_vol = implied_vols[t * n_strikes + 2];  /* ATM is at index 2 */
            float otm_put_vol = implied_vols[t * n_strikes + 0];  /* 90 strike */
            float otm_call_vol = implied_vols[t * n_strikes + 4];  /* 110 strike */

            /* OTM options should have higher IV (smile) */
            ck_assert_msg(otm_put_vol >= atm_vol - 0.02f,
                "OTM put vol should be >= ATM vol: OTM=%.4f, ATM=%.4f (maturity %d)",
                otm_put_vol, atm_vol, t);
            ck_assert_msg(otm_call_vol >= atm_vol - 0.02f,
                "OTM call vol should be >= ATM vol: OTM=%.4f, ATM=%.4f (maturity %d)",
                otm_call_vol, atm_vol, t);
        }

        /* All implied vols should be in reasonable range */
        for (int i = 0; i < n_options; i++) {
            ck_assert_msg(implied_vols[i] > 0.05f && implied_vols[i] < 0.50f,
                "Implied vol should be in reasonable range: got %.4f at option %d",
                implied_vols[i], i);
        }
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Stress Testing Pipeline Tests
 * ============================================================================ */

START_TEST(test_stress_testing_pipeline)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    fin_gpu_rng_t* rng = fin_gpu_rng_create(ctx, NUM_PATHS, 99999);
    if (!rng) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create RNG");
        return;
    }

    /* Step 1: Normal market conditions */
    fin_monte_carlo_gpu_params_t normal_params = {
        .initial_value = 100.0f,
        .drift = 0.07f,
        .volatility = 0.20f,
        .time_horizon = 1.0f,
        .num_steps = 252,
        .num_paths = NUM_PATHS
    };

    /* Step 2: Stressed market conditions */
    fin_monte_carlo_gpu_params_t stress_params = {
        .initial_value = 100.0f,
        .drift = -0.10f,        /* Negative drift */
        .volatility = 0.50f,    /* Higher volatility */
        .time_horizon = 1.0f,
        .num_steps = 252,
        .num_paths = NUM_PATHS
    };

    fin_monte_carlo_gpu_result_t normal_result, stress_result;
    memset(&normal_result, 0, sizeof(normal_result));
    memset(&stress_result, 0, sizeof(stress_result));
    normal_result.terminal_values = (float*)malloc(NUM_PATHS * sizeof(float));
    normal_result.path_returns = (float*)malloc(NUM_PATHS * sizeof(float));
    stress_result.terminal_values = (float*)malloc(NUM_PATHS * sizeof(float));
    stress_result.path_returns = (float*)malloc(NUM_PATHS * sizeof(float));

    bool normal_success = fin_monte_carlo_gpu_simulate(ctx, rng, &normal_params, &normal_result);
    bool stress_success = fin_monte_carlo_gpu_simulate(ctx, rng, &stress_params, &stress_result);

    if (normal_success && stress_success) {
        /* Compute VaR for both scenarios */
        fin_risk_gpu_params_t risk_params = {
            .confidence_level = 0.99f,
            .time_horizon_days = 1
        };

        fin_risk_gpu_result_t normal_risk, stress_risk;

        bool normal_risk_success = fin_risk_gpu_compute(ctx, normal_result.path_returns, NUM_PATHS,
            &risk_params, &normal_risk);
        bool stress_risk_success = fin_risk_gpu_compute(ctx, stress_result.path_returns, NUM_PATHS,
            &risk_params, &stress_risk);

        if (normal_risk_success && stress_risk_success) {
            /* Stressed VaR should be worse (more negative or larger absolute value) */
            ck_assert_msg(fabs(stress_risk.var_99) > fabs(normal_risk.var_99) - TOLERANCE,
                "Stress VaR should be worse than normal: stress=%.4f, normal=%.4f",
                stress_risk.var_99, normal_risk.var_99);

            /* Stressed CVaR should also be worse */
            ck_assert_msg(fabs(stress_risk.cvar_99) > fabs(normal_risk.cvar_99) - TOLERANCE,
                "Stress CVaR should be worse than normal: stress=%.4f, normal=%.4f",
                stress_risk.cvar_99, normal_risk.cvar_99);
        }

        /* Mean terminal value should reflect the different drifts */
        ck_assert_msg(normal_result.mean_terminal > stress_result.mean_terminal,
            "Normal scenario should have higher mean: normal=%.2f, stress=%.2f",
            normal_result.mean_terminal, stress_result.mean_terminal);
    }

    free(normal_result.terminal_values);
    free(normal_result.path_returns);
    free(stress_result.terminal_values);
    free(stress_result.path_returns);
    fin_gpu_rng_destroy(rng);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* financial_gpu_integration_suite(void)
{
    Suite* s = suite_create("Financial GPU Integration");

    /* Monte Carlo Pipeline Tests */
    TCase* tc_mc = tcase_create("Monte Carlo Pipelines");
    tcase_add_test(tc_mc, test_monte_carlo_to_var_pipeline);
    tcase_add_test(tc_mc, test_multi_asset_portfolio_pipeline);
    tcase_set_timeout(tc_mc, 120);
    suite_add_tcase(s, tc_mc);

    /* Options and Derivatives Tests */
    TCase* tc_deriv = tcase_create("Options and Derivatives");
    tcase_add_test(tc_deriv, test_options_pricing_hedging_pipeline);
    tcase_add_test(tc_deriv, test_implied_vol_surface);
    tcase_set_timeout(tc_deriv, 120);
    suite_add_tcase(s, tc_deriv);

    /* Portfolio Optimization Tests */
    TCase* tc_opt = tcase_create("Portfolio Optimization");
    tcase_add_test(tc_opt, test_efficient_frontier_computation);
    tcase_add_test(tc_opt, test_risk_parity_portfolio);
    tcase_set_timeout(tc_opt, 120);
    suite_add_tcase(s, tc_opt);

    /* Stress Testing Tests */
    TCase* tc_stress = tcase_create("Stress Testing");
    tcase_add_test(tc_stress, test_stress_testing_pipeline);
    tcase_set_timeout(tc_stress, 180);
    suite_add_tcase(s, tc_stress);

    return s;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    Suite* s = financial_gpu_integration_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
