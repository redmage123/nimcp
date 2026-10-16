/*=============================================================================
 * test_snn_layer_subtypes_e2e.c — full-brain subclass distribution smoke.
 *=============================================================================*/
/**
 * @file test_snn_layer_subtypes_e2e.c
 * @brief End-to-end smoke: full ~1.8M-neuron SNN brain, verify the P4.1
 *        layer-specific subclass distribution is wired through the entire
 *        brain construction path (not only the standalone hierarchical
 *        builder).
 *
 * WHAT: Builds a full nimcp brain with snn_target_neurons = 1,800,000 via
 *       brain_create_custom() — the same path used by the daemon — forces
 *       SNN creation, then walks the SNN pop list and asserts the resulting
 *       subclass distribution matches the P4.1 design:
 *           >= 1 SNN_NSC_PYRAMIDAL                  (input/L1/L6/output stubs)
 *           >= 1 SNN_NSC_PYRAMIDAL_L23              (L2_pattern / L3_concept)
 *           >= 1 SNN_NSC_PYRAMIDAL_L4_STELLATE      (L4_integr)
 *           >= 1 SNN_NSC_PYRAMIDAL_L5_BETZ          (L5_exec)
 *
 * WHY:  Hierarchical-builder unit/integration tests pin the construction
 *       primitives in isolation. This pins that the full brain build path
 *       doesn't drop the subclass tags somewhere downstream (e.g. a future
 *       refactor that reconstructs SNN pops post-hierarchy could silently
 *       zero-out subclass).
 *
 * HOW:  libcheck (matches test_snn_per_receptor_e2e.c idiom). Same fixture
 *       gating via NIMCP_E2E_QUICK so developer `make test` runs stay fast.
 *
 *       NOTE on language choice: this is a .c file rather than .cpp because
 *       including core/brain/nimcp_brain_internal.h transitively pulls cuda
 *       headers, and the GCC C++ frontend chokes on a cublas redeclaration
 *       warning that the C frontend tolerates. The same pattern holds in
 *       test_snn_per_receptor_e2e.c.
 *
 * RUNTIME: 5-15 minutes (full brain init). NIMCP_E2E_QUICK=1 short-circuits.
 * RESOURCE_LOCK: brain_heavy + lots of RAM (1.8M SNN ≈ 12-16 GB)
 */

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "nimcp.h"
#include "api/nimcp_api_internal.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_network.h"

/* Forces SNN/LNN/CNN sub-network creation. Same helper used by
 * test_snn_per_receptor_e2e.c. */
extern int brain_enable_multi_network_training(brain_t brain);

/*=============================================================================
 * Tunables
 *=============================================================================*/
#define E2E_NUM_INPUTS         128u
#define E2E_NUM_OUTPUTS        64u
#define E2E_SNN_TARGET_NEURONS 1800000u

/*=============================================================================
 * Fixtures
 *=============================================================================*/

static nimcp_brain_t g_brain      = NULL;
static bool          g_quick_skip = false;

static void setup_e2e(void)
{
    const char* q = getenv("NIMCP_E2E_QUICK");
    if (q && q[0] && q[0] != '0') {
        g_quick_skip = true;
        return;
    }

    ck_assert_int_eq(nimcp_init(), NIMCP_OK);

    brain_config_t cfg = brain_config_from_profile(BRAIN_CONFIG_STANDARD);
    cfg.size               = BRAIN_SIZE_LARGE;
    cfg.task               = BRAIN_TASK_CLASSIFICATION;
    cfg.num_inputs         = E2E_NUM_INPUTS;
    cfg.num_outputs        = E2E_NUM_OUTPUTS;
    cfg.snn_target_neurons = E2E_SNN_TARGET_NEURONS;
    snprintf(cfg.task_name, sizeof(cfg.task_name), "snn_layer_subtypes_e2e");

    brain_t internal = brain_create_custom(&cfg);
    ck_assert_msg(internal != NULL, "brain_create_custom failed");

    g_brain = (nimcp_brain_t)calloc(1, sizeof(struct nimcp_brain_handle));
    ck_assert_msg(g_brain != NULL, "Failed to alloc nimcp_brain_handle");
    g_brain->internal_brain     = internal;
    g_brain->last_loss          = 0.0f;
    g_brain->last_gradient_norm = 0.0f;

    int rc = brain_enable_multi_network_training(internal);
    ck_assert_msg(rc == 0, "brain_enable_multi_network_training rc=%d", rc);
    ck_assert_msg(internal->snn_network != NULL,
                  "SNN network not created by enable_multi_network_training");
}

static void teardown_e2e(void)
{
    if (g_quick_skip) return;
    if (g_brain) {
        if (g_brain->internal_brain) {
            brain_destroy(g_brain->internal_brain);
            g_brain->internal_brain = NULL;
        }
        free(g_brain);
        g_brain = NULL;
    }
    nimcp_shutdown();
}

/*=============================================================================
 * Test: subclass distribution matches P4.1 design end-to-end.
 *=============================================================================*/

START_TEST(test_subclass_distribution_matches_design)
{
    if (g_quick_skip) {
        printf("[SKIP] NIMCP_E2E_QUICK set — full-scale subclass test bypassed\n");
        return;
    }

    brain_t b = g_brain->internal_brain;
    ck_assert_ptr_nonnull(b);
    snn_network_t* net = b->snn_network;
    ck_assert_ptr_nonnull(net);
    ck_assert_msg(net->n_populations > 0, "SNN has zero populations");

    uint32_t n_pyramidal = 0;
    uint32_t n_l23       = 0;
    uint32_t n_stellate  = 0;
    uint32_t n_betz      = 0;
    for (uint32_t i = 0; i < net->n_populations; i++) {
        snn_population_t* p = net->populations[i];
        if (!p) continue;
        switch (p->subclass) {
            case SNN_NSC_PYRAMIDAL:             n_pyramidal++; break;
            case SNN_NSC_PYRAMIDAL_L23:         n_l23++;       break;
            case SNN_NSC_PYRAMIDAL_L4_STELLATE: n_stellate++;  break;
            case SNN_NSC_PYRAMIDAL_L5_BETZ:     n_betz++;      break;
            default: break;
        }
    }

    printf("[E2E] subclass distribution: PYRAMIDAL=%u L23=%u "
           "L4_STELLATE=%u L5_BETZ=%u (of %u total pops)\n",
           n_pyramidal, n_l23, n_stellate, n_betz, net->n_populations);

    ck_assert_msg(n_pyramidal >= 1u,
                  "Expected >=1 default PYRAMIDAL pop (input/L1/L6/output), "
                  "got %u", n_pyramidal);
    ck_assert_msg(n_l23 >= 1u,
                  "Expected >=1 PYRAMIDAL_L23 pop (L2_pattern + L3_concept), "
                  "got %u", n_l23);
    ck_assert_msg(n_stellate >= 1u,
                  "Expected >=1 PYRAMIDAL_L4_STELLATE pop (L4_integr), got %u",
                  n_stellate);
    ck_assert_msg(n_betz >= 1u,
                  "Expected >=1 PYRAMIDAL_L5_BETZ pop (L5_exec), got %u",
                  n_betz);
}
END_TEST

/*=============================================================================
 * Test runner
 *=============================================================================*/

static Suite* layer_subtypes_e2e_suite(void)
{
    Suite* s = suite_create("snn_layer_subtypes_e2e");

    TCase* tc = tcase_create("subclass_distribution");
    tcase_set_timeout(tc, 1800);  /* 30 min — full 1.8M brain init. */
    tcase_add_unchecked_fixture(tc, setup_e2e, teardown_e2e);
    tcase_add_test(tc, test_subclass_distribution_matches_design);
    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite*   s  = layer_subtypes_e2e_suite();
    SRunner* sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int n_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
