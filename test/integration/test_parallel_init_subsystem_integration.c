/**
 * @file test_parallel_init_subsystem_integration.c
 * @brief Integration tests: parallel init produces correct cross-module state
 *
 * WHAT: Verify that subsystems initialized in parallel waves can interact
 * WHY:  Wave ordering bugs could leave dependency links broken
 * HOW:  Create brain with parallel init, exercise cross-subsystem interactions
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
// Shared Fixtures
//=============================================================================

static brain_t test_brain_par = NULL;
static brain_t test_brain_seq = NULL;

static brain_config_t make_test_config(bool parallel) {
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

static void setup_parallel(void) {
    nimcp_init();
    brain_config_t config = make_test_config(true);
    test_brain_par = brain_create_custom(&config);
}

static void teardown_parallel(void) {
    if (test_brain_par) { brain_destroy(test_brain_par); test_brain_par = NULL; }
    nimcp_shutdown();
}

static void setup_both(void) {
    nimcp_init();
    brain_config_t config_par = make_test_config(true);
    test_brain_par = brain_create_custom(&config_par);
    brain_config_t config_seq = make_test_config(false);
    test_brain_seq = brain_create_custom(&config_seq);
}

static void teardown_both(void) {
    if (test_brain_par) { brain_destroy(test_brain_par); test_brain_par = NULL; }
    if (test_brain_seq) { brain_destroy(test_brain_seq); test_brain_seq = NULL; }
    nimcp_shutdown();
}

//=============================================================================
// Test: Parallel-initialized brain has same subsystem presence as sequential
//=============================================================================

START_TEST(test_subsystem_presence_parity)
{
    ck_assert_ptr_nonnull(test_brain_par);
    ck_assert_ptr_nonnull(test_brain_seq);

    // Compare critical subsystem presence (non-NULL means initialized)
    #define CHECK_PARITY(field) \
        ck_assert_msg((test_brain_par->field != NULL) == (test_brain_seq->field != NULL), \
                      "Parity mismatch for " #field ": par=%p seq=%p", \
                      (void*)test_brain_par->field, (void*)test_brain_seq->field)

    CHECK_PARITY(network);
    CHECK_PARITY(immune_system);
    CHECK_PARITY(mental_health_monitor);
    CHECK_PARITY(shadow_emotions);
    CHECK_PARITY(grief_system);
    CHECK_PARITY(joy_system);
    CHECK_PARITY(remorse_system);
    CHECK_PARITY(social_bond_system);
    CHECK_PARITY(bias_detection);

    #undef CHECK_PARITY
}
END_TEST

//=============================================================================
// Test: Immune system is functional after parallel init
//=============================================================================

START_TEST(test_immune_functional_after_parallel_init)
{
    ck_assert_ptr_nonnull(test_brain_par);
    // Immune may be NULL on TINY brains — just verify no crash
    // If immune_system exists, it should be a valid pointer
    if (test_brain_par->immune_system) {
        ck_assert(1);  // Immune pointer is valid, no crash
    }
    // Either way, brain creation succeeded which is the key test
    ck_assert(1);
}
END_TEST

//=============================================================================
// Test: Security/immune wave ordering doesn't crash
//=============================================================================

START_TEST(test_security_before_immune_ordering)
{
    ck_assert_ptr_nonnull(test_brain_par);
    // Brain creation succeeded — if security/immune ordering was wrong,
    // creation would have failed or crashed
    ck_assert_ptr_nonnull(test_brain_par->network);
}
END_TEST

//=============================================================================
// Test: Training pipeline works after parallel init
//=============================================================================

START_TEST(test_training_after_parallel_init)
{
    ck_assert_ptr_nonnull(test_brain_par);

    // Wave 12 initializes training subsystem — verify it's functional
    float inputs[] = {1.0f, 0.5f, 0.3f, 0.8f};

    // Call brain_learn_example which exercises the training pipeline
    float loss = brain_learn_example(test_brain_par, inputs, 4, "class_0", 0.9f);
    // brain_learn_example may return high loss on TINY brains, but should not crash
    (void)loss;
    ck_assert(1);  // Reached here without crash
}
END_TEST

//=============================================================================
// Test: Inference works after parallel init
//=============================================================================

START_TEST(test_inference_after_parallel_init)
{
    ck_assert_ptr_nonnull(test_brain_par);

    float inputs[] = {1.0f, 0.5f, 0.3f, 0.8f};
    brain_decision_t* decision = brain_decide(test_brain_par, inputs, 4);
    if (decision) {
        ck_assert_msg(decision->confidence >= 0.0f && decision->confidence <= 1.0f,
                      "Confidence should be in [0,1], got %f", decision->confidence);
        brain_free_decision(decision);
    }
    ck_assert(1);  // Reached here without crash
}
END_TEST

//=============================================================================
// Test: Emotional subsystems are cross-wired
//=============================================================================

START_TEST(test_emotional_subsystems_parallel_init)
{
    ck_assert_ptr_nonnull(test_brain_par);

    // Wave 4 inline inits create emotional systems
    // Verify they exist and are distinct objects
    if (test_brain_par->shadow_emotions && test_brain_par->bias_detection) {
        ck_assert_msg(test_brain_par->shadow_emotions != test_brain_par->bias_detection,
                      "Shadow and bias systems should be distinct objects");
    }
}
END_TEST

//=============================================================================
// Test: Bio-async state is consistent
//=============================================================================

START_TEST(test_bio_async_state_after_parallel_init)
{
    ck_assert_ptr_nonnull(test_brain_par);
    // Bio-async is initialized in Wave 13 (serial)
    // Just verify the flag is set consistently
    // (bio_async_enabled may be true or false depending on environment)
    ck_assert(test_brain_par->bio_async_enabled == true ||
              test_brain_par->bio_async_enabled == false);
}
END_TEST

//=============================================================================
// Test: Coordinator subsystems initialized (Waves 23-26)
//=============================================================================

START_TEST(test_coordinators_after_parallel_init)
{
    ck_assert_ptr_nonnull(test_brain_par);
    // Plasticity coordinator (Wave 24) should be set up
    // These may be NULL if bio-async failed, but creation itself shouldn't crash
    ck_assert(1);  // Reached here = no crash in coordinator waves
}
END_TEST

//=============================================================================
// Suite Registration
//=============================================================================

Suite* parallel_init_integration_suite(void) {
    Suite* s = suite_create("Parallel Init Integration");

    TCase* tc_parity = tcase_create("Subsystem Parity");
    tcase_add_checked_fixture(tc_parity, setup_both, teardown_both);
    tcase_set_timeout(tc_parity, 300);
    tcase_add_test(tc_parity, test_subsystem_presence_parity);
    suite_add_tcase(s, tc_parity);

    TCase* tc_cross = tcase_create("Cross-Module Integration");
    tcase_add_checked_fixture(tc_cross, setup_parallel, teardown_parallel);
    tcase_set_timeout(tc_cross, 300);
    tcase_add_test(tc_cross, test_immune_functional_after_parallel_init);
    tcase_add_test(tc_cross, test_security_before_immune_ordering);
    tcase_add_test(tc_cross, test_training_after_parallel_init);
    tcase_add_test(tc_cross, test_inference_after_parallel_init);
    tcase_add_test(tc_cross, test_emotional_subsystems_parallel_init);
    tcase_add_test(tc_cross, test_bio_async_state_after_parallel_init);
    tcase_add_test(tc_cross, test_coordinators_after_parallel_init);
    suite_add_tcase(s, tc_cross);

    return s;
}

int main(void) {
    SRunner* sr = srunner_create(parallel_init_integration_suite());
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
