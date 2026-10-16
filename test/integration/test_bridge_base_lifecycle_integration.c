/**
 * @file test_bridge_base_lifecycle_integration.c
 * @brief Integration test: bridge_base_init/cleanup in all SNN and physics bridges
 *
 * WHAT: Create a TINY brain, run a few training steps, destroy it cleanly
 * WHY:  Exercises all 46 SNN + physics bridges that were fixed to call
 *       bridge_base_init() in create and bridge_base_cleanup() in destroy.
 *       A crash (SIGSEGV / double-free / use-after-free) during destroy
 *       indicates a bridge was not properly initialized or cleaned up.
 * HOW:  1. nimcp_init()
 *       2. nimcp_brain_create(NIMCP_BRAIN_TINY) -- wires all bridges
 *       3. Run 3 training steps (activates bridge paths)
 *       4. nimcp_brain_destroy() -- calls all bridge destroy functions
 *       5. Repeat create/destroy cycle to catch stale global state
 *       6. nimcp_shutdown()
 *
 * RESOURCE_LOCK: brain_heavy (allocates full brain)
 *
 * @date 2026-03-05
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "nimcp.h"
#include "api/nimcp_api_internal.h"

/*=============================================================================
 * Test Fixtures
 *=============================================================================*/

static void setup(void)
{
    nimcp_init();
}

static void teardown(void)
{
    nimcp_shutdown();
}

/*=============================================================================
 * Test: Single create/train/destroy cycle
 *=============================================================================*/

START_TEST(test_bridge_base_single_lifecycle)
{
    /* Create brain -- this wires ALL SNN and physics bridges */
    nimcp_brain_t brain = nimcp_brain_create(
        "bridge_base_test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        4,   /* num_inputs */
        2    /* num_outputs */
    );
    ck_assert_msg(brain != NULL, "Brain creation failed");
    ck_assert_ptr_nonnull(brain->internal_brain);

    /* Configure training so bridges get exercised */
    nimcp_training_config_t config = nimcp_training_config_default();
    config.network_type = NIMCP_NETWORK_ADAPTIVE;
    config.learning_rate = 0.01F;

    nimcp_status_t status = nimcp_brain_configure_training(brain, &config);
    ck_assert_int_eq(status, NIMCP_OK);

    /* Run 3 training steps to activate bridge pathways */
    float inputs[4]  = {0.8F, 0.2F, 0.3F, 0.7F};
    float targets[2] = {1.0F, 0.0F};

    for (int i = 0; i < 3; i++) {
        nimcp_training_result_t result;
        status = nimcp_brain_train_step(brain, inputs, 4, targets, 2, &result);
        ck_assert_int_eq(status, NIMCP_OK);
        ck_assert_msg(!isnan(result.loss), "NaN loss at step %d", i);
        ck_assert_msg(!isinf(result.loss), "Inf loss at step %d", i);
    }

    /* Destroy -- exercises bridge_base_cleanup() in all bridges.
     * If any bridge was not properly initialized, this will crash. */
    nimcp_brain_destroy(brain);
}
END_TEST

/*=============================================================================
 * Test: Two sequential create/destroy cycles
 *=============================================================================*/

START_TEST(test_bridge_base_double_lifecycle)
{
    /* Two cycles catches stale global state / double-free bugs */
    for (int round = 0; round < 2; round++) {
        nimcp_brain_t brain = nimcp_brain_create(
            "bridge_base_double",
            NIMCP_BRAIN_TINY,
            NIMCP_TASK_CLASSIFICATION,
            2, 1
        );
        ck_assert_msg(brain != NULL,
                      "Brain creation failed on round %d", round);

        nimcp_training_config_t config = nimcp_training_config_default();
        config.network_type = NIMCP_NETWORK_ADAPTIVE;
        config.learning_rate = 0.01F;
        nimcp_brain_configure_training(brain, &config);

        float inp[2] = {0.5F, 0.5F};
        float tgt[1] = {1.0F};
        nimcp_training_result_t result;

        for (int i = 0; i < 3; i++) {
            nimcp_status_t s = nimcp_brain_train_step(
                brain, inp, 2, tgt, 1, &result);
            ck_assert_int_eq(s, NIMCP_OK);
        }

        nimcp_brain_destroy(brain);
    }
}
END_TEST

/*=============================================================================
 * Suite Creation
 *=============================================================================*/

Suite* bridge_base_lifecycle_suite(void)
{
    Suite* s = suite_create("Bridge Base Lifecycle Integration");

    TCase* tc_single = tcase_create("Single Lifecycle");
    tcase_add_checked_fixture(tc_single, setup, teardown);
    tcase_set_timeout(tc_single, 300);
    tcase_add_test(tc_single, test_bridge_base_single_lifecycle);
    suite_add_tcase(s, tc_single);

    TCase* tc_double = tcase_create("Double Lifecycle");
    tcase_add_checked_fixture(tc_double, setup, teardown);
    tcase_set_timeout(tc_double, 600);
    tcase_add_test(tc_double, test_bridge_base_double_lifecycle);
    suite_add_tcase(s, tc_double);

    return s;
}

/*=============================================================================
 * Main
 *=============================================================================*/

int main(void)
{
    int number_failed;
    Suite* s = bridge_base_lifecycle_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
