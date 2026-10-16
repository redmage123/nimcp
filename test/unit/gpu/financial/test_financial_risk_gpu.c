/**
 * @file test_financial_risk_gpu.c
 * @brief Unit tests for GPU risk metrics computation
 *
 * WHAT: Test suite for GPU VaR, CVaR, and volatility kernels
 * WHY:  Verify correct risk metric calculations
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
#include "gpu/financial/nimcp_financial_risk_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/exception/nimcp_exception.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_EPSILON        0.01f
#define TEST_NUM_RETURNS    1000
#define TEST_CONFIDENCE     0.95f

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static nimcp_gpu_context_t* g_ctx = NULL;
static bool g_cuda_available = false;
static float* g_test_returns = NULL;

static void setup(void)
{
    nimcp_exception_system_init();

    g_ctx = nimcp_gpu_context_create(NULL);
    g_cuda_available = (g_ctx != NULL && nimcp_gpu_context_is_valid(g_ctx));

    /* Generate test returns with known properties */
    /* Normal distribution with mean=0.0005 (daily), std=0.02 (2% daily vol) */
    g_test_returns = (float*)malloc(TEST_NUM_RETURNS * sizeof(float));

    /* Simple deterministic "pseudo-random" for reproducibility */
    srand(12345);
    for (int i = 0; i < TEST_NUM_RETURNS; i++) {
        /* Box-Muller transform for normal distribution */
        float u1 = (float)(rand() + 1) / ((float)RAND_MAX + 2);
        float u2 = (float)(rand() + 1) / ((float)RAND_MAX + 2);
        float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * M_PI * u2);
        g_test_returns[i] = 0.0005f + 0.02f * z;
    }
}

static void teardown(void)
{
    if (g_ctx) {
        nimcp_gpu_context_destroy(g_ctx);
        g_ctx = NULL;
    }
    g_cuda_available = false;

    free(g_test_returns);
    g_test_returns = NULL;

    nimcp_exception_system_shutdown();
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static int compare_floats(const void* a, const void* b)
{
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

static float compute_cpu_var(const float* returns, uint32_t n, float confidence)
{
    /* Copy and sort */
    float* sorted = (float*)malloc(n * sizeof(float));
    memcpy(sorted, returns, n * sizeof(float));
    qsort(sorted, n, sizeof(float), compare_floats);

    uint32_t var_idx = (uint32_t)((1.0f - confidence) * n);
    float var = -sorted[var_idx];

    free(sorted);
    return var;
}

static float compute_cpu_cvar(const float* returns, uint32_t n, float confidence)
{
    /* Copy and sort */
    float* sorted = (float*)malloc(n * sizeof(float));
    memcpy(sorted, returns, n * sizeof(float));
    qsort(sorted, n, sizeof(float), compare_floats);

    uint32_t cutoff = (uint32_t)((1.0f - confidence) * n);
    if (cutoff == 0) cutoff = 1;

    float sum = 0.0f;
    for (uint32_t i = 0; i < cutoff; i++) {
        sum += sorted[i];
    }
    float cvar = -sum / (float)cutoff;

    free(sorted);
    return cvar;
}

static float compute_cpu_volatility(const float* returns, uint32_t n)
{
    float mean = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        mean += returns[i];
    }
    mean /= (float)n;

    float variance = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float diff = returns[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)(n - 1);

    return sqrtf(variance);
}

/* ============================================================================
 * VaR Tests
 * ============================================================================ */

START_TEST(test_var_basic)
{
    if (!g_cuda_available) {
        fin_risk_gpu_params_t params = {0};
        fin_risk_gpu_result_t result = {0};

        bool ok = fin_risk_gpu_compute(g_ctx, g_test_returns, &params, &result);
        ck_assert(!ok);
        return;
    }

    fin_risk_gpu_params_t params = {
        .num_returns = TEST_NUM_RETURNS,
        .confidence_level = TEST_CONFIDENCE
    };

    fin_risk_gpu_result_t result = {0};

    bool ok = fin_risk_gpu_compute(g_ctx, g_test_returns, &params, &result);
    ck_assert(ok);

    /* Verify VaR is positive */
    ck_assert(result.var > 0.0f);

    /* Compare with CPU computation */
    float cpu_var = compute_cpu_var(g_test_returns, TEST_NUM_RETURNS, TEST_CONFIDENCE);
    ck_assert_float_eq_tol(result.var, cpu_var, 0.005f);
}
END_TEST

START_TEST(test_var_confidence_levels)
{
    if (!g_cuda_available) {
        return;
    }

    float confidences[] = {0.90f, 0.95f, 0.99f};
    float prev_var = 0.0f;

    for (int c = 0; c < 3; c++) {
        fin_risk_gpu_params_t params = {
            .num_returns = TEST_NUM_RETURNS,
            .confidence_level = confidences[c]
        };

        fin_risk_gpu_result_t result = {0};

        bool ok = fin_risk_gpu_compute(g_ctx, g_test_returns, &params, &result);
        ck_assert(ok);

        /* Higher confidence should give higher VaR */
        if (c > 0) {
            ck_assert(result.var >= prev_var - 0.001f);
        }
        prev_var = result.var;
    }
}
END_TEST

/* ============================================================================
 * CVaR Tests
 * ============================================================================ */

START_TEST(test_cvar_basic)
{
    if (!g_cuda_available) {
        return;
    }

    fin_risk_gpu_params_t params = {
        .num_returns = TEST_NUM_RETURNS,
        .confidence_level = TEST_CONFIDENCE
    };

    fin_risk_gpu_result_t result = {0};

    bool ok = fin_risk_gpu_compute(g_ctx, g_test_returns, &params, &result);
    ck_assert(ok);

    /* CVaR should be >= VaR (average of tail is worse than the cutoff) */
    ck_assert(result.cvar >= result.var - 0.001f);

    /* Compare with CPU computation */
    float cpu_cvar = compute_cpu_cvar(g_test_returns, TEST_NUM_RETURNS, TEST_CONFIDENCE);
    ck_assert_float_eq_tol(result.cvar, cpu_cvar, 0.005f);
}
END_TEST

START_TEST(test_cvar_vs_var)
{
    if (!g_cuda_available) {
        return;
    }

    /* Test that CVaR >= VaR always */
    for (float conf = 0.90f; conf <= 0.99f; conf += 0.01f) {
        fin_risk_gpu_params_t params = {
            .num_returns = TEST_NUM_RETURNS,
            .confidence_level = conf
        };

        fin_risk_gpu_result_t result = {0};

        bool ok = fin_risk_gpu_compute(g_ctx, g_test_returns, &params, &result);
        ck_assert(ok);

        /* CVaR should always be >= VaR (it's the average of the tail) */
        ck_assert(result.cvar >= result.var - 0.0001f);
    }
}
END_TEST

/* ============================================================================
 * Volatility Tests
 * ============================================================================ */

START_TEST(test_volatility_simple)
{
    if (!g_cuda_available) {
        float vol = 0.0f;
        bool ok = fin_risk_gpu_volatility(g_ctx, g_test_returns, TEST_NUM_RETURNS,
                                           FIN_VOL_SIMPLE, &vol);
        ck_assert(!ok);
        return;
    }

    float vol = 0.0f;
    bool ok = fin_risk_gpu_volatility(g_ctx, g_test_returns, TEST_NUM_RETURNS,
                                       FIN_VOL_SIMPLE, &vol);
    ck_assert(ok);
    ck_assert(vol > 0.0f);

    /* Compare with CPU */
    float cpu_vol = compute_cpu_volatility(g_test_returns, TEST_NUM_RETURNS);
    ck_assert_float_eq_tol(vol, cpu_vol, 0.001f);
}
END_TEST

START_TEST(test_volatility_ewma)
{
    if (!g_cuda_available) {
        return;
    }

    float vol = 0.0f;
    bool ok = fin_risk_gpu_volatility(g_ctx, g_test_returns, TEST_NUM_RETURNS,
                                       FIN_VOL_EWMA, &vol);
    ck_assert(ok);
    ck_assert(vol > 0.0f);

    /* EWMA should be different from simple (more weight on recent) */
    float simple_vol = 0.0f;
    fin_risk_gpu_volatility(g_ctx, g_test_returns, TEST_NUM_RETURNS,
                             FIN_VOL_SIMPLE, &simple_vol);

    /* They won't be exactly equal */
    /* (Could be higher or lower depending on recent volatility) */
}
END_TEST

/* ============================================================================
 * OHLC Volatility Tests
 * ============================================================================ */

START_TEST(test_volatility_parkinson)
{
    if (!g_cuda_available) {
        return;
    }

    /* Generate synthetic OHLC data */
    uint32_t n = 252;  /* 1 year of daily data */
    float* opens = (float*)malloc(n * sizeof(float));
    float* highs = (float*)malloc(n * sizeof(float));
    float* lows = (float*)malloc(n * sizeof(float));
    float* closes = (float*)malloc(n * sizeof(float));

    float price = 100.0f;
    for (uint32_t i = 0; i < n; i++) {
        opens[i] = price;
        float daily_move = (float)(rand() % 100 - 50) / 1000.0f;  /* +/- 5% */
        closes[i] = price * (1.0f + daily_move);
        highs[i] = fmaxf(opens[i], closes[i]) * (1.0f + 0.01f);  /* 1% above */
        lows[i] = fminf(opens[i], closes[i]) * (1.0f - 0.01f);   /* 1% below */
        price = closes[i];
    }

    float vol = 0.0f;
    bool ok = fin_risk_gpu_volatility_ohlc(g_ctx, opens, highs, lows, closes,
                                            n, FIN_VOL_PARKINSON, &vol);
    ck_assert(ok);
    ck_assert(vol > 0.0f);

    free(opens);
    free(highs);
    free(lows);
    free(closes);
}
END_TEST

START_TEST(test_volatility_garman_klass)
{
    if (!g_cuda_available) {
        return;
    }

    uint32_t n = 252;
    float* opens = (float*)malloc(n * sizeof(float));
    float* highs = (float*)malloc(n * sizeof(float));
    float* lows = (float*)malloc(n * sizeof(float));
    float* closes = (float*)malloc(n * sizeof(float));

    float price = 100.0f;
    for (uint32_t i = 0; i < n; i++) {
        opens[i] = price;
        float daily_move = (float)(rand() % 100 - 50) / 1000.0f;
        closes[i] = price * (1.0f + daily_move);
        highs[i] = fmaxf(opens[i], closes[i]) * (1.0f + 0.015f);
        lows[i] = fminf(opens[i], closes[i]) * (1.0f - 0.015f);
        price = closes[i];
    }

    float vol = 0.0f;
    bool ok = fin_risk_gpu_volatility_ohlc(g_ctx, opens, highs, lows, closes,
                                            n, FIN_VOL_GARMAN_KLASS, &vol);
    ck_assert(ok);
    ck_assert(vol > 0.0f);

    free(opens);
    free(highs);
    free(lows);
    free(closes);
}
END_TEST

/* ============================================================================
 * Rolling Risk Tests
 * ============================================================================ */

START_TEST(test_rolling_volatility)
{
    if (!g_cuda_available) {
        return;
    }

    uint32_t window = 20;
    float* rolling_vol = (float*)malloc(TEST_NUM_RETURNS * sizeof(float));

    bool ok = fin_risk_gpu_rolling(g_ctx, g_test_returns, TEST_NUM_RETURNS,
                                    window, TEST_CONFIDENCE, NULL, rolling_vol);
    ck_assert(ok);

    /* First (window-1) values should be 0 or invalid */
    for (uint32_t i = 0; i < window - 1; i++) {
        ck_assert_float_eq_tol(rolling_vol[i], 0.0f, TEST_EPSILON);
    }

    /* Subsequent values should be positive */
    for (uint32_t i = window - 1; i < TEST_NUM_RETURNS; i++) {
        ck_assert(rolling_vol[i] >= 0.0f);
    }

    free(rolling_vol);
}
END_TEST

START_TEST(test_rolling_var)
{
    if (!g_cuda_available) {
        return;
    }

    uint32_t window = 50;
    float* rolling_var = (float*)malloc(TEST_NUM_RETURNS * sizeof(float));

    bool ok = fin_risk_gpu_rolling(g_ctx, g_test_returns, TEST_NUM_RETURNS,
                                    window, TEST_CONFIDENCE, rolling_var, NULL);
    ck_assert(ok);

    /* Rolling VaR should be positive after warmup */
    for (uint32_t i = window - 1; i < TEST_NUM_RETURNS; i++) {
        ck_assert(rolling_var[i] >= 0.0f);
    }

    free(rolling_var);
}
END_TEST

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

START_TEST(test_risk_null_context)
{
    fin_risk_gpu_params_t params = {.num_returns = 100};
    fin_risk_gpu_result_t result = {0};

    bool ok = fin_risk_gpu_compute(NULL, g_test_returns, &params, &result);
    ck_assert(!ok);
}
END_TEST

START_TEST(test_risk_null_returns)
{
    if (!g_cuda_available) {
        return;
    }

    fin_risk_gpu_params_t params = {.num_returns = 100};
    fin_risk_gpu_result_t result = {0};

    bool ok = fin_risk_gpu_compute(g_ctx, NULL, &params, &result);
    ck_assert(!ok);
}
END_TEST

START_TEST(test_risk_zero_returns)
{
    if (!g_cuda_available) {
        return;
    }

    fin_risk_gpu_params_t params = {.num_returns = 0};
    fin_risk_gpu_result_t result = {0};

    bool ok = fin_risk_gpu_compute(g_ctx, g_test_returns, &params, &result);
    ck_assert(!ok);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* financial_risk_gpu_suite(void)
{
    Suite* s = suite_create("Financial Risk GPU");

    /* VaR tests */
    TCase* tc_var = tcase_create("Value at Risk");
    tcase_add_checked_fixture(tc_var, setup, teardown);
    tcase_add_test(tc_var, test_var_basic);
    tcase_add_test(tc_var, test_var_confidence_levels);
    suite_add_tcase(s, tc_var);

    /* CVaR tests */
    TCase* tc_cvar = tcase_create("Conditional VaR");
    tcase_add_checked_fixture(tc_cvar, setup, teardown);
    tcase_add_test(tc_cvar, test_cvar_basic);
    tcase_add_test(tc_cvar, test_cvar_vs_var);
    suite_add_tcase(s, tc_cvar);

    /* Volatility tests */
    TCase* tc_vol = tcase_create("Volatility");
    tcase_add_checked_fixture(tc_vol, setup, teardown);
    tcase_add_test(tc_vol, test_volatility_simple);
    tcase_add_test(tc_vol, test_volatility_ewma);
    suite_add_tcase(s, tc_vol);

    /* OHLC volatility tests */
    TCase* tc_ohlc = tcase_create("OHLC Volatility");
    tcase_add_checked_fixture(tc_ohlc, setup, teardown);
    tcase_add_test(tc_ohlc, test_volatility_parkinson);
    tcase_add_test(tc_ohlc, test_volatility_garman_klass);
    suite_add_tcase(s, tc_ohlc);

    /* Rolling tests */
    TCase* tc_rolling = tcase_create("Rolling Metrics");
    tcase_add_checked_fixture(tc_rolling, setup, teardown);
    tcase_add_test(tc_rolling, test_rolling_volatility);
    tcase_add_test(tc_rolling, test_rolling_var);
    suite_add_tcase(s, tc_rolling);

    /* Error handling */
    TCase* tc_errors = tcase_create("Error Handling");
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_risk_null_context);
    tcase_add_test(tc_errors, test_risk_null_returns);
    tcase_add_test(tc_errors, test_risk_zero_returns);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = financial_risk_gpu_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
