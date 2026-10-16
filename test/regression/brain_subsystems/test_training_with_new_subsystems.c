/**
 * @file test_training_with_new_subsystems.c
 * @brief Regression test: training with all 7 new subsystems active
 *
 * WHAT: Verify brain training produces valid loss values with all new subsystems
 * WHY:  New subsystems may introduce NaN/Inf in the training path if their
 *       modulation signals are uninitialized or their update functions corrupt
 *       shared state
 * HOW:  Create brain, run 20 learn steps, assert no NaN/Inf in loss,
 *       verify brain stats are accessible
 *
 * ITERATION COUNT: 20 (kept low for parallel ctest — RESOURCE_LOCK "brain_heavy")
 *
 * @author NIMCP Test Team
 * @date 2026-03-05
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "nimcp.h"

/*=============================================================================
 * Test Fixtures
 *=============================================================================*/

static nimcp_brain_t test_brain = NULL;

static void setup_brain(void)
{
    nimcp_init();
    test_brain = nimcp_brain_create(
        "training_subsystem_regression",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        4,   /* num_inputs */
        2    /* num_outputs */
    );
}

static void teardown_brain(void)
{
    if (test_brain) {
        nimcp_brain_destroy(test_brain);
        test_brain = NULL;
    }
    nimcp_shutdown();
}

/*=============================================================================
 * Test 1: Training produces valid loss (no NaN/Inf)
 *=============================================================================*/

START_TEST(test_training_loss_valid)
{
    /* WHAT: Run 20 training steps and verify loss is finite
     * WHY:  New subsystems may inject NaN into the training gradient path
     * REGRESSION: Any NaN or Inf in loss indicates broken modulation
     */
    ck_assert_ptr_nonnull(test_brain);

    /* Configure training */
    nimcp_training_config_t config = nimcp_training_config_default();
    config.network_type = NIMCP_NETWORK_ADAPTIVE;
    config.learning_rate = 0.01F;

    nimcp_status_t status = nimcp_brain_configure_training(test_brain, &config);
    ck_assert_int_eq(status, NIMCP_OK);

    /* Simple binary classification data */
    float inputs_a[4] = {0.8F, 0.2F, 0.1F, 0.9F};
    float inputs_b[4] = {0.1F, 0.9F, 0.8F, 0.2F};
    float targets_a[2] = {1.0F, 0.0F};
    float targets_b[2] = {0.0F, 1.0F};

    /* Run 20 training steps (keep low for parallel ctest) */
    for (int i = 0; i < 20; i++) {
        nimcp_training_result_t result;
        float* inputs = (i % 2 == 0) ? inputs_a : inputs_b;
        float* targets = (i % 2 == 0) ? targets_a : targets_b;

        status = nimcp_brain_train_step(
            test_brain,
            inputs, 4,
            targets, 2,
            &result
        );
        ck_assert_int_eq(status, NIMCP_OK);

        /* Loss must be a valid finite number */
        ck_assert_msg(!isnan(result.loss),
                      "Training step %d produced NaN loss", i);
        ck_assert_msg(!isinf(result.loss),
                      "Training step %d produced Inf loss", i);

        /* Loss should be non-negative */
        ck_assert_msg(result.loss >= 0.0F,
                      "Training step %d produced negative loss: %f", i, (double)result.loss);
    }
}
END_TEST

/*=============================================================================
 * Test 2: Training stats accessible after learning
 *=============================================================================*/

START_TEST(test_training_stats_accessible)
{
    /* WHAT: After training, verify brain training stats can be retrieved
     * WHY:  New subsystem initialization may corrupt the stats struct
     */
    ck_assert_ptr_nonnull(test_brain);

    nimcp_training_config_t config = nimcp_training_config_default();
    config.network_type = NIMCP_NETWORK_ADAPTIVE;
    config.learning_rate = 0.01F;

    nimcp_status_t status = nimcp_brain_configure_training(test_brain, &config);
    ck_assert_int_eq(status, NIMCP_OK);

    /* Train a few steps */
    float inputs[4] = {0.5F, 0.5F, 0.5F, 0.5F};
    float targets[2] = {1.0F, 0.0F};
    nimcp_training_result_t result;

    for (int i = 0; i < 10; i++) {
        status = nimcp_brain_train_step(test_brain, inputs, 4, targets, 2, &result);
        ck_assert_int_eq(status, NIMCP_OK);
    }

    /* Retrieve training stats */
    uint64_t total_steps = 0;
    float total_loss = 0.0F;
    float current_lr = 0.0F;

    status = nimcp_brain_get_training_stats(test_brain, &total_steps, &total_loss, &current_lr);
    ck_assert_int_eq(status, NIMCP_OK);

    /* Steps should be tracked */
    ck_assert_msg(total_steps > 0,
                  "Expected total_steps > 0 after training, got %lu",
                  (unsigned long)total_steps);

    /* Learning rate should be positive */
    ck_assert_msg(current_lr > 0.0F,
                  "Expected positive learning rate, got %f", (double)current_lr);

    /* Total loss should be finite */
    ck_assert_msg(!isnan(total_loss), "Total loss is NaN");
    ck_assert_msg(!isinf(total_loss), "Total loss is Inf");
}
END_TEST

/*=============================================================================
 * Test 3: Learning rate does not explode
 *=============================================================================*/

START_TEST(test_learning_rate_bounded)
{
    /* WHAT: Verify learning rate stays bounded during training
     * WHY:  New subsystem modulation might cause LR to grow unbounded
     */
    ck_assert_ptr_nonnull(test_brain);

    nimcp_training_config_t config = nimcp_training_config_default();
    config.network_type = NIMCP_NETWORK_ADAPTIVE;
    config.learning_rate = 0.01F;

    nimcp_status_t status = nimcp_brain_configure_training(test_brain, &config);
    ck_assert_int_eq(status, NIMCP_OK);

    float inputs[4] = {0.3F, 0.7F, 0.4F, 0.6F};
    float targets[2] = {0.0F, 1.0F};

    for (int i = 0; i < 20; i++) {
        nimcp_training_result_t result;
        status = nimcp_brain_train_step(test_brain, inputs, 4, targets, 2, &result);
        ck_assert_int_eq(status, NIMCP_OK);

        /* Learning rate should not explode */
        ck_assert_msg(result.learning_rate < 100.0F,
                      "Learning rate exploded at step %d: %f",
                      i, (double)result.learning_rate);

        /* Learning rate should be positive */
        ck_assert_msg(result.learning_rate > 0.0F || result.learning_rate == 0.0F,
                      "Learning rate negative at step %d: %f",
                      i, (double)result.learning_rate);
    }
}
END_TEST

/*=============================================================================
 * Test 4: Multiple training rounds with different data
 *=============================================================================*/

START_TEST(test_training_multiple_patterns)
{
    /* WHAT: Train on multiple patterns to exercise subsystem interactions
     * WHY:  Some bugs only manifest when different activation patterns are seen
     */
    ck_assert_ptr_nonnull(test_brain);

    nimcp_training_config_t config = nimcp_training_config_default();
    config.network_type = NIMCP_NETWORK_ADAPTIVE;
    config.learning_rate = 0.01F;

    nimcp_status_t status = nimcp_brain_configure_training(test_brain, &config);
    ck_assert_int_eq(status, NIMCP_OK);

    /* 4 distinct patterns */
    float patterns[4][4] = {
        {1.0F, 0.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 0.0F, 1.0F}
    };
    float targets[4][2] = {
        {1.0F, 0.0F},
        {1.0F, 0.0F},
        {0.0F, 1.0F},
        {0.0F, 1.0F}
    };

    float last_loss = 0.0F;
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int p = 0; p < 4; p++) {
            nimcp_training_result_t result;
            status = nimcp_brain_train_step(
                test_brain,
                patterns[p], 4,
                targets[p], 2,
                &result
            );
            ck_assert_int_eq(status, NIMCP_OK);
            ck_assert(!isnan(result.loss));
            ck_assert(!isinf(result.loss));
            last_loss = result.loss;
        }
    }

    /* Loss should be reasonable (not exploding) */
    ck_assert_msg(last_loss < 1000.0F,
                  "Loss seems unreasonably large: %f", (double)last_loss);
}
END_TEST

/*=============================================================================
 * Suite Creation
 *=============================================================================*/

Suite* training_subsystems_regression_suite(void)
{
    Suite* s = suite_create("Training With New Subsystems Regression");

    TCase* tc_training = tcase_create("Training Validity");
    tcase_add_checked_fixture(tc_training, setup_brain, teardown_brain);
    tcase_set_timeout(tc_training, 120);
    tcase_add_test(tc_training, test_training_loss_valid);
    tcase_add_test(tc_training, test_training_stats_accessible);
    tcase_add_test(tc_training, test_learning_rate_bounded);
    tcase_add_test(tc_training, test_training_multiple_patterns);
    suite_add_tcase(s, tc_training);

    return s;
}

/*=============================================================================
 * Main
 *=============================================================================*/

int main(void)
{
    int number_failed;
    Suite* s = training_subsystems_regression_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
