//=============================================================================
// test_snn_membrane_unit.cpp — Unit tests for CB membrane helpers
//=============================================================================
/**
 * @file test_snn_membrane_unit.cpp
 * @brief Pure-function tests for nimcp_snn_membrane.h.
 *
 * Migrated to the 4-receptor API (P0). What were `g_exc`/`g_inh` lumps
 * are tested here as g_ampa / g_gaba_a with NMDA + GABA_B set to 0 and
 * Mg²⁺ disabled (mg_mm = 0) — preserving the original lumped-CB semantics
 * end-to-end. Per-receptor coverage (NMDA Mg block, four-decay routing,
 * etc.) lives in test_snn_per_receptor.c.
 */

#include <gtest/gtest.h>
#include <cmath>
#include "core/synapse_types/nimcp_synapse_types.h"

extern "C" {
#include "snn/nimcp_snn_membrane.h"
}

// Compact wrappers so each call site reads like the legacy 2-receptor form.
static inline float dv2(float v, float v_rest, float tau, float dt,
                        float i_syn, float g_exc, float g_inh,
                        float e_exc, float e_inh, bool cb) {
    return snn_membrane_compute_dv(
        v, v_rest, tau, dt, i_syn,
        g_exc, /*g_nmda=*/0.0f, g_inh, /*g_gaba_b=*/0.0f,
        e_exc, /*e_nmda=*/0.0f, e_inh, /*e_gaba_b=*/-90.0f,
        /*mg_mm=*/0.0f, cb);
}

static inline void decay2(float* g_exc, float* g_inh, float decay_e, float decay_i) {
    snn_membrane_decay_one(g_exc, /*g_nmda=*/nullptr, g_inh, /*g_gaba_b=*/nullptr,
                           decay_e, /*decay_nmda=*/0.0f, decay_i, /*decay_gaba_b=*/0.0f);
}

static inline void deposit2(float* i_syn, float* g_exc, float* g_inh,
                            float w, bool cb) {
    int syn_type = (w >= 0.0f) ? SYNAPSE_AMPA : SYNAPSE_GABA_A;
    snn_membrane_deposit_synapse(i_syn, g_exc, /*g_nmda=*/nullptr,
                                 g_inh, /*g_gaba_b=*/nullptr,
                                 w, syn_type, cb);
}

//=============================================================================
// snn_membrane_compute_dv — current mode (OFF, must be bit-identical)
//=============================================================================

TEST(SnnMembraneComputeDv, CurrentMode_RestEquilibrium_DvZero) {
    float dv = dv2(-65.0f, -65.0f, 20.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -80.0f, false);
    EXPECT_FLOAT_EQ(dv, 0.0f);
}

TEST(SnnMembraneComputeDv, CurrentMode_BelowRest_PullsUp) {
    float dv = dv2(-75.0f, -65.0f, 20.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -80.0f, false);
    EXPECT_NEAR(dv, 0.5f, 1e-5f);
}

TEST(SnnMembraneComputeDv, CurrentMode_BitIdenticalToLegacyEquation) {
    const float v_rest = -65.0f, tau = 20.0f, dt = 1.0f;
    for (float v = -80; v <= -40; v += 5) {
        for (float i_syn = -20; i_syn <= 20; i_syn += 5) {
            float expected = (v_rest - v + i_syn) / tau * dt;
            float dv = dv2(v, v_rest, tau, dt, i_syn,
                           0.0f, 0.0f, 0.0f, -80.0f, false);
            EXPECT_FLOAT_EQ(dv, expected)
                << "Mismatch at v=" << v << " I_syn=" << i_syn;
        }
    }
}

//=============================================================================
// snn_membrane_compute_dv — conductance mode (lumped: g_ampa + g_gaba_a)
//=============================================================================

TEST(SnnMembraneComputeDv, CbMode_RestNoConductance_DvZero) {
    float dv = dv2(-65.0f, -65.0f, 20.0f, 1.0f,
                   /*i_syn=*/123.0f /* must be ignored in CB */,
                   0.0f, 0.0f, 0.0f, -80.0f, true);
    EXPECT_FLOAT_EQ(dv, 0.0f);
}

TEST(SnnMembraneComputeDv, CbMode_ExcitatoryConductance_DepolarizesTowardEexc) {
    float v = -65.0f, e_exc = 0.0f;
    float g_exc = 0.5f;
    float dv = dv2(v, -65.0f, 20.0f, 1.0f, 0.0f,
                   g_exc, 0.0f, e_exc, -80.0f, true);
    EXPECT_NEAR(dv, 1.625f, 1e-5f);
    EXPECT_GT(dv, 0.0f);
}

TEST(SnnMembraneComputeDv, CbMode_InhibitoryConductance_HyperpolarizesTowardEinh) {
    float v = -65.0f, e_inh = -80.0f;
    float g_inh = 0.3f;
    float dv = dv2(v, -65.0f, 20.0f, 1.0f, 0.0f,
                   0.0f, g_inh, 0.0f, e_inh, true);
    EXPECT_NEAR(dv, -0.225f, 1e-5f);
    EXPECT_LT(dv, 0.0f);
}

TEST(SnnMembraneComputeDv, CbMode_NeverOvershootsEexc) {
    float v = -65.0f;
    const float g_exc = 1.0f;
    for (int step = 0; step < 1000; step++) {
        float dv = dv2(v, -65.0f, 20.0f, 1.0f, 0.0f,
                       g_exc, 0.0f, 0.0f, -80.0f, true);
        v += dv;
        EXPECT_LE(v, 0.05f) << "V exceeded E_exc at step " << step;
    }
    EXPECT_NEAR(v, -32.5f, 0.5f);
}

TEST(SnnMembraneComputeDv, CbMode_StrongerGexcKeepsVNearEexc) {
    float v = -65.0f;
    const float g_exc = 100.0f;
    float v_min_after_settling = 1e9f;
    for (int step = 0; step < 1000; step++) {
        float dv = dv2(v, -65.0f, 20.0f, 1.0f, 0.0f,
                       g_exc, 0.0f, 0.0f, -80.0f, true);
        v += dv;
        EXPECT_LE(v, 0.05f) << "V exceeded E_exc at step " << step;
        if (step >= 100 && v < v_min_after_settling) v_min_after_settling = v;
    }
    EXPECT_GE(v_min_after_settling, -5.0f)
        << "V drifted further than 5 mV from E_exc with g_exc=100";
}

TEST(SnnMembraneComputeDv, CbMode_NeverUndershootsEinh) {
    float v = -65.0f;
    const float g_inh = 1.0f;
    for (int step = 0; step < 1000; step++) {
        float dv = dv2(v, -65.0f, 20.0f, 1.0f, 0.0f,
                       0.0f, g_inh, 0.0f, -80.0f, true);
        v += dv;
        EXPECT_GE(v, -80.05f) << "V undershot E_inh at step " << step;
    }
    EXPECT_NEAR(v, -72.5f, 0.5f);
}

//=============================================================================
// dv clamp (biophysical guard)
//=============================================================================

TEST(SnnMembraneComputeDv, DvClamp_PositiveOverflowClampedTo100) {
    float dv = dv2(-65.0f, -65.0f, /*tau=*/0.001f, /*dt=*/1.0f,
                   /*i_syn=*/10000.0f,
                   0.0f, 0.0f, 0.0f, -80.0f, false);
    EXPECT_LE(dv, 100.0f);
    EXPECT_FLOAT_EQ(dv, 100.0f);
}

TEST(SnnMembraneComputeDv, DvClamp_NegativeOverflowClampedToNeg100) {
    float dv = dv2(-65.0f, -65.0f, 0.001f, 1.0f,
                   /*i_syn=*/-10000.0f,
                   0.0f, 0.0f, 0.0f, -80.0f, false);
    EXPECT_GE(dv, -100.0f);
    EXPECT_FLOAT_EQ(dv, -100.0f);
}

TEST(SnnMembraneComputeDv, DvClamp_NormalRangeUnaffected) {
    float dv = dv2(-65.0f, -65.0f, 20.0f, 1.0f,
                   /*i_syn=*/15.0f,
                   0.0f, 0.0f, 0.0f, -80.0f, false);
    EXPECT_LT(dv, 5.0f);
    EXPECT_GT(dv, 0.0f);
    EXPECT_FLOAT_EQ(dv, 15.0f / 20.0f);
}

TEST(SnnMembraneComputeDv, TauZeroFloored_DoesNotCrashOrReturnNaN) {
    float dv = dv2(-65.0f, -65.0f, 0.0f, 1.0f,
                   1.0f, 0.0f, 0.0f, 0.0f, -80.0f, false);
    EXPECT_TRUE(std::isfinite(dv));
}

//=============================================================================
// snn_membrane_decay_one
//=============================================================================

TEST(SnnMembraneDecayOne, ScalesByDecayFactor) {
    float g_exc = 1.0f, g_inh = 0.5f;
    decay2(&g_exc, &g_inh, 0.5f, 0.25f);
    EXPECT_FLOAT_EQ(g_exc, 0.5f);
    EXPECT_FLOAT_EQ(g_inh, 0.125f);
}

TEST(SnnMembraneDecayOne, RepeatedDecayConvergesToZero) {
    float g = 1.0f, g_unused = 0.0f;
    const float decay = 0.9f;
    for (int i = 0; i < 100; i++) {
        decay2(&g, &g_unused, decay, decay);
    }
    EXPECT_LT(g, 0.001f);
    EXPECT_FLOAT_EQ(g_unused, 0.0f);
}

TEST(SnnMembraneDecayOne, NullPointersAreNoOp) {
    decay2(nullptr, nullptr, 0.5f, 0.5f);
    SUCCEED();
}

TEST(SnnMembraneDecayOne, OnlyExcSet_OnlyExcDecays) {
    float g_exc = 1.0f;
    decay2(&g_exc, nullptr, 0.5f, 0.5f);
    EXPECT_FLOAT_EQ(g_exc, 0.5f);
}

//=============================================================================
// snn_membrane_deposit_synapse
//=============================================================================

TEST(SnnMembraneDeposit, CurrentMode_PositiveWeight_AddsToISyn) {
    float i_syn = 0.0f, g_exc = 0.0f, g_inh = 0.0f;
    deposit2(&i_syn, &g_exc, &g_inh, 5.0f, /*cb=*/false);
    EXPECT_FLOAT_EQ(i_syn, 5.0f);
    EXPECT_FLOAT_EQ(g_exc, 0.0f);
    EXPECT_FLOAT_EQ(g_inh, 0.0f);
}

TEST(SnnMembraneDeposit, CurrentMode_NegativeWeight_AddsToISyn) {
    float i_syn = 10.0f, g_exc = 0.0f, g_inh = 0.0f;
    deposit2(&i_syn, &g_exc, &g_inh, -3.0f, false);
    EXPECT_FLOAT_EQ(i_syn, 7.0f);
    EXPECT_FLOAT_EQ(g_exc, 0.0f);
    EXPECT_FLOAT_EQ(g_inh, 0.0f);
}

TEST(SnnMembraneDeposit, CbMode_PositiveWeight_RoutesToGexc) {
    float i_syn = 0.0f, g_exc = 0.1f, g_inh = 0.2f;
    deposit2(&i_syn, &g_exc, &g_inh, 0.5f, /*cb=*/true);
    EXPECT_FLOAT_EQ(i_syn, 0.0f);
    EXPECT_FLOAT_EQ(g_exc, 0.6f);
    EXPECT_FLOAT_EQ(g_inh, 0.2f);
}

TEST(SnnMembraneDeposit, CbMode_NegativeWeight_RoutesAbsToGinh) {
    float i_syn = 0.0f, g_exc = 0.1f, g_inh = 0.2f;
    deposit2(&i_syn, &g_exc, &g_inh, -0.5f, true);
    EXPECT_FLOAT_EQ(i_syn, 0.0f);
    EXPECT_FLOAT_EQ(g_exc, 0.1f);
    EXPECT_FLOAT_EQ(g_inh, 0.7f);
}

TEST(SnnMembraneDeposit, CbMode_ZeroWeight_RoutesToGexc_NoChange) {
    float i_syn = 0.0f, g_exc = 0.5f, g_inh = 0.5f;
    deposit2(&i_syn, &g_exc, &g_inh, 0.0f, true);
    EXPECT_FLOAT_EQ(g_exc, 0.5f);
    EXPECT_FLOAT_EQ(g_inh, 0.5f);
}

TEST(SnnMembraneDeposit, CbMode_NullGexc_PositiveWeightDropped) {
    float i_syn = 0.0f, g_inh = 0.0f;
    deposit2(&i_syn, /*g_exc=*/nullptr, &g_inh, 1.0f, true);
    EXPECT_FLOAT_EQ(i_syn, 0.0f);
    EXPECT_FLOAT_EQ(g_inh, 0.0f);
}

TEST(SnnMembraneDeposit, CurrentMode_NullISyn_NoOpNoCrash) {
    deposit2(nullptr, nullptr, nullptr, 5.0f, false);
    SUCCEED();
}

//=============================================================================
// Composition: deposit + decay + integrate over multiple steps
//=============================================================================

TEST(SnnMembraneComposition, CbMode_SingleSpikeDecaysWithTau) {
    float g_exc = 0.0f, g_inh = 0.0f;
    deposit2(/*i_syn=*/nullptr, &g_exc, &g_inh, 1.0f, true);
    EXPECT_FLOAT_EQ(g_exc, 1.0f);

    const float decay = std::exp(-1.0f / 2.0f);
    for (int step = 0; step < 10; step++) {
        decay2(&g_exc, &g_inh, decay, decay);
    }
    EXPECT_LT(g_exc, 0.01f) << "g_exc did not decay to <1% in 10 steps";
}

TEST(SnnMembraneComposition, CbMode_BalancedEandI_VStaysAtRest) {
    float v = -65.0f, g_exc = 0.1f, g_inh = 0.1f * (65.0f / 15.0f);
    for (int step = 0; step < 100; step++) {
        float dv = dv2(v, -65.0f, 20.0f, 1.0f, 0.0f,
                       g_exc, g_inh, 0.0f, -80.0f, true);
        v += dv;
    }
    EXPECT_NEAR(v, -65.0f, 0.5f);
}
