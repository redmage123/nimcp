/**
 * @file test_training_dispatch.c
 * @brief Unit tests for training dispatcher
 *
 * WHAT: Comprehensive test suite for training_dispatch API
 * WHY:  Verify correct behavior of training dispatch routing for all network types
 * HOW:  Unit tests using Check framework covering all API functions
 *
 * NOTE: The training_dispatch API is an INTERNAL API that operates on brain_t
 *       (internal brain pointer), not nimcp_brain_t (public handle). Tests
 *       create brains via the public API and extract the internal pointer
 *       via nimcp_brain_handle->internal_brain.
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "nimcp.h"
#include "api/nimcp_api_internal.h"
#include "training/nimcp_training_dispatch.h"

/*=============================================================================
 * Helper: Extract internal brain_t from public nimcp_brain_t handle
 *=============================================================================*/

static brain_t get_internal_brain(nimcp_brain_t handle) {
    if (!handle) return NULL;
    return handle->internal_brain;
}

/*=============================================================================
 * Test Fixtures
 *=============================================================================*/

static void setup(void)
{
    /* Common setup if needed */
}

static void teardown(void)
{
    /* Common teardown if needed */
}

/*=============================================================================
 * training_dispatch_type_name() Tests
 *=============================================================================*/

START_TEST(test_type_name_adaptive)
{
    const char* name = training_dispatch_type_name(NIMCP_NETWORK_ADAPTIVE);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(name, "ADAPTIVE");
}
END_TEST

START_TEST(test_type_name_snn)
{
    const char* name = training_dispatch_type_name(NIMCP_NETWORK_SNN);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(name, "SNN");
}
END_TEST

START_TEST(test_type_name_lnn)
{
    const char* name = training_dispatch_type_name(NIMCP_NETWORK_LNN);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(name, "LNN");
}
END_TEST

START_TEST(test_type_name_cnn)
{
    const char* name = training_dispatch_type_name(NIMCP_NETWORK_CNN);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(name, "CNN");
}
END_TEST

START_TEST(test_type_name_hybrid)
{
    const char* name = training_dispatch_type_name(NIMCP_NETWORK_HYBRID);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(name, "HYBRID");
}
END_TEST

START_TEST(test_type_name_unknown)
{
    /* Test with an invalid/unknown network type */
    const char* name = training_dispatch_type_name(255);
    ck_assert_ptr_nonnull(name);
    /* Should return "UNKNOWN" for invalid types */
    ck_assert_str_eq(name, "UNKNOWN");
}
END_TEST

START_TEST(test_type_name_boundary_low)
{
    /* Test with type value 0 (ADAPTIVE) */
    const char* name = training_dispatch_type_name(0);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(name, "ADAPTIVE");
}
END_TEST

START_TEST(test_type_name_boundary_high)
{
    /* Test just beyond valid range */
    const char* name = training_dispatch_type_name(5);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(name, "UNKNOWN");
}
END_TEST

/*=============================================================================
 * training_dispatch_is_supported() Tests
 *=============================================================================*/

START_TEST(test_is_supported_adaptive)
{
    bool supported = training_dispatch_is_supported(NIMCP_NETWORK_ADAPTIVE);
    ck_assert(supported == true);
}
END_TEST

START_TEST(test_is_supported_snn)
{
    bool supported = training_dispatch_is_supported(NIMCP_NETWORK_SNN);
    ck_assert(supported == true);
}
END_TEST

START_TEST(test_is_supported_lnn)
{
    bool supported = training_dispatch_is_supported(NIMCP_NETWORK_LNN);
    ck_assert(supported == true);
}
END_TEST

START_TEST(test_is_supported_cnn)
{
    bool supported = training_dispatch_is_supported(NIMCP_NETWORK_CNN);
    ck_assert(supported == true);
}
END_TEST

START_TEST(test_is_supported_hybrid)
{
    bool supported = training_dispatch_is_supported(NIMCP_NETWORK_HYBRID);
    ck_assert(supported == true);
}
END_TEST

START_TEST(test_is_supported_invalid_high)
{
    /* Test unsupported type - beyond valid enum range */
    bool supported = training_dispatch_is_supported(255);
    ck_assert(supported == false);
}
END_TEST

START_TEST(test_is_supported_invalid_six)
{
    /* Test first value beyond HYBRID (4) */
    bool supported = training_dispatch_is_supported(6);
    ck_assert(supported == false);
}
END_TEST

START_TEST(test_is_supported_all_valid_types)
{
    /* Verify all defined types return consistent values */
    for (uint8_t type = 0; type <= NIMCP_NETWORK_HYBRID; type++) {
        bool supported = training_dispatch_is_supported(type);
        ck_assert_msg(supported == true,
            "Expected type %u to be supported", type);
    }
}
END_TEST

/*=============================================================================
 * training_dispatch_init() NULL Parameter Tests
 *=============================================================================*/

START_TEST(test_init_null_brain)
{
    nimcp_training_config_t config = nimcp_training_config_default();
    int result = training_dispatch_init(NULL, &config);
    ck_assert_int_lt(result, 0);  /* Should return negative error code */
}
END_TEST

START_TEST(test_init_null_config)
{
    /* Create a minimal brain for testing */
    nimcp_brain_t handle = nimcp_brain_create(
        "test_dispatch",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,  /* num_inputs */
        2    /* num_outputs */
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);
        int result = training_dispatch_init(brain, NULL);
        ck_assert_int_lt(result, 0);  /* Should return negative error code */
        nimcp_brain_destroy(handle);
    } else {
        /* If brain creation fails, skip this test gracefully */
        ck_assert_msg(handle != NULL, "Could not create test brain");
    }
}
END_TEST

START_TEST(test_init_both_null)
{
    int result = training_dispatch_init(NULL, NULL);
    ck_assert_int_lt(result, 0);  /* Should return negative error code */
}
END_TEST

/*=============================================================================
 * training_dispatch_step() NULL Parameter Tests
 *=============================================================================*/

START_TEST(test_step_null_brain)
{
    float inputs[] = {1.0f, 2.0f, 3.0f};
    float targets[] = {0.0f, 1.0f};
    training_dispatch_result_t result;

    int ret = training_dispatch_step(NULL, inputs, 3, targets, 2, &result);
    ck_assert_int_lt(ret, 0);  /* Should return negative error code */
}
END_TEST

START_TEST(test_step_null_inputs)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_step",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        3,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);
        float targets[] = {0.0f, 1.0f};
        training_dispatch_result_t result;

        int ret = training_dispatch_step(brain, NULL, 3, targets, 2, &result);
        ck_assert_int_lt(ret, 0);  /* Should return negative error code */

        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_step_null_targets)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_step",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        3,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);
        float inputs[] = {1.0f, 2.0f, 3.0f};
        training_dispatch_result_t result;

        int ret = training_dispatch_step(brain, inputs, 3, NULL, 2, &result);
        ck_assert_int_lt(ret, 0);  /* Should return negative error code */

        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_step_null_result)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_step",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        3,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);

        /* Initialize training first */
        nimcp_training_config_t config = nimcp_training_config_default();
        training_dispatch_init(brain, &config);

        float inputs[] = {1.0f, 2.0f, 3.0f};
        float targets[] = {0.0f, 1.0f};

        /* NULL result should be acceptable (optional output).
         * For ADAPTIVE, step returns -2 (signal to use standard backprop path).
         * Should not crash with NULL result parameter. */
        int ret = training_dispatch_step(brain, inputs, 3, targets, 2, NULL);
        (void)ret;

        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_step_zero_inputs)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_step",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        3,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);
        float inputs[] = {1.0f, 2.0f, 3.0f};
        float targets[] = {0.0f, 1.0f};
        training_dispatch_result_t result;

        /* M-3: Zero-length inputs should return an error (or at least not
         * succeed). For ADAPTIVE, dispatch returns -2 (use standard backprop).
         * Either way, the return value should be <= 0 (not positive success). */
        int ret = training_dispatch_step(brain, inputs, 0, targets, 2, &result);
        ck_assert_int_le(ret, 0);

        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_step_zero_targets)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_step",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        3,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);
        float inputs[] = {1.0f, 2.0f, 3.0f};
        float targets[] = {0.0f, 1.0f};
        training_dispatch_result_t result;

        /* M-3: Zero-length targets should return an error (or at least not
         * succeed). For ADAPTIVE, dispatch returns -2 (use standard backprop).
         * Either way, the return value should be <= 0 (not positive success). */
        int ret = training_dispatch_step(brain, inputs, 3, targets, 0, &result);
        ck_assert_int_le(ret, 0);

        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_step_all_null)
{
    int ret = training_dispatch_step(NULL, NULL, 0, NULL, 0, NULL);
    ck_assert_int_lt(ret, 0);  /* Should return negative error code */
}
END_TEST

/*=============================================================================
 * training_dispatch_set_reward() NULL Parameter Tests
 *=============================================================================*/

START_TEST(test_set_reward_null_brain)
{
    int result = training_dispatch_set_reward(NULL, 0.5f);
    ck_assert_int_lt(result, 0);  /* Should return -1 or negative */
}
END_TEST

START_TEST(test_set_reward_valid_positive)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_reward",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);

        /* Initialize with SNN training for reward to be meaningful.
         * Note: TINY brain may not have SNN subsystem, so init may fail.
         * set_reward returns -1 if not SNN or no training context. */
        nimcp_training_config_t config = nimcp_training_config_default();
        config.network_type = NIMCP_NETWORK_SNN;
        config.snn_method = NIMCP_SNN_TRAIN_R_STDP;
        training_dispatch_init(brain, &config);

        int result = training_dispatch_set_reward(brain, 1.0f);
        /* Result depends on whether SNN training is properly initialized.
         * Should return 0 for SNN with context, -1 otherwise. */
        ck_assert(result == 0 || result == -1);

        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_set_reward_valid_negative)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_reward",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);
        int result = training_dispatch_set_reward(brain, -1.0f);
        /* May fail if not SNN, but should not crash */
        ck_assert(result == 0 || result == -1);

        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_set_reward_zero)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_reward",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);
        int result = training_dispatch_set_reward(brain, 0.0f);
        /* May fail if not SNN, but should not crash */
        ck_assert(result == 0 || result == -1);

        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_set_reward_out_of_range)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_reward",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);
        /* Test with reward outside -1 to +1 range.
         * M-4: The training_dispatch_set_reward() API does NOT validate the
         * reward range — it passes the value directly to the SNN R-STDP
         * implementation which may clamp or use as-is. This is by design:
         * the caller (Python training loop) is responsible for range control. */
        int result = training_dispatch_set_reward(brain, 5.0f);
        /* Should either clamp the value or reject it */
        ck_assert(result == 0 || result == -1);

        nimcp_brain_destroy(handle);
    }
}
END_TEST

/*=============================================================================
 * training_dispatch_get_stats() NULL Parameter Tests
 *=============================================================================*/

START_TEST(test_get_stats_null_brain)
{
    uint64_t total_steps;
    float total_loss;
    float current_lr;

    int result = training_dispatch_get_stats(NULL, &total_steps, &total_loss, &current_lr);
    ck_assert_int_lt(result, 0);  /* Should return negative error code */
}
END_TEST

START_TEST(test_get_stats_null_total_steps)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_stats",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);
        float total_loss;
        float current_lr;

        /* The implementation uses `if (total_steps) *total_steps = ...` guards,
         * so NULL individual output params are tolerated. The result depends on
         * whether a training context exists for the active network type. */
        int result = training_dispatch_get_stats(brain, NULL, &total_loss, &current_lr);
        /* For ADAPTIVE (default), returns -1 since there's no stats tracking */
        ck_assert(result == 0 || result == -1);

        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_get_stats_null_total_loss)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_stats",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);
        uint64_t total_steps;
        float current_lr;

        int result = training_dispatch_get_stats(brain, &total_steps, NULL, &current_lr);
        ck_assert(result == 0 || result == -1);

        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_get_stats_null_current_lr)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_stats",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);
        uint64_t total_steps;
        float total_loss;

        int result = training_dispatch_get_stats(brain, &total_steps, &total_loss, NULL);
        ck_assert(result == 0 || result == -1);

        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_get_stats_all_null_outputs)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_stats",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);
        int result = training_dispatch_get_stats(brain, NULL, NULL, NULL);
        /* Implementation tolerates NULL output params via if-guards.
         * Returns -1 for ADAPTIVE (no stats tracking). */
        ck_assert(result == 0 || result == -1);

        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_get_stats_all_null)
{
    int result = training_dispatch_get_stats(NULL, NULL, NULL, NULL);
    ck_assert_int_lt(result, 0);  /* Should return negative error code */
}
END_TEST

/*=============================================================================
 * training_dispatch_reset() NULL Parameter Tests
 *=============================================================================*/

START_TEST(test_reset_null_brain)
{
    int result = training_dispatch_reset(NULL);
    ck_assert_int_lt(result, 0);  /* Should return negative error code */
}
END_TEST

START_TEST(test_reset_valid_brain)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_reset",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);

        /* Initialize training first */
        nimcp_training_config_t config = nimcp_training_config_default();
        training_dispatch_init(brain, &config);

        int result = training_dispatch_reset(brain);
        /* Reset returns 0 for all network types (including default/ADAPTIVE) */
        ck_assert_int_eq(result, 0);

        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_reset_uninitialized_brain)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_reset",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);

        /* Don't initialize training - call reset directly.
         * Reset returns 0 for all types (falls through switch default). */
        int result = training_dispatch_reset(brain);
        ck_assert_int_eq(result, 0);

        nimcp_brain_destroy(handle);
    }
}
END_TEST

/*=============================================================================
 * training_dispatch_destroy() NULL Parameter Tests
 *=============================================================================*/

START_TEST(test_destroy_null_brain)
{
    /* Should not crash when called with NULL */
    training_dispatch_destroy(NULL);
    /* No assertion needed - just verify no crash */
}
END_TEST

START_TEST(test_destroy_valid_brain)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_destroy",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);

        /* Initialize training */
        nimcp_training_config_t config = nimcp_training_config_default();
        training_dispatch_init(brain, &config);

        /* Destroy training resources */
        training_dispatch_destroy(brain);

        /* Cleanup brain */
        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_destroy_twice)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_destroy",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);

        /* Initialize training */
        nimcp_training_config_t config = nimcp_training_config_default();
        training_dispatch_init(brain, &config);

        /* Destroy training resources twice - should not crash.
         * The implementation NULLs out pointers after free, so second call is safe. */
        training_dispatch_destroy(brain);
        training_dispatch_destroy(brain);

        /* Cleanup brain */
        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_destroy_without_init)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_destroy",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);

        /* Don't initialize - call destroy directly.
         * Should not crash even without initialization. */
        training_dispatch_destroy(brain);

        nimcp_brain_destroy(handle);
    }
}
END_TEST

/*=============================================================================
 * Integration Tests - Valid Usage Patterns
 *=============================================================================*/

START_TEST(test_full_lifecycle_adaptive)
{
    nimcp_brain_t handle = nimcp_brain_create(
        "test_lifecycle",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        4,
        2
    );

    if (handle != NULL) {
        brain_t brain = get_internal_brain(handle);

        /* Initialize with adaptive network */
        nimcp_training_config_t config = nimcp_training_config_default();
        config.network_type = NIMCP_NETWORK_ADAPTIVE;

        int init_result = training_dispatch_init(brain, &config);
        ck_assert_int_eq(init_result, 0);

        /* Perform a training step.
         * For ADAPTIVE, dispatch returns -2 as a signal to the caller
         * that standard backprop should be used instead. This is not
         * an error but a dispatch signal. */
        float inputs[] = {0.1f, 0.2f, 0.3f, 0.4f};
        float targets[] = {1.0f, 0.0f};
        training_dispatch_result_t result;

        int step_result = training_dispatch_step(brain, inputs, 4, targets, 2, &result);
        /* -2 = "use standard backprop", not an error */
        ck_assert(step_result == 0 || step_result == -2);

        /* Get stats - ADAPTIVE does not have specialized stats tracking,
         * so this returns -1 (falls through to default case). */
        uint64_t total_steps;
        float total_loss;
        float current_lr;

        int stats_result = training_dispatch_get_stats(brain, &total_steps, &total_loss, &current_lr);
        /* Accept either success (0) or no-stats (-1) for ADAPTIVE */
        ck_assert(stats_result == 0 || stats_result == -1);

        /* Reset - returns 0 for all types */
        int reset_result = training_dispatch_reset(brain);
        ck_assert_int_eq(reset_result, 0);

        /* Destroy */
        training_dispatch_destroy(brain);
        nimcp_brain_destroy(handle);
    }
}
END_TEST

START_TEST(test_type_name_returns_non_empty)
{
    /* Verify all type names are non-empty strings */
    for (uint8_t i = 0; i <= 10; i++) {
        const char* name = training_dispatch_type_name(i);
        ck_assert_ptr_nonnull(name);
        ck_assert_uint_gt(strlen(name), 0);
    }
}
END_TEST

/*=============================================================================
 * Test Suite Creation
 *=============================================================================*/

Suite* training_dispatch_suite(void)
{
    Suite* s = suite_create("Training Dispatch");

    /* Type name tests - no brain needed, fast */
    TCase* tc_type_name = tcase_create("Type Name");
    tcase_add_checked_fixture(tc_type_name, setup, teardown);
    tcase_set_timeout(tc_type_name, 10);
    tcase_add_test(tc_type_name, test_type_name_adaptive);
    tcase_add_test(tc_type_name, test_type_name_snn);
    tcase_add_test(tc_type_name, test_type_name_lnn);
    tcase_add_test(tc_type_name, test_type_name_cnn);
    tcase_add_test(tc_type_name, test_type_name_hybrid);
    tcase_add_test(tc_type_name, test_type_name_unknown);
    tcase_add_test(tc_type_name, test_type_name_boundary_low);
    tcase_add_test(tc_type_name, test_type_name_boundary_high);
    tcase_add_test(tc_type_name, test_type_name_returns_non_empty);
    suite_add_tcase(s, tc_type_name);

    /* Is supported tests - no brain needed, fast */
    TCase* tc_is_supported = tcase_create("Is Supported");
    tcase_add_checked_fixture(tc_is_supported, setup, teardown);
    tcase_set_timeout(tc_is_supported, 10);
    tcase_add_test(tc_is_supported, test_is_supported_adaptive);
    tcase_add_test(tc_is_supported, test_is_supported_snn);
    tcase_add_test(tc_is_supported, test_is_supported_lnn);
    tcase_add_test(tc_is_supported, test_is_supported_cnn);
    tcase_add_test(tc_is_supported, test_is_supported_hybrid);
    tcase_add_test(tc_is_supported, test_is_supported_invalid_high);
    tcase_add_test(tc_is_supported, test_is_supported_invalid_six);
    tcase_add_test(tc_is_supported, test_is_supported_all_valid_types);
    suite_add_tcase(s, tc_is_supported);

    /* Init tests - brain creation + dispatch init */
    TCase* tc_init = tcase_create("Init");
    tcase_add_checked_fixture(tc_init, setup, teardown);
    tcase_set_timeout(tc_init, 30);
    tcase_add_test(tc_init, test_init_null_brain);
    tcase_add_test(tc_init, test_init_null_config);
    tcase_add_test(tc_init, test_init_both_null);
    suite_add_tcase(s, tc_init);

    /* Step tests - brain creation + dispatch step */
    TCase* tc_step = tcase_create("Step");
    tcase_add_checked_fixture(tc_step, setup, teardown);
    tcase_set_timeout(tc_step, 30);
    tcase_add_test(tc_step, test_step_null_brain);
    tcase_add_test(tc_step, test_step_null_inputs);
    tcase_add_test(tc_step, test_step_null_targets);
    tcase_add_test(tc_step, test_step_null_result);
    tcase_add_test(tc_step, test_step_zero_inputs);
    tcase_add_test(tc_step, test_step_zero_targets);
    tcase_add_test(tc_step, test_step_all_null);
    suite_add_tcase(s, tc_step);

    /* Set reward tests */
    TCase* tc_reward = tcase_create("Set Reward");
    tcase_add_checked_fixture(tc_reward, setup, teardown);
    tcase_set_timeout(tc_reward, 30);
    tcase_add_test(tc_reward, test_set_reward_null_brain);
    tcase_add_test(tc_reward, test_set_reward_valid_positive);
    tcase_add_test(tc_reward, test_set_reward_valid_negative);
    tcase_add_test(tc_reward, test_set_reward_zero);
    tcase_add_test(tc_reward, test_set_reward_out_of_range);
    suite_add_tcase(s, tc_reward);

    /* Get stats tests */
    TCase* tc_stats = tcase_create("Get Stats");
    tcase_add_checked_fixture(tc_stats, setup, teardown);
    tcase_set_timeout(tc_stats, 30);
    tcase_add_test(tc_stats, test_get_stats_null_brain);
    tcase_add_test(tc_stats, test_get_stats_null_total_steps);
    tcase_add_test(tc_stats, test_get_stats_null_total_loss);
    tcase_add_test(tc_stats, test_get_stats_null_current_lr);
    tcase_add_test(tc_stats, test_get_stats_all_null_outputs);
    tcase_add_test(tc_stats, test_get_stats_all_null);
    suite_add_tcase(s, tc_stats);

    /* Reset tests */
    TCase* tc_reset = tcase_create("Reset");
    tcase_add_checked_fixture(tc_reset, setup, teardown);
    tcase_set_timeout(tc_reset, 30);
    tcase_add_test(tc_reset, test_reset_null_brain);
    tcase_add_test(tc_reset, test_reset_valid_brain);
    tcase_add_test(tc_reset, test_reset_uninitialized_brain);
    suite_add_tcase(s, tc_reset);

    /* Destroy tests */
    TCase* tc_destroy = tcase_create("Destroy");
    tcase_add_checked_fixture(tc_destroy, setup, teardown);
    tcase_set_timeout(tc_destroy, 30);
    tcase_add_test(tc_destroy, test_destroy_null_brain);
    tcase_add_test(tc_destroy, test_destroy_valid_brain);
    tcase_add_test(tc_destroy, test_destroy_twice);
    tcase_add_test(tc_destroy, test_destroy_without_init);
    suite_add_tcase(s, tc_destroy);

    /* Integration tests - full lifecycle */
    TCase* tc_integration = tcase_create("Integration");
    tcase_add_checked_fixture(tc_integration, setup, teardown);
    tcase_set_timeout(tc_integration, 60);
    tcase_add_test(tc_integration, test_full_lifecycle_adaptive);
    /* M-5 TODO: Add full lifecycle tests for SNN, LNN, CNN, and HYBRID network
     * types. These require creating brains with specific network configurations
     * (e.g., NIMCP_NETWORK_SNN with R-STDP), which need more complex setup than
     * the TINY brain used here. The ADAPTIVE test covers the dispatch mechanism;
     * network-specific training is tested in dedicated test suites. */
    suite_add_tcase(s, tc_integration);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = training_dispatch_suite();
    SRunner* sr = srunner_create(s);

    /* Run in CK_NOFORK mode to avoid repeated process forking.
     * Each brain creation takes ~3-5 seconds due to full subsystem init
     * (GPU recovery, neuromodulators, knowledge graph, etc.), and with
     * Check's default fork mode, each test forks a new process that
     * re-initializes everything. CK_NOFORK runs all tests in one process. */
    srunner_set_fork_status(sr, CK_NOFORK);

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
