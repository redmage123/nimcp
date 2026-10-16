/*=============================================================================
 * test_snn_dendritic_e2e.c — Wave H end-to-end smoke test
 *=============================================================================*/
/**
 * @file test_snn_dendritic_e2e.c
 * @brief Wave H — Full hierarchical brain with dendritic_enabled = 1 for
 *        tier-pyramidal pops; assert no NaN, bounded plateau, finite V's.
 *
 * WHAT: Boots a full ~1.8M-neuron SNN brain with dendritic mode enabled,
 *       runs 50 forward steps + a 200 ms quiet taper, and pins:
 *         - no NaN/Inf in membrane_v / v_basal / v_apical
 *         - all dendritic-tagged pops are tier-pyr (interneurons stay
 *           single-compartment)
 *         - no infinite plateau: plateau_active count returns to 0 within
 *           200 ms of input withdrawal
 * WHY:  Pod-scale regressions in the two-compartment integration only
 *       surface at production wiring density. Skip via NIMCP_E2E_QUICK.
 * HOW:  C + libcheck (matches test_snn_per_receptor_e2e.c). C++ would
 *       trip a cublas redeclaration warning when src/ headers transitively
 *       pull cuda — same reason the per-receptor e2e is C, not C++.
 *
 * RUNTIME: 5-15 minutes. Set NIMCP_E2E_QUICK=1 to skip — designed for
 *          scheduled / nightly runs only.
 *
 * RESOURCE_LOCK: brain_heavy + lots of RAM (1.8M SNN ≈ 12-16 GB)
 *
 * See docs/claude/wave-h-dendritic-design-2026-04-27.md.
 */

#include <check.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "nimcp.h"
#include "api/nimcp_api_internal.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_synapse.h"
#include "utils/tensor/nimcp_tensor.h"

extern void  snn_tune_set_conductance_enabled(float v);
extern float snn_tune_get_conductance_enabled(void);
extern void  snn_tune_set_cb_weights_rescaled(float v);
extern void  snn_tune_set_dendritic_enabled(float v);
extern float snn_tune_get_dendritic_enabled(void);
extern int   brain_enable_multi_network_training(brain_t brain);

/*============================================================================
 * Tunables
 *==========================================================================*/
#define E2E_NUM_INPUTS         128u
#define E2E_NUM_OUTPUTS        64u
#define E2E_SNN_TARGET_NEURONS 1800000u
#define E2E_N_STEPS            50
#define E2E_DT_MS              1.0f
#define E2E_DRIVE_CURRENT      120.0f
#define E2E_PLATEAU_TAPER_MS   200.0f

/*============================================================================
 * Fixtures
 *==========================================================================*/

static nimcp_brain_t g_brain = NULL;
static bool          g_quick_skip = false;

static void setup_e2e(void)
{
    const char* q = getenv("NIMCP_E2E_QUICK");
    if (q && q[0] && q[0] != '0') {
        g_quick_skip = true;
        return;
    }
    ck_assert_int_eq(nimcp_init(), NIMCP_OK);

    /* Both flags must be ON BEFORE pop creation so the hierarchical
     * builder sets pop->dendritic_enabled and allocates the arrays. */
    snn_tune_set_conductance_enabled(1.0f);
    snn_tune_set_cb_weights_rescaled(1.0f);
    snn_tune_set_dendritic_enabled(1.0f);

    brain_config_t cfg = brain_config_from_profile(BRAIN_CONFIG_STANDARD);
    cfg.size               = BRAIN_SIZE_LARGE;
    cfg.task               = BRAIN_TASK_CLASSIFICATION;
    cfg.num_inputs         = E2E_NUM_INPUTS;
    cfg.num_outputs        = E2E_NUM_OUTPUTS;
    cfg.snn_target_neurons = E2E_SNN_TARGET_NEURONS;
    snprintf(cfg.task_name, sizeof(cfg.task_name), "snn_dendritic_e2e");

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
                  "SNN network was not created by enable_multi_network_training");
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
    snn_tune_set_conductance_enabled(0.0f);
    snn_tune_set_cb_weights_rescaled(0.0f);
    snn_tune_set_dendritic_enabled(0.0f);
    nimcp_shutdown();
}

/*============================================================================
 * Helpers
 *==========================================================================*/

static void drive_input_pop(snn_network_t* net, int step)
{
    if (net->n_populations == 0) return;
    snn_population_t* in = net->populations[0];
    if (!in || !in->external_current) return;
    for (uint32_t i = 0; i < in->n_neurons; i++) {
        float phase = (float)(i + step) * 0.01f;
        float scale = 0.5f + 0.5f * sinf(phase);
        in->external_current[i] = E2E_DRIVE_CURRENT * scale;
    }
}

static void quiet_input_pop(snn_network_t* net)
{
    if (net->n_populations == 0) return;
    snn_population_t* in = net->populations[0];
    if (!in || !in->external_current) return;
    for (uint32_t i = 0; i < in->n_neurons; i++) {
        in->external_current[i] = 0.0f;
    }
}

static bool has_nan_or_inf(const float* data, uint32_t n)
{
    if (!data) return false;
    for (uint32_t i = 0; i < n; i++) {
        if (!isfinite(data[i])) return true;
    }
    return false;
}

/*============================================================================
 * Test
 *==========================================================================*/

START_TEST(test_dendritic_full_brain_smoke)
{
    if (g_quick_skip) {
        printf("[SKIP] NIMCP_E2E_QUICK set — full-scale dendritic e2e bypassed\n");
        return;
    }

    brain_t b = g_brain->internal_brain;
    ck_assert_ptr_nonnull(b);
    snn_network_t* net = b->snn_network;
    ck_assert_ptr_nonnull(net);
    ck_assert_msg(net->n_populations > 0, "SNN has zero populations");

    printf("[E2E-Wave-H] SNN populations: %u (target neurons: %u)\n",
           net->n_populations, E2E_SNN_TARGET_NEURONS);

    /* Phase 1: the dendritic-enabled set must equal the tier-pyr set. */
    uint32_t n_dend  = 0;
    uint32_t n_pyr   = 0;
    for (uint32_t p = 0; p < net->n_populations; p++) {
        const snn_population_t* pop = net->populations[p];
        if (!pop) continue;
        bool is_pyr =
            pop->subclass == SNN_NSC_PYRAMIDAL ||
            pop->subclass == SNN_NSC_PYRAMIDAL_L23 ||
            pop->subclass == SNN_NSC_PYRAMIDAL_L4_STELLATE ||
            pop->subclass == SNN_NSC_PYRAMIDAL_L5_BETZ;
        if (is_pyr) n_pyr++;
        if (pop->dendritic_enabled) n_dend++;
    }
    printf("[E2E-Wave-H] %u/%u pops dendritic_enabled (pyr: %u)\n",
           n_dend, net->n_populations, n_pyr);
    ck_assert_msg(n_dend > 0,
                  "No dendritic pops created — wiring did not opt-in tier pyrs");
    ck_assert_msg(n_dend == n_pyr,
                  "Dendritic count %u != pyramidal count %u — wiring "
                  "selectivity broke", n_dend, n_pyr);

    /* Phase 2: 50 driven steps, count errors. */
    int n_err = 0;
    for (int s = 0; s < E2E_N_STEPS; s++) {
        drive_input_pop(net, s);
        int rc = snn_network_step(net, E2E_DT_MS);
        if (rc < 0) n_err++;
    }
    ck_assert_msg(n_err == 0, "%d/%d driven steps returned error",
                  n_err, E2E_N_STEPS);

    /* Phase 3: NaN scan across membrane_v / v_basal / v_apical. */
    uint32_t nan_main = 0, nan_basal = 0, nan_apical = 0;
    for (uint32_t p = 0; p < net->n_populations; p++) {
        const snn_population_t* pop = net->populations[p];
        if (!pop || !pop->membrane_v) continue;
        const float* v = (const float*)nimcp_tensor_data(pop->membrane_v);
        if (has_nan_or_inf(v, pop->n_neurons)) nan_main++;
        if (pop->dendritic_enabled) {
            if (has_nan_or_inf(pop->v_basal,  pop->n_neurons)) nan_basal++;
            if (has_nan_or_inf(pop->v_apical, pop->n_neurons)) nan_apical++;
        }
    }
    ck_assert_msg(nan_main   == 0, "membrane_v NaN/Inf in %u pops", nan_main);
    ck_assert_msg(nan_basal  == 0, "v_basal NaN/Inf in %u pops",    nan_basal);
    ck_assert_msg(nan_apical == 0, "v_apical NaN/Inf in %u pops",   nan_apical);

    /* Phase 4: count active plateaus during drive. */
    uint32_t plateau_during = 0;
    for (uint32_t p = 0; p < net->n_populations; p++) {
        const snn_population_t* pop = net->populations[p];
        if (!pop || !pop->dendritic_enabled || !pop->plateau_active) continue;
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            if (pop->plateau_active[n]) plateau_during++;
        }
    }
    printf("[E2E-Wave-H] active plateaus after drive: %u\n", plateau_during);

    /* Phase 5: 200 ms quiet taper — plateaus must drop to 0 (3τ deactivation). */
    int n_taper = (int)(E2E_PLATEAU_TAPER_MS / E2E_DT_MS);
    for (int s = 0; s < n_taper; s++) {
        quiet_input_pop(net);
        int rc = snn_network_step(net, E2E_DT_MS);
        ck_assert_int_ge(rc, 0);
    }
    uint32_t plateau_after = 0;
    for (uint32_t p = 0; p < net->n_populations; p++) {
        const snn_population_t* pop = net->populations[p];
        if (!pop || !pop->dendritic_enabled || !pop->plateau_active) continue;
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            if (pop->plateau_active[n]) plateau_after++;
        }
    }
    printf("[E2E-Wave-H] active plateaus after %d ms quiet: %u\n",
           (int)E2E_PLATEAU_TAPER_MS, plateau_after);
    ck_assert_msg(plateau_after == 0,
                  "Infinite plateau: %u neurons still have plateau_active = 1 "
                  "after %d ms quiet — τ-based deactivation not running",
                  plateau_after, (int)E2E_PLATEAU_TAPER_MS);
}
END_TEST

/*============================================================================
 * Test runner
 *==========================================================================*/

static Suite* dendritic_e2e_suite(void)
{
    Suite* s = suite_create("snn_dendritic_e2e");
    TCase* tc = tcase_create("smoke_full_brain");
    tcase_set_timeout(tc, 1800);  /* 30 min */
    tcase_add_unchecked_fixture(tc, setup_e2e, teardown_e2e);
    tcase_add_test(tc, test_dendritic_full_brain_smoke);
    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite*   s  = dendritic_e2e_suite();
    SRunner* sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int n_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
