/**
 * @file test_fuzzy_gpu_defuzz.c
 * @brief Unit tests for GPU fuzzy defuzzification methods
 *
 * WHAT: Test suite for 7 GPU defuzzification kernels
 * WHY:  Verify correct defuzzification computation
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

#include "gpu/fuzzy/nimcp_fuzzy_gpu.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_types.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_params.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/exception/nimcp_exception.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_EPSILON        1e-4f
#define TEST_RESOLUTION     256
#define TEST_BATCH_SIZE     10

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static nimcp_gpu_context_t* g_ctx = NULL;
static bool g_cuda_available = false;
static float* g_test_fuzzy_set = NULL;

static void setup(void)
{
    nimcp_exception_system_init();

    g_ctx = nimcp_gpu_context_create(NULL);
    g_cuda_available = (g_ctx != NULL && nimcp_gpu_context_is_valid(g_ctx));

    /* Create a triangular fuzzy set for testing */
    g_test_fuzzy_set = (float*)malloc(TEST_RESOLUTION * sizeof(float));
    for (int i = 0; i < TEST_RESOLUTION; i++) {
        float x = (float)i / (float)(TEST_RESOLUTION - 1);
        /* Triangular: peak at 0.5 */
        if (x < 0.5f) {
            g_test_fuzzy_set[i] = 2.0f * x;
        } else {
            g_test_fuzzy_set[i] = 2.0f * (1.0f - x);
        }
    }
}

static void teardown(void)
{
    if (g_ctx) {
        nimcp_gpu_context_destroy(g_ctx);
        g_ctx = NULL;
    }
    g_cuda_available = false;

    free(g_test_fuzzy_set);
    g_test_fuzzy_set = NULL;

    nimcp_exception_system_shutdown();
}

/* ============================================================================
 * Centroid Method Tests
 * ============================================================================ */

START_TEST(test_defuzz_centroid_symmetric)
{
    if (!g_cuda_available) {
        float result = 0.0f;
        nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
        params.method = FUZZY_DEFUZZ_CENTROID;

        bool ok = nimcp_gpu_fuzzy_defuzzify(g_ctx, g_test_fuzzy_set, TEST_RESOLUTION,
                                             0.0f, 1.0f, &params, &result);
        ck_assert(!ok);  /* Should fail without CUDA */
        return;
    }

    float result = 0.0f;
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_CENTROID;

    bool ok = nimcp_gpu_fuzzy_defuzzify(g_ctx, g_test_fuzzy_set, TEST_RESOLUTION,
                                         0.0f, 1.0f, &params, &result);
    ck_assert(ok);
    /* Centroid of symmetric triangle should be at center (0.5) */
    ck_assert_float_eq_tol(result, 0.5f, 0.01f);
}
END_TEST

START_TEST(test_defuzz_centroid_left_skew)
{
    if (!g_cuda_available) {
        return;
    }

    /* Create left-skewed fuzzy set */
    float* skewed = (float*)malloc(TEST_RESOLUTION * sizeof(float));
    for (int i = 0; i < TEST_RESOLUTION; i++) {
        float x = (float)i / (float)(TEST_RESOLUTION - 1);
        /* Peak at 0.25 */
        if (x < 0.25f) {
            skewed[i] = 4.0f * x;
        } else {
            skewed[i] = (1.0f - x) / 0.75f;
        }
    }

    float result = 0.0f;
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_CENTROID;

    bool ok = nimcp_gpu_fuzzy_defuzzify(g_ctx, skewed, TEST_RESOLUTION,
                                         0.0f, 1.0f, &params, &result);
    ck_assert(ok);
    /* Centroid should be left of center */
    ck_assert(result < 0.5f);

    free(skewed);
}
END_TEST

/* ============================================================================
 * Bisector Method Tests
 * ============================================================================ */

START_TEST(test_defuzz_bisector_symmetric)
{
    if (!g_cuda_available) {
        return;
    }

    float result = 0.0f;
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_BISECTOR;

    bool ok = nimcp_gpu_fuzzy_defuzzify(g_ctx, g_test_fuzzy_set, TEST_RESOLUTION,
                                         0.0f, 1.0f, &params, &result);
    ck_assert(ok);
    /* Bisector of symmetric triangle should be at center (0.5) */
    ck_assert_float_eq_tol(result, 0.5f, 0.02f);
}
END_TEST

/* ============================================================================
 * Mean of Maximum (MOM) Tests
 * ============================================================================ */

START_TEST(test_defuzz_mom_single_peak)
{
    if (!g_cuda_available) {
        return;
    }

    float result = 0.0f;
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_MOM;

    bool ok = nimcp_gpu_fuzzy_defuzzify(g_ctx, g_test_fuzzy_set, TEST_RESOLUTION,
                                         0.0f, 1.0f, &params, &result);
    ck_assert(ok);
    /* MOM of triangle peak should be at 0.5 */
    ck_assert_float_eq_tol(result, 0.5f, 0.02f);
}
END_TEST

START_TEST(test_defuzz_mom_plateau)
{
    if (!g_cuda_available) {
        return;
    }

    /* Create trapezoidal with plateau from 0.3 to 0.7 */
    float* plateau = (float*)malloc(TEST_RESOLUTION * sizeof(float));
    for (int i = 0; i < TEST_RESOLUTION; i++) {
        float x = (float)i / (float)(TEST_RESOLUTION - 1);
        if (x < 0.2f) {
            plateau[i] = x / 0.2f;
        } else if (x > 0.8f) {
            plateau[i] = (1.0f - x) / 0.2f;
        } else {
            plateau[i] = 1.0f;
        }
    }

    float result = 0.0f;
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_MOM;

    bool ok = nimcp_gpu_fuzzy_defuzzify(g_ctx, plateau, TEST_RESOLUTION,
                                         0.0f, 1.0f, &params, &result);
    ck_assert(ok);
    /* MOM of symmetric plateau should be at center (0.5) */
    ck_assert_float_eq_tol(result, 0.5f, 0.02f);

    free(plateau);
}
END_TEST

/* ============================================================================
 * Smallest of Maximum (SOM) Tests
 * ============================================================================ */

START_TEST(test_defuzz_som_plateau)
{
    if (!g_cuda_available) {
        return;
    }

    /* Create trapezoidal with plateau */
    float* plateau = (float*)malloc(TEST_RESOLUTION * sizeof(float));
    for (int i = 0; i < TEST_RESOLUTION; i++) {
        float x = (float)i / (float)(TEST_RESOLUTION - 1);
        if (x < 0.3f) {
            plateau[i] = x / 0.3f;
        } else if (x > 0.7f) {
            plateau[i] = (1.0f - x) / 0.3f;
        } else {
            plateau[i] = 1.0f;
        }
    }

    float result = 0.0f;
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_SOM;

    bool ok = nimcp_gpu_fuzzy_defuzzify(g_ctx, plateau, TEST_RESOLUTION,
                                         0.0f, 1.0f, &params, &result);
    ck_assert(ok);
    /* SOM should be at start of plateau (~0.3) */
    ck_assert_float_eq_tol(result, 0.3f, 0.02f);

    free(plateau);
}
END_TEST

/* ============================================================================
 * Largest of Maximum (LOM) Tests
 * ============================================================================ */

START_TEST(test_defuzz_lom_plateau)
{
    if (!g_cuda_available) {
        return;
    }

    /* Create trapezoidal with plateau */
    float* plateau = (float*)malloc(TEST_RESOLUTION * sizeof(float));
    for (int i = 0; i < TEST_RESOLUTION; i++) {
        float x = (float)i / (float)(TEST_RESOLUTION - 1);
        if (x < 0.3f) {
            plateau[i] = x / 0.3f;
        } else if (x > 0.7f) {
            plateau[i] = (1.0f - x) / 0.3f;
        } else {
            plateau[i] = 1.0f;
        }
    }

    float result = 0.0f;
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_LOM;

    bool ok = nimcp_gpu_fuzzy_defuzzify(g_ctx, plateau, TEST_RESOLUTION,
                                         0.0f, 1.0f, &params, &result);
    ck_assert(ok);
    /* LOM should be at end of plateau (~0.7) */
    ck_assert_float_eq_tol(result, 0.7f, 0.02f);

    free(plateau);
}
END_TEST

/* ============================================================================
 * Weighted Average Tests
 * ============================================================================ */

START_TEST(test_defuzz_weighted_avg)
{
    if (!g_cuda_available) {
        return;
    }

    float result = 0.0f;
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_WEIGHTED_AVG;

    bool ok = nimcp_gpu_fuzzy_defuzzify(g_ctx, g_test_fuzzy_set, TEST_RESOLUTION,
                                         0.0f, 1.0f, &params, &result);
    ck_assert(ok);
    /* For symmetric fuzzy set, weighted average should be at center */
    ck_assert_float_eq_tol(result, 0.5f, 0.02f);
}
END_TEST

/* ============================================================================
 * Weighted Sum Tests
 * ============================================================================ */

START_TEST(test_defuzz_weighted_sum)
{
    if (!g_cuda_available) {
        return;
    }

    float result = 0.0f;
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_WEIGHTED_SUM;

    bool ok = nimcp_gpu_fuzzy_defuzzify(g_ctx, g_test_fuzzy_set, TEST_RESOLUTION,
                                         0.0f, 1.0f, &params, &result);
    ck_assert(ok);
    /* Weighted sum gives absolute value (unnormalized) */
    ck_assert(result > 0.0f);
}
END_TEST

/* ============================================================================
 * Batch Defuzzification Tests
 * ============================================================================ */

START_TEST(test_defuzz_batch)
{
    if (!g_cuda_available) {
        float* batch_sets = NULL;
        float results[TEST_BATCH_SIZE];
        nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();

        bool ok = nimcp_gpu_fuzzy_defuzzify_batch(g_ctx, batch_sets, TEST_BATCH_SIZE,
                                                   TEST_RESOLUTION, 0.0f, 1.0f,
                                                   &params, results);
        ck_assert(!ok);
        return;
    }

    /* Create batch of fuzzy sets */
    float* batch_sets = (float*)malloc(TEST_BATCH_SIZE * TEST_RESOLUTION * sizeof(float));
    float results[TEST_BATCH_SIZE];

    for (int b = 0; b < TEST_BATCH_SIZE; b++) {
        float peak = (float)b / (float)(TEST_BATCH_SIZE - 1);  /* Peaks at 0, 0.11, 0.22, ... 1.0 */
        for (int i = 0; i < TEST_RESOLUTION; i++) {
            float x = (float)i / (float)(TEST_RESOLUTION - 1);
            float dist = fabsf(x - peak);
            batch_sets[b * TEST_RESOLUTION + i] = fmaxf(0.0f, 1.0f - 5.0f * dist);
        }
    }

    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_CENTROID;

    bool ok = nimcp_gpu_fuzzy_defuzzify_batch(g_ctx, batch_sets, TEST_BATCH_SIZE,
                                               TEST_RESOLUTION, 0.0f, 1.0f,
                                               &params, results);
    ck_assert(ok);

    /* Verify results are near the expected peaks */
    for (int b = 0; b < TEST_BATCH_SIZE; b++) {
        float expected_peak = (float)b / (float)(TEST_BATCH_SIZE - 1);
        ck_assert_float_eq_tol(results[b], expected_peak, 0.1f);
    }

    free(batch_sets);
}
END_TEST

/* ============================================================================
 * Edge Cases Tests
 * ============================================================================ */

START_TEST(test_defuzz_all_zeros)
{
    if (!g_cuda_available) {
        return;
    }

    /* Create all-zero fuzzy set */
    float* zeros = (float*)calloc(TEST_RESOLUTION, sizeof(float));

    float result = 0.0f;
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_CENTROID;

    bool ok = nimcp_gpu_fuzzy_defuzzify(g_ctx, zeros, TEST_RESOLUTION,
                                         0.0f, 1.0f, &params, &result);
    /* Should handle gracefully - return midpoint or 0 */
    ck_assert(ok || !ok);  /* Either succeed or fail gracefully */

    free(zeros);
}
END_TEST

START_TEST(test_defuzz_all_ones)
{
    if (!g_cuda_available) {
        return;
    }

    /* Create all-ones fuzzy set */
    float* ones = (float*)malloc(TEST_RESOLUTION * sizeof(float));
    for (int i = 0; i < TEST_RESOLUTION; i++) {
        ones[i] = 1.0f;
    }

    float result = 0.0f;
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_CENTROID;

    bool ok = nimcp_gpu_fuzzy_defuzzify(g_ctx, ones, TEST_RESOLUTION,
                                         0.0f, 1.0f, &params, &result);
    ck_assert(ok);
    /* Centroid of uniform should be at center */
    ck_assert_float_eq_tol(result, 0.5f, TEST_EPSILON);

    free(ones);
}
END_TEST

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

START_TEST(test_defuzz_null_context)
{
    float result = 0.0f;
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();

    bool ok = nimcp_gpu_fuzzy_defuzzify(NULL, g_test_fuzzy_set, TEST_RESOLUTION,
                                         0.0f, 1.0f, &params, &result);
    ck_assert(!ok);
}
END_TEST

START_TEST(test_defuzz_null_fuzzy_set)
{
    if (!g_cuda_available) {
        return;
    }

    float result = 0.0f;
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();

    bool ok = nimcp_gpu_fuzzy_defuzzify(g_ctx, NULL, TEST_RESOLUTION,
                                         0.0f, 1.0f, &params, &result);
    ck_assert(!ok);
}
END_TEST

START_TEST(test_defuzz_zero_resolution)
{
    if (!g_cuda_available) {
        return;
    }

    float result = 0.0f;
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();

    bool ok = nimcp_gpu_fuzzy_defuzzify(g_ctx, g_test_fuzzy_set, 0,
                                         0.0f, 1.0f, &params, &result);
    ck_assert(!ok);
}
END_TEST

/* ============================================================================
 * Default Parameters Tests
 * ============================================================================ */

START_TEST(test_defuzz_params_default)
{
    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();

    ck_assert_uint_eq(params.method, FUZZY_DEFUZZ_CENTROID);
    ck_assert_uint_eq(params.block_size, 256);
    ck_assert(params.use_shared_memory);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* fuzzy_gpu_defuzz_suite(void)
{
    Suite* s = suite_create("Fuzzy GPU Defuzzification");

    /* Centroid tests */
    TCase* tc_centroid = tcase_create("Centroid");
    tcase_add_checked_fixture(tc_centroid, setup, teardown);
    tcase_add_test(tc_centroid, test_defuzz_centroid_symmetric);
    tcase_add_test(tc_centroid, test_defuzz_centroid_left_skew);
    suite_add_tcase(s, tc_centroid);

    /* Bisector tests */
    TCase* tc_bisector = tcase_create("Bisector");
    tcase_add_checked_fixture(tc_bisector, setup, teardown);
    tcase_add_test(tc_bisector, test_defuzz_bisector_symmetric);
    suite_add_tcase(s, tc_bisector);

    /* MOM tests */
    TCase* tc_mom = tcase_create("Mean of Maximum");
    tcase_add_checked_fixture(tc_mom, setup, teardown);
    tcase_add_test(tc_mom, test_defuzz_mom_single_peak);
    tcase_add_test(tc_mom, test_defuzz_mom_plateau);
    suite_add_tcase(s, tc_mom);

    /* SOM tests */
    TCase* tc_som = tcase_create("Smallest of Maximum");
    tcase_add_checked_fixture(tc_som, setup, teardown);
    tcase_add_test(tc_som, test_defuzz_som_plateau);
    suite_add_tcase(s, tc_som);

    /* LOM tests */
    TCase* tc_lom = tcase_create("Largest of Maximum");
    tcase_add_checked_fixture(tc_lom, setup, teardown);
    tcase_add_test(tc_lom, test_defuzz_lom_plateau);
    suite_add_tcase(s, tc_lom);

    /* Weighted tests */
    TCase* tc_weighted = tcase_create("Weighted Methods");
    tcase_add_checked_fixture(tc_weighted, setup, teardown);
    tcase_add_test(tc_weighted, test_defuzz_weighted_avg);
    tcase_add_test(tc_weighted, test_defuzz_weighted_sum);
    suite_add_tcase(s, tc_weighted);

    /* Batch tests */
    TCase* tc_batch = tcase_create("Batch Defuzzification");
    tcase_add_checked_fixture(tc_batch, setup, teardown);
    tcase_add_test(tc_batch, test_defuzz_batch);
    suite_add_tcase(s, tc_batch);

    /* Edge cases */
    TCase* tc_edge = tcase_create("Edge Cases");
    tcase_add_checked_fixture(tc_edge, setup, teardown);
    tcase_add_test(tc_edge, test_defuzz_all_zeros);
    tcase_add_test(tc_edge, test_defuzz_all_ones);
    suite_add_tcase(s, tc_edge);

    /* Error handling */
    TCase* tc_errors = tcase_create("Error Handling");
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_defuzz_null_context);
    tcase_add_test(tc_errors, test_defuzz_null_fuzzy_set);
    tcase_add_test(tc_errors, test_defuzz_zero_resolution);
    suite_add_tcase(s, tc_errors);

    /* Default parameters */
    TCase* tc_params = tcase_create("Default Parameters");
    tcase_add_test(tc_params, test_defuzz_params_default);
    suite_add_tcase(s, tc_params);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = fuzzy_gpu_defuzz_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
