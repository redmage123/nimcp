/*
 * Wave F (substrate skip-path extension) — integration test.
 *
 * WHAT: Builds the hierarchical SNN and asserts that all 5 skip paths in
 *       SKIP_DEFS produce non-zero AMPA connection counts in the wiring.
 * WHY:  The 3 new entries (input→L4, L3→L6, L5→L2) extend the original 2
 *       (L1→L5, L2→L6). If a future refactor accidentally drops or
 *       silently no-ops one of them, this test catches it before old
 *       cached SNNs are silently shipped to prod.
 * HOW:  Construct a minimal hierarchical net (target=8000 neurons → small
 *       per-tier pop sizes), then for each (src_tier, dst_tier) pair walk
 *       the dst pops' incoming CSRs and count entries whose source pop
 *       belongs to the src tier. All 5 pairs must have count > 0.
 *
 * The test uses libcheck to match the existing SNN integration suite.
 */

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nimcp.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_synapse.h"
#include "snn/nimcp_snn_membrane.h"

/* Skip pairs we expect to find — must match SKIP_DEFS in
 * src/snn/nimcp_snn_hierarchical.c. */
typedef struct {
    const char* src_prefix;   /* tier-name prefix; matches snn_population_t::name */
    const char* dst_prefix;
    const char* label;
} skip_expectation_t;

static const skip_expectation_t SKIP_EXPECTATIONS[] = {
    { "L1_feature",  "L5_exec",     "L1->L5 (original)"          },
    { "L2_pattern",  "L6_project",  "L2->L6 (original)"          },
    { "input",       "L4_integr",   "input->L4 (Wave F)"         },
    { "L3_concept",  "L6_project",  "L3->L6 (Wave F)"            },
    { "L5_exec",     "L2_pattern",  "L5->L2 (Wave F top-down)"   },
};
#define NUM_EXPECTATIONS (sizeof(SKIP_EXPECTATIONS) / sizeof(SKIP_EXPECTATIONS[0]))

/* Match a tier-prefix against a population name.
 * Tier pyramidal pops are named "<tier>_<digit>" (e.g. "L1_feature_0",
 * "L1_feature_3"). Tier inhibitory pops are "<tier>_PV", "<tier>_SOM",
 * "<tier>_VIP". We want pyramidal only — accept iff the suffix after
 * "<tier>_" is a digit. */
static int name_belongs_to_tier(const char* name, const char* prefix)
{
    if (!name || !prefix) return 0;
    size_t plen = strlen(prefix);
    if (strncmp(name, prefix, plen) != 0) return 0;
    if (name[plen] != '_') return 0;
    char first = name[plen + 1];
    return (first >= '0' && first <= '9') ? 1 : 0;
}

/* Combined test: build the hierarchical network once, then verify both the
 * topology (5 skip pairs all wired) and the receptor (all entries are AMPA).
 * Building twice would put us comfortably outside libcheck's per-test budget
 * — a single shared build keeps the wiring cost amortized. */
START_TEST(test_skip_defs_wired_and_ampa)
{
    snn_network_t* net = snn_create_hierarchical_network(
        /*n_inputs*/ 100, /*n_outputs*/ 100, /*target_total*/ 8000);
    ck_assert_ptr_nonnull(net);

    /* For each expected skip pair, walk all populations matching dst_prefix
     * and inspect their incoming CSR. Any entry whose src_pop name matches
     * src_prefix counts; while we're walking, also check the receptor. */
    int per_pair_counts[NUM_EXPECTATIONS] = { 0 };
    int receptor_violations = 0;

    for (uint32_t e = 0; e < NUM_EXPECTATIONS; e++) {
        const skip_expectation_t* exp = &SKIP_EXPECTATIONS[e];

        for (uint32_t dpi = 0; dpi < net->n_populations; dpi++) {
            snn_population_t* dst = net->populations[dpi];
            if (!dst || !dst->name) continue;
            if (!name_belongs_to_tier(dst->name, exp->dst_prefix)) continue;
            if (!dst->incoming_csr) continue;

            snn_csr_storage_t* csr = dst->incoming_csr;
            uint32_t n = csr->n_synapses;
            for (uint32_t k = 0; k < n; k++) {
                uint32_t src_pop_idx = csr->entries[k].src_pop;
                if (src_pop_idx >= net->n_populations) continue;
                snn_population_t* src = net->populations[src_pop_idx];
                if (!src || !src->name) continue;
                if (name_belongs_to_tier(src->name, exp->src_prefix)) {
                    per_pair_counts[e]++;
                    /* Receptor must be AMPA: all 5 skip pairs are pyr→pyr
                     * (Dale's principle, Wave C); the inline byte-array
                     * dst->synapse_type_per_src holds the receptor tag. */
                    if (dst->synapse_type_per_src[src_pop_idx] != SYNAPSE_AMPA) {
                        receptor_violations++;
                    }
                }
            }
        }
    }

    /* All 5 skip pairs must have non-zero connection count. */
    for (uint32_t e = 0; e < NUM_EXPECTATIONS; e++) {
        ck_assert_msg(per_pair_counts[e] > 0,
                      "Skip pair %s: expected > 0 connections, got %d",
                      SKIP_EXPECTATIONS[e].label, per_pair_counts[e]);
    }
    ck_assert_msg(receptor_violations == 0,
                  "Wave F skip pairs must use SYNAPSE_AMPA; %d violations found",
                  receptor_violations);

    snn_network_destroy(net);
}
END_TEST

static Suite* snn_skip_topology_suite(void)
{
    Suite* s = suite_create("snn_skip_topology_wave_f");

    TCase* tc = tcase_create("topology");
    /* Per-test timeout 600 s: full hierarchical wiring of an 8k-neuron net
     * (8 tiers × 3 inh sub-pops + skip cross-product + apportionment).
     * In CI/serialized cmake context this can take 1-2 minutes. */
    tcase_set_timeout(tc, 600);
    tcase_add_test(tc, test_skip_defs_wired_and_ampa);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    if (nimcp_init() != NIMCP_SUCCESS) {
        fprintf(stderr, "nimcp_init failed\n");
        return 1;
    }

    SRunner* sr = srunner_create(snn_skip_topology_suite());
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    nimcp_shutdown();
    return (failed == 0) ? 0 : 1;
}
