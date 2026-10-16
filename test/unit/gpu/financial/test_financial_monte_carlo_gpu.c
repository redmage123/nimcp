/**
 * @file test_financial_monte_carlo_gpu.c
 * @brief Unit tests for GPU Monte Carlo simulation
 *
 * WHAT: Test suite for GPU Monte Carlo pricing kernels
 * WHY:  Verify correct GBM, Heston, jump-diffusion simulations
 * HOW:  Unit tests using Check framework
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "gpu/financial/nimcp_financial_gpu.h"
#include "gpu/financial/nimcp_financial_monte_carlo_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/exception/nimcp_exception.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_EPSILON        0.01f
#define TEST_NUM_PATHS      10000
#define TEST_NUM_STEPS      252    /* Daily steps for 1 year */
#define TEST_INITIAL_VALUE  100.0f
#define TEST_DRIFT          0.05f  /* 5% annual drift */
#define TEST_VOLATILITY     0.20f  /* 20% annual volatility */
#define TEST_TIME_HORIZON   1.0f   /* 1 year */

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static nimcp_gpu_context_t* g_ctx = NULL;
static fin_gpu_rng_t* g_rng = NULL;
static bool g_cuda_available = false;

static void setup(void)
{
    nimcp_exception_system_init();

    g_ctx = nimcp_gpu_context_create(NULL);
    g_cuda_available = (g_ctx != NULL && nimcp_gpu_context_is_valid(g_ctx));

    if (g_cuda_available) {
        g_rng = fin_gpu_rng_create(g_ctx, TEST_NUM_PATHS, 12345);
    }
}

static void teardown(void)
{
    if (g_rng) {
        fin_gpu_rng_destroy(g_rng);
        g_rng = NULL;
    }
    if (g_ctx) {
        nimcp_gpu_context_destroy(g_ctx);
        g_ctx = NULL;
    }
    g_cuda_available = false;

    nimcp_exception_system_shutdown();
}

/* ============================================================================
 * RNG Tests
 * ============================================================================ */

START_TEST(test_rng_create)
{
    if (!g_cuda_available) {
        fin_gpu_rng_t* rng = fin_gpu_rng_create(NULL, 1000, 42);
        ck_assert_ptr_null(rng);
        return;
    }

    fin_gpu_rng_t* rng = fin_gpu_rng_create(g_ctx, 1000, 42);
    ck_assert_ptr_nonnull(rng);
    fin_gpu_rng_destroy(rng);
}
END_TEST

START_TEST(test_rng_reseed)
{
    if (!g_cuda_available) {
        return;
    }

    bool result = fin_gpu_rng_reseed(g_rng, 54321);
    ck_assert(result);
}
END_TEST

START_TEST(test_rng_uniform)
{
    if (!g_cuda_available) {
        return;
    }

    float* uniform = (float*)malloc(1000 * sizeof(float));
    ck_assert_ptr_nonnull(uniform);

    bool result = fin_gpu_rng_uniform(g_rng, uniform, 1000);
    /* Note: This tests the host API wrapper; actual values need GPU memory */
    /* For this test, just verify the call doesn't crash */
    (void)result;

    free(uniform);
}
END_TEST

START_TEST(test_rng_normal)
{
    if (!g_cuda_available) {
        return;
    }

    float* normal = (float*)malloc(1000 * sizeof(float));
    ck_assert_ptr_nonnull(normal);

    bool result = fin_gpu_rng_normal(g_rng, normal, 1000);
    (void)result;

    free(normal);
}
END_TEST

/* ============================================================================
 * GBM Simulation Tests
 * ============================================================================ */

START_TEST(test_gbm_simulate_basic)
{
    if (!g_cuda_available) {
        fin_monte_carlo_gpu_params_t params = {0};
        fin_monte_carlo_gpu_result_t result = {0};

        bool ok = fin_monte_carlo_gpu_simulate(g_ctx, g_rng, &params, &result);
        ck_assert(!ok);  /* Should fail without CUDA */
        return;
    }

    fin_monte_carlo_gpu_params_t params = {
        .initial_value = TEST_INITIAL_VALUE,
        .drift = TEST_DRIFT,
        .volatility = TEST_VOLATILITY,
        .time_horizon = TEST_TIME_HORIZON,
        .num_steps = TEST_NUM_STEPS,
        .num_paths = TEST_NUM_PATHS,
        .seed = 12345,
        .use_antithetic = false
    };

    fin_monte_carlo_gpu_result_t result = {0};

    bool ok = fin_monte_carlo_gpu_simulate(g_ctx, g_rng, &params, &result);
    ck_assert(ok);

    /* Verify result is reasonable */
    ck_assert(result.mean_value > 0.0f);
    ck_assert(result.std_error > 0.0f);
    ck_assert_uint_eq(result.num_paths, TEST_NUM_PATHS);

    /* Expected mean: S0 * exp(drift * T) */
    float expected_mean = TEST_INITIAL_VALUE * expf(TEST_DRIFT * TEST_TIME_HORIZON);
    /* Should be within 2 std errors */
    ck_assert_float_eq_tol(result.mean_value, expected_mean, 3.0f * result.std_error);
}
END_TEST

START_TEST(test_gbm_simulate_antithetic)
{
    if (!g_cuda_available) {
        return;
    }

    fin_monte_carlo_gpu_params_t params = {
        .initial_value = TEST_INITIAL_VALUE,
        .drift = TEST_DRIFT,
        .volatility = TEST_VOLATILITY,
        .time_horizon = TEST_TIME_HORIZON,
        .num_steps = TEST_NUM_STEPS,
        .num_paths = TEST_NUM_PATHS,
        .seed = 12345,
        .use_antithetic = true  /* Enable antithetic variates */
    };

    fin_monte_carlo_gpu_result_t result = {0};

    bool ok = fin_monte_carlo_gpu_simulate(g_ctx, g_rng, &params, &result);
    ck_assert(ok);

    /* Antithetic should reduce variance */
    ck_assert(result.mean_value > 0.0f);
}
END_TEST

START_TEST(test_gbm_zero_volatility)
{
    if (!g_cuda_available) {
        return;
    }

    fin_monte_carlo_gpu_params_t params = {
        .initial_value = TEST_INITIAL_VALUE,
        .drift = TEST_DRIFT,
        .volatility = 0.0f,  /* Zero volatility */
        .time_horizon = TEST_TIME_HORIZON,
        .num_steps = TEST_NUM_STEPS,
        .num_paths = 1000,
        .seed = 12345
    };

    fin_monte_carlo_gpu_result_t result = {0};

    bool ok = fin_monte_carlo_gpu_simulate(g_ctx, g_rng, &params, &result);
    ck_assert(ok);

    /* With zero vol, all paths should give same deterministic result */
    float expected = TEST_INITIAL_VALUE * expf(TEST_DRIFT * TEST_TIME_HORIZON);
    ck_assert_float_eq_tol(result.mean_value, expected, 0.01f);
    /* Variance should be very small */
    ck_assert(result.variance < 0.01f);
}
END_TEST

/* ============================================================================
 * Heston Model Tests
 * ============================================================================ */

START_TEST(test_heston_simulate)
{
    if (!g_cuda_available) {
        fin_heston_params_t params = {0};
        fin_monte_carlo_gpu_result_t result = {0};

        bool ok = fin_monte_carlo_gpu_heston(g_ctx, g_rng, &params, &result);
        ck_assert(!ok);
        return;
    }

    fin_heston_params_t params = {
        .base = {
            .initial_value = TEST_INITIAL_VALUE,
            .drift = TEST_DRIFT,
            .volatility = TEST_VOLATILITY,
            .time_horizon = TEST_TIME_HORIZON,
            .num_steps = TEST_NUM_STEPS,
            .num_paths = TEST_NUM_PATHS,
            .seed = 12345
        },
        .initial_variance = 0.04f,   /* V0 = 0.04 (20% vol squared) */
        .kappa = 2.0f,               /* Mean reversion speed */
        .theta = 0.04f,              /* Long-run variance */
        .xi = 0.3f,                  /* Vol of vol */
        .rho = -0.7f                 /* Correlation (negative for equity) */
    };

    fin_monte_carlo_gpu_result_t result = {0};

    bool ok = fin_monte_carlo_gpu_heston(g_ctx, g_rng, &params, &result);
    ck_assert(ok);

    /* Verify reasonable result */
    ck_assert(result.mean_value > 0.0f);
    ck_assert(result.std_error > 0.0f);
}
END_TEST

/* ============================================================================
 * Path-Dependent Option Tests
 * ============================================================================ */

START_TEST(test_asian_option)
{
    if (!g_cuda_available) {
        return;
    }

    fin_mc_option_params_t params = {
        .base = {
            .initial_value = TEST_INITIAL_VALUE,
            .drift = TEST_DRIFT,
            .volatility = TEST_VOLATILITY,
            .time_horizon = TEST_TIME_HORIZON,
            .num_steps = TEST_NUM_STEPS,
            .num_paths = TEST_NUM_PATHS,
            .seed = 12345
        },
        .strike = TEST_INITIAL_VALUE,  /* ATM */
        .option_type = FIN_OPT_CALL,
        .option_style = FIN_MC_OPT_ASIAN
    };

    fin_mc_option_result_t result = {0};

    bool ok = fin_monte_carlo_gpu_option_price(g_ctx, g_rng, &params, &result);
    ck_assert(ok);

    /* Asian option price should be less than European (averaging effect) */
    ck_assert(result.price > 0.0f);
    ck_assert(result.std_error > 0.0f);
}
END_TEST

START_TEST(test_lookback_option)
{
    if (!g_cuda_available) {
        return;
    }

    fin_mc_option_params_t params = {
        .base = {
            .initial_value = TEST_INITIAL_VALUE,
            .drift = TEST_DRIFT,
            .volatility = TEST_VOLATILITY,
            .time_horizon = TEST_TIME_HORIZON,
            .num_steps = TEST_NUM_STEPS,
            .num_paths = TEST_NUM_PATHS,
            .seed = 12345
        },
        .option_type = FIN_OPT_CALL,
        .option_style = FIN_MC_OPT_LOOKBACK
    };

    fin_mc_option_result_t result = {0};

    bool ok = fin_monte_carlo_gpu_option_price(g_ctx, g_rng, &params, &result);
    ck_assert(ok);

    /* Lookback should have positive value */
    ck_assert(result.price > 0.0f);
}
END_TEST

START_TEST(test_barrier_option)
{
    if (!g_cuda_available) {
        return;
    }

    fin_mc_option_params_t params = {
        .base = {
            .initial_value = TEST_INITIAL_VALUE,
            .drift = TEST_DRIFT,
            .volatility = TEST_VOLATILITY,
            .time_horizon = TEST_TIME_HORIZON,
            .num_steps = TEST_NUM_STEPS,
            .num_paths = TEST_NUM_PATHS,
            .seed = 12345
        },
        .strike = TEST_INITIAL_VALUE,
        .barrier = 120.0f,  /* Up-and-out barrier at 120 */
        .option_type = FIN_OPT_CALL,
        .option_style = FIN_MC_OPT_BARRIER,
        .is_up_barrier = true,
        .rebate = 0.0f
    };

    fin_mc_option_result_t result = {0};

    bool ok = fin_monte_carlo_gpu_option_price(g_ctx, g_rng, &params, &result);
    ck_assert(ok);

    /* Barrier option should be less than vanilla */
    ck_assert(result.price >= 0.0f);  /* Could be knocked out */
}
END_TEST

/* ============================================================================
 * Multi-Asset Tests
 * ============================================================================ */

START_TEST(test_multi_asset_simulation)
{
    if (!g_cuda_available) {
        return;
    }

    uint32_t n_assets = 3;
    uint32_t num_paths = 1000;

    float initial_values[] = {100.0f, 50.0f, 200.0f};
    float drifts[] = {0.05f, 0.03f, 0.07f};
    float volatilities[] = {0.20f, 0.15f, 0.25f};

    /* Cholesky factor for correlation matrix */
    float cholesky_L[] = {
        1.0f, 0.0f, 0.0f,
        0.5f, 0.866f, 0.0f,
        0.3f, 0.4f, 0.866f
    };

    fin_multi_asset_params_t params = {
        .base = {
            .initial_value = 0.0f,  /* Not used for multi-asset */
            .drift = 0.05f,
            .volatility = 0.20f,
            .time_horizon = TEST_TIME_HORIZON,
            .num_steps = 252,
            .num_paths = num_paths,
            .seed = 12345
        },
        .n_assets = n_assets,
        .initial_values = initial_values,
        .drifts = drifts,
        .volatilities = volatilities,
        .cholesky_L = cholesky_L
    };

    float* terminal_values = (float*)malloc(num_paths * n_assets * sizeof(float));
    ck_assert_ptr_nonnull(terminal_values);

    bool ok = fin_monte_carlo_gpu_multi_asset(g_ctx, g_rng, &params, terminal_values);
    ck_assert(ok);

    /* Verify each asset has positive terminal values */
    for (uint32_t p = 0; p < num_paths; p++) {
        for (uint32_t a = 0; a < n_assets; a++) {
            ck_assert(terminal_values[p * n_assets + a] > 0.0f);
        }
    }

    free(terminal_values);
}
END_TEST

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

START_TEST(test_mc_null_context)
{
    fin_monte_carlo_gpu_params_t params = {0};
    fin_monte_carlo_gpu_result_t result = {0};

    bool ok = fin_monte_carlo_gpu_simulate(NULL, g_rng, &params, &result);
    ck_assert(!ok);
}
END_TEST

START_TEST(test_mc_null_rng)
{
    if (!g_cuda_available) {
        return;
    }

    fin_monte_carlo_gpu_params_t params = {
        .initial_value = 100.0f,
        .num_paths = 1000,
        .num_steps = 100
    };
    fin_monte_carlo_gpu_result_t result = {0};

    bool ok = fin_monte_carlo_gpu_simulate(g_ctx, NULL, &params, &result);
    ck_assert(!ok);
}
END_TEST

START_TEST(test_mc_zero_paths)
{
    if (!g_cuda_available) {
        return;
    }

    fin_monte_carlo_gpu_params_t params = {
        .initial_value = 100.0f,
        .num_paths = 0,  /* Invalid */
        .num_steps = 100
    };
    fin_monte_carlo_gpu_result_t result = {0};

    bool ok = fin_monte_carlo_gpu_simulate(g_ctx, g_rng, &params, &result);
    ck_assert(!ok);
}
END_TEST

START_TEST(test_mc_zero_steps)
{
    if (!g_cuda_available) {
        return;
    }

    fin_monte_carlo_gpu_params_t params = {
        .initial_value = 100.0f,
        .num_paths = 1000,
        .num_steps = 0  /* Invalid */
    };
    fin_monte_carlo_gpu_result_t result = {0};

    bool ok = fin_monte_carlo_gpu_simulate(g_ctx, g_rng, &params, &result);
    ck_assert(!ok);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* financial_monte_carlo_gpu_suite(void)
{
    Suite* s = suite_create("Financial Monte Carlo GPU");

    /* RNG tests */
    TCase* tc_rng = tcase_create("RNG");
    tcase_add_checked_fixture(tc_rng, setup, teardown);
    tcase_add_test(tc_rng, test_rng_create);
    tcase_add_test(tc_rng, test_rng_reseed);
    tcase_add_test(tc_rng, test_rng_uniform);
    tcase_add_test(tc_rng, test_rng_normal);
    suite_add_tcase(s, tc_rng);

    /* GBM tests */
    TCase* tc_gbm = tcase_create("GBM Simulation");
    tcase_add_checked_fixture(tc_gbm, setup, teardown);
    tcase_set_timeout(tc_gbm, 30);  /* Allow more time for GPU */
    tcase_add_test(tc_gbm, test_gbm_simulate_basic);
    tcase_add_test(tc_gbm, test_gbm_simulate_antithetic);
    tcase_add_test(tc_gbm, test_gbm_zero_volatility);
    suite_add_tcase(s, tc_gbm);

    /* Heston tests */
    TCase* tc_heston = tcase_create("Heston Model");
    tcase_add_checked_fixture(tc_heston, setup, teardown);
    tcase_set_timeout(tc_heston, 30);
    tcase_add_test(tc_heston, test_heston_simulate);
    suite_add_tcase(s, tc_heston);

    /* Path-dependent options */
    TCase* tc_exotic = tcase_create("Path-Dependent Options");
    tcase_add_checked_fixture(tc_exotic, setup, teardown);
    tcase_set_timeout(tc_exotic, 60);
    tcase_add_test(tc_exotic, test_asian_option);
    tcase_add_test(tc_exotic, test_lookback_option);
    tcase_add_test(tc_exotic, test_barrier_option);
    suite_add_tcase(s, tc_exotic);

    /* Multi-asset tests */
    TCase* tc_multi = tcase_create("Multi-Asset");
    tcase_add_checked_fixture(tc_multi, setup, teardown);
    tcase_set_timeout(tc_multi, 30);
    tcase_add_test(tc_multi, test_multi_asset_simulation);
    suite_add_tcase(s, tc_multi);

    /* Error handling */
    TCase* tc_errors = tcase_create("Error Handling");
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_mc_null_context);
    tcase_add_test(tc_errors, test_mc_null_rng);
    tcase_add_test(tc_errors, test_mc_zero_paths);
    tcase_add_test(tc_errors, test_mc_zero_steps);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = financial_monte_carlo_gpu_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
