/**
 * @file test_snn_per_receptor.c
 * @brief Unit tests for the new per-receptor SNN conductance kernels.
 *
 * WHAT: Pure-function tests for snn_membrane_nmda_mg_block,
 *       snn_membrane_compute_dv (4-receptor signature),
 *       snn_membrane_decay_one (4-receptor signature),
 *       snn_membrane_deposit_synapse (receptor-typed routing).
 * WHY:  These header-inline helpers are the single source of truth for the
 *       per-receptor membrane equation introduced by the AMPA/NMDA/GABA-A/
 *       GABA-B split. Bugs here corrupt every CB-mode SNN population.
 * HOW:  Direct calls into the static-inline helpers. Self-contained C99
 *       assertion macros (no external test framework) so the file compiles
 *       even when GTest / Check are unavailable, and it does not depend on
 *       any non-header NIMCP library symbols.
 *
 * @date 2026-04-26
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "snn/nimcp_snn_membrane.h"

/* ============================================================================
 * Minimal in-file test harness
 * ============================================================================ */

static int g_tests_run    = 0;
static int g_tests_failed = 0;

#define CHECK(cond, fmt, ...)                                                 \
    do {                                                                       \
        g_tests_run++;                                                         \
        if (!(cond)) {                                                         \
            g_tests_failed++;                                                  \
            fprintf(stderr, "FAIL %s:%d: " fmt "\n",                          \
                    __FILE__, __LINE__, ##__VA_ARGS__);                       \
        }                                                                      \
    } while (0)

#define CHECK_NEAR(actual, expected, tol)                                     \
    CHECK(fabsf((actual) - (expected)) < (tol),                               \
          "expected %g ± %g, got %g", (double)(expected), (double)(tol),      \
          (double)(actual))

#define CHECK_LT(a, b)  CHECK((a) <  (b), "expected %g < %g",  (double)(a), (double)(b))
#define CHECK_GT(a, b)  CHECK((a) >  (b), "expected %g > %g",  (double)(a), (double)(b))
#define CHECK_LE(a, b)  CHECK((a) <= (b), "expected %g <= %g", (double)(a), (double)(b))
#define CHECK_GE(a, b)  CHECK((a) >= (b), "expected %g >= %g", (double)(a), (double)(b))
#define CHECK_EQ(a, b)  CHECK_NEAR((a), (b), 1e-6f)
#define CHECK_IN(a, lo, hi)                                                   \
    CHECK((a) >= (lo) && (a) <= (hi),                                          \
          "expected %g in [%g, %g]", (double)(a), (double)(lo), (double)(hi))

/* Default reversal potentials (mV) used throughout the suite. */
#define E_AMPA    0.0f
#define E_NMDA    0.0f
#define E_GABA_A  -70.0f
#define E_GABA_B  -90.0f
#define V_REST    -65.0f
#define TAU       20.0f
#define DT        1.0f
#define MG_NORM   1.0f

/* ============================================================================
 * 1. Mg2+ block at rest — heavily blocked
 * ============================================================================ */
static void test_mg_block_at_rest(void)
{
    float b = snn_membrane_nmda_mg_block(-65.0f, 1.0f);
    CHECK_LT(b, 0.10f);
}

/* ============================================================================
 * 2. Mg2+ block depolarized — partial relief.
 *
 * NOTE: The classical Jahr-Stevens formula with the canonical constants used
 * in the header (slope 1/16.13, normalization 3.57) gives m(-40 mV) ≈ 0.23,
 * not the rounded 0.36 quoted in the header docstring. The docstring is a
 * rough biological summary; this unit test pins the actual mathematical
 * behavior of the implementation (still well above the resting m≈0.06 — the
 * biological claim "partial relief" still holds — and well below the m≈0.78
 * peak value at V=0).
 * ============================================================================ */
static void test_mg_block_depolarized(void)
{
    float b = snn_membrane_nmda_mg_block(-40.0f, 1.0f);
    /* Expected: 1 / (1 + exp(40/16.13) / 3.57) ≈ 0.230. */
    CHECK_IN(b, 0.20f, 0.30f);
    /* Biological invariants the docstring is really claiming: partial relief
     * relative to rest, but still less than peak. */
    CHECK_GT(b, snn_membrane_nmda_mg_block(-65.0f, 1.0f));   /* > rest */
    CHECK_LT(b, snn_membrane_nmda_mg_block(  0.0f, 1.0f));   /* < peak */
}

/* ============================================================================
 * 3. Mg2+ unblocked at peak — almost fully relieved.
 *
 * NOTE: Header-implementation peak value at V=0 mV with the canonical
 * Jahr-Stevens constants is m(0) = 1/(1 + 1/3.57) ≈ 0.781, not the 0.93
 * quoted in the docstring. The docstring is approximate (it appears to use
 * a different normalization). This test pins the actual implementation.
 * ============================================================================ */
static void test_mg_block_at_peak(void)
{
    float b = snn_membrane_nmda_mg_block(0.0f, 1.0f);
    CHECK_GT(b, 0.70f);
    /* And firmly above the depolarized-but-subthreshold value: monotone. */
    CHECK_GT(b, snn_membrane_nmda_mg_block(-40.0f, 1.0f));
}

/* ============================================================================
 * 4. Mg disabled — block factor is exactly 1.0
 * ============================================================================ */
static void test_mg_block_disabled(void)
{
    float b = snn_membrane_nmda_mg_block(-65.0f, 0.0f);
    CHECK_EQ(b, 1.0f);
}

/* ============================================================================
 * 5. Distinct decay time constants — NMDA decays slower than AMPA
 * ============================================================================ */
static void test_distinct_decay_constants(void)
{
    float g_ampa = 1.0f, g_nmda = 1.0f;
    float g_gaba_a = 0.0f, g_gaba_b = 0.0f;
    /* dt = 5 ms; tau_ampa = 2 ms, tau_nmda = 100 ms */
    float decay_ampa = expf(-5.0f / 2.0f);
    float decay_nmda = expf(-5.0f / 100.0f);
    float decay_gaba_a = 1.0f, decay_gaba_b = 1.0f;
    snn_membrane_decay_one(&g_ampa, &g_nmda, &g_gaba_a, &g_gaba_b,
                           decay_ampa, decay_nmda, decay_gaba_a, decay_gaba_b);
    CHECK_LT(g_ampa, g_nmda);
    /* Sanity: AMPA should be heavily decayed (e^-2.5 ≈ 0.082); NMDA barely (~0.95). */
    CHECK_LT(g_ampa, 0.2f);
    CHECK_GT(g_nmda, 0.9f);
}

/* ============================================================================
 * 6. Receptor routing — AMPA only
 * ============================================================================ */
static void test_route_ampa(void)
{
    float i_syn = 0.0f;
    float g_ampa = 0.0f, g_nmda = 0.0f, g_gaba_a = 0.0f, g_gaba_b = 0.0f;
    snn_membrane_deposit_synapse(&i_syn, &g_ampa, &g_nmda, &g_gaba_a, &g_gaba_b,
                                 0.5f, SYNAPSE_AMPA, /*cb=*/true);
    CHECK_EQ(g_ampa, 0.5f);
    CHECK_EQ(g_nmda, 0.0f);
    CHECK_EQ(g_gaba_a, 0.0f);
    CHECK_EQ(g_gaba_b, 0.0f);
    CHECK_EQ(i_syn, 0.0f);
}

/* ============================================================================
 * 7. Receptor routing — NMDA only
 * ============================================================================ */
static void test_route_nmda(void)
{
    float i_syn = 0.0f;
    float g_ampa = 0.0f, g_nmda = 0.0f, g_gaba_a = 0.0f, g_gaba_b = 0.0f;
    snn_membrane_deposit_synapse(&i_syn, &g_ampa, &g_nmda, &g_gaba_a, &g_gaba_b,
                                 0.5f, SYNAPSE_NMDA, true);
    CHECK_EQ(g_ampa, 0.0f);
    CHECK_EQ(g_nmda, 0.5f);
    CHECK_EQ(g_gaba_a, 0.0f);
    CHECK_EQ(g_gaba_b, 0.0f);
}

/* ============================================================================
 * 8. Receptor routing — GABA_A always positive conductance (|w| add)
 * ============================================================================ */
static void test_route_gaba_a(void)
{
    /* Negative weight -> |weight| should be added to g_gaba_a. */
    float i_syn = 0.0f;
    float g_ampa = 0.0f, g_nmda = 0.0f, g_gaba_a = 0.0f, g_gaba_b = 0.0f;
    snn_membrane_deposit_synapse(&i_syn, &g_ampa, &g_nmda, &g_gaba_a, &g_gaba_b,
                                 -0.5f, SYNAPSE_GABA_A, true);
    CHECK_EQ(g_gaba_a, 0.5f);
    CHECK_EQ(g_ampa, 0.0f);
    CHECK_EQ(g_nmda, 0.0f);
    CHECK_EQ(g_gaba_b, 0.0f);

    /* Positive weight -> same |weight| add, never negative. */
    g_gaba_a = 0.0f;
    snn_membrane_deposit_synapse(&i_syn, &g_ampa, &g_nmda, &g_gaba_a, &g_gaba_b,
                                 +0.5f, SYNAPSE_GABA_A, true);
    CHECK_EQ(g_gaba_a, 0.5f);
}

/* ============================================================================
 * 9. Receptor routing — GABA_B
 * ============================================================================ */
static void test_route_gaba_b(void)
{
    float i_syn = 0.0f;
    float g_ampa = 0.0f, g_nmda = 0.0f, g_gaba_a = 0.0f, g_gaba_b = 0.0f;
    snn_membrane_deposit_synapse(&i_syn, &g_ampa, &g_nmda, &g_gaba_a, &g_gaba_b,
                                 -0.5f, SYNAPSE_GABA_B, true);
    CHECK_EQ(g_gaba_b, 0.5f);
    CHECK_EQ(g_gaba_a, 0.0f);
    CHECK_EQ(g_ampa, 0.0f);
    CHECK_EQ(g_nmda, 0.0f);
}

/* ============================================================================
 * 10. Generic fallback — positive weight routes to AMPA
 * ============================================================================ */
static void test_route_generic_positive(void)
{
    float i_syn = 0.0f;
    float g_ampa = 0.0f, g_nmda = 0.0f, g_gaba_a = 0.0f, g_gaba_b = 0.0f;
    snn_membrane_deposit_synapse(&i_syn, &g_ampa, &g_nmda, &g_gaba_a, &g_gaba_b,
                                 0.3f, SYNAPSE_GENERIC, true);
    CHECK_EQ(g_ampa, 0.3f);
    CHECK_EQ(g_gaba_a, 0.0f);
    CHECK_EQ(g_nmda, 0.0f);
    CHECK_EQ(g_gaba_b, 0.0f);
}

/* ============================================================================
 * 11. Generic fallback — negative weight routes |w| to GABA_A
 * ============================================================================ */
static void test_route_generic_negative(void)
{
    float i_syn = 0.0f;
    float g_ampa = 0.0f, g_nmda = 0.0f, g_gaba_a = 0.0f, g_gaba_b = 0.0f;
    snn_membrane_deposit_synapse(&i_syn, &g_ampa, &g_nmda, &g_gaba_a, &g_gaba_b,
                                 -0.3f, SYNAPSE_GENERIC, true);
    CHECK_EQ(g_gaba_a, 0.3f);
    CHECK_EQ(g_ampa, 0.0f);
    CHECK_EQ(g_nmda, 0.0f);
    CHECK_EQ(g_gaba_b, 0.0f);
}

/* ============================================================================
 * 12. NULL-tolerance — deposit, decay, and compute_dv survive NULL pointers
 * ============================================================================ */
static void test_null_tolerance(void)
{
    /* deposit: all NULL conductance pointers — should not crash, and must not
     * touch i_syn in CB mode either (CB-mode receptor routing only). */
    float i_syn = 0.0f;
    snn_membrane_deposit_synapse(&i_syn, NULL, NULL, NULL, NULL,
                                 1.0f, SYNAPSE_AMPA, true);
    CHECK_EQ(i_syn, 0.0f);

    /* deposit fallback (GENERIC) with NULLs — same: silent no-op. */
    snn_membrane_deposit_synapse(&i_syn, NULL, NULL, NULL, NULL,
                                 -0.5f, SYNAPSE_GENERIC, true);
    CHECK_EQ(i_syn, 0.0f);

    /* decay: all NULLs — must not crash. */
    snn_membrane_decay_one(NULL, NULL, NULL, NULL, 0.5f, 0.5f, 0.5f, 0.5f);

    /* compute_dv with all g_* zero behaves like the leak-only kernel and
     * must not produce NaN/Inf. */
    float dv = snn_membrane_compute_dv(
        V_REST, V_REST, TAU, DT, /*i_syn=*/0.0f,
        /*g_ampa=*/0.0f, /*g_nmda=*/0.0f, /*g_gaba_a=*/0.0f, /*g_gaba_b=*/0.0f,
        E_AMPA, E_NMDA, E_GABA_A, E_GABA_B, MG_NORM, /*cb=*/true);
    CHECK(isfinite(dv), "dv must be finite");
    CHECK_EQ(dv, 0.0f);
}

/* ============================================================================
 * 13. NMDA at rest is silent — Mg2+ block suppresses dv.
 *
 * The biological claim under test: at rest, NMDA conductance produces far
 * less depolarization per unit g than AMPA. We pin "NMDA dv at rest" to be
 * a small fraction (≤ 10%) of the equivalent AMPA dv — the Mg2+ block is
 * what makes NMDA a coincidence detector rather than just slow excitation.
 * ============================================================================ */
static void test_nmda_at_rest_silent(void)
{
    const float v = -65.0f;
    float dv_nmda = snn_membrane_compute_dv(
        v, V_REST, TAU, DT, /*i_syn=*/0.0f,
        /*g_ampa=*/0.0f, /*g_nmda=*/10.0f, /*g_gaba_a=*/0.0f, /*g_gaba_b=*/0.0f,
        E_AMPA, E_NMDA, E_GABA_A, E_GABA_B, /*mg_mm=*/1.0f, /*cb=*/true);
    float dv_ampa = snn_membrane_compute_dv(
        v, V_REST, TAU, DT, /*i_syn=*/0.0f,
        /*g_ampa=*/10.0f, /*g_nmda=*/0.0f, /*g_gaba_a=*/0.0f, /*g_gaba_b=*/0.0f,
        E_AMPA, E_NMDA, E_GABA_A, E_GABA_B, /*mg_mm=*/1.0f, /*cb=*/true);
    /* Ratio reflects the Mg2+ block factor (~0.06 at -65 mV). */
    CHECK_LT(fabsf(dv_nmda), 0.10f * fabsf(dv_ampa));
    /* And dv must remain small in absolute terms — well under 2 mV/ms. */
    CHECK_LT(fabsf(dv_nmda), 2.0f);
}

/* ============================================================================
 * 14. NMDA depolarized fires — relief from Mg2+ block.
 *
 * At V=-30 mV the Mg2+ block factor relieves to ~0.42; the reversal-potential
 * clamp (E_nmda = 0 mV) caps how far V can move per step, so dv lands around
 * 3-4 mV/ms with g_nmda=10. The biological claim under test is "NMDA becomes
 * a meaningful driver only after the cell is already depolarized" — i.e.
 * dv at -30 mV must be substantially larger than dv at -65 mV.
 * ============================================================================ */
static void test_nmda_depolarized_fires(void)
{
    float dv_depol = snn_membrane_compute_dv(
        /*v=*/-30.0f, V_REST, TAU, DT, /*i_syn=*/0.0f,
        /*g_ampa=*/0.0f, /*g_nmda=*/10.0f, /*g_gaba_a=*/0.0f, /*g_gaba_b=*/0.0f,
        E_AMPA, E_NMDA, E_GABA_A, E_GABA_B, /*mg_mm=*/1.0f, /*cb=*/true);
    float dv_rest = snn_membrane_compute_dv(
        /*v=*/-65.0f, V_REST, TAU, DT, /*i_syn=*/0.0f,
        /*g_ampa=*/0.0f, /*g_nmda=*/10.0f, /*g_gaba_a=*/0.0f, /*g_gaba_b=*/0.0f,
        E_AMPA, E_NMDA, E_GABA_A, E_GABA_B, /*mg_mm=*/1.0f, /*cb=*/true);
    /* Coincidence-detection: at least 1.5× the resting drive. */
    CHECK_GT(dv_depol, 1.5f * dv_rest);
    /* And meaningfully positive in absolute terms. */
    CHECK_GT(dv_depol, 2.5f);
}

/* ============================================================================
 * 15. AMPA at rest fires — no Mg2+ block
 * ============================================================================ */
static void test_ampa_at_rest_fires(void)
{
    float dv = snn_membrane_compute_dv(
        /*v=*/-65.0f, V_REST, TAU, DT, /*i_syn=*/0.0f,
        /*g_ampa=*/10.0f, /*g_nmda=*/0.0f, /*g_gaba_a=*/0.0f, /*g_gaba_b=*/0.0f,
        E_AMPA, E_NMDA, E_GABA_A, E_GABA_B, /*mg_mm=*/1.0f, /*cb=*/true);
    CHECK_GT(dv, 5.0f);
}

/* ============================================================================
 * 16. Reversal-potential clamp — V cannot overshoot E_ampa
 * ============================================================================ */
static void test_reversal_clamp(void)
{
    /* Start V just below E_ampa, hit it with a huge g_ampa. dv should bring
     * v exactly to E_ampa, no overshoot. */
    const float v = E_AMPA - 0.5f;   /* -0.5 mV, just below E_ampa = 0 */
    const float g_ampa = 1000.0f;
    float dv = snn_membrane_compute_dv(
        v, V_REST, TAU, DT, /*i_syn=*/0.0f,
        g_ampa, /*g_nmda=*/0.0f, /*g_gaba_a=*/0.0f, /*g_gaba_b=*/0.0f,
        E_AMPA, E_NMDA, E_GABA_A, E_GABA_B, MG_NORM, /*cb=*/true);
    /* The reversal-potential clamp must cap V at E_ampa. */
    CHECK_LE(v + dv, E_AMPA + 1e-4f);
    /* And stop ON the rail — clamp pinned dv = E_ampa - v exactly. */
    CHECK_NEAR(v + dv, E_AMPA, 1e-4f);
}

/* ============================================================================
 * 17. Current-mode equivalence — conductance_mode=false ignores g_*
 * ============================================================================ */
static void test_current_mode_equivalence(void)
{
    /* In current mode the synaptic drive is i_syn alone; g_* values are
     * silently ignored even when wildly large. */
    float dv_a = snn_membrane_compute_dv(
        V_REST, V_REST, TAU, DT, /*i_syn=*/15.0f,
        /*g_ampa=*/0.0f, /*g_nmda=*/0.0f, /*g_gaba_a=*/0.0f, /*g_gaba_b=*/0.0f,
        E_AMPA, E_NMDA, E_GABA_A, E_GABA_B, MG_NORM, /*cb=*/false);
    float dv_b = snn_membrane_compute_dv(
        V_REST, V_REST, TAU, DT, /*i_syn=*/15.0f,
        /*g_ampa=*/100.0f, /*g_nmda=*/100.0f,
        /*g_gaba_a=*/100.0f, /*g_gaba_b=*/100.0f,
        E_AMPA, E_NMDA, E_GABA_A, E_GABA_B, MG_NORM, /*cb=*/false);
    /* Same i_syn, same dv regardless of g_*. */
    CHECK_NEAR(dv_a, dv_b, 1e-6f);
    /* And the value matches the legacy current-mode formula:
     * dv = (v_rest - v + i_syn) / tau * dt = (0 + 15) / 20 * 1 = 0.75. */
    CHECK_NEAR(dv_a, 15.0f / 20.0f, 1e-6f);
}

/* ============================================================================
 * Test runner
 * ============================================================================ */

typedef void (*test_fn_t)(void);

typedef struct {
    const char* name;
    test_fn_t   fn;
} test_case_t;

int main(void)
{
    static const test_case_t cases[] = {
        { "mg_block_at_rest",          test_mg_block_at_rest          },
        { "mg_block_depolarized",      test_mg_block_depolarized      },
        { "mg_block_at_peak",          test_mg_block_at_peak          },
        { "mg_block_disabled",         test_mg_block_disabled         },
        { "distinct_decay_constants",  test_distinct_decay_constants  },
        { "route_ampa",                test_route_ampa                },
        { "route_nmda",                test_route_nmda                },
        { "route_gaba_a",              test_route_gaba_a              },
        { "route_gaba_b",              test_route_gaba_b              },
        { "route_generic_positive",    test_route_generic_positive    },
        { "route_generic_negative",    test_route_generic_negative    },
        { "null_tolerance",            test_null_tolerance            },
        { "nmda_at_rest_silent",       test_nmda_at_rest_silent       },
        { "nmda_depolarized_fires",    test_nmda_depolarized_fires    },
        { "ampa_at_rest_fires",        test_ampa_at_rest_fires        },
        { "reversal_clamp",            test_reversal_clamp            },
        { "current_mode_equivalence",  test_current_mode_equivalence  },
    };
    const size_t n_cases = sizeof(cases) / sizeof(cases[0]);

    fprintf(stderr, "Running %zu test cases for SNN per-receptor kernels...\n",
            n_cases);
    for (size_t i = 0; i < n_cases; i++) {
        const int failed_before = g_tests_failed;
        cases[i].fn();
        const int delta = g_tests_failed - failed_before;
        fprintf(stderr, "  [%zu/%zu] %-30s %s\n", i + 1, n_cases,
                cases[i].name, delta == 0 ? "ok" : "FAIL");
    }

    fprintf(stderr, "\n%s: %d assertions, %d failed\n",
            g_tests_failed == 0 ? "PASS" : "FAIL",
            g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
