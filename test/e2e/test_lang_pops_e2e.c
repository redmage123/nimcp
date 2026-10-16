//=============================================================================
// test_lang_pops_e2e.c — full brain init + lang pops + adapter binding
//=============================================================================
/**
 * @file test_lang_pops_e2e.c
 * @brief E2E: bring up a brain via brain_create_custom + force SNN spin-up,
 *        then verify the four substrate pops were created AND broca/wernicke
 *        adapters were bound to their pops.
 *
 * WHAT: Mirrors the test_snn_per_receptor_e2e.c pattern — uses
 *       brain_create_custom() + brain_enable_multi_network_training() to
 *       force the SNN substrate path, then asserts:
 *         - the four substrate pops exist by name
 *         - the broca + wernicke adapters report their bound pop ids match
 *           the SNN's lookup
 *         - the sensorymotor_ring pop has the expected size
 * WHY:  Smoke-test the entire wiring chain end-to-end. If any of the steps
 *       between "broca/wernicke created" and "init_language_pops calls
 *       attach helpers" silently regresses, this catches it before pod deploy.
 * HOW:  C99. No GTest framework — uses fail/pass sentinels + return codes
 *       (matches test_snn_per_receptor_e2e.c style).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "nimcp.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"

#define E2E_LOG(...) do { fprintf(stderr, "[lang_pops_e2e] " __VA_ARGS__); fputc('\n', stderr); } while(0)

#define ASSERT_TRUE_OR_FAIL(cond, msg) do { \
    if (!(cond)) { E2E_LOG("FAIL: %s (cond: %s)", msg, #cond); goto fail; } \
} while(0)

/* MUST mirror nimcp_brain_init_language_pops.c — drift here is the bug. */
#define LANG_BROCA_POP_NAME             "broca_substrate"
#define LANG_WERNICKE_POP_NAME          "wernicke_substrate"
#define LANG_ARCUATE_POP_NAME           "arcuate_relay"
#define SENSORYMOTOR_RING_POP_NAME      "sensorymotor_ring"
#define EXPECTED_SENSORYMOTOR_NEURONS   40000u

#define E2E_NUM_INPUTS         16u
#define E2E_NUM_OUTPUTS        8u
#define E2E_SNN_TARGET         50000u  /* small enough to keep init fast */

extern int  brain_enable_multi_network_training(brain_t brain);
extern bool nimcp_brain_factory_init_broca_subsystem(brain_t);
extern bool nimcp_brain_factory_init_wernicke_subsystem(brain_t);

static int run_e2e(void) {
    if (nimcp_init() != NIMCP_OK) {
        E2E_LOG("nimcp_init failed");
        return 1;
    }

    brain_t brain = NULL;

    /* Configure a small brain — large enough that brain_decide / learn_vector
     * take the SNN substrate path (which gates on num_inputs >= 8 &&
     * num_outputs >= 8). Speech cortex + multimodal flags must be enabled
     * to force broca + wernicke adapter creation. */
    brain_config_t cfg = brain_config_from_profile(BRAIN_CONFIG_STANDARD);
    cfg.size               = BRAIN_SIZE_SMALL;
    cfg.task               = BRAIN_TASK_CLASSIFICATION;
    cfg.num_inputs         = E2E_NUM_INPUTS;
    cfg.num_outputs        = E2E_NUM_OUTPUTS;
    cfg.snn_target_neurons = E2E_SNN_TARGET;
    cfg.enable_speech_cortex = true;
    cfg.enable_multimodal_integration = true;
    snprintf(cfg.task_name, sizeof(cfg.task_name), "lang_pops_e2e");

    brain = brain_create_custom(&cfg);
    ASSERT_TRUE_OR_FAIL(brain != NULL, "brain_create_custom returned NULL");

    /* brain_create_custom doesn't always run the region-init wave (it
     * depends on init_mode + parallel-init availability). Call broca +
     * wernicke factory inits explicitly so the adapters exist BEFORE the
     * SNN spin-up — which is the contract init_language_pops relies on
     * to wire the adapter ↔ pop binding. */
    if (!brain->broca) {
        nimcp_brain_factory_init_broca_subsystem(brain);
    }
    if (!brain->wernicke) {
        nimcp_brain_factory_init_wernicke_subsystem(brain);
    }
    ASSERT_TRUE_OR_FAIL(brain->broca != NULL, "broca init failed");
    ASSERT_TRUE_OR_FAIL(brain->wernicke != NULL, "wernicke init failed");

    /* Force SNN spin-up + lazy init_language_pops. Without this, the SNN
     * stays NULL until the first brain_learn_vector call. */
    int mn_rc = brain_enable_multi_network_training(brain);
    if (mn_rc != 0) {
        E2E_LOG("brain_enable_multi_network_training rc=%d", mn_rc);
        goto fail;
    }
    ASSERT_TRUE_OR_FAIL(brain->snn_network != NULL,
                        "SNN network was not created");

    /* The four substrate pops must be present after init_language_pops. */
    snn_network_t* snn = brain->snn_network;
    int broca_pop    = snn_network_find_pop_by_name(snn, LANG_BROCA_POP_NAME);
    int wernicke_pop = snn_network_find_pop_by_name(snn, LANG_WERNICKE_POP_NAME);
    int arcuate_pop  = snn_network_find_pop_by_name(snn, LANG_ARCUATE_POP_NAME);
    int sensory_pop  = snn_network_find_pop_by_name(snn, SENSORYMOTOR_RING_POP_NAME);
    ASSERT_TRUE_OR_FAIL(broca_pop    >= 0, "broca_substrate pop not found");
    ASSERT_TRUE_OR_FAIL(wernicke_pop >= 0, "wernicke_substrate pop not found");
    ASSERT_TRUE_OR_FAIL(arcuate_pop  >= 0, "arcuate_relay pop not found");
    ASSERT_TRUE_OR_FAIL(sensory_pop  >= 0, "sensorymotor_ring pop not found");

    /* Adapters must exist and be bound to the right pops. */
    ASSERT_TRUE_OR_FAIL(brain->broca    != NULL, "brain->broca was not created");
    ASSERT_TRUE_OR_FAIL(brain->wernicke != NULL, "brain->wernicke was not created");

    int bound_broca    = broca_get_snn_pop_id(brain->broca);
    int bound_wernicke = wernicke_get_snn_pop_id(brain->wernicke);
    if (bound_broca != broca_pop) {
        E2E_LOG("FAIL: broca bound to %d, expected %d", bound_broca, broca_pop);
        goto fail;
    }
    if (bound_wernicke != wernicke_pop) {
        E2E_LOG("FAIL: wernicke bound to %d, expected %d",
                bound_wernicke, wernicke_pop);
        goto fail;
    }
    ASSERT_TRUE_OR_FAIL(broca_get_snn_network(brain->broca) == snn,
                        "broca SNN handle mismatch");
    ASSERT_TRUE_OR_FAIL(wernicke_get_snn_network(brain->wernicke) == snn,
                        "wernicke SNN handle mismatch");

    /* Sensorymotor synfire ring sanity: pop size matches the constant in
     * the init code. */
    snn_population_t* sm = snn->populations[sensory_pop];
    ASSERT_TRUE_OR_FAIL(sm != NULL, "sensorymotor_ring pop NULL");
    if (sm->n_neurons != EXPECTED_SENSORYMOTOR_NEURONS) {
        E2E_LOG("FAIL: sensorymotor_ring n=%u, expected %u",
                sm->n_neurons, EXPECTED_SENSORYMOTOR_NEURONS);
        goto fail;
    }

    E2E_LOG("PASS: 4 substrate pops present (broca=%d wernicke=%d arcuate=%d sensorymotor=%d)",
            broca_pop, wernicke_pop, arcuate_pop, sensory_pop);
    E2E_LOG("PASS: broca adapter bound to pop %d, wernicke adapter bound to pop %d",
            bound_broca, bound_wernicke);

    brain_destroy(brain);
    nimcp_shutdown();
    return 0;

fail:
    if (brain) brain_destroy(brain);
    nimcp_shutdown();
    return 1;
}

int main(void) {
    int rc = run_e2e();
    fprintf(stderr, "[lang_pops_e2e] %s\n",
            (rc == 0) ? "ALL CHECKS PASSED" : "FAIL");
    return rc;
}
