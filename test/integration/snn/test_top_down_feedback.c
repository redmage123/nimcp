/**
 * @file test_top_down_feedback.c
 * @brief Integration tests for P1.1 — top-down NMDA feedback projections.
 * @date 2026-04-26
 *
 * WHAT: Verifies the new descending-pathway block in nimcp_snn_hierarchical.c
 *       (L5_exec → L3_concept and L6_project → L2_pattern) is wired with
 *       SYNAPSE_NMDA, produces a non-zero number of synapses, is silent at
 *       v_rest (Mg²⁺ block) and provides a measurable boost when paired
 *       with bottom-up AMPA depolarisation (coincidence detection).
 *
 * WHY:  Predictive coding / active inference / Bayesian brain at the
 *       substrate level requires descending projections. Without these
 *       wires the cortical hierarchy can only do pure bottom-up sensory
 *       accumulation. Tests pin the pathway is present, NMDA-typed, and
 *       behaves like a coincidence detector (Larkum 2013).
 *
 * HOW:  Test 1+2 build the full 1.8M hierarchy and inspect the wiring
 *       tables / CSR storage. Tests 3+4 build a small mini-network that
 *       reproduces the L5→L3 NMDA + L2→L3 AMPA topology to check the
 *       runtime behaviour without paying the 1.8M build cost.
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

/* SNN tunables (extern, no header). Mirrors test_snn_per_receptor_integration.c. */
extern void  snn_tune_set_conductance_enabled(float);
extern void  snn_tune_set_cb_weights_rescaled(float);
extern void  snn_tune_set_noise_rate_hz(float);
extern void  snn_tune_set_basket_enabled(float);
extern void  snn_tune_set_ahp_enabled(float);
extern void  snn_tune_set_pump_enabled(float);
extern void  snn_tune_set_substrate_enabled(float);
extern void  snn_tune_set_nmda_mg_mm(float);
extern void  snn_tune_set_tau_ampa_ms(float);
extern void  snn_tune_set_tau_nmda_ms(float);
extern void  snn_tune_set_tau_gaba_a_ms(float);
extern void  snn_tune_set_tau_gaba_b_ms(float);
extern void  snn_tune_set_e_ampa_mv(float);
extern void  snn_tune_set_e_nmda_mv(float);
extern void  snn_tune_set_e_gaba_a_mv(float);
extern void  snn_tune_set_e_gaba_b_mv(float);

/* ============================================================================
 * Globals + fixtures
 * ============================================================================ */

static snn_network_t* g_hier_net = NULL;  /* shared across tests 1+2 — full
                                             hierarchy build is ~minute-class
                                             so we build once. */
static snn_network_t* g_mini_net = NULL;  /* fresh per-test for 3+4. */

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

/* Mini-net fixture (tests 3+4). */
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

/* Hierarchy (tests 1+2): build at startup and never touch tunables. */
static void hier_ensure_built(void)
{
    if (g_hier_net) return;
    /* 1.8M target as in production. n_inputs/n_outputs match a small input
     * dim — we don't drive the full network, just inspect its wiring. */
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
}

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

/* Find populations whose name starts with `prefix` (e.g. "L5_exec_"). Fills
 * `out` up to `max` and returns the count. */
static uint32_t find_pops_by_prefix(snn_network_t* net,
                                    const char* prefix,
                                    uint32_t* out, uint32_t max)
{
    uint32_t n = 0;
    size_t pl = strlen(prefix);
    for (uint32_t i = 0; i < net->n_populations && n < max; i++) {
        snn_population_t* p = net->populations[i];
        if (!p) continue;
        if (strncmp(p->name, prefix, pl) == 0) {
            out[n++] = i;
        }
    }
    return n;
}

/* Count how many entries in dst's incoming CSR have src_pop ∈ src_set AND
 * the dst's per-src receptor table marks src_pop as NMDA. */
static uint64_t count_nmda_synapses_from(snn_population_t* dst,
                                         const uint32_t* src_set,
                                         uint32_t n_src)
{
    if (!dst || !dst->incoming_csr) return 0;
    snn_csr_storage_t* csr = dst->incoming_csr;
    uint64_t total = 0;
    for (uint32_t s = 0; s < csr->n_synapses; s++) {
        uint32_t sp = csr->entries[s].src_pop;
        if ((synapse_type_t)dst->synapse_type_per_src[sp] != SYNAPSE_NMDA)
            continue;
        for (uint32_t k = 0; k < n_src; k++) {
            if (src_set[k] == sp) { total++; break; }
        }
    }
    return total;
}

/* ============================================================================
 * Test 1: Pathway exists — L5→L3 and L6→L2 are wired as SYNAPSE_NMDA
 * ============================================================================ */
START_TEST(test_top_down_pathway_exists)
{
    hier_ensure_built();

    uint32_t l5_pops[16], l3_pops[16], l6_pops[16], l2_pops[16];
    uint32_t n_l5 = find_pops_by_prefix(g_hier_net, "L5_exec_",   l5_pops, 16);
    uint32_t n_l3 = find_pops_by_prefix(g_hier_net, "L3_concept_", l3_pops, 16);
    uint32_t n_l6 = find_pops_by_prefix(g_hier_net, "L6_project_", l6_pops, 16);
    uint32_t n_l2 = find_pops_by_prefix(g_hier_net, "L2_pattern_", l2_pops, 16);

    ck_assert_msg(n_l5 > 0 && n_l3 > 0,
                  "L5_exec or L3_concept not found (n_l5=%u n_l3=%u)", n_l5, n_l3);
    ck_assert_msg(n_l6 > 0 && n_l2 > 0,
                  "L6_project or L2_pattern not found (n_l6=%u n_l2=%u)", n_l6, n_l2);

    /* For at least one L3 dst, at least one L5 src must be marked NMDA. */
    bool found_l5_l3 = false;
    for (uint32_t di = 0; di < n_l3 && !found_l5_l3; di++) {
        snn_population_t* dst = g_hier_net->populations[l3_pops[di]];
        for (uint32_t si = 0; si < n_l5; si++) {
            if ((synapse_type_t)dst->synapse_type_per_src[l5_pops[si]] == SYNAPSE_NMDA) {
                found_l5_l3 = true; break;
            }
        }
    }
    ck_assert_msg(found_l5_l3,
                  "No L5_exec_*→L3_concept_* pop-pair flagged as SYNAPSE_NMDA");

    /* Same for L6→L2. */
    bool found_l6_l2 = false;
    for (uint32_t di = 0; di < n_l2 && !found_l6_l2; di++) {
        snn_population_t* dst = g_hier_net->populations[l2_pops[di]];
        for (uint32_t si = 0; si < n_l6; si++) {
            if ((synapse_type_t)dst->synapse_type_per_src[l6_pops[si]] == SYNAPSE_NMDA) {
                found_l6_l2 = true; break;
            }
        }
    }
    ck_assert_msg(found_l6_l2,
                  "No L6_project_*→L2_pattern_* pop-pair flagged as SYNAPSE_NMDA");
}
END_TEST

/* ============================================================================
 * Test 2: Top-down connection count is non-zero on a known L3 pop
 * ============================================================================ */
START_TEST(test_top_down_connection_count_nonzero)
{
    hier_ensure_built();

    uint32_t l5_pops[16], l3_pops[16];
    uint32_t n_l5 = find_pops_by_prefix(g_hier_net, "L5_exec_",   l5_pops, 16);
    uint32_t n_l3 = find_pops_by_prefix(g_hier_net, "L3_concept_", l3_pops, 16);
    ck_assert_int_gt(n_l5, 0);
    ck_assert_int_gt(n_l3, 0);

    /* Sum over all L3 pops — at least one must have NMDA inputs from L5. */
    uint64_t total_l5_nmda = 0;
    for (uint32_t di = 0; di < n_l3; di++) {
        snn_population_t* dst = g_hier_net->populations[l3_pops[di]];
        total_l5_nmda += count_nmda_synapses_from(dst, l5_pops, n_l5);
    }
    ck_assert_msg(total_l5_nmda > 0,
                  "L5_exec → L3_concept NMDA synapse total = 0 (top-down block didn't wire)");

    /* Also check L6 → L2. */
    uint32_t l6_pops[16], l2_pops[16];
    uint32_t n_l6 = find_pops_by_prefix(g_hier_net, "L6_project_", l6_pops, 16);
    uint32_t n_l2 = find_pops_by_prefix(g_hier_net, "L2_pattern_", l2_pops, 16);
    ck_assert_int_gt(n_l6, 0);
    ck_assert_int_gt(n_l2, 0);

    uint64_t total_l6_nmda = 0;
    for (uint32_t di = 0; di < n_l2; di++) {
        snn_population_t* dst = g_hier_net->populations[l2_pops[di]];
        total_l6_nmda += count_nmda_synapses_from(dst, l6_pops, n_l6);
    }
    ck_assert_msg(total_l6_nmda > 0,
                  "L6_project → L2_pattern NMDA synapse total = 0");
}
END_TEST

/* ============================================================================
 * Test 3: Top-down NMDA at rest is silent (mini-network, full Mg block)
 * ============================================================================
 *
 * We build a minimal L5-like → L3-like NMDA pathway inside the mini-net.
 * With the postsyn pop sitting at v_rest and no AMPA priming, the Mg²⁺
 * block must keep top-down ineffective: zero spikes from NMDA-only drive.
 */
START_TEST(test_top_down_silent_at_rest)
{
    int l5 = snn_network_add_population_lightweight(g_mini_net, 8,  NEURON_GENERIC_LIF, "L5_mini");
    int l3 = snn_network_add_population_lightweight(g_mini_net, 60, NEURON_GENERIC_LIF, "L3_mini");
    ck_assert_int_ge(l5, 0);
    ck_assert_int_ge(l3, 0);

    /* Match P1.1 receptor (NMDA) — weight calibrated so unblocked drive
     * could fire (prevents the test from being a no-op) but blocked drive
     * is well subthreshold. 8 × 0.05 × 0.06 × 65 ≈ 1.6 mV at rest. */
    int n = connect_dense(g_mini_net, l5, l3, SYNAPSE_NMDA, 0.05f);
    ck_assert_int_gt(n, 0);
    ck_assert_int_ge(snn_network_finalize_connections(g_mini_net), 0);

    snn_tune_set_conductance_enabled(1.0f);
    snn_tune_set_nmda_mg_mm(1.0f);

    uint32_t total_post_spikes = 0;
    for (int s = 0; s < 30; s++) {
        drive_pop_all_spike(g_mini_net, l5);
        snn_network_step(g_mini_net, 1.0f);
        total_post_spikes += count_spikes(g_mini_net, l3);
    }
    ck_assert_msg(total_post_spikes == 0,
                  "Top-down NMDA at rest fired %u spikes (expected 0 — Mg block)",
                  total_post_spikes);
}
END_TEST

/* ============================================================================
 * Test 4: Top-down + bottom-up coincidence ≥ 1.10× bottom-up alone
 * ============================================================================
 *
 * Mini-network mirrors the real hierarchy:
 *    L2_mini (bottom-up AMPA) ─┐
 *                              ├──→ L3_mini
 *    L5_mini (top-down NMDA) ──┘
 *
 * Phase A: only L2 fires → L3 spikes some baseline N_baseline > 0.
 * Phase B: both L2 + L5 fire → AMPA depolarises L3, Mg block opens, NMDA
 *          contributes additional drive → L3 spikes more.
 *
 * Assert: spikes_with_top_down > spikes_without * 1.1.
 */
START_TEST(test_top_down_coincidence_boost)
{
    int l5 = snn_network_add_population_lightweight(g_mini_net, 8,  NEURON_GENERIC_LIF, "L5_mini");
    int l2 = snn_network_add_population_lightweight(g_mini_net, 8,  NEURON_GENERIC_LIF, "L2_mini");
    int l3 = snn_network_add_population_lightweight(g_mini_net, 60, NEURON_GENERIC_LIF, "L3_mini");
    ck_assert_int_ge(l5, 0);
    ck_assert_int_ge(l2, 0);
    ck_assert_int_ge(l3, 0);

    /* AMPA — strong enough that bottom-up alone fires a measurable baseline,
     * leaving headroom for NMDA-driven boost. With 8 presyn × 0.060 weight,
     * v_ss = -65 / (1 + 0.48) ≈ -44 mV → suprathreshold (-50 mV thresh). */
    int na = connect_dense(g_mini_net, l2, l3, SYNAPSE_AMPA, 0.060f);
    int nn = connect_dense(g_mini_net, l5, l3, SYNAPSE_NMDA, 0.30f);
    ck_assert_int_gt(na, 0);
    ck_assert_int_gt(nn, 0);
    ck_assert_int_ge(snn_network_finalize_connections(g_mini_net), 0);

    snn_tune_set_conductance_enabled(1.0f);
    snn_tune_set_nmda_mg_mm(1.0f);

    /* Phase A: bottom-up only. */
    uint32_t spikes_no_top_down = 0;
    for (int s = 0; s < 30; s++) {
        drive_pop_all_spike(g_mini_net, l2);
        quiet_pop(g_mini_net, l5);
        snn_network_step(g_mini_net, 1.0f);
        spikes_no_top_down += count_spikes(g_mini_net, l3);
    }
    reset_pop_state(g_mini_net, l3);

    /* Phase B: bottom-up + top-down. */
    uint32_t spikes_with_top_down = 0;
    for (int s = 0; s < 30; s++) {
        drive_pop_all_spike(g_mini_net, l2);
        drive_pop_all_spike(g_mini_net, l5);
        snn_network_step(g_mini_net, 1.0f);
        spikes_with_top_down += count_spikes(g_mini_net, l3);
    }

    /* Both must fire at least some spikes (otherwise the AMPA dose was
     * miscalibrated and the test is uninformative). */
    ck_assert_msg(spikes_no_top_down > 0,
                  "Bottom-up alone fired 0 spikes — AMPA weight too low for test");

    /* The increment must be measurable: ≥ 10% boost. */
    ck_assert_msg((float)spikes_with_top_down > (float)spikes_no_top_down * 1.10f,
                  "Top-down did not boost firing: bu=%u, bu+td=%u (need bu+td > 1.10×bu)",
                  spikes_no_top_down, spikes_with_top_down);
}
END_TEST

/* ============================================================================
 * Test Suite
 * ============================================================================ */

static Suite* top_down_suite(void)
{
    Suite* s = suite_create("SNN Top-Down Feedback (P1.1)");

    /* Tests 1 + 2 share a long-lived hierarchy net (built lazily inside
     * each test; first call pays the build cost). No fixture — global. */
    TCase* tc_path = tcase_create("Pathway exists");
    tcase_add_test(tc_path, test_top_down_pathway_exists);
    tcase_set_timeout(tc_path, 600); /* hierarchy build can take a while */
    suite_add_tcase(s, tc_path);

    TCase* tc_count = tcase_create("Connection count nonzero");
    tcase_add_test(tc_count, test_top_down_connection_count_nonzero);
    tcase_set_timeout(tc_count, 600);
    suite_add_tcase(s, tc_count);

    /* Tests 3 + 4 use a fresh mini-net with full setup/teardown. */
    TCase* tc_silent = tcase_create("Silent at rest");
    tcase_add_checked_fixture(tc_silent, mini_setup, mini_teardown);
    tcase_add_test(tc_silent, test_top_down_silent_at_rest);
    tcase_set_timeout(tc_silent, 30);
    suite_add_tcase(s, tc_silent);

    TCase* tc_coinc = tcase_create("Coincidence boost");
    tcase_add_checked_fixture(tc_coinc, mini_setup, mini_teardown);
    tcase_add_test(tc_coinc, test_top_down_coincidence_boost);
    tcase_set_timeout(tc_coinc, 30);
    suite_add_tcase(s, tc_coinc);

    return s;
}

int main(void)
{
    if (nimcp_init() != NIMCP_SUCCESS) {
        fprintf(stderr, "nimcp_init failed\n");
        return EXIT_FAILURE;
    }

    Suite* s = top_down_suite();
    SRunner* sr = srunner_create(s);
    /* CK_NOFORK so we can share g_hier_net across tests 1+2 (forked tests
     * each get their own address space and would have to rebuild the
     * 1.8M-neuron hierarchy 4 times). */
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
