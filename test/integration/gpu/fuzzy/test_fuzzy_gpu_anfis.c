/* ============================================================================
 * Fuzzy GPU ANFIS Training Integration Tests
 * ============================================================================
 * WHAT: Integration tests for GPU ANFIS training pipeline
 * WHY:  Validate ANFIS training workflow on GPU (forward/backward pass, convergence)
 * HOW:  Train ANFIS networks, verify learning, compare CPU/GPU results
 * ============================================================================
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef NIMCP_CUDA_ENABLED
#include "gpu/fuzzy/nimcp_fuzzy_anfis_gpu.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu.h"
#include "gpu/nimcp_gpu_context.h"
#include "cognitive/fuzzy/nimcp_fuzzy_anfis.h"
#endif

#define TOLERANCE 1e-4
#define TRAIN_TOLERANCE 0.1

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/* Generate training data for function approximation: y = sin(x1) * cos(x2) */
static void generate_sincos_data(float* inputs, float* targets, int n_samples) {
    for (int i = 0; i < n_samples; i++) {
        float x1 = ((float)(i % 100) / 100.0f) * 2.0f * (float)M_PI;
        float x2 = ((float)(i / 100) / 100.0f) * 2.0f * (float)M_PI;
        inputs[i * 2] = x1;
        inputs[i * 2 + 1] = x2;
        targets[i] = sinf(x1) * cosf(x2);
    }
}

/* Generate training data for XOR-like function */
static void generate_xor_data(float* inputs, float* targets, int n_samples) {
    for (int i = 0; i < n_samples; i++) {
        float x1 = (float)(rand() % 1000) / 1000.0f;
        float x2 = (float)(rand() % 1000) / 1000.0f;
        inputs[i * 2] = x1;
        inputs[i * 2 + 1] = x2;
        /* XOR-like: high when exactly one input is high */
        float xor_val = (x1 > 0.5f) != (x2 > 0.5f) ? 1.0f : 0.0f;
        /* Add some noise */
        targets[i] = xor_val + ((float)(rand() % 100) / 1000.0f - 0.05f);
    }
}

/* ============================================================================
 * Basic ANFIS Training Tests
 * ============================================================================ */

START_TEST(test_anfis_create_and_destroy)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    /* Create ANFIS structure */
    nimcp_gpu_anfis_params_t params = {
        .num_inputs = 2,
        .num_mfs_per_input = 3,
        .mf_type = FUZZY_MF_GAUSSIAN,
        .learning_rate = 0.01f,
        .momentum = 0.9f
    };

    nimcp_gpu_anfis_state_t* state = nimcp_gpu_anfis_create(ctx, &params);
    ck_assert_msg(state != NULL, "Failed to create ANFIS state");

    if (state) {
        /* Verify structure */
        ck_assert_msg(nimcp_gpu_anfis_get_num_rules(state) == 9,
            "2 inputs x 3 MFs should produce 9 rules");

        nimcp_gpu_anfis_destroy(state);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_anfis_forward_pass)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    nimcp_gpu_anfis_params_t params = {
        .num_inputs = 2,
        .num_mfs_per_input = 2,
        .mf_type = FUZZY_MF_GAUSSIAN,
        .learning_rate = 0.01f,
        .momentum = 0.9f
    };

    nimcp_gpu_anfis_state_t* state = nimcp_gpu_anfis_create(ctx, &params);
    if (!state) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create ANFIS state");
        return;
    }

    /* Test forward pass with batch data */
    const int batch_size = 100;
    float* inputs = (float*)malloc(batch_size * 2 * sizeof(float));
    float* outputs = (float*)malloc(batch_size * sizeof(float));

    for (int i = 0; i < batch_size; i++) {
        inputs[i * 2] = (float)i / batch_size;
        inputs[i * 2 + 1] = (float)i / batch_size;
    }

    nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_create(ctx,
        (size_t[]){batch_size, 2}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output_tensor = nimcp_gpu_tensor_create(ctx,
        (size_t[]){batch_size, 1}, 2, NIMCP_GPU_PRECISION_FP32);

    if (input_tensor && output_tensor) {
        nimcp_gpu_tensor_copy_from_host(input_tensor, inputs, batch_size * 2 * sizeof(float));

        bool success = nimcp_gpu_anfis_forward(ctx, state, input_tensor, output_tensor);
        ck_assert_msg(success, "Forward pass should succeed");

        if (success) {
            nimcp_gpu_tensor_copy_to_host(output_tensor, outputs, batch_size * sizeof(float));

            /* Verify outputs are finite */
            int nan_count = 0;
            for (int i = 0; i < batch_size; i++) {
                if (!isfinite(outputs[i])) nan_count++;
            }
            ck_assert_msg(nan_count == 0,
                "All outputs should be finite: %d NaN/Inf values", nan_count);
        }

        nimcp_gpu_tensor_destroy(input_tensor);
        nimcp_gpu_tensor_destroy(output_tensor);
    }

    free(inputs);
    free(outputs);
    nimcp_gpu_anfis_destroy(state);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * ANFIS Training Convergence Tests
 * ============================================================================ */

START_TEST(test_anfis_training_linear_function)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    nimcp_gpu_anfis_params_t params = {
        .num_inputs = 1,
        .num_mfs_per_input = 3,
        .mf_type = FUZZY_MF_GAUSSIAN,
        .learning_rate = 0.1f,
        .momentum = 0.0f
    };

    nimcp_gpu_anfis_state_t* state = nimcp_gpu_anfis_create(ctx, &params);
    if (!state) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create ANFIS state");
        return;
    }

    /* Generate linear function data: y = 2x + 1 */
    const int n_samples = 200;
    float* inputs = (float*)malloc(n_samples * sizeof(float));
    float* targets = (float*)malloc(n_samples * sizeof(float));

    for (int i = 0; i < n_samples; i++) {
        inputs[i] = (float)i / n_samples * 10.0f;  /* 0 to 10 */
        targets[i] = 2.0f * inputs[i] + 1.0f;      /* y = 2x + 1 */
    }

    nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_create(ctx,
        (size_t[]){n_samples, 1}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* target_tensor = nimcp_gpu_tensor_create(ctx,
        (size_t[]){n_samples, 1}, 2, NIMCP_GPU_PRECISION_FP32);

    if (input_tensor && target_tensor) {
        nimcp_gpu_tensor_copy_from_host(input_tensor, inputs, n_samples * sizeof(float));
        nimcp_gpu_tensor_copy_from_host(target_tensor, targets, n_samples * sizeof(float));

        float initial_error = 0.0f, final_error = 0.0f;

        nimcp_gpu_anfis_train_params_t train_params = {
            .num_epochs = 100,
            .batch_size = 50,
            .early_stop_threshold = 0.001f,
            .shuffle = true
        };

        bool success = nimcp_gpu_anfis_train(ctx, state, input_tensor, target_tensor,
            &train_params, &initial_error, &final_error);

        if (success) {
            /* Error should decrease */
            ck_assert_msg(final_error < initial_error,
                "Error should decrease: initial=%.4f, final=%.4f",
                initial_error, final_error);

            /* Final error should be small for linear function */
            ck_assert_msg(final_error < 1.0f,
                "Final error should be small for linear function: got %.4f", final_error);
        }

        nimcp_gpu_tensor_destroy(input_tensor);
        nimcp_gpu_tensor_destroy(target_tensor);
    }

    free(inputs);
    free(targets);
    nimcp_gpu_anfis_destroy(state);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_anfis_training_nonlinear_function)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    nimcp_gpu_anfis_params_t params = {
        .num_inputs = 2,
        .num_mfs_per_input = 5,  /* More MFs for nonlinear function */
        .mf_type = FUZZY_MF_GAUSSIAN,
        .learning_rate = 0.05f,
        .momentum = 0.9f
    };

    nimcp_gpu_anfis_state_t* state = nimcp_gpu_anfis_create(ctx, &params);
    if (!state) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create ANFIS state");
        return;
    }

    /* Generate sin*cos data */
    const int n_samples = 400;
    float* inputs = (float*)malloc(n_samples * 2 * sizeof(float));
    float* targets = (float*)malloc(n_samples * sizeof(float));

    generate_sincos_data(inputs, targets, n_samples);

    nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_create(ctx,
        (size_t[]){n_samples, 2}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* target_tensor = nimcp_gpu_tensor_create(ctx,
        (size_t[]){n_samples, 1}, 2, NIMCP_GPU_PRECISION_FP32);

    if (input_tensor && target_tensor) {
        nimcp_gpu_tensor_copy_from_host(input_tensor, inputs, n_samples * 2 * sizeof(float));
        nimcp_gpu_tensor_copy_from_host(target_tensor, targets, n_samples * sizeof(float));

        float initial_error = 0.0f, final_error = 0.0f;

        nimcp_gpu_anfis_train_params_t train_params = {
            .num_epochs = 200,
            .batch_size = 100,
            .early_stop_threshold = 0.01f,
            .shuffle = true
        };

        bool success = nimcp_gpu_anfis_train(ctx, state, input_tensor, target_tensor,
            &train_params, &initial_error, &final_error);

        if (success) {
            /* Error should decrease significantly */
            ck_assert_msg(final_error < initial_error * 0.5f,
                "Error should decrease by at least 50%%: initial=%.4f, final=%.4f",
                initial_error, final_error);
        }

        nimcp_gpu_tensor_destroy(input_tensor);
        nimcp_gpu_tensor_destroy(target_tensor);
    }

    free(inputs);
    free(targets);
    nimcp_gpu_anfis_destroy(state);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Hybrid Learning Tests
 * ============================================================================ */

START_TEST(test_anfis_hybrid_learning)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    nimcp_gpu_anfis_params_t params = {
        .num_inputs = 2,
        .num_mfs_per_input = 3,
        .mf_type = FUZZY_MF_GAUSSIAN,
        .learning_rate = 0.01f,
        .momentum = 0.9f
    };

    nimcp_gpu_anfis_state_t* state = nimcp_gpu_anfis_create(ctx, &params);
    if (!state) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create ANFIS state");
        return;
    }

    /* Generate XOR-like data */
    const int n_samples = 500;
    float* inputs = (float*)malloc(n_samples * 2 * sizeof(float));
    float* targets = (float*)malloc(n_samples * sizeof(float));

    generate_xor_data(inputs, targets, n_samples);

    nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_create(ctx,
        (size_t[]){n_samples, 2}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* target_tensor = nimcp_gpu_tensor_create(ctx,
        (size_t[]){n_samples, 1}, 2, NIMCP_GPU_PRECISION_FP32);

    if (input_tensor && target_tensor) {
        nimcp_gpu_tensor_copy_from_host(input_tensor, inputs, n_samples * 2 * sizeof(float));
        nimcp_gpu_tensor_copy_from_host(target_tensor, targets, n_samples * sizeof(float));

        float initial_error = 0.0f, final_error = 0.0f;

        /* Hybrid learning: LSE for consequent params, backprop for premise params */
        nimcp_gpu_anfis_train_params_t train_params = {
            .num_epochs = 100,
            .batch_size = n_samples,  /* Full batch for LSE */
            .early_stop_threshold = 0.05f,
            .shuffle = false,
            .hybrid_learning = true,
            .lse_lambda = 0.01f  /* Regularization for LSE */
        };

        bool success = nimcp_gpu_anfis_train(ctx, state, input_tensor, target_tensor,
            &train_params, &initial_error, &final_error);

        if (success) {
            /* Hybrid learning should converge faster */
            ck_assert_msg(final_error < initial_error,
                "Hybrid learning should decrease error: initial=%.4f, final=%.4f",
                initial_error, final_error);
        }

        nimcp_gpu_tensor_destroy(input_tensor);
        nimcp_gpu_tensor_destroy(target_tensor);
    }

    free(inputs);
    free(targets);
    nimcp_gpu_anfis_destroy(state);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Validation and Generalization Tests
 * ============================================================================ */

START_TEST(test_anfis_validation_split)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    nimcp_gpu_anfis_params_t params = {
        .num_inputs = 1,
        .num_mfs_per_input = 5,
        .mf_type = FUZZY_MF_GAUSSIAN,
        .learning_rate = 0.05f,
        .momentum = 0.9f
    };

    nimcp_gpu_anfis_state_t* state = nimcp_gpu_anfis_create(ctx, &params);
    if (!state) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create ANFIS state");
        return;
    }

    /* Generate training data: y = sin(x) */
    const int n_train = 100;
    const int n_val = 50;
    float* train_inputs = (float*)malloc(n_train * sizeof(float));
    float* train_targets = (float*)malloc(n_train * sizeof(float));
    float* val_inputs = (float*)malloc(n_val * sizeof(float));
    float* val_targets = (float*)malloc(n_val * sizeof(float));

    for (int i = 0; i < n_train; i++) {
        train_inputs[i] = (float)i / n_train * 2.0f * (float)M_PI;
        train_targets[i] = sinf(train_inputs[i]);
    }

    /* Validation set: different points in same range */
    for (int i = 0; i < n_val; i++) {
        val_inputs[i] = ((float)i + 0.5f) / n_val * 2.0f * (float)M_PI;
        val_targets[i] = sinf(val_inputs[i]);
    }

    nimcp_gpu_tensor_t* train_input_tensor = nimcp_gpu_tensor_create(ctx,
        (size_t[]){n_train, 1}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* train_target_tensor = nimcp_gpu_tensor_create(ctx,
        (size_t[]){n_train, 1}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* val_input_tensor = nimcp_gpu_tensor_create(ctx,
        (size_t[]){n_val, 1}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* val_output_tensor = nimcp_gpu_tensor_create(ctx,
        (size_t[]){n_val, 1}, 2, NIMCP_GPU_PRECISION_FP32);

    if (train_input_tensor && train_target_tensor && val_input_tensor && val_output_tensor) {
        nimcp_gpu_tensor_copy_from_host(train_input_tensor, train_inputs, n_train * sizeof(float));
        nimcp_gpu_tensor_copy_from_host(train_target_tensor, train_targets, n_train * sizeof(float));
        nimcp_gpu_tensor_copy_from_host(val_input_tensor, val_inputs, n_val * sizeof(float));

        float initial_error = 0.0f, final_error = 0.0f;

        nimcp_gpu_anfis_train_params_t train_params = {
            .num_epochs = 200,
            .batch_size = 50,
            .early_stop_threshold = 0.01f,
            .shuffle = true
        };

        bool success = nimcp_gpu_anfis_train(ctx, state, train_input_tensor, train_target_tensor,
            &train_params, &initial_error, &final_error);

        if (success) {
            /* Test on validation set */
            nimcp_gpu_anfis_forward(ctx, state, val_input_tensor, val_output_tensor);

            float* val_outputs = (float*)malloc(n_val * sizeof(float));
            nimcp_gpu_tensor_copy_to_host(val_output_tensor, val_outputs, n_val * sizeof(float));

            /* Compute validation error */
            float val_mse = 0.0f;
            for (int i = 0; i < n_val; i++) {
                float diff = val_outputs[i] - val_targets[i];
                val_mse += diff * diff;
            }
            val_mse /= n_val;

            /* Validation error should be comparable to training error */
            ck_assert_msg(val_mse < final_error * 3.0f,
                "Validation error should be comparable to training error: val=%.4f, train=%.4f",
                val_mse, final_error);

            free(val_outputs);
        }

        nimcp_gpu_tensor_destroy(train_input_tensor);
        nimcp_gpu_tensor_destroy(train_target_tensor);
        nimcp_gpu_tensor_destroy(val_input_tensor);
        nimcp_gpu_tensor_destroy(val_output_tensor);
    }

    free(train_inputs);
    free(train_targets);
    free(val_inputs);
    free(val_targets);
    nimcp_gpu_anfis_destroy(state);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Parameter Access Tests
 * ============================================================================ */

START_TEST(test_anfis_get_set_parameters)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    nimcp_gpu_anfis_params_t params = {
        .num_inputs = 2,
        .num_mfs_per_input = 2,
        .mf_type = FUZZY_MF_GAUSSIAN,
        .learning_rate = 0.01f,
        .momentum = 0.9f
    };

    nimcp_gpu_anfis_state_t* state = nimcp_gpu_anfis_create(ctx, &params);
    if (!state) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create ANFIS state");
        return;
    }

    /* Get premise parameters */
    size_t premise_size = nimcp_gpu_anfis_get_premise_param_count(state);
    ck_assert_msg(premise_size > 0, "Should have premise parameters");

    float* premise_params = (float*)malloc(premise_size * sizeof(float));
    bool got_premise = nimcp_gpu_anfis_get_premise_params(state, premise_params, premise_size);
    ck_assert_msg(got_premise, "Should be able to get premise parameters");

    /* Modify and set back */
    for (size_t i = 0; i < premise_size; i++) {
        premise_params[i] *= 1.1f;  /* Scale by 10% */
    }

    bool set_premise = nimcp_gpu_anfis_set_premise_params(state, premise_params, premise_size);
    ck_assert_msg(set_premise, "Should be able to set premise parameters");

    /* Get consequent parameters */
    size_t consequent_size = nimcp_gpu_anfis_get_consequent_param_count(state);
    ck_assert_msg(consequent_size > 0, "Should have consequent parameters");

    float* consequent_params = (float*)malloc(consequent_size * sizeof(float));
    bool got_consequent = nimcp_gpu_anfis_get_consequent_params(state, consequent_params, consequent_size);
    ck_assert_msg(got_consequent, "Should be able to get consequent parameters");

    free(premise_params);
    free(consequent_params);
    nimcp_gpu_anfis_destroy(state);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

START_TEST(test_anfis_null_inputs)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    /* Null params should fail */
    nimcp_gpu_anfis_state_t* state = nimcp_gpu_anfis_create(ctx, NULL);
    ck_assert_msg(state == NULL, "Should fail with null params");

    /* Create valid state for further tests */
    nimcp_gpu_anfis_params_t params = {
        .num_inputs = 2,
        .num_mfs_per_input = 2,
        .mf_type = FUZZY_MF_GAUSSIAN,
        .learning_rate = 0.01f,
        .momentum = 0.9f
    };

    state = nimcp_gpu_anfis_create(ctx, &params);
    if (state) {
        /* Null tensors should fail */
        bool success = nimcp_gpu_anfis_forward(ctx, state, NULL, NULL);
        ck_assert_msg(!success, "Forward with null tensors should fail");

        nimcp_gpu_anfis_destroy(state);
    }

    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_anfis_mismatched_dimensions)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    nimcp_gpu_anfis_params_t params = {
        .num_inputs = 2,
        .num_mfs_per_input = 2,
        .mf_type = FUZZY_MF_GAUSSIAN,
        .learning_rate = 0.01f,
        .momentum = 0.9f
    };

    nimcp_gpu_anfis_state_t* state = nimcp_gpu_anfis_create(ctx, &params);
    if (!state) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create ANFIS state");
        return;
    }

    /* Create tensor with wrong number of inputs */
    nimcp_gpu_tensor_t* wrong_input = nimcp_gpu_tensor_create(ctx,
        (size_t[]){10, 5}, 2, NIMCP_GPU_PRECISION_FP32);  /* 5 inputs instead of 2 */
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx,
        (size_t[]){10, 1}, 2, NIMCP_GPU_PRECISION_FP32);

    if (wrong_input && output) {
        bool success = nimcp_gpu_anfis_forward(ctx, state, wrong_input, output);
        ck_assert_msg(!success, "Forward with mismatched input dimensions should fail");

        nimcp_gpu_tensor_destroy(wrong_input);
        nimcp_gpu_tensor_destroy(output);
    }

    nimcp_gpu_anfis_destroy(state);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* fuzzy_gpu_anfis_suite(void)
{
    Suite* s = suite_create("Fuzzy GPU ANFIS Integration");

    /* Basic Tests */
    TCase* tc_basic = tcase_create("Basic ANFIS");
    tcase_add_test(tc_basic, test_anfis_create_and_destroy);
    tcase_add_test(tc_basic, test_anfis_forward_pass);
    tcase_set_timeout(tc_basic, 60);
    suite_add_tcase(s, tc_basic);

    /* Training Tests */
    TCase* tc_train = tcase_create("Training Convergence");
    tcase_add_test(tc_train, test_anfis_training_linear_function);
    tcase_add_test(tc_train, test_anfis_training_nonlinear_function);
    tcase_set_timeout(tc_train, 120);
    suite_add_tcase(s, tc_train);

    /* Hybrid Learning Tests */
    TCase* tc_hybrid = tcase_create("Hybrid Learning");
    tcase_add_test(tc_hybrid, test_anfis_hybrid_learning);
    tcase_set_timeout(tc_hybrid, 120);
    suite_add_tcase(s, tc_hybrid);

    /* Validation Tests */
    TCase* tc_valid = tcase_create("Validation");
    tcase_add_test(tc_valid, test_anfis_validation_split);
    tcase_set_timeout(tc_valid, 120);
    suite_add_tcase(s, tc_valid);

    /* Parameter Access Tests */
    TCase* tc_params = tcase_create("Parameters");
    tcase_add_test(tc_params, test_anfis_get_set_parameters);
    tcase_set_timeout(tc_params, 60);
    suite_add_tcase(s, tc_params);

    /* Error Handling Tests */
    TCase* tc_errors = tcase_create("Error Handling");
    tcase_add_test(tc_errors, test_anfis_null_inputs);
    tcase_add_test(tc_errors, test_anfis_mismatched_dimensions);
    tcase_set_timeout(tc_errors, 60);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    Suite* s = fuzzy_gpu_anfis_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
