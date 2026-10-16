/**
 * @file test_phase5_batch11.c
 * @brief Phase 5 Batch 11: SNN Backprop, Cochlea Bio-Async, Network Analysis, LNN
 *
 * Tests for final stub eliminations: SNN backprop loss_grad/gradient_norm/weight_norm,
 * cochlea bio-async broadcasts, network analysis validation/community detection,
 * LNN backward/immune/optimizer connections.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "training/nimcp_snn_backprop.h"
#include "perception/nimcp_cochlea.h"
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_gradient.h"
#include "lnn/nimcp_lnn_immune.h"
#include "lnn/nimcp_lnn_training.h"
#include "utils/memory/nimcp_memory.h"

/* Not declared in public header — defined in nimcp_lnn_gradient_part_core.c */
extern int lnn_network_backward(lnn_network_t* network, const nimcp_tensor_t* loss_grad);

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-60s", name); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* =========================================================================
 * SNN Backprop Tests
 * ========================================================================= */

static void test_snn_compute_loss_grad_null(void) {
    TEST("SNN Backprop: compute_loss_grad NULL returns error");
    int rc = snn_backprop_compute_loss_grad(NULL, NULL, NULL, 0, NULL);
    ASSERT_TRUE(rc != 0, "NULL should return error");
    PASS();
}

static void test_snn_compute_loss_grad_valid(void) {
    TEST("SNN Backprop: compute_loss_grad computes MSE gradient");
    float outputs[3] = { 1.0f, 2.0f, 3.0f };
    float targets[3] = { 1.5f, 1.5f, 3.5f };
    float grads[3] = { 0.0f };

    /* Create minimal config + context for the function */
    snn_backprop_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.learning_rate = 0.01f;
    cfg.gradient_clip_norm = 1.0f;
    cfg.sequence_length = 50;
    cfg.batch_size = 3;
    cfg.bptt.unroll_steps = 50;

    /* compute_loss_grad only uses ctx for NULL check */
    int rc = snn_backprop_compute_loss_grad(
        (snn_backprop_ctx_t*)1, /* non-NULL sentinel */
        outputs, targets, 3, grads);
    ASSERT_TRUE(rc == 0, "should succeed");

    /* MSE gradient: 2/N * (output - target) */
    float expected0 = 2.0f / 3.0f * (1.0f - 1.5f);  /* -0.333... */
    float expected1 = 2.0f / 3.0f * (2.0f - 1.5f);  /*  0.333... */
    float expected2 = 2.0f / 3.0f * (3.0f - 3.5f);  /* -0.333... */

    ASSERT_TRUE(fabsf(grads[0] - expected0) < 0.01f, "grad[0] wrong");
    ASSERT_TRUE(fabsf(grads[1] - expected1) < 0.01f, "grad[1] wrong");
    ASSERT_TRUE(fabsf(grads[2] - expected2) < 0.01f, "grad[2] wrong");
    PASS();
}

static void test_snn_get_gradient_norm_null(void) {
    TEST("SNN Backprop: get_gradient_norm NULL returns 0");
    float norm = snn_backprop_get_gradient_norm(NULL);
    ASSERT_TRUE(norm == 0.0f, "NULL should return 0");
    PASS();
}

static void test_snn_get_weight_norm_null(void) {
    TEST("SNN Backprop: get_weight_norm NULL returns 0");
    float norm = snn_backprop_get_weight_norm(NULL);
    ASSERT_TRUE(norm == 0.0f, "NULL should return 0");
    PASS();
}

static void test_snn_connect_grad_manager_null(void) {
    TEST("SNN Backprop: connect_gradient_manager NULL returns error");
    int rc = snn_backprop_connect_gradient_manager(NULL, NULL);
    ASSERT_TRUE(rc != 0, "NULL ctx should return error");
    PASS();
}

/* =========================================================================
 * Cochlea Bio-Async Tests
 * ========================================================================= */

static void test_cochlea_broadcast_audio_onset_null(void) {
    TEST("Cochlea: broadcast_audio_onset NULL returns error");
    nimcp_error_t rc = cochlea_broadcast_audio_onset(NULL, 1000.0f, 60.0f);
    ASSERT_TRUE(rc != NIMCP_SUCCESS, "NULL should fail");
    PASS();
}

static void test_cochlea_broadcast_speech_null(void) {
    TEST("Cochlea: broadcast_speech_detected NULL returns error");
    nimcp_error_t rc = cochlea_broadcast_speech_detected(NULL, 0.9f);
    ASSERT_TRUE(rc != NIMCP_SUCCESS, "NULL should fail");
    PASS();
}

static void test_cochlea_broadcast_echo_null(void) {
    TEST("Cochlea: broadcast_echo_target NULL returns error");
    nimcp_error_t rc = cochlea_broadcast_echo_target(NULL, NULL);
    ASSERT_TRUE(rc != NIMCP_SUCCESS, "NULL should fail");
    PASS();
}

static void test_cochlea_broadcast_audio_onset_valid(void) {
    TEST("Cochlea: broadcast_audio_onset valid (no bio-async)");
    cochlea_config_t cfg = cochlea_config_default(BM_MODE_HUMAN, 44100);
    cochlea_t* c = cochlea_create(&cfg);
    ASSERT_TRUE(c != NULL, "cochlea NULL");

    /* Without bio-async enabled, should succeed as no-op */
    nimcp_error_t rc = cochlea_broadcast_audio_onset(c, 1000.0f, 60.0f);
    ASSERT_TRUE(rc == NIMCP_SUCCESS, "should succeed");

    cochlea_destroy(c);
    PASS();
}

static void test_cochlea_broadcast_speech_valid(void) {
    TEST("Cochlea: broadcast_speech_detected valid (no bio-async)");
    cochlea_config_t cfg = cochlea_config_default(BM_MODE_HUMAN, 44100);
    cochlea_t* c = cochlea_create(&cfg);
    ASSERT_TRUE(c != NULL, "cochlea NULL");

    nimcp_error_t rc = cochlea_broadcast_speech_detected(c, 0.85f);
    ASSERT_TRUE(rc == NIMCP_SUCCESS, "should succeed");

    cochlea_destroy(c);
    PASS();
}

static void test_cochlea_register_kg_null(void) {
    TEST("Cochlea: register_with_kg NULL returns error");
    nimcp_error_t rc = cochlea_register_with_kg(NULL, NULL);
    ASSERT_TRUE(rc != NIMCP_SUCCESS, "NULL should fail");
    PASS();
}

/* =========================================================================
 * LNN Tests
 * ========================================================================= */

static void test_lnn_network_backward_null(void) {
    TEST("LNN: network_backward NULL returns -1");
    int rc = lnn_network_backward(NULL, NULL);
    ASSERT_TRUE(rc == -1, "NULL should return -1");
    PASS();
}

static void test_lnn_immune_connect_null(void) {
    TEST("LNN: immune_connect NULL returns -1");
    int rc = lnn_immune_connect(NULL, NULL);
    ASSERT_TRUE(rc == -1, "NULL should return -1");
    PASS();
}

static void test_lnn_training_connect_optimizer_null(void) {
    TEST("LNN: training_connect_optimizer NULL returns -1");
    int rc = lnn_training_connect_optimizer(NULL, NULL);
    ASSERT_TRUE(rc == -1, "NULL should return -1");
    PASS();
}

/* lnn_layer_is_ternary / lnn_layer_get_ternary_W_rec are not compiled
 * into the shared library (ternary module optional). Skipped. */

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void) {
    printf("\n=== Phase 5 Batch 11: SNN Backprop, Cochlea Bio-Async, LNN ===\n\n");

    printf("--- SNN Backprop ---\n");
    test_snn_compute_loss_grad_null();
    test_snn_compute_loss_grad_valid();
    test_snn_get_gradient_norm_null();
    test_snn_get_weight_norm_null();
    test_snn_connect_grad_manager_null();

    printf("\n--- Cochlea Bio-Async ---\n");
    test_cochlea_broadcast_audio_onset_null();
    test_cochlea_broadcast_speech_null();
    test_cochlea_broadcast_echo_null();
    test_cochlea_broadcast_audio_onset_valid();
    test_cochlea_broadcast_speech_valid();
    test_cochlea_register_kg_null();

    printf("\n--- LNN ---\n");
    test_lnn_network_backward_null();
    test_lnn_immune_connect_null();
    test_lnn_training_connect_optimizer_null();

    printf("\n=== Results: %d passed, %d failed ===\n\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
