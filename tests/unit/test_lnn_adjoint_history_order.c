/**
 * @file test_lnn_adjoint_history_order.c
 * @brief The LNN adjoint integrates the state history in time order after
 *        the ring buffer wraps (2026-11-05).
 *
 * lnn_gradient_compute_adjoint read network->state_history[step] by raw
 * index. Once history_len reaches capacity the buffer wraps, so the backward
 * pass walked the stored states out of time order. Two identical networks
 * take the same 12 forward steps: A keeps 5 states (wrapped) and integrates
 * all of them; B keeps all 12 and integrates the last 5 (max_adjoint_steps).
 * Both cover the same 5 states, so every parameter gradient must match.
 * The two are loaded from one saved network: NCP construction is not
 * deterministic for a weight seed, so separately created networks differ.
 *
 * Signatures (include/lnn/nimcp_lnn.h, nimcp_lnn_network.h, nimcp_lnn_gradient.h):
 *   int lnn_init(uint32_t n_threads);
 *   lnn_network_t* lnn_network_create_ncp(uint32_t n_inputs, uint32_t n_inter,
 *                                         uint32_t n_command, uint32_t n_outputs);
 *   int  lnn_network_init_weights(lnn_network_t*, uint64_t seed);
 *   int  lnn_network_save(const lnn_network_t*, const char* filepath);
 *   lnn_network_t* lnn_network_load(const char* filepath);
 *   void lnn_network_set_training(lnn_network_t*, bool training);
 *   int  lnn_network_zero_gradients(lnn_network_t*);
 *   void lnn_network_destroy(lnn_network_t*);
 *   int  lnn_forward_step(lnn_network_t*, const nimcp_tensor_t* input,
 *                         nimcp_tensor_t* output, float dt);
 *   lnn_gradient_ctx_t* lnn_gradient_ctx_create(lnn_network_t*, uint32_t max_steps,
 *                                               bool use_checkpointing, uint32_t checkpoint_interval);
 *   void lnn_gradient_ctx_destroy(lnn_gradient_ctx_t*);
 *   int  lnn_gradient_compute_adjoint(lnn_gradient_ctx_t*, lnn_network_t*,
 *                                     const nimcp_tensor_t* dL_dx_final);
 */
#include "nimcp.h"
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_gradient.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_types.h"
#include "utils/tensor/nimcp_tensor.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d " #cond " : ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); g_failures++; } } while (0)

#define N_IN   8u
#define N_OUT  4u
#define STEPS  12u
#define WINDOW 5u

static lnn_network_t* load_net(const char* path, uint32_t history) {
    lnn_network_t* net = lnn_network_load(path);
    if (!net) return NULL;
    net->config->bptt_truncation = history;
    lnn_network_set_training(net, true);
    return net;
}

static void run_steps(lnn_network_t* net) {
    uint32_t in_dims[1] = { N_IN }, out_dims[1] = { N_OUT };
    nimcp_tensor_t* in = nimcp_tensor_create(in_dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* out = nimcp_tensor_create(out_dims, 1, NIMCP_DTYPE_F32);
    if (!in || !out) { nimcp_tensor_destroy(in); nimcp_tensor_destroy(out); return; }
    float* x = (float*)nimcp_tensor_data(in);
    for (uint32_t s = 0; s < STEPS; s++) {
        for (uint32_t i = 0; i < N_IN; i++) x[i] = sinf(0.7f * (float)s + 0.9f * (float)i);
        CHECK(lnn_forward_step(net, in, out, 1.0f) == 0, "forward step %u", s);
    }
    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
}

static int adjoint(lnn_network_t* net, uint32_t max_adjoint_steps) {
    lnn_network_zero_gradients(net);
    lnn_gradient_ctx_t* ctx = lnn_gradient_ctx_create(net, 100, false, 0);
    if (!ctx) return -1;
    ctx->max_adjoint_steps = max_adjoint_steps;
    uint32_t dims[1] = { N_OUT };
    nimcp_tensor_t* dl = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    float* d = (float*)nimcp_tensor_data(dl);
    for (uint32_t i = 0; i < N_OUT; i++) d[i] = 0.25f * (float)(i + 1);
    int rc = lnn_gradient_compute_adjoint(ctx, net, dl);
    nimcp_tensor_destroy(dl);
    lnn_gradient_ctx_destroy(ctx);
    return rc;
}

static void compare(const char* what, const nimcp_tensor_t* a, const nimcp_tensor_t* b,
                    double* abs_sum) {
    if (!a || !b) return;
    size_t n = nimcp_tensor_numel(a);
    CHECK(n == nimcp_tensor_numel(b), "%s sizes", what);
    const float* pa = (const float*)nimcp_tensor_data_const(a);
    const float* pb = (const float*)nimcp_tensor_data_const(b);
    for (size_t i = 0; i < n; i++) {
        *abs_sum += fabs((double)pa[i]);
        CHECK(fabsf(pa[i] - pb[i]) <= 1e-5f * (1.0f + fabsf(pb[i])),
              "%s[%zu]: wrapped %g vs unwrapped %g", what, i, pa[i], pb[i]);
    }
}

int main(void) {
    nimcp_init();
    CHECK(lnn_init(1) == 0, "lnn_init");

    char path[128];
    snprintf(path, sizeof(path), "/tmp/test_lnn_adjoint_order_%d.lnn", (int)getpid());
    lnn_network_t* base = lnn_network_create_ncp(N_IN, 8, 8, N_OUT);
    CHECK(base != NULL, "create base network");
    if (!base) return 1;
    lnn_network_init_weights(base, 42);
    CHECK(lnn_network_save(base, path) == 0, "save base network");
    lnn_network_destroy(base);
    lnn_network_t* a = load_net(path, WINDOW);   /* wraps after 5 steps */
    lnn_network_t* b = load_net(path, 64);       /* keeps all 12 */
    remove(path);
    CHECK(a && b, "create networks");
    if (!a || !b) return 1;
    CHECK(a->history_capacity == WINDOW && b->history_capacity == 64, "history capacities");

    run_steps(a);
    run_steps(b);
    CHECK(a->history_len == WINDOW, "A holds %u states (%u)", WINDOW, a->history_len);
    CHECK(a->history_write_idx != 0, "A's ring buffer has wrapped (write_idx %u)", a->history_write_idx);
    CHECK(b->history_len == STEPS, "B holds all %u states (%u)", STEPS, b->history_len);

    CHECK(adjoint(a, 0) == 0, "adjoint A (all 5 stored states)");
    CHECK(adjoint(b, WINDOW) == 0, "adjoint B (last 5 of 12 states)");

    double total = 0.0;
    for (uint32_t l = 0; l < a->n_layers && l < b->n_layers; l++) {
        lnn_layer_t* la = a->layers[l];
        lnn_layer_t* lb = b->layers[l];
        compare("grad_W_in", la->grad_W_in, lb->grad_W_in, &total);
        compare("grad_W_rec", la->grad_W_rec, lb->grad_W_rec, &total);
        compare("grad_W_tau", la->grad_W_tau, lb->grad_W_tau, &total);
        compare("grad_b_in", la->grad_b_in, lb->grad_b_in, &total);
        compare("grad_b_tau", la->grad_b_tau, lb->grad_b_tau, &total);
        compare("grad_tau_base", la->grad_tau_base, lb->grad_tau_base, &total);
    }
    CHECK(total > 0.0, "gradients are nonzero (sum |g| = %g)", total);

    lnn_network_destroy(a);
    lnn_network_destroy(b);
    if (g_failures) { fprintf(stderr, "test_lnn_adjoint_history_order: %d failure(s)\n", g_failures); return 1; }
    printf("test_lnn_adjoint_history_order: all passed\n");
    return 0;
}
