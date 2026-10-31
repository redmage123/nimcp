/**
 * @file test_cortex_fusion_scale.c
 * @brief cortex_cnn_fusion_scale / fusion_alpha tuning (2026-10-30 fix).
 *
 * The fused cortex embedding used to be blended into the ANN input at a
 * fixed 0.3; the input is spike-encoded against a ~0.1 threshold, so vision
 * never reached the output (8 photos -> identical responses). The blend is
 * now alpha * rms(features) / rms(fused).
 *
 * Signatures (include/training/nimcp_cortex_cnn.h):
 *   void  cortex_cnn_tune_set_fusion_alpha(float v);
 *   float cortex_cnn_tune_get_fusion_alpha(void);
 *   float cortex_cnn_fusion_scale(const float* features, uint32_t n_features,
 *                                 const float* fused, uint32_t n_fused);
 */
#include "training/nimcp_cortex_cnn.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

static int g_failures = 0;
#define EXPECT(cond, ...) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); g_failures++; } } while (0)

int main(void) {
    float feat[16], fused[8];
    for (int i = 0; i < 16; i++) feat[i] = 2.0f;     /* rms 2 */
    for (int i = 0; i < 8; i++)  fused[i] = 0.01f;   /* rms 0.01 */

    EXPECT(fabsf(cortex_cnn_tune_get_fusion_alpha() - 0.5f) < 1e-6f, "default alpha %f",
           cortex_cnn_tune_get_fusion_alpha());

    float s = cortex_cnn_fusion_scale(feat, 16, fused, 8);
    /* 0.5 * 2 / 0.01 = 100 -> the blended contribution has rms 0.5*rms(feat) */
    EXPECT(fabsf(s - 100.0f) < 1e-2f, "scale %f", s);
    EXPECT(fabsf(s * 0.01f - 0.5f * 2.0f) < 1e-3f, "contribution rms %f", s * 0.01f);

    cortex_cnn_tune_set_fusion_alpha(1.0f);
    EXPECT(fabsf(cortex_cnn_fusion_scale(feat, 16, fused, 8) - 200.0f) < 1e-2f, "alpha 1");
    cortex_cnn_tune_set_fusion_alpha(9.0f);   /* out of range: ignored */
    EXPECT(fabsf(cortex_cnn_tune_get_fusion_alpha() - 1.0f) < 1e-6f, "range guard");
    cortex_cnn_tune_set_fusion_alpha(0.0f);
    EXPECT(cortex_cnn_fusion_scale(feat, 16, fused, 8) == 0.0f, "alpha 0 disables");
    cortex_cnn_tune_set_fusion_alpha(0.5f);

    float zero[8] = {0};
    EXPECT(cortex_cnn_fusion_scale(feat, 16, zero, 8) == 0.0f, "zero fused -> 0");
    EXPECT(cortex_cnn_fusion_scale(NULL, 16, fused, 8) == 0.0f, "NULL features -> 0");

    float tiny[8];
    for (int i = 0; i < 8; i++) tiny[i] = 1e-5f;
    EXPECT(cortex_cnn_fusion_scale(feat, 16, tiny, 8) <= 1000.0f, "scale capped");

    if (g_failures) { fprintf(stderr, "test_cortex_fusion_scale: %d failure(s)\n", g_failures); return 1; }
    printf("test_cortex_fusion_scale: all passed\n");
    return 0;
}
