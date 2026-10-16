//=============================================================================
// test_snn_layer_subtypes_unit.cpp — Unit tests for P4.1 layer-specific
// pyramidal subtypes (L23 / L4_STELLATE / L5_BETZ).
//=============================================================================
/**
 * @file test_snn_layer_subtypes_unit.cpp
 * @brief Pure-function tests for snn_pop_lif_params() under the three
 *        layer-specific pyramidal subtypes added by P4.1.
 *
 * WHAT: Pins the LIF-parameter deltas snn_pop_lif_params() applies for
 *       SNN_NSC_PYRAMIDAL_L23, SNN_NSC_PYRAMIDAL_L4_STELLATE, and
 *       SNN_NSC_PYRAMIDAL_L5_BETZ. Also spot-checks one prior-coverage
 *       branch (SNN_NSC_PV) so the new switch arms cannot quietly break
 *       existing PV/SOM/VIP/TRN profiles.
 *
 * WHY:  The layer-specific pyramidal profiles are the substrate for
 *       cortical-layer-aware membrane dynamics — Betz cells must fire
 *       earlier than their L23 neighbours under identical drive, L4
 *       stellate must demand convergent input. If snn_pop_lif_params()
 *       silently regresses on its delta math (either by missing a case or
 *       reading off cfg defaults), that property disappears with no other
 *       audit trail.
 *
 * HOW:  Pure-function: stack-allocate a zero-initialized snn_population_t
 *       and an snn_config_t loaded with snn_config_default(), set only
 *       pop.subclass, and inspect the returned snn_lif_params_t. No SNN
 *       network creation, no GPU, no link to anything heavier than libm —
 *       this is the same self-contained idiom test_snn_per_receptor.c uses
 *       for the membrane kernels.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
}

namespace {

constexpr float kTol = 1e-6f;

/* Build a default snn_config_t exactly as snn_network_create() would.
 * Returns by value so test bodies stay self-contained. */
static snn_config_t make_default_config()
{
    snn_config_t cfg;
    std::memset(&cfg, 0, sizeof(cfg));
    int rc = snn_config_default(&cfg);
    EXPECT_EQ(rc, 0) << "snn_config_default failed";
    return cfg;
}

/* Build a zero-initialized population with only the subclass set. The
 * helper under test only reads pop->subclass, so the other tensor / CSR
 * fields can stay NULL. */
static snn_population_t make_pop(neuron_subclass_t sub)
{
    snn_population_t pop;
    std::memset(&pop, 0, sizeof(pop));
    pop.subclass = sub;
    return pop;
}

}  // namespace

//=============================================================================
// PYRAMIDAL → returns network defaults verbatim (no-op).
//=============================================================================
TEST(SnnLayerSubtypesUnit, PyramidalReturnsDefaults)
{
    snn_config_t cfg = make_default_config();
    snn_population_t pop = make_pop(SNN_NSC_PYRAMIDAL);

    snn_lif_params_t p = snn_pop_lif_params(&pop, &cfg);

    EXPECT_NEAR(p.v_thresh, cfg.v_thresh, kTol);
    EXPECT_NEAR(p.v_reset,  cfg.v_reset,  kTol);
    EXPECT_NEAR(p.v_rest,   cfg.v_rest,   kTol);
    EXPECT_NEAR(p.tau_mem,  cfg.tau_mem,  kTol);
    EXPECT_NEAR(p.t_ref,    cfg.t_ref,    kTol);
}

//=============================================================================
// L23 → tau_mem = 18 ms; v_thresh / v_reset / v_rest / t_ref unchanged.
//=============================================================================
TEST(SnnLayerSubtypesUnit, L23OverridesTauMemOnly)
{
    snn_config_t cfg = make_default_config();
    snn_population_t pop = make_pop(SNN_NSC_PYRAMIDAL_L23);

    snn_lif_params_t p = snn_pop_lif_params(&pop, &cfg);

    EXPECT_NEAR(p.tau_mem, 18.0f, kTol)
        << "L23 must pin tau_mem to 18 ms (intratelencephalic; faster than default 20)";
    EXPECT_NEAR(p.v_thresh, cfg.v_thresh, kTol);
    EXPECT_NEAR(p.v_reset,  cfg.v_reset,  kTol);
    EXPECT_NEAR(p.v_rest,   cfg.v_rest,   kTol);
    EXPECT_NEAR(p.t_ref,    cfg.t_ref,    kTol);
}

//=============================================================================
// L4_STELLATE → tau_mem = 14 ms; v_thresh = cfg.v_thresh + 2 mV; rest unchanged.
//=============================================================================
TEST(SnnLayerSubtypesUnit, L4StellateOverridesTauMemAndVThresh)
{
    snn_config_t cfg = make_default_config();
    snn_population_t pop = make_pop(SNN_NSC_PYRAMIDAL_L4_STELLATE);

    snn_lif_params_t p = snn_pop_lif_params(&pop, &cfg);

    EXPECT_NEAR(p.tau_mem, 14.0f, kTol)
        << "L4 spiny-stellate small-soma fast integrator (Lübke 2003)";
    EXPECT_NEAR(p.v_thresh, cfg.v_thresh + 2.0f, kTol)
        << "L4 stellate must demand convergent input (higher threshold)";
    EXPECT_NEAR(p.v_reset, cfg.v_reset, kTol);
    EXPECT_NEAR(p.v_rest,  cfg.v_rest,  kTol);
    EXPECT_NEAR(p.t_ref,   cfg.t_ref,   kTol);
}

//=============================================================================
// L5_BETZ → tau_mem = 25 ms; v_thresh = cfg.v_thresh - 2 mV; rest unchanged.
//=============================================================================
TEST(SnnLayerSubtypesUnit, L5BetzOverridesTauMemAndVThresh)
{
    snn_config_t cfg = make_default_config();
    snn_population_t pop = make_pop(SNN_NSC_PYRAMIDAL_L5_BETZ);

    snn_lif_params_t p = snn_pop_lif_params(&pop, &cfg);

    EXPECT_NEAR(p.tau_mem, 25.0f, kTol)
        << "L5 Betz giant pyramidal: large capacitance, slow tau (Sholl 1955)";
    EXPECT_NEAR(p.v_thresh, cfg.v_thresh - 2.0f, kTol)
        << "L5 Betz must fire earlier than neighbours (lower threshold)";
    EXPECT_NEAR(p.v_reset, cfg.v_reset, kTol);
    EXPECT_NEAR(p.v_rest,  cfg.v_rest,  kTol);
    EXPECT_NEAR(p.t_ref,   cfg.t_ref,   kTol);
}

//=============================================================================
// Cross-property: under identical default v_thresh, L5_BETZ's threshold is
// strictly less than L4_STELLATE's threshold. This is the ranking the
// behavioural integration test relies on.
//=============================================================================
TEST(SnnLayerSubtypesUnit, L5BetzThresholdBelowL4Stellate)
{
    snn_config_t cfg = make_default_config();
    snn_population_t pop_betz     = make_pop(SNN_NSC_PYRAMIDAL_L5_BETZ);
    snn_population_t pop_stellate = make_pop(SNN_NSC_PYRAMIDAL_L4_STELLATE);

    snn_lif_params_t pb = snn_pop_lif_params(&pop_betz,     &cfg);
    snn_lif_params_t ps = snn_pop_lif_params(&pop_stellate, &cfg);

    EXPECT_LT(pb.v_thresh, ps.v_thresh)
        << "Ranking invariant: L5 Betz fires earlier than L4 stellate at "
           "matched drive (the behavioural integration test depends on this)";
    /* Specifically, the gap is exactly 4 mV (BETZ -2, STELLATE +2). */
    EXPECT_NEAR(ps.v_thresh - pb.v_thresh, 4.0f, kTol);
}

//=============================================================================
// Spot-check: PV branch unchanged (no duplicate of full PV/SOM/VIP/TRN
// coverage which already exists — just confirm a prior arm still fires).
//=============================================================================
TEST(SnnLayerSubtypesUnit, PvBranchUnchangedByLayerSubtypes)
{
    snn_config_t cfg = make_default_config();
    snn_population_t pop = make_pop(SNN_NSC_PV);

    snn_lif_params_t p = snn_pop_lif_params(&pop, &cfg);

    EXPECT_NEAR(p.tau_mem, 10.0f, kTol)
        << "PV fast-spiking tau (Cauli 1997) must survive the new switch arms";
    EXPECT_NEAR(p.t_ref, 1.0f, kTol);
    /* v_thresh / v_reset / v_rest unchanged — PV inherits cfg defaults. */
    EXPECT_NEAR(p.v_thresh, cfg.v_thresh, kTol);
    EXPECT_NEAR(p.v_reset,  cfg.v_reset,  kTol);
    EXPECT_NEAR(p.v_rest,   cfg.v_rest,   kTol);
}

//=============================================================================
// NULL pop → returns network defaults (silent-degrade contract).
//=============================================================================
TEST(SnnLayerSubtypesUnit, NullPopReturnsDefaults)
{
    snn_config_t cfg = make_default_config();

    snn_lif_params_t p = snn_pop_lif_params(nullptr, &cfg);

    EXPECT_NEAR(p.v_thresh, cfg.v_thresh, kTol);
    EXPECT_NEAR(p.v_reset,  cfg.v_reset,  kTol);
    EXPECT_NEAR(p.v_rest,   cfg.v_rest,   kTol);
    EXPECT_NEAR(p.tau_mem,  cfg.tau_mem,  kTol);
    EXPECT_NEAR(p.t_ref,    cfg.t_ref,    kTol);
}
