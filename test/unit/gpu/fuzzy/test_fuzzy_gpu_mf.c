/**
 * @file test_fuzzy_gpu_mf.c
 * @brief Unit tests for GPU fuzzy membership function evaluation
 *
 * WHAT: Test suite for GPU fuzzy membership function kernels
 * WHY:  Verify correct MF evaluation across 14 types and 8 hedges
 * HOW:  Unit tests using Check framework, with CUDA/non-CUDA paths
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "gpu/fuzzy/nimcp_fuzzy_gpu.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_types.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_params.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/exception/nimcp_exception.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_EPSILON        1e-5f
#define TEST_NUM_SAMPLES    100
#define TEST_NUM_MFS        5
#define TEST_RESOLUTION     256

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static nimcp_gpu_context_t* g_ctx = NULL;
static bool g_cuda_available = false;

static void setup(void)
{
    nimcp_exception_system_init();

    /* Try to create GPU context */
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
 * MF Type Tests
 * ============================================================================ */

START_TEST(test_mf_triangular)
{
    if (!g_cuda_available) {
        /* Without CUDA, function should return false gracefully */
        float input = 0.5f;
        float output = 0.0f;
        fuzzy_gpu_mf_t mf = {
            .type = FUZZY_MF_TRIANGULAR,
            .params = {0.0f, 0.5f, 1.0f, 0.0f}
        };

        bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, input, &output);
        ck_assert(!result);  /* Should fail without CUDA */
        return;
    }

    /* With CUDA, verify triangular MF computation */
    float input = 0.5f;
    float output = 0.0f;
    fuzzy_gpu_mf_t mf = {
        .type = FUZZY_MF_TRIANGULAR,
        .params = {0.0f, 0.5f, 1.0f, 0.0f}  /* a=0, b=0.5, c=1 */
    };

    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, input, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, 1.0f, TEST_EPSILON);  /* Peak at center */
}
END_TEST

START_TEST(test_mf_trapezoidal)
{
    if (!g_cuda_available) {
        return;  /* Skip if no CUDA */
    }

    float output = 0.0f;
    fuzzy_gpu_mf_t mf = {
        .type = FUZZY_MF_TRAPEZOIDAL,
        .params = {0.0f, 0.25f, 0.75f, 1.0f}  /* a=0, b=0.25, c=0.75, d=1 */
    };

    /* Test at plateau (should be 1.0) */
    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.5f, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, 1.0f, TEST_EPSILON);

    /* Test at rising edge midpoint */
    result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.125f, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, 0.5f, TEST_EPSILON);
}
END_TEST

START_TEST(test_mf_gaussian)
{
    if (!g_cuda_available) {
        return;
    }

    float output = 0.0f;
    fuzzy_gpu_mf_t mf = {
        .type = FUZZY_MF_GAUSSIAN,
        .params = {0.5f, 0.15f, 0.0f, 0.0f}  /* c=0.5, sigma=0.15 */
    };

    /* Test at center (should be 1.0) */
    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.5f, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, 1.0f, TEST_EPSILON);

    /* Test at 1 sigma away */
    result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.65f, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, expf(-0.5f), 0.01f);  /* exp(-0.5) ~ 0.606 */
}
END_TEST

START_TEST(test_mf_bell)
{
    if (!g_cuda_available) {
        return;
    }

    float output = 0.0f;
    fuzzy_gpu_mf_t mf = {
        .type = FUZZY_MF_BELL,
        .params = {0.25f, 2.0f, 0.5f, 0.0f}  /* a=0.25, b=2, c=0.5 */
    };

    /* Test at center */
    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.5f, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, 1.0f, TEST_EPSILON);
}
END_TEST

START_TEST(test_mf_sigmoid)
{
    if (!g_cuda_available) {
        return;
    }

    float output = 0.0f;
    fuzzy_gpu_mf_t mf = {
        .type = FUZZY_MF_SIGMOID,
        .params = {10.0f, 0.5f, 0.0f, 0.0f}  /* a=10 (slope), c=0.5 (center) */
    };

    /* Test at center (should be 0.5) */
    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.5f, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, 0.5f, TEST_EPSILON);

    /* Test far right (should approach 1) */
    result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 1.0f, &output);
    ck_assert(result);
    ck_assert(output > 0.99f);
}
END_TEST

START_TEST(test_mf_s_shaped)
{
    if (!g_cuda_available) {
        return;
    }

    float output = 0.0f;
    fuzzy_gpu_mf_t mf = {
        .type = FUZZY_MF_S_SHAPED,
        .params = {0.25f, 0.75f, 0.0f, 0.0f}  /* a=0.25, b=0.75 */
    };

    /* Test at midpoint (should be 0.5) */
    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.5f, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, 0.5f, TEST_EPSILON);
}
END_TEST

START_TEST(test_mf_z_shaped)
{
    if (!g_cuda_available) {
        return;
    }

    float output = 0.0f;
    fuzzy_gpu_mf_t mf = {
        .type = FUZZY_MF_Z_SHAPED,
        .params = {0.25f, 0.75f, 0.0f, 0.0f}  /* a=0.25, b=0.75 */
    };

    /* Test at start (should be 1) */
    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.0f, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, 1.0f, TEST_EPSILON);

    /* Test at end (should be 0) */
    result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 1.0f, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, 0.0f, TEST_EPSILON);
}
END_TEST

START_TEST(test_mf_pi_shaped)
{
    if (!g_cuda_available) {
        return;
    }

    float output = 0.0f;
    fuzzy_gpu_mf_t mf = {
        .type = FUZZY_MF_PI_SHAPED,
        .params = {0.1f, 0.4f, 0.6f, 0.9f}  /* a, b, c, d */
    };

    /* Test at center (should be 1.0) */
    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.5f, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, 1.0f, TEST_EPSILON);
}
END_TEST

START_TEST(test_mf_singleton)
{
    if (!g_cuda_available) {
        return;
    }

    float output = 0.0f;
    fuzzy_gpu_mf_t mf = {
        .type = FUZZY_MF_SINGLETON,
        .params = {0.5f, 0.0f, 0.0f, 0.0f}  /* value=0.5 */
    };

    /* Test at singleton location */
    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.5f, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, 1.0f, TEST_EPSILON);

    /* Test away from singleton */
    result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.6f, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, 0.0f, TEST_EPSILON);
}
END_TEST

/* ============================================================================
 * Batch Evaluation Tests
 * ============================================================================ */

START_TEST(test_mf_batch_evaluate)
{
    if (!g_cuda_available) {
        /* Test that batch evaluation fails gracefully without CUDA */
        nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
        float inputs[10];
        float outputs[10];

        for (int i = 0; i < 10; i++) {
            inputs[i] = (float)i / 10.0f;
        }

        bool result = nimcp_gpu_fuzzy_mf_evaluate_batch(g_ctx, NULL, 0, inputs, outputs, 10, &params);
        ck_assert(!result);
        return;
    }

    /* With CUDA, test batch evaluation */
    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();

    float inputs[TEST_NUM_SAMPLES];
    float outputs[TEST_NUM_SAMPLES];

    for (int i = 0; i < TEST_NUM_SAMPLES; i++) {
        inputs[i] = (float)i / (float)(TEST_NUM_SAMPLES - 1);
    }

    fuzzy_gpu_mf_t mf = {
        .type = FUZZY_MF_GAUSSIAN,
        .params = {0.5f, 0.2f, 0.0f, 0.0f}
    };

    bool result = nimcp_gpu_fuzzy_mf_evaluate_batch(g_ctx, &mf, 1, inputs, outputs, TEST_NUM_SAMPLES, &params);
    ck_assert(result);

    /* Verify peak at center */
    int center_idx = TEST_NUM_SAMPLES / 2;
    ck_assert(outputs[center_idx] > outputs[0]);
    ck_assert(outputs[center_idx] > outputs[TEST_NUM_SAMPLES - 1]);
}
END_TEST

START_TEST(test_mf_discretize)
{
    if (!g_cuda_available) {
        float* discretized = NULL;
        fuzzy_gpu_mf_t mf = {.type = FUZZY_MF_TRIANGULAR};
        bool result = nimcp_gpu_fuzzy_mf_discretize(g_ctx, &mf, 0.0f, 1.0f, TEST_RESOLUTION, &discretized);
        ck_assert(!result);
        return;
    }

    float* discretized = NULL;
    fuzzy_gpu_mf_t mf = {
        .type = FUZZY_MF_TRIANGULAR,
        .params = {0.0f, 0.5f, 1.0f, 0.0f}
    };

    bool result = nimcp_gpu_fuzzy_mf_discretize(g_ctx, &mf, 0.0f, 1.0f, TEST_RESOLUTION, &discretized);
    ck_assert(result);
    ck_assert_ptr_nonnull(discretized);

    /* Verify shape */
    ck_assert_float_eq_tol(discretized[0], 0.0f, TEST_EPSILON);
    ck_assert_float_eq_tol(discretized[TEST_RESOLUTION / 2], 1.0f, 0.01f);
    ck_assert_float_eq_tol(discretized[TEST_RESOLUTION - 1], 0.0f, TEST_EPSILON);

    free(discretized);
}
END_TEST

/* ============================================================================
 * Hedge Tests
 * ============================================================================ */

START_TEST(test_hedge_very)
{
    if (!g_cuda_available) {
        return;
    }

    float input = 0.8f;  /* High membership */
    float output = 0.0f;
    fuzzy_gpu_mf_t mf = {
        .type = FUZZY_MF_TRIANGULAR,
        .params = {0.5f, 1.0f, 1.5f, 0.0f},
        .hedge = FUZZY_HEDGE_VERY
    };

    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 1.0f, &output);
    ck_assert(result);
    /* "Very" squares the membership, so 1.0^2 = 1.0 */
    ck_assert_float_eq_tol(output, 1.0f, TEST_EPSILON);

    /* Test with 0.8 membership - squared should be 0.64 */
    /* First get base membership, then apply hedge */
}
END_TEST

START_TEST(test_hedge_somewhat)
{
    if (!g_cuda_available) {
        return;
    }

    /* "Somewhat" takes square root - makes membership higher */
    float output = 0.0f;
    fuzzy_gpu_mf_t mf = {
        .type = FUZZY_MF_SINGLETON,
        .params = {0.5f, 0.0f, 0.0f, 0.0f},
        .hedge = FUZZY_HEDGE_SOMEWHAT
    };

    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.5f, &output);
    ck_assert(result);
    /* At singleton point, membership is 1.0, sqrt(1.0) = 1.0 */
    ck_assert_float_eq_tol(output, 1.0f, TEST_EPSILON);
}
END_TEST

START_TEST(test_hedge_not)
{
    if (!g_cuda_available) {
        return;
    }

    float output = 0.0f;
    fuzzy_gpu_mf_t mf = {
        .type = FUZZY_MF_SINGLETON,
        .params = {0.5f, 0.0f, 0.0f, 0.0f},
        .hedge = FUZZY_HEDGE_NOT
    };

    /* At singleton point, membership is 1.0, NOT(1.0) = 0.0 */
    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.5f, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, 0.0f, TEST_EPSILON);

    /* Away from singleton, membership is 0.0, NOT(0.0) = 1.0 */
    result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.0f, &output);
    ck_assert(result);
    ck_assert_float_eq_tol(output, 1.0f, TEST_EPSILON);
}
END_TEST

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

START_TEST(test_null_context)
{
    float output = 0.0f;
    fuzzy_gpu_mf_t mf = {.type = FUZZY_MF_TRIANGULAR};

    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(NULL, &mf, 0.5f, &output);
    ck_assert(!result);
}
END_TEST

START_TEST(test_null_mf)
{
    if (!g_cuda_available) {
        return;
    }

    float output = 0.0f;
    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, NULL, 0.5f, &output);
    ck_assert(!result);
}
END_TEST

START_TEST(test_null_output)
{
    if (!g_cuda_available) {
        return;
    }

    fuzzy_gpu_mf_t mf = {.type = FUZZY_MF_TRIANGULAR};
    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.5f, NULL);
    ck_assert(!result);
}
END_TEST

START_TEST(test_invalid_mf_type)
{
    if (!g_cuda_available) {
        return;
    }

    float output = 0.0f;
    fuzzy_gpu_mf_t mf = {.type = (fuzzy_mf_type_t)99};  /* Invalid type */

    bool result = nimcp_gpu_fuzzy_mf_evaluate_single(g_ctx, &mf, 0.5f, &output);
    /* Should either fail or default to some behavior */
    (void)result;
}
END_TEST

/* ============================================================================
 * Default Parameters Tests
 * ============================================================================ */

START_TEST(test_mf_eval_params_default)
{
    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();

    ck_assert_uint_eq(params.block_size, 256);
    ck_assert(params.use_shared_memory);
    ck_assert(!params.use_texture_memory);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* fuzzy_gpu_mf_suite(void)
{
    Suite* s = suite_create("Fuzzy GPU MF");

    /* MF Type tests */
    TCase* tc_types = tcase_create("MF Types");
    tcase_add_checked_fixture(tc_types, setup, teardown);
    tcase_add_test(tc_types, test_mf_triangular);
    tcase_add_test(tc_types, test_mf_trapezoidal);
    tcase_add_test(tc_types, test_mf_gaussian);
    tcase_add_test(tc_types, test_mf_bell);
    tcase_add_test(tc_types, test_mf_sigmoid);
    tcase_add_test(tc_types, test_mf_s_shaped);
    tcase_add_test(tc_types, test_mf_z_shaped);
    tcase_add_test(tc_types, test_mf_pi_shaped);
    tcase_add_test(tc_types, test_mf_singleton);
    suite_add_tcase(s, tc_types);

    /* Batch evaluation tests */
    TCase* tc_batch = tcase_create("Batch Evaluation");
    tcase_add_checked_fixture(tc_batch, setup, teardown);
    tcase_add_test(tc_batch, test_mf_batch_evaluate);
    tcase_add_test(tc_batch, test_mf_discretize);
    suite_add_tcase(s, tc_batch);

    /* Hedge tests */
    TCase* tc_hedge = tcase_create("Hedges");
    tcase_add_checked_fixture(tc_hedge, setup, teardown);
    tcase_add_test(tc_hedge, test_hedge_very);
    tcase_add_test(tc_hedge, test_hedge_somewhat);
    tcase_add_test(tc_hedge, test_hedge_not);
    suite_add_tcase(s, tc_hedge);

    /* Error handling tests */
    TCase* tc_errors = tcase_create("Error Handling");
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_null_context);
    tcase_add_test(tc_errors, test_null_mf);
    tcase_add_test(tc_errors, test_null_output);
    tcase_add_test(tc_errors, test_invalid_mf_type);
    suite_add_tcase(s, tc_errors);

    /* Default parameters tests */
    TCase* tc_params = tcase_create("Default Parameters");
    tcase_add_test(tc_params, test_mf_eval_params_default);
    suite_add_tcase(s, tc_params);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = fuzzy_gpu_mf_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
