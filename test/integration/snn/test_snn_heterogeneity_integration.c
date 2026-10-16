/**
 * @file test_snn_heterogeneity_integration.c
 * @brief Wave G — Integration test for per-neuron LIF heterogeneity.
 * @date 2026-04-27
 *
 * WHAT: Drive identical sustained external currents into two 64-neuron
 *       lightweight pops — one homogeneous (σ=0), one heterogeneous
 *       (σ=0.2). Run for several steps and verify the heterogeneous
 *       pop's spike-time variance (jitter across neurons) is HIGHER
 *       than the homogeneous control.
 * WHY:  Lock-step firing is a degenerate failure mode of homogeneous
 *       pops. The structural fix (per-neuron τ_mem + v_thresh draws)
 *       must produce desynchronisation in the spike pattern — not just
 *       allocate buffers. Quantify by measuring inter-neuron spike-time
 *       variance under matched drive.
 * HOW:  libcheck. Build two identically-configured nets, populate one
 *       with σ=0.2 heterogeneity, drive both with the same current per
 *       step for 50 steps, and compare per-step spike-count variance
 *       across neurons (a proxy for desynchronisation).
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "nimcp.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "utils/tensor/nimcp_tensor.h"

/* SNN tunables (extern decls — no header). */
extern void  snn_tune_set_conductance_enabled(float);
extern void  snn_tune_set_cb_weights_rescaled(float);
extern void  snn_tune_set_noise_rate_hz(float);
extern void  snn_tune_set_basket_enabled(float);
extern void  snn_tune_set_ahp_enabled(float);
extern void  snn_tune_set_pump_enabled(float);
extern void  snn_tune_set_substrate_enabled(float);

static snn_network_t* g_net_homo = NULL;
static snn_network_t* g_net_het  = NULL;

static void reset_tunables(void) {
    /* Quiet baseline — we want only LIF dynamics + drive, no noise / AHP /
     * pump / basket / substrate.
     *
     * CB ON — forces the CPU fallback path. The GPU LIF kernel does not
     * yet read per-neuron τ_mem / v_thresh arrays (out-of-scope per Wave
     * G constraints). Engaging CB mode is the canonical way to take the
     * CPU branch where Wave G's per-neuron resolution lives. The mean-
     * pull-toward-mean / gap-junction integration test uses the same
     * trick. */
    snn_tune_set_noise_rate_hz(0.0f);
    snn_tune_set_basket_enabled(0.0f);
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);
    snn_tune_set_substrate_enabled(0.0f);
    snn_tune_set_conductance_enabled(1.0f);
    snn_tune_set_cb_weights_rescaled(1.0f);
}

static snn_network_t* fresh_net(void) {
    snn_config_t cfg;
    snn_config_default(&cfg);
    cfg.n_inputs  = 1;
    cfg.n_outputs = 1;
    cfg.n_hidden  = 0;
    cfg.dt        = 1.0f;
    return snn_network_create(&cfg);
}

static void setup(void) {
    reset_tunables();
    g_net_homo = fresh_net();
    g_net_het  = fresh_net();
    ck_assert_ptr_nonnull(g_net_homo);
    ck_assert_ptr_nonnull(g_net_het);
}

static void teardown(void) {
    if (g_net_homo) { snn_network_destroy(g_net_homo); g_net_homo = NULL; }
    if (g_net_het)  { snn_network_destroy(g_net_het);  g_net_het  = NULL; }
}

/* Drive a 64-neuron pop with `drive` mV per step for n_steps and return
 * the per-neuron spike counts in `out` (pre-allocated [64]). */
static void run_pop_with_drive(snn_network_t* net,
                               int pop_id,
                               float drive,
                               int n_steps,
                               uint32_t* out_spike_counts,
                               float* out_first_spike_step)
{
    snn_population_t* pop = snn_network_get_population(net, (uint32_t)pop_id);
    ck_assert_ptr_nonnull(pop);
    ck_assert_ptr_nonnull(pop->external_current);

    float* v_data = (float*)nimcp_tensor_data(pop->membrane_v);
    float* ref    = (float*)nimcp_tensor_data(pop->refractory);
    float* spk    = (float*)nimcp_tensor_data(pop->spike_output);

    /* Initialize: rest, no refractory, no past spikes. */
    snn_lif_params_t base = snn_pop_lif_params(pop, &net->config);
    for (uint32_t i = 0; i < pop->n_neurons; i++) {
        v_data[i] = base.v_rest;
        ref[i]    = 0.0f;
        spk[i]    = 0.0f;
        out_spike_counts[i] = 0;
        out_first_spike_step[i] = -1.0f;
    }

    for (int s = 0; s < n_steps; s++) {
        for (uint32_t i = 0; i < pop->n_neurons; i++) {
            pop->external_current[i] = drive;
        }
        int sr = snn_network_step(net, 1.0f);
        ck_assert_int_ge(sr, 0);

        for (uint32_t i = 0; i < pop->n_neurons; i++) {
            if (spk[i] > 0.5f) {
                out_spike_counts[i] += 1;
                if (out_first_spike_step[i] < 0.0f) {
                    out_first_spike_step[i] = (float)s;
                }
            }
        }
    }
}

/* Sample variance helper. */
static double sample_variance_uint(const uint32_t* arr, uint32_t n) {
    if (n < 2) return 0.0;
    double mean = 0.0;
    for (uint32_t i = 0; i < n; i++) mean += (double)arr[i];
    mean /= (double)n;
    double sumsq = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double d = (double)arr[i] - mean;
        sumsq += d * d;
    }
    return sumsq / (double)(n - 1);
}

/* Sample variance helper for first-spike steps; ignores -1 (never spiked).
 * Returns the variance + how many neurons spiked. */
static double sample_variance_first_spike(const float* arr, uint32_t n,
                                          uint32_t* out_n_spiked) {
    uint32_t k = 0;
    double mean = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        if (arr[i] >= 0.0f) { mean += (double)arr[i]; k++; }
    }
    *out_n_spiked = k;
    if (k < 2) return 0.0;
    mean /= (double)k;
    double sumsq = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        if (arr[i] >= 0.0f) {
            double d = (double)arr[i] - mean;
            sumsq += d * d;
        }
    }
    return sumsq / (double)(k - 1);
}

/*
 * The integration test:
 *   - 64 neurons in each pop, identical config + drive.
 *   - Homogeneous: σ = 0, all neurons identical → fire on the SAME step.
 *   - Heterogeneous: σ = 0.2, τ_mem and v_thresh vary per-neuron → fire
 *     across a window of steps.
 *
 * Assertion: the variance of FIRST-SPIKE step number across the heterogeneous
 * pop must be measurably HIGHER than the homogeneous pop's variance. The
 * homogeneous pop's first-spike variance is exactly 0 (every neuron fires
 * the same step). Heterogeneous should be > 0.
 */
START_TEST(test_heterogeneity_increases_spike_jitter)
{
    /* Build two parallel 64-neuron pops. */
    int pop_homo = snn_network_add_population_lightweight(
        g_net_homo, 64, NEURON_GENERIC_LIF, "homo");
    int pop_het = snn_network_add_population_lightweight(
        g_net_het, 64, NEURON_GENERIC_LIF, "het");
    ck_assert_int_ge(pop_homo, 0);
    ck_assert_int_ge(pop_het, 0);
    snn_network_finalize_connections(g_net_homo);
    snn_network_finalize_connections(g_net_het);

    /* Engage heterogeneity ONLY on g_net_het. Homogeneous control keeps
     * the default σ=0. */
    int rc = snn_network_set_pop_heterogeneity(g_net_het, (uint32_t)pop_het, 0.2f);
    ck_assert_int_eq(rc, 0);

    /* Drive: in CB mode this is interpreted as an AMPA conductance bump
     * each step. Driving force ≈ (E_ampa - V) ≈ 65 mV at rest, so a
     * conductance of ~0.5/step injects ~32 mV/τ into dv — enough to push
     * neurons across threshold within a few steps. Tuned to fire the
     * pop-wide neuron consistently around step 4-5 so heterogeneity has
     * room to spread firing across a multi-step window.
     *
     * Note: external_current is cleared at end of each step (lightweight
     * path), so we re-set every step inside run_pop_with_drive. */
    const float drive = 0.5f;
    const int n_steps = 50;

    uint32_t cnt_homo[64], cnt_het[64];
    float    fs_homo[64],  fs_het[64];

    run_pop_with_drive(g_net_homo, pop_homo, drive, n_steps, cnt_homo, fs_homo);
    run_pop_with_drive(g_net_het,  pop_het,  drive, n_steps, cnt_het,  fs_het);

    /* Pre-condition: at least some neurons in BOTH pops must have spiked
     * — otherwise the variance measurement is meaningless. */
    uint32_t spiked_homo = 0, spiked_het = 0;
    for (uint32_t i = 0; i < 64; i++) {
        if (fs_homo[i] >= 0.0f) spiked_homo++;
        if (fs_het[i]  >= 0.0f) spiked_het++;
    }
    ck_assert_msg(spiked_homo >= 32,
                  "Pre-condition failed: only %u/64 homogeneous neurons spiked; "
                  "raise drive or lengthen run", spiked_homo);
    ck_assert_msg(spiked_het >= 32,
                  "Pre-condition failed: only %u/64 heterogeneous neurons spiked; "
                  "raise drive or lengthen run", spiked_het);

    uint32_t k_homo = 0, k_het = 0;
    double var_fs_homo = sample_variance_first_spike(fs_homo, 64, &k_homo);
    double var_fs_het  = sample_variance_first_spike(fs_het,  64, &k_het);

    /* Core assertion: heterogeneous pop's first-spike-step variance is
     * strictly greater than the homogeneous pop's. The homogeneous case
     * should be near-zero (every neuron identical, no noise → all fire
     * the same step). The heterogeneous case should be measurably above. */
    ck_assert_msg(var_fs_het > var_fs_homo + 0.1,
                  "Heterogeneity FAILED to increase spike-time jitter: "
                  "var_fs_het=%.6f var_fs_homo=%.6f (k_het=%u k_homo=%u). "
                  "If the homogeneous variance is 0 and heterogeneous is also "
                  "0, the per-neuron arrays are not being read by the LIF loop.",
                  var_fs_het, var_fs_homo, k_het, k_homo);

    /* Also assert per-neuron spike-count variance is non-zero on the
     * heterogeneous side — different neurons accumulate different total
     * spike counts because their τ_mem and threshold differ. */
    double var_cnt_homo = sample_variance_uint(cnt_homo, 64);
    double var_cnt_het  = sample_variance_uint(cnt_het,  64);
    ck_assert_msg(var_cnt_het > var_cnt_homo,
                  "Heterogeneous total spike count variance (%.4f) "
                  "should exceed homogeneous (%.4f) — heterogeneity may not "
                  "be reaching the LIF inner loop.",
                  var_cnt_het, var_cnt_homo);
}
END_TEST

/* Companion negative control: σ=0 on both nets ⇒ identical spike trains. */
START_TEST(test_zero_sigma_matches_homogeneous)
{
    int pop_a = snn_network_add_population_lightweight(
        g_net_homo, 64, NEURON_GENERIC_LIF, "a");
    int pop_b = snn_network_add_population_lightweight(
        g_net_het,  64, NEURON_GENERIC_LIF, "b");
    ck_assert_int_ge(pop_a, 0);
    ck_assert_int_ge(pop_b, 0);
    snn_network_finalize_connections(g_net_homo);
    snn_network_finalize_connections(g_net_het);

    /* Both default σ=0 — neither has heterogeneity. */
    snn_population_t* pa = snn_network_get_population(g_net_homo, (uint32_t)pop_a);
    snn_population_t* pb = snn_network_get_population(g_net_het,  (uint32_t)pop_b);
    ck_assert_float_eq(pa->heterogeneity_sigma, 0.0f);
    ck_assert_float_eq(pb->heterogeneity_sigma, 0.0f);

    const float drive = 0.5f;
    const int n_steps = 50;
    uint32_t cnt_a[64], cnt_b[64];
    float    fs_a[64],  fs_b[64];

    run_pop_with_drive(g_net_homo, pop_a, drive, n_steps, cnt_a, fs_a);
    run_pop_with_drive(g_net_het,  pop_b, drive, n_steps, cnt_b, fs_b);

    /* Both pops are σ=0; output shape must be identical to within the
     * scalar per-pop randomness inherent in the LIF kernel. With no noise
     * source and identical config, this should be EXACTLY identical. */
    for (uint32_t i = 0; i < 64; i++) {
        ck_assert_msg(cnt_a[i] == cnt_b[i],
                      "σ=0 case: per-neuron spike count differs (n=%u: "
                      "homo=%u het=%u) — implies hidden state leak.",
                      i, cnt_a[i], cnt_b[i]);
    }
}
END_TEST

static Suite* heterogeneity_suite(void) {
    Suite* s = suite_create("SNN Per-Neuron Heterogeneity (Wave G)");

    TCase* tc_jitter = tcase_create("Heterogeneity desynchronises spikes");
    tcase_add_checked_fixture(tc_jitter, setup, teardown);
    tcase_add_test(tc_jitter, test_heterogeneity_increases_spike_jitter);
    tcase_set_timeout(tc_jitter, 60);
    suite_add_tcase(s, tc_jitter);

    TCase* tc_zero = tcase_create("Sigma=0 matches homogeneous control");
    tcase_add_checked_fixture(tc_zero, setup, teardown);
    tcase_add_test(tc_zero, test_zero_sigma_matches_homogeneous);
    tcase_set_timeout(tc_zero, 60);
    suite_add_tcase(s, tc_zero);

    return s;
}

int main(void) {
    if (nimcp_init() != NIMCP_SUCCESS) {
        fprintf(stderr, "nimcp_init failed\n");
        return EXIT_FAILURE;
    }
    Suite* s = heterogeneity_suite();
    SRunner* sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    nimcp_shutdown();
    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
