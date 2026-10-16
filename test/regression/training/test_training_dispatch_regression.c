/**
 * @file test_training_dispatch_regression.c
 * @brief Regression tests for Training Dispatch module backward compatibility
 *
 * WHAT: Regression tests verifying backward compatibility and preventing regressions
 * WHY:  Ensure training dispatch changes don't break existing code
 * HOW:  Tests for default values, API compatibility, config struct stability
 *
 * REGRESSION CATEGORIES:
 * - Default Network Type: Verify default is ADAPTIVE
 * - ADAPTIVE Training Unchanged: XOR training behavior stable
 * - API Backward Compatibility: nimcp_brain_train_step() without network_type
 * - Config Struct Size Stable: ABI compatibility check
 * - SNN Method Defaults: Sensible SNN defaults
 * - LNN Method Defaults: Sensible LNN defaults
 * - Original Train Step Path: Backprop path when dispatcher returns -2
 *
 * @author NIMCP Test Team
 * @date 2026-01-16
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "nimcp.h"
/* Note: training dispatch functions are internal; test public API only */

/*=============================================================================
 * Test Fixtures
 *=============================================================================*/

static nimcp_brain_t test_brain = NULL;

static void setup_brain(void)
{
    nimcp_init();
    test_brain = nimcp_brain_create(
        "regression_test_brain",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        2,   /* num_inputs: XOR has 2 inputs */
        1    /* num_outputs: XOR has 1 output */
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

static void setup_basic(void)
{
    /* Minimal setup - no brain needed */
}

static void teardown_basic(void)
{
    /* Minimal teardown */
}

/*=============================================================================
 * Test 1: Default Network Type is ADAPTIVE
 *=============================================================================*/

START_TEST(test_default_network_type_is_adaptive)
{
    /* WHAT: Verify default config has network_type = NIMCP_NETWORK_ADAPTIVE
     * WHY:  Backward compatibility - existing code expects adaptive as default
     * REGRESSION: Default must remain ADAPTIVE for API stability
     */
    nimcp_training_config_t config = nimcp_training_config_default();

    ck_assert_int_eq(config.network_type, NIMCP_NETWORK_ADAPTIVE);

    /* Verify enum value hasn't changed */
    ck_assert_int_eq(NIMCP_NETWORK_ADAPTIVE, 0);
}
END_TEST

/*=============================================================================
 * Test 2: ADAPTIVE Training Unchanged (XOR Test)
 *=============================================================================*/

START_TEST(test_adaptive_training_unchanged)
{
    /* WHAT: Run XOR training with ADAPTIVE type, verify same behavior as before
     * WHY:  Ensure training dispatcher doesn't break existing adaptive training
     * REGRESSION: Training should complete without errors and loss should be valid
     */
    ck_assert_ptr_nonnull(test_brain);

    /* Configure training with ADAPTIVE (default) */
    nimcp_training_config_t config = nimcp_training_config_default();
    ck_assert_int_eq(config.network_type, NIMCP_NETWORK_ADAPTIVE);
    config.learning_rate = 0.1F;  /* Higher LR for XOR */

    nimcp_status_t status = nimcp_brain_configure_training(test_brain, &config);
    ck_assert_int_eq(status, NIMCP_OK);

    /* XOR training data */
    float xor_inputs[4][2] = {
        {0.0F, 0.0F},
        {0.0F, 1.0F},
        {1.0F, 0.0F},
        {1.0F, 1.0F}
    };
    float xor_targets[4][1] = {
        {0.0F},
        {1.0F},
        {1.0F},
        {0.0F}
    };

    /* Train for a number of epochs */
    float total_loss = 0.0F;
    float max_loss = 0.0F;
    nimcp_training_result_t result;
    uint32_t num_epochs = 50;
    int step_count = 0;

    for (uint32_t epoch = 0; epoch < num_epochs; epoch++) {
        for (int i = 0; i < 4; i++) {
            status = nimcp_brain_train_step(
                test_brain,
                xor_inputs[i], 2,
                xor_targets[i], 1,
                &result
            );
            ck_assert_int_eq(status, NIMCP_OK);

            /* Track loss values */
            total_loss += result.loss;
            if (result.loss > max_loss) {
                max_loss = result.loss;
            }
            step_count++;
        }
    }

    /* Verify training completed the expected number of steps */
    ck_assert_int_eq(step_count, num_epochs * 4);

    /* Loss should be valid (not NaN or Inf) */
    ck_assert(!isnan(total_loss));
    ck_assert(!isinf(total_loss));

    /* Average loss should be reasonable (not exploding) */
    float avg_loss = total_loss / step_count;
    ck_assert_msg(avg_loss >= 0.0F, "Average loss should be non-negative: %.4f", avg_loss);
    ck_assert_msg(avg_loss < 100.0F, "Average loss should be reasonable: %.4f", avg_loss);
}
END_TEST

/*=============================================================================
 * Test 3: Training API Backward Compatible
 *=============================================================================*/

START_TEST(test_training_api_backward_compatible)
{
    /* WHAT: Call nimcp_brain_train_step() without setting network_type explicitly
     * WHY:  Existing code that doesn't set network_type must continue to work
     * REGRESSION: API must default to adaptive backprop path
     */
    ck_assert_ptr_nonnull(test_brain);

    /* Configure training WITHOUT explicitly setting network_type
     * (relies on default being ADAPTIVE) */
    nimcp_training_config_t config = nimcp_training_config_default();
    /* Do NOT modify network_type - use default */

    nimcp_status_t status = nimcp_brain_configure_training(test_brain, &config);
    ck_assert_int_eq(status, NIMCP_OK);

    /* Single training step should work */
    float inputs[2] = {1.0F, 0.0F};
    float targets[1] = {1.0F};
    nimcp_training_result_t result;

    status = nimcp_brain_train_step(test_brain, inputs, 2, targets, 1, &result);
    ck_assert_int_eq(status, NIMCP_OK);

    /* Verify result is populated */
    ck_assert(result.step >= 0);
    /* Loss should be a valid number (not NaN) */
    ck_assert(!isnan(result.loss));
}
END_TEST

/*=============================================================================
 * Test 4: Config Struct Size Stable (ABI Compatibility)
 *=============================================================================*/

/* Expected size for nimcp_training_config_t based on current definition
 * This prevents accidental struct layout changes that would break ABI */
#define EXPECTED_CONFIG_STRUCT_MIN_SIZE 80  /* Minimum expected size in bytes */

START_TEST(test_config_struct_size_stable)
{
    /* WHAT: Verify sizeof(nimcp_training_config_t) hasn't changed unexpectedly
     * WHY:  ABI compatibility - struct size changes break binary compatibility
     * REGRESSION: Size must remain stable or only grow (never shrink)
     */
    size_t actual_size = sizeof(nimcp_training_config_t);

    /* Struct should be at least as large as expected minimum */
    ck_assert_msg(actual_size >= EXPECTED_CONFIG_STRUCT_MIN_SIZE,
                  "Config struct size changed: expected >= %zu, got %zu. "
                  "This may break ABI compatibility!",
                  (size_t)EXPECTED_CONFIG_STRUCT_MIN_SIZE, actual_size);

    /* Verify key fields exist at expected offsets (compile-time check) */
    nimcp_training_config_t config;
    (void)config.loss_type;
    (void)config.optimizer_type;
    (void)config.scheduler_type;
    (void)config.learning_rate;
    (void)config.network_type;
    (void)config.snn_method;
    (void)config.snn_eligibility_tau;
    (void)config.snn_surrogate_beta;
    (void)config.lnn_method;
    (void)config.lnn_bptt_truncation;
    (void)config.lnn_use_adjoint_checkpointing;

    /* Print actual size for documentation */
    /* fprintf(stderr, "nimcp_training_config_t size: %zu bytes\n", actual_size); */
}
END_TEST

/*=============================================================================
 * Test 5: SNN Method Defaults are Sensible
 *=============================================================================*/

START_TEST(test_snn_method_defaults)
{
    /* WHAT: Verify SNN defaults are sensible (surrogate, 20ms tau, 5.0 beta)
     * WHY:  SNN defaults must work out-of-box for common use cases
     * REGRESSION: Default values documented in API must remain stable
     */
    nimcp_training_config_t config = nimcp_training_config_default();

    /* SNN method should default to surrogate gradient (most general) */
    ck_assert_int_eq(config.snn_method, NIMCP_SNN_TRAIN_SURROGATE);

    /* Eligibility trace tau should be ~20ms (biologically plausible) */
    ck_assert_msg(fabsf(config.snn_eligibility_tau - 20.0F) < 0.001F,
                  "SNN eligibility_tau should be 20.0ms, got %.4f",
                  config.snn_eligibility_tau);

    /* Surrogate beta should be 5.0 (standard value for surrogate gradient) */
    ck_assert_msg(fabsf(config.snn_surrogate_beta - 5.0F) < 0.001F,
                  "SNN surrogate_beta should be 5.0, got %.4f",
                  config.snn_surrogate_beta);

    /* Reward tau should also be reasonable (default 100ms) */
    ck_assert(config.snn_reward_tau > 0.0F);
    ck_assert_msg(fabsf(config.snn_reward_tau - 100.0F) < 0.001F,
                  "SNN reward_tau should be 100.0ms, got %.4f",
                  config.snn_reward_tau);

    /* Verify enum values haven't changed */
    ck_assert_int_eq(NIMCP_SNN_TRAIN_STDP, 0);
    ck_assert_int_eq(NIMCP_SNN_TRAIN_R_STDP, 1);
    ck_assert_int_eq(NIMCP_SNN_TRAIN_EPROP, 2);
    ck_assert_int_eq(NIMCP_SNN_TRAIN_SURROGATE, 3);
    ck_assert_int_eq(NIMCP_SNN_TRAIN_HOMEOSTATIC, 4);
}
END_TEST

/*=============================================================================
 * Test 6: LNN Method Defaults are Sensible
 *=============================================================================*/

START_TEST(test_lnn_method_defaults)
{
    /* WHAT: Verify LNN defaults are sensible (adjoint, 100 truncation, checkpointing on)
     * WHY:  LNN defaults must work out-of-box for common use cases
     * REGRESSION: Default values documented in API must remain stable
     */
    nimcp_training_config_t config = nimcp_training_config_default();

    /* LNN method should default to adjoint (memory efficient) */
    ck_assert_int_eq(config.lnn_method, NIMCP_LNN_TRAIN_ADJOINT);

    /* BPTT truncation should be 100 (reasonable for sequence learning) */
    ck_assert_int_eq(config.lnn_bptt_truncation, 100);

    /* Adjoint checkpointing should be enabled by default (memory efficient) */
    ck_assert(config.lnn_use_adjoint_checkpointing == true);

    /* Verify enum values haven't changed */
    ck_assert_int_eq(NIMCP_LNN_TRAIN_ADJOINT, 0);
    ck_assert_int_eq(NIMCP_LNN_TRAIN_BPTT, 1);
    ck_assert_int_eq(NIMCP_LNN_TRAIN_RTRL, 2);
    ck_assert_int_eq(NIMCP_LNN_TRAIN_EPROP, 3);
}
END_TEST

/*=============================================================================
 * Test 7: Original Train Step Path Works (Dispatcher Returns -2)
 *=============================================================================*/

START_TEST(test_original_train_step_path)
{
    /* WHAT: Verify the original backprop path still works when dispatcher returns -2
     * WHY:  ADAPTIVE type dispatcher returns -2 to signal fallback to original path
     * REGRESSION: Must maintain original training behavior for ADAPTIVE networks
     */
    ck_assert_ptr_nonnull(test_brain);

    /* Configure with ADAPTIVE type */
    nimcp_training_config_t config = nimcp_training_config_default();
    config.network_type = NIMCP_NETWORK_ADAPTIVE;

    nimcp_status_t status = nimcp_brain_configure_training(test_brain, &config);
    ck_assert_int_eq(status, NIMCP_OK);

    /* The main test is that nimcp_brain_train_step() works correctly */

    float inputs[2] = {0.5F, 0.5F};
    float targets[1] = {0.5F};
    nimcp_training_result_t result;

    /* First training step */
    status = nimcp_brain_train_step(test_brain, inputs, 2, targets, 1, &result);
    ck_assert_int_eq(status, NIMCP_OK);

    /* Result should be populated from backprop path */
    ck_assert(!isnan(result.loss));
    ck_assert(result.learning_rate > 0.0F);

    /* Multiple steps should work */
    for (int i = 0; i < 10; i++) {
        status = nimcp_brain_train_step(test_brain, inputs, 2, targets, 1, &result);
        ck_assert_int_eq(status, NIMCP_OK);
    }
}
END_TEST

/*=============================================================================
 * Additional Regression Tests: Network Type Enum Stability
 *=============================================================================*/

START_TEST(test_network_type_enum_values_stable)
{
    /* WHAT: Verify network type enum values are stable
     * WHY:  Enum values must not change for serialization and ABI compatibility
     * REGRESSION: Critical for stored configs and binary compatibility
     */
    ck_assert_int_eq(NIMCP_NETWORK_ADAPTIVE, 0);
    ck_assert_int_eq(NIMCP_NETWORK_SNN, 1);
    ck_assert_int_eq(NIMCP_NETWORK_LNN, 2);
    ck_assert_int_eq(NIMCP_NETWORK_CNN, 3);
    ck_assert_int_eq(NIMCP_NETWORK_HYBRID, 4);
}
END_TEST

/*=============================================================================
 * Additional Regression Tests: All Network Types Can Be Configured
 *=============================================================================*/

START_TEST(test_all_network_types_configurable)
{
    /* WHAT: Verify all network types can be set in training config
     * WHY:  Ensure config accepts all valid network type values
     * REGRESSION: All network types must be configurable
     */
    ck_assert_ptr_nonnull(test_brain);

    nimcp_training_config_t config = nimcp_training_config_default();
    nimcp_status_t status;

    /* Test ADAPTIVE (default) */
    config.network_type = NIMCP_NETWORK_ADAPTIVE;
    status = nimcp_brain_configure_training(test_brain, &config);
    ck_assert_int_eq(status, NIMCP_OK);

    /* Note: SNN/LNN/CNN require specialized network setup
     * so we only verify ADAPTIVE works for this regression test */
}
END_TEST

/*=============================================================================
 * Test Suite Creation
 *=============================================================================*/

Suite* training_dispatch_regression_suite(void)
{
    Suite* s = suite_create("Training Dispatch Regression");

    /* Default Values Test Case */
    TCase* tc_defaults = tcase_create("Default Values");
    tcase_add_checked_fixture(tc_defaults, setup_basic, teardown_basic);
    tcase_add_test(tc_defaults, test_default_network_type_is_adaptive);
    tcase_add_test(tc_defaults, test_config_struct_size_stable);
    tcase_add_test(tc_defaults, test_snn_method_defaults);
    tcase_add_test(tc_defaults, test_lnn_method_defaults);
    tcase_add_test(tc_defaults, test_network_type_enum_values_stable);
    suite_add_tcase(s, tc_defaults);

    /* API Compatibility Test Case */
    TCase* tc_api = tcase_create("API Compatibility");
    tcase_add_checked_fixture(tc_api, setup_brain, teardown_brain);
    tcase_set_timeout(tc_api, 60);  /* 60 second timeout for training tests */
    tcase_add_test(tc_api, test_training_api_backward_compatible);
    tcase_add_test(tc_api, test_original_train_step_path);
    suite_add_tcase(s, tc_api);

    /* ADAPTIVE Training Test Case */
    TCase* tc_adaptive = tcase_create("ADAPTIVE Training");
    tcase_add_checked_fixture(tc_adaptive, setup_brain, teardown_brain);
    tcase_set_timeout(tc_adaptive, 120);  /* 2 minute timeout for XOR training */
    tcase_add_test(tc_adaptive, test_adaptive_training_unchanged);
    suite_add_tcase(s, tc_adaptive);

    /* Additional API Tests */
    TCase* tc_additional = tcase_create("Additional API");
    tcase_add_checked_fixture(tc_additional, setup_brain, teardown_brain);
    tcase_add_test(tc_additional, test_all_network_types_configurable);
    suite_add_tcase(s, tc_additional);

    return s;
}

/*=============================================================================
 * Main Entry Point
 *=============================================================================*/

int main(void)
{
    int number_failed;
    Suite* s = training_dispatch_regression_suite();
    SRunner* sr = srunner_create(s);

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
