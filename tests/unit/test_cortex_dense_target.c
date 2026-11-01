/**
 * @file test_cortex_dense_target.c
 * @brief Visual cortex trained toward a dense semantic target (2026-11-01).
 *
 * Before: the cortex was trained with a one-hot over caption strings, sized
 * to the brain output (4096) while the cortex emits 256 values, so labels
 * >= 256 trained it toward silence. Now the trainer supplies the projected
 * text embedding of what is in the picture via a pending target.
 *
 * Signatures (include/training/nimcp_cortex_cnn.h):
 *   cortex_cnn_processor_t* cortex_cnn_create(cortex_cnn_type_t type, uint32_t embedding_dim);
 *   void cortex_cnn_destroy(cortex_cnn_processor_t* proc);
 *   const float* cortex_cnn_forward_visual(cortex_cnn_processor_t* proc, const uint8_t* pixels,
 *                                          uint32_t w, uint32_t h, uint32_t ch);
 *   float cortex_cnn_backward(cortex_cnn_processor_t* proc, const char* label, uint32_t num_outputs);
 *   int  cortex_cnn_set_pending_target(uint32_t modality, const float* target, uint32_t dim);
 *   bool cortex_cnn_take_pending_target(uint32_t modality, float* out, uint32_t dim);
 *   uint64_t cortex_cnn_get_dense_target_steps(const cortex_cnn_processor_t* proc);
 */
#include "training/nimcp_cortex_cnn.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;
#define EXPECT(cond, ...) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); g_failures++; } } while (0)

static float cosine(const float* a, const float* b, uint32_t n) {
    double ab = 0, aa = 0, bb = 0;
    for (uint32_t i = 0; i < n; i++) { ab += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i]; }
    return (aa > 0 && bb > 0) ? (float)(ab / sqrt(aa * bb)) : 0.0f;
}

static void test_pending_slot_semantics(void) {
    float t[8] = {1, 2, 3, 4, 5, 6, 7, 8}, out[10];
    EXPECT(cortex_cnn_set_pending_target(9, t, 8) == -1, "bad modality rejected");
    EXPECT(!cortex_cnn_take_pending_target(1, out, 10), "nothing pending");
    EXPECT(cortex_cnn_set_pending_target(1, t, 8) == 0, "set");
    EXPECT(cortex_cnn_take_pending_target(1, out, 10), "take");
    EXPECT(out[7] == 8.0f && out[8] == 0.0f && out[9] == 0.0f, "copied and zero-padded");
    EXPECT(!cortex_cnn_take_pending_target(1, out, 10), "consumed");
    cortex_cnn_set_pending_target(1, t, 8);
    cortex_cnn_set_pending_target(1, NULL, 0);
    EXPECT(!cortex_cnn_take_pending_target(1, out, 10), "cleared");
}

static void test_learns_toward_dense_target(void) {
    cortex_cnn_processor_t* p = cortex_cnn_create(CORTEX_CNN_VISUAL, 0);
    EXPECT(p != NULL, "create visual");
    if (!p) return;
    uint8_t img[32 * 32 * 3];
    srand(7);
    for (int i = 0; i < 32 * 32 * 3; i++) img[i] = (uint8_t)(rand() % 256);
    const uint32_t D = 256;
    float target[256];
    for (uint32_t i = 0; i < D; i++) target[i] = (float)sin(0.37 * i) * 0.2f;

    const float* e = cortex_cnn_forward_visual(p, img, 32, 32, 3);
    EXPECT(e != NULL, "forward");
    if (!e) { cortex_cnn_destroy(p); return; }
    float before = cosine(e, target, D);

    float first_loss = -1, last_loss = -1;
    for (int k = 0; k < 200; k++) {
        e = cortex_cnn_forward_visual(p, img, 32, 32, 3);
        cortex_cnn_set_pending_target(CORTEX_CNN_VISUAL, target, D);
        float l = cortex_cnn_backward(p, "a caption string that must not matter", 4096);
        if (k == 0) first_loss = l;
        last_loss = l;
    }
    e = cortex_cnn_forward_visual(p, img, 32, 32, 3);
    float after = cosine(e, target, D);
    printf("  cosine to target: %.3f -> %.3f | loss %.4f -> %.4f | dense steps %llu\n",
           before, after, first_loss, last_loss,
           (unsigned long long)cortex_cnn_get_dense_target_steps(p));
    EXPECT(last_loss >= 0 && last_loss < first_loss, "loss decreased (%f -> %f)", first_loss, last_loss);
    EXPECT(after > before + 0.1f, "output moved toward the target (%f -> %f)", before, after);
    EXPECT(cortex_cnn_get_dense_target_steps(p) == 200, "dense steps counted");

    /* Without a pending target the one-hot path runs inside the 256 outputs */
    e = cortex_cnn_forward_visual(p, img, 32, 32, 3);
    float l = cortex_cnn_backward(p, "label beyond index 256 still visible", 4096);
    EXPECT(l >= 0, "one-hot fallback ok");
    EXPECT(cortex_cnn_get_dense_target_steps(p) == 200, "fallback not counted as dense");
    cortex_cnn_destroy(p);
}

int main(void) {
    test_pending_slot_semantics();
    test_learns_toward_dense_target();
    if (g_failures) { fprintf(stderr, "test_cortex_dense_target: %d failure(s)\n", g_failures); return 1; }
    printf("test_cortex_dense_target: all passed\n");
    return 0;
}
