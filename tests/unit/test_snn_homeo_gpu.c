/**
 * @file test_snn_homeo_gpu.c
 * @brief Device-resident SNN homeostasis matches the host reference (2026-11-05).
 *
 * The current-based GPU step ran AHP / pump / basket / IP / depression as host
 * loops over every neuron each 1 ms step (~150-200 ms at 3.25M neurons). They
 * now run as kernels around the LIF kernel. This test drives the real LIF
 * forward to produce spikes, runs the module's pre/post for many steps, and
 * checks every array against the host reference: snn_adaptation_update for
 * AHP / pump and the IP + depression formula of snn_cb_post_spike_pop.
 * Runs against the CPU emulation without CUDA and against the kernels with
 * NIMCP_TEST_ALLOW_GPU=1 on a CUDA build.
 *
 * Signatures (include/gpu/snn/nimcp_snn_gpu.h, nimcp_snn_homeo_math.h,
 * include/gpu/context/nimcp_gpu_context.h, include/gpu/tensor/nimcp_tensor_gpu.h,
 * include/snn/nimcp_snn_adaptation.h):
 *   nimcp_gpu_context_t* nimcp_gpu_context_create_auto(void);
 *   void nimcp_gpu_context_destroy(nimcp_gpu_context_t*);
 *   nimcp_lif_state_t* nimcp_lif_state_create(nimcp_gpu_context_t*, size_t, const nimcp_lif_params_t*);
 *   void nimcp_lif_state_destroy(nimcp_lif_state_t*);
 *   bool nimcp_gpu_lif_forward(nimcp_gpu_context_t*, nimcp_lif_state_t*, const nimcp_gpu_tensor_t*);
 *   nimcp_gpu_tensor_t* nimcp_gpu_tensor_from_host(nimcp_gpu_context_t*, const void*, const size_t*, uint32_t, nimcp_gpu_precision_t);
 *   bool nimcp_gpu_tensor_to_host(const nimcp_gpu_tensor_t*, void*);
 *   void nimcp_gpu_tensor_destroy(nimcp_gpu_tensor_t*);
 *   nimcp_snn_homeo_gpu_t* nimcp_snn_homeo_gpu_create(nimcp_gpu_context_t*, nimcp_lif_state_t*, const uint32_t*, uint32_t);
 *   void nimcp_snn_homeo_gpu_destroy(nimcp_snn_homeo_gpu_t*);
 *   bool nimcp_snn_homeo_gpu_matches(const nimcp_snn_homeo_gpu_t*, const nimcp_lif_state_t*, size_t, uint32_t);
 *   bool nimcp_snn_homeo_gpu_upload_pop(nimcp_snn_homeo_gpu_t*, uint32_t, const float* x5);
 *   bool nimcp_snn_homeo_gpu_download_pop(const nimcp_snn_homeo_gpu_t*, uint32_t, float* x5);
 *   bool nimcp_snn_homeo_gpu_set_params(nimcp_snn_homeo_gpu_t*, const nimcp_snn_homeo_pop_params_t*, const nimcp_snn_homeo_globals_t*);
 *   bool nimcp_snn_homeo_gpu_pre(nimcp_snn_homeo_gpu_t*, nimcp_gpu_tensor_t*);
 *   bool nimcp_snn_homeo_gpu_post(nimcp_snn_homeo_gpu_t*, uint32_t*);
 *   uint32_t nimcp_snn_homeo_pop_of(const uint32_t*, uint32_t, uint32_t);
 *   snn_adaptation_state_t* snn_adaptation_create(uint32_t, float tau_ms, float gain_mv, float spike_bump);
 *   void snn_adaptation_update(snn_adaptation_state_t*, const float* fired, float dt_ms);
 *   void snn_adaptation_destroy(snn_adaptation_state_t*);
 */
#include "nimcp.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/snn/nimcp_snn_homeo_math.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "snn/nimcp_snn_adaptation.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d " #cond " : ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); g_failures++; } } while (0)

/* Layout: P0 full (AHP + pump + IP + basket), P1 empty, P2 inactive,
 * P3 IP + basket only. */
#define N_POPS 4u
static const uint32_t OFFS[N_POPS + 1] = { 0, 7, 7, 12, 21 };
#define N 21u
#define STEPS 25
#define DT 1.0f

static float frand(uint32_t* s, float lo, float hi) {
    *s = *s * 1664525u + 1013904223u;
    return lo + (hi - lo) * (float)(*s >> 8) / 16777216.0f;
}

static bool close_enough(float a, float b) {
    return fabsf(a - b) <= 1e-5f * (1.0f + fabsf(b));
}

static void test_pop_of(void) {
    const uint32_t off[] = { 0, 5, 5, 8 };
    CHECK(nimcp_snn_homeo_pop_of(off, 3, 0) == 0, "neuron 0 in pop 0");
    CHECK(nimcp_snn_homeo_pop_of(off, 3, 4) == 0, "neuron 4 in pop 0");
    CHECK(nimcp_snn_homeo_pop_of(off, 3, 5) == 2, "neuron 5 skips empty pop 1");
    CHECK(nimcp_snn_homeo_pop_of(off, 3, 7) == 2, "neuron 7 in pop 2");
    CHECK(nimcp_snn_homeo_pop_of(OFFS, N_POPS, 12) == 3, "neuron 12 in pop 3");
    CHECK(nimcp_snn_homeo_pop_of(OFFS, N_POPS, 7) == 2, "neuron 7 skips empty pop 1");
}

static nimcp_lif_state_t* make_lif(nimcp_gpu_context_t* ctx, size_t n) {
    nimcp_lif_params_t p;
    memset(&p, 0, sizeof(p));
    p.tau_mem = 20.0f; p.tau_syn = 5.0f;
    p.v_thresh = -50.0f; p.v_reset = -65.0f; p.v_rest = -65.0f;
    p.dt = DT; p.hard_reset = true;
    return nimcp_lif_state_create(ctx, n, &p);
}

static void test_create_guards(nimcp_gpu_context_t* ctx) {
    nimcp_lif_state_t* lif = make_lif(ctx, N);
    CHECK(lif != NULL, "lif create");
    if (!lif) return;
    const uint32_t short_off[N_POPS + 1] = { 0, 7, 7, 12, 20 };
    const uint32_t bad_order[N_POPS + 1] = { 0, 9, 7, 12, 21 };
    const uint32_t bad_start[N_POPS + 1] = { 1, 7, 7, 12, 21 };
    CHECK(nimcp_snn_homeo_gpu_create(ctx, lif, short_off, N_POPS) == NULL, "sum != n rejected");
    CHECK(nimcp_snn_homeo_gpu_create(ctx, lif, bad_order, N_POPS) == NULL, "decreasing offsets rejected");
    CHECK(nimcp_snn_homeo_gpu_create(ctx, lif, bad_start, N_POPS) == NULL, "nonzero start rejected");
    CHECK(nimcp_snn_homeo_gpu_create(ctx, lif, OFFS, 0) == NULL, "zero pops rejected");
    CHECK(nimcp_snn_homeo_gpu_create(ctx, NULL, OFFS, N_POPS) == NULL, "NULL lif rejected");

    nimcp_snn_homeo_gpu_t* h = nimcp_snn_homeo_gpu_create(ctx, lif, OFFS, N_POPS);
    CHECK(h != NULL, "valid create");
    CHECK(lif->thresh_offset != NULL, "create allocates the LIF threshold offset");
    CHECK(nimcp_snn_homeo_gpu_matches(h, lif, N, N_POPS), "matches its own lif");
    CHECK(!nimcp_snn_homeo_gpu_matches(h, lif, N, N_POPS + 1), "pop count change detected");
    CHECK(!nimcp_snn_homeo_gpu_matches(h, lif, N + 1, N_POPS), "neuron count change detected");
    CHECK(!nimcp_snn_homeo_gpu_matches(NULL, lif, N, N_POPS), "NULL handle never matches");
    CHECK(!nimcp_snn_homeo_gpu_pre(h, NULL), "pre without params/input rejected");
    CHECK(!nimcp_snn_homeo_gpu_post(h, NULL), "post without params rejected");
    CHECK(!nimcp_snn_homeo_gpu_upload_pop(h, N_POPS, NULL, NULL, NULL, NULL, NULL), "bad pop index rejected");

    /* Fresh state is zero; NULL arrays are skipped on download. */
    float z[7];
    for (int i = 0; i < 7; i++) z[i] = 9.0f;
    CHECK(nimcp_snn_homeo_gpu_download_pop(h, 0, NULL, NULL, z, NULL, NULL), "download rate only");
    for (int i = 0; i < 7; i++) CHECK(z[i] == 0.0f, "fresh rate_ema[%d] = %f", i, z[i]);
    nimcp_snn_homeo_gpu_destroy(h);
    nimcp_lif_state_destroy(lif);
}

static void test_matches_host_reference(nimcp_gpu_context_t* ctx) {
    nimcp_lif_state_t* lif = make_lif(ctx, N);
    CHECK(lif != NULL, "lif create");
    if (!lif) return;
    nimcp_snn_homeo_gpu_t* h = nimcp_snn_homeo_gpu_create(ctx, lif, OFFS, N_POPS);
    CHECK(h != NULL, "homeo create");
    if (!h) { nimcp_lif_state_destroy(lif); return; }

    /* Host reference state */
    snn_adaptation_state_t* ahp0  = snn_adaptation_create(7, 150.0f, 2.0f, 1.0f);
    snn_adaptation_state_t* pump0 = snn_adaptation_create(7, 5000.0f, 0.5f, 1.0f);
    float rate[N], thr[N], dep[N];
    uint32_t seed = 12345u;
    for (uint32_t i = 0; i < N; i++) {
        rate[i] = frand(&seed, 0.0f, 0.1f);
        thr[i]  = frand(&seed, -2.0f, 2.0f);
        dep[i]  = frand(&seed, 0.0f, 0.5f);
    }
    for (uint32_t i = 0; i < 7; i++) {
        ahp0->adapt_var[i]  = frand(&seed, 0.0f, 3.0f);
        pump0->adapt_var[i] = frand(&seed, 0.0f, 3.0f);
    }
    const float thr_inactive0[5] = { thr[7], thr[8], thr[9], thr[10], thr[11] };

    CHECK(nimcp_snn_homeo_gpu_upload_pop(h, 0, ahp0->adapt_var, pump0->adapt_var,
                                         rate, dep, thr), "upload pop 0");
    CHECK(nimcp_snn_homeo_gpu_upload_pop(h, 1, NULL, NULL, NULL, NULL, NULL), "upload empty pop");
    CHECK(nimcp_snn_homeo_gpu_upload_pop(h, 2, NULL, NULL, rate + 7, dep + 7, thr + 7), "upload pop 2");
    CHECK(nimcp_snn_homeo_gpu_upload_pop(h, 3, NULL, NULL, rate + 12, dep + 12, thr + 12), "upload pop 3");

    nimcp_snn_homeo_pop_params_t pp[N_POPS];
    memset(pp, 0, sizeof(pp));
    pp[0].flags = NIMCP_SNN_HOMEO_ACTIVE | NIMCP_SNN_HOMEO_AHP | NIMCP_SNN_HOMEO_PUMP | NIMCP_SNN_HOMEO_IP;
    pp[0].ahp_gain = ahp0->gain_mv;   pp[0].ahp_decay = expf(-DT / ahp0->tau_ms);   pp[0].ahp_bump = 1.0f;
    pp[0].pump_gain = pump0->gain_mv; pp[0].pump_decay = expf(-DT / pump0->tau_ms); pp[0].pump_bump = 1.0f;
    pp[0].basket = -0.7f;
    pp[2].flags = 0;                    /* inactive: must be left alone */
    pp[2].basket = -99.0f;
    pp[3].flags = NIMCP_SNN_HOMEO_ACTIVE | NIMCP_SNN_HOMEO_IP;
    pp[3].basket = -0.3f;
    nimcp_snn_homeo_globals_t g;
    g.pump_factor = 0.8f;
    g.ip_alpha = 0.01f; g.ip_gain = 0.5f; g.ip_target = 0.03f; g.ip_cap = 10.0f;
    g.dep_decay = expf(-DT / 50.0f); g.dep_inc = 0.2f; g.dep_cap = 0.9f;
    CHECK(nimcp_snn_homeo_gpu_set_params(h, pp, &g), "set params");

    size_t dims[1] = { N };
    int total_spikes = 0;
    for (int step = 0; step < STEPS; step++) {
        float base[N], got_in[N], spikes[N];
        for (uint32_t i = 0; i < N; i++) {
            base[i] = (frand(&seed, 0.0f, 1.0f) < 0.4f) ? 1000.0f : -1000.0f;
        }
        nimcp_gpu_tensor_t* in = nimcp_gpu_tensor_from_host(ctx, base, dims, 1,
                                                            NIMCP_GPU_PRECISION_FP32);
        CHECK(in != NULL, "input tensor");
        if (!in) break;

        /* Pre: input += basket - (ahp + pump hyperpolarization) × pump_factor */
        CHECK(nimcp_snn_homeo_gpu_pre(h, in), "pre step %d", step);
        CHECK(nimcp_gpu_tensor_to_host(in, got_in), "read input");
        for (uint32_t i = 0; i < N; i++) {
            float want = base[i];
            if (i < 7) want += -0.7f - (ahp0->gain_mv * ahp0->adapt_var[i]
                                       + pump0->gain_mv * pump0->adapt_var[i]) * g.pump_factor;
            else if (i >= 12) want += -0.3f;
            CHECK(close_enough(got_in[i], want), "step %d input[%u] = %f, want %f",
                  step, i, got_in[i], want);
        }

        CHECK(nimcp_gpu_lif_forward(ctx, lif, in), "lif forward");
        CHECK(nimcp_gpu_tensor_to_host(lif->spikes, spikes), "read spikes");
        nimcp_gpu_tensor_destroy(in);

        uint32_t counts[N_POPS] = { 99, 99, 99, 99 };
        CHECK(nimcp_snn_homeo_gpu_post(h, counts), "post step %d", step);

        /* Reference update */
        uint32_t want_counts[N_POPS] = { 0, 0, 0, 0 };
        for (uint32_t p = 0; p < N_POPS; p++) {
            for (uint32_t i = OFFS[p]; i < OFFS[p + 1]; i++) {
                if (spikes[i] > 0.5f) want_counts[p]++;
            }
        }
        total_spikes += (int)(want_counts[0] + want_counts[2] + want_counts[3]);
        for (uint32_t p = 0; p < N_POPS; p++) {
            CHECK(counts[p] == want_counts[p], "step %d count[%u] = %u, want %u",
                  step, p, counts[p], want_counts[p]);
        }
        snn_adaptation_update(ahp0, spikes, DT);
        snn_adaptation_update(pump0, spikes, DT);
        for (uint32_t i = 0; i < N; i++) {
            if (i >= 7 && i < 12) continue;   /* inactive pop */
            const float fired = spikes[i] > 0.5f ? 1.0f : 0.0f;
            rate[i] = (1.0f - g.ip_alpha) * rate[i] + g.ip_alpha * fired;
            thr[i] += g.ip_gain * (rate[i] - g.ip_target);
            if (thr[i] > g.ip_cap) thr[i] = g.ip_cap;
            if (thr[i] < -g.ip_cap) thr[i] = -g.ip_cap;
            dep[i] *= g.dep_decay;
            if (fired > 0.5f) {
                dep[i] += g.dep_inc;
                if (dep[i] > g.dep_cap) dep[i] = g.dep_cap;
            }
        }

        float d_ahp[7], d_pump[7], d_rate[N], d_dep[N], d_thr[N];
        CHECK(nimcp_snn_homeo_gpu_download_pop(h, 0, d_ahp, d_pump, d_rate, d_dep, d_thr), "dl 0");
        CHECK(nimcp_snn_homeo_gpu_download_pop(h, 2, NULL, NULL, d_rate + 7, d_dep + 7, d_thr + 7), "dl 2");
        CHECK(nimcp_snn_homeo_gpu_download_pop(h, 3, NULL, NULL, d_rate + 12, d_dep + 12, d_thr + 12), "dl 3");
        for (uint32_t i = 0; i < 7; i++) {
            CHECK(close_enough(d_ahp[i], ahp0->adapt_var[i]), "step %d ahp[%u] %f vs %f",
                  step, i, d_ahp[i], ahp0->adapt_var[i]);
            CHECK(close_enough(d_pump[i], pump0->adapt_var[i]), "step %d pump[%u] %f vs %f",
                  step, i, d_pump[i], pump0->adapt_var[i]);
        }
        for (uint32_t i = 0; i < N; i++) {
            CHECK(close_enough(d_rate[i], rate[i]), "step %d rate[%u] %f vs %f", step, i, d_rate[i], rate[i]);
            CHECK(close_enough(d_dep[i], dep[i]), "step %d dep[%u] %f vs %f", step, i, d_dep[i], dep[i]);
            CHECK(close_enough(d_thr[i], thr[i]), "step %d thr[%u] %f vs %f", step, i, d_thr[i], thr[i]);
        }
        for (uint32_t k = 0; k < 5; k++) {
            CHECK(d_thr[7 + k] == thr_inactive0[k], "inactive pop threshold untouched");
        }
    }
    CHECK(total_spikes > 0 && total_spikes < STEPS * 16, "spikes were mixed (%d)", total_spikes);

    snn_adaptation_destroy(ahp0);
    snn_adaptation_destroy(pump0);
    nimcp_snn_homeo_gpu_destroy(h);
    nimcp_lif_state_destroy(lif);
}

int main(void) {
    if (!getenv("NIMCP_TEST_ALLOW_GPU")) setenv("CUDA_VISIBLE_DEVICES", "", 1);
    nimcp_init();
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create_auto();
    printf("test_snn_homeo_gpu: %s\n", ctx ? "GPU context" : "no GPU context (CPU emulation)");

    test_pop_of();
    test_create_guards(ctx);
    test_matches_host_reference(ctx);

    if (ctx) nimcp_gpu_context_destroy(ctx);
    if (g_failures) { fprintf(stderr, "test_snn_homeo_gpu: %d failure(s)\n", g_failures); return 1; }
    printf("test_snn_homeo_gpu: all passed\n");
    return 0;
}
