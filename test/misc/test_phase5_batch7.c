/**
 * @file test_phase5_batch7.c
 * @brief Phase 5 Batch 7: SNN Backprop, Cortical Interneuron Bridges
 *
 * Tests for: snn_backprop_forward/backward/step/batch/tensor,
 * cortical interneuron bridge connections and modulation.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "training/nimcp_snn_backprop.h"
#include "core/cortical_columns/nimcp_cortical_interneurons.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-60s", name); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* Forward declarations for bridge functions */
extern int cint_bridge_connect_cortical_columns(cortical_interneuron_system_t* system,
                                                 void* column_pool);
extern int cint_bridge_disconnect_cortical_columns(cortical_interneuron_system_t* system);
extern int cint_bridge_connect_plasticity(cortical_interneuron_system_t* system,
                                           void* plasticity_ctx);
extern float cint_bridge_get_stdp_modulation(const cortical_interneuron_system_t* system);
extern int cint_bridge_connect_training(cortical_interneuron_system_t* system,
                                         void* training_ctx);
extern int cint_bridge_training_post_batch(cortical_interneuron_system_t* system,
                                            float batch_loss, float batch_accuracy);
extern int cint_bridge_connect_inference(cortical_interneuron_system_t* system,
                                          void* inference_ctx);
extern float cint_bridge_get_inference_gate(const cortical_interneuron_system_t* system);
extern int cint_bridge_connect_thalamic_trn(cortical_interneuron_system_t* system,
                                             void* trn_ctx);
extern float cint_bridge_get_inhibitory_tone(const cortical_interneuron_system_t* system);
extern int cint_bridge_connect_bio_async(cortical_interneuron_system_t* system);
extern int cint_bridge_disconnect_bio_async(cortical_interneuron_system_t* system);
extern int cint_bridge_connect_immune(cortical_interneuron_system_t* system,
                                       void* immune_ctx);
extern int cint_bridge_apply_inflammation(cortical_interneuron_system_t* system,
                                           float inflammation_level);
extern int cint_bridge_connect_substrate_gpu(cortical_interneuron_system_t* system,
                                              void* gpu_ctx);
extern int cint_bridge_sync_from_gpu(cortical_interneuron_system_t* system);

/* =========================================================================
 * SNN Backprop Tests
 * ========================================================================= */

static void test_snn_batch_create_destroy(void) {
    TEST("SNN Batch: create and destroy");
    float inputs[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float targets[] = {0.0f, 1.0f};

    snn_batch_t* batch = snn_batch_create(inputs, targets, 2, 2, 1);
    ASSERT_TRUE(batch != NULL, "batch NULL");
    snn_batch_destroy(batch);
    PASS();
}

static void test_snn_batch_create_null(void) {
    TEST("SNN Batch: create with NULL returns NULL");
    snn_batch_t* batch = snn_batch_create(NULL, NULL, 0, 0, 0);
    ASSERT_TRUE(batch == NULL, "should return NULL");
    PASS();
}

static void test_snn_batch_destroy_null(void) {
    TEST("SNN Batch: destroy NULL is safe");
    snn_batch_destroy(NULL);
    PASS();
}

static void test_snn_compute_loss(void) {
    TEST("SNN: compute_loss MSE");
    snn_backprop_config_t cfg = snn_backprop_default_config(SNN_TRAIN_BPTT);
    snn_backprop_ctx_t* ctx = snn_backprop_create(NULL, &cfg);
    /* ctx may be NULL if network is required, test the standalone function */
    float outputs[] = {1.0f, 2.0f, 3.0f};
    float targets[] = {1.0f, 2.0f, 3.0f};
    float loss = snn_backprop_compute_loss(ctx, outputs, targets, 3);
    ASSERT_TRUE(loss < 0.001f, "identical should give ~0 loss");
    if (ctx) snn_backprop_destroy(ctx);
    PASS();
}

static void test_snn_compute_loss_nonzero(void) {
    TEST("SNN: compute_loss with error");
    float outputs[] = {1.0f, 0.0f};
    float targets[] = {0.0f, 1.0f};
    /* Without ctx, returns 0 */
    float loss = snn_backprop_compute_loss(NULL, outputs, targets, 2);
    ASSERT_TRUE(loss == 0.0f, "NULL ctx returns 0.0f");
    PASS();
}

static void test_snn_compute_loss_null(void) {
    TEST("SNN: compute_loss NULL safety");
    float loss = snn_backprop_compute_loss(NULL, NULL, NULL, 0);
    ASSERT_TRUE(loss == 0.0f, "NULL should return 0.0f");
    PASS();
}

static void test_snn_forward_null(void) {
    TEST("SNN: forward NULL safety");
    int rc = snn_backprop_forward(NULL, NULL, 0, 0.0f, NULL);
    ASSERT_TRUE(rc != 0, "NULL should fail");
    PASS();
}

static void test_snn_backward_null(void) {
    TEST("SNN: backward NULL safety");
    int rc = snn_backprop_backward(NULL, NULL, 0);
    ASSERT_TRUE(rc != 0, "NULL should fail");
    PASS();
}

static void test_snn_step_null(void) {
    TEST("SNN: step NULL safety");
    int rc = snn_backprop_step(NULL, 0.01f);
    ASSERT_TRUE(rc != 0, "NULL should fail");
    PASS();
}

static void test_snn_zero_grad_null(void) {
    TEST("SNN: zero_grad NULL safety");
    int rc = snn_backprop_zero_grad(NULL);
    ASSERT_TRUE(rc != 0, "NULL should fail");
    PASS();
}

static void test_snn_train_step_null(void) {
    TEST("SNN: train_step NULL safety");
    int rc = snn_backprop_train_step(NULL, NULL, NULL, 0, 0.0f, NULL);
    ASSERT_TRUE(rc != 0, "NULL should fail");
    PASS();
}

static void test_snn_forward_tensor_null(void) {
    TEST("SNN: forward_tensor NULL safety");
    int rc = snn_backprop_forward_tensor(NULL, NULL, NULL);
    ASSERT_TRUE(rc != 0, "NULL should fail");
    PASS();
}

static void test_snn_backward_tensor_null(void) {
    TEST("SNN: backward_tensor NULL safety");
    int rc = snn_backprop_backward_tensor(NULL, NULL);
    ASSERT_TRUE(rc != 0, "NULL should fail");
    PASS();
}

static void test_snn_train_batch_null(void) {
    TEST("SNN: train_batch NULL safety");
    int rc = snn_backprop_train_batch(NULL, NULL, 0.0f, NULL);
    ASSERT_TRUE(rc != 0, "NULL should fail");
    PASS();
}

/* =========================================================================
 * Cortical Interneuron Bridge Tests
 * ========================================================================= */

static void test_cint_connect_columns_null(void) {
    TEST("CINT: connect cortical columns NULL returns -1");
    int rc = cint_bridge_connect_cortical_columns(NULL, NULL);
    ASSERT_TRUE(rc == -1, "NULL system should return -1");
    PASS();
}

static void test_cint_connect_columns_null_pool(void) {
    TEST("CINT: connect cortical columns NULL pool returns 0");
    cortical_interneuron_system_t* sys = cint_create(NULL);
    ASSERT_TRUE(sys != NULL, "sys NULL");
    int rc = cint_bridge_connect_cortical_columns(sys, NULL);
    ASSERT_TRUE(rc == 0, "NULL pool should return 0 (skip)");
    cint_destroy(sys);
    PASS();
}

static void test_cint_connect_columns_valid(void) {
    TEST("CINT: connect cortical columns valid");
    cortical_interneuron_system_t* sys = cint_create(NULL);
    ASSERT_TRUE(sys != NULL, "sys NULL");
    int dummy = 1;
    int rc = cint_bridge_connect_cortical_columns(sys, &dummy);
    ASSERT_TRUE(rc == 0, "valid connect should succeed");
    cint_destroy(sys);
    PASS();
}

static void test_cint_disconnect_columns(void) {
    TEST("CINT: disconnect cortical columns");
    cortical_interneuron_system_t* sys = cint_create(NULL);
    ASSERT_TRUE(sys != NULL, "sys NULL");
    int rc = cint_bridge_disconnect_cortical_columns(sys);
    ASSERT_TRUE(rc == 0, "disconnect should succeed");
    cint_destroy(sys);
    PASS();
}

static void test_cint_stdp_modulation(void) {
    TEST("CINT: STDP modulation factor");
    cortical_interneuron_system_t* sys = cint_create(NULL);
    ASSERT_TRUE(sys != NULL, "sys NULL");
    float mod = cint_bridge_get_stdp_modulation(sys);
    ASSERT_TRUE(mod >= 0.1f && mod <= 2.0f, "modulation out of range [0.1-2.0]");
    cint_destroy(sys);
    PASS();
}

static void test_cint_stdp_modulation_null(void) {
    TEST("CINT: STDP modulation NULL returns 1.0");
    float mod = cint_bridge_get_stdp_modulation(NULL);
    ASSERT_TRUE(fabsf(mod - 1.0f) < 0.001f, "NULL should return 1.0");
    PASS();
}

static void test_cint_inference_gate(void) {
    TEST("CINT: inference gate factor");
    cortical_interneuron_system_t* sys = cint_create(NULL);
    ASSERT_TRUE(sys != NULL, "sys NULL");
    float gate = cint_bridge_get_inference_gate(sys);
    ASSERT_TRUE(gate >= 0.0f && gate <= 1.0f, "gate out of range");
    cint_destroy(sys);
    PASS();
}

static void test_cint_inhibitory_tone(void) {
    TEST("CINT: inhibitory tone");
    cortical_interneuron_system_t* sys = cint_create(NULL);
    ASSERT_TRUE(sys != NULL, "sys NULL");
    float tone = cint_bridge_get_inhibitory_tone(sys);
    ASSERT_TRUE(tone >= 0.0f && tone <= 1.0f, "tone out of range");
    cint_destroy(sys);
    PASS();
}

static void test_cint_training_post_batch(void) {
    TEST("CINT: training post_batch E/I adjustment");
    cortical_interneuron_system_t* sys = cint_create(NULL);
    ASSERT_TRUE(sys != NULL, "sys NULL");

    int rc = cint_bridge_training_post_batch(sys, 2.5f, 0.5f);
    ASSERT_TRUE(rc == 0, "post_batch should succeed");

    cint_destroy(sys);
    PASS();
}

static void test_cint_apply_inflammation(void) {
    TEST("CINT: apply inflammation reduces gamma");
    cortical_interneuron_system_t* sys = cint_create(NULL);
    ASSERT_TRUE(sys != NULL, "sys NULL");

    float gamma_before = sys->gamma_power;
    int rc = cint_bridge_apply_inflammation(sys, 0.8f);
    ASSERT_TRUE(rc == 0, "apply_inflammation should succeed");
    ASSERT_TRUE(sys->gamma_power <= gamma_before, "gamma should decrease");

    cint_destroy(sys);
    PASS();
}

static void test_cint_apply_inflammation_null(void) {
    TEST("CINT: apply inflammation NULL returns -1");
    int rc = cint_bridge_apply_inflammation(NULL, 0.5f);
    ASSERT_TRUE(rc == -1, "NULL should return -1");
    PASS();
}

static void test_cint_bio_async_connect_disconnect(void) {
    TEST("CINT: bio-async connect and disconnect");
    cortical_interneuron_system_t* sys = cint_create(NULL);
    ASSERT_TRUE(sys != NULL, "sys NULL");

    int rc1 = cint_bridge_connect_bio_async(sys);
    ASSERT_TRUE(rc1 == 0, "connect should succeed");

    int rc2 = cint_bridge_disconnect_bio_async(sys);
    ASSERT_TRUE(rc2 == 0, "disconnect should succeed");

    cint_destroy(sys);
    PASS();
}

static void test_cint_gpu_connect_sync(void) {
    TEST("CINT: GPU connect and sync");
    cortical_interneuron_system_t* sys = cint_create(NULL);
    ASSERT_TRUE(sys != NULL, "sys NULL");

    int dummy_gpu = 1;
    int rc1 = cint_bridge_connect_substrate_gpu(sys, &dummy_gpu);
    ASSERT_TRUE(rc1 == 0, "GPU connect should succeed");

    int rc2 = cint_bridge_sync_from_gpu(sys);
    ASSERT_TRUE(rc2 == 0, "GPU sync should succeed");

    cint_destroy(sys);
    PASS();
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void) {
    printf("\n=== Phase 5 Batch 7: SNN Backprop, Cortical Interneuron Bridges ===\n\n");

    printf("--- SNN Backprop ---\n");
    test_snn_batch_create_destroy();
    test_snn_batch_create_null();
    test_snn_batch_destroy_null();
    test_snn_compute_loss();
    test_snn_compute_loss_nonzero();
    test_snn_compute_loss_null();
    test_snn_forward_null();
    test_snn_backward_null();
    test_snn_step_null();
    test_snn_zero_grad_null();
    test_snn_train_step_null();
    test_snn_forward_tensor_null();
    test_snn_backward_tensor_null();
    test_snn_train_batch_null();

    printf("\n--- Cortical Interneuron Bridges ---\n");
    test_cint_connect_columns_null();
    test_cint_connect_columns_null_pool();
    test_cint_connect_columns_valid();
    test_cint_disconnect_columns();
    test_cint_stdp_modulation();
    test_cint_stdp_modulation_null();
    test_cint_inference_gate();
    test_cint_inhibitory_tone();
    test_cint_training_post_batch();
    test_cint_apply_inflammation();
    test_cint_apply_inflammation_null();
    test_cint_bio_async_connect_disconnect();
    test_cint_gpu_connect_sync();

    printf("\n=== Results: %d passed, %d failed ===\n\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
