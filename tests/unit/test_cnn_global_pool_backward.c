/**
 * @file test_cnn_global_pool_backward.c
 * @brief Global pooling backward must reach every input channel (2026-11-06).
 *
 * Regression: the global average/max pool backward fell into a pass-through
 * memcpy that wrote the C per-channel gradients into channel 0's spatial
 * slots, so channels 1..C-1 of the layer below never got gradient. The visual,
 * audio and speech cortex CNNs all end in global average pooling, which froze
 * their conv backbones once channel 0 went silent.
 *
 * Network: Conv(3->8, 3x3, pad 1) -> GlobalPool -> Flatten -> Dense(8->2).
 * Asserts every conv output channel's weight-gradient row is non-zero.
 *
 * Signatures (include/training/nimcp_cnn_training.h):
 *   nimcp_error_t cnn_trainer_default_config(cnn_trainer_config_t* config);
 *   cnn_trainer_t* cnn_trainer_create(const cnn_trainer_config_t* config);
 *   void cnn_trainer_destroy(cnn_trainer_t* trainer);
 *   cnn_layer_t* cnn_trainer_add_conv_layer(cnn_trainer_t*, const cnn_conv_config_t*);
 *   cnn_layer_t* cnn_trainer_add_pool_layer(cnn_trainer_t*, const cnn_pool_config_t*);
 *   cnn_layer_t* cnn_trainer_add_flatten_layer(cnn_trainer_t*);
 *   cnn_layer_t* cnn_trainer_add_dense_layer(cnn_trainer_t*, const cnn_dense_config_t*);
 *   nimcp_error_t cnn_trainer_forward(cnn_trainer_t*, const nimcp_tensor_t*, cnn_forward_result_t*);
 *   nimcp_error_t cnn_trainer_backward(cnn_trainer_t*, const nimcp_tensor_t*, const cnn_forward_result_t*);
 *   nimcp_error_t cnn_get_layer_weight_grad(const cnn_trainer_t*, uint32_t, float**, size_t*);
 */
#include "training/nimcp_cnn_training.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/tensor/nimcp_tensor.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int g_failures = 0;
#define EXPECT(cond, ...) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); g_failures++; } } while (0)

#define IN_CH 3u
#define OUT_CH 8u
#define H 6u
#define W 6u

static void run(cnn_pooling_type_t pool_type, const char* name) {
    cnn_trainer_config_t cfg;
    cnn_trainer_default_config(&cfg);
    cfg.enable_bio_async = false;
    cfg.verbose = false;
    cnn_trainer_t* t = cnn_trainer_create(&cfg);
    EXPECT(t != NULL, "%s: create trainer", name);
    if (!t) return;

    cnn_conv_config_t conv = {0};
    conv.kernel_h = 3; conv.kernel_w = 3;
    conv.stride_h = 1; conv.stride_w = 1;
    conv.padding_h = 1; conv.padding_w = 1;
    conv.dilation_h = 1; conv.dilation_w = 1;
    conv.in_channels = IN_CH; conv.out_channels = OUT_CH;
    conv.groups = 1;
    conv.activation = CNN_ACTIVATION_NONE;
    conv.use_bias = true;
    conv.weight_init_std = 0.1f;
    EXPECT(cnn_trainer_add_conv_layer(t, &conv) != NULL, "%s: add conv", name);

    cnn_pool_config_t pool = {0};
    pool.type = pool_type;
    pool.stride_h = 1; pool.stride_w = 1;
    EXPECT(cnn_trainer_add_pool_layer(t, &pool) != NULL, "%s: add pool", name);
    EXPECT(cnn_trainer_add_flatten_layer(t) != NULL, "%s: add flatten", name);

    cnn_dense_config_t dense = {0};
    dense.in_features = OUT_CH; dense.out_features = 2;
    dense.activation = CNN_ACTIVATION_NONE;
    dense.use_bias = true;
    dense.weight_init_std = 0.5f;
    EXPECT(cnn_trainer_add_dense_layer(t, &dense) != NULL, "%s: add dense", name);

    uint32_t in_dims[4] = {1, IN_CH, H, W};
    nimcp_tensor_t* in = nimcp_tensor_create(in_dims, 4, NIMCP_DTYPE_F32);
    float* x = nimcp_tensor_data(in);
    for (uint32_t i = 0; i < IN_CH * H * W; i++) x[i] = 0.1f + 0.37f * (float)((i * 7919u) % 23u) / 23.0f;

    uint32_t t_dims[2] = {1, 2};
    nimcp_tensor_t* tg = nimcp_tensor_create(t_dims, 2, NIMCP_DTYPE_F32);
    float* y = nimcp_tensor_data(tg);
    y[0] = 1.0f; y[1] = 0.0f;

    cnn_forward_result_t fr;
    memset(&fr, 0, sizeof(fr));
    EXPECT(cnn_trainer_forward(t, in, &fr) == NIMCP_SUCCESS, "%s: forward", name);
    EXPECT(cnn_trainer_backward(t, tg, &fr) == NIMCP_SUCCESS, "%s: backward", name);

    float* g = NULL;
    size_t n = 0;
    EXPECT(cnn_get_layer_weight_grad(t, 0, &g, &n) == NIMCP_SUCCESS && g, "%s: get conv grad", name);
    EXPECT(n == (size_t)OUT_CH * IN_CH * 9, "%s: conv grad size %zu", name, n);
    if (g && n == (size_t)OUT_CH * IN_CH * 9) {
        size_t per = n / OUT_CH;
        uint32_t live = 0;
        for (uint32_t c = 0; c < OUT_CH; c++) {
            float s = 0.0f;
            for (size_t k = 0; k < per; k++) s += fabsf(g[c * per + k]);
            EXPECT(s > 1e-7f, "%s: conv output channel %u got no gradient", name, c);
            live += s > 1e-7f;
        }
        printf("%s: %u/%u conv channels receive gradient\n", name, live, OUT_CH);
    }
    if (g) nimcp_free(g);

    if (fr.activations) {
        for (uint32_t i = 0; i < fr.num_layers; i++)
            if (fr.activations[i]) nimcp_tensor_destroy(fr.activations[i]);
        nimcp_free(fr.activations);
    }
    if (fr.output) nimcp_tensor_destroy(fr.output);
    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(tg);
    cnn_trainer_destroy(t);
}

int main(void) {
    run(CNN_POOL_GLOBAL_AVERAGE, "global_avg");
    run(CNN_POOL_GLOBAL_MAX, "global_max");
    if (g_failures) {
        fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    printf("PASS test_cnn_global_pool_backward\n");
    return 0;
}
