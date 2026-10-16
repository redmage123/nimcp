/**
 * @file test_snn_per_receptor_integration.c
 * @brief Integration tests for multi-pop SNN with mixed per-receptor
 *        synapse types (P0 receptor split — AMPA/NMDA/GABA_A/GABA_B).
 * @date 2026-04-26
 *
 * WHAT: Builds small multi-population SNNs (≤300 neurons), wires them with
 *       snn_network_connect_populations() using each of the four real
 *       receptor types, and verifies the runtime hot loop routes deposits
 *       into the correct per-receptor conductance bucket.
 *
 * WHY:  The CB migration's P0 step splits the lumped g_exc / g_inh into
 *       four real receptors. The receptor type is stored per-pop-pair on
 *       dst->synapse_type_per_src[src] and read by the deposit kernel.
 *       Unit tests cover the math in isolation; these tests verify the
 *       wiring/runtime composition end-to-end.
 *
 * HOW:  Check framework. Each test creates a fresh net + populations,
 *       wires a specific topology (e.g. NMDA-only top-down), drives the
 *       presyn pop, runs snn_network_step for a controlled number of ms,
 *       and asserts the post-state of g_ampa / g_nmda / g_gaba_a /
 *       g_gaba_b on the destination pop.
 *
 * NOTE: The impl is being updated in parallel with the test. If a test
 *       compiles but fails at runtime, surface the failure verbatim — do
 *       not patch the impl.
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
#include "snn/nimcp_snn_synapse.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "core/synapse_types/nimcp_synapse_types.h"
#include "utils/tensor/nimcp_tensor.h"

/* SNN tunables (extern declarations — no header exposes them). */
extern void  snn_tune_set_conductance_enabled(float);
extern void  snn_tune_set_cb_weights_rescaled(float);
extern void  snn_tune_set_noise_rate_hz(float);
extern void  snn_tune_set_basket_enabled(float);
extern void  snn_tune_set_ahp_enabled(float);
extern void  snn_tune_set_pump_enabled(float);
extern void  snn_tune_set_substrate_enabled(float);

extern void  snn_tune_set_e_ampa_mv(float);
extern void  snn_tune_set_e_nmda_mv(float);
extern void  snn_tune_set_e_gaba_a_mv(float);
extern void  snn_tune_set_e_gaba_b_mv(float);
extern void  snn_tune_set_tau_ampa_ms(float);
extern void  snn_tune_set_tau_nmda_ms(float);
extern void  snn_tune_set_tau_gaba_a_ms(float);
extern void  snn_tune_set_tau_gaba_b_ms(float);
extern void  snn_tune_set_nmda_mg_mm(float);

/* Legacy alias setters (Phase 6 below). */
extern void  snn_tune_set_tau_exc_ms(float);
extern void  snn_tune_set_tau_inh_ms(float);

/* Getters used in the alias-preservation test. */
extern float snn_tune_get_tau_ampa_ms(void);
extern float snn_tune_get_tau_gaba_a_ms(void);

/* ============================================================================
 * Test Globals
 * ============================================================================
 *
 * Like the SNN conductance integration suite (.cpp), nimcp_init/shutdown is
 * not idempotent — running it per-test corrupts subsequent tests. Use a
 * one-shot global init in main() and a per-test SNN-only fixture.
 */

static snn_network_t* g_net = NULL;

/* Reset all SNN tunables to a known baseline so per-test order does not
 * leak state across cases. The values mirror the conductance integration
 * fixture's defaults. */
static void reset_snn_tunables(void)
{
    snn_tune_set_noise_rate_hz(0.0f);
    snn_tune_set_basket_enabled(0.0f);
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);
    snn_tune_set_substrate_enabled(0.0f);

    snn_tune_set_conductance_enabled(0.0f);   /* start CB OFF — tests opt in */
    snn_tune_set_cb_weights_rescaled(0.0f);

    /* Per-receptor reversal potentials (mV). */
    snn_tune_set_e_ampa_mv(0.0f);
    snn_tune_set_e_nmda_mv(0.0f);
    snn_tune_set_e_gaba_a_mv(-75.0f);
    snn_tune_set_e_gaba_b_mv(-90.0f);

    /* Per-receptor decay (ms). Defaults reflect biology
     * (AMPA fast, NMDA slow, GABA_A fast-ish, GABA_B slow). */
    snn_tune_set_tau_ampa_ms(2.0f);
    snn_tune_set_tau_nmda_ms(100.0f);
    snn_tune_set_tau_gaba_a_ms(10.0f);
    snn_tune_set_tau_gaba_b_ms(150.0f);

    /* NMDA Mg2+ block — biologically ~1.0 mM. */
    snn_tune_set_nmda_mg_mm(1.0f);
}

static snn_network_t* fresh_net(void)
{
    /* snn_config_validate() rejects n_inputs == 0 || n_outputs == 0. We do
     * not actually use the legacy input/output dense path — all populations
     * are added via snn_network_add_population_lightweight() — but we still
     * need a non-zero n_inputs/n_outputs to pass validation. Mirror the
     * pattern in test_snn_conductance_integration.cpp. */
    snn_config_t cfg;
    snn_config_default(&cfg);
    cfg.n_inputs  = 1;
    cfg.n_outputs = 1;
    cfg.n_hidden  = 0;
    cfg.dt        = 1.0f;
    snn_network_t* net = snn_network_create(&cfg);
    return net;
}

static void setup(void)
{
    reset_snn_tunables();
    g_net = fresh_net();
    ck_assert_ptr_nonnull(g_net);
}

static void teardown(void)
{
    if (g_net) {
        snn_network_destroy(g_net);
        g_net = NULL;
    }
    /* Always restore CB off so a CB-on test can't pollute the next. */
    snn_tune_set_conductance_enabled(0.0f);
    snn_tune_set_cb_weights_rescaled(0.0f);
}

/* ============================================================================
 * Helper utilities
 * ============================================================================ */

/* Force-fire every neuron in the source pop on this step.
 *
 * Strategy: park v just above v_thresh, clear refractory, set external
 * current so it stays driven. Identical to the helper used in
 * test_snn_conductance_integration.cpp (drive_input_all_spike). */
static void drive_pop_all_spike(snn_network_t* net, int pop_id)
{
    snn_population_t* p = net->populations[pop_id];
    if (!p) return;

    float* v = (float*)nimcp_tensor_data(p->membrane_v);
    if (v) {
        for (uint32_t i = 0; i < p->n_neurons; i++) v[i] = -49.5f;
    }
    float* ref = (float*)nimcp_tensor_data(p->refractory);
    if (ref) {
        for (uint32_t i = 0; i < p->n_neurons; i++) ref[i] = 0.0f;
    }
    if (p->external_current) {
        for (uint32_t i = 0; i < p->n_neurons; i++) p->external_current[i] = 100.0f;
    }
}

/* Park every neuron in the source pop deeply hyperpolarized + in refractory
 * so it cannot fire on this step — the dual of drive_pop_all_spike. */
static void quiet_pop(snn_network_t* net, int pop_id)
{
    snn_population_t* p = net->populations[pop_id];
    if (!p) return;

    float* v = (float*)nimcp_tensor_data(p->membrane_v);
    if (v) {
        for (uint32_t i = 0; i < p->n_neurons; i++) v[i] = -75.0f;
    }
    float* spk = (float*)nimcp_tensor_data(p->spike_output);
    if (spk) {
        for (uint32_t i = 0; i < p->n_neurons; i++) spk[i] = 0.0f;
    }
    float* ref = (float*)nimcp_tensor_data(p->refractory);
    if (ref) {
        for (uint32_t i = 0; i < p->n_neurons; i++) ref[i] = 100.0f;
    }
    if (p->external_current) {
        for (uint32_t i = 0; i < p->n_neurons; i++) p->external_current[i] = 0.0f;
    }
}

/* Reset a pop's transient state (V, spikes, g_*, external_current) WITHOUT
 * raising refractory — for use between test phases where we want the pop
 * to be able to fire freely in the next phase. quiet_pop blocks firing
 * for 100 ms via the ref clamp; this is its non-blocking sibling. */
static void reset_pop_state(snn_network_t* net, int pop_id)
{
    snn_population_t* p = net->populations[pop_id];
    if (!p) return;

    float* v = (float*)nimcp_tensor_data(p->membrane_v);
    if (v) for (uint32_t i = 0; i < p->n_neurons; i++) v[i] = -65.0f;
    float* spk = (float*)nimcp_tensor_data(p->spike_output);
    if (spk) for (uint32_t i = 0; i < p->n_neurons; i++) spk[i] = 0.0f;
    float* ref = (float*)nimcp_tensor_data(p->refractory);
    if (ref) for (uint32_t i = 0; i < p->n_neurons; i++) ref[i] = 0.0f;
    if (p->external_current)
        for (uint32_t i = 0; i < p->n_neurons; i++) p->external_current[i] = 0.0f;
    if (p->g_ampa)   for (uint32_t i = 0; i < p->n_neurons; i++) p->g_ampa[i]   = 0.0f;
    if (p->g_nmda)   for (uint32_t i = 0; i < p->n_neurons; i++) p->g_nmda[i]   = 0.0f;
    if (p->g_gaba_a) for (uint32_t i = 0; i < p->n_neurons; i++) p->g_gaba_a[i] = 0.0f;
    if (p->g_gaba_b) for (uint32_t i = 0; i < p->n_neurons; i++) p->g_gaba_b[i] = 0.0f;
}

/* Inject conductance directly into one bucket on the dst pop, bypassing
 * upstream firing. Useful for the GABA_A vs GABA_B IPSP duration test —
 * we don't need to model an inhibitory presyn, just the IPSP itself. */
static void inject_conductance(snn_network_t* net, int pop_id,
                               int receptor /* 0=AMPA 1=NMDA 2=GABA_A 3=GABA_B */,
                               float g_value)
{
    snn_population_t* p = net->populations[pop_id];
    if (!p) return;
    float* g_arr = NULL;
    switch (receptor) {
        case 0: g_arr = p->g_ampa;   break;
        case 1: g_arr = p->g_nmda;   break;
        case 2: g_arr = p->g_gaba_a; break;
        case 3: g_arr = p->g_gaba_b; break;
        default: return;
    }
    if (!g_arr) return;
    for (uint32_t i = 0; i < p->n_neurons; i++) g_arr[i] = g_value;
}

/* Mean membrane voltage across a population. */
static float mean_v(snn_network_t* net, int pop_id)
{
    snn_population_t* p = net->populations[pop_id];
    if (!p) return 0.0f;
    const float* v = (const float*)nimcp_tensor_data(p->membrane_v);
    if (!v) return 0.0f;
    double sum = 0;
    for (uint32_t i = 0; i < p->n_neurons; i++) sum += v[i];
    return (float)(sum / p->n_neurons);
}

/* Total spikes across a population in this last step (spike_output is a
 * per-step buffer, not cumulative). */
static uint32_t count_spikes(snn_network_t* net, int pop_id)
{
    snn_population_t* p = net->populations[pop_id];
    if (!p) return 0;
    const float* spk = (const float*)nimcp_tensor_data(p->spike_output);
    if (!spk) return 0;
    uint32_t c = 0;
    for (uint32_t i = 0; i < p->n_neurons; i++) if (spk[i] > 0.5f) c++;
    return c;
}

/* Wire dense connectivity from src→dst with the given receptor type and
 * weight. Uses snn_network_connect_populations directly — that's the API
 * the impl writes synapse_type_per_src[src] inside. Connectivity = 1.0
 * keeps the test deterministic at ≤100×100 sizes. */
static int connect_dense(snn_network_t* net,
                         int src_pop, int dst_pop,
                         synapse_type_t syn_type,
                         float weight)
{
    return snn_network_connect_populations(
        net, (uint32_t)src_pop, (uint32_t)dst_pop,
        SNN_TOPO_FULL, 1.0f,
        syn_type,
        weight, 0.0f /* no jitter */);
}

/* ============================================================================
 * Test 1: NMDA-only top-down is silent at rest
 * ============================================================================
 *
 * Pure-NMDA pathway: presyn → postsyn fully connected as SYNAPSE_NMDA, no
 * AMPA. With CB on and the postsyn pop sitting at v_rest, the NMDA Mg²⁺
 * block keeps the conductance ineffective. Postsyn must NOT spike even
 * when presyn fires hard.
 */
START_TEST(test_nmda_only_silent_at_rest)
{
    /* Calibration: 8 presyn neurons + per-synapse weight 0.05 → at peak
     * unblocking m≈0.78 the effective drive per dst neuron is at most
     * 8 × 0.05 × 0.78 × 65 ≈ 20 mV; at rest m≈0.06 → 8 × 0.05 × 0.06 × 65
     * ≈ 1.6 mV (well subthreshold). This matches the biological scale of
     * a single thalamocortical bouton without saturating the test setup. */
    int presyn  = snn_network_add_population_lightweight(g_net, 8,  NEURON_GENERIC_LIF, "presyn");
    int postsyn = snn_network_add_population_lightweight(g_net, 50, NEURON_GENERIC_LIF, "postsyn");
    ck_assert_int_ge(presyn, 0);
    ck_assert_int_ge(postsyn, 0);

    /* NMDA pathway — moderate weight per synapse. Mg²⁺ block at v_rest
     * should keep effective drive near zero (≈ 6% of unblocked). */
    int n_conn = connect_dense(g_net, presyn, postsyn, SYNAPSE_NMDA, 0.05f);
    ck_assert_int_gt(n_conn, 0);
    /* finalize_connections returns the count of populations whose CSR was
     * finalized; we just need it non-negative (any error would be -1). */
    ck_assert_int_ge(snn_network_finalize_connections(g_net), 0);

    snn_tune_set_conductance_enabled(1.0f);
    /* Strong Mg block to make this test maximally biological. */
    snn_tune_set_nmda_mg_mm(1.0f);

    /* Drive presyn for several ms; check postsyn never spikes. */
    uint32_t total_post_spikes = 0;
    for (int s = 0; s < 30; s++) {
        drive_pop_all_spike(g_net, presyn);
        snn_network_step(g_net, 1.0f);
        total_post_spikes += count_spikes(g_net, postsyn);
    }
    ck_assert_msg(total_post_spikes == 0,
                  "NMDA-only at rest: postsyn fired %u spikes (expected 0 — Mg block)",
                  total_post_spikes);
}
END_TEST

/* ============================================================================
 * Test 2: NMDA + AMPA coincidence
 * ============================================================================
 *
 * Pop layout:
 *   ampa_src   → postsyn   (SYNAPSE_AMPA, weight too small alone)
 *   nmda_src   → postsyn   (SYNAPSE_NMDA, blocked at rest)
 *
 * Phase A: only ampa_src fires with weak AMPA → postsyn does NOT cross
 *          threshold (weak AMPA alone insufficient).
 * Phase B: both ampa_src AND nmda_src fire → AMPA depolarizes postsyn,
 *          relieving the Mg²⁺ block on NMDA, letting the NMDA conductance
 *          push postsyn over threshold. Postsyn DOES spike.
 *
 * This is the classical "NMDA as coincidence detector" property.
 */
START_TEST(test_nmda_ampa_coincidence)
{
    /* Calibration: steady-state v_ss = -65 / (1 + Σg) drives the design.
     * AMPA-only:  6 × 0.045 = g_ampa 0.27 → v_ss = -65/1.27 = -51.2 mV
     *             (just subthreshold — phase A doesn't fire).
     * Both:       g_nmda 1.80 with m(-50) ≈ 0.139 → effective ≈ 0.25
     *             added → v_ss = -65/(1.27+0.25) ≈ -42.8 mV (fires).
     * The "blocked at rest, opens with depolarization" coincidence
     * mechanism is what lets the second pathway tip the balance. */
    int ampa_src = snn_network_add_population_lightweight(g_net, 6, NEURON_GENERIC_LIF, "ampa_src");
    int nmda_src = snn_network_add_population_lightweight(g_net, 6, NEURON_GENERIC_LIF, "nmda_src");
    int postsyn  = snn_network_add_population_lightweight(g_net, 50, NEURON_GENERIC_LIF, "postsyn");
    ck_assert_int_ge(ampa_src, 0);
    ck_assert_int_ge(nmda_src, 0);
    ck_assert_int_ge(postsyn,  0);

    /* Weak AMPA — alone not enough to cross threshold (v_ss ≈ -51 mV). */
    int n_a = connect_dense(g_net, ampa_src, postsyn, SYNAPSE_AMPA, 0.045f);
    /* Strong NMDA — but blocked unless AMPA primes the cell.
     * Once V depolarizes past ~-50 mV, the Mg block opens enough to
     * deliver another ~25% effective conductance, pushing v_ss over thresh. */
    int n_n = connect_dense(g_net, nmda_src, postsyn, SYNAPSE_NMDA, 0.30f);
    ck_assert_int_gt(n_a, 0);
    ck_assert_int_gt(n_n, 0);
    /* finalize_connections returns the count of populations whose CSR was
     * finalized; we just need it non-negative (any error would be -1). */
    ck_assert_int_ge(snn_network_finalize_connections(g_net), 0);

    snn_tune_set_conductance_enabled(1.0f);
    snn_tune_set_nmda_mg_mm(1.0f);

    /* Phase A: AMPA-only, NMDA quiet. Should NOT spike (weak AMPA). */
    uint32_t spikes_ampa_only = 0;
    for (int s = 0; s < 20; s++) {
        drive_pop_all_spike(g_net, ampa_src);
        quiet_pop(g_net, nmda_src);
        snn_network_step(g_net, 1.0f);
        spikes_ampa_only += count_spikes(g_net, postsyn);
    }

    /* Reset postsyn state for phase B — clear V, spikes, conductances,
     * external_current — but do NOT raise refractory (which would block
     * phase B from firing in the 20-step window). */
    reset_pop_state(g_net, postsyn);

    /* Phase B: AMPA + NMDA together. SHOULD spike (coincidence). */
    uint32_t spikes_both = 0;
    for (int s = 0; s < 20; s++) {
        drive_pop_all_spike(g_net, ampa_src);
        drive_pop_all_spike(g_net, nmda_src);
        snn_network_step(g_net, 1.0f);
        spikes_both += count_spikes(g_net, postsyn);
    }

    ck_assert_msg(spikes_ampa_only == 0,
                  "AMPA alone (weak) caused %u spikes; expected 0 (subthreshold)",
                  spikes_ampa_only);
    ck_assert_msg(spikes_both > 0,
                  "AMPA+NMDA coincidence produced no spikes (expected >0)");
}
END_TEST

/* ============================================================================
 * Test 3: GABA_A vs GABA_B distinct IPSP duration
 * ============================================================================
 *
 * Inject a brief conductance pulse directly on the dst pop's GABA_A bucket
 * (or GABA_B) and let it decay across many simulation steps.
 * GABA_A: τ ≈ 10 ms → IPSP returns to baseline within ~30 ms (≈3τ).
 * GABA_B: τ ≈ 150 ms → IPSP persists well beyond 100 ms.
 *
 * Membrane V is the cleanest probe: while inhibitory g is active, V is
 * pulled toward E_gaba_a/b (negative) below v_rest. When g decays to ~0,
 * V relaxes back to v_rest.
 */
START_TEST(test_gaba_a_vs_gaba_b_ipsp_duration)
{
    int dst = snn_network_add_population_lightweight(g_net, 80, NEURON_GENERIC_LIF, "dst");
    ck_assert_int_ge(dst, 0);

    /* Need to finalize for the lightweight CSR step path to be active. */
    /* finalize_connections returns the count of populations whose CSR was
     * finalized; we just need it non-negative (any error would be -1). */
    ck_assert_int_ge(snn_network_finalize_connections(g_net), 0);

    snn_tune_set_conductance_enabled(1.0f);
    /* Use biologically-canonical values so τ ratio is unambiguous. */
    snn_tune_set_tau_gaba_a_ms(10.0f);
    snn_tune_set_tau_gaba_b_ms(150.0f);

    /* --- GABA_A pulse --- */
    quiet_pop(g_net, dst);
    snn_population_t* dst_pop = g_net->populations[dst];
    /* Bring V to rest first so we can see the IPSP vs baseline. */
    float* v = (float*)nimcp_tensor_data(dst_pop->membrane_v);
    for (uint32_t i = 0; i < dst_pop->n_neurons; i++) v[i] = -65.0f;
    /* Inject a single pulse on g_gaba_a. */
    inject_conductance(g_net, dst, 2 /*GABA_A*/, 1.0f);

    /* Step 30 ms; expect g_gaba_a effectively decayed. */
    for (int s = 0; s < 30; s++) snn_network_step(g_net, 1.0f);

    /* After 30 ms ≈ 3τ for τ=10ms, g should be ~5% of initial. */
    float g_a_after_30 = 0.0f;
    for (uint32_t i = 0; i < dst_pop->n_neurons; i++) g_a_after_30 += dst_pop->g_gaba_a[i];
    g_a_after_30 /= dst_pop->n_neurons;

    /* --- GABA_B pulse --- */
    quiet_pop(g_net, dst);
    for (uint32_t i = 0; i < dst_pop->n_neurons; i++) v[i] = -65.0f;
    /* Clear GABA_A residue from the previous phase. */
    for (uint32_t i = 0; i < dst_pop->n_neurons; i++) dst_pop->g_gaba_a[i] = 0.0f;
    inject_conductance(g_net, dst, 3 /*GABA_B*/, 1.0f);

    /* Step 100 ms — for τ=150ms, g_gaba_b should still be > 50% of initial. */
    for (int s = 0; s < 100; s++) snn_network_step(g_net, 1.0f);

    float g_b_after_100 = 0.0f;
    for (uint32_t i = 0; i < dst_pop->n_neurons; i++) g_b_after_100 += dst_pop->g_gaba_b[i];
    g_b_after_100 /= dst_pop->n_neurons;

    /* GABA_A: should have decayed substantially by 30 ms. */
    ck_assert_msg(g_a_after_30 < 0.15f,
                  "GABA_A IPSP did not decay within 30 ms: g_a = %.4f (expected < 0.15 ≈ exp(-3))",
                  g_a_after_30);
    /* GABA_B: should still be substantially present at 100 ms. */
    ck_assert_msg(g_b_after_100 > 0.30f,
                  "GABA_B IPSP decayed too fast at 100 ms: g_b = %.4f (expected > 0.30, τ=150ms)",
                  g_b_after_100);
    /* And the durations must be ordered: GABA_B persists much longer than GABA_A. */
    ck_assert_msg(g_b_after_100 > g_a_after_30,
                  "GABA_B at 100 ms (%.4f) should exceed GABA_A at 30 ms (%.4f)",
                  g_b_after_100, g_a_after_30);
}
END_TEST

/* ============================================================================
 * Test 4: Per-pop-pair table populated by connect_populations
 * ============================================================================
 *
 * After snn_network_connect_populations(net, src, dst, ..., synapse_type),
 * the dst pop's synapse_type_per_src[src] entry must equal that
 * synapse_type cast to byte. Test all four real receptors against
 * different (src, dst) pairs.
 */
START_TEST(test_synapse_type_per_src_populated)
{
    int a = snn_network_add_population_lightweight(g_net, 20, NEURON_GENERIC_LIF, "a");
    int b = snn_network_add_population_lightweight(g_net, 20, NEURON_GENERIC_LIF, "b");
    int c = snn_network_add_population_lightweight(g_net, 20, NEURON_GENERIC_LIF, "c");
    int d = snn_network_add_population_lightweight(g_net, 20, NEURON_GENERIC_LIF, "d");
    ck_assert_int_ge(a, 0);
    ck_assert_int_ge(b, 0);
    ck_assert_int_ge(c, 0);
    ck_assert_int_ge(d, 0);

    /* a→b AMPA, a→c NMDA, b→d GABA_A, c→d GABA_B */
    ck_assert_int_gt(connect_dense(g_net, a, b, SYNAPSE_AMPA,    0.1f), 0);
    ck_assert_int_gt(connect_dense(g_net, a, c, SYNAPSE_NMDA,    0.1f), 0);
    ck_assert_int_gt(connect_dense(g_net, b, d, SYNAPSE_GABA_A,  0.1f), 0);
    ck_assert_int_gt(connect_dense(g_net, c, d, SYNAPSE_GABA_B,  0.1f), 0);

    snn_population_t* B = g_net->populations[b];
    snn_population_t* C = g_net->populations[c];
    snn_population_t* D = g_net->populations[d];
    ck_assert_ptr_nonnull(B);
    ck_assert_ptr_nonnull(C);
    ck_assert_ptr_nonnull(D);

    ck_assert_msg((synapse_type_t)B->synapse_type_per_src[a] == SYNAPSE_AMPA,
                  "B->synapse_type_per_src[a] = %u, expected SYNAPSE_AMPA(%d)",
                  (unsigned)B->synapse_type_per_src[a], (int)SYNAPSE_AMPA);
    ck_assert_msg((synapse_type_t)C->synapse_type_per_src[a] == SYNAPSE_NMDA,
                  "C->synapse_type_per_src[a] = %u, expected SYNAPSE_NMDA(%d)",
                  (unsigned)C->synapse_type_per_src[a], (int)SYNAPSE_NMDA);
    ck_assert_msg((synapse_type_t)D->synapse_type_per_src[b] == SYNAPSE_GABA_A,
                  "D->synapse_type_per_src[b] = %u, expected SYNAPSE_GABA_A(%d)",
                  (unsigned)D->synapse_type_per_src[b], (int)SYNAPSE_GABA_A);
    ck_assert_msg((synapse_type_t)D->synapse_type_per_src[c] == SYNAPSE_GABA_B,
                  "D->synapse_type_per_src[c] = %u, expected SYNAPSE_GABA_B(%d)",
                  (unsigned)D->synapse_type_per_src[c], (int)SYNAPSE_GABA_B);
}
END_TEST

/* ============================================================================
 * Test 5: Mixed-receptor pop — one dst, three different presyn pathways
 * ============================================================================
 *
 *   src1  → dst   (AMPA)
 *   src2  → dst   (NMDA)
 *   src3  → dst   (GABA_A)
 *
 * After driving all three sources to fire, dst must have NONZERO values in
 * g_ampa, g_nmda, AND g_gaba_a. g_gaba_b stays zero (no source connected
 * with that type).
 */
START_TEST(test_mixed_receptor_population)
{
    int src1 = snn_network_add_population_lightweight(g_net, 30, NEURON_GENERIC_LIF, "src_ampa");
    int src2 = snn_network_add_population_lightweight(g_net, 30, NEURON_GENERIC_LIF, "src_nmda");
    int src3 = snn_network_add_population_lightweight(g_net, 30, NEURON_GENERIC_LIF, "src_gabaa");
    int dst  = snn_network_add_population_lightweight(g_net, 30, NEURON_GENERIC_LIF, "dst");
    ck_assert_int_ge(src1, 0);
    ck_assert_int_ge(src2, 0);
    ck_assert_int_ge(src3, 0);
    ck_assert_int_ge(dst,  0);

    ck_assert_int_gt(connect_dense(g_net, src1, dst, SYNAPSE_AMPA,    0.10f), 0);
    ck_assert_int_gt(connect_dense(g_net, src2, dst, SYNAPSE_NMDA,    0.10f), 0);
    /* GABA_A weight is positive on input — the deposit kernel takes |w|
     * for inhibitory receptors per the membrane.h contract. */
    ck_assert_int_gt(connect_dense(g_net, src3, dst, SYNAPSE_GABA_A,  0.10f), 0);
    /* finalize_connections returns the count of populations whose CSR was
     * finalized; we just need it non-negative (any error would be -1). */
    ck_assert_int_ge(snn_network_finalize_connections(g_net), 0);

    snn_tune_set_conductance_enabled(1.0f);

    /* Drive all three sources to fire for several steps. */
    for (int s = 0; s < 5; s++) {
        drive_pop_all_spike(g_net, src1);
        drive_pop_all_spike(g_net, src2);
        drive_pop_all_spike(g_net, src3);
        snn_network_step(g_net, 1.0f);
    }

    snn_population_t* D = g_net->populations[dst];
    ck_assert_ptr_nonnull(D);
    ck_assert_ptr_nonnull(D->g_ampa);
    ck_assert_ptr_nonnull(D->g_nmda);
    ck_assert_ptr_nonnull(D->g_gaba_a);
    ck_assert_ptr_nonnull(D->g_gaba_b);

    /* Sum across neurons — at least one neuron in each bucket must be > 0
     * for that receptor to count as "active". */
    double sum_ampa = 0, sum_nmda = 0, sum_gaba_a = 0, sum_gaba_b = 0;
    for (uint32_t i = 0; i < D->n_neurons; i++) {
        sum_ampa   += D->g_ampa[i];
        sum_nmda   += D->g_nmda[i];
        sum_gaba_a += D->g_gaba_a[i];
        sum_gaba_b += D->g_gaba_b[i];
    }

    ck_assert_msg(sum_ampa   > 0.0, "g_ampa  not populated; sum=%.4f", sum_ampa);
    ck_assert_msg(sum_nmda   > 0.0, "g_nmda  not populated; sum=%.4f", sum_nmda);
    ck_assert_msg(sum_gaba_a > 0.0, "g_gaba_a not populated; sum=%.4f", sum_gaba_a);
    /* GABA_B has no source — must remain zero. */
    ck_assert_msg(sum_gaba_b == 0.0,
                  "g_gaba_b unexpectedly populated; sum=%.4f", sum_gaba_b);
}
END_TEST

/* ============================================================================
 * Test 6: Lumped-CB regression — legacy aliased setters preserved
 * ============================================================================
 *
 * The CB migration kept the legacy snn_tune_set_tau_exc_ms /
 * snn_tune_set_tau_inh_ms setters as aliases routing into AMPA / GABA_A
 * (the dominant excitatory / inhibitory receptors). Existing Python
 * tune RPCs depend on this. A regression here would silently break callers
 * that haven't been upgraded to the per-receptor setters.
 */
START_TEST(test_legacy_aliases_preserved)
{
    /* Drive the alias setters to a non-default value. */
    snn_tune_set_tau_exc_ms(2.0f);
    snn_tune_set_tau_inh_ms(8.0f);

    float tau_ampa   = snn_tune_get_tau_ampa_ms();
    float tau_gaba_a = snn_tune_get_tau_gaba_a_ms();

    ck_assert_msg(fabsf(tau_ampa   - 2.0f) < 1e-4f,
                  "tau_exc alias did not set tau_ampa: got %.4f, expected 2.0",
                  tau_ampa);
    ck_assert_msg(fabsf(tau_gaba_a - 8.0f) < 1e-4f,
                  "tau_inh alias did not set tau_gaba_a: got %.4f, expected 8.0",
                  tau_gaba_a);
}
END_TEST

/* ============================================================================
 * Test 7: GABA_B firing pathway — actual spike → synapse → g_gaba_b deposit.
 *
 * Closes the gap surfaced by P0 walkthrough #3: prior GABA_B coverage only
 * directly injected conductance (bypassing the synapse type table). This
 * test wires src→dst with SYNAPSE_GABA_B, fires the source via external
 * current, and verifies that ONLY g_gaba_b is populated on the dst — proving
 * the deposit kernel routes GABA_B-tagged synapses into the slow inhibitory
 * bucket and not into AMPA / NMDA / GABA_A.
 * ============================================================================ */
START_TEST(test_gaba_b_firing_pathway_routes_to_g_gaba_b)
{
    snn_tune_set_conductance_enabled(1.0f);

    int src = snn_network_add_population_lightweight(
        g_net, 16, NEURON_GENERIC_LIF, "src_gaba_b");
    int dst = snn_network_add_population_lightweight(
        g_net, 16, NEURON_GENERIC_LIF, "dst_gaba_b");
    ck_assert_int_ge(src, 0);
    ck_assert_int_ge(dst, 0);

    /* Wire one GABA_B pathway. Sign-irrelevant — the deposit kernel takes
     * the absolute value for inhibitory receptors. Pick a large weight so
     * one spike registers strongly on g_gaba_b. */
    int nc = connect_dense(g_net, src, dst, SYNAPSE_GABA_B, -0.5f);
    ck_assert_int_gt(nc, 0);
    ck_assert_int_ge(snn_network_finalize_connections(g_net), 0);

    snn_population_t* dst_pop = g_net->populations[dst];
    ck_assert_ptr_nonnull(dst_pop);
    ck_assert_msg(
        (synapse_type_t)dst_pop->synapse_type_per_src[src] == SYNAPSE_GABA_B,
        "GABA_B-typed connect did not register in synapse_type_per_src "
        "(found %d, expected %d)",
        (int)dst_pop->synapse_type_per_src[src], (int)SYNAPSE_GABA_B);

    /* Pre-conditions: zero all dst conductances. */
    if (dst_pop->g_ampa)   for (uint32_t i = 0; i < dst_pop->n_neurons; i++) dst_pop->g_ampa[i]   = 0.0f;
    if (dst_pop->g_nmda)   for (uint32_t i = 0; i < dst_pop->n_neurons; i++) dst_pop->g_nmda[i]   = 0.0f;
    if (dst_pop->g_gaba_a) for (uint32_t i = 0; i < dst_pop->n_neurons; i++) dst_pop->g_gaba_a[i] = 0.0f;
    if (dst_pop->g_gaba_b) for (uint32_t i = 0; i < dst_pop->n_neurons; i++) dst_pop->g_gaba_b[i] = 0.0f;

    /* Force the source to fire on this step, step the network, observe dst. */
    drive_pop_all_spike(g_net, src);
    snn_network_step(g_net, 1.0f);

    float sum_ampa = 0.0f, sum_nmda = 0.0f, sum_gaba_a = 0.0f, sum_gaba_b = 0.0f;
    for (uint32_t i = 0; i < dst_pop->n_neurons; i++) {
        if (dst_pop->g_ampa)   sum_ampa   += dst_pop->g_ampa[i];
        if (dst_pop->g_nmda)   sum_nmda   += dst_pop->g_nmda[i];
        if (dst_pop->g_gaba_a) sum_gaba_a += dst_pop->g_gaba_a[i];
        if (dst_pop->g_gaba_b) sum_gaba_b += dst_pop->g_gaba_b[i];
    }
    ck_assert_msg(sum_gaba_b > 0.0f,
                  "GABA_B deposit did not populate g_gaba_b (sum=%.4f)",
                  sum_gaba_b);
    ck_assert_msg(sum_ampa == 0.0f && sum_nmda == 0.0f && sum_gaba_a == 0.0f,
                  "GABA_B-typed synapse leaked into another bucket: "
                  "g_ampa=%.4f g_nmda=%.4f g_gaba_a=%.4f g_gaba_b=%.4f",
                  sum_ampa, sum_nmda, sum_gaba_a, sum_gaba_b);
}
END_TEST

/* ============================================================================
 * Test Suite
 * ============================================================================ */

static Suite* per_receptor_integration_suite(void)
{
    Suite* s = suite_create("SNN Per-Receptor Integration (P0)");

    TCase* tc_nmda_silent = tcase_create("NMDA-only at rest is silent");
    tcase_add_checked_fixture(tc_nmda_silent, setup, teardown);
    tcase_add_test(tc_nmda_silent, test_nmda_only_silent_at_rest);
    tcase_set_timeout(tc_nmda_silent, 30);
    suite_add_tcase(s, tc_nmda_silent);

    TCase* tc_coincidence = tcase_create("NMDA+AMPA coincidence");
    tcase_add_checked_fixture(tc_coincidence, setup, teardown);
    tcase_add_test(tc_coincidence, test_nmda_ampa_coincidence);
    tcase_set_timeout(tc_coincidence, 30);
    suite_add_tcase(s, tc_coincidence);

    TCase* tc_ipsp = tcase_create("GABA_A vs GABA_B IPSP duration");
    tcase_add_checked_fixture(tc_ipsp, setup, teardown);
    tcase_add_test(tc_ipsp, test_gaba_a_vs_gaba_b_ipsp_duration);
    tcase_set_timeout(tc_ipsp, 30);
    suite_add_tcase(s, tc_ipsp);

    TCase* tc_table = tcase_create("synapse_type_per_src table");
    tcase_add_checked_fixture(tc_table, setup, teardown);
    tcase_add_test(tc_table, test_synapse_type_per_src_populated);
    tcase_set_timeout(tc_table, 15);
    suite_add_tcase(s, tc_table);

    TCase* tc_mixed = tcase_create("Mixed-receptor population");
    tcase_add_checked_fixture(tc_mixed, setup, teardown);
    tcase_add_test(tc_mixed, test_mixed_receptor_population);
    tcase_set_timeout(tc_mixed, 30);
    suite_add_tcase(s, tc_mixed);

    /* Alias preservation does NOT need a network — just tune state. */
    TCase* tc_alias = tcase_create("Legacy lumped-CB aliases");
    tcase_add_test(tc_alias, test_legacy_aliases_preserved);
    tcase_set_timeout(tc_alias, 5);
    suite_add_tcase(s, tc_alias);

    TCase* tc_gaba_b = tcase_create("GABA_B firing pathway");
    tcase_add_checked_fixture(tc_gaba_b, setup, teardown);
    tcase_add_test(tc_gaba_b, test_gaba_b_firing_pathway_routes_to_g_gaba_b);
    tcase_set_timeout(tc_gaba_b, 30);
    suite_add_tcase(s, tc_gaba_b);

    return s;
}

int main(void)
{
    /* nimcp_init/shutdown is not idempotent — once for the whole process. */
    if (nimcp_init() != NIMCP_SUCCESS) {
        fprintf(stderr, "nimcp_init failed\n");
        return EXIT_FAILURE;
    }

    Suite* s = per_receptor_integration_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    nimcp_shutdown();
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
