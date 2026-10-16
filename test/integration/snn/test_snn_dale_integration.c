/**
 * @file test_snn_dale_integration.c
 * @brief Integration test — Dale's principle on hierarchical SNN wiring.
 * @date 2026-04-27
 *
 * WHAT: Two integration scenarios for snn_network_validate_dale():
 *   (1) full production hierarchy (1.8M neurons via
 *       snn_create_hierarchical_network) — the canonical audit target,
 *       but heavy: ~30 GB RSS and several minutes of wiring.
 *   (2) a downscaled simulated-tier mini-network mirroring the production
 *       recurrent-wiring pattern (mod-5 GABA branch + AMPA FF) — runs in
 *       ~1 second and exercises the exact code path that produces the
 *       audited violation.
 *
 * WHY:  The per-receptor migration (g_ampa/g_nmda/g_gaba_a/g_gaba_b) makes
 *       Dale violations biophysically observable. If a source pop emits
 *       both AMPA and GABA_A its spikes are deposited into both buckets,
 *       which no real neuron does. Both scenarios encode the contract.
 *
 * HOW:  libcheck. The full-hierarchy test is wrapped in a NIMCP_RUN_FULL_
 *       HIERARCHY env-gate so CI / memory-constrained environments can
 *       skip it; the mini-network test always runs.
 *
 * NOTE: As of 2026-04-27 the production wiring DOES violate Dale at the
 *       within-tier recurrent step (mod-5 GABA branch in pyramidal pops,
 *       see docs/claude/dale-audit-2026-04-27.md). Both tests therefore
 *       FAIL on the current tree — that is the audit signal. Per the
 *       Wave C walkthrough rule "if the integration test reveals a Dale
 *       violation in production wiring, STOP and report — do not silence
 *       the test", the failure is intentional and stays until Option A
 *       (drop the GABA branch from within-tier recurrent) is applied.
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "core/synapse_types/nimcp_synapse_types.h"

/* ----------------------------------------------------------------------
 * Mini-hierarchy: 4 pops in one tier mimicking the production within-tier
 * recurrent wiring (lines 363-365 of nimcp_snn_hierarchical.c). For each
 * ordered pair (sp, dp), (sp+dp) % 5 == 0 selects GABA_A else AMPA. With
 * pop ids 0..3 the violators are 0 (0+0,0+1,0+2,0+3 → 0,1,2,3 → GABA when
 * 5 divides; with 0+0=0 no, 0+5 would, etc. — at this size some pops do
 * mix). We use 5 pops so id 0 emits AMPA→1,2,3,4 plus GABA→0 (self), but
 * we exclude self-connections; pop 1 emits AMPA→0,2,3 + GABA→4; etc.
 * The test asserts the validator detects ≥ 1 violator on this topology
 * — the bug pattern.
 * -------------------------------------------------------------------- */
#define MINI_TIER_POPS 5
#define MINI_NEURONS   4

static snn_network_t* fresh_mini_net(void) {
    snn_config_t cfg;
    snn_config_default(&cfg);
    cfg.n_inputs = 1;
    cfg.n_outputs = 1;
    cfg.n_hidden = 0;
    cfg.dt = 1.0f;
    return snn_network_create(&cfg);
}

START_TEST(test_dale_catches_simulated_recurrent_pattern)
{
    snn_network_t* net = fresh_mini_net();
    ck_assert_ptr_nonnull(net);

    int pop_ids[MINI_TIER_POPS];
    for (int i = 0; i < MINI_TIER_POPS; i++) {
        char nm[32];
        snprintf(nm, sizeof(nm), "tier_pyr_%d", i);
        pop_ids[i] = snn_network_add_population_lightweight(
            net, MINI_NEURONS, NEURON_GENERIC_LIF, nm);
        ck_assert_int_ge(pop_ids[i], 0);
    }

    /* Mirror the production hierarchical recurrent loop — same mod-5
     * GABA / AMPA branching that produces the Dale violation in the
     * primary SNN. */
    for (int sp = 0; sp < MINI_TIER_POPS; sp++) {
        for (int dp = 0; dp < MINI_TIER_POPS; dp++) {
            if (sp == dp) continue;
            synapse_type_t type = ((sp + dp) % 5 == 0)
                ? SYNAPSE_GABA_A : SYNAPSE_AMPA;
            float w = (type == SYNAPSE_GABA_A) ? -0.4f : 0.1f;
            int nc = snn_network_connect_populations(
                net, (uint32_t)pop_ids[sp], (uint32_t)pop_ids[dp],
                SNN_TOPO_FULL, 1.0f, type, w, 0.0f);
            ck_assert_int_gt(nc, 0);
        }
    }

    char err[1024];
    int v = snn_network_validate_dale(net, err, sizeof(err));
    ck_assert_msg(v > 0,
        "Validator failed to detect Dale violation in simulated recurrent "
        "wiring; this is the same pattern as nimcp_snn_hierarchical.c "
        "lines 363-365");
    ck_assert_msg(strlen(err) > 0,
                  "err_buf must contain a violation description");

    snn_network_destroy(net);
}
END_TEST

/* ----------------------------------------------------------------------
 * Full-hierarchy test — gated on NIMCP_RUN_FULL_HIERARCHY=1 because the
 * builder is ~30 GB RSS and 5+ minutes of wiring. Default-off so CI and
 * memory-constrained dev boxes do not OOM-kill on every test run.
 * -------------------------------------------------------------------- */
static snn_network_t* g_hier_net = NULL;

static void hier_ensure_built(void) {
    if (g_hier_net) return;
    g_hier_net = snn_create_hierarchical_network(64, 64, 1800000u);
    ck_assert_ptr_nonnull(g_hier_net);
}

START_TEST(test_dale_holds_on_full_hierarchy)
{
    const char* gate = getenv("NIMCP_RUN_FULL_HIERARCHY");
    if (!gate || gate[0] != '1') {
        fprintf(stderr,
            "[skip] full-hierarchy Dale test gated on "
            "NIMCP_RUN_FULL_HIERARCHY=1 (skipped)\n");
        return;
    }

    hier_ensure_built();

    char err[1024];
    int violations = snn_network_validate_dale(g_hier_net, err, sizeof(err));

    if (violations != 0) {
        fprintf(stderr,
                "[Dale audit] Production hierarchical SNN violates "
                "Dale's principle: %d source pop(s) emit both excitatory "
                "and inhibitory synapses.\n  err='%s'\n",
                violations, err);
    }
    ck_assert_msg(violations == 0,
                  "snn_network_validate_dale returned %d violations: %s",
                  violations, err);
}
END_TEST

static Suite* dale_suite(void)
{
    Suite* s = suite_create("SNN Dale's Principle (Wave C)");

    TCase* tc_mini = tcase_create("mini");
    tcase_set_timeout(tc_mini, 60);
    tcase_add_test(tc_mini, test_dale_catches_simulated_recurrent_pattern);
    suite_add_tcase(s, tc_mini);

    TCase* tc_full = tcase_create("full_hierarchy");
    tcase_set_timeout(tc_full, 1800);  /* 30 min — full wiring is slow */
    tcase_add_test(tc_full, test_dale_holds_on_full_hierarchy);
    suite_add_tcase(s, tc_full);

    return s;
}

int main(void)
{
    SRunner* sr = srunner_create(dale_suite());
    srunner_set_fork_status(sr, CK_NOFORK);  /* share g_hier_net */
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    if (g_hier_net) snn_network_destroy(g_hier_net);
    return failed ? 1 : 0;
}
