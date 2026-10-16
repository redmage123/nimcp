/*=============================================================================
 * test_snn_per_receptor_e2e.c — full-scale CB / per-receptor smoke test
 *=============================================================================*/
/**
 * @file test_snn_per_receptor_e2e.c
 * @brief End-to-end smoke test: full ~1.8M-neuron SNN brain with CB mode +
 *        per-receptor (g_ampa / g_nmda / g_gaba_a / g_gaba_b) split.
 *
 * WHAT: Boots a realistic NIMCP brain at production scale with the per-receptor
 *       conductance-based PSC kernel enabled, runs 100 forward steps under a
 *       deterministic stimulus, and asserts that the resulting state has none
 *       of the historical pathologies the migration was supposed to remove.
 *
 * WHY:  Unit + regression tests exercise the per-receptor kernel at small
 *       scale. Live pod runs occasionally surface dead-pop, runaway-firing,
 *       or NaN-in-membrane patterns that only appear at the 1.8M-pop wiring
 *       density. This test pins the smoke-level invariants at that scale so
 *       regressions are caught before the next pod redeploy.
 *
 * HOW:
 *   1. Set CB ON via snn_tune_set_conductance_enabled(1.0f) BEFORE brain init,
 *      so every pop allocated below gets all 4 g_* arrays in
 *      snn_population_create().
 *   2. Build a brain with snn_target_neurons = 1,800,000 via the
 *      brain_config_t / brain_create_custom() path (the public
 *      nimcp_brain_create() doesn't expose snn_target_neurons).
 *   3. Force lazy SNN creation by calling brain_enable_multi_network_training()
 *      directly. This triggers snn_create_hierarchical_network() which lays
 *      down the 8-tier / 46-population layout with per-pop-pair receptor
 *      typing.
 *   4. Verify g_ampa / g_nmda / g_gaba_a / g_gaba_b non-NULL on the first 5
 *      populations (deeper coverage is the regression test's job).
 *   5. Verify at least one wired src→dst pair has non-default
 *      synapse_type_per_src.
 *   6. Run 100 simulated steps via snn_network_step(net, 1.0f) with a
 *      deterministic external_current driving the input pop.
 *   7. Assertions:
 *        - No NaN/Inf in any pop's membrane_v
 *        - >= 50% of pops with firing_rate_ema > 0.001 Hz over 100 steps
 *        - >= 50% of pops with average rate < 100 Hz (no runaway)
 *        - max(g_ampa, g_nmda, g_gaba_a, g_gaba_b) < 1000 for any neuron
 *
 * RUNTIME: 5-15 minutes (full brain init dominates). Set NIMCP_E2E_QUICK=1
 *          to skip — designed for scheduled / nightly runs only.
 *
 * RESOURCE_LOCK: brain_heavy + lots of RAM (1.8M SNN ≈ 12-16 GB)
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
#include "core/synapse_types/nimcp_synapse_types.h"

/*-----------------------------------------------------------------------------
 * Private SNN tuning hooks (defined in src/snn/nimcp_snn_tuning.c).
 *---------------------------------------------------------------------------*/
extern void  snn_tune_set_conductance_enabled(float v);
extern float snn_tune_get_conductance_enabled(void);
extern void  snn_tune_set_cb_weights_rescaled(float v);

/*-----------------------------------------------------------------------------
 * Private brain helper — forces SNN/LNN/CNN sub-network creation. This is the
 * same function brain_learn_vector() lazily calls on its first invocation.
 *---------------------------------------------------------------------------*/
extern int brain_enable_multi_network_training(brain_t brain);

/*=============================================================================
 * Tunables
 *=============================================================================*/
#define E2E_NUM_INPUTS         128u
#define E2E_NUM_OUTPUTS        64u
#define E2E_SNN_TARGET_NEURONS 1800000u   /* matches NIMCP_DEFAULT_SNN_NEURONS */
#define E2E_N_STEPS            100
#define E2E_DT_MS              1.0f
#define E2E_DRIVE_CURRENT      120.0f      /* "realistic" deterministic stim */
#define E2E_MAX_GBOUND         1000.0f     /* g_* runaway-deposit ceiling */
#define E2E_MAX_RATE_HZ        100.0f      /* >= half pops must stay below */
#define E2E_MIN_RATE_HZ        0.001f      /* >= half pops must stay above */
#define E2E_MIN_LIVE_FRAC      0.50f
#define E2E_MAX_TAME_FRAC      0.50f
#define E2E_FIRST_K_POP_CHECK  5u          /* g_* alloc smoke-check depth */

/*=============================================================================
 * Fixtures
 *=============================================================================*/

static nimcp_brain_t g_brain = NULL;
static bool          g_quick_skip = false;

static void setup_e2e(void)
{
    /* Quick-mode gate: this test is genuinely slow (full 1.8M init). The
     * scheduled CI / nightly run unsets the env var; developer runs default
     * to skip to keep `make test` snappy. */
    const char* q = getenv("NIMCP_E2E_QUICK");
    if (q && q[0] && q[0] != '0') {
        g_quick_skip = true;
        return;
    }

    ck_assert_int_eq(nimcp_init(), NIMCP_OK);

    /* CB MUST be on BEFORE snn_population_create runs. The g_ampa / g_nmda /
     * g_gaba_a / g_gaba_b arrays are allocated unconditionally today
     * (see nimcp_snn_network.c:229..234), but the deposit-side kernel only
     * routes by synapse_type when CB is enabled. Setting it here also
     * documents intent. */
    snn_tune_set_conductance_enabled(1.0f);

    /* Build a custom brain config with the SNN target wired. The public
     * nimcp_brain_create() helper doesn't expose snn_target_neurons. */
    brain_config_t cfg = brain_config_from_profile(BRAIN_CONFIG_STANDARD);
    cfg.size               = BRAIN_SIZE_LARGE;
    cfg.task               = BRAIN_TASK_CLASSIFICATION;
    cfg.num_inputs         = E2E_NUM_INPUTS;
    cfg.num_outputs        = E2E_NUM_OUTPUTS;
    cfg.snn_target_neurons = E2E_SNN_TARGET_NEURONS;
    /* LNN cap stays at default (NIMCP_DEFAULT_LNN_NEURONS) — keep this test
     * focused on the SNN. CNN is left to its profile default. */
    snprintf(cfg.task_name, sizeof(cfg.task_name), "snn_per_receptor_e2e");

    brain_t internal = brain_create_custom(&cfg);
    ck_assert_msg(internal != NULL,
                  "brain_create_custom failed — cannot continue");

    /* Wrap the internal brain in a public handle so the existing
     * destruction path works (nimcp_brain_destroy). */
    g_brain = (nimcp_brain_t)calloc(1, sizeof(struct nimcp_brain_handle));
    ck_assert_msg(g_brain != NULL, "Failed to alloc nimcp_brain_handle");
    g_brain->internal_brain     = internal;
    g_brain->last_loss          = 0.0f;
    g_brain->last_gradient_norm = 0.0f;

    /* Force the SNN/LNN/CNN networks to spin up. Without this, snn_network is
     * NULL until the first brain_learn_vector() call. Doing it here makes
     * the test deterministic — it's not testing the lazy-init path. */
    int rc = brain_enable_multi_network_training(internal);
    ck_assert_msg(rc == 0, "brain_enable_multi_network_training rc=%d", rc);
    ck_assert_msg(internal->snn_network != NULL,
                  "SNN network was not created by enable_multi_network_training");
}

static void teardown_e2e(void)
{
    if (g_quick_skip) return;
    if (g_brain) {
        /* Free the internal brain via brain_destroy() (the symmetric
         * counterpart to brain_create_custom). Don't call
         * nimcp_brain_destroy(g_brain) because the public destroy path
         * frees the handle with nimcp_free(), which mismatches our raw
         * calloc() allocation in setup_e2e. */
        if (g_brain->internal_brain) {
            brain_destroy(g_brain->internal_brain);
            g_brain->internal_brain = NULL;
        }
        free(g_brain);
        g_brain = NULL;
    }
    snn_tune_set_conductance_enabled(0.0f);
    snn_tune_set_cb_weights_rescaled(0.0f);
    nimcp_shutdown();
}

/*=============================================================================
 * Helpers
 *=============================================================================*/

/* Set a deterministic, sustained external_current on the SNN's input pop
 * (population 0 by hierarchical convention). Use a sine pattern to avoid
 * pathological all-firing / all-silent stimuli. */
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

/* Returns true if any element in [data, data+n) is NaN or Inf. */
static bool has_nan_or_inf(const float* data, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        if (!isfinite(data[i])) return true;
    }
    return false;
}

/* Find max-abs float in [data, data+n). Returns 0 if data is NULL. */
static float max_abs(const float* data, uint32_t n)
{
    if (!data) return 0.0f;
    float m = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float a = fabsf(data[i]);
        if (a > m) m = a;
    }
    return m;
}

/*=============================================================================
 * Test: full lifecycle smoke
 *=============================================================================*/

START_TEST(test_per_receptor_smoke_full_brain)
{
    if (g_quick_skip) {
        printf("[SKIP] NIMCP_E2E_QUICK set — full-scale per-receptor test bypassed\n");
        return;
    }

    brain_t b = g_brain->internal_brain;
    ck_assert_ptr_nonnull(b);
    snn_network_t* net = b->snn_network;
    ck_assert_ptr_nonnull(net);
    ck_assert_msg(net->n_populations > 0, "SNN has zero populations");

    printf("[E2E] SNN populations: %u  (target neurons: %u)\n",
           net->n_populations, E2E_SNN_TARGET_NEURONS);

    /* CB must still be on after init — config-write races would unset it. */
    ck_assert_msg(snn_tune_get_conductance_enabled() > 0.5f,
                  "CB flag flipped off by brain init");

    /* ---- Phase 1: per-receptor g_* arrays allocated (smoke check) ------- */
    uint32_t k = (net->n_populations < E2E_FIRST_K_POP_CHECK)
                 ? net->n_populations : E2E_FIRST_K_POP_CHECK;
    for (uint32_t p = 0; p < k; p++) {
        snn_population_t* pop = net->populations[p];
        ck_assert_msg(pop != NULL,        "pop[%u] NULL", p);
        ck_assert_msg(pop->g_ampa   != NULL, "pop[%u].g_ampa NULL",   p);
        ck_assert_msg(pop->g_nmda   != NULL, "pop[%u].g_nmda NULL",   p);
        ck_assert_msg(pop->g_gaba_a != NULL, "pop[%u].g_gaba_a NULL", p);
        ck_assert_msg(pop->g_gaba_b != NULL, "pop[%u].g_gaba_b NULL", p);
    }
    printf("[E2E] First %u pops have all 4 receptor arrays allocated\n", k);

    /* ---- Phase 2: at least one pop-pair has non-default receptor type --- */
    bool found_typed = false;
    for (uint32_t dst_id = 0; dst_id < net->n_populations && !found_typed; dst_id++) {
        snn_population_t* dst = net->populations[dst_id];
        if (!dst) continue;
        for (uint32_t src_id = 0; src_id < SNN_MAX_POPULATIONS; src_id++) {
            if (dst->synapse_type_per_src[src_id] != (uint8_t)SYNAPSE_GENERIC) {
                printf("[E2E] non-default receptor: src=%u→dst=%u type=%u\n",
                       src_id, dst_id, dst->synapse_type_per_src[src_id]);
                found_typed = true;
                break;
            }
        }
    }
    /* Hierarchical SNN wires AMPA/NMDA between cortical tiers — at least one
     * non-GENERIC entry must exist. If this fails, either the wiring code
     * regressed to GENERIC-only or the table population isn't running. */
    ck_assert_msg(found_typed,
                  "No pop-pair has non-default synapse_type — wiring regressed?");

    /* ---- Phase 3: 100 forward steps under deterministic drive ---------- */
    /* Snapshot per-pop spike-count baselines so we can compute an avg rate
     * over the run window. */
    uint64_t* baseline_spikes = (uint64_t*)calloc(net->n_populations, sizeof(uint64_t));
    ck_assert_ptr_nonnull(baseline_spikes);
    for (uint32_t p = 0; p < net->n_populations; p++) {
        baseline_spikes[p] = net->populations[p]
                             ? net->populations[p]->total_spikes : 0;
    }

    int n_step_errors = 0;
    for (int s = 0; s < E2E_N_STEPS; s++) {
        drive_input_pop(net, s);
        int rc = snn_network_step(net, E2E_DT_MS);
        if (rc < 0) {
            n_step_errors++;
            if (n_step_errors <= 3) {
                printf("[E2E] step %d: snn_network_step rc=%d\n", s, rc);
            }
        }
    }
    ck_assert_msg(n_step_errors == 0,
                  "%d/%d steps returned an error", n_step_errors, E2E_N_STEPS);
    printf("[E2E] %d forward steps completed cleanly\n", E2E_N_STEPS);

    /* ---- Phase 4: NaN / Inf scan across all populations ---------------- */
    uint32_t n_nan_pops = 0;
    for (uint32_t p = 0; p < net->n_populations; p++) {
        snn_population_t* pop = net->populations[p];
        if (!pop || !pop->membrane_v) continue;
        const float* v = (const float*)nimcp_tensor_data(pop->membrane_v);
        if (!v) continue;
        if (has_nan_or_inf(v, pop->n_neurons)) n_nan_pops++;
    }
    ck_assert_msg(n_nan_pops == 0,
                  "%u/%u pops have NaN/Inf in membrane_v after %d steps",
                  n_nan_pops, net->n_populations, E2E_N_STEPS);
    printf("[E2E] No NaN/Inf in membrane voltages across all %u pops\n",
           net->n_populations);

    /* ---- Phase 5: dead-pop / runaway-firing screen --------------------- */
    /* avg_rate_hz_p = (delta_total_spikes / n_neurons / window_ms) * 1000 */
    float window_ms = (float)E2E_N_STEPS * E2E_DT_MS;
    uint32_t n_alive   = 0;
    uint32_t n_tame    = 0;
    uint32_t n_counted = 0;
    float    max_rate  = 0.0f;
    for (uint32_t p = 0; p < net->n_populations; p++) {
        snn_population_t* pop = net->populations[p];
        if (!pop || pop->n_neurons == 0) continue;
        uint64_t spikes = pop->total_spikes - baseline_spikes[p];
        float    rate   = ((float)spikes / (float)pop->n_neurons) *
                          (1000.0f / window_ms);
        n_counted++;
        if (rate > E2E_MIN_RATE_HZ)  n_alive++;
        if (rate < E2E_MAX_RATE_HZ)  n_tame++;
        if (rate > max_rate)         max_rate = rate;
    }
    free(baseline_spikes);

    ck_assert_msg(n_counted > 0, "No countable pops");
    float live_frac = (float)n_alive / (float)n_counted;
    float tame_frac = (float)n_tame  / (float)n_counted;
    printf("[E2E] Pops: %u counted, %u alive (>%.3fHz), %u tame (<%.0fHz), max=%.2fHz\n",
           n_counted, n_alive, (double)E2E_MIN_RATE_HZ,
           n_tame, (double)E2E_MAX_RATE_HZ, (double)max_rate);

    ck_assert_msg(live_frac >= E2E_MIN_LIVE_FRAC,
                  "Dead-pop pattern: only %.1f%% pops fired (need >=%.0f%%)",
                  (double)(live_frac * 100.0f),
                  (double)(E2E_MIN_LIVE_FRAC * 100.0f));
    ck_assert_msg(tame_frac >= E2E_MAX_TAME_FRAC,
                  "Runaway pattern: only %.1f%% pops below %.0fHz (need >=%.0f%%)",
                  (double)(tame_frac * 100.0f),
                  (double)E2E_MAX_RATE_HZ,
                  (double)(E2E_MAX_TAME_FRAC * 100.0f));

    /* ---- Phase 6: per-receptor g_* magnitude ceiling ------------------- */
    /* Catches deposit-side bugs (e.g. type-mux pointing GABA_A weights at
     * g_ampa would inflate the wrong bucket). Bound is loose by design —
     * this is a sanity check, not a tight kinetics test. */
    float gmax_ampa = 0.0f, gmax_nmda = 0.0f;
    float gmax_gaba_a = 0.0f, gmax_gaba_b = 0.0f;
    for (uint32_t p = 0; p < net->n_populations; p++) {
        snn_population_t* pop = net->populations[p];
        if (!pop) continue;
        float a = max_abs(pop->g_ampa,   pop->n_neurons);
        float n = max_abs(pop->g_nmda,   pop->n_neurons);
        float ga = max_abs(pop->g_gaba_a, pop->n_neurons);
        float gb = max_abs(pop->g_gaba_b, pop->n_neurons);
        if (a  > gmax_ampa)   gmax_ampa   = a;
        if (n  > gmax_nmda)   gmax_nmda   = n;
        if (ga > gmax_gaba_a) gmax_gaba_a = ga;
        if (gb > gmax_gaba_b) gmax_gaba_b = gb;
    }
    printf("[E2E] g_* maxima: AMPA=%.3f NMDA=%.3f GABA_A=%.3f GABA_B=%.3f\n",
           (double)gmax_ampa, (double)gmax_nmda,
           (double)gmax_gaba_a, (double)gmax_gaba_b);
    ck_assert_msg(gmax_ampa   < E2E_MAX_GBOUND, "g_ampa max %.3f >= %.0f",
                  (double)gmax_ampa,   (double)E2E_MAX_GBOUND);
    ck_assert_msg(gmax_nmda   < E2E_MAX_GBOUND, "g_nmda max %.3f >= %.0f",
                  (double)gmax_nmda,   (double)E2E_MAX_GBOUND);
    ck_assert_msg(gmax_gaba_a < E2E_MAX_GBOUND, "g_gaba_a max %.3f >= %.0f",
                  (double)gmax_gaba_a, (double)E2E_MAX_GBOUND);
    ck_assert_msg(gmax_gaba_b < E2E_MAX_GBOUND, "g_gaba_b max %.3f >= %.0f",
                  (double)gmax_gaba_b, (double)E2E_MAX_GBOUND);

    /* Teardown happens in fixture — exit normally. */
}
END_TEST

/*=============================================================================
 * Test runner
 *=============================================================================*/

static Suite* per_receptor_e2e_suite(void)
{
    Suite* s = suite_create("snn_per_receptor_e2e");

    TCase* tc = tcase_create("smoke_full_brain");
    /* Full brain init + 100 steps can take 5-15 minutes. The fixture
     * short-circuits when NIMCP_E2E_QUICK is set, so the wall-clock for
     * a quick run is sub-second. */
    tcase_set_timeout(tc, 1800);  /* 30 min — generous for slow hardware */
    tcase_add_unchecked_fixture(tc, setup_e2e, teardown_e2e);
    tcase_add_test(tc, test_per_receptor_smoke_full_brain);
    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite*   s  = per_receptor_e2e_suite();
    SRunner* sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int n_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
