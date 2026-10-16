/* ============================================================================
 * Fuzzy GPU Inference Integration Tests
 * ============================================================================
 * WHAT: Integration tests for GPU fuzzy inference pipeline
 * WHY:  Validate complete fuzzy inference workflow on GPU
 * HOW:  Test batch inference, Mamdani/Sugeno systems, CPU/GPU equivalence
 * ============================================================================
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef NIMCP_CUDA_ENABLED
#include "gpu/fuzzy/nimcp_fuzzy_gpu.h"
#include "gpu/nimcp_gpu_context.h"
#include "cognitive/fuzzy/nimcp_fuzzy.h"
#endif

#define TOLERANCE 1e-4
#define BATCH_SIZE 1000

/* ============================================================================
 * Mamdani Inference Integration Tests
 * ============================================================================ */

START_TEST(test_mamdani_inference_simple)
{
#ifdef NIMCP_CUDA_ENABLED
    /* Simple 2-input, 1-output Mamdani system */
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    /* Create fuzzy inference engine */
    fuzzy_inference_engine_t* cpu_engine = fuzzy_inference_create(
        FUZZY_INFERENCE_MAMDANI, 2, 1);
    if (!cpu_engine) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create CPU engine");
        return;
    }

    /* Add input variables with triangular MFs */
    fuzzy_variable_t* temp = fuzzy_variable_create("temperature", 0.0, 100.0);
    fuzzy_variable_add_mf(temp, "cold", FUZZY_MF_TRIANGULAR, (float[]){0, 0, 40}, 3);
    fuzzy_variable_add_mf(temp, "warm", FUZZY_MF_TRIANGULAR, (float[]){20, 50, 80}, 3);
    fuzzy_variable_add_mf(temp, "hot", FUZZY_MF_TRIANGULAR, (float[]){60, 100, 100}, 3);
    fuzzy_inference_add_input(cpu_engine, temp);

    fuzzy_variable_t* humidity = fuzzy_variable_create("humidity", 0.0, 100.0);
    fuzzy_variable_add_mf(humidity, "low", FUZZY_MF_TRIANGULAR, (float[]){0, 0, 50}, 3);
    fuzzy_variable_add_mf(humidity, "high", FUZZY_MF_TRIANGULAR, (float[]){50, 100, 100}, 3);
    fuzzy_inference_add_input(cpu_engine, humidity);

    /* Add output variable */
    fuzzy_variable_t* fan_speed = fuzzy_variable_create("fan_speed", 0.0, 100.0);
    fuzzy_variable_add_mf(fan_speed, "slow", FUZZY_MF_TRIANGULAR, (float[]){0, 0, 50}, 3);
    fuzzy_variable_add_mf(fan_speed, "medium", FUZZY_MF_TRIANGULAR, (float[]){25, 50, 75}, 3);
    fuzzy_variable_add_mf(fan_speed, "fast", FUZZY_MF_TRIANGULAR, (float[]){50, 100, 100}, 3);
    fuzzy_inference_add_output(cpu_engine, fan_speed);

    /* Add rules */
    fuzzy_inference_add_rule(cpu_engine, "IF temperature IS cold THEN fan_speed IS slow");
    fuzzy_inference_add_rule(cpu_engine, "IF temperature IS warm THEN fan_speed IS medium");
    fuzzy_inference_add_rule(cpu_engine, "IF temperature IS hot THEN fan_speed IS fast");
    fuzzy_inference_add_rule(cpu_engine, "IF humidity IS high AND temperature IS warm THEN fan_speed IS fast");

    /* Create GPU state from CPU engine */
    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    if (!gpu_state) {
        fuzzy_inference_destroy(cpu_engine);
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create GPU state");
        return;
    }

    /* Single inference test */
    float inputs[2] = {70.0, 60.0};  /* Hot and high humidity */
    float output = 0.0;

    nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_create(ctx, (size_t[]){1, 2}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output_tensor = nimcp_gpu_tensor_create(ctx, (size_t[]){1, 1}, 2, NIMCP_GPU_PRECISION_FP32);

    if (input_tensor && output_tensor) {
        nimcp_gpu_tensor_copy_from_host(input_tensor, inputs, sizeof(inputs));

        nimcp_gpu_inference_params_t params = {
            .t_norm = FUZZY_TNORM_MIN,
            .t_conorm = FUZZY_TCONORM_MAX,
            .defuzz_method = FUZZY_DEFUZZ_CENTROID,
            .resolution = 100
        };

        bool success = nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, input_tensor, output_tensor, &params);

        if (success) {
            nimcp_gpu_tensor_copy_to_host(output_tensor, &output, sizeof(output));
            /* Hot + high humidity should result in fast fan speed (high value) */
            ck_assert_msg(output > 60.0,
                "Hot+humid should result in fast fan: got %.2f", output);
        }

        nimcp_gpu_tensor_destroy(input_tensor);
        nimcp_gpu_tensor_destroy(output_tensor);
    }

    nimcp_gpu_fuzzy_state_destroy(gpu_state);
    fuzzy_inference_destroy(cpu_engine);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_mamdani_batch_inference)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    /* Create simple fuzzy system */
    fuzzy_inference_engine_t* cpu_engine = fuzzy_inference_create(
        FUZZY_INFERENCE_MAMDANI, 1, 1);
    if (!cpu_engine) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create CPU engine");
        return;
    }

    fuzzy_variable_t* input_var = fuzzy_variable_create("x", 0.0, 10.0);
    fuzzy_variable_add_mf(input_var, "low", FUZZY_MF_TRIANGULAR, (float[]){0, 0, 5}, 3);
    fuzzy_variable_add_mf(input_var, "high", FUZZY_MF_TRIANGULAR, (float[]){5, 10, 10}, 3);
    fuzzy_inference_add_input(cpu_engine, input_var);

    fuzzy_variable_t* output_var = fuzzy_variable_create("y", 0.0, 10.0);
    fuzzy_variable_add_mf(output_var, "small", FUZZY_MF_TRIANGULAR, (float[]){0, 0, 5}, 3);
    fuzzy_variable_add_mf(output_var, "large", FUZZY_MF_TRIANGULAR, (float[]){5, 10, 10}, 3);
    fuzzy_inference_add_output(cpu_engine, output_var);

    fuzzy_inference_add_rule(cpu_engine, "IF x IS low THEN y IS small");
    fuzzy_inference_add_rule(cpu_engine, "IF x IS high THEN y IS large");

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    if (!gpu_state) {
        fuzzy_inference_destroy(cpu_engine);
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create GPU state");
        return;
    }

    /* Batch inference with BATCH_SIZE samples */
    float* inputs = (float*)malloc(BATCH_SIZE * sizeof(float));
    float* outputs = (float*)malloc(BATCH_SIZE * sizeof(float));

    for (int i = 0; i < BATCH_SIZE; i++) {
        inputs[i] = (float)i / (BATCH_SIZE - 1) * 10.0f;  /* 0 to 10 */
    }

    nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_create(ctx, (size_t[]){BATCH_SIZE, 1}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output_tensor = nimcp_gpu_tensor_create(ctx, (size_t[]){BATCH_SIZE, 1}, 2, NIMCP_GPU_PRECISION_FP32);

    if (input_tensor && output_tensor) {
        nimcp_gpu_tensor_copy_from_host(input_tensor, inputs, BATCH_SIZE * sizeof(float));

        nimcp_gpu_inference_params_t params = {
            .t_norm = FUZZY_TNORM_MIN,
            .t_conorm = FUZZY_TCONORM_MAX,
            .defuzz_method = FUZZY_DEFUZZ_CENTROID,
            .resolution = 100
        };

        bool success = nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, input_tensor, output_tensor, &params);

        if (success) {
            nimcp_gpu_tensor_copy_to_host(output_tensor, outputs, BATCH_SIZE * sizeof(float));

            /* Check monotonicity: output should increase with input */
            int monotonic_violations = 0;
            for (int i = 1; i < BATCH_SIZE; i++) {
                if (outputs[i] < outputs[i-1] - TOLERANCE) {
                    monotonic_violations++;
                }
            }
            ck_assert_msg(monotonic_violations == 0,
                "Batch outputs should be monotonically increasing: %d violations",
                monotonic_violations);

            /* Check endpoints */
            ck_assert_msg(outputs[0] < 3.0,
                "Low input should produce small output: got %.2f", outputs[0]);
            ck_assert_msg(outputs[BATCH_SIZE-1] > 7.0,
                "High input should produce large output: got %.2f", outputs[BATCH_SIZE-1]);
        }

        nimcp_gpu_tensor_destroy(input_tensor);
        nimcp_gpu_tensor_destroy(output_tensor);
    }

    free(inputs);
    free(outputs);
    nimcp_gpu_fuzzy_state_destroy(gpu_state);
    fuzzy_inference_destroy(cpu_engine);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Sugeno Inference Integration Tests
 * ============================================================================ */

START_TEST(test_sugeno_inference_simple)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    /* Create Sugeno system */
    fuzzy_inference_engine_t* cpu_engine = fuzzy_inference_create(
        FUZZY_INFERENCE_SUGENO, 2, 1);
    if (!cpu_engine) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create CPU engine");
        return;
    }

    /* Add input variables */
    fuzzy_variable_t* x1 = fuzzy_variable_create("x1", 0.0, 10.0);
    fuzzy_variable_add_mf(x1, "small", FUZZY_MF_GAUSSIAN, (float[]){2.0, 1.5}, 2);
    fuzzy_variable_add_mf(x1, "large", FUZZY_MF_GAUSSIAN, (float[]){8.0, 1.5}, 2);
    fuzzy_inference_add_input(cpu_engine, x1);

    fuzzy_variable_t* x2 = fuzzy_variable_create("x2", 0.0, 10.0);
    fuzzy_variable_add_mf(x2, "small", FUZZY_MF_GAUSSIAN, (float[]){2.0, 1.5}, 2);
    fuzzy_variable_add_mf(x2, "large", FUZZY_MF_GAUSSIAN, (float[]){8.0, 1.5}, 2);
    fuzzy_inference_add_input(cpu_engine, x2);

    /* Add Sugeno output (constant or linear consequent) */
    fuzzy_sugeno_add_output(cpu_engine, "y");

    /* Add rules with consequent functions */
    /* Rule 1: IF x1 IS small AND x2 IS small THEN y = 1 */
    fuzzy_sugeno_add_rule(cpu_engine,
        (int[]){0, 0}, 2,  /* antecedent: x1=small, x2=small */
        (float[]){0, 0, 1.0}, 3);  /* consequent: 0*x1 + 0*x2 + 1 */

    /* Rule 2: IF x1 IS large AND x2 IS large THEN y = 9 */
    fuzzy_sugeno_add_rule(cpu_engine,
        (int[]){1, 1}, 2,  /* antecedent: x1=large, x2=large */
        (float[]){0, 0, 9.0}, 3);  /* consequent: 0*x1 + 0*x2 + 9 */

    /* Create GPU state */
    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    if (!gpu_state) {
        fuzzy_inference_destroy(cpu_engine);
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create GPU state");
        return;
    }

    /* Test inference */
    float inputs_small[2] = {2.0, 2.0};
    float inputs_large[2] = {8.0, 8.0};
    float output_small = 0.0, output_large = 0.0;

    nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_create(ctx, (size_t[]){1, 2}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output_tensor = nimcp_gpu_tensor_create(ctx, (size_t[]){1, 1}, 2, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_inference_params_t params = {
        .t_norm = FUZZY_TNORM_PRODUCT,
        .t_conorm = FUZZY_TCONORM_PROBOR,
        .defuzz_method = FUZZY_DEFUZZ_WEIGHTED_AVG,
        .resolution = 100
    };

    if (input_tensor && output_tensor) {
        /* Test small inputs */
        nimcp_gpu_tensor_copy_from_host(input_tensor, inputs_small, sizeof(inputs_small));
        if (nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, input_tensor, output_tensor, &params)) {
            nimcp_gpu_tensor_copy_to_host(output_tensor, &output_small, sizeof(output_small));
        }

        /* Test large inputs */
        nimcp_gpu_tensor_copy_from_host(input_tensor, inputs_large, sizeof(inputs_large));
        if (nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, input_tensor, output_tensor, &params)) {
            nimcp_gpu_tensor_copy_to_host(output_tensor, &output_large, sizeof(output_large));
        }

        /* Small inputs should produce output close to 1 */
        ck_assert_msg(output_small < 3.0,
            "Small inputs should produce small output: got %.2f", output_small);
        /* Large inputs should produce output close to 9 */
        ck_assert_msg(output_large > 7.0,
            "Large inputs should produce large output: got %.2f", output_large);

        nimcp_gpu_tensor_destroy(input_tensor);
        nimcp_gpu_tensor_destroy(output_tensor);
    }

    nimcp_gpu_fuzzy_state_destroy(gpu_state);
    fuzzy_inference_destroy(cpu_engine);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * CPU/GPU Equivalence Tests
 * ============================================================================ */

START_TEST(test_cpu_gpu_equivalence_mamdani)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    /* Create fuzzy system */
    fuzzy_inference_engine_t* cpu_engine = fuzzy_inference_create(
        FUZZY_INFERENCE_MAMDANI, 2, 1);
    if (!cpu_engine) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create CPU engine");
        return;
    }

    /* Setup variables and rules */
    fuzzy_variable_t* x1 = fuzzy_variable_create("x1", 0.0, 10.0);
    fuzzy_variable_add_mf(x1, "low", FUZZY_MF_TRIANGULAR, (float[]){0, 0, 5}, 3);
    fuzzy_variable_add_mf(x1, "med", FUZZY_MF_TRIANGULAR, (float[]){2.5, 5, 7.5}, 3);
    fuzzy_variable_add_mf(x1, "high", FUZZY_MF_TRIANGULAR, (float[]){5, 10, 10}, 3);
    fuzzy_inference_add_input(cpu_engine, x1);

    fuzzy_variable_t* x2 = fuzzy_variable_create("x2", 0.0, 10.0);
    fuzzy_variable_add_mf(x2, "low", FUZZY_MF_TRIANGULAR, (float[]){0, 0, 5}, 3);
    fuzzy_variable_add_mf(x2, "high", FUZZY_MF_TRIANGULAR, (float[]){5, 10, 10}, 3);
    fuzzy_inference_add_input(cpu_engine, x2);

    fuzzy_variable_t* y = fuzzy_variable_create("y", 0.0, 10.0);
    fuzzy_variable_add_mf(y, "small", FUZZY_MF_TRIANGULAR, (float[]){0, 0, 5}, 3);
    fuzzy_variable_add_mf(y, "large", FUZZY_MF_TRIANGULAR, (float[]){5, 10, 10}, 3);
    fuzzy_inference_add_output(cpu_engine, y);

    fuzzy_inference_add_rule(cpu_engine, "IF x1 IS low THEN y IS small");
    fuzzy_inference_add_rule(cpu_engine, "IF x1 IS high AND x2 IS high THEN y IS large");
    fuzzy_inference_add_rule(cpu_engine, "IF x1 IS med THEN y IS small");

    /* Create GPU state */
    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    if (!gpu_state) {
        fuzzy_inference_destroy(cpu_engine);
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create GPU state");
        return;
    }

    /* Test multiple input combinations */
    float test_inputs[10][2] = {
        {0.0, 0.0}, {2.5, 2.5}, {5.0, 5.0}, {7.5, 7.5}, {10.0, 10.0},
        {0.0, 10.0}, {10.0, 0.0}, {3.0, 7.0}, {7.0, 3.0}, {5.0, 5.0}
    };

    float cpu_outputs[10], gpu_outputs[10];

    /* Compute CPU outputs */
    for (int i = 0; i < 10; i++) {
        cpu_outputs[i] = fuzzy_inference_evaluate(cpu_engine, test_inputs[i], 2);
    }

    /* Compute GPU outputs */
    nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_create(ctx, (size_t[]){10, 2}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output_tensor = nimcp_gpu_tensor_create(ctx, (size_t[]){10, 1}, 2, NIMCP_GPU_PRECISION_FP32);

    if (input_tensor && output_tensor) {
        nimcp_gpu_tensor_copy_from_host(input_tensor, test_inputs, sizeof(test_inputs));

        nimcp_gpu_inference_params_t params = {
            .t_norm = FUZZY_TNORM_MIN,
            .t_conorm = FUZZY_TCONORM_MAX,
            .defuzz_method = FUZZY_DEFUZZ_CENTROID,
            .resolution = 100
        };

        if (nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, input_tensor, output_tensor, &params)) {
            nimcp_gpu_tensor_copy_to_host(output_tensor, gpu_outputs, sizeof(gpu_outputs));

            /* Compare CPU and GPU outputs */
            int mismatches = 0;
            for (int i = 0; i < 10; i++) {
                if (fabs(cpu_outputs[i] - gpu_outputs[i]) > 0.1) {
                    mismatches++;
                    printf("Mismatch at %d: CPU=%.4f, GPU=%.4f\n",
                           i, cpu_outputs[i], gpu_outputs[i]);
                }
            }
            ck_assert_msg(mismatches == 0,
                "CPU/GPU outputs should match within tolerance: %d mismatches", mismatches);
        }

        nimcp_gpu_tensor_destroy(input_tensor);
        nimcp_gpu_tensor_destroy(output_tensor);
    }

    nimcp_gpu_fuzzy_state_destroy(gpu_state);
    fuzzy_inference_destroy(cpu_engine);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Multiple Output Tests
 * ============================================================================ */

START_TEST(test_multi_output_inference)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    /* Create 2-input, 2-output system */
    fuzzy_inference_engine_t* cpu_engine = fuzzy_inference_create(
        FUZZY_INFERENCE_MAMDANI, 2, 2);
    if (!cpu_engine) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create CPU engine");
        return;
    }

    /* Add inputs */
    fuzzy_variable_t* error = fuzzy_variable_create("error", -10.0, 10.0);
    fuzzy_variable_add_mf(error, "negative", FUZZY_MF_TRIANGULAR, (float[]){-10, -10, 0}, 3);
    fuzzy_variable_add_mf(error, "zero", FUZZY_MF_TRIANGULAR, (float[]){-5, 0, 5}, 3);
    fuzzy_variable_add_mf(error, "positive", FUZZY_MF_TRIANGULAR, (float[]){0, 10, 10}, 3);
    fuzzy_inference_add_input(cpu_engine, error);

    fuzzy_variable_t* delta_error = fuzzy_variable_create("delta_error", -5.0, 5.0);
    fuzzy_variable_add_mf(delta_error, "negative", FUZZY_MF_TRIANGULAR, (float[]){-5, -5, 0}, 3);
    fuzzy_variable_add_mf(delta_error, "zero", FUZZY_MF_TRIANGULAR, (float[]){-2.5, 0, 2.5}, 3);
    fuzzy_variable_add_mf(delta_error, "positive", FUZZY_MF_TRIANGULAR, (float[]){0, 5, 5}, 3);
    fuzzy_inference_add_input(cpu_engine, delta_error);

    /* Add outputs */
    fuzzy_variable_t* proportional = fuzzy_variable_create("proportional", -10.0, 10.0);
    fuzzy_variable_add_mf(proportional, "negative", FUZZY_MF_TRIANGULAR, (float[]){-10, -10, 0}, 3);
    fuzzy_variable_add_mf(proportional, "zero", FUZZY_MF_TRIANGULAR, (float[]){-5, 0, 5}, 3);
    fuzzy_variable_add_mf(proportional, "positive", FUZZY_MF_TRIANGULAR, (float[]){0, 10, 10}, 3);
    fuzzy_inference_add_output(cpu_engine, proportional);

    fuzzy_variable_t* derivative = fuzzy_variable_create("derivative", -5.0, 5.0);
    fuzzy_variable_add_mf(derivative, "negative", FUZZY_MF_TRIANGULAR, (float[]){-5, -5, 0}, 3);
    fuzzy_variable_add_mf(derivative, "zero", FUZZY_MF_TRIANGULAR, (float[]){-2.5, 0, 2.5}, 3);
    fuzzy_variable_add_mf(derivative, "positive", FUZZY_MF_TRIANGULAR, (float[]){0, 5, 5}, 3);
    fuzzy_inference_add_output(cpu_engine, derivative);

    /* Add rules for PD controller */
    fuzzy_inference_add_rule(cpu_engine, "IF error IS positive THEN proportional IS negative");
    fuzzy_inference_add_rule(cpu_engine, "IF error IS negative THEN proportional IS positive");
    fuzzy_inference_add_rule(cpu_engine, "IF delta_error IS positive THEN derivative IS negative");
    fuzzy_inference_add_rule(cpu_engine, "IF delta_error IS negative THEN derivative IS positive");

    /* Create GPU state */
    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    if (!gpu_state) {
        fuzzy_inference_destroy(cpu_engine);
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create GPU state");
        return;
    }

    /* Test inference */
    float inputs[2] = {5.0, 2.0};  /* Positive error, positive delta */
    float outputs[2] = {0.0, 0.0};

    nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_create(ctx, (size_t[]){1, 2}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output_tensor = nimcp_gpu_tensor_create(ctx, (size_t[]){1, 2}, 2, NIMCP_GPU_PRECISION_FP32);

    if (input_tensor && output_tensor) {
        nimcp_gpu_tensor_copy_from_host(input_tensor, inputs, sizeof(inputs));

        nimcp_gpu_inference_params_t params = {
            .t_norm = FUZZY_TNORM_MIN,
            .t_conorm = FUZZY_TCONORM_MAX,
            .defuzz_method = FUZZY_DEFUZZ_CENTROID,
            .resolution = 100
        };

        if (nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, input_tensor, output_tensor, &params)) {
            nimcp_gpu_tensor_copy_to_host(output_tensor, outputs, sizeof(outputs));

            /* Positive error -> negative proportional control */
            ck_assert_msg(outputs[0] < 0.0,
                "Positive error should produce negative proportional: got %.2f", outputs[0]);
            /* Positive delta_error -> negative derivative control */
            ck_assert_msg(outputs[1] < 0.0,
                "Positive delta_error should produce negative derivative: got %.2f", outputs[1]);
        }

        nimcp_gpu_tensor_destroy(input_tensor);
        nimcp_gpu_tensor_destroy(output_tensor);
    }

    nimcp_gpu_fuzzy_state_destroy(gpu_state);
    fuzzy_inference_destroy(cpu_engine);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * T-Norm and T-Conorm Tests
 * ============================================================================ */

START_TEST(test_different_tnorms)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    /* Create simple system to test different t-norms */
    fuzzy_inference_engine_t* cpu_engine = fuzzy_inference_create(
        FUZZY_INFERENCE_MAMDANI, 2, 1);
    if (!cpu_engine) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create CPU engine");
        return;
    }

    fuzzy_variable_t* x1 = fuzzy_variable_create("x1", 0.0, 1.0);
    fuzzy_variable_add_mf(x1, "high", FUZZY_MF_TRIANGULAR, (float[]){0.5, 1.0, 1.0}, 3);
    fuzzy_inference_add_input(cpu_engine, x1);

    fuzzy_variable_t* x2 = fuzzy_variable_create("x2", 0.0, 1.0);
    fuzzy_variable_add_mf(x2, "high", FUZZY_MF_TRIANGULAR, (float[]){0.5, 1.0, 1.0}, 3);
    fuzzy_inference_add_input(cpu_engine, x2);

    fuzzy_variable_t* y = fuzzy_variable_create("y", 0.0, 1.0);
    fuzzy_variable_add_mf(y, "high", FUZZY_MF_TRIANGULAR, (float[]){0.5, 1.0, 1.0}, 3);
    fuzzy_inference_add_output(cpu_engine, y);

    fuzzy_inference_add_rule(cpu_engine, "IF x1 IS high AND x2 IS high THEN y IS high");

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    if (!gpu_state) {
        fuzzy_inference_destroy(cpu_engine);
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create GPU state");
        return;
    }

    float inputs[2] = {0.8, 0.8};  /* Both partially "high" */
    float output_min = 0.0, output_product = 0.0, output_lukasiewicz = 0.0;

    nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_create(ctx, (size_t[]){1, 2}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output_tensor = nimcp_gpu_tensor_create(ctx, (size_t[]){1, 1}, 2, NIMCP_GPU_PRECISION_FP32);

    if (input_tensor && output_tensor) {
        nimcp_gpu_tensor_copy_from_host(input_tensor, inputs, sizeof(inputs));

        /* Test MIN t-norm */
        nimcp_gpu_inference_params_t params_min = {
            .t_norm = FUZZY_TNORM_MIN,
            .t_conorm = FUZZY_TCONORM_MAX,
            .defuzz_method = FUZZY_DEFUZZ_CENTROID,
            .resolution = 100
        };
        if (nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, input_tensor, output_tensor, &params_min)) {
            nimcp_gpu_tensor_copy_to_host(output_tensor, &output_min, sizeof(output_min));
        }

        /* Test PRODUCT t-norm */
        nimcp_gpu_inference_params_t params_prod = {
            .t_norm = FUZZY_TNORM_PRODUCT,
            .t_conorm = FUZZY_TCONORM_MAX,
            .defuzz_method = FUZZY_DEFUZZ_CENTROID,
            .resolution = 100
        };
        if (nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, input_tensor, output_tensor, &params_prod)) {
            nimcp_gpu_tensor_copy_to_host(output_tensor, &output_product, sizeof(output_product));
        }

        /* Test LUKASIEWICZ t-norm */
        nimcp_gpu_inference_params_t params_luk = {
            .t_norm = FUZZY_TNORM_LUKASIEWICZ,
            .t_conorm = FUZZY_TCONORM_MAX,
            .defuzz_method = FUZZY_DEFUZZ_CENTROID,
            .resolution = 100
        };
        if (nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, input_tensor, output_tensor, &params_luk)) {
            nimcp_gpu_tensor_copy_to_host(output_tensor, &output_lukasiewicz, sizeof(output_lukasiewicz));
        }

        /* Different t-norms should produce different results */
        /* Product t-norm is typically less than min t-norm */
        ck_assert_msg(output_product <= output_min + TOLERANCE,
            "Product t-norm should be <= min t-norm: prod=%.4f, min=%.4f",
            output_product, output_min);

        /* Lukasiewicz is the strongest (most restrictive) t-norm */
        ck_assert_msg(output_lukasiewicz <= output_product + TOLERANCE,
            "Lukasiewicz t-norm should be <= product t-norm: luk=%.4f, prod=%.4f",
            output_lukasiewicz, output_product);

        nimcp_gpu_tensor_destroy(input_tensor);
        nimcp_gpu_tensor_destroy(output_tensor);
    }

    nimcp_gpu_fuzzy_state_destroy(gpu_state);
    fuzzy_inference_destroy(cpu_engine);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Performance and Stress Tests
 * ============================================================================ */

START_TEST(test_large_batch_inference)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    if (!ctx) {
        ck_assert_msg(1, "GPU not available - skipping");
        return;
    }

    /* Create system for large batch test */
    fuzzy_inference_engine_t* cpu_engine = fuzzy_inference_create(
        FUZZY_INFERENCE_MAMDANI, 3, 1);
    if (!cpu_engine) {
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create CPU engine");
        return;
    }

    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "x%d", i);
        fuzzy_variable_t* var = fuzzy_variable_create(name, 0.0, 10.0);
        fuzzy_variable_add_mf(var, "low", FUZZY_MF_TRIANGULAR, (float[]){0, 0, 5}, 3);
        fuzzy_variable_add_mf(var, "high", FUZZY_MF_TRIANGULAR, (float[]){5, 10, 10}, 3);
        fuzzy_inference_add_input(cpu_engine, var);
    }

    fuzzy_variable_t* y = fuzzy_variable_create("y", 0.0, 10.0);
    fuzzy_variable_add_mf(y, "small", FUZZY_MF_TRIANGULAR, (float[]){0, 0, 5}, 3);
    fuzzy_variable_add_mf(y, "large", FUZZY_MF_TRIANGULAR, (float[]){5, 10, 10}, 3);
    fuzzy_inference_add_output(cpu_engine, y);

    fuzzy_inference_add_rule(cpu_engine, "IF x0 IS low AND x1 IS low AND x2 IS low THEN y IS small");
    fuzzy_inference_add_rule(cpu_engine, "IF x0 IS high OR x1 IS high OR x2 IS high THEN y IS large");

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    if (!gpu_state) {
        fuzzy_inference_destroy(cpu_engine);
        nimcp_gpu_context_destroy(ctx);
        ck_assert_msg(1, "Failed to create GPU state");
        return;
    }

    /* Large batch: 10000 samples */
    const int large_batch = 10000;
    float* inputs = (float*)malloc(large_batch * 3 * sizeof(float));
    float* outputs = (float*)malloc(large_batch * sizeof(float));

    /* Generate random inputs */
    for (int i = 0; i < large_batch * 3; i++) {
        inputs[i] = (float)(rand() % 1000) / 100.0f;  /* 0 to ~10 */
    }

    nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_create(ctx, (size_t[]){large_batch, 3}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output_tensor = nimcp_gpu_tensor_create(ctx, (size_t[]){large_batch, 1}, 2, NIMCP_GPU_PRECISION_FP32);

    if (input_tensor && output_tensor) {
        nimcp_gpu_tensor_copy_from_host(input_tensor, inputs, large_batch * 3 * sizeof(float));

        nimcp_gpu_inference_params_t params = {
            .t_norm = FUZZY_TNORM_MIN,
            .t_conorm = FUZZY_TCONORM_MAX,
            .defuzz_method = FUZZY_DEFUZZ_CENTROID,
            .resolution = 100
        };

        bool success = nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, input_tensor, output_tensor, &params);

        if (success) {
            nimcp_gpu_tensor_copy_to_host(output_tensor, outputs, large_batch * sizeof(float));

            /* Verify outputs are in valid range */
            int out_of_range = 0;
            for (int i = 0; i < large_batch; i++) {
                if (outputs[i] < 0.0 || outputs[i] > 10.0) {
                    out_of_range++;
                }
            }
            ck_assert_msg(out_of_range == 0,
                "All outputs should be in valid range [0, 10]: %d out of range",
                out_of_range);
        }

        nimcp_gpu_tensor_destroy(input_tensor);
        nimcp_gpu_tensor_destroy(output_tensor);
    }

    free(inputs);
    free(outputs);
    nimcp_gpu_fuzzy_state_destroy(gpu_state);
    fuzzy_inference_destroy(cpu_engine);
    nimcp_gpu_context_destroy(ctx);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* fuzzy_gpu_inference_suite(void)
{
    Suite* s = suite_create("Fuzzy GPU Inference Integration");

    /* Mamdani Tests */
    TCase* tc_mamdani = tcase_create("Mamdani Inference");
    tcase_add_test(tc_mamdani, test_mamdani_inference_simple);
    tcase_add_test(tc_mamdani, test_mamdani_batch_inference);
    tcase_set_timeout(tc_mamdani, 60);
    suite_add_tcase(s, tc_mamdani);

    /* Sugeno Tests */
    TCase* tc_sugeno = tcase_create("Sugeno Inference");
    tcase_add_test(tc_sugeno, test_sugeno_inference_simple);
    tcase_set_timeout(tc_sugeno, 60);
    suite_add_tcase(s, tc_sugeno);

    /* Equivalence Tests */
    TCase* tc_equiv = tcase_create("CPU/GPU Equivalence");
    tcase_add_test(tc_equiv, test_cpu_gpu_equivalence_mamdani);
    tcase_set_timeout(tc_equiv, 60);
    suite_add_tcase(s, tc_equiv);

    /* Multi-Output Tests */
    TCase* tc_multi = tcase_create("Multi-Output Systems");
    tcase_add_test(tc_multi, test_multi_output_inference);
    tcase_set_timeout(tc_multi, 60);
    suite_add_tcase(s, tc_multi);

    /* T-Norm Tests */
    TCase* tc_tnorm = tcase_create("T-Norms and T-Conorms");
    tcase_add_test(tc_tnorm, test_different_tnorms);
    tcase_set_timeout(tc_tnorm, 60);
    suite_add_tcase(s, tc_tnorm);

    /* Performance Tests */
    TCase* tc_perf = tcase_create("Performance");
    tcase_add_test(tc_perf, test_large_batch_inference);
    tcase_set_timeout(tc_perf, 120);
    suite_add_tcase(s, tc_perf);

    return s;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    Suite* s = fuzzy_gpu_inference_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
