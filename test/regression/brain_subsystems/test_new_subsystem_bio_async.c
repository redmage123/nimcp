/**
 * @file test_new_subsystem_bio_async.c
 * @brief Regression test: standalone create/update/destroy lifecycle for each subsystem
 *
 * WHAT: Verify each of the 7 new subsystems can be created, updated, and
 *       destroyed independently without crashing
 * WHY:  Ensures the subsystem APIs work in isolation, catching NULL derefs,
 *       uninitialized mutexes, and use-after-free bugs without the overhead
 *       of full brain creation
 * HOW:  For each module: default_config() -> create(config) -> update(dt) ->
 *       destroy(). No full brain required — lightweight tests.
 *
 * @author NIMCP Test Team
 * @date 2026-03-05
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Subsystem headers */
#include "core/brain/white_matter/nimcp_white_matter_tracts.h"
#include "core/brain/subcortical/nimcp_inferior_colliculus.h"
#include "core/spinal/nimcp_spinal_cord.h"
#include "core/cortical_columns/nimcp_cortical_interneurons.h"
#include "core/brain/regions/neuropeptide/nimcp_neuropeptide.h"
#include "core/brain/regions/endocannabinoid/nimcp_endocannabinoid.h"
#include "core/brain/regions/glymphatic/nimcp_glymphatic.h"

/*=============================================================================
 * Test 1: White Matter Tracts lifecycle
 *=============================================================================*/

START_TEST(test_wmt_lifecycle)
{
    wmt_config_t config = wmt_default_config();
    wmt_system_t* wmt = wmt_create(&config);
    ck_assert_ptr_nonnull(wmt);

    /* Update a few timesteps */
    for (int i = 0; i < 10; i++) {
        int rc = wmt_update(wmt, 0.01f);
        ck_assert_int_eq(rc, 0);
    }

    wmt_destroy(wmt);
}
END_TEST

START_TEST(test_wmt_create_null_config)
{
    /* NULL config should use defaults */
    wmt_system_t* wmt = wmt_create(NULL);
    ck_assert_ptr_nonnull(wmt);
    wmt_destroy(wmt);
}
END_TEST

START_TEST(test_wmt_destroy_null_safe)
{
    wmt_destroy(NULL);
    ck_assert(1);
}
END_TEST

/*=============================================================================
 * Test 2: Inferior Colliculus lifecycle
 *=============================================================================*/

START_TEST(test_ic_lifecycle)
{
    ic_config_t config = ic_default_config();
    inferior_colliculus_t* ic = ic_create(&config);
    ck_assert_ptr_nonnull(ic);

    for (int i = 0; i < 10; i++) {
        int rc = ic_update(ic, 0.01f);
        ck_assert_int_eq(rc, 0);
    }

    ic_destroy(ic);
}
END_TEST

START_TEST(test_ic_create_null_config)
{
    inferior_colliculus_t* ic = ic_create(NULL);
    ck_assert_ptr_nonnull(ic);
    ic_destroy(ic);
}
END_TEST

START_TEST(test_ic_destroy_null_safe)
{
    ic_destroy(NULL);
    ck_assert(1);
}
END_TEST

/*=============================================================================
 * Test 3: Spinal Cord lifecycle
 *=============================================================================*/

START_TEST(test_spinal_lifecycle)
{
    spinal_config_t config = spinal_default_config();
    spinal_cord_t* sc = spinal_create(&config);
    ck_assert_ptr_nonnull(sc);

    for (int i = 0; i < 10; i++) {
        int rc = spinal_update(sc, 0.01f);
        ck_assert_int_eq(rc, 0);
    }

    spinal_destroy(sc);
}
END_TEST

START_TEST(test_spinal_create_null_config)
{
    /* spinal_create(NULL) returns NULL (requires explicit config) */
    spinal_cord_t* sc = spinal_create(NULL);
    ck_assert_ptr_null(sc);
}
END_TEST

START_TEST(test_spinal_destroy_null_safe)
{
    spinal_destroy(NULL);
    ck_assert(1);
}
END_TEST

/*=============================================================================
 * Test 4: Cortical Interneurons lifecycle
 *=============================================================================*/

START_TEST(test_cint_lifecycle)
{
    cint_config_t config;
    int rc = cint_default_config(&config);
    ck_assert_int_eq(rc, 0);

    cortical_interneuron_system_t* cint = cint_create(&config);
    ck_assert_ptr_nonnull(cint);

    for (int i = 0; i < 10; i++) {
        rc = cint_update(cint, 0.01f);
        ck_assert_int_eq(rc, 0);
    }

    cint_destroy(cint);
}
END_TEST

START_TEST(test_cint_create_null_config)
{
    cortical_interneuron_system_t* cint = cint_create(NULL);
    ck_assert_ptr_nonnull(cint);
    cint_destroy(cint);
}
END_TEST

START_TEST(test_cint_destroy_null_safe)
{
    cint_destroy(NULL);
    ck_assert(1);
}
END_TEST

/*=============================================================================
 * Test 5: Neuropeptide system lifecycle
 *=============================================================================*/

START_TEST(test_npt_lifecycle)
{
    npt_config_t config = npt_default_config();
    neuropeptide_system_t* npt = npt_create(&config);
    ck_assert_ptr_nonnull(npt);

    for (int i = 0; i < 10; i++) {
        npt_error_t err = npt_update(npt, 0.1f);
        ck_assert_int_eq(err, NPT_OK);
    }

    npt_destroy(npt);
}
END_TEST

START_TEST(test_npt_create_null_config)
{
    neuropeptide_system_t* npt = npt_create(NULL);
    ck_assert_ptr_nonnull(npt);
    npt_destroy(npt);
}
END_TEST

START_TEST(test_npt_destroy_null_safe)
{
    npt_destroy(NULL);
    ck_assert(1);
}
END_TEST

START_TEST(test_npt_concentration_query)
{
    neuropeptide_system_t* npt = npt_create(NULL);
    ck_assert_ptr_nonnull(npt);

    float conc = 0.0f;
    npt_error_t err = npt_get_concentration(npt, NPT_OXYTOCIN, &conc);
    ck_assert_int_eq(err, NPT_OK);
    ck_assert(!isnan(conc));
    ck_assert(conc >= 0.0f);

    npt_destroy(npt);
}
END_TEST

/*=============================================================================
 * Test 6: Endocannabinoid system lifecycle
 *=============================================================================*/

START_TEST(test_ecb_lifecycle)
{
    ecb_config_t config = ecb_default_config();
    endocannabinoid_system_t* ecb = ecb_create(&config);
    ck_assert_ptr_nonnull(ecb);

    for (int i = 0; i < 10; i++) {
        int rc = ecb_update(ecb, 0.01f);
        ck_assert_int_eq(rc, 0);
    }

    ecb_destroy(ecb);
}
END_TEST

START_TEST(test_ecb_create_null_config)
{
    endocannabinoid_system_t* ecb = ecb_create(NULL);
    ck_assert_ptr_nonnull(ecb);
    ecb_destroy(ecb);
}
END_TEST

START_TEST(test_ecb_destroy_null_safe)
{
    ecb_destroy(NULL);
    ck_assert(1);
}
END_TEST

/*=============================================================================
 * Test 7: Glymphatic system lifecycle
 *=============================================================================*/

START_TEST(test_glym_lifecycle)
{
    glymphatic_config_t config = glymphatic_default_config();
    glymphatic_system_t* glym = glymphatic_create(&config);
    ck_assert_ptr_nonnull(glym);

    for (int i = 0; i < 10; i++) {
        int rc = glymphatic_update(glym, 0.1f);
        ck_assert_int_eq(rc, 0);
    }

    glymphatic_destroy(glym);
}
END_TEST

START_TEST(test_glym_create_null_config)
{
    glymphatic_system_t* glym = glymphatic_create(NULL);
    ck_assert_ptr_nonnull(glym);
    glymphatic_destroy(glym);
}
END_TEST

START_TEST(test_glym_destroy_null_safe)
{
    glymphatic_destroy(NULL);
    ck_assert(1);
}
END_TEST

START_TEST(test_glym_waste_query)
{
    glymphatic_system_t* glym = glymphatic_create(NULL);
    ck_assert_ptr_nonnull(glym);

    float waste = glymphatic_get_waste_level(glym);
    ck_assert(!isnan(waste));
    ck_assert(waste >= 0.0f);
    ck_assert(waste <= 1.0f);

    glymphatic_destroy(glym);
}
END_TEST

/*=============================================================================
 * Suite Creation
 *=============================================================================*/

Suite* new_subsystem_bio_async_suite(void)
{
    Suite* s = suite_create("New Subsystem Bio-Async Lifecycle");

    /* White Matter Tracts */
    TCase* tc_wmt = tcase_create("White Matter Tracts");
    tcase_set_timeout(tc_wmt, 30);
    tcase_add_test(tc_wmt, test_wmt_lifecycle);
    tcase_add_test(tc_wmt, test_wmt_create_null_config);
    tcase_add_test(tc_wmt, test_wmt_destroy_null_safe);
    suite_add_tcase(s, tc_wmt);

    /* Inferior Colliculus */
    TCase* tc_ic = tcase_create("Inferior Colliculus");
    tcase_set_timeout(tc_ic, 30);
    tcase_add_test(tc_ic, test_ic_lifecycle);
    tcase_add_test(tc_ic, test_ic_create_null_config);
    tcase_add_test(tc_ic, test_ic_destroy_null_safe);
    suite_add_tcase(s, tc_ic);

    /* Spinal Cord */
    TCase* tc_sc = tcase_create("Spinal Cord");
    tcase_set_timeout(tc_sc, 30);
    tcase_add_test(tc_sc, test_spinal_lifecycle);
    tcase_add_test(tc_sc, test_spinal_create_null_config);
    tcase_add_test(tc_sc, test_spinal_destroy_null_safe);
    suite_add_tcase(s, tc_sc);

    /* Cortical Interneurons */
    TCase* tc_cint = tcase_create("Cortical Interneurons");
    tcase_set_timeout(tc_cint, 30);
    tcase_add_test(tc_cint, test_cint_lifecycle);
    tcase_add_test(tc_cint, test_cint_create_null_config);
    tcase_add_test(tc_cint, test_cint_destroy_null_safe);
    suite_add_tcase(s, tc_cint);

    /* Neuropeptide */
    TCase* tc_npt = tcase_create("Neuropeptide");
    tcase_set_timeout(tc_npt, 30);
    tcase_add_test(tc_npt, test_npt_lifecycle);
    tcase_add_test(tc_npt, test_npt_create_null_config);
    tcase_add_test(tc_npt, test_npt_destroy_null_safe);
    tcase_add_test(tc_npt, test_npt_concentration_query);
    suite_add_tcase(s, tc_npt);

    /* Endocannabinoid */
    TCase* tc_ecb = tcase_create("Endocannabinoid");
    tcase_set_timeout(tc_ecb, 30);
    tcase_add_test(tc_ecb, test_ecb_lifecycle);
    tcase_add_test(tc_ecb, test_ecb_create_null_config);
    tcase_add_test(tc_ecb, test_ecb_destroy_null_safe);
    suite_add_tcase(s, tc_ecb);

    /* Glymphatic */
    TCase* tc_glym = tcase_create("Glymphatic");
    tcase_set_timeout(tc_glym, 30);
    tcase_add_test(tc_glym, test_glym_lifecycle);
    tcase_add_test(tc_glym, test_glym_create_null_config);
    tcase_add_test(tc_glym, test_glym_destroy_null_safe);
    tcase_add_test(tc_glym, test_glym_waste_query);
    suite_add_tcase(s, tc_glym);

    return s;
}

/*=============================================================================
 * Main
 *=============================================================================*/

int main(void)
{
    int number_failed;
    Suite* s = new_subsystem_bio_async_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
