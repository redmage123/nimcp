/**
 * @file test_full_brain_lifecycle_e2e.c
 * @brief E2E test: full brain lifecycle with all 7 new subsystems
 *
 * WHAT: Create brain -> 5 training steps -> verify subsystems -> destroy
 * WHY:  End-to-end validation that the full lifecycle works with all
 *       7 new subsystems integrated — catches wiring bugs, initialization
 *       order issues, and shutdown crashes
 * HOW:  1. Create brain (TINY size to limit memory)
 *       2. Verify all 7 subsystem fields are non-NULL
 *       3. Run 5 training steps (classification task)
 *       4. Verify no NaN/Inf in loss
 *       5. Destroy brain cleanly
 *
 * RESOURCE_LOCK: brain_heavy (allocates full brain)
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
#include "api/nimcp_api_internal.h"
#include "core/brain/nimcp_brain_internal.h"

/*=============================================================================
 * Test Fixtures
 *=============================================================================*/

static nimcp_brain_t test_brain = NULL;

static void setup_e2e(void)
{
    nimcp_init();
    test_brain = nimcp_brain_create(
        "e2e_lifecycle_brain",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        4,   /* num_inputs */
        2    /* num_outputs */
    );
}

static void teardown_e2e(void)
{
    if (test_brain) {
        nimcp_brain_destroy(test_brain);
        test_brain = NULL;
    }
    nimcp_shutdown();
}

/* Fixture for sequential test — just init/shutdown, no brain */
static void setup_seq(void)
{
    nimcp_init();
}

static void teardown_seq(void)
{
    nimcp_shutdown();
}

/*=============================================================================
 * Test: Full lifecycle
 *=============================================================================*/

START_TEST(test_full_lifecycle)
{
    /* ---- Phase 1: Verify brain was created ---- */
    ck_assert_ptr_nonnull(test_brain);
    ck_assert_ptr_nonnull(test_brain->internal_brain);

    brain_t b = test_brain->internal_brain;

    /* ---- Phase 2: Verify all 7 subsystems initialized ---- */
    ck_assert_msg(b->white_matter != NULL,
                  "E2E: white_matter not initialized");
    ck_assert_msg(b->inferior_colliculus != NULL,
                  "E2E: inferior_colliculus not initialized");
    ck_assert_msg(b->spinal_cord != NULL,
                  "E2E: spinal_cord not initialized");
    ck_assert_msg(b->cortical_interneurons != NULL,
                  "E2E: cortical_interneurons not initialized");
    ck_assert_msg(b->neuropeptide != NULL,
                  "E2E: neuropeptide not initialized");
    ck_assert_msg(b->endocannabinoid != NULL,
                  "E2E: endocannabinoid not initialized");
    ck_assert_msg(b->glymphatic != NULL,
                  "E2E: glymphatic not initialized");

    /* ---- Phase 3: Configure and run 5 training steps ---- */
    nimcp_training_config_t config = nimcp_training_config_default();
    config.network_type = NIMCP_NETWORK_ADAPTIVE;
    config.learning_rate = 0.01F;

    nimcp_status_t status = nimcp_brain_configure_training(test_brain, &config);
    ck_assert_int_eq(status, NIMCP_OK);

    float inputs_a[4] = {0.9F, 0.1F, 0.2F, 0.8F};
    float inputs_b[4] = {0.1F, 0.8F, 0.9F, 0.2F};
    float targets_a[2] = {1.0F, 0.0F};
    float targets_b[2] = {0.0F, 1.0F};

    float first_loss = 0.0F;
    float last_loss = 0.0F;

    for (int i = 0; i < 5; i++) {
        nimcp_training_result_t result;
        float* inp = (i % 2 == 0) ? inputs_a : inputs_b;
        float* tgt = (i % 2 == 0) ? targets_a : targets_b;

        status = nimcp_brain_train_step(test_brain, inp, 4, tgt, 2, &result);
        ck_assert_int_eq(status, NIMCP_OK);

        ck_assert_msg(!isnan(result.loss),
                      "E2E: NaN loss at step %d", i);
        ck_assert_msg(!isinf(result.loss),
                      "E2E: Inf loss at step %d", i);
        ck_assert_msg(result.loss >= 0.0F,
                      "E2E: negative loss at step %d: %f", i, (double)result.loss);

        if (i == 0) {
            first_loss = result.loss;
        }
        last_loss = result.loss;
    }

    /* ---- Phase 4: Verify training stats are accessible ---- */
    uint64_t total_steps = 0;
    float total_loss = 0.0F;
    float current_lr = 0.0F;

    status = nimcp_brain_get_training_stats(test_brain, &total_steps, &total_loss, &current_lr);
    ck_assert_int_eq(status, NIMCP_OK);
    ck_assert_msg(total_steps >= 5,
                  "E2E: expected >= 5 steps, got %lu", (unsigned long)total_steps);
    ck_assert(!isnan(total_loss));
    ck_assert(!isinf(total_loss));

    /* ---- Phase 5: Verify subsystems still valid after training ---- */
    ck_assert_ptr_nonnull(b->white_matter);
    ck_assert_ptr_nonnull(b->glymphatic);
    ck_assert_ptr_nonnull(b->neuropeptide);

    /* Phase 6: Destroy happens in teardown — test passes if no SIGSEGV */
}
END_TEST

/*=============================================================================
 * Test: Create and destroy multiple brains sequentially
 *=============================================================================*/

START_TEST(test_sequential_brain_lifecycle)
{
    /* WHAT: Create and destroy 2 brains in sequence
     * WHY:  Catches static/global state corruption between brain lifecycles
     */
    for (int round = 0; round < 2; round++) {
        nimcp_brain_t brain = nimcp_brain_create(
            "seq_lifecycle_brain",
            NIMCP_BRAIN_TINY,
            NIMCP_TASK_CLASSIFICATION,
            2, 1
        );
        ck_assert_ptr_nonnull(brain);

        /* Quick training step */
        nimcp_training_config_t config = nimcp_training_config_default();
        config.network_type = NIMCP_NETWORK_ADAPTIVE;
        config.learning_rate = 0.01F;
        nimcp_brain_configure_training(brain, &config);

        float inp[2] = {0.5F, 0.5F};
        float tgt[1] = {1.0F};
        nimcp_training_result_t result;

        for (int i = 0; i < 5; i++) {
            nimcp_status_t status = nimcp_brain_train_step(
                brain, inp, 2, tgt, 1, &result);
            ck_assert_int_eq(status, NIMCP_OK);
            ck_assert(!isnan(result.loss));
        }

        nimcp_brain_destroy(brain);
    }
}
END_TEST

/*=============================================================================
 * Suite Creation
 *=============================================================================*/

Suite* full_brain_lifecycle_e2e_suite(void)
{
    Suite* s = suite_create("Full Brain Lifecycle E2E");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup_e2e, teardown_e2e);
    tcase_set_timeout(tc_lifecycle, 600);
    tcase_add_test(tc_lifecycle, test_full_lifecycle);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_sequential = tcase_create("Sequential Lifecycle");
    tcase_add_checked_fixture(tc_sequential, setup_seq, teardown_seq);
    tcase_set_timeout(tc_sequential, 600);
    tcase_add_test(tc_sequential, test_sequential_brain_lifecycle);
    suite_add_tcase(s, tc_sequential);

    return s;
}

/*=============================================================================
 * Main
 *=============================================================================*/

int main(void)
{
    int number_failed;
    Suite* s = full_brain_lifecycle_e2e_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
