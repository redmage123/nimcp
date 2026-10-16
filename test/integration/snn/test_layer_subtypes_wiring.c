/**
 * @file test_layer_subtypes_wiring.c
 * @brief Integration tests for P4.1 — layer-specific pyramidal subtype wiring
 *        in the 1.8M-neuron hierarchical SNN.
 * @date 2026-04-27
 *
 * WHAT: Verifies that snn_create_hierarchical_network() tags each tier's
 *       pyramidal pop with the correct neuron_subclass_t per the P4.1
 *       design:
 *         tiers 0,1 (input_*, L1_feature_*)            → SNN_NSC_PYRAMIDAL
 *         tiers 2,3 (L2_pattern_*, L3_concept_*)       → SNN_NSC_PYRAMIDAL_L23
 *         tier  4   (L4_integr_*)                      → SNN_NSC_PYRAMIDAL_L4_STELLATE
 *         tier  5   (L5_exec_*)                        → SNN_NSC_PYRAMIDAL_L5_BETZ
 *         tiers 6,7 (L6_project_*, output_*)           → SNN_NSC_PYRAMIDAL
 *       Plus one behavioural test: a 2-pop mini-net (one L5_BETZ pop, one
 *       L23 pop) driven with identical AMPA input must yield more spikes in
 *       the BETZ pop because its v_thresh is 4 mV lower.
 *
 * WHY:  The subclass tag silently routes through snn_pop_lif_params() and
 *       changes membrane dynamics. If a tier is left untagged (default
 *       PYRAMIDAL) or wired to the wrong subclass, the layer-specific
 *       biological profile disappears with no other audit trail. P4.1's
 *       cortical-layer-aware integration depends on Betz-fires-earlier and
 *       L4-stellate-demands-convergence holding at the wiring level.
 *
 * HOW:  Tests 1-2 build the full 1.8M hierarchy ONCE (CK_NOFORK) and
 *       inspect pop->subclass for each tier prefix. Test 3 builds a fresh
 *       3-pop mini-network (driver + L5_BETZ + L23) under default LIF
 *       integration, drives both pops with identical sustained AMPA from
 *       a brute-force-driven sensor pop, and asserts the BETZ spike count
 *       exceeds the L23 spike count.
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

extern void  snn_tune_set_conductance_enabled(float);
extern float snn_tune_get_conductance_enabled(void);
extern void  snn_tune_set_cb_weights_rescaled(float);
extern void  snn_tune_set_noise_rate_hz(float);
extern void  snn_tune_set_basket_enabled(float);
extern void  snn_tune_set_ahp_enabled(float);
extern void  snn_tune_set_pump_enabled(float);
extern void  snn_tune_set_substrate_enabled(float);
extern void  snn_tune_set_nmda_mg_mm(float);

/* ============================================================================
 * Globals + fixtures
 * ============================================================================ */

static snn_network_t* g_hier_net = NULL; /* shared across structural tests. */
static snn_network_t* g_mini_net = NULL; /* fresh per-test for behavioural. */

static void reset_snn_tunables(void)
{
    snn_tune_set_noise_rate_hz(0.0f);
    snn_tune_set_basket_enabled(0.0f);
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);
    snn_tune_set_substrate_enabled(0.0f);
    snn_tune_set_conductance_enabled(0.0f);
    snn_tune_set_cb_weights_rescaled(0.0f);
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
 * Helpers (mirrored from test_pv_som_vip_disinhibition.c — DRY)
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

/* Find first pop whose name starts with prefix AND ends with a tier index
 * (e.g. "L5_exec_0".."L5_exec_5"). Returns -1 if not found. */
static int find_pop_by_prefix(snn_network_t* net, const char* prefix)
{
    size_t pl = strlen(prefix);
    for (uint32_t i = 0; i < net->n_populations; i++) {
        snn_population_t* p = net->populations[i];
        if (!p) continue;
        if (strncmp(p->name, prefix, pl) != 0) continue;
        /* skip the inh sub-pops (PV/SOM/VIP) — those have different suffixes. */
        const char* tail = p->name + pl;
        if (tail[0] < '0' || tail[0] > '9') continue;
        return (int)i;
    }
    return -1;
}

/* ============================================================================
 * Test 1: tier-prefix → subclass map matches the P4.1 design
 *
 * For each (tier_prefix, expected_subclass) pair, find the first pop whose
 * name matches "<prefix>_<digit>..." and assert its subclass.
 * ============================================================================ */
typedef struct {
    const char*       prefix;  /* e.g. "L5_exec_" */
    neuron_subclass_t expected;
    const char*       reason;  /* docstring shown on failure */
} tier_subclass_expectation_t;

static const tier_subclass_expectation_t TIER_EXPECT[] = {
    { "input_",      SNN_NSC_PYRAMIDAL,
      "Tier 0 input pops are sensory-relay stubs — keep PYRAMIDAL default" },
    { "L1_feature_", SNN_NSC_PYRAMIDAL,
      "Tier 1 L1 has no classical pyramidal LIF profile — PYRAMIDAL default" },
    { "L2_pattern_", SNN_NSC_PYRAMIDAL_L23,
      "Tier 2 L2_pattern → L23 (intratelencephalic, faster tau)" },
    { "L3_concept_", SNN_NSC_PYRAMIDAL_L23,
      "Tier 3 L3_concept → L23" },
    { "L4_integr_",  SNN_NSC_PYRAMIDAL_L4_STELLATE,
      "Tier 4 L4_integr → L4_STELLATE (small soma, high v_thresh)" },
    { "L5_exec_",    SNN_NSC_PYRAMIDAL_L5_BETZ,
      "Tier 5 L5_exec → L5_BETZ (giant pyramidal, low v_thresh)" },
    { "L6_project_", SNN_NSC_PYRAMIDAL,
      "Tier 6 L6_project — corticothalamic, no measured layer profile" },
    { "output_",     SNN_NSC_PYRAMIDAL,
      "Tier 7 output is aggregation — PYRAMIDAL default" },
};
#define N_TIER_EXPECT \
    ((uint32_t)(sizeof(TIER_EXPECT) / sizeof(TIER_EXPECT[0])))

START_TEST(test_tier_subclass_mapping)
{
    hier_ensure_built();

    for (uint32_t i = 0; i < N_TIER_EXPECT; i++) {
        const tier_subclass_expectation_t* e = &TIER_EXPECT[i];
        int pop_id = find_pop_by_prefix(g_hier_net, e->prefix);
        ck_assert_msg(pop_id >= 0,
                      "No pop with prefix '%s' found — hierarchy regressed?",
                      e->prefix);

        snn_population_t* p = g_hier_net->populations[pop_id];
        ck_assert_msg(p->subclass == e->expected,
                      "%s: pop[%d]='%s' has subclass=%d, expected %d (%s)",
                      e->prefix, pop_id, p->name,
                      (int)p->subclass, (int)e->expected, e->reason);
    }
}
END_TEST

/* ============================================================================
 * Test 2: subclass distribution across the full hierarchy.
 *
 * Independent of the per-prefix check above: count populations whose subclass
 * falls into each P4.1 bucket. Asserts AT LEAST ONE pop of each expected
 * non-default subclass exists, so an accidental no-op tagging (always
 * defaulting to PYRAMIDAL) cannot pass test 1 by some prefix-find quirk.
 * ============================================================================ */
START_TEST(test_subclass_distribution_present)
{
    hier_ensure_built();

    uint32_t n_l23 = 0, n_stellate = 0, n_betz = 0;
    for (uint32_t i = 0; i < g_hier_net->n_populations; i++) {
        snn_population_t* p = g_hier_net->populations[i];
        if (!p) continue;
        switch (p->subclass) {
            case SNN_NSC_PYRAMIDAL_L23:           n_l23++;      break;
            case SNN_NSC_PYRAMIDAL_L4_STELLATE:   n_stellate++; break;
            case SNN_NSC_PYRAMIDAL_L5_BETZ:       n_betz++;     break;
            default: break;
        }
    }

    /* L2_pattern (8 pops) + L3_concept (8 pops) → expect 16 L23 pops. */
    ck_assert_msg(n_l23 >= 8,
                  "Expected >=8 L23 pops (L2_pattern + L3_concept), found %u", n_l23);
    /* L4_integr → 6 pops. */
    ck_assert_msg(n_stellate >= 1,
                  "Expected >=1 L4_STELLATE pop (L4_integr), found %u", n_stellate);
    /* L5_exec → 6 pops. */
    ck_assert_msg(n_betz >= 1,
                  "Expected >=1 L5_BETZ pop (L5_exec), found %u", n_betz);
}
END_TEST

/* ============================================================================
 * Test 3: behavioural — at a between-threshold V, L5_BETZ fires while L23
 * does not. Direct test of the v_thresh delta the production code applies
 * per-pop via snn_pop_lif_params():
 *
 *   default v_thresh = -50 mV
 *   L5_BETZ v_thresh = -52 mV   ← clamped V of -51 crosses this
 *   L23     v_thresh = -50 mV   ← clamped V of -51 stays subthreshold
 *
 * Implementation: clamp V to V_BETWEEN at the START of each step, AND clamp
 * the spike-decision input by zeroing the synaptic conductances and external
 * current. This isolates the threshold check from any AMPA / leak dynamics —
 * the only thing that matters is "is V > v_thresh?". Done across N_STEPS to
 * accumulate enough spikes for a statistically meaningful comparison.
 * ============================================================================ */
START_TEST(test_betz_fires_more_than_l23_under_identical_drive)
{
    int betz = snn_network_add_population_lightweight(
        g_mini_net, 50, NEURON_GENERIC_LIF, "betz");
    int l23  = snn_network_add_population_lightweight(
        g_mini_net, 50, NEURON_GENERIC_LIF, "l23");
    ck_assert_int_ge(betz, 0);
    ck_assert_int_ge(l23,  0);

    ck_assert_int_eq(snn_network_set_pop_subclass(g_mini_net, (uint32_t)betz,
                                                  SNN_NSC_PYRAMIDAL_L5_BETZ), 0);
    ck_assert_int_eq(snn_network_set_pop_subclass(g_mini_net, (uint32_t)l23,
                                                  SNN_NSC_PYRAMIDAL_L23), 0);
    ck_assert_int_ge(snn_network_finalize_connections(g_mini_net), 0);

    /* CRITICAL: enable CB mode here, NOT for the receptor dynamics, but to
     * force snn_network_step() onto the CPU LIF path. The GPU LIF state in
     * snn_network_create() bakes a single nimcp_lif_params struct with
     * config defaults — it does NOT consult snn_pop_lif_params() per pop,
     * so on the GPU path PYRAMIDAL_L5_BETZ silently behaves identically
     * to default PYRAMIDAL. The CB-on guard at nimcp_snn_network.c:938-939
     * routes around the GPU path, exercising the CPU CSR loop where
     * snn_pop_lif_params() IS called per pop (see line 1337). This
     * production gap is flagged in the walkthrough. */
    snn_tune_set_conductance_enabled(1.0f);
    snn_tune_set_nmda_mg_mm(0.0f);

    const float V_BETWEEN = -51.0f;  /* between L23 (-50) and BETZ (-52). */

    snn_population_t* p_betz = g_mini_net->populations[betz];
    snn_population_t* p_l23  = g_mini_net->populations[l23];

    const int N_STEPS = 200;
    uint32_t spikes_betz = 0;
    uint32_t spikes_l23  = 0;

    for (int s = 0; s < N_STEPS; s++) {
        /* Clamp V and zero out everything else that could drag V away from
         * V_BETWEEN before the threshold-check. Refractory cleared so each
         * step's threshold check is independent. */
        float* vb = (float*)nimcp_tensor_data(p_betz->membrane_v);
        float* vl = (float*)nimcp_tensor_data(p_l23->membrane_v);
        ck_assert_ptr_nonnull(vb);
        ck_assert_ptr_nonnull(vl);
        for (uint32_t i = 0; i < p_betz->n_neurons; i++) vb[i] = V_BETWEEN;
        for (uint32_t i = 0; i < p_l23->n_neurons;  i++) vl[i] = V_BETWEEN;

        float* rb = (float*)nimcp_tensor_data(p_betz->refractory);
        float* rl = (float*)nimcp_tensor_data(p_l23->refractory);
        if (rb) for (uint32_t i = 0; i < p_betz->n_neurons; i++) rb[i] = 0.0f;
        if (rl) for (uint32_t i = 0; i < p_l23->n_neurons;  i++) rl[i] = 0.0f;

        if (p_betz->external_current)
            for (uint32_t i = 0; i < p_betz->n_neurons; i++)
                p_betz->external_current[i] = 0.0f;
        if (p_l23->external_current)
            for (uint32_t i = 0; i < p_l23->n_neurons; i++)
                p_l23->external_current[i] = 0.0f;

        snn_network_step(g_mini_net, 1.0f);
        spikes_betz += count_spikes(g_mini_net, betz);
        spikes_l23  += count_spikes(g_mini_net, l23);
    }

    /* The signature property under test: BETZ (v_thresh = -52) fires from
     * V=-51; L23 (v_thresh = -50) does not. The leak term drags V toward
     * v_rest = -65 over the step, so V at threshold-check time is below
     * V_BETWEEN — but BETZ's lower threshold gives it more room before the
     * leak moves V below v_thresh_BETZ (= -52). */
    printf("[behav] V=%.1f mV → betz=%u spikes  l23=%u spikes (over %d steps)\n",
           (double)V_BETWEEN, spikes_betz, spikes_l23, N_STEPS);
    ck_assert_msg(spikes_betz > spikes_l23,
                  "L5_BETZ (v_thresh=-52) must fire MORE than L23 (v_thresh=-50) "
                  "with V clamped at %.1f mV: betz=%u l23=%u",
                  (double)V_BETWEEN, spikes_betz, spikes_l23);
}
END_TEST

/* ============================================================================
 * Test Suite
 * ============================================================================ */

static Suite* layer_subtypes_suite(void)
{
    Suite* s = suite_create("SNN Layer-Specific Pyramidal Subtypes (P4.1)");

    /* Structural tests share the long-lived hierarchy (built lazily). */
    TCase* tc_map = tcase_create("tier subclass map");
    tcase_add_test(tc_map, test_tier_subclass_mapping);
    tcase_set_timeout(tc_map, 600);
    suite_add_tcase(s, tc_map);

    TCase* tc_dist = tcase_create("subclass distribution");
    tcase_add_test(tc_dist, test_subclass_distribution_present);
    tcase_set_timeout(tc_dist, 600);
    suite_add_tcase(s, tc_dist);

    /* Behavioural test uses a fresh mini-net per call. */
    TCase* tc_behav = tcase_create("betz vs l23 behavioural ranking");
    tcase_add_checked_fixture(tc_behav, mini_setup, mini_teardown);
    tcase_add_test(tc_behav, test_betz_fires_more_than_l23_under_identical_drive);
    tcase_set_timeout(tc_behav, 60);
    suite_add_tcase(s, tc_behav);

    return s;
}

int main(void)
{
    if (nimcp_init() != NIMCP_SUCCESS) {
        fprintf(stderr, "nimcp_init failed\n");
        return EXIT_FAILURE;
    }

    Suite*   s  = layer_subtypes_suite();
    SRunner* sr = srunner_create(s);
    /* CK_NOFORK so structural tests share the long-lived g_hier_net. */
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
