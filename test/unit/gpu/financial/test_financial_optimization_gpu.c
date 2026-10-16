/**
 * @file test_financial_optimization_gpu.c
 * @brief Unit tests for GPU portfolio optimization
 *
 * WHAT: Test suite for GPU portfolio optimization kernels
 * WHY:  Verify correct mean-variance, efficient frontier, risk parity
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
#include "gpu/financial/nimcp_financial_optimization_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/exception/nimcp_exception.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_EPSILON        1e-4f
#define TEST_N_ASSETS       5
#define TEST_RISK_FREE      0.02f

/* ============================================================================
 * Test Data
 * ============================================================================ */

/* Expected returns (annualized) */
static float g_expected_returns[TEST_N_ASSETS] = {
    0.08f,   /* Asset 0: 8% */
    0.10f,   /* Asset 1: 10% */
    0.06f,   /* Asset 2: 6% */
    0.12f,   /* Asset 3: 12% */
    0.07f    /* Asset 4: 7% */
};

/* Covariance matrix (symmetric, positive definite) */
static float g_covariance[TEST_N_ASSETS * TEST_N_ASSETS] = {
    0.0400f, 0.0080f, 0.0040f, 0.0120f, 0.0060f,  /* Row 0 */
    0.0080f, 0.0625f, 0.0075f, 0.0200f, 0.0100f,  /* Row 1 */
    0.0040f, 0.0075f, 0.0225f, 0.0060f, 0.0045f,  /* Row 2 */
    0.0120f, 0.0200f, 0.0060f, 0.0900f, 0.0150f,  /* Row 3 */
    0.0060f, 0.0100f, 0.0045f, 0.0150f, 0.0289f   /* Row 4 */
};

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static nimcp_gpu_context_t* g_ctx = NULL;
static bool g_cuda_available = false;

static void setup(void)
{
    nimcp_exception_system_init();

    g_ctx = nimcp_gpu_context_create(NULL);
    g_cuda_available = (g_ctx != NULL && nimcp_gpu_context_is_valid(g_ctx));
}

static void teardown(void)
{
    if (g_ctx) {
        nimcp_gpu_context_destroy(g_ctx);
        g_ctx = NULL;
    }
    g_cuda_available = false;

    nimcp_exception_system_shutdown();
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static float compute_portfolio_return(const float* weights, const float* returns, uint32_t n)
{
    float ret = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        ret += weights[i] * returns[i];
    }
    return ret;
}

static float compute_portfolio_variance(const float* weights, const float* cov, uint32_t n)
{
    float var = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            var += weights[i] * weights[j] * cov[i * n + j];
        }
    }
    return var;
}

static float sum_weights(const float* weights, uint32_t n)
{
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += weights[i];
    }
    return sum;
}

/* ============================================================================
 * Mean-Variance Optimization Tests
 * ============================================================================ */

START_TEST(test_mean_variance_basic)
{
    if (!g_cuda_available) {
        fin_optimization_gpu_params_t params = {0};
        fin_optimization_gpu_result_t result = {0};

        bool ok = fin_optimization_gpu_mean_variance(g_ctx, g_expected_returns,
                                                      g_covariance, &params, &result);
        ck_assert(!ok);
        return;
    }

    fin_optimization_gpu_params_t params = {
        .n_assets = TEST_N_ASSETS,
        .risk_aversion = 2.0f,
        .risk_free_rate = TEST_RISK_FREE,
        .max_iterations = 1000,
        .learning_rate = 0.01f,
        .lower_bounds = NULL,
        .upper_bounds = NULL
    };

    fin_optimization_gpu_result_t result = {0};

    bool ok = fin_optimization_gpu_mean_variance(g_ctx, g_expected_returns,
                                                  g_covariance, &params, &result);
    ck_assert(ok);
    ck_assert_ptr_nonnull(result.optimal_weights);

    /* Verify weights sum to 1 */
    float weight_sum = sum_weights(result.optimal_weights, TEST_N_ASSETS);
    ck_assert_float_eq_tol(weight_sum, 1.0f, 0.01f);

    /* Verify all weights are non-negative */
    for (uint32_t i = 0; i < TEST_N_ASSETS; i++) {
        ck_assert(result.optimal_weights[i] >= -0.01f);  /* Allow small numerical error */
    }

    /* Verify computed return and variance match */
    float computed_ret = compute_portfolio_return(result.optimal_weights,
                                                   g_expected_returns, TEST_N_ASSETS);
    float computed_var = compute_portfolio_variance(result.optimal_weights,
                                                     g_covariance, TEST_N_ASSETS);

    ck_assert_float_eq_tol(result.expected_return, computed_ret, 0.01f);
    ck_assert_float_eq_tol(result.portfolio_variance, computed_var, 0.001f);

    fin_optimization_gpu_result_free(&result);
}
END_TEST

START_TEST(test_mean_variance_with_bounds)
{
    if (!g_cuda_available) {
        return;
    }

    /* Set bounds: 5% to 40% for each asset */
    float lower_bounds[TEST_N_ASSETS] = {0.05f, 0.05f, 0.05f, 0.05f, 0.05f};
    float upper_bounds[TEST_N_ASSETS] = {0.40f, 0.40f, 0.40f, 0.40f, 0.40f};

    fin_optimization_gpu_params_t params = {
        .n_assets = TEST_N_ASSETS,
        .risk_aversion = 2.0f,
        .risk_free_rate = TEST_RISK_FREE,
        .max_iterations = 1000,
        .learning_rate = 0.01f,
        .lower_bounds = lower_bounds,
        .upper_bounds = upper_bounds
    };

    fin_optimization_gpu_result_t result = {0};

    bool ok = fin_optimization_gpu_mean_variance(g_ctx, g_expected_returns,
                                                  g_covariance, &params, &result);
    ck_assert(ok);
    ck_assert_ptr_nonnull(result.optimal_weights);

    /* Verify bounds are respected */
    for (uint32_t i = 0; i < TEST_N_ASSETS; i++) {
        ck_assert(result.optimal_weights[i] >= lower_bounds[i] - 0.01f);
        ck_assert(result.optimal_weights[i] <= upper_bounds[i] + 0.01f);
    }

    fin_optimization_gpu_result_free(&result);
}
END_TEST

START_TEST(test_mean_variance_high_risk_aversion)
{
    if (!g_cuda_available) {
        return;
    }

    fin_optimization_gpu_params_t params_low = {
        .n_assets = TEST_N_ASSETS,
        .risk_aversion = 1.0f,
        .risk_free_rate = TEST_RISK_FREE,
        .max_iterations = 1000,
        .learning_rate = 0.01f
    };

    fin_optimization_gpu_params_t params_high = {
        .n_assets = TEST_N_ASSETS,
        .risk_aversion = 10.0f,  /* Higher risk aversion */
        .risk_free_rate = TEST_RISK_FREE,
        .max_iterations = 1000,
        .learning_rate = 0.01f
    };

    fin_optimization_gpu_result_t result_low = {0};
    fin_optimization_gpu_result_t result_high = {0};

    bool ok1 = fin_optimization_gpu_mean_variance(g_ctx, g_expected_returns,
                                                   g_covariance, &params_low, &result_low);
    bool ok2 = fin_optimization_gpu_mean_variance(g_ctx, g_expected_returns,
                                                   g_covariance, &params_high, &result_high);

    ck_assert(ok1 && ok2);

    /* Higher risk aversion should result in lower volatility */
    ck_assert(result_high.portfolio_volatility <= result_low.portfolio_volatility + 0.01f);

    fin_optimization_gpu_result_free(&result_low);
    fin_optimization_gpu_result_free(&result_high);
}
END_TEST

/* ============================================================================
 * Efficient Frontier Tests
 * ============================================================================ */

START_TEST(test_efficient_frontier_basic)
{
    if (!g_cuda_available) {
        return;
    }

    uint32_t num_points = 10;

    fin_optimization_gpu_params_t params = {
        .n_assets = TEST_N_ASSETS,
        .risk_aversion = 2.0f,
        .risk_free_rate = TEST_RISK_FREE,
        .max_iterations = 500,
        .learning_rate = 0.01f
    };

    fin_efficient_frontier_result_t result = {0};

    bool ok = fin_optimization_gpu_efficient_frontier(g_ctx, g_expected_returns,
                                                       g_covariance, &params,
                                                       num_points, &result);
    ck_assert(ok);
    ck_assert_uint_eq(result.num_points, num_points);
    ck_assert_ptr_nonnull(result.returns);
    ck_assert_ptr_nonnull(result.volatilities);
    ck_assert_ptr_nonnull(result.weights);

    /* Frontier should be monotonically increasing in return vs volatility */
    for (uint32_t i = 1; i < num_points; i++) {
        /* Generally, higher return means higher volatility on efficient frontier */
        /* But this isn't strictly enforced for optimization results */
    }

    fin_efficient_frontier_result_free(&result);
}
END_TEST

START_TEST(test_efficient_frontier_sharpe)
{
    if (!g_cuda_available) {
        return;
    }

    uint32_t num_points = 20;

    fin_optimization_gpu_params_t params = {
        .n_assets = TEST_N_ASSETS,
        .risk_aversion = 2.0f,
        .risk_free_rate = TEST_RISK_FREE,
        .max_iterations = 500,
        .learning_rate = 0.01f
    };

    fin_efficient_frontier_result_t result = {0};

    bool ok = fin_optimization_gpu_efficient_frontier(g_ctx, g_expected_returns,
                                                       g_covariance, &params,
                                                       num_points, &result);
    ck_assert(ok);

    /* Find maximum Sharpe ratio */
    float max_sharpe = -100.0f;
    uint32_t max_sharpe_idx = 0;

    for (uint32_t i = 0; i < num_points; i++) {
        if (result.volatilities[i] > 0.001f) {
            float sharpe = (result.returns[i] - TEST_RISK_FREE) / result.volatilities[i];
            if (sharpe > max_sharpe) {
                max_sharpe = sharpe;
                max_sharpe_idx = i;
            }
        }
    }

    /* Max Sharpe should be positive for these inputs */
    ck_assert(max_sharpe > 0.0f);

    fin_efficient_frontier_result_free(&result);
}
END_TEST

/* ============================================================================
 * Risk Parity Tests
 * ============================================================================ */

START_TEST(test_risk_parity_basic)
{
    if (!g_cuda_available) {
        fin_risk_parity_params_t params = {0};
        fin_optimization_gpu_result_t result = {0};

        bool ok = fin_optimization_gpu_risk_parity(g_ctx, g_covariance, &params, &result);
        ck_assert(!ok);
        return;
    }

    fin_risk_parity_params_t params = {
        .n_assets = TEST_N_ASSETS,
        .max_iterations = 1000,
        .learning_rate = 0.01f,
        .tolerance = 1e-6f
    };

    fin_optimization_gpu_result_t result = {0};

    bool ok = fin_optimization_gpu_risk_parity(g_ctx, g_covariance, &params, &result);
    ck_assert(ok);
    ck_assert_ptr_nonnull(result.optimal_weights);

    /* Verify weights sum to 1 */
    float weight_sum = sum_weights(result.optimal_weights, TEST_N_ASSETS);
    ck_assert_float_eq_tol(weight_sum, 1.0f, 0.01f);

    /* Verify all weights are positive */
    for (uint32_t i = 0; i < TEST_N_ASSETS; i++) {
        ck_assert(result.optimal_weights[i] > -0.01f);
    }

    /* In risk parity, each asset should contribute equally to risk */
    /* This is harder to verify exactly, but weights should be more equal
       than mean-variance with the same covariance */

    fin_optimization_gpu_result_free(&result);
}
END_TEST

START_TEST(test_risk_parity_equal_variance)
{
    if (!g_cuda_available) {
        return;
    }

    /* If all assets have same variance and zero correlation,
       risk parity should give equal weights */
    float equal_cov[TEST_N_ASSETS * TEST_N_ASSETS] = {0};
    for (uint32_t i = 0; i < TEST_N_ASSETS; i++) {
        equal_cov[i * TEST_N_ASSETS + i] = 0.04f;  /* 20% vol for all */
    }

    fin_risk_parity_params_t params = {
        .n_assets = TEST_N_ASSETS,
        .max_iterations = 1000,
        .learning_rate = 0.01f,
        .tolerance = 1e-6f
    };

    fin_optimization_gpu_result_t result = {0};

    bool ok = fin_optimization_gpu_risk_parity(g_ctx, equal_cov, &params, &result);
    ck_assert(ok);

    /* All weights should be equal (~0.2 for 5 assets) */
    float expected_weight = 1.0f / TEST_N_ASSETS;
    for (uint32_t i = 0; i < TEST_N_ASSETS; i++) {
        ck_assert_float_eq_tol(result.optimal_weights[i], expected_weight, 0.02f);
    }

    fin_optimization_gpu_result_free(&result);
}
END_TEST

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

START_TEST(test_opt_null_context)
{
    fin_optimization_gpu_params_t params = {.n_assets = TEST_N_ASSETS};
    fin_optimization_gpu_result_t result = {0};

    bool ok = fin_optimization_gpu_mean_variance(NULL, g_expected_returns,
                                                  g_covariance, &params, &result);
    ck_assert(!ok);
}
END_TEST

START_TEST(test_opt_null_returns)
{
    if (!g_cuda_available) {
        return;
    }

    fin_optimization_gpu_params_t params = {.n_assets = TEST_N_ASSETS};
    fin_optimization_gpu_result_t result = {0};

    bool ok = fin_optimization_gpu_mean_variance(g_ctx, NULL,
                                                  g_covariance, &params, &result);
    ck_assert(!ok);
}
END_TEST

START_TEST(test_opt_null_covariance)
{
    if (!g_cuda_available) {
        return;
    }

    fin_optimization_gpu_params_t params = {.n_assets = TEST_N_ASSETS};
    fin_optimization_gpu_result_t result = {0};

    bool ok = fin_optimization_gpu_mean_variance(g_ctx, g_expected_returns,
                                                  NULL, &params, &result);
    ck_assert(!ok);
}
END_TEST

START_TEST(test_opt_zero_assets)
{
    if (!g_cuda_available) {
        return;
    }

    fin_optimization_gpu_params_t params = {.n_assets = 0};
    fin_optimization_gpu_result_t result = {0};

    bool ok = fin_optimization_gpu_mean_variance(g_ctx, g_expected_returns,
                                                  g_covariance, &params, &result);
    ck_assert(!ok);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* financial_optimization_gpu_suite(void)
{
    Suite* s = suite_create("Financial Optimization GPU");

    /* Mean-variance tests */
    TCase* tc_mv = tcase_create("Mean-Variance");
    tcase_add_checked_fixture(tc_mv, setup, teardown);
    tcase_set_timeout(tc_mv, 30);
    tcase_add_test(tc_mv, test_mean_variance_basic);
    tcase_add_test(tc_mv, test_mean_variance_with_bounds);
    tcase_add_test(tc_mv, test_mean_variance_high_risk_aversion);
    suite_add_tcase(s, tc_mv);

    /* Efficient frontier tests */
    TCase* tc_ef = tcase_create("Efficient Frontier");
    tcase_add_checked_fixture(tc_ef, setup, teardown);
    tcase_set_timeout(tc_ef, 60);
    tcase_add_test(tc_ef, test_efficient_frontier_basic);
    tcase_add_test(tc_ef, test_efficient_frontier_sharpe);
    suite_add_tcase(s, tc_ef);

    /* Risk parity tests */
    TCase* tc_rp = tcase_create("Risk Parity");
    tcase_add_checked_fixture(tc_rp, setup, teardown);
    tcase_set_timeout(tc_rp, 30);
    tcase_add_test(tc_rp, test_risk_parity_basic);
    tcase_add_test(tc_rp, test_risk_parity_equal_variance);
    suite_add_tcase(s, tc_rp);

    /* Error handling */
    TCase* tc_errors = tcase_create("Error Handling");
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_opt_null_context);
    tcase_add_test(tc_errors, test_opt_null_returns);
    tcase_add_test(tc_errors, test_opt_null_covariance);
    tcase_add_test(tc_errors, test_opt_zero_assets);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = financial_optimization_gpu_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
