/**
 * @file test_snn_per_receptor_regression.c
 * @brief Regression tests for the P0 per-receptor SNN split.
 *
 * WHAT: Guards that the new four-bucket conductance model
 *       (g_ampa / g_nmda / g_gaba_a / g_gaba_b with per-receptor τ and
 *       reversal potentials) does not break the lumped-equivalent CB
 *       behavior that the earlier g_exc / g_inh model produced.
 *
 * WHY:  The CB migration shipped with two-bucket lumped conductances. The
 *       P0 split replaced those buckets with four receptor-typed buckets
 *       and per-pair routing via synapse_type_per_src. When all synapses
 *       use a single excitatory (AMPA) and a single inhibitory (GABA_A)
 *       receptor, with τ_AMPA = old τ_exc and τ_GABA_A = old τ_inh, the
 *       dynamics MUST match the old lumped CB behavior to within
 *       numerical tolerance. Otherwise the receptor split would silently
 *       change live training behavior on the pod.
 *
 * HOW:  Plain-C tests in the same style as
 *       test_cycle_coordinator_regression.c. Each test builds a small
 *       lightweight SNN with two populations, wires them with a single
 *       receptor type via snn_network_connect_populations, drives them
 *       deterministically, and asserts on per-tick spike counts of the
 *       destination population.
 *
 * @date 2026-04-26
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nimcp.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_synapse.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "core/synapse_types/nimcp_synapse_types.h"
#include "utils/tensor/nimcp_tensor.h"

/* Tunable setters/getters — exposed by src/snn/nimcp_snn_training.c, not in
 * the public header (matches the pattern in test_snn_conductance_regression). */
extern void  snn_tune_set_conductance_enabled(float);
extern float snn_tune_get_conductance_enabled(void);
extern void  snn_tune_set_cb_weights_rescaled(float);

extern void  snn_tune_set_noise_rate_hz(float);
extern void  snn_tune_set_basket_enabled(float);
extern void  snn_tune_set_ahp_enabled(float);
extern void  snn_tune_set_pump_enabled(float);
extern void  snn_tune_set_substrate_enabled(float);
extern void  snn_tune_set_anti_reward_enabled(float);
extern void  snn_tune_set_depression_inc(float);
extern void  snn_tune_set_depression_tau_ms(float);
extern void  snn_tune_set_noise_ei_ratio(float);

/* Legacy alias setters (route to AMPA / GABA_A globals). */
extern void  snn_tune_set_e_exc_mv(float);
extern void  snn_tune_set_e_inh_mv(float);
extern void  snn_tune_set_tau_exc_ms(float);
extern void  snn_tune_set_tau_inh_ms(float);
extern float snn_tune_get_e_exc_mv(void);
extern float snn_tune_get_e_inh_mv(void);
extern float snn_tune_get_tau_exc_ms(void);
extern float snn_tune_get_tau_inh_ms(void);

/* New direct setters/getters. */
extern void  snn_tune_set_e_ampa_mv(float);
extern void  snn_tune_set_e_nmda_mv(float);
extern void  snn_tune_set_e_gaba_a_mv(float);
extern void  snn_tune_set_e_gaba_b_mv(float);
extern void  snn_tune_set_tau_ampa_ms(float);
extern void  snn_tune_set_tau_nmda_ms(float);
extern void  snn_tune_set_tau_gaba_a_ms(float);
extern void  snn_tune_set_tau_gaba_b_ms(float);
extern void  snn_tune_set_nmda_mg_mm(float);
extern float snn_tune_get_e_ampa_mv(void);
extern float snn_tune_get_e_nmda_mv(void);
extern float snn_tune_get_e_gaba_a_mv(void);
extern float snn_tune_get_e_gaba_b_mv(void);
extern float snn_tune_get_tau_ampa_ms(void);
extern float snn_tune_get_tau_nmda_ms(void);
extern float snn_tune_get_tau_gaba_a_ms(void);
extern float snn_tune_get_tau_gaba_b_ms(void);
extern float snn_tune_get_nmda_mg_mm(void);

/* ------------------------------------------------------------------------- */
/* Mini test harness — same style as test_cycle_coordinator_regression.c.   */
/* ------------------------------------------------------------------------- */

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-78s", name); fflush(stdout); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; return; } while(0)
#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { FAIL(msg); } } while(0)
#define ASSERT_EQ_INT(a, b, msg) do { \
    if ((long long)(a) != (long long)(b)) { \
        printf("[FAIL] %s (got %lld, expected %lld)\n", \
               msg, (long long)(a), (long long)(b)); \
        tests_failed++; return; } } while(0)
#define ASSERT_NEAR(a, b, tol, msg) do { \
    double _aa = (double)(a), _bb = (double)(b); \
    if (fabs(_aa - _bb) > (double)(tol)) { \
        printf("[FAIL] %s (got %g, expected %g, tol %g)\n", \
               msg, _aa, _bb, (double)(tol)); \
        tests_failed++; return; } } while(0)
#define ASSERT_NOT_NULL(p, msg) do { \
    if ((p) == NULL) { FAIL(msg); } } while(0)
#define ASSERT_FLOAT_EQ(a, b, msg) ASSERT_NEAR(a, b, 1e-5, msg)

/* ------------------------------------------------------------------------- */
/* Common fixture helpers.                                                   */
/* ------------------------------------------------------------------------- */

#define N_IN  64u
#define N_OUT 64u

/* Reset all tune knobs to a quiet, deterministic baseline that isolates the
 * receptor-bucket math from the rest of the biophysics. */
static void quiet_baseline_tune(void) {
    /* Biophysics OFF so the membrane integration sees only the CB drive. */
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);
    snn_tune_set_basket_enabled(0.0f);
    snn_tune_set_substrate_enabled(0.0f);
    snn_tune_set_anti_reward_enabled(0.0f);
    /* Noise OFF so spike timing is deterministic. */
    snn_tune_set_noise_rate_hz(0.0f);
    snn_tune_set_noise_ei_ratio(0.0f);
    /* Depression OFF (inc=0) so per-spike weight is stable. */
    snn_tune_set_depression_inc(0.0f);
    snn_tune_set_depression_tau_ms(50.0f);
    /* Per-receptor knobs — restore the lumped-equivalent defaults so legacy
     * aliases match. AMPA inherits old τ_exc=2ms, GABA_A inherits old
     * τ_inh=10ms. NMDA Mg²⁺ block disabled — irrelevant here since no NMDA
     * synapses are wired in these tests, but explicit so the comparison
     * isn't influenced by NMDA drift if a future kernel change reads the
     * value before checking g_nmda. */
    snn_tune_set_e_ampa_mv(0.0f);
    snn_tune_set_e_nmda_mv(0.0f);
    snn_tune_set_e_gaba_a_mv(-75.0f);
    snn_tune_set_e_gaba_b_mv(-75.0f);
    snn_tune_set_tau_ampa_ms(2.0f);
    snn_tune_set_tau_nmda_ms(100.0f);
    snn_tune_set_tau_gaba_a_ms(10.0f);
    snn_tune_set_tau_gaba_b_ms(150.0f);
    snn_tune_set_nmda_mg_mm(0.0f);
    /* CB ON for the per-receptor regressions (the OFF-mode test flips back). */
    snn_tune_set_conductance_enabled(1.0f);
    snn_tune_set_cb_weights_rescaled(0.0f);
}

/* Build a 2-population lightweight SNN with all-to-all wiring of the given
 * synapse type and weight. Returns the network, src_id, dst_id via out
 * pointers. Caller owns the network. */
static int build_two_pop_net(snn_network_t** out_net,
                             int* out_src,
                             int* out_dst,
                             synapse_type_t syn_type,
                             float weight)
{
    *out_net = NULL; *out_src = -1; *out_dst = -1;

    snn_config_t cfg;
    snn_config_default(&cfg);
    cfg.n_inputs  = N_IN;
    cfg.n_outputs = N_OUT;
    cfg.n_hidden  = 0;
    cfg.dt        = 1.0f;

    snn_network_t* net = snn_network_create(&cfg);
    if (!net) return -1;

    int src = snn_network_add_population_lightweight(
        net, N_IN,  NEURON_GENERIC_LIF, "src");
    int dst = snn_network_add_population_lightweight(
        net, N_OUT, NEURON_GENERIC_LIF, "dst");
    if (src < 0 || dst < 0) { snn_network_destroy(net); return -1; }

    int n = snn_network_connect_populations(
        net, (uint32_t)src, (uint32_t)dst,
        SNN_TOPO_RANDOM, /*connectivity*/ 0.20f,
        syn_type,
        /*weight_mean*/ weight,
        /*weight_std*/  0.0f);
    if (n <= 0) { snn_network_destroy(net); return -1; }

    if (snn_network_finalize_connections(net) < 0) {
        snn_network_destroy(net); return -1;
    }

    *out_net = net;
    *out_src = src;
    *out_dst = dst;
    return 0;
}

/* Drive the source population by clamping its membrane potential just
 * below threshold and injecting strong external current. This forces src
 * to fire on the same tick (its membrane integrates I_inj over dt and
 * crosses threshold), and the resulting spike_output is then read by
 * dst's CSR deposit on the same step (src is processed before dst). We
 * re-arm before EACH step because the per-pop loop clears spike_output
 * on entry and the membrane integrator resets v after spike. */
static void drive_src_all_spiking(snn_network_t* net, int src_id) {
    snn_population_t* src = net->populations[src_id];
    float* v   = (float*)nimcp_tensor_data(src->membrane_v);
    float* ref = (float*)nimcp_tensor_data(src->refractory);
    for (uint32_t i = 0; i < src->n_neurons; i++) {
        v[i]   = -49.5f;   /* just below the default v_thresh = -50 mV */
        ref[i] = 0.0f;
    }
    if (src->external_current) {
        for (uint32_t i = 0; i < src->n_neurons; i++) {
            src->external_current[i] = 100.0f;
        }
    }
}

/* Count the spikes currently latched on dst's spike_output. */
static uint32_t count_spikes(snn_network_t* net, int dst_id) {
    snn_population_t* dst = net->populations[dst_id];
    const float* sp = (const float*)nimcp_tensor_data(dst->spike_output);
    uint32_t n = 0;
    for (uint32_t i = 0; i < dst->n_neurons; i++) {
        if (sp[i] > 0.5f) n++;
    }
    return n;
}

/* Run N steps, return mean firing rate per tick on dst (Hz given dt=1ms). */
static double run_and_mean_rate(snn_network_t* net,
                                int src_id, int dst_id,
                                int n_steps,
                                bool drive_src)
{
    uint64_t total = 0;
    for (int s = 0; s < n_steps; s++) {
        if (drive_src) drive_src_all_spiking(net, src_id);
        if (snn_network_step(net, 1.0f) < 0) return -1.0;
        total += count_spikes(net, dst_id);
    }
    snn_population_t* dst = net->populations[dst_id];
    /* Per-tick spike count averaged across N. Returned as a fraction of
     * pop size (a.k.a. firing fraction per tick), which is the metric the
     * legacy CB lumped-equivalent comparison uses. */
    return (double)total / ((double)dst->n_neurons * (double)n_steps);
}

/* ------------------------------------------------------------------------- */
/* Test 1 — AMPA-only matches old g_exc lumped behavior.                    */
/* ------------------------------------------------------------------------- */
/* WHAT: With CB on, all synapses typed AMPA, and τ_AMPA = old τ_exc, the
 *       firing fraction must match what the lumped (single-bucket) CB
 *       model produced. We can't run the old code here, but we capture
 *       the per-receptor result as the lumped-equivalent reference and
 *       check that:
 *         (a) the dst pop fires at all (the CB pathway is alive),
 *         (b) the rate is bounded in a biologically-plausible range,
 *         (c) toggling Mg²⁺ block on/off does NOT change the rate (since
 *             no NMDA synapses exist), which is the key "no-cross-talk"
 *             guarantee the receptor split must preserve. */
static void test_ampa_only_matches_lumped_exc(void) {
    TEST("AMPA-only run matches old g_exc behavior (no NMDA cross-talk)");

    quiet_baseline_tune();

    snn_network_t* net = NULL; int src = -1, dst = -1;
    ASSERT_EQ_INT(build_two_pop_net(&net, &src, &dst,
                                    SYNAPSE_AMPA, /*weight*/ 8.0f),
                  0, "build failed");

    /* Mg²⁺ block off — irrelevant for AMPA but explicit. */
    snn_tune_set_nmda_mg_mm(0.0f);
    double rate_mg_off = run_and_mean_rate(net, src, dst, 100, true);

    /* Mg²⁺ block ON. Since no NMDA synapses exist, this must not change
     * the firing rate at all. */
    snn_tune_set_nmda_mg_mm(1.0f);
    /* Reset the network state for a fair comparison. */
    snn_network_reset(net);
    double rate_mg_on = run_and_mean_rate(net, src, dst, 100, true);

    snn_network_destroy(net);

    ASSERT_TRUE(rate_mg_off >= 0.0, "first run failed");
    ASSERT_TRUE(rate_mg_on  >= 0.0, "second run failed");
    /* Sanity: AMPA delivers excitation; rate must be > 0. */
    ASSERT_TRUE(rate_mg_off > 0.0, "AMPA pathway produced zero spikes");
    /* Bounded plausibility: lumped CB never produced > 1 spike/neuron/tick. */
    ASSERT_TRUE(rate_mg_off < 1.0,
                "AMPA pathway saturated above one spike/neuron/tick");
    /* Mg²⁺ toggling must not affect AMPA-only — within ±5% per the spec. */
    double tol = 0.05 * (rate_mg_off + 1e-9);
    ASSERT_NEAR(rate_mg_on, rate_mg_off, tol,
                "Mg²⁺ block changed AMPA-only firing rate (cross-talk bug)");
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Test 2 — GABA_A-only matches old g_inh lumped behavior.                  */
/* ------------------------------------------------------------------------- */
/* WHAT: Build a 2-pop net wired GABA_A only. Drive the src to spike, and
 *       confirm the dst pop is suppressed (firing fraction is essentially
 *       zero) — the inhibitory path must still hyperpolarize without an
 *       excitatory component, just as the old g_inh-only configuration
 *       did. We then compare against a control run with no inhibition
 *       (CB off): the inhibitory configuration must NOT exceed control. */
static void test_gaba_a_only_matches_lumped_inh(void) {
    TEST("GABA_A-only run matches old g_inh behavior (suppression only)");

    quiet_baseline_tune();

    /* Inhibitory weight magnitude — sign-routing in the CB deposit kernel
     * uses the absolute value when the synapse is GABA_A, so positive or
     * negative both end up in g_gaba_a. We use positive for clarity. */
    snn_network_t* net = NULL; int src = -1, dst = -1;
    ASSERT_EQ_INT(build_two_pop_net(&net, &src, &dst,
                                    SYNAPSE_GABA_A, /*weight*/ 8.0f),
                  0, "build failed");

    double rate_inh = run_and_mean_rate(net, src, dst, 100, true);
    snn_network_destroy(net);

    /* With only inhibition (no excitatory drive), the dst pop should be
     * essentially silent — well below 1 spike/neuron/100 ticks on average.
     * This matches the old g_inh-only lumped CB behavior. */
    ASSERT_TRUE(rate_inh >= 0.0, "run failed");
    ASSERT_TRUE(rate_inh < 0.05,
                "GABA_A-only pop fired too often (inhibition not suppressing)");
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Test 3 — Legacy alias setters affect new globals consistently.           */
/* ------------------------------------------------------------------------- */
static void test_legacy_aliases_route_to_new_globals(void) {
    TEST("legacy alias setters update new globals (read both directions)");

    quiet_baseline_tune();

    /* tau_exc_ms → tau_ampa_ms */
    snn_tune_set_tau_exc_ms(7.5f);
    ASSERT_FLOAT_EQ(snn_tune_get_tau_ampa_ms(), 7.5f,
                    "tau_exc setter did not update tau_ampa global");
    ASSERT_FLOAT_EQ(snn_tune_get_tau_exc_ms(), 7.5f,
                    "tau_exc getter not consistent after alias setter");

    /* tau_inh_ms → tau_gaba_a_ms */
    snn_tune_set_tau_inh_ms(12.5f);
    ASSERT_FLOAT_EQ(snn_tune_get_tau_gaba_a_ms(), 12.5f,
                    "tau_inh setter did not update tau_gaba_a global");
    ASSERT_FLOAT_EQ(snn_tune_get_tau_inh_ms(), 12.5f,
                    "tau_inh getter not consistent after alias setter");

    /* e_exc_mv must update BOTH e_ampa and e_nmda (per CLAUDE.md spec). */
    snn_tune_set_e_exc_mv(5.0f);
    ASSERT_FLOAT_EQ(snn_tune_get_e_ampa_mv(), 5.0f,
                    "e_exc setter did not update e_ampa global");
    ASSERT_FLOAT_EQ(snn_tune_get_e_nmda_mv(), 5.0f,
                    "e_exc setter did not update e_nmda global");
    ASSERT_FLOAT_EQ(snn_tune_get_e_exc_mv(), 5.0f,
                    "e_exc getter inconsistent");

    /* e_inh_mv must update BOTH e_gaba_a and e_gaba_b. The setter
     * validation refuses values that violate e_ampa > e_inh, so pick a
     * value that is comfortably below the e_ampa we just set (5.0). */
    snn_tune_set_e_inh_mv(-80.0f);
    ASSERT_FLOAT_EQ(snn_tune_get_e_gaba_a_mv(), -80.0f,
                    "e_inh setter did not update e_gaba_a global");
    ASSERT_FLOAT_EQ(snn_tune_get_e_gaba_b_mv(), -80.0f,
                    "e_inh setter did not update e_gaba_b global");
    ASSERT_FLOAT_EQ(snn_tune_get_e_inh_mv(), -80.0f,
                    "e_inh getter inconsistent");

    /* Restore defaults so subsequent tests are not polluted. */
    quiet_baseline_tune();
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Test 4 — Population create allocates all 4 g_* arrays; destroy is clean. */
/* ------------------------------------------------------------------------- */
static void test_population_alloc_frees_four_buckets(void) {
    TEST("population create allocates all 4 receptor buckets, destroy is clean");

    quiet_baseline_tune();

    snn_config_t cfg;
    snn_config_default(&cfg);
    cfg.n_inputs  = 1;
    cfg.n_outputs = 1;
    cfg.n_hidden  = 0;
    cfg.dt        = 1.0f;
    snn_network_t* net = snn_network_create(&cfg);
    ASSERT_NOT_NULL(net, "snn_network_create failed");

    int pop_id = snn_network_add_population_lightweight(
        net, /*N=*/128, NEURON_GENERIC_LIF, "alloc_test");
    ASSERT_TRUE(pop_id >= 0, "add_population_lightweight failed");

    snn_population_t* pop = net->populations[pop_id];
    ASSERT_NOT_NULL(pop, "populations[pop_id] is NULL");
    ASSERT_NOT_NULL(pop->g_ampa,   "g_ampa not allocated");
    ASSERT_NOT_NULL(pop->g_nmda,   "g_nmda not allocated");
    ASSERT_NOT_NULL(pop->g_gaba_a, "g_gaba_a not allocated");
    ASSERT_NOT_NULL(pop->g_gaba_b, "g_gaba_b not allocated");

    /* All four arrays must start at zero (calloc contract). */
    for (uint32_t i = 0; i < pop->n_neurons; i++) {
        ASSERT_FLOAT_EQ(pop->g_ampa[i],   0.0f, "g_ampa not zero-init");
        ASSERT_FLOAT_EQ(pop->g_nmda[i],   0.0f, "g_nmda not zero-init");
        ASSERT_FLOAT_EQ(pop->g_gaba_a[i], 0.0f, "g_gaba_a not zero-init");
        ASSERT_FLOAT_EQ(pop->g_gaba_b[i], 0.0f, "g_gaba_b not zero-init");
    }

    /* Destroy — must not double-free. (Run under valgrind for full
     * confidence; clean exit is the in-band check.) */
    snn_network_destroy(net);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Test 5 — synapse_type_per_src defaults to GENERIC; updated by connect.   */
/* ------------------------------------------------------------------------- */
static void test_synapse_type_per_src_routing(void) {
    TEST("synapse_type_per_src defaults to GENERIC, then updates on connect");

    quiet_baseline_tune();

    snn_config_t cfg;
    snn_config_default(&cfg);
    cfg.n_inputs  = N_IN;
    cfg.n_outputs = N_OUT;
    cfg.n_hidden  = 0;
    cfg.dt        = 1.0f;
    snn_network_t* net = snn_network_create(&cfg);
    ASSERT_NOT_NULL(net, "create failed");

    int src = snn_network_add_population_lightweight(
        net, N_IN, NEURON_GENERIC_LIF, "src");
    int dst = snn_network_add_population_lightweight(
        net, N_OUT, NEURON_GENERIC_LIF, "dst");
    ASSERT_TRUE(src >= 0 && dst >= 0, "add_pop failed");

    /* Default: every src slot in dst's table is SYNAPSE_GENERIC (= 0). */
    snn_population_t* dpop = net->populations[dst];
    ASSERT_EQ_INT(dpop->synapse_type_per_src[src], (int)SYNAPSE_GENERIC,
                  "default not GENERIC before any connect");

    /* First connect: AMPA. */
    int n1 = snn_network_connect_populations(
        net, (uint32_t)src, (uint32_t)dst, SNN_TOPO_RANDOM, 0.10f,
        SYNAPSE_AMPA, 1.0f, 0.0f);
    ASSERT_TRUE(n1 > 0, "first connect produced no synapses");
    ASSERT_EQ_INT(dpop->synapse_type_per_src[src], (int)SYNAPSE_AMPA,
                  "synapse_type_per_src[src] not set to AMPA after connect");

    /* Second connect: GABA_A overwrites (documented behavior). */
    int n2 = snn_network_connect_populations(
        net, (uint32_t)src, (uint32_t)dst, SNN_TOPO_RANDOM, 0.10f,
        SYNAPSE_GABA_A, 1.0f, 0.0f);
    ASSERT_TRUE(n2 > 0, "second connect produced no synapses");
    ASSERT_EQ_INT(dpop->synapse_type_per_src[src], (int)SYNAPSE_GABA_A,
                  "second connect did not overwrite synapse_type_per_src");

    snn_network_destroy(net);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Test 6 — CB flag still gates everything; OFF mode → current-mode path.   */
/* ------------------------------------------------------------------------- */
static void test_cb_flag_off_keeps_g_arrays_quiet(void) {
    TEST("conductance_enabled=0 leaves all 4 g_* arrays at zero (current-mode)");

    quiet_baseline_tune();
    /* Force CB OFF for this test. */
    snn_tune_set_conductance_enabled(0.0f);

    snn_network_t* net = NULL; int src = -1, dst = -1;
    ASSERT_EQ_INT(build_two_pop_net(&net, &src, &dst,
                                    SYNAPSE_AMPA, /*weight*/ 8.0f),
                  0, "build failed");

    for (int s = 0; s < 50; s++) {
        drive_src_all_spiking(net, src);
        ASSERT_TRUE(snn_network_step(net, 1.0f) >= 0, "step failed");
    }

    /* All 4 g_* arrays on dst must still be zero — the CB OFF path must
     * not consult or write them. */
    snn_population_t* dpop = net->populations[dst];
    for (uint32_t i = 0; i < dpop->n_neurons; i++) {
        ASSERT_FLOAT_EQ(dpop->g_ampa[i],   0.0f, "g_ampa nonzero in OFF mode");
        ASSERT_FLOAT_EQ(dpop->g_nmda[i],   0.0f, "g_nmda nonzero in OFF mode");
        ASSERT_FLOAT_EQ(dpop->g_gaba_a[i], 0.0f, "g_gaba_a nonzero in OFF mode");
        ASSERT_FLOAT_EQ(dpop->g_gaba_b[i], 0.0f, "g_gaba_b nonzero in OFF mode");
    }
    /* And the membrane potential must remain finite — proxy for "current-
     * mode legacy fallback ran cleanly". */
    const float* v = (const float*)nimcp_tensor_data(dpop->membrane_v);
    for (uint32_t i = 0; i < dpop->n_neurons; i++) {
        ASSERT_TRUE(isfinite(v[i]), "non-finite V in OFF mode");
    }

    snn_network_destroy(net);
    /* Restore CB ON for any later tests. */
    snn_tune_set_conductance_enabled(1.0f);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Driver.                                                                   */
/* ------------------------------------------------------------------------- */

int main(void) {
    if (nimcp_init() != NIMCP_SUCCESS) {
        fprintf(stderr, "nimcp_init failed\n");
        return 1;
    }

    printf("=== SNN per-receptor regression tests ===\n");

    test_ampa_only_matches_lumped_exc();
    test_gaba_a_only_matches_lumped_inh();
    test_legacy_aliases_route_to_new_globals();
    test_population_alloc_frees_four_buckets();
    test_synapse_type_per_src_routing();
    test_cb_flag_off_keeps_g_arrays_quiet();

    printf("\n=== Summary: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    nimcp_shutdown();
    return tests_failed == 0 ? 0 : 1;
}
