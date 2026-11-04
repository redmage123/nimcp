/**
 * @file test_lnn_decide_readout.c
 * @brief Brain decisions blend the LNN's learned readout across the whole
 *        output (2026-11-04).
 *
 * The decision path sigmoid-gated only output[0:n_outputs] (64 of 4096) with
 * the raw LNN state, so history moved a decision by ~1e-6 (video probe
 * temporal = 0.0000). It now blends y = W·x + b — the readout trained
 * against the full target — into every output, at the training dt.
 *
 * Signatures (include/lnn/nimcp_lnn.h, nimcp_lnn_network.h, nimcp_lnn_training.h):
 *   int lnn_init(uint32_t n_threads);
 *   lnn_network_t* lnn_network_create_ncp(uint32_t n_inputs, uint32_t n_inter,
 *                                         uint32_t n_command, uint32_t n_outputs);
 *   void lnn_network_destroy(lnn_network_t* network);
 *   int lnn_network_readout_init(lnn_network_t*, uint32_t readout_dim,
 *                                float init_scale, uint64_t seed);
 *   int lnn_network_readout_forward(const lnn_network_t*, const float* x, float* y);
 *   int lnn_network_readout_blend(const lnn_network_t*, const float* x,
 *                                 float* out, uint32_t out_size, float weight);
 *   void  lnn_tune_set_decide_{readout,weight,dt}(float v);
 *   float lnn_tune_get_decide_{readout,weight,dt}(void);
 */
#include "nimcp.h"
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_training.h"
#include "lnn/nimcp_lnn_types.h"
#include "utils/tensor/nimcp_tensor.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d " #cond " : ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); g_failures++; } } while (0)

#define N_IN  8u
#define N_OUT 4u    /* LNN state dim feeding the readout */
#define R_DIM 12u   /* readout / decision dim, > N_OUT like 4096 > 64 */

static void set_known_weights(lnn_network_t* net) {
    float* W = (float*)nimcp_tensor_data(net->readout_W);
    float* b = (float*)nimcp_tensor_data(net->readout_b);
    for (uint32_t o = 0; o < R_DIM; o++) {
        for (uint32_t k = 0; k < N_OUT; k++) {
            W[o * N_OUT + k] = 0.1f * (float)(o + 1) - 0.05f * (float)k;
        }
        b[o] = 0.01f * (float)o - 0.02f;
    }
}

static lnn_network_t* make_net(int with_readout) {
    lnn_network_t* net = lnn_network_create_ncp(N_IN, 8, 8, N_OUT);
    if (!net) return NULL;
    if (with_readout) {
        if (lnn_network_readout_init(net, R_DIM, 0.5f, 3) != LNN_SUCCESS) {
            lnn_network_destroy(net);
            return NULL;
        }
        set_known_weights(net);
    }
    return net;
}

static void test_tunables(void) {
    CHECK(lnn_tune_get_decide_readout() == 1.0f, "readout on by default (%f)", lnn_tune_get_decide_readout());
    CHECK(lnn_tune_get_decide_weight() == 0.1f, "weight 0.1 by default (%f)", lnn_tune_get_decide_weight());
    CHECK(lnn_tune_get_decide_dt() == 1.0f, "dt 1.0 by default, matching training (%f)", lnn_tune_get_decide_dt());

    lnn_tune_set_decide_weight(1.5f);
    CHECK(lnn_tune_get_decide_weight() == 0.1f, "weight above 1 ignored");
    lnn_tune_set_decide_weight(-0.1f);
    CHECK(lnn_tune_get_decide_weight() == 0.1f, "negative weight ignored");
    lnn_tune_set_decide_weight(0.3f);
    CHECK(lnn_tune_get_decide_weight() == 0.3f, "weight settable");
    lnn_tune_set_decide_weight(0.1f);

    lnn_tune_set_decide_dt(0.0f);
    CHECK(lnn_tune_get_decide_dt() == 1.0f, "dt 0 ignored");
    lnn_tune_set_decide_dt(11.0f);
    CHECK(lnn_tune_get_decide_dt() == 1.0f, "dt above 10 ignored");
    lnn_tune_set_decide_dt(0.01f);
    CHECK(lnn_tune_get_decide_dt() == 0.01f, "dt settable (legacy 0.01)");
    lnn_tune_set_decide_dt(1.0f);

    lnn_tune_set_decide_readout(0.0f);
    CHECK(lnn_tune_get_decide_readout() == 0.0f, "readout can be turned off");
    lnn_tune_set_decide_readout(5.0f);
    CHECK(lnn_tune_get_decide_readout() == 1.0f, "any nonzero turns readout on");
}

static void test_blend_covers_whole_output(void) {
    lnn_network_t* net = make_net(1);
    CHECK(net != NULL, "create net with readout");
    if (!net) return;
    const float x[N_OUT] = { 0.5f, -1.0f, 2.0f, 0.75f };
    float y[R_DIM], out[R_DIM], before[R_DIM];
    CHECK(lnn_network_readout_forward(net, x, y) == LNN_SUCCESS, "readout forward");
    for (uint32_t o = 0; o < R_DIM; o++) out[o] = before[o] = 0.3f - 0.1f * (float)o;

    const float w = 0.25f;
    CHECK(lnn_network_readout_blend(net, x, out, R_DIM, w) == LNN_SUCCESS, "blend");
    for (uint32_t o = 0; o < R_DIM; o++) {
        float ref = (1.0f - w) * before[o] + w * y[o];
        CHECK(fabsf(out[o] - ref) < 1e-6f, "out[%u] = %f, expected %f", o, out[o], ref);
    }
    for (uint32_t o = N_OUT; o < R_DIM; o++) {
        CHECK(out[o] != before[o], "output %u beyond the state dim moved", o);
    }
    lnn_network_destroy(net);
}

static void test_blend_depends_on_state(void) {
    lnn_network_t* net = make_net(1);
    CHECK(net != NULL, "create net with readout");
    if (!net) return;
    const float x1[N_OUT] = { 1.0f, 0.0f, 0.0f, 0.0f };
    const float x2[N_OUT] = { 0.0f, 1.0f, 0.0f, 0.0f };
    float a[R_DIM], b[R_DIM];
    for (uint32_t o = 0; o < R_DIM; o++) a[o] = b[o] = 0.1f;
    CHECK(lnn_network_readout_blend(net, x1, a, R_DIM, 0.1f) == LNN_SUCCESS, "blend x1");
    CHECK(lnn_network_readout_blend(net, x2, b, R_DIM, 0.1f) == LNN_SUCCESS, "blend x2");
    double diff = 0.0;
    for (uint32_t o = 0; o < R_DIM; o++) diff += fabs((double)a[o] - (double)b[o]);
    CHECK(diff > 1e-3, "different LNN states give different decisions (|diff| = %g)", diff);
    lnn_network_destroy(net);
}

static void test_blend_weight_zero_is_noop(void) {
    lnn_network_t* net = make_net(1);
    CHECK(net != NULL, "create net with readout");
    if (!net) return;
    const float x[N_OUT] = { 1.0f, 2.0f, 3.0f, 4.0f };
    float out[R_DIM];
    for (uint32_t o = 0; o < R_DIM; o++) out[o] = 0.42f;
    CHECK(lnn_network_readout_blend(net, x, out, R_DIM, 0.0f) == LNN_SUCCESS, "blend w=0");
    for (uint32_t o = 0; o < R_DIM; o++) CHECK(out[o] == 0.42f, "out[%u] unchanged at w=0", o);
    lnn_network_destroy(net);
}

static void test_blend_rejects(void) {
    const float x[N_OUT] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float out[R_DIM];
    for (uint32_t o = 0; o < R_DIM; o++) out[o] = 0.42f;

    lnn_network_t* bare = make_net(0);
    CHECK(bare != NULL, "create net without readout");
    if (bare) {
        CHECK(lnn_network_readout_blend(bare, x, out, R_DIM, 0.1f) == LNN_ERROR_INVALID_STATE,
              "no readout → INVALID_STATE (caller falls back to the gate)");
        lnn_network_destroy(bare);
    }

    lnn_network_t* net = make_net(1);
    CHECK(net != NULL, "create net with readout");
    if (!net) return;
    CHECK(lnn_network_readout_blend(net, x, out, R_DIM - 1, 0.1f) == LNN_ERROR_INVALID_STATE,
          "size mismatch → INVALID_STATE");
    CHECK(lnn_network_readout_blend(net, x, out, R_DIM, 1.5f) != LNN_SUCCESS, "weight > 1 rejected");
    CHECK(lnn_network_readout_blend(net, x, out, R_DIM, -0.1f) != LNN_SUCCESS, "weight < 0 rejected");
    CHECK(lnn_network_readout_blend(net, x, out, R_DIM, NAN) != LNN_SUCCESS, "NaN weight rejected");
    CHECK(lnn_network_readout_blend(NULL, x, out, R_DIM, 0.1f) != LNN_SUCCESS, "NULL network rejected");
    CHECK(lnn_network_readout_blend(net, NULL, out, R_DIM, 0.1f) != LNN_SUCCESS, "NULL state rejected");
    CHECK(lnn_network_readout_blend(net, x, NULL, R_DIM, 0.1f) != LNN_SUCCESS, "NULL output rejected");
    for (uint32_t o = 0; o < R_DIM; o++) CHECK(out[o] == 0.42f, "rejected calls leave out[%u] alone", o);
    lnn_network_destroy(net);
}

static void test_blend_skips_non_finite(void) {
    lnn_network_t* net = make_net(1);
    CHECK(net != NULL, "create net with readout");
    if (!net) return;
    ((float*)nimcp_tensor_data(net->readout_b))[2] = NAN;
    const float x[N_OUT] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float out[R_DIM];
    for (uint32_t o = 0; o < R_DIM; o++) out[o] = 0.42f;
    CHECK(lnn_network_readout_blend(net, x, out, R_DIM, 0.5f) == LNN_SUCCESS, "blend");
    CHECK(out[2] == 0.42f, "non-finite readout value leaves out[2] alone (%f)", out[2]);
    CHECK(isfinite(out[0]) && out[0] != 0.42f, "finite readout values still blend (%f)", out[0]);
    lnn_network_destroy(net);
}

int main(void) {
    nimcp_init();
    if (lnn_init(1) != LNN_SUCCESS) { fprintf(stderr, "lnn_init failed\n"); return 1; }

    test_tunables();
    test_blend_covers_whole_output();
    test_blend_depends_on_state();
    test_blend_weight_zero_is_noop();
    test_blend_rejects();
    test_blend_skips_non_finite();

    if (g_failures) { fprintf(stderr, "test_lnn_decide_readout: %d failure(s)\n", g_failures); return 1; }
    printf("test_lnn_decide_readout: all passed\n");
    return 0;
}
