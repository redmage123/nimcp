/**
 * @file test_phase5_batch8.c
 * @brief Phase 5 Batch 8: ODE Solvers, Brain Parallel Stages, Autodiff Backward
 *
 * Tests for: CPU kernel backend ODE solvers (Heun, RK4, DOPRI5, compute_derivative,
 * update_tau), brain parallel post-forward stages (systems consolidation, WM transfer,
 * semantic memory, Theory of Mind), and tensor autodiff backward pass.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/lnn/nimcp_lnn_gpu.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "core/brain/nimcp_brain_parallel_stages.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-60s", name); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* =========================================================================
 * ODE Solver Tests (via kernel backend function pointer table)
 * ========================================================================= */

static void test_kernel_backend_init_cpu(void) {
    TEST("Kernel Backend: init CPU");
    bool ok = nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);
    ASSERT_TRUE(ok, "CPU backend init failed");
    PASS();
}

static void test_kernel_backend_get(void) {
    TEST("Kernel Backend: get backend non-NULL");
    nimcp_kernel_backend_t* kb = nimcp_get_kernel_backend();
    ASSERT_TRUE(kb != NULL, "backend NULL");
    PASS();
}

static void test_euler_step_basic(void) {
    TEST("Kernel Backend: Euler step basic");
    nimcp_kernel_backend_t* kb = nimcp_get_kernel_backend();
    ASSERT_TRUE(kb != NULL, "backend NULL");
    ASSERT_TRUE(kb->lnn.euler_step != NULL, "euler_step NULL");

    /* Create simple tensors: x=[1,2,3], dx=[0.1,0.2,0.3], dt=1.0 */
    nimcp_gpu_tensor_t x = {0}, dx = {0}, x_new = {0};
    float x_data[3] = {1.0f, 2.0f, 3.0f};
    float dx_data[3] = {0.1f, 0.2f, 0.3f};
    float out_data[3] = {0};

    x.data = x_data; x.numel = 3;
    dx.data = dx_data; dx.numel = 3;
    x_new.data = out_data; x_new.numel = 3;

    nimcp_kernel_error_t err = kb->lnn.euler_step(NULL, &x, &dx, 1.0f, &x_new);
    ASSERT_TRUE(err == NIMCP_KERNEL_SUCCESS, "euler step failed");
    ASSERT_TRUE(fabsf(out_data[0] - 1.1f) < 0.001f, "x[0] wrong");
    ASSERT_TRUE(fabsf(out_data[1] - 2.2f) < 0.001f, "x[1] wrong");
    ASSERT_TRUE(fabsf(out_data[2] - 3.3f) < 0.001f, "x[2] wrong");
    PASS();
}

static void test_euler_step_null(void) {
    TEST("Kernel Backend: Euler step NULL safety");
    nimcp_kernel_backend_t* kb = nimcp_get_kernel_backend();
    ASSERT_TRUE(kb != NULL && kb->lnn.euler_step != NULL, "no backend");
    nimcp_kernel_error_t err = kb->lnn.euler_step(NULL, NULL, NULL, 1.0f, NULL);
    ASSERT_TRUE(err != NIMCP_KERNEL_SUCCESS, "NULL should fail");
    PASS();
}

static void test_heun_step_exists(void) {
    TEST("Kernel Backend: Heun step function exists");
    nimcp_kernel_backend_t* kb = nimcp_get_kernel_backend();
    ASSERT_TRUE(kb != NULL, "backend NULL");
    ASSERT_TRUE(kb->lnn.heun_step != NULL, "heun_step not registered");
    PASS();
}

static void test_heun_step_null(void) {
    TEST("Kernel Backend: Heun step NULL safety");
    nimcp_kernel_backend_t* kb = nimcp_get_kernel_backend();
    ASSERT_TRUE(kb != NULL && kb->lnn.heun_step != NULL, "no backend");
    nimcp_kernel_error_t err = kb->lnn.heun_step(NULL, NULL, NULL, 1.0f, NULL);
    ASSERT_TRUE(err != NIMCP_KERNEL_SUCCESS, "NULL should fail");
    PASS();
}

static void test_rk4_step_exists(void) {
    TEST("Kernel Backend: RK4 step function exists");
    nimcp_kernel_backend_t* kb = nimcp_get_kernel_backend();
    ASSERT_TRUE(kb != NULL, "backend NULL");
    ASSERT_TRUE(kb->lnn.rk4_step != NULL, "rk4_step not registered");
    PASS();
}

static void test_rk4_step_null(void) {
    TEST("Kernel Backend: RK4 step NULL safety");
    nimcp_kernel_backend_t* kb = nimcp_get_kernel_backend();
    ASSERT_TRUE(kb != NULL && kb->lnn.rk4_step != NULL, "no backend");
    nimcp_kernel_error_t err = kb->lnn.rk4_step(NULL, NULL, NULL, 1.0f, NULL);
    ASSERT_TRUE(err != NIMCP_KERNEL_SUCCESS, "NULL should fail");
    PASS();
}

static void test_dopri5_step_exists(void) {
    TEST("Kernel Backend: DOPRI5 step function exists");
    nimcp_kernel_backend_t* kb = nimcp_get_kernel_backend();
    ASSERT_TRUE(kb != NULL, "backend NULL");
    ASSERT_TRUE(kb->lnn.dopri5_step != NULL, "dopri5_step not registered");
    PASS();
}

static void test_dopri5_step_null(void) {
    TEST("Kernel Backend: DOPRI5 step NULL safety");
    nimcp_kernel_backend_t* kb = nimcp_get_kernel_backend();
    ASSERT_TRUE(kb != NULL && kb->lnn.dopri5_step != NULL, "no backend");
    float dt = 0.01f;
    nimcp_kernel_error_t err = kb->lnn.dopri5_step(NULL, NULL, NULL, &dt, NULL);
    ASSERT_TRUE(err != NIMCP_KERNEL_SUCCESS, "NULL should fail");
    PASS();
}

static void test_compute_derivative_exists(void) {
    TEST("Kernel Backend: compute_derivative function exists");
    nimcp_kernel_backend_t* kb = nimcp_get_kernel_backend();
    ASSERT_TRUE(kb != NULL, "backend NULL");
    ASSERT_TRUE(kb->lnn.compute_derivative != NULL, "compute_derivative not registered");
    PASS();
}

static void test_compute_derivative_null(void) {
    TEST("Kernel Backend: compute_derivative NULL safety");
    nimcp_kernel_backend_t* kb = nimcp_get_kernel_backend();
    ASSERT_TRUE(kb != NULL && kb->lnn.compute_derivative != NULL, "no backend");
    nimcp_kernel_error_t err = kb->lnn.compute_derivative(NULL, NULL, NULL, NULL);
    ASSERT_TRUE(err != NIMCP_KERNEL_SUCCESS, "NULL should fail");
    PASS();
}

static void test_update_tau_exists(void) {
    TEST("Kernel Backend: update_tau function exists");
    nimcp_kernel_backend_t* kb = nimcp_get_kernel_backend();
    ASSERT_TRUE(kb != NULL, "backend NULL");
    ASSERT_TRUE(kb->lnn.update_tau != NULL, "update_tau not registered");
    PASS();
}

static void test_update_tau_null(void) {
    TEST("Kernel Backend: update_tau NULL safety");
    nimcp_kernel_backend_t* kb = nimcp_get_kernel_backend();
    ASSERT_TRUE(kb != NULL && kb->lnn.update_tau != NULL, "no backend");
    nimcp_kernel_error_t err = kb->lnn.update_tau(NULL, NULL, NULL);
    ASSERT_TRUE(err != NIMCP_KERNEL_SUCCESS, "NULL should fail");
    PASS();
}

static void test_euler_step_size_mismatch(void) {
    TEST("Kernel Backend: Euler step size mismatch");
    nimcp_kernel_backend_t* kb = nimcp_get_kernel_backend();
    ASSERT_TRUE(kb != NULL && kb->lnn.euler_step != NULL, "no backend");

    float x_data[3] = {1,2,3}, dx_data[2] = {0.1f,0.2f}, out[3] = {0};
    nimcp_gpu_tensor_t x = {.data = x_data, .numel = 3};
    nimcp_gpu_tensor_t dx = {.data = dx_data, .numel = 2};
    nimcp_gpu_tensor_t x_new = {.data = out, .numel = 3};

    nimcp_kernel_error_t err = kb->lnn.euler_step(NULL, &x, &dx, 1.0f, &x_new);
    ASSERT_TRUE(err == NIMCP_KERNEL_ERROR_INVALID_SIZE, "size mismatch should fail");
    PASS();
}

/* =========================================================================
 * Autodiff Backward Tests
 * ========================================================================= */

static void test_autodiff_backward_null(void) {
    TEST("Autodiff: backward NULL safety");
    int rc = nimcp_autodiff_backward(NULL, NULL, NULL, 0, NULL);
    ASSERT_TRUE(rc != 0, "NULL should fail");
    PASS();
}

static void test_autodiff_create_destroy(void) {
    TEST("Autodiff: create and destroy context");
    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_TRUE(ctx != NULL, "ctx NULL");
    nimcp_autodiff_destroy(ctx);
    PASS();
}

static void test_autodiff_start_stop(void) {
    TEST("Autodiff: start and stop recording");
    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_TRUE(ctx != NULL, "ctx NULL");
    int rc1 = nimcp_autodiff_start(ctx);
    ASSERT_TRUE(rc1 == 0, "start failed");
    int rc2 = nimcp_autodiff_stop(ctx);
    ASSERT_TRUE(rc2 == 0, "stop failed");
    nimcp_autodiff_destroy(ctx);
    PASS();
}

static void test_autodiff_backward_empty_tape(void) {
    TEST("Autodiff: backward with empty tape");
    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_TRUE(ctx != NULL, "ctx NULL");

    uint32_t dims[] = {1};
    nimcp_tensor_t* output = nimcp_tensor_ones(dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* input = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_TRUE(output != NULL && input != NULL, "tensor alloc failed");

    nimcp_tensor_t* gradients[1] = {NULL};
    nimcp_tensor_t* const inputs[1] = {input};
    int rc = nimcp_autodiff_backward(ctx, output, inputs, 1, gradients);
    ASSERT_TRUE(rc == 0, "backward on empty tape should succeed");
    ASSERT_TRUE(gradients[0] != NULL, "gradient should be allocated");

    if (gradients[0]) nimcp_tensor_destroy(gradients[0]);
    nimcp_tensor_destroy(output);
    nimcp_tensor_destroy(input);
    nimcp_autodiff_destroy(ctx);
    PASS();
}

static void test_autodiff_backward_zero_inputs(void) {
    TEST("Autodiff: backward with zero inputs fails");
    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_TRUE(ctx != NULL, "ctx NULL");

    uint32_t dims[] = {1};
    nimcp_tensor_t* output = nimcp_tensor_ones(dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* gradients[1] = {NULL};

    int rc = nimcp_autodiff_backward(ctx, output, (nimcp_tensor_t* const*)gradients, 0, gradients);
    ASSERT_TRUE(rc != 0, "zero inputs should fail");

    nimcp_tensor_destroy(output);
    nimcp_autodiff_destroy(ctx);
    PASS();
}

/* =========================================================================
 * Brain Parallel Stages Context Tests
 * ========================================================================= */

static void test_pre_forward_ctx_null(void) {
    TEST("Brain Parallel: pre_forward NULL safety");
    bool ok = brain_decide_parallel_pre_forward(NULL, NULL, 0, NULL, NULL);
    ASSERT_TRUE(!ok, "NULL should return false");
    PASS();
}

static void test_post_forward_ctx_null(void) {
    TEST("Brain Parallel: post_forward NULL safety");
    bool ok = brain_decide_submit_post_forward(NULL, NULL, NULL, 0, NULL, NULL);
    ASSERT_TRUE(!ok, "NULL should return false");
    PASS();
}

static void test_post_forward_context_struct(void) {
    TEST("Brain Parallel: post_forward_context_t has all done fields");
    post_forward_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT_TRUE(ctx.engram_consol_done == false, "initial should be false");
    ASSERT_TRUE(ctx.systems_consol_done == false, "initial should be false");
    ASSERT_TRUE(ctx.wm_transfer_done == false, "initial should be false");
    ASSERT_TRUE(ctx.semantic_done == false, "initial should be false");
    ASSERT_TRUE(ctx.glial_done == false, "initial should be false");
    ASSERT_TRUE(ctx.tom_done == false, "initial should be false");
    ASSERT_TRUE(ctx.shannon_done == false, "initial should be false");
    ASSERT_TRUE(ctx.quantum_shannon_done == false, "initial should be false");
    PASS();
}

static void test_pre_forward_context_struct(void) {
    TEST("Brain Parallel: pre_forward_context_t has all done fields");
    pre_forward_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT_TRUE(ctx.wellbeing_done == false, "initial should be false");
    ASSERT_TRUE(ctx.engram_done == false, "initial should be false");
    ASSERT_TRUE(ctx.sleep_done == false, "initial should be false");
    ASSERT_TRUE(ctx.curiosity_done == false, "initial should be false");
    ASSERT_TRUE(ctx.prediction_done == false, "initial should be false");
    PASS();
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void) {
    printf("\n=== Phase 5 Batch 8: ODE Solvers, Parallel Stages, Autodiff ===\n\n");

    printf("--- Kernel Backend ODE Solvers ---\n");
    test_kernel_backend_init_cpu();
    test_kernel_backend_get();
    test_euler_step_basic();
    test_euler_step_null();
    test_euler_step_size_mismatch();
    test_heun_step_exists();
    test_heun_step_null();
    test_rk4_step_exists();
    test_rk4_step_null();
    test_dopri5_step_exists();
    test_dopri5_step_null();
    test_compute_derivative_exists();
    test_compute_derivative_null();
    test_update_tau_exists();
    test_update_tau_null();

    printf("\n--- Autodiff Backward ---\n");
    test_autodiff_backward_null();
    test_autodiff_create_destroy();
    test_autodiff_start_stop();
    test_autodiff_backward_empty_tape();
    test_autodiff_backward_zero_inputs();

    printf("\n--- Brain Parallel Stages ---\n");
    test_pre_forward_ctx_null();
    test_post_forward_ctx_null();
    test_post_forward_context_struct();
    test_pre_forward_context_struct();

    printf("\n=== Results: %d passed, %d failed ===\n\n", tests_passed, tests_failed);

    nimcp_kernel_backend_shutdown();
    return tests_failed > 0 ? 1 : 0;
}
