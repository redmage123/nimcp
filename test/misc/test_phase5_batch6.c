/**
 * @file test_phase5_batch6.c
 * @brief Phase 5 Batch 6: Tensor Layer Norm, Parallel Stages, White Matter Bridges
 *
 * Tests for: nimcp_tensor_layer_norm, brain parallel stage tasks,
 * white matter GPU/bio-async bridges.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "utils/tensor/nimcp_tensor.h"
#include "core/brain/white_matter/nimcp_white_matter_tracts.h"
#include "utils/memory/nimcp_memory.h"

/* Forward declarations for bridge functions (no header exists) */
extern int wmt_bridge_gpu_upload_state(const wmt_system_t* wmt, void* gpu_ctx);
extern int wmt_bridge_gpu_sync_state(wmt_system_t* wmt, void* gpu_ctx);
extern int wmt_bridge_bio_async_publish_state(const wmt_system_t* wmt, void* bio_async_ctx,
                                               white_matter_tract_t tract);
extern int wmt_bridge_bio_async_handle_message(wmt_system_t* wmt, void* bio_async_ctx,
                                                uint32_t message_type, const void* payload,
                                                uint32_t payload_size);

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-60s", name); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* =========================================================================
 * Tensor Layer Norm Tests
 * ========================================================================= */

static void test_layer_norm_null(void) {
    TEST("LayerNorm: NULL input returns NULL");
    nimcp_tensor_t* result = nimcp_tensor_layer_norm(NULL, NULL, NULL, 1e-5);
    ASSERT_TRUE(result == NULL, "should return NULL for NULL input");
    PASS();
}

static void test_layer_norm_basic(void) {
    TEST("LayerNorm: basic normalization");
    uint32_t dims[] = {4};
    nimcp_tensor_t* t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_TRUE(t != NULL, "tensor NULL");

    float* data = (float*)nimcp_tensor_data(t);
    data[0] = 1.0f; data[1] = 2.0f; data[2] = 3.0f; data[3] = 4.0f;

    nimcp_tensor_t* result = nimcp_tensor_layer_norm(t, NULL, NULL, 1e-5);
    ASSERT_TRUE(result != NULL, "result NULL");

    float* out = (float*)nimcp_tensor_data(result);
    /* After layer norm without gamma/beta: mean≈0, variance≈1 */
    float sum = 0.0f;
    for (int i = 0; i < 4; i++) sum += out[i];
    ASSERT_TRUE(fabsf(sum) < 0.01f, "mean should be ~0");

    float var_sum = 0.0f;
    float mean = sum / 4.0f;
    for (int i = 0; i < 4; i++) {
        float d = out[i] - mean;
        var_sum += d * d;
    }
    float var = var_sum / 4.0f;
    ASSERT_TRUE(fabsf(var - 1.0f) < 0.01f, "variance should be ~1");

    nimcp_tensor_destroy(result);
    nimcp_tensor_destroy(t);
    PASS();
}

static void test_layer_norm_2d(void) {
    TEST("LayerNorm: 2D tensor (batch normalization)");
    uint32_t dims[] = {2, 4};
    nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_TRUE(t != NULL, "tensor NULL");

    float* data = (float*)nimcp_tensor_data(t);
    data[0] = 1.0f; data[1] = 2.0f; data[2] = 3.0f; data[3] = 4.0f;
    data[4] = 10.0f; data[5] = 20.0f; data[6] = 30.0f; data[7] = 40.0f;

    nimcp_tensor_t* result = nimcp_tensor_layer_norm(t, NULL, NULL, 1e-5);
    ASSERT_TRUE(result != NULL, "result NULL");

    float* out = (float*)nimcp_tensor_data(result);

    /* Check row 0 mean ≈ 0 */
    float row0_sum = out[0] + out[1] + out[2] + out[3];
    ASSERT_TRUE(fabsf(row0_sum) < 0.01f, "row 0 mean should be ~0");

    /* Check row 1 mean ≈ 0 */
    float row1_sum = out[4] + out[5] + out[6] + out[7];
    ASSERT_TRUE(fabsf(row1_sum) < 0.01f, "row 1 mean should be ~0");

    nimcp_tensor_destroy(result);
    nimcp_tensor_destroy(t);
    PASS();
}

static void test_layer_norm_with_gamma_beta(void) {
    TEST("LayerNorm: with affine transform (gamma, beta)");
    uint32_t dims[] = {4};
    nimcp_tensor_t* t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* gamma = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* beta = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_TRUE(t && gamma && beta, "tensor alloc failed");

    float* data = (float*)nimcp_tensor_data(t);
    data[0] = 1.0f; data[1] = 2.0f; data[2] = 3.0f; data[3] = 4.0f;

    float* g = (float*)nimcp_tensor_data(gamma);
    float* b = (float*)nimcp_tensor_data(beta);
    for (int i = 0; i < 4; i++) { g[i] = 2.0f; b[i] = 1.0f; }

    nimcp_tensor_t* result = nimcp_tensor_layer_norm(t, gamma, beta, 1e-5);
    ASSERT_TRUE(result != NULL, "result NULL");

    float* out = (float*)nimcp_tensor_data(result);
    float sum = 0.0f;
    for (int i = 0; i < 4; i++) sum += out[i];
    float mean = sum / 4.0f;
    ASSERT_TRUE(fabsf(mean - 1.0f) < 0.1f, "mean should be ~1.0 (beta)");

    nimcp_tensor_destroy(result);
    nimcp_tensor_destroy(beta);
    nimcp_tensor_destroy(gamma);
    nimcp_tensor_destroy(t);
    PASS();
}

static void test_layer_norm_constant_input(void) {
    TEST("LayerNorm: constant input → all near zero");
    uint32_t dims[] = {4};
    nimcp_tensor_t* t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_TRUE(t != NULL, "tensor NULL");

    float* data = (float*)nimcp_tensor_data(t);
    for (int i = 0; i < 4; i++) data[i] = 5.0f;

    nimcp_tensor_t* result = nimcp_tensor_layer_norm(t, NULL, NULL, 1e-5);
    ASSERT_TRUE(result != NULL, "result NULL");

    float* out = (float*)nimcp_tensor_data(result);
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE(fabsf(out[i]) < 0.1f, "constant input should normalize near 0");
    }

    nimcp_tensor_destroy(result);
    nimcp_tensor_destroy(t);
    PASS();
}

static void test_layer_norm_preserves_shape(void) {
    TEST("LayerNorm: output shape matches input");
    uint32_t dims[] = {3, 8};
    nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_TRUE(t != NULL, "tensor NULL");

    nimcp_tensor_t* result = nimcp_tensor_layer_norm(t, NULL, NULL, 1e-5);
    ASSERT_TRUE(result != NULL, "result NULL");

    uint32_t out_rank = nimcp_tensor_rank(result);
    ASSERT_TRUE(out_rank == 2, "rank should be 2");

    nimcp_tensor_destroy(result);
    nimcp_tensor_destroy(t);
    PASS();
}

/* =========================================================================
 * White Matter GPU Bridge Tests
 * ========================================================================= */

static void test_wmt_gpu_upload_null(void) {
    TEST("WMT GPU: upload with NULL wmt returns -1");
    int rc = wmt_bridge_gpu_upload_state(NULL, NULL);
    ASSERT_TRUE(rc == -1, "NULL wmt should return -1");
    PASS();
}

static void test_wmt_gpu_upload_no_gpu(void) {
    TEST("WMT GPU: upload with NULL gpu_ctx returns 0");
    wmt_system_t* wmt = wmt_create(NULL);
    ASSERT_TRUE(wmt != NULL, "wmt NULL");

    int rc = wmt_bridge_gpu_upload_state(wmt, NULL);
    ASSERT_TRUE(rc == 0, "NULL gpu_ctx should return 0 (skip)");

    wmt_destroy(wmt);
    PASS();
}

static void test_wmt_gpu_upload_with_ctx(void) {
    TEST("WMT GPU: upload with dummy gpu_ctx returns 0");
    wmt_system_t* wmt = wmt_create(NULL);
    ASSERT_TRUE(wmt != NULL, "wmt NULL");

    int dummy_gpu = 42;
    int rc = wmt_bridge_gpu_upload_state(wmt, &dummy_gpu);
    ASSERT_TRUE(rc == 0, "should succeed with valid pointers");

    wmt_destroy(wmt);
    PASS();
}

static void test_wmt_gpu_sync_null(void) {
    TEST("WMT GPU: sync with NULL wmt returns -1");
    int rc = wmt_bridge_gpu_sync_state(NULL, NULL);
    ASSERT_TRUE(rc == -1, "NULL wmt should return -1");
    PASS();
}

static void test_wmt_gpu_sync_no_gpu(void) {
    TEST("WMT GPU: sync with NULL gpu_ctx returns 0");
    wmt_system_t* wmt = wmt_create(NULL);
    ASSERT_TRUE(wmt != NULL, "wmt NULL");

    int rc = wmt_bridge_gpu_sync_state(wmt, NULL);
    ASSERT_TRUE(rc == 0, "NULL gpu_ctx should return 0");

    wmt_destroy(wmt);
    PASS();
}

/* =========================================================================
 * White Matter Bio-Async Bridge Tests
 * ========================================================================= */

static void test_wmt_bio_async_publish_null(void) {
    TEST("WMT Bio-Async: publish with NULL wmt returns -1");
    int rc = wmt_bridge_bio_async_publish_state(NULL, NULL, WMT_CORPUS_CALLOSUM);
    ASSERT_TRUE(rc == -1, "NULL wmt should return -1");
    PASS();
}

static void test_wmt_bio_async_publish_no_ctx(void) {
    TEST("WMT Bio-Async: publish with NULL bio_ctx returns 0");
    wmt_system_t* wmt = wmt_create(NULL);
    ASSERT_TRUE(wmt != NULL, "wmt NULL");

    int rc = wmt_bridge_bio_async_publish_state(wmt, NULL, WMT_CORPUS_CALLOSUM);
    ASSERT_TRUE(rc == 0, "NULL bio_ctx should return 0 (skip)");

    wmt_destroy(wmt);
    PASS();
}

static void test_wmt_bio_async_publish_valid(void) {
    TEST("WMT Bio-Async: publish with valid context succeeds");
    wmt_system_t* wmt = wmt_create(NULL);
    ASSERT_TRUE(wmt != NULL, "wmt NULL");

    int dummy_bio = 1;
    int rc = wmt_bridge_bio_async_publish_state(wmt, &dummy_bio, WMT_CORPUS_CALLOSUM);
    ASSERT_TRUE(rc == 0, "valid publish should succeed");

    wmt_destroy(wmt);
    PASS();
}

static void test_wmt_bio_async_handle_null(void) {
    TEST("WMT Bio-Async: handle message with NULL wmt returns -1");
    int rc = wmt_bridge_bio_async_handle_message(NULL, NULL, 0x0001, NULL, 0);
    ASSERT_TRUE(rc == -1, "NULL wmt should return -1");
    PASS();
}

static void test_wmt_bio_async_handle_neuromod(void) {
    TEST("WMT Bio-Async: handle neuromodulator message");
    wmt_system_t* wmt = wmt_create(NULL);
    ASSERT_TRUE(wmt != NULL, "wmt NULL");

    uint8_t payload[8];
    uint32_t tract_id = 0;
    float delta = 0.005f;
    memcpy(payload, &tract_id, sizeof(uint32_t));
    memcpy(payload + sizeof(uint32_t), &delta, sizeof(float));

    int dummy_bio = 1;
    int rc = wmt_bridge_bio_async_handle_message(wmt, &dummy_bio, 0x0001, payload, 8);
    ASSERT_TRUE(rc == 0, "neuromodulator message should succeed");

    wmt_destroy(wmt);
    PASS();
}

static void test_wmt_bio_async_handle_inflammation(void) {
    TEST("WMT Bio-Async: handle inflammation message");
    wmt_system_t* wmt = wmt_create(NULL);
    ASSERT_TRUE(wmt != NULL, "wmt NULL");

    uint8_t payload[8];
    uint32_t tract_id = 1;
    float severity = 0.5f;
    memcpy(payload, &tract_id, sizeof(uint32_t));
    memcpy(payload + sizeof(uint32_t), &severity, sizeof(float));

    int dummy_bio = 1;
    int rc = wmt_bridge_bio_async_handle_message(wmt, &dummy_bio, 0x0002, payload, 8);
    ASSERT_TRUE(rc == 0, "inflammation message should succeed");

    wmt_destroy(wmt);
    PASS();
}

static void test_wmt_bio_async_handle_myelination(void) {
    TEST("WMT Bio-Async: handle myelination boost message");
    wmt_system_t* wmt = wmt_create(NULL);
    ASSERT_TRUE(wmt != NULL, "wmt NULL");

    uint8_t payload[8];
    uint32_t tract_id = 2;
    float amount = 0.003f;
    memcpy(payload, &tract_id, sizeof(uint32_t));
    memcpy(payload + sizeof(uint32_t), &amount, sizeof(float));

    int dummy_bio = 1;
    int rc = wmt_bridge_bio_async_handle_message(wmt, &dummy_bio, 0x0003, payload, 8);
    ASSERT_TRUE(rc == 0, "myelination boost message should succeed");

    wmt_destroy(wmt);
    PASS();
}

static void test_wmt_bio_async_handle_unknown(void) {
    TEST("WMT Bio-Async: unknown message type doesn't crash");
    wmt_system_t* wmt = wmt_create(NULL);
    ASSERT_TRUE(wmt != NULL, "wmt NULL");

    int dummy_bio = 1;
    int rc = wmt_bridge_bio_async_handle_message(wmt, &dummy_bio, 0xFFFF, NULL, 0);
    ASSERT_TRUE(rc == 0, "unknown message type should return 0");

    wmt_destroy(wmt);
    PASS();
}

static void test_wmt_bio_async_null_payload(void) {
    TEST("WMT Bio-Async: NULL payload with nonzero size returns -1");
    wmt_system_t* wmt = wmt_create(NULL);
    ASSERT_TRUE(wmt != NULL, "wmt NULL");

    int dummy_bio = 1;
    int rc = wmt_bridge_bio_async_handle_message(wmt, &dummy_bio, 0x0001, NULL, 8);
    ASSERT_TRUE(rc == -1, "NULL payload with size>0 should return -1");

    wmt_destroy(wmt);
    PASS();
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void) {
    printf("\n=== Phase 5 Batch 6: Layer Norm, Parallel Stages, WMT Bridges ===\n\n");

    printf("--- Tensor Layer Norm ---\n");
    test_layer_norm_null();
    test_layer_norm_basic();
    test_layer_norm_2d();
    test_layer_norm_with_gamma_beta();
    test_layer_norm_constant_input();
    test_layer_norm_preserves_shape();

    printf("\n--- White Matter GPU Bridge ---\n");
    test_wmt_gpu_upload_null();
    test_wmt_gpu_upload_no_gpu();
    test_wmt_gpu_upload_with_ctx();
    test_wmt_gpu_sync_null();
    test_wmt_gpu_sync_no_gpu();

    printf("\n--- White Matter Bio-Async Bridge ---\n");
    test_wmt_bio_async_publish_null();
    test_wmt_bio_async_publish_no_ctx();
    test_wmt_bio_async_publish_valid();
    test_wmt_bio_async_handle_null();
    test_wmt_bio_async_handle_neuromod();
    test_wmt_bio_async_handle_inflammation();
    test_wmt_bio_async_handle_myelination();
    test_wmt_bio_async_handle_unknown();
    test_wmt_bio_async_null_payload();

    printf("\n=== Results: %d passed, %d failed ===\n\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
