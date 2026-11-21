/**
 * @file test_lnn_bptt.c
 * @brief Exact truncated BPTT for the LNN (2026-11-07, nimcp_lnn_bptt.h).
 *
 * Regression: the legacy adjoint used the CURRENT layer state at every
 * history step, gave W_in no gradient and pushed the output adjoint into
 * every layer, so a loss could never reach earlier inputs and the LNN could
 * not learn temporal structure (video next-frame training showed none).
 * Alongside it: lnn_network_zero_gradients never cleared the gradient data,
 * and explicit Euler at dt = 1 oscillated for every neuron with tau < 0.5.
 *
 *  a. finite differences over a T=5 recorded window on a 4-layer NCP for
 *     W_in, b_in, W_rec, W_tau, b_tau and tau_base of every layer:
 *     exponential Euler (default) at dt = 1, explicit Euler at dt = 0.2
 *     (stable there; at dt = 1 it is chaotic and finite differences are not
 *     a valid reference);
 *  b. lnn_network_zero_gradients clears the gradient data;
 *  c. forwards made without recording (decide/inference) leave the trace
 *     alone, and one that moves the state cuts the chain at that point;
 *  d. a delay task (target(t) = input(t-1)) learned through lnn_backward with
 *     a test-side readout and Adam, for 3 seeds with deterministic init:
 *     with the default horizon (2) the MSE must fall far below the no-memory
 *     baseline (1.0); with the trace cleared every step (depth-1 gradient,
 *     what the legacy path amounted to) it must not.
 *
 * Signatures (include/lnn/nimcp_lnn.h, nimcp_lnn_network.h, nimcp_lnn_bptt.h,
 * nimcp_lnn_training.h, utils/tensor/nimcp_tensor.h):
 *   int lnn_init(uint32_t n_threads);
 *   void lnn_shutdown(void);
 *   int lnn_forward_step(lnn_network_t*, const nimcp_tensor_t*, nimcp_tensor_t*, float dt);
 *   int lnn_backward(lnn_network_t*, const nimcp_tensor_t* loss_grad);
 *   lnn_network_t* lnn_network_create_ncp(uint32_t, uint32_t, uint32_t, uint32_t);
 *   void lnn_network_destroy(lnn_network_t*);
 *   int lnn_network_init_weights(lnn_network_t*, uint64_t seed);
 *   void lnn_network_reset_state(lnn_network_t*);
 *   int lnn_network_zero_gradients(lnn_network_t*);
 *   void lnn_network_set_bptt_record(lnn_network_t*, bool on);
 *   uint32_t lnn_bptt_trace_len(const lnn_network_t*);
 *   void lnn_bptt_clear(lnn_network_t*);
 *   int lnn_bptt_backward(lnn_network_t*, const nimcp_tensor_t*, lnn_bptt_stats_t*);
 *   void lnn_tune_set_cpu_integrator(float v);
 *   void lnn_tune_set_bptt_steps(float v);
 *   float lnn_tune_get_bptt_steps(void);
 *   nimcp_tensor_t* nimcp_tensor_create(const uint32_t* dims, uint32_t rank, nimcp_dtype_t);
 *   void* nimcp_tensor_data(nimcp_tensor_t*);
 *   size_t nimcp_tensor_numel(const nimcp_tensor_t*);
 *   void nimcp_tensor_destroy(nimcp_tensor_t*);
 */
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_bptt.h"
#include "lnn/nimcp_lnn_types.h"
#include "lnn/nimcp_lnn_training.h"
#include "utils/tensor/nimcp_tensor.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;
#define EXPECT(cond, ...) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); g_failures++; } } while (0)

static uint64_t g_rng = 0x9E3779B97F4A7C15ULL;
static float frand(void) {   /* [0, 1) */
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 7; g_rng ^= g_rng << 17;
    return (float)((g_rng >> 40) & 0xFFFFFF) / 16777216.0f;
}
static float frandn(void) {
    float u1 = frand() + 1e-7f, u2 = frand();
    return sqrtf(-2.0f * logf(u1)) * cosf(6.2831853f * u2);
}

static nimcp_tensor_t* vec(uint32_t n) {
    uint32_t d[1] = { n };
    return nimcp_tensor_create(d, 1, NIMCP_DTYPE_F32);
}

/* Construction draws W_tau, b_tau and the NCP wiring from a clock-seeded
 * RNG; overwrite every parameter from g_rng so runs are reproducible.
 * tau_base log-uniform 0.5..50 like lnn_layer_create. */
static void set_params_deterministic(lnn_network_t* net, float w_std) {
    for (uint32_t i = 0; i < net->n_layers; i++) {
        lnn_layer_t* L = net->layers[i];
        nimcp_tensor_t* P[5] = { L->W_in, L->W_rec, L->W_tau, L->b_in, L->b_tau };
        const float sd[5] = { w_std, w_std, w_std, 0.0f, 0.0f };
        for (int p = 0; p < 5; p++) {
            float* d = (float*)nimcp_tensor_data(P[p]);
            for (size_t k = 0; k < nimcp_tensor_numel(P[p]); k++) d[k] = sd[p] * frandn();
        }
        float* tb = (float*)nimcp_tensor_data(L->tau_base);
        const uint32_t n = L->n_neurons;
        for (uint32_t k = 0; k < n; k++) {
            float f = (n > 1) ? (float)k / (float)(n - 1) : 0.0f;
            tb[k] = expf(logf(0.5f) + f * (logf(50.0f) - logf(0.5f)));
        }
    }
    lnn_network_reset_state(net);
}

static double grad_sumsq(lnn_network_t* net) {
    double sq = 0.0;
    for (uint32_t li = 0; li < net->n_layers; li++) {
        lnn_layer_t* L = net->layers[li];
        nimcp_tensor_t* G[6] = { L->grad_W_in, L->grad_b_in, L->grad_W_rec,
                                 L->grad_W_tau, L->grad_b_tau, L->grad_tau_base };
        for (int pi = 0; pi < 6; pi++) {
            if (!G[pi]) continue;
            const float* gd = (const float*)nimcp_tensor_data(G[pi]);
            for (size_t k = 0; k < nimcp_tensor_numel(G[pi]); k++) sq += (double)gd[k] * gd[k];
        }
    }
    return sq;
}

/*---------------------------------------------------------------------------
 * a. Finite-difference check
 *-------------------------------------------------------------------------*/
#define FD_T 5
#define FD_IN 3
#define FD_OUT 3

static float g_fd_inputs[FD_T][FD_IN];
static float g_fd_w[FD_OUT];

/* Replays the window from a zero state; returns L = w·out(T). */
static double fd_run(lnn_network_t* net, float dt, bool record) {
    nimcp_tensor_t* in = vec(FD_IN);
    nimcp_tensor_t* out = vec(FD_OUT);
    lnn_network_reset_state(net);
    lnn_bptt_clear(net);
    for (int t = 0; t < FD_T; t++) {
        memcpy(nimcp_tensor_data(in), g_fd_inputs[t], sizeof(g_fd_inputs[t]));
        lnn_network_set_bptt_record(net, record);
        lnn_forward_step(net, in, out, dt);
        lnn_network_set_bptt_record(net, false);
    }
    const float* o = (const float*)nimcp_tensor_data(out);
    double L = 0.0;
    for (int j = 0; j < FD_OUT; j++) L += (double)g_fd_w[j] * o[j];
    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
    return L;
}

static void test_finite_difference(int integrator, float dt) {
    lnn_tune_set_cpu_integrator((float)integrator);
    lnn_tune_set_bptt_steps(0.0f);   /* whole window */
    lnn_network_t* net = lnn_network_create_ncp(FD_IN, 4, 3, FD_OUT);
    EXPECT(net != NULL, "create");
    if (!net) return;
    g_rng = 0x1234567ULL + (uint64_t)integrator;
    set_params_deterministic(net, 0.3f);
    for (int t = 0; t < FD_T; t++)
        for (int k = 0; k < FD_IN; k++) g_fd_inputs[t][k] = frand() * 2.0f - 1.0f;
    /* Small loss weights keep every gradient entry inside the ±10 clamp. */
    for (int j = 0; j < FD_OUT; j++) g_fd_w[j] = 0.05f * (frand() * 2.0f - 1.0f);

    lnn_network_zero_gradients(net);
    fd_run(net, dt, true);
    EXPECT(lnn_bptt_trace_len(net) == FD_T, "trace len %u", lnn_bptt_trace_len(net));
    nimcp_tensor_t* lg = vec(FD_OUT);
    memcpy(nimcp_tensor_data(lg), g_fd_w, sizeof(g_fd_w));
    lnn_bptt_stats_t st;
    EXPECT(lnn_bptt_backward(net, lg, &st) == LNN_SUCCESS, "backward");
    EXPECT(st.steps_used == FD_T && st.chain_breaks == 0,
           "steps_used %u breaks %u", st.steps_used, st.chain_breaks);

    const char* names[6] = { "W_in", "b_in", "W_rec", "W_tau", "b_tau", "tau_base" };
    int checked = 0, bad = 0;
    double worst = 0.0;
    for (uint32_t li = 0; li < net->n_layers; li++) {
        lnn_layer_t* L = net->layers[li];
        nimcp_tensor_t* P[6] = { L->W_in, L->b_in, L->W_rec, L->W_tau, L->b_tau, L->tau_base };
        nimcp_tensor_t* G[6] = { L->grad_W_in, L->grad_b_in, L->grad_W_rec,
                                 L->grad_W_tau, L->grad_b_tau, L->grad_tau_base };
        for (int pi = 0; pi < 6; pi++) {
            if (!P[pi] || !G[pi]) continue;
            float* p = (float*)nimcp_tensor_data(P[pi]);
            const float* gd = (const float*)nimcp_tensor_data(G[pi]);
            size_t ne = nimcp_tensor_numel(P[pi]);
            size_t samples = ne < 6 ? ne : 6;
            for (size_t s = 0; s < samples; s++) {
                size_t idx = (size_t)(frand() * (float)ne) % ne;
                const float eps = 1e-3f;
                float orig = p[idx];
                p[idx] = orig + eps; double lp = fd_run(net, dt, false);
                p[idx] = orig - eps; double lm = fd_run(net, dt, false);
                p[idx] = orig;
                double fd = (lp - lm) / (2.0 * eps);
                double an = gd[idx];
                double err = fabs(an - fd);
                double scale = fmax(fabs(an), fabs(fd));
                checked++;
                if (err > 1e-2 * scale + 2e-5) {
                    bad++;
                    fprintf(stderr, "  layer %u %s[%zu]: analytic %.6g fd %.6g\n",
                            li, names[pi], idx, an, fd);
                }
                if (scale > 1e-3 && err / scale > worst) worst = err / scale;
            }
        }
    }
    printf("finite difference (%s, dt=%.1f): %d params checked, %d outside tolerance, "
           "worst rel err %.2e (|g|>1e-3)\n",
           integrator ? "exponential Euler" : "explicit Euler", dt, checked, bad, worst);
    EXPECT(bad == 0, "%d of %d gradient entries disagree with finite differences", bad, checked);
    nimcp_tensor_destroy(lg);
    lnn_network_destroy(net);
}

/*---------------------------------------------------------------------------
 * b. zero_gradients clears the gradient data
 *-------------------------------------------------------------------------*/
static void test_zero_gradients(void) {
    lnn_tune_set_bptt_steps(0.0f);
    lnn_network_t* net = lnn_network_create_ncp(3, 4, 3, 3);
    EXPECT(net != NULL, "create");
    if (!net) return;
    g_rng = 99;
    set_params_deterministic(net, 0.3f);
    nimcp_tensor_t* in = vec(3);
    nimcp_tensor_t* out = vec(3);
    nimcp_tensor_t* lg = vec(3);
    float* id = (float*)nimcp_tensor_data(in);
    id[0] = 0.5f; id[1] = -0.3f; id[2] = 0.8f;
    float* g = (float*)nimcp_tensor_data(lg);
    g[0] = 1.0f; g[1] = -1.0f; g[2] = 0.5f;
    lnn_network_set_bptt_record(net, true);
    lnn_forward_step(net, in, out, 1.0f);
    lnn_network_set_bptt_record(net, false);
    lnn_bptt_backward(net, lg, NULL);
    double before = grad_sumsq(net);
    lnn_network_zero_gradients(net);
    double after = grad_sumsq(net);
    printf("zero_gradients: |grad|^2 %.3g -> %.3g\n", before, after);
    EXPECT(before > 0.0 && after == 0.0, "zero_gradients left |grad|^2 = %g", after);
    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
    nimcp_tensor_destroy(lg);
    lnn_network_destroy(net);
}

/*---------------------------------------------------------------------------
 * c. Unrecorded forwards
 *-------------------------------------------------------------------------*/
static void test_unrecorded_forwards(void) {
    lnn_tune_set_bptt_steps(0.0f);
    lnn_network_t* net = lnn_network_create_ncp(3, 4, 3, 3);
    EXPECT(net != NULL, "create");
    if (!net) return;
    g_rng = 3;
    set_params_deterministic(net, 0.3f);
    nimcp_tensor_t* in = vec(3);
    nimcp_tensor_t* out = vec(3);
    nimcp_tensor_t* lg = vec(3);
    float* id = (float*)nimcp_tensor_data(in);
    for (int t = 0; t < 4; t++) {
        id[0] = 0.3f * t; id[1] = -0.2f; id[2] = 0.5f;
        lnn_network_set_bptt_record(net, true);
        lnn_forward_step(net, in, out, 1.0f);
        lnn_network_set_bptt_record(net, false);
    }
    EXPECT(lnn_bptt_trace_len(net) == 4, "trace len %u after 4 recorded", lnn_bptt_trace_len(net));
    float* g = (float*)nimcp_tensor_data(lg);
    g[0] = 1.0f; g[1] = -0.5f; g[2] = 0.25f;
    lnn_bptt_stats_t st;
    lnn_network_zero_gradients(net);
    lnn_bptt_backward(net, lg, &st);
    EXPECT(st.steps_used == 4 && st.chain_breaks == 0,
           "continuous chain: used %u breaks %u", st.steps_used, st.chain_breaks);

    /* Two decide-style forwards (not recorded) move the state. */
    lnn_forward_step(net, in, out, 1.0f);
    lnn_forward_step(net, in, out, 1.0f);
    EXPECT(lnn_bptt_trace_len(net) == 4, "unrecorded forwards changed the trace: %u",
           lnn_bptt_trace_len(net));

    lnn_network_set_bptt_record(net, true);
    lnn_forward_step(net, in, out, 1.0f);
    lnn_network_set_bptt_record(net, false);
    EXPECT(lnn_bptt_trace_len(net) == 5, "trace len %u", lnn_bptt_trace_len(net));
    lnn_network_zero_gradients(net);
    lnn_bptt_backward(net, lg, &st);
    EXPECT(st.steps_used == 1 && st.chain_breaks == 1,
           "chain must stop at the unrecorded forwards: used %u breaks %u",
           st.steps_used, st.chain_breaks);
    printf("unrecorded forwards: trace untouched, chain cut (used %u of %u, breaks %u)\n",
           st.steps_used, st.steps_recorded, st.chain_breaks);

    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
    nimcp_tensor_destroy(lg);
    lnn_network_destroy(net);
}

/*---------------------------------------------------------------------------
 * d. Delay task
 *-------------------------------------------------------------------------*/
#define DT_IN 8
#define DT_OUT 8
#define DT_DELAY 1

typedef struct { float* m; float* v; size_t n; } adam_t;

/* Adam on every LNN parameter and the readout; MSE over the last `tail`. */
static double delay_task(int steps, int tail, bool depth1, uint64_t seed) {
    g_rng = seed;
    lnn_network_t* net = lnn_network_create_ncp(DT_IN, 12, 8, DT_OUT);
    if (!net) return NAN;
    set_params_deterministic(net, 0.1f);
    adam_t A[4][6];
    memset(A, 0, sizeof(A));
    for (uint32_t i = 0; i < 4; i++) {
        lnn_layer_t* L = net->layers[i];
        nimcp_tensor_t* P[6] = { L->W_in, L->b_in, L->W_rec, L->W_tau, L->b_tau, L->tau_base };
        for (int p = 0; p < 6; p++) {
            A[i][p].n = nimcp_tensor_numel(P[p]);
            A[i][p].m = calloc(A[i][p].n, sizeof(float));
            A[i][p].v = calloc(A[i][p].n, sizeof(float));
        }
    }
    float r[DT_OUT + 1], rm[DT_OUT + 1] = {0}, rv[DT_OUT + 1] = {0};
    for (int j = 0; j < DT_OUT; j++) r[j] = (frand() - 0.5f) * 0.2f;
    r[DT_OUT] = 0.0f;
    nimcp_tensor_t* in = vec(DT_IN);
    nimcp_tensor_t* out = vec(DT_OUT);
    nimcp_tensor_t* lg = vec(DT_OUT);
    float bits[DT_DELAY + 1] = {0};
    const float lr = 0.003f, b1 = 0.9f, b2 = 0.999f;
    double tail_sq = 0.0;
    int tail_n = 0;
    for (int t = 1; t <= steps; t++) {
        float b = frand() < 0.5f ? -1.0f : 1.0f;
        for (int d = DT_DELAY; d > 0; d--) bits[d] = bits[d - 1];
        bits[0] = b;
        float* id = (float*)nimcp_tensor_data(in);
        id[0] = b;
        for (int q = 1; q < DT_IN; q++) id[q] = 0.25f * (float)q - 1.0f;   /* fixed context */
        if (depth1) lnn_bptt_clear(net);
        lnn_network_set_bptt_record(net, true);
        lnn_forward_step(net, in, out, 1.0f);
        lnn_network_set_bptt_record(net, false);
        const float* o = (const float*)nimcp_tensor_data(out);
        float y = r[DT_OUT];
        for (int j = 0; j < DT_OUT; j++) y += r[j] * o[j];
        const float e = y - bits[DT_DELAY];
        if (t > steps - tail) { tail_sq += (double)e * e; tail_n++; }
        float* g = (float*)nimcp_tensor_data(lg);
        for (int j = 0; j < DT_OUT; j++) g[j] = e * r[j];
        lnn_network_zero_gradients(net);
        const float bc1 = 1.0f - powf(b1, (float)t), bc2 = 1.0f - powf(b2, (float)t);
        if (lnn_backward(net, lg) == 0) {
            for (uint32_t i = 0; i < 4; i++) {
                lnn_layer_t* L = net->layers[i];
                nimcp_tensor_t* P[6] = { L->W_in, L->b_in, L->W_rec, L->W_tau, L->b_tau, L->tau_base };
                nimcp_tensor_t* G[6] = { L->grad_W_in, L->grad_b_in, L->grad_W_rec,
                                         L->grad_W_tau, L->grad_b_tau, L->grad_tau_base };
                for (int p = 0; p < 6; p++) {
                    float* pp = (float*)nimcp_tensor_data(P[p]);
                    const float* gd = (const float*)nimcp_tensor_data(G[p]);
                    adam_t* a = &A[i][p];
                    for (size_t k = 0; k < a->n; k++) {
                        a->m[k] = b1 * a->m[k] + (1.0f - b1) * gd[k];
                        a->v[k] = b2 * a->v[k] + (1.0f - b2) * gd[k] * gd[k];
                        pp[k] -= lr * (a->m[k] / bc1) / (sqrtf(a->v[k] / bc2) + 1e-8f);
                    }
                    if (p == 5) for (size_t k = 0; k < a->n; k++) if (pp[k] < 0.05f) pp[k] = 0.05f;
                }
            }
        }
        for (int j = 0; j <= DT_OUT; j++) {
            const float gk = (j < DT_OUT) ? e * o[j] : e;
            rm[j] = b1 * rm[j] + (1.0f - b1) * gk;
            rv[j] = b2 * rv[j] + (1.0f - b2) * gk * gk;
            r[j] -= lr * (rm[j] / bc1) / (sqrtf(rv[j] / bc2) + 1e-8f);
        }
    }
    for (uint32_t i = 0; i < 4; i++)
        for (int p = 0; p < 6; p++) { free(A[i][p].m); free(A[i][p].v); }
    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
    nimcp_tensor_destroy(lg);
    lnn_network_destroy(net);
    return tail_n ? tail_sq / tail_n : NAN;
}

static void test_delay_task(void) {
    lnn_tune_set_cpu_integrator(1.0f);
    lnn_tune_set_bptt_steps(2.0f);   /* production default */
    const int steps = 8000, tail = 2000;
    const uint64_t seeds[3] = { 1, 2, 3 };
    for (int s = 0; s < 3; s++) {
        double full = delay_task(steps, tail, false, seeds[s]);
        double d1   = delay_task(steps, tail, true, seeds[s]);
        printf("delay task seed %llu (target = input 1 step back, no-memory MSE 1.0): "
               "BPTT horizon %.0f MSE %.3f, depth-1 gradient MSE %.3f\n",
               (unsigned long long)seeds[s], lnn_tune_get_bptt_steps(), full, d1);
        EXPECT(isfinite(full) && full < 0.1, "seed %llu: BPTT MSE %.3f not below 0.1",
               (unsigned long long)seeds[s], full);
        EXPECT(isfinite(d1) && d1 > 0.3, "seed %llu: depth-1 MSE %.3f — task solvable without memory?",
               (unsigned long long)seeds[s], d1);
    }
}

/*---------------------------------------------------------------------------
 * Timing at production shape (128 → 64 → 32 → 64, 100 recorded steps)
 *-------------------------------------------------------------------------*/
static void report_timing(void) {
    lnn_network_t* net = lnn_network_create_ncp(128, 64, 32, 64);
    if (!net) return;
    lnn_network_init_weights(net, 5);
    nimcp_tensor_t* in = vec(128);
    nimcp_tensor_t* out = vec(64);
    nimcp_tensor_t* lg = vec(64);
    float* id = (float*)nimcp_tensor_data(in);
    for (int t = 0; t < 100; t++) {
        for (int k = 0; k < 128; k++) id[k] = frand() - 0.5f;
        lnn_network_set_bptt_record(net, true);
        lnn_forward_step(net, in, out, 1.0f);
        lnn_network_set_bptt_record(net, false);
    }
    float* g = (float*)nimcp_tensor_data(lg);
    for (int j = 0; j < 64; j++) g[j] = frand() - 0.5f;
    const float horizons[3] = { 2.0f, 16.0f, 0.0f };
    for (int h = 0; h < 3; h++) {
        lnn_tune_set_bptt_steps(horizons[h]);
        lnn_bptt_stats_t st;
        double best = 1e30;
        for (int r = 0; r < 5; r++) {
            lnn_network_zero_gradients(net);
            lnn_bptt_backward(net, lg, &st);
            if (st.time_ms < best) best = st.time_ms;
        }
        printf("production shape 128-64-32-64: backward through %u of %u steps %.2f ms\n",
               st.steps_used, st.steps_recorded, best);
    }
    lnn_tune_set_bptt_steps(2.0f);
    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
    nimcp_tensor_destroy(lg);
    lnn_network_destroy(net);
}

int main(void) {
    lnn_init(1);
    test_finite_difference(0, 0.2f);
    test_finite_difference(1, 1.0f);   /* production default */
    test_zero_gradients();
    test_unrecorded_forwards();
    test_delay_task();
    report_timing();
    lnn_shutdown();
    if (g_failures) {
        fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    printf("PASS test_lnn_bptt\n");
    return 0;
}
