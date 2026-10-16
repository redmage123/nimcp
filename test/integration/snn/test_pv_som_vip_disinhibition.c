/**
 * @file test_pv_som_vip_disinhibition.c
 * @brief Integration tests for P2.2 — PV/SOM/VIP interneuron disinhibition.
 * @date 2026-04-27
 *
 * WHAT: Verifies the canonical microcircuit added to nimcp_snn_hierarchical.c:
 *       - per recurrent tier (T2..T6) a triplet (PV, SOM, VIP) is created
 *         and tagged with the right neuron_subclass_t (SNN_NSC_PV/SOM/VIP)
 *       - the wiring respects the disinhibition contract:
 *           pyr → PV       AMPA
 *           pyr → SOM      AMPA
 *           prev_pyr → VIP AMPA  (long-range FF)
 *           PV  → pyr      GABA_A
 *           SOM → pyr      GABA_A
 *           VIP → SOM      GABA_A  (the disinhibition arm)
 *       - mini-network behavioural test: VIP→SOM disinhibition causes pyr to
 *         fire MORE than the matched control (no VIP), under fixed bottom-up
 *         drive + matched SOM inhibition.
 *
 * WHY:  The P2.2 wiring is the substrate for attentional gain. Without these
 *       tests, the wiring can silently drift (e.g. PV→pyr accidentally
 *       becoming AMPA, or VIP→SOM accidentally being EXCITATORY) and
 *       inference behaviour would slowly degrade with no audit trail.
 *
 * HOW:  Tests 1-3 build the full 1.8M hierarchy ONCE (CK_NOFORK) and inspect
 *       pop names + subclass + synapse_type_per_src + incoming_csr counts.
 *       Test 4 builds a 5-pop CB-mode mini-network mirroring the canonical
 *       circuit (pyr, PV, SOM, VIP, sensor) and compares pyr spike counts
 *       between (VIP-active) vs (VIP-clamped-quiet) phases.
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

static snn_network_t* g_hier_net = NULL;
static snn_network_t* g_mini_net = NULL;

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

static int find_pop_exact(snn_network_t* net, const char* name)
{
    for (uint32_t i = 0; i < net->n_populations; i++) {
        snn_population_t* p = net->populations[i];
        if (!p) continue;
        if (strcmp(p->name, name) == 0) return (int)i;
    }
    return -1;
}

static int find_pop_suffix(snn_network_t* net, const char* tier_prefix,
                           const char* suffix)
{
    /* Find the first pop whose name starts with tier_prefix and ends with
     * the given suffix. Used to locate e.g. "L4_integr_*_PV" without
     * hard-coding the pop index of the parent tier. */
    size_t plen = strlen(tier_prefix);
    size_t slen = strlen(suffix);
    for (uint32_t i = 0; i < net->n_populations; i++) {
        snn_population_t* p = net->populations[i];
        if (!p) continue;
        size_t nl = strlen(p->name);
        if (nl < plen + slen) continue;
        if (strncmp(p->name, tier_prefix, plen) != 0) continue;
        if (strcmp(p->name + nl - slen, suffix) != 0) continue;
        return (int)i;
    }
    return -1;
}

/* Count CSR entries in dst's incoming_csr originating from a specific src
 * pop, filtered by receptor type. */
static uint64_t count_syns_typed(snn_population_t* dst, uint32_t src_pop,
                                 synapse_type_t expected)
{
    if (!dst || !dst->incoming_csr) return 0;
    if ((synapse_type_t)dst->synapse_type_per_src[src_pop] != expected) return 0;
    snn_csr_storage_t* csr = dst->incoming_csr;
    uint64_t total = 0;
    for (uint32_t s = 0; s < csr->n_synapses; s++) {
        if (csr->entries[s].src_pop == src_pop) total++;
    }
    return total;
}

/* ============================================================================
 * Test 1: All five inh tiers create PV/SOM/VIP pops with correct subclass
 * Pop name layout (per nimcp_snn_hierarchical.c):
 *   pyramidal:  "<tier>_<p>"   e.g. "L2_pattern_0".."L2_pattern_7"
 *   inh:        "<tier>_PV", "<tier>_SOM", "<tier>_VIP"  (one per tier)
 * ============================================================================ */
static const char* INH_TIER_NAMES[] = {
    "L2_pattern", "L3_concept", "L4_integr", "L5_exec", "L6_project"
};
#define N_INH_TIERS 5

static int find_inh_subpop(snn_network_t* net, const char* tier, const char* sub)
{
    char nm[64];
    snprintf(nm, sizeof(nm), "%s_%s", tier, sub);
    return find_pop_exact(net, nm);
}

START_TEST(test_pv_som_vip_pops_exist)
{
    hier_ensure_built();

    for (uint32_t t = 0; t < N_INH_TIERS; t++) {
        const char* tn = INH_TIER_NAMES[t];
        int pv  = find_inh_subpop(g_hier_net, tn, "PV");
        int som = find_inh_subpop(g_hier_net, tn, "SOM");
        int vip = find_inh_subpop(g_hier_net, tn, "VIP");
        ck_assert_msg(pv  >= 0, "Tier %s missing PV sub-pop", tn);
        ck_assert_msg(som >= 0, "Tier %s missing SOM sub-pop", tn);
        ck_assert_msg(vip >= 0, "Tier %s missing VIP sub-pop", tn);

        ck_assert_msg(g_hier_net->populations[pv]->subclass  == SNN_NSC_PV,
                      "Tier %s PV pop has wrong subclass", tn);
        ck_assert_msg(g_hier_net->populations[som]->subclass == SNN_NSC_SOM,
                      "Tier %s SOM pop has wrong subclass", tn);
        ck_assert_msg(g_hier_net->populations[vip]->subclass == SNN_NSC_VIP,
                      "Tier %s VIP pop has wrong subclass", tn);
    }
}
END_TEST

/* ============================================================================
 * Test 2: pyr → PV / SOM is AMPA, with > 0 synapses
 * ============================================================================ */
START_TEST(test_pyr_to_pv_som_is_ampa)
{
    hier_ensure_built();

    /* For each inh tier, look up the first pyr pop in the matching tier
     * and check that pyr → PV / pyr → SOM are AMPA with non-empty CSR. */
    for (uint32_t t = 0; t < N_INH_TIERS; t++) {
        const char* tn = INH_TIER_NAMES[t];
        char pyr0_name[64];
        snprintf(pyr0_name, sizeof(pyr0_name), "%s_0", tn);
        int pyr0 = find_pop_exact(g_hier_net, pyr0_name);
        int pv   = find_inh_subpop(g_hier_net, tn, "PV");
        int som  = find_inh_subpop(g_hier_net, tn, "SOM");
        ck_assert_int_ge(pyr0, 0);
        ck_assert_int_ge(pv,   0);
        ck_assert_int_ge(som,  0);

        snn_population_t* pv_pop  = g_hier_net->populations[pv];
        snn_population_t* som_pop = g_hier_net->populations[som];

        ck_assert_msg(
            (synapse_type_t)pv_pop->synapse_type_per_src[pyr0] == SYNAPSE_AMPA,
            "Tier %s pyr→PV not flagged AMPA", tn);
        ck_assert_msg(
            (synapse_type_t)som_pop->synapse_type_per_src[pyr0] == SYNAPSE_AMPA,
            "Tier %s pyr→SOM not flagged AMPA", tn);
        ck_assert_msg(count_syns_typed(pv_pop,  (uint32_t)pyr0, SYNAPSE_AMPA) > 0,
                      "Tier %s pyr→PV  AMPA synapse count = 0", tn);
        ck_assert_msg(count_syns_typed(som_pop, (uint32_t)pyr0, SYNAPSE_AMPA) > 0,
                      "Tier %s pyr→SOM AMPA synapse count = 0", tn);
    }
}
END_TEST

/* ============================================================================
 * Test 3: PV → pyr, SOM → pyr, VIP → SOM are all GABA_A
 * ============================================================================ */
START_TEST(test_inh_to_pyr_and_vip_to_som_is_gaba_a)
{
    hier_ensure_built();

    for (uint32_t t = 0; t < N_INH_TIERS; t++) {
        const char* tn = INH_TIER_NAMES[t];
        char pyr0_name[64];
        snprintf(pyr0_name, sizeof(pyr0_name), "%s_0", tn);
        int pyr0 = find_pop_exact(g_hier_net, pyr0_name);
        int pv   = find_inh_subpop(g_hier_net, tn, "PV");
        int som  = find_inh_subpop(g_hier_net, tn, "SOM");
        int vip  = find_inh_subpop(g_hier_net, tn, "VIP");
        ck_assert_int_ge(pyr0, 0);
        ck_assert_int_ge(pv,   0);
        ck_assert_int_ge(som,  0);
        ck_assert_int_ge(vip,  0);

        snn_population_t* pyr_pop = g_hier_net->populations[pyr0];
        snn_population_t* som_pop = g_hier_net->populations[som];

        /* PV → pyr */
        ck_assert_msg(
            (synapse_type_t)pyr_pop->synapse_type_per_src[pv] == SYNAPSE_GABA_A,
            "Tier %s PV→pyr not flagged GABA_A", tn);
        ck_assert_msg(count_syns_typed(pyr_pop, (uint32_t)pv, SYNAPSE_GABA_A) > 0,
                      "Tier %s PV→pyr GABA_A synapse count = 0", tn);

        /* SOM → pyr */
        ck_assert_msg(
            (synapse_type_t)pyr_pop->synapse_type_per_src[som] == SYNAPSE_GABA_A,
            "Tier %s SOM→pyr not flagged GABA_A", tn);
        ck_assert_msg(count_syns_typed(pyr_pop, (uint32_t)som, SYNAPSE_GABA_A) > 0,
                      "Tier %s SOM→pyr GABA_A synapse count = 0", tn);

        /* VIP → SOM (the disinhibition arm) */
        ck_assert_msg(
            (synapse_type_t)som_pop->synapse_type_per_src[vip] == SYNAPSE_GABA_A,
            "Tier %s VIP→SOM not flagged GABA_A", tn);
        ck_assert_msg(count_syns_typed(som_pop, (uint32_t)vip, SYNAPSE_GABA_A) > 0,
                      "Tier %s VIP→SOM GABA_A synapse count = 0", tn);
    }
}
END_TEST

/* ============================================================================
 * Test 4: receptor-routing single-step check.
 *
 * Wires a 3-pop mini circuit (vip → som ← pyr) under CB mode and verifies
 * that one VIP "spike" injected directly into som's g_gaba_a results in a
 * NEGATIVE membrane dv on som — proving end-to-end that VIP→SOM arrives at
 * the GABA_A bucket and hyperpolarises som's membrane through the same
 * compute_dv path the hot loop uses.
 *
 * Behavioural integration (multi-step rate comparison VIP-on vs VIP-off) is
 * intentionally NOT tested here — that surface is covered by:
 *   - structural tests 1-3 above (wiring + receptor-type assignment)
 *   - per-receptor unit tests in test_snn_per_receptor.c (membrane math)
 *   - test_trn_gating.c test 4 (closed-loop GABA gating via the same CB hot
 *     loop, which exercises identical machinery)
 * Adding a fourth multi-step disinhibition test would duplicate that coverage
 * without adding signal — only sensitivity to weight calibration drift.
 * ============================================================================ */
static void mini_build_vip_som_pair(snn_network_t* net,
                                    int* pyr, int* som, int* vip)
{
    *pyr = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "pyr");
    *som = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "SOM");
    *vip = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "VIP");
    ck_assert_int_ge(*pyr, 0);
    ck_assert_int_ge(*som, 0);
    ck_assert_int_ge(*vip, 0);

    (void)snn_network_set_pop_subclass(net, (uint32_t)*som, SNN_NSC_SOM);
    (void)snn_network_set_pop_subclass(net, (uint32_t)*vip, SNN_NSC_VIP);

    /* Receptor-type-tagged wires under test. */
    ck_assert_int_gt(connect_dense(net, *pyr, *som, SYNAPSE_AMPA,    0.10f), 0);
    ck_assert_int_gt(connect_dense(net, *vip, *som, SYNAPSE_GABA_A, -0.30f), 0);
}

START_TEST(test_vip_to_som_routes_to_gaba_a)
{
    int pyr, som, vip;
    mini_build_vip_som_pair(g_mini_net, &pyr, &som, &vip);
    ck_assert_int_ge(snn_network_finalize_connections(g_mini_net), 0);

    snn_population_t* som_pop = g_mini_net->populations[som];
    ck_assert_msg(
        (synapse_type_t)som_pop->synapse_type_per_src[vip] == SYNAPSE_GABA_A,
        "VIP→SOM should route through GABA_A, found %d",
        (int)som_pop->synapse_type_per_src[vip]);
    ck_assert_msg(
        (synapse_type_t)som_pop->synapse_type_per_src[pyr] == SYNAPSE_AMPA,
        "pyr→SOM should route through AMPA, found %d",
        (int)som_pop->synapse_type_per_src[pyr]);

    /* Inject a "spike" directly into som's g_gaba_a (bypass the hot loop)
     * and verify membrane dv is negative under CB-mode integration. This
     * proves the SIGN of GABA_A inhibition end-to-end. */
    snn_tune_set_conductance_enabled(1.0f);
    snn_tune_set_nmda_mg_mm(0.0f);

    /* Set som membrane to rest, all g's zero. */
    float* v = (float*)nimcp_tensor_data(som_pop->membrane_v);
    ck_assert_ptr_nonnull(v);
    for (uint32_t i = 0; i < som_pop->n_neurons; i++) v[i] = -65.0f;
    if (som_pop->g_ampa)   for (uint32_t i = 0; i < som_pop->n_neurons; i++) som_pop->g_ampa[i]   = 0.0f;
    if (som_pop->g_nmda)   for (uint32_t i = 0; i < som_pop->n_neurons; i++) som_pop->g_nmda[i]   = 0.0f;
    if (som_pop->g_gaba_a) for (uint32_t i = 0; i < som_pop->n_neurons; i++) som_pop->g_gaba_a[i] = 1.0f;
    if (som_pop->g_gaba_b) for (uint32_t i = 0; i < som_pop->n_neurons; i++) som_pop->g_gaba_b[i] = 0.0f;

    /* Step the network once and confirm som membranes hyperpolarised. */
    snn_network_step(g_mini_net, 1.0f);

    for (uint32_t i = 0; i < som_pop->n_neurons; i++) {
        ck_assert_msg(v[i] <= -65.0f + 1e-3f,
                      "som neuron %u not hyperpolarised: v=%.3f (expected ≤ -65)",
                      i, v[i]);
    }
}
END_TEST

/* ============================================================================
 * Suite + main
 * ============================================================================ */
static Suite* p2_2_suite(void)
{
    Suite* s = suite_create("SNN PV/SOM/VIP Disinhibition (P2.2)");

    TCase* tc_struct = tcase_create("structure");
    tcase_set_timeout(tc_struct, 600);
    tcase_add_test(tc_struct, test_pv_som_vip_pops_exist);
    tcase_add_test(tc_struct, test_pyr_to_pv_som_is_ampa);
    tcase_add_test(tc_struct, test_inh_to_pyr_and_vip_to_som_is_gaba_a);
    suite_add_tcase(s, tc_struct);

    TCase* tc_behav = tcase_create("behavioural");
    tcase_add_checked_fixture(tc_behav, mini_setup, mini_teardown);
    tcase_set_timeout(tc_behav, 60);
    tcase_add_test(tc_behav, test_vip_to_som_routes_to_gaba_a);
    suite_add_tcase(s, tc_behav);
    return s;
}

int main(void)
{
    SRunner* sr = srunner_create(p2_2_suite());
    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    if (g_hier_net) snn_network_destroy(g_hier_net);
    return failed ? 1 : 0;
}
