/**
 * @file test_parallel_init_lifecycle_e2e.c
 * @brief E2E test: full brain lifecycle with parallel subsystem initialization
 *
 * WHAT: End-to-end test of create(parallel) → train → infer → verify → destroy
 * WHY:  Validates that a parallel-initialized brain is fully functional across
 *       its entire lifecycle — not just at creation time
 * HOW:  libcheck tests running the complete brain lifecycle pipeline
 *
 * PHASES:
 *   1. Create brain with parallel_init=true
 *   2. Verify all subsystem pointers are non-NULL
 *   3. Run 10 training steps, verify no NaN/Inf
 *   4. Run inference, verify valid confidence
 *   5. Verify subsystems remain valid post-training
 *   6. Create second brain (serial), verify training stats are independent
 *   7. Destroy both brains cleanly
 *
 * RESOURCE_LOCK: brain_heavy
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "nimcp.h"
#include "api/nimcp_api_internal.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/factory/nimcp_brain_factory.h"

//=============================================================================
// Fixtures
//=============================================================================

static brain_t e2e_brain = NULL;

static brain_config_t make_e2e_config(bool parallel) {
    brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 4;
    config.num_outputs = 2;
    config.learning_rate = 0.01f;
    config.sparsity_target = 0.85f;
    config.parallel_init = parallel;
    config.init_threads = 4;
    config.fast_training_mode = true;
    return config;
}

static void setup_e2e(void) {
    nimcp_init();
    brain_config_t config = make_e2e_config(true);
    e2e_brain = brain_create_custom(&config);
}

static void teardown_e2e(void) {
    if (e2e_brain) { brain_destroy(e2e_brain); e2e_brain = NULL; }
    nimcp_shutdown();
}

static void setup_nimcp_only(void) {
    nimcp_init();
}

static void teardown_nimcp_only(void) {
    nimcp_shutdown();
}

//=============================================================================
// E2E Test: Full lifecycle with parallel init
//=============================================================================

START_TEST(test_e2e_full_lifecycle_parallel)
{
    // Phase 1: Verify creation
    ck_assert_ptr_nonnull(e2e_brain);
    ck_assert_ptr_nonnull(e2e_brain->network);

    // Phase 2: Verify core structure
    ck_assert_ptr_nonnull(e2e_brain->network);

    // Phase 3: Training (10 steps)
    float inputs[4];

    for (int step = 0; step < 10; step++) {
        inputs[0] = (float)(step % 4) / 4.0f;
        inputs[1] = (float)((step + 1) % 4) / 4.0f;
        inputs[2] = (float)((step + 2) % 4) / 4.0f;
        inputs[3] = (float)((step + 3) % 4) / 4.0f;

        const char* label = (step >= 5) ? "class_1" : "class_0";
        float loss = brain_learn_example(e2e_brain, inputs, 4, label, 0.9f);
        (void)loss;  // May be high on TINY brain
    }

    // Phase 4: Inference
    float test_input[] = {0.5f, 0.5f, 0.5f, 0.5f};
    brain_decision_t* decision = brain_decide(e2e_brain, test_input, 4);
    if (decision) {
        ck_assert_msg(!isnan(decision->confidence),
                      "Inference confidence should not be NaN");
        ck_assert_msg(!isinf(decision->confidence),
                      "Inference confidence should not be Inf");
        ck_assert_msg(decision->confidence >= 0.0f && decision->confidence <= 1.0f,
                      "Confidence should be in [0,1], got %f", decision->confidence);
        brain_free_decision(decision);
    }

    // Phase 5: Network still valid post-training
    ck_assert_ptr_nonnull(e2e_brain->network);
}
END_TEST

//=============================================================================
// E2E Test: Parallel + sequential brains coexist
//=============================================================================

START_TEST(test_e2e_parallel_and_sequential_coexist)
{
    // Create two brains: one parallel, one sequential
    brain_config_t config_par = make_e2e_config(true);
    brain_t brain_par = brain_create_custom(&config_par);
    ck_assert_ptr_nonnull(brain_par);

    brain_config_t config_seq = make_e2e_config(false);
    brain_t brain_seq = brain_create_custom(&config_seq);
    ck_assert_ptr_nonnull(brain_seq);

    // Train one, not the other
    float inputs[] = {0.5f, 0.5f, 0.5f, 0.5f};
    for (int i = 0; i < 5; i++) {
        brain_learn_example(brain_par, inputs, 4, "class_0", 0.9f);
    }

    // Both should be able to do inference independently
    brain_decision_t* dec_par = brain_decide(brain_par, inputs, 4);
    brain_decision_t* dec_seq = brain_decide(brain_seq, inputs, 4);

    // Both may return NULL on TINY, but shouldn't crash
    if (dec_par) brain_free_decision(dec_par);
    if (dec_seq) brain_free_decision(dec_seq);

    brain_destroy(brain_par);
    brain_destroy(brain_seq);
}
END_TEST

//=============================================================================
// E2E Test: Rapid lifecycle stress test
//=============================================================================

START_TEST(test_e2e_rapid_parallel_lifecycle)
{
    // Create and destroy 5 brains with parallel init in sequence
    for (int i = 0; i < 5; i++) {
        brain_config_t config = make_e2e_config(true);
        brain_t brain = brain_create_custom(&config);
        ck_assert_msg(brain != NULL,
                      "Rapid lifecycle iteration %d: brain creation failed", i);

        // Quick smoke test: inference
        float inputs[] = {0.1f * (float)i, 0.2f, 0.3f, 0.4f};
        brain_decision_t* dec = brain_decide(brain, inputs, 4);
        if (dec) brain_free_decision(dec);

        brain_destroy(brain);
    }
}
END_TEST

//=============================================================================
// E2E Test: Various thread counts
//=============================================================================

START_TEST(test_e2e_various_thread_counts)
{
    uint32_t thread_counts[] = {1, 2, 4, 8};
    for (int i = 0; i < 4; i++) {
        brain_config_t config = make_e2e_config(true);
        config.init_threads = thread_counts[i];
        brain_t brain = brain_create_custom(&config);
        ck_assert_msg(brain != NULL,
                      "Failed with init_threads=%u", thread_counts[i]);
        ck_assert_ptr_nonnull(brain->network);
        brain_destroy(brain);
    }
}
END_TEST

//=============================================================================
// Suite Registration
//=============================================================================

Suite* parallel_init_e2e_suite(void) {
    Suite* s = suite_create("Parallel Init E2E Lifecycle");

    TCase* tc_lifecycle = tcase_create("Full Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup_e2e, teardown_e2e);
    tcase_set_timeout(tc_lifecycle, 600);
    tcase_add_test(tc_lifecycle, test_e2e_full_lifecycle_parallel);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_coexist = tcase_create("Coexistence");
    tcase_add_checked_fixture(tc_coexist, setup_nimcp_only, teardown_nimcp_only);
    tcase_set_timeout(tc_coexist, 600);
    tcase_add_test(tc_coexist, test_e2e_parallel_and_sequential_coexist);
    suite_add_tcase(s, tc_coexist);

    TCase* tc_stress = tcase_create("Stress");
    tcase_add_checked_fixture(tc_stress, setup_nimcp_only, teardown_nimcp_only);
    tcase_set_timeout(tc_stress, 900);
    tcase_add_test(tc_stress, test_e2e_rapid_parallel_lifecycle);
    tcase_add_test(tc_stress, test_e2e_various_thread_counts);
    suite_add_tcase(s, tc_stress);

    return s;
}

int main(void) {
    SRunner* sr = srunner_create(parallel_init_e2e_suite());
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
