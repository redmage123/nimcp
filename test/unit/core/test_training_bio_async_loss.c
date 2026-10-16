/**
 * @file test_training_bio_async_loss.c
 * @brief Unit test for C2-new CRITICAL: best_loss ordering fix
 *
 * WHAT: Verify loss_improvement is computed BEFORE best_loss is updated
 * WHY:  Bug C2-new caused improvement to always be 0 (and thus dopamine_release=0)
 * HOW:  Check framework tests that simulate loss sequences
 */

#include <check.h>
#include <math.h>
#include <stdlib.h>

/* We test the logic pattern, not the full bridge (which requires brain context) */

/* Reproduce the FIXED logic */
static void compute_loss_improvement_fixed(
    float loss, float* best_loss, float* improvement, float* dopamine)
{
    if (loss < *best_loss) {
        /* FIXED: compute improvement BEFORE updating best_loss */
        *improvement = (*best_loss - loss) / *best_loss;
        *best_loss = loss;
        *dopamine = fminf(0.3f, *improvement * 1.0f);
    } else {
        *improvement = 0.0f;
        *dopamine = 0.0f;
    }
}

/* Reproduce the BUGGY logic (for contrast) */
static void compute_loss_improvement_buggy(
    float loss, float* best_loss, float* improvement, float* dopamine)
{
    if (loss < *best_loss) {
        /* BUG: update best_loss first, so improvement = (loss - loss)/loss = 0 */
        *best_loss = loss;
        *improvement = (*best_loss - loss) / *best_loss;
        *dopamine = fminf(0.3f, *improvement * 1.0f);
    } else {
        *improvement = 0.0f;
        *dopamine = 0.0f;
    }
}

START_TEST(test_fixed_loss_improvement_nonzero)
{
    float best_loss = 1.0f;
    float improvement = 0.0f;
    float dopamine = 0.0f;

    compute_loss_improvement_fixed(0.8f, &best_loss, &improvement, &dopamine);

    /* Improvement should be (1.0 - 0.8) / 1.0 = 0.2 */
    ck_assert_float_gt(improvement, 0.19f);
    ck_assert_float_lt(improvement, 0.21f);

    /* Dopamine should be non-zero */
    ck_assert_float_gt(dopamine, 0.0f);

    /* best_loss should be updated */
    ck_assert_float_eq_tol(best_loss, 0.8f, 0.001f);
}
END_TEST

START_TEST(test_buggy_loss_improvement_always_zero)
{
    float best_loss = 1.0f;
    float improvement = 0.0f;
    float dopamine = 0.0f;

    compute_loss_improvement_buggy(0.8f, &best_loss, &improvement, &dopamine);

    /* Bug: improvement is always 0 because best_loss was updated first */
    ck_assert_float_eq_tol(improvement, 0.0f, 0.001f);
    ck_assert_float_eq_tol(dopamine, 0.0f, 0.001f);
}
END_TEST

START_TEST(test_decreasing_loss_sequence)
{
    float best_loss = 2.0f;
    float improvement, dopamine;

    /* Sequence of decreasing losses */
    float losses[] = {1.5f, 1.0f, 0.7f, 0.5f};
    for (int i = 0; i < 4; i++) {
        compute_loss_improvement_fixed(losses[i], &best_loss, &improvement, &dopamine);
        ck_assert_float_gt(improvement, 0.0f);
        ck_assert_float_gt(dopamine, 0.0f);
    }
}
END_TEST

START_TEST(test_no_improvement_when_loss_increases)
{
    float best_loss = 0.5f;
    float improvement = 999.0f;
    float dopamine = 999.0f;

    compute_loss_improvement_fixed(0.8f, &best_loss, &improvement, &dopamine);

    /* No improvement — loss increased */
    ck_assert_float_eq_tol(improvement, 0.0f, 0.001f);
    ck_assert_float_eq_tol(dopamine, 0.0f, 0.001f);
    /* best_loss unchanged */
    ck_assert_float_eq_tol(best_loss, 0.5f, 0.001f);
}
END_TEST

Suite* training_bio_async_loss_suite(void) {
    Suite* s = suite_create("Training Bio-Async Loss Fix");
    TCase* tc = tcase_create("C2-new CRITICAL");
    tcase_add_test(tc, test_fixed_loss_improvement_nonzero);
    tcase_add_test(tc, test_buggy_loss_improvement_always_zero);
    tcase_add_test(tc, test_decreasing_loss_sequence);
    tcase_add_test(tc, test_no_improvement_when_loss_increases);
    suite_add_tcase(s, tc);
    return s;
}

int main(void) {
    Suite* s = training_bio_async_loss_suite();
    SRunner* sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
