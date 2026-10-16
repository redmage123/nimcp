/**
 * @file test_brain_create_with_new_subsystems.c
 * @brief Regression test: brain creation initializes all 7 new subsystem fields
 *
 * WHAT: Verify nimcp_brain_create() populates the 7 new subsystem pointers
 * WHY:  Prevent regressions where brain_init code is refactored and a subsystem
 *       initializer is accidentally removed or reordered
 * HOW:  Create a brain, access internal handle, check each field is non-NULL,
 *       then destroy the brain (must not SIGSEGV)
 *
 * SUBSYSTEMS UNDER TEST:
 *   1. white_matter         (wmt_system_t*)
 *   2. inferior_colliculus   (inferior_colliculus_t*)
 *   3. spinal_cord           (spinal_cord_t*)
 *   4. cortical_interneurons (cortical_interneuron_system_t*)
 *   5. neuropeptide          (neuropeptide_system_t*)
 *   6. endocannabinoid       (endocannabinoid_system_t*)
 *   7. glymphatic            (glymphatic_system_t*)
 *
 * @author NIMCP Test Team
 * @date 2026-03-05
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "nimcp.h"
#include "api/nimcp_api_internal.h"
#include "core/brain/nimcp_brain_internal.h"

/*=============================================================================
 * Test Fixtures
 *=============================================================================*/

static nimcp_brain_t test_brain = NULL;

static void setup_brain(void)
{
    nimcp_init();
    test_brain = nimcp_brain_create(
        "subsystem_regression_brain",
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
 * Test 1: Brain creation succeeds
 *=============================================================================*/

START_TEST(test_brain_create_succeeds)
{
    /* WHAT: Verify brain handle is non-NULL after creation
     * WHY:  If creation fails, all subsequent subsystem checks are meaningless
     */
    ck_assert_ptr_nonnull(test_brain);
}
END_TEST

/*=============================================================================
 * Test 2: Internal brain pointer is valid
 *=============================================================================*/

START_TEST(test_internal_brain_not_null)
{
    /* WHAT: Verify internal_brain pointer is set
     * WHY:  The handle wraps an internal brain_t; if NULL, subsystem fields
     *       cannot be accessed
     */
    ck_assert_ptr_nonnull(test_brain);
    ck_assert_ptr_nonnull(test_brain->internal_brain);
}
END_TEST

/*=============================================================================
 * Test 3: White matter tract system initialized
 *=============================================================================*/

START_TEST(test_white_matter_initialized)
{
    ck_assert_ptr_nonnull(test_brain);
    ck_assert_ptr_nonnull(test_brain->internal_brain);

    brain_t b = test_brain->internal_brain;
    ck_assert_msg(b->white_matter != NULL,
                  "white_matter field should be non-NULL after brain creation");
    ck_assert_msg(b->white_matter_enabled == true,
                  "white_matter_enabled should be true");
}
END_TEST

/*=============================================================================
 * Test 4: Inferior colliculus initialized
 *=============================================================================*/

START_TEST(test_inferior_colliculus_initialized)
{
    ck_assert_ptr_nonnull(test_brain);
    ck_assert_ptr_nonnull(test_brain->internal_brain);

    brain_t b = test_brain->internal_brain;
    ck_assert_msg(b->inferior_colliculus != NULL,
                  "inferior_colliculus field should be non-NULL after brain creation");
    ck_assert_msg(b->inferior_colliculus_enabled == true,
                  "inferior_colliculus_enabled should be true");
}
END_TEST

/*=============================================================================
 * Test 5: Spinal cord initialized
 *=============================================================================*/

START_TEST(test_spinal_cord_initialized)
{
    ck_assert_ptr_nonnull(test_brain);
    ck_assert_ptr_nonnull(test_brain->internal_brain);

    brain_t b = test_brain->internal_brain;
    ck_assert_msg(b->spinal_cord != NULL,
                  "spinal_cord field should be non-NULL after brain creation");
    ck_assert_msg(b->spinal_cord_enabled == true,
                  "spinal_cord_enabled should be true");
}
END_TEST

/*=============================================================================
 * Test 6: Cortical interneurons initialized
 *=============================================================================*/

START_TEST(test_cortical_interneurons_initialized)
{
    ck_assert_ptr_nonnull(test_brain);
    ck_assert_ptr_nonnull(test_brain->internal_brain);

    brain_t b = test_brain->internal_brain;
    ck_assert_msg(b->cortical_interneurons != NULL,
                  "cortical_interneurons field should be non-NULL after brain creation");
    ck_assert_msg(b->cortical_interneurons_enabled == true,
                  "cortical_interneurons_enabled should be true");
}
END_TEST

/*=============================================================================
 * Test 7: Neuropeptide system initialized
 *=============================================================================*/

START_TEST(test_neuropeptide_initialized)
{
    ck_assert_ptr_nonnull(test_brain);
    ck_assert_ptr_nonnull(test_brain->internal_brain);

    brain_t b = test_brain->internal_brain;
    ck_assert_msg(b->neuropeptide != NULL,
                  "neuropeptide field should be non-NULL after brain creation");
    ck_assert_msg(b->neuropeptide_enabled == true,
                  "neuropeptide_enabled should be true");
}
END_TEST

/*=============================================================================
 * Test 8: Endocannabinoid system initialized
 *=============================================================================*/

START_TEST(test_endocannabinoid_initialized)
{
    ck_assert_ptr_nonnull(test_brain);
    ck_assert_ptr_nonnull(test_brain->internal_brain);

    brain_t b = test_brain->internal_brain;
    ck_assert_msg(b->endocannabinoid != NULL,
                  "endocannabinoid field should be non-NULL after brain creation");
    ck_assert_msg(b->endocannabinoid_enabled == true,
                  "endocannabinoid_enabled should be true");
}
END_TEST

/*=============================================================================
 * Test 9: Glymphatic system initialized
 *=============================================================================*/

START_TEST(test_glymphatic_initialized)
{
    ck_assert_ptr_nonnull(test_brain);
    ck_assert_ptr_nonnull(test_brain->internal_brain);

    brain_t b = test_brain->internal_brain;
    ck_assert_msg(b->glymphatic != NULL,
                  "glymphatic field should be non-NULL after brain creation");
    ck_assert_msg(b->glymphatic_enabled == true,
                  "glymphatic_enabled should be true");
}
END_TEST

/*=============================================================================
 * Test 10: Brain destroy does not crash (SIGSEGV guard)
 *=============================================================================*/

START_TEST(test_brain_destroy_no_crash)
{
    /* WHAT: Create and immediately destroy a brain
     * WHY:  Verify all 7 subsystem destructors are called without SIGSEGV
     * HOW:  If any subsystem destroy dereferences invalid memory, the test
     *       process will be killed by the signal handler
     */
    nimcp_brain_t local_brain = nimcp_brain_create(
        "destroy_test_brain",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        2, 1
    );
    ck_assert_ptr_nonnull(local_brain);

    /* This is the actual test: destroy must not crash */
    nimcp_brain_destroy(local_brain);

    /* If we reach here, destroy succeeded */
    ck_assert(1);
}
END_TEST

/*=============================================================================
 * Test 11: Double destroy is safe (NULL-safe)
 *=============================================================================*/

START_TEST(test_brain_destroy_null_safe)
{
    /* WHAT: Calling nimcp_brain_destroy(NULL) must not crash
     * WHY:  Defensive programming - callers may accidentally double-free
     */
    nimcp_brain_destroy(NULL);
    ck_assert(1);
}
END_TEST

/*=============================================================================
 * Suite Creation
 *=============================================================================*/

Suite* brain_subsystems_create_suite(void)
{
    Suite* s = suite_create("Brain Subsystems Create Regression");

    /* Subsystem initialization tests */
    TCase* tc_init = tcase_create("Subsystem Initialization");
    tcase_add_checked_fixture(tc_init, setup_brain, teardown_brain);
    tcase_set_timeout(tc_init, 120);
    tcase_add_test(tc_init, test_brain_create_succeeds);
    tcase_add_test(tc_init, test_internal_brain_not_null);
    tcase_add_test(tc_init, test_white_matter_initialized);
    tcase_add_test(tc_init, test_inferior_colliculus_initialized);
    tcase_add_test(tc_init, test_spinal_cord_initialized);
    tcase_add_test(tc_init, test_cortical_interneurons_initialized);
    tcase_add_test(tc_init, test_neuropeptide_initialized);
    tcase_add_test(tc_init, test_endocannabinoid_initialized);
    tcase_add_test(tc_init, test_glymphatic_initialized);
    suite_add_tcase(s, tc_init);

    /* Destroy safety tests */
    TCase* tc_destroy = tcase_create("Destroy Safety");
    tcase_set_timeout(tc_destroy, 120);
    tcase_add_test(tc_destroy, test_brain_destroy_no_crash);
    tcase_add_test(tc_destroy, test_brain_destroy_null_safe);
    suite_add_tcase(s, tc_destroy);

    return s;
}

/*=============================================================================
 * Main
 *=============================================================================*/

int main(void)
{
    int number_failed;
    Suite* s = brain_subsystems_create_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
