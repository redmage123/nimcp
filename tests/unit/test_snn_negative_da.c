/**
 * @file test_snn_negative_da.c
 * @brief Slice F — Anti-Hebbian punishment via negative DA modulation gain.
 *
 * Slice F (negative-DA) was scoped to widen the bridge's
 * da_modulation_gain clamp from [0, +200] to [-200, +200] AND audit the
 * SNN-side three-factor STDP to confirm it propagates negative DA with
 * the correct sign. After Slice A made the bridge transport-only, the
 * bridge's STDP/binding state no longer exists — but Slice F's
 * load-bearing claim, the SNN STDP correctness audit, still applies.
 *
 * This test exercises the SNN three-factor STDP path directly via the
 * canonical entry point `stdp_apply_modulated_weight_change` (see
 * `src/plasticity/stdp/nimcp_stdp.c:391`), driven through the same
 * `stdp_synapse_t` synapses the SNN uses for plasticity.
 *
 * The DA modulation rule is:
 *     modulation_factor = 1 + DA_concentration * da_modulation_gain
 *     weight_change     = base_weight_change * modulation_factor
 *
 * Positive `da_modulation_gain` (reward) amplifies the base STDP delta.
 * Negative `da_modulation_gain` (punishment) inverts the sign — turning
 * LTP into LTD — once |DA × gain| > 1.0. The weight clamp [w_min, w_max]
 * keeps the synapse from crossing zero into negative territory.
 *
 * Coverage:
 *   1. test_positive_da_amplifies_ltp:
 *      base_dw = +0.05, DA=0.5, gain=+10 → modulation=+6 → weight up by ~0.30.
 *
 *   2. test_negative_gain_flips_sign:
 *      base_dw = +0.05, DA=0.5, gain=-10 → modulation=-4 → weight DOWN.
 *
 *   3. test_weight_floors_at_wmin:
 *      Repeated punishment (large negative gain) → weight clamps at w_min,
 *      never goes negative.
 *
 *   4. test_no_nan_under_extremes:
 *      Long sweep at gain=-200 (lower clamp), DA=1.0 stays finite, no NaN.
 *
 *   5. test_zero_da_no_change:
 *      DA=0, any gain → modulation = 1.0 (since DA × gain = 0). Weight
 *      changes only by base_dw (unmodulated).
 *
 *   6. test_three_factor_sign_propagates:
 *      Drive stdp_post_spike_modulated (LTP path) with pre_trace > threshold
 *      then deliver negative DA → resulting weight change must be negative.
 *      This validates the full three-factor pipeline (pre-trace × post-spike
 *      × DA gate) end-to-end.
 *
 * Compile (matches other standalone-C tests in tests/unit/):
 *   gcc -I include tests/unit/test_snn_negative_da.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,$(pwd)/build/lib \
 *       -o /tmp/test_snn_negative_da
 *
 * Run:
 *   /tmp/test_snn_negative_da
 *
 * Exit code: 0 on PASS, non-zero on FAIL.
 */

#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static int g_failures = 0;

#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

/* Build an stdp_synapse_t with deterministic seed weight and the supplied
 * DA modulation gain. We use the standard `stdp_synapse_init_with_config`
 * entry point — same path the SNN populations use — so we exercise the
 * canonical three-factor learning rule.
 */
static void init_synapse(stdp_synapse_t* s, float gain, float seed_w)
{
    stdp_config_t cfg = stdp_config_default();
    cfg.enable_da_modulation = true;
    cfg.da_modulation_gain = gain;
    cfg.burst_amplification = 1.0F; /* keep test math simple */
    stdp_synapse_init_with_config(s, &cfg);
    s->weight = seed_w;
}

/* Create a neuromodulator system pre-seeded with the requested dopamine
 * concentration in [0, 1]. */
static neuromodulator_system_t make_neuromod_with_da(float da_level)
{
    neuromodulator_config_t nc;
    memset(&nc, 0, sizeof(nc));
    neuromodulator_system_t sys = neuromodulator_system_create(&nc);
    if (!sys) return NULL;
    (void)neuromodulator_set_level(sys, NEUROMOD_DOPAMINE, da_level);
    return sys;
}

/* ============================================================
 * Test 1 — positive DA + positive gain amplifies LTP
 * ============================================================ */
static void test_positive_da_amplifies_ltp(void)
{
    stdp_synapse_t syn;
    init_synapse(&syn, /*gain=*/10.0F, /*seed_w=*/0.5F);

    neuromodulator_system_t sys = make_neuromod_with_da(0.5F);
    EXPECT(sys != NULL, "neuromod create");
    if (!sys) return;

    /* Modulation factor = 1 + 0.5 * 10 = 6.0 */
    float mod = stdp_get_da_modulation_factor(&syn, sys);
    EXPECT(fabsf(mod - 6.0F) < 1e-3F,
           "positive DA × positive gain: modulation expected 6.0, got %.4f", mod);

    /* Apply +0.05 base LTP delta. Expect weight to increase by 0.05 * 6 = 0.30. */
    float base_dw = +0.05F;
    float dw = stdp_apply_modulated_weight_change(&syn, base_dw, sys);
    EXPECT(dw > 0.0F, "positive DA: applied dw must be > 0, got %.4f", dw);
    EXPECT(syn.weight > 0.5F,
           "positive DA: weight must increase from seed, got %.4f", syn.weight);
    EXPECT(syn.weight <= syn.w_max,
           "weight must stay <= w_max=%.2f, got %.4f", syn.w_max, syn.weight);
    EXPECT(syn.num_potentiation_events == 1,
           "positive dw → bumps potentiation_events, got %llu",
           (unsigned long long)syn.num_potentiation_events);

    neuromodulator_system_destroy(sys);
}

/* ============================================================
 * Test 2 — negative gain flips LTP sign → weight DECREASES
 * ============================================================ */
static void test_negative_gain_flips_sign(void)
{
    stdp_synapse_t syn;
    init_synapse(&syn, /*gain=*/-10.0F, /*seed_w=*/0.5F);

    neuromodulator_system_t sys = make_neuromod_with_da(0.5F);
    EXPECT(sys != NULL, "neuromod create");
    if (!sys) return;

    /* Modulation factor = 1 + 0.5 * (-10) = -4.0 (sign flip) */
    float mod = stdp_get_da_modulation_factor(&syn, sys);
    EXPECT(mod < 0.0F,
           "negative gain: modulation must be < 0, got %.4f", mod);
    EXPECT(fabsf(mod - (-4.0F)) < 1e-3F,
           "negative gain: modulation expected -4.0, got %.4f", mod);

    /* Apply +0.05 base LTP delta. Modulation -4 flips it: effective dw = -0.20.
     * Synapse seeded at 0.5 → new weight clamps but should be < seed. */
    float base_dw = +0.05F;
    float dw = stdp_apply_modulated_weight_change(&syn, base_dw, sys);
    EXPECT(dw < 0.0F,
           "negative gain: applied dw must be < 0, got %.4f", dw);
    EXPECT(syn.weight < 0.5F,
           "negative gain: weight must decrease from seed, got %.4f", syn.weight);
    EXPECT(syn.weight >= syn.w_min,
           "weight must stay >= w_min=%.2f, got %.4f", syn.w_min, syn.weight);
    EXPECT(isfinite(syn.weight),
           "weight must be finite, got %.4f", syn.weight);
    EXPECT(syn.num_depression_events == 1,
           "negative dw → bumps depression_events, got %llu",
           (unsigned long long)syn.num_depression_events);

    neuromodulator_system_destroy(sys);
}

/* ============================================================
 * Test 3 — repeated punishment floors at w_min, never negative
 * ============================================================ */
static void test_weight_floors_at_wmin(void)
{
    stdp_synapse_t syn;
    init_synapse(&syn, /*gain=*/-50.0F, /*seed_w=*/0.5F);

    neuromodulator_system_t sys = make_neuromod_with_da(1.0F);  /* max DA */
    EXPECT(sys != NULL, "neuromod create");
    if (!sys) return;

    /* Modulation factor = 1 + 1.0 * (-50) = -49.0
     * base_dw=+0.05 → effective dw = -2.45 per pass. After 1 pass:
     * weight = 0.5 + (-2.45) = -1.95 → clamps to w_min=0. */
    for (int i = 0; i < 50; i++) {
        float dw = stdp_apply_modulated_weight_change(&syn, +0.05F, sys);
        (void)dw;
        EXPECT(isfinite(syn.weight),
               "iter %d: weight must stay finite, got %.4f", i, syn.weight);
        EXPECT(syn.weight >= 0.0F,
               "iter %d: weight must clamp at w_min=0, got %.4f", i, syn.weight);
    }

    /* After repeated punishment the weight should be sitting at w_min. */
    EXPECT(syn.weight < 0.001F,
           "repeated punishment: weight should be at floor, got %.6f", syn.weight);
    /* w_min saturation tracked. */
    EXPECT(syn.num_saturate_min_events >= 1,
           "w_min saturation event must be tracked, got %llu",
           (unsigned long long)syn.num_saturate_min_events);

    neuromodulator_system_destroy(sys);
}

/* ============================================================
 * Test 4 — long sweep at extreme negative gain, no NaN/Inf
 * ============================================================ */
static void test_no_nan_under_extremes(void)
{
    stdp_synapse_t syn;
    init_synapse(&syn, /*gain=*/-200.0F, /*seed_w=*/0.5F);  /* lower clamp */

    neuromodulator_system_t sys = make_neuromod_with_da(1.0F);
    EXPECT(sys != NULL, "neuromod create");
    if (!sys) return;

    for (int i = 0; i < 1000; i++) {
        /* Alternate base sign so we exercise both LTP and LTD paths
         * under the negative modulation factor. */
        float base = (i % 2 == 0) ? +0.05F : -0.05F;
        (void)stdp_apply_modulated_weight_change(&syn, base, sys);
        EXPECT(isfinite(syn.weight),
               "iter %d: weight finite, got %.4f", i, syn.weight);
    }
    EXPECT(syn.weight >= syn.w_min && syn.weight <= syn.w_max,
           "after extreme sweep: weight in [%.2f, %.2f], got %.4f",
           syn.w_min, syn.w_max, syn.weight);

    neuromodulator_system_destroy(sys);
}

/* ============================================================
 * Test 5 — DA=0 → modulation=1.0, no change from DA gate
 * ============================================================ */
static void test_zero_da_no_change(void)
{
    stdp_synapse_t syn;
    init_synapse(&syn, /*gain=*/-10.0F, /*seed_w=*/0.5F);

    neuromodulator_system_t sys = make_neuromod_with_da(0.0F);
    EXPECT(sys != NULL, "neuromod create");
    if (!sys) return;

    /* Modulation factor = 1 + 0.0 * (-10) = 1.0 exactly. */
    float mod = stdp_get_da_modulation_factor(&syn, sys);
    EXPECT(fabsf(mod - 1.0F) < 1e-6F,
           "DA=0 → modulation must be 1.0, got %.6f", mod);

    /* Applied delta equals base delta (no modulation effect). */
    float dw = stdp_apply_modulated_weight_change(&syn, +0.05F, sys);
    EXPECT(fabsf(dw - 0.05F) < 1e-4F,
           "DA=0: dw must equal base 0.05, got %.4f", dw);

    neuromodulator_system_destroy(sys);
}

/* ============================================================
 * Test 6 — full three-factor: pre-trace × post-spike × negative DA
 * ============================================================ */
static void test_three_factor_sign_propagates(void)
{
    stdp_synapse_t syn;
    init_synapse(&syn, /*gain=*/-10.0F, /*seed_w=*/0.5F);

    /* Pre-loaded pre-trace simulates a recent presynaptic spike. The
     * trace must exceed min_trace_threshold (default 0.1) for the LTP
     * path to fire. */
    syn.pre_trace = 1.0F;

    neuromodulator_system_t sys = make_neuromod_with_da(0.5F);
    EXPECT(sys != NULL, "neuromod create");
    if (!sys) return;

    /* Post-spike fires LTP. Under negative DA, sign should flip → weight DOWN. */
    float seed = syn.weight;
    float dw = stdp_post_spike_modulated(&syn, 10.0F, sys);
    EXPECT(dw < 0.0F,
           "three-factor: LTP * negative DA must yield dw < 0, got %.4f", dw);
    EXPECT(syn.weight < seed,
           "three-factor: weight must decrease from %.3f, got %.4f", seed, syn.weight);
    EXPECT(syn.weight >= 0.0F,
           "weight clamps at w_min, got %.4f", syn.weight);
    EXPECT(isfinite(syn.weight) && isfinite(dw),
           "three-factor: weight=%.4f dw=%.4f must both be finite",
           syn.weight, dw);

    neuromodulator_system_destroy(sys);
}

int main(void)
{
    fprintf(stderr, "=== test_snn_negative_da (Slice F — SNN STDP path) ===\n");

    test_positive_da_amplifies_ltp();
    test_negative_gain_flips_sign();
    test_weight_floors_at_wmin();
    test_no_nan_under_extremes();
    test_zero_da_no_change();
    test_three_factor_sign_propagates();

    if (g_failures == 0) {
        fprintf(stderr, "ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "FAIL: %d failures\n", g_failures);
    return 1;
}
