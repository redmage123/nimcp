/**
 * @file test_cortex_visual_sizes.c
 * @brief Visual cortex CNN at the acuity-schedule resolutions (2026-11-02).
 *
 * The curriculum grows visual acuity with the stages: 32x32 (stage 0),
 * 64x64 (stages 1-2), 128x128 (stage 3+). The visual CNN ends in global
 * average pooling, so one set of weights must accept every size. This test
 * checks forward + backward at each size on the SAME processor (mixed sizes,
 * as happens across a stage change), that the embedding is finite and
 * size-consistent, and prints the per-step cost of each size.
 *
 * Signatures (include/training/nimcp_cortex_cnn.h):
 *   cortex_cnn_processor_t* cortex_cnn_create(cortex_cnn_type_t type, uint32_t embedding_dim);
 *   void cortex_cnn_destroy(cortex_cnn_processor_t* proc);
 *   const float* cortex_cnn_forward_visual(cortex_cnn_processor_t* proc,
 *                                          const uint8_t* pixels,
 *                                          uint32_t w, uint32_t h, uint32_t ch);
 *   float cortex_cnn_backward(cortex_cnn_processor_t* proc,
 *                             const char* label, uint32_t num_outputs);
 *   const float* cortex_cnn_get_embedding(const cortex_cnn_processor_t* proc, uint32_t* dim);
 */
#include "training/nimcp_cortex_cnn.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int g_failures = 0;
#define EXPECT(cond, ...) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); g_failures++; } } while (0)

#define EMBED_DIM 256u
#define N_TIMED 5

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

/* Smooth image with a size-independent pattern: a bright disc on a gradient. */
static uint8_t* make_image(uint32_t s, uint32_t seed) {
    uint8_t* px = malloc((size_t)s * s * 3);
    if (!px) return NULL;
    for (uint32_t y = 0; y < s; y++) {
        for (uint32_t x = 0; x < s; x++) {
            float fx = (float)x / s, fy = (float)y / s;
            float d = (fx - 0.5f) * (fx - 0.5f) + (fy - 0.5f) * (fy - 0.5f);
            uint8_t disc = d < 0.06f ? 200 : 0;
            px[(y * s + x) * 3 + 0] = (uint8_t)(disc + 50 * fx);
            px[(y * s + x) * 3 + 1] = (uint8_t)(50 * fy + (seed * 37) % 50);
            px[(y * s + x) * 3 + 2] = (uint8_t)(disc / 2 + 30);
        }
    }
    return px;
}

static int embedding_finite(const float* e, uint32_t n, float* norm_out) {
    double s = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        if (!isfinite(e[i])) return 0;
        s += (double)e[i] * e[i];
    }
    *norm_out = (float)sqrt(s);
    return 1;
}

int main(void) {
    cortex_cnn_processor_t* proc = cortex_cnn_create(CORTEX_CNN_VISUAL, EMBED_DIM);
    EXPECT(proc != NULL, "create visual cortex");
    if (!proc) return 1;

    const uint32_t sizes[3] = {32, 64, 128};
    float norms[3] = {0};
    double fwd_ms[3] = {0}, bwd_ms[3] = {0};

    for (int k = 0; k < 3; k++) {
        uint32_t s = sizes[k];
        uint8_t* img = make_image(s, 1);
        EXPECT(img != NULL, "alloc %u", s);
        if (!img) continue;

        const float* e = cortex_cnn_forward_visual(proc, img, s, s, 3);
        EXPECT(e != NULL, "forward at %ux%u", s, s);
        uint32_t dim = 0;
        const float* e2 = cortex_cnn_get_embedding(proc, &dim);
        EXPECT(e2 != NULL && dim == EMBED_DIM, "embedding dim at %u: %u", s, dim);
        if (e) EXPECT(embedding_finite(e, EMBED_DIM, &norms[k]), "finite embedding at %u", s);

        float loss = cortex_cnn_backward(proc, "disc", 8);
        EXPECT(loss >= 0.0f && isfinite(loss), "backward at %u: loss %f", s, loss);

        /* Timed steps (forward + backward) */
        double t0 = now_ms();
        for (int i = 0; i < N_TIMED; i++) cortex_cnn_forward_visual(proc, img, s, s, 3);
        double t1 = now_ms();
        for (int i = 0; i < N_TIMED; i++) {
            cortex_cnn_forward_visual(proc, img, s, s, 3);
            cortex_cnn_backward(proc, "disc", 8);
        }
        double t2 = now_ms();
        fwd_ms[k] = (t1 - t0) / N_TIMED;
        bwd_ms[k] = (t2 - t1) / N_TIMED - fwd_ms[k];
        free(img);
    }

    /* Back to the smallest size after the largest: the stage-0 path must
     * still work on a processor that has seen bigger frames. */
    uint8_t* small = make_image(32, 2);
    const float* e = small ? cortex_cnn_forward_visual(proc, small, 32, 32, 3) : NULL;
    float n_back = 0.0f;
    EXPECT(e != NULL && embedding_finite(e, EMBED_DIM, &n_back), "32x32 again after 128x128");
    free(small);

    /* Same scene at different sizes should give embeddings of similar scale
     * (global average pooling normalises for area). */
    if (norms[0] > 0.0f)
        EXPECT(norms[2] / norms[0] < 10.0f && norms[2] / norms[0] > 0.1f,
               "embedding scale comparable across sizes: %f vs %f", norms[0], norms[2]);

    /* Non-square and degenerate inputs */
    uint8_t* rect = malloc(48 * 64 * 3);
    if (rect) {
        for (int i = 0; i < 48 * 64 * 3; i++) rect[i] = (uint8_t)(i * 7);
        EXPECT(cortex_cnn_forward_visual(proc, rect, 64, 48, 3) != NULL, "non-square 64x48");
        free(rect);
    }
    uint8_t one = 0;
    EXPECT(cortex_cnn_forward_visual(proc, &one, 0, 32, 3) == NULL, "zero width rejected");

    for (int k = 0; k < 3; k++)
        printf("  visual %3ux%-3u forward %7.2f ms  backward %7.2f ms  (x%.1f of 32x32)\n",
               sizes[k], sizes[k], fwd_ms[k], bwd_ms[k],
               (fwd_ms[0] + bwd_ms[0]) > 0 ? (fwd_ms[k] + bwd_ms[k]) / (fwd_ms[0] + bwd_ms[0]) : 0.0);

    cortex_cnn_destroy(proc);
    if (g_failures) { fprintf(stderr, "test_cortex_visual_sizes: %d failure(s)\n", g_failures); return 1; }
    printf("test_cortex_visual_sizes: all passed\n");
    return 0;
}
