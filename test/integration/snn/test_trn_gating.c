/**
 * @file test_trn_gating.c
 * @brief Integration tests for P3.1 — reticular thalamic nucleus (TRN).
 * @date 2026-04-27
 *
 * WHAT: Verifies the new TRN block in nimcp_snn_hierarchical.c:
 *       - "thalamus_reticular" pop (10K NEURON_GENERIC_LIF) is created
 *       - tier-0 (input_*) and tier-6 (L6_project_*) sources wire INTO TRN
 *         as SYNAPSE_NMDA
 *       - TRN wires OUT to tier-0 dst pops as SYNAPSE_GABA_A
 *       - Behavioural: a relay→TRN→relay loop measurably reduces relay
 *         firing in the closed-loop phase vs the open-loop phase
 *
 * WHY:  TRN is the substrate's only biological mechanism for stimulus-
 *       specific attentional gating on sensory drive (Crick 1984's
 *       "attentional searchlight", Pinault 2004 review). Without these
 *       wires, the substrate-correctness audit flags a missing primitive.
 *
 * HOW:  Tests 1-3 build the full 1.8M hierarchy ONCE (CK_NOFORK) and
 *       inspect synapse_type_per_src + incoming_csr counts on each
 *       relevant dst. Test 4 builds a small two-pop CB-mode network and
 *       compares spike counts between an open-loop phase (TRN clamped
 *       quiet) and a closed-loop phase (TRN allowed to fire and
 *       inhibit).
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

/* SNN runtime tunables (extern, no header). Mirrors test_top_down_feedback.c. */
extern void snn_tune_set_conductance_enabled(float);
extern void snn_tune_set_cb_weights_rescaled(float);
extern void snn_tune_set_noise_rate_hz(float);
extern void snn_tune_set_basket_enabled(float);
extern void snn_tune_set_ahp_enabled(float);
extern void snn_tune_set_pump_enabled(float);
extern void snn_tune_set_substrate_enabled(float);
extern void snn_tune_set_nmda_mg_mm(float);
extern void snn_tune_set_tau_ampa_ms(float);
extern void snn_tune_set_tau_nmda_ms(float);
extern void snn_tune_set_tau_gaba_a_ms(float);
extern void snn_tune_set_tau_gaba_b_ms(float);
extern void snn_tune_set_e_ampa_mv(float);
extern void snn_tune_set_e_nmda_mv(float);
extern void snn_tune_set_e_gaba_a_mv(float);
extern void snn_tune_set_e_gaba_b_mv(float);

/* ============================================================================
 * Globals + fixtures
 * ============================================================================ */

static snn_network_t* g_hier_net = NULL; /* shared across tests 1-3. */
static snn_network_t* g_mini_net = NULL; /* fresh per-test for test 4. */

static void reset_snn_tunables(void)
{
    snn_tune_set_noise_rate_hz(0.0f);
    snn_tune_set_basket_enabled(0.0f);
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);
    snn_tune_set_substrate_enabled(0.0f);
    snn_tune_set_conductance_enabled(0.0f);
    snn_tune_set_cb_weights_rescaled(0.0f);
    snn_tune_set_e_ampa_mv(0.0f);
    snn_tune_set_e_nmda_mv(0.0f);
    snn_tune_set_e_gaba_a_mv(-75.0f);
    snn_tune_set_e_gaba_b_mv(-90.0f);
    snn_tune_set_tau_ampa_ms(2.0f);
    snn_tune_set_tau_nmda_ms(100.0f);
    snn_tune_set_tau_gaba_a_ms(10.0f);
    snn_tune_set_tau_gaba_b_ms(150.0f);
    snn_tune_set_nmda_mg_mm(1.0f);
}

static snn_network_t* fresh_mini_net(void)
{
    snn_config_t cfg;
    snn_config_default(&cfg);
    cfg.n_inputs  = 1;
    cfg.n_outputs = 1;
    cfg.n_hidden  = 0;
    cfg.dt        = 1.0f;
    return snn_network_create(&cfg);
}

static void mini_setup(void)
{
    reset_snn_tunables();
    g_mini_net = fresh_mini_net();
    ck_assert_ptr_nonnull(g_mini_net);
}

static void mini_teardown(void)
{
    if (g_mini_net) {
        snn_network_destroy(g_mini_net);
        g_mini_net = NULL;
    }
    snn_tune_set_conductance_enabled(0.0f);
    snn_tune_set_cb_weights_rescaled(0.0f);
}

static void hier_ensure_built(void)
{
    if (g_hier_net) return;
    /* Note: snn_create_hierarchical_network may load a cached SNN sidecar
     * from "checkpoints/athena/athena_immersive.bin.snn" if reachable from
     * CWD. From the build dir that path doesn't resolve, so we always get
     * a fresh wire. If a stale cache is ever placed under build/checkpoints
     * the cache will lack the TRN pop and tests 1-3 will (correctly) flag
     * regression. */
    g_hier_net = snn_create_hierarchical_network(64, 64, 1800000u);
    ck_assert_ptr_nonnull(g_hier_net);
}

/* ============================================================================
 * Helpers (driving + counting)
 * ============================================================================ */

static void drive_pop_all_spike(snn_network_t* net, int pop_id)
{
    snn_population_t* p = net->populations[pop_id];
    if (!p) return;
    float* v = (float*)nimcp_tensor_data(p->membrane_v);
    if (v) for (uint32_t i = 0; i < p->n_neurons; i++) v[i] = -49.5f;
    float* ref = (float*)nimcp_tensor_data(p->refractory);
    if (ref) for (uint32_t i = 0; i < p->n_neurons; i++) ref[i] = 0.0f;
    if (p->external_current)
        for (uint32_t i = 0; i < p->n_neurons; i++) p->external_current[i] = 100.0f;
}

static void quiet_pop(snn_network_t* net, int pop_id)
{
    snn_population_t* p = net->populations[pop_id];
    if (!p) return;
    float* v = (float*)nimcp_tensor_data(p->membrane_v);
    if (v) for (uint32_t i = 0; i < p->n_neurons; i++) v[i] = -75.0f;
    float* spk = (float*)nimcp_tensor_data(p->spike_output);
    if (spk) for (uint32_t i = 0; i < p->n_neurons; i++) spk[i] = 0.0f;
    float* ref = (float*)nimcp_tensor_data(p->refractory);
    if (ref) for (uint32_t i = 0; i < p->n_neurons; i++) ref[i] = 100.0f;
    if (p->external_current)
        for (uint32_t i = 0; i < p->n_neurons; i++) p->external_current[i] = 0.0f;
    if (p->g_ampa)   for (uint32_t i = 0; i < p->n_neurons; i++) p->g_ampa[i]   = 0.0f;
    if (p->g_nmda)   for (uint32_t i = 0; i < p->n_neurons; i++) p->g_nmda[i]   = 0.0f;
    if (p->g_gaba_a) for (uint32_t i = 0; i < p->n_neurons; i++) p->g_gaba_a[i] = 0.0f;
    if (p->g_gaba_b) for (uint32_t i = 0; i < p->n_neurons; i++) p->g_gaba_b[i] = 0.0f;
}

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

static int connect_dense(snn_network_t* net,
                         int src_pop, int dst_pop,
                         synapse_type_t syn_type,
                         float weight)
{
    return snn_network_connect_populations(
        net, (uint32_t)src_pop, (uint32_t)dst_pop,
        SNN_TOPO_FULL, 1.0f, syn_type, weight, 0.0f);
}

/* Find populations whose name starts with `prefix`. Fills `out` up to `max`
 * and returns the count. */
static uint32_t find_pops_by_prefix(snn_network_t* net,
                                    const char* prefix,
                                    uint32_t* out, uint32_t max)
{
    uint32_t n = 0;
    size_t pl = strlen(prefix);
    for (uint32_t i = 0; i < net->n_populations && n < max; i++) {
        snn_population_t* p = net->populations[i];
        if (!p) continue;
        if (strncmp(p->name, prefix, pl) == 0) out[n++] = i;
    }
    return n;
}

/* Find the first pop whose name exactly matches `name`. Returns -1 if not
 * found. */
static int find_pop_exact(snn_network_t* net, const char* name)
{
    for (uint32_t i = 0; i < net->n_populations; i++) {
        snn_population_t* p = net->populations[i];
        if (!p) continue;
        if (strcmp(p->name, name) == 0) return (int)i;
    }
    return -1;
}

/* Count CSR entries in dst's incoming_csr that originate from any src in
 * src_set AND whose dst-side per-src receptor type matches `expected`. */
static uint64_t count_syns_from(snn_population_t* dst,
                                const uint32_t* src_set, uint32_t n_src,
                                synapse_type_t expected)
{
    if (!dst || !dst->incoming_csr) return 0;
    snn_csr_storage_t* csr = dst->incoming_csr;
    uint64_t total = 0;
    for (uint32_t s = 0; s < csr->n_synapses; s++) {
        uint32_t sp = csr->entries[s].src_pop;
        if ((synapse_type_t)dst->synapse_type_per_src[sp] != expected)
            continue;
        for (uint32_t k = 0; k < n_src; k++) {
            if (src_set[k] == sp) { total++; break; }
        }
    }
    return total;
}

/* Count CSR entries in dst's incoming_csr that originate from a single
 * src_pop_idx (no receptor filter). */
static uint64_t count_syns_from_single(snn_population_t* dst, uint32_t src)
{
    if (!dst || !dst->incoming_csr) return 0;
    snn_csr_storage_t* csr = dst->incoming_csr;
    uint64_t total = 0;
    for (uint32_t s = 0; s < csr->n_synapses; s++) {
        if (csr->entries[s].src_pop == src) total++;
    }
    return total;
}

/* ============================================================================
 * Test 1: TRN population exists at expected size
 * ============================================================================ */
START_TEST(test_trn_pop_exists)
{
    hier_ensure_built();

    int trn = find_pop_exact(g_hier_net, "thalamus_reticular");
    ck_assert_msg(trn >= 0,
                  "Population 'thalamus_reticular' not found in hierarchy");

    snn_population_t* p = g_hier_net->populations[trn];
    ck_assert_ptr_nonnull(p);
    ck_assert_msg(p->n_neurons == 10000u,
                  "thalamus_reticular has %u neurons, expected 10000",
                  p->n_neurons);
}
END_TEST

/* ============================================================================
 * Test 2: TRN inputs wired NMDA from tier-0 (input_*) and tier-6 (L6_*)
 * ============================================================================ */
START_TEST(test_trn_inputs_nmda)
{
    hier_ensure_built();

    int trn = find_pop_exact(g_hier_net, "thalamus_reticular");
    ck_assert_int_ge(trn, 0);
    snn_population_t* trn_pop = g_hier_net->populations[trn];

    uint32_t t0_pops[16], l6_pops[16];
    uint32_t n_t0 = find_pops_by_prefix(g_hier_net, "input_",       t0_pops, 16);
    uint32_t n_l6 = find_pops_by_prefix(g_hier_net, "L6_project_",  l6_pops, 16);
    ck_assert_msg(n_t0 > 0, "No tier-0 (input_*) pops found");
    ck_assert_msg(n_l6 > 0, "No tier-6 (L6_project_*) pops found");

    /* At least one tier-0 src must be flagged NMDA on TRN. */
    bool t0_nmda = false;
    for (uint32_t i = 0; i < n_t0; i++) {
        if ((synapse_type_t)trn_pop->synapse_type_per_src[t0_pops[i]] == SYNAPSE_NMDA) {
            t0_nmda = true; break;
        }
    }
    ck_assert_msg(t0_nmda,
                  "No tier-0 (input_*) → TRN pop-pair flagged as SYNAPSE_NMDA");

    /* Same for tier-6. */
    bool l6_nmda = false;
    for (uint32_t i = 0; i < n_l6; i++) {
        if ((synapse_type_t)trn_pop->synapse_type_per_src[l6_pops[i]] == SYNAPSE_NMDA) {
            l6_nmda = true; break;
        }
    }
    ck_assert_msg(l6_nmda,
                  "No tier-6 (L6_project_*) → TRN pop-pair flagged as SYNAPSE_NMDA");

    /* CSR conn count from each family > 0. */
    uint64_t t0_count = count_syns_from(trn_pop, t0_pops, n_t0, SYNAPSE_NMDA);
    uint64_t l6_count = count_syns_from(trn_pop, l6_pops, n_l6, SYNAPSE_NMDA);
    ck_assert_msg(t0_count > 0,
                  "Tier-0 → TRN NMDA synapse count = 0");
    ck_assert_msg(l6_count > 0,
                  "L6_project → TRN NMDA synapse count = 0");
}
END_TEST

/* ============================================================================
 * Test 3: TRN output wired GABA_A back to every tier-0 pop
 * ============================================================================ */
START_TEST(test_trn_output_gaba_a)
{
    hier_ensure_built();

    int trn = find_pop_exact(g_hier_net, "thalamus_reticular");
    ck_assert_int_ge(trn, 0);
    uint32_t trn_idx = (uint32_t)trn;

    uint32_t t0_pops[16];
    uint32_t n_t0 = find_pops_by_prefix(g_hier_net, "input_", t0_pops, 16);
    ck_assert_msg(n_t0 > 0, "No tier-0 (input_*) pops found");

    /* Every tier-0 dst must have synapse_type_per_src[trn] == GABA_A. */
    uint64_t total_gaba = 0;
    for (uint32_t i = 0; i < n_t0; i++) {
        snn_population_t* dst = g_hier_net->populations[t0_pops[i]];
        ck_assert_msg(
            (synapse_type_t)dst->synapse_type_per_src[trn_idx] == SYNAPSE_GABA_A,
            "tier-0 pop %u (%s): TRN→pop synapse_type_per_src=%u, expected SYNAPSE_GABA_A=%u",
            t0_pops[i], dst->name,
            (unsigned)dst->synapse_type_per_src[trn_idx],
            (unsigned)SYNAPSE_GABA_A);
        total_gaba += count_syns_from_single(dst, trn_idx);
    }
    ck_assert_msg(total_gaba > 0,
                  "TRN → tier-0 outgoing synapse count = 0");
}
END_TEST

/* ============================================================================
 * Test 4: gating behaviour — TRN feedback measurably reduces relay firing
 * ============================================================================
 *
 * Mini network mirrors the TRN loop with an external driver pop. Driving
 * relay directly via external_current=100 mA bypasses the LIF integration
 * (the V is forced suprathreshold every step), so GABA_A inhibition cannot
 * counteract it. Instead, we drive relay through synaptic AMPA input from
 * a "driver" pop, which lets g_gaba_a from TRN actually pull V back down.
 *
 *   driver (50 LIF, brute-force-driven) ──AMPA──┐
 *                                                ├──→ relay ──AMPA──→ trn ──GABA_A──→ relay
 *
 * Phase A (open loop): driver→relay, but TRN clamped quiet each step (so
 *                       its GABA_A inhibition stays at zero). Relay fires
 *                       at the open-loop AMPA-driven rate.
 * Phase B (closed loop): same driver→relay, TRN free to integrate +
 *                        inhibit. Relay firing should drop measurably.
 *
 * Assert: phase B spike count <= 0.7 × phase A spike count.
 */
START_TEST(test_trn_gating_behavior)
{
    int driver = snn_network_add_population_lightweight(
        g_mini_net, 50, NEURON_GENERIC_LIF, "driver");
    int relay  = snn_network_add_population_lightweight(
        g_mini_net, 50, NEURON_GENERIC_LIF, "relay");
    int trn    = snn_network_add_population_lightweight(
        g_mini_net, 50, NEURON_GENERIC_LIF, "trn");
    ck_assert_int_ge(driver, 0);
    ck_assert_int_ge(relay,  0);
    ck_assert_int_ge(trn,    0);

    /* driver → relay AMPA: strong enough that AMPA alone keeps relay
     * spiking, but synaptic (not external-current) so GABA_A can actually
     * cancel some of the drive via E_gaba_a hyperpolarisation. 0.060 was
     * the calibrated value in test_top_down_feedback. */
    int n_dr = connect_dense(g_mini_net, driver, relay, SYNAPSE_AMPA, 0.060f);
    /* relay → trn AMPA: same strong dose so TRN reliably fires when relay
     * fires. */
    int n_rt = connect_dense(g_mini_net, relay,  trn,   SYNAPSE_AMPA, 0.060f);
    /* trn → relay GABA_A: large enough conductance to pull relay V down
     * below threshold despite ongoing AMPA drive. Sign follows the codebase
     * convention (per-receptor branch deposits absolute values). */
    int n_tr = connect_dense(g_mini_net, trn,    relay, SYNAPSE_GABA_A, -0.30f);
    ck_assert_int_gt(n_dr, 0);
    ck_assert_int_gt(n_rt, 0);
    ck_assert_int_gt(n_tr, 0);
    ck_assert_int_ge(snn_network_finalize_connections(g_mini_net), 0);

    snn_tune_set_conductance_enabled(1.0f);
    snn_tune_set_nmda_mg_mm(1.0f);

    const int N_STEPS = 100;

    /* Phase A: open loop. Brute-force driver every step (so it spikes at
     * 100% rate, providing a steady AMPA input to relay). Clamp TRN quiet
     * each step so its inhibition stays zero. */
    uint32_t spikes_open = 0;
    for (int s = 0; s < N_STEPS; s++) {
        drive_pop_all_spike(g_mini_net, driver);
        quiet_pop(g_mini_net, trn);
        snn_network_step(g_mini_net, 1.0f);
        spikes_open += count_spikes(g_mini_net, relay);
    }

    /* Reset relay/TRN state between phases so accumulated conductances
     * don't bias phase B. */
    quiet_pop(g_mini_net, relay);
    quiet_pop(g_mini_net, trn);

    /* Phase B: closed loop. Same driver, let TRN integrate. */
    uint32_t spikes_closed = 0;
    for (int s = 0; s < N_STEPS; s++) {
        drive_pop_all_spike(g_mini_net, driver);
        snn_network_step(g_mini_net, 1.0f);
        spikes_closed += count_spikes(g_mini_net, relay);
    }

    /* Open-loop must be solidly active so the comparison is meaningful. */
    ck_assert_msg(spikes_open >= 50,
                  "Open-loop relay spikes %u below 50 — AMPA dose mis-calibrated",
                  spikes_open);

    /* Gating must reduce relay firing by >= 30%. */
    ck_assert_msg((float)spikes_closed <= (float)spikes_open * 0.70f,
                  "TRN gating did not reduce relay firing >=30%%: "
                  "open=%u, closed=%u (need closed <= 0.70*open = %.1f)",
                  spikes_open, spikes_closed, (float)spikes_open * 0.70f);
}
END_TEST

/* ============================================================================
 * Test Suite
 * ============================================================================ */

static Suite* trn_suite(void)
{
    Suite* s = suite_create("SNN Reticular Thalamic Nucleus (P3.1)");

    /* Tests 1-3 share the long-lived hierarchy net (built lazily on first
     * call). No fixture — global. */
    TCase* tc_pop = tcase_create("TRN pop exists");
    tcase_add_test(tc_pop, test_trn_pop_exists);
    tcase_set_timeout(tc_pop, 600);
    suite_add_tcase(s, tc_pop);

    TCase* tc_in = tcase_create("TRN inputs NMDA");
    tcase_add_test(tc_in, test_trn_inputs_nmda);
    tcase_set_timeout(tc_in, 600);
    suite_add_tcase(s, tc_in);

    TCase* tc_out = tcase_create("TRN output GABA_A");
    tcase_add_test(tc_out, test_trn_output_gaba_a);
    tcase_set_timeout(tc_out, 600);
    suite_add_tcase(s, tc_out);

    /* Test 4 uses a fresh mini-net with full setup/teardown. */
    TCase* tc_gate = tcase_create("Gating behavior");
    tcase_add_checked_fixture(tc_gate, mini_setup, mini_teardown);
    tcase_add_test(tc_gate, test_trn_gating_behavior);
    tcase_set_timeout(tc_gate, 60);
    suite_add_tcase(s, tc_gate);

    return s;
}

int main(void)
{
    if (nimcp_init() != NIMCP_SUCCESS) {
        fprintf(stderr, "nimcp_init failed\n");
        return EXIT_FAILURE;
    }

    Suite* s = trn_suite();
    SRunner* sr = srunner_create(s);
    /* CK_NOFORK so tests 1-3 share the long-lived g_hier_net (forked tests
     * each get their own address space and would have to rebuild the
     * 1.8M-neuron hierarchy three times). */
    srunner_set_fork_status(sr, CK_NOFORK);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    if (g_hier_net) {
        snn_network_destroy(g_hier_net);
        g_hier_net = NULL;
    }

    nimcp_shutdown();
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
