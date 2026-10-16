//=============================================================================
// test_snn_dendritic_unit.cpp — Wave H unit tests
//=============================================================================
/**
 * @file test_snn_dendritic_unit.cpp
 * @brief Wave H — Unit tests for the two-compartment dendritic helpers and
 *        the NMDA plateau onset detector.
 *
 * WHAT: Verifies snn_membrane_compute_dv_two_compartment math, plateau-onset
 *       detector, plateau decay envelope, and the runtime
 *       snn_tune_set/get_dendritic_enabled idempotency.
 * WHY:  These header-inline helpers are the single source of truth for the
 *       Wave H two-compartment integration. Bugs here corrupt every
 *       dendritic-mode SNN pop.
 * HOW:  Google Test. All tests on the pure header helpers — no SNN network
 *       construction needed for the unit tier.
 *
 * See docs/claude/wave-h-dendritic-design-2026-04-27.md.
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "snn/nimcp_snn_membrane.h"
}

/* dendritic flag setter/getter live in nimcp_snn_training.c */
extern "C" {
    void  snn_tune_set_dendritic_enabled(float v);
    float snn_tune_get_dendritic_enabled(void);
}

namespace {

constexpr float kE_AMPA   =   0.0f;
constexpr float kE_NMDA   =   0.0f;
constexpr float kE_GABA_A = -70.0f;
constexpr float kE_GABA_B = -90.0f;
constexpr float kV_REST   = -65.0f;
constexpr float kTAU_B    =  10.0f;
constexpr float kTAU_A    =  20.0f;  /* apical typically slower than basal */
constexpr float kDT       =   1.0f;
constexpr float kMG       =   1.0f;
constexpr float kG_COUP   = SNN_DEND_G_COUP_DEFAULT;
constexpr float kPL_GAIN  = SNN_DEND_PLATEAU_GAIN;
constexpr float kPL_TAU   = SNN_DEND_PLATEAU_TAU_MS;
constexpr float kPL_THR   = SNN_DEND_V_PLATEAU_THRESHOLD_MV;

//-----------------------------------------------------------------------------
// 1. Rest equilibrium with zero conductances → both dv ≈ 0.
//-----------------------------------------------------------------------------
TEST(WaveHDendriticUnit, RestEquilibriumZeroDrive) {
    float dv_b = 999.0f, dv_a = 999.0f;
    snn_membrane_compute_dv_two_compartment(
        kV_REST, kV_REST,
        kV_REST, kTAU_B, kTAU_A, kDT,
        0.0f, 0.0f, 0.0f, 0.0f,
        kG_COUP,
        kE_AMPA, kE_NMDA, kE_GABA_A, kE_GABA_B,
        kMG,
        false, 0.0f, kPL_GAIN, kPL_TAU,
        &dv_b, &dv_a);
    EXPECT_NEAR(dv_b, 0.0f, 1e-5f);
    EXPECT_NEAR(dv_a, 0.0f, 1e-5f);
}

//-----------------------------------------------------------------------------
// 2. AMPA-on-basal raises basal V toward E_AMPA (depolarizing).
//-----------------------------------------------------------------------------
TEST(WaveHDendriticUnit, AmpaOnBasalDepolarizesBasal) {
    float dv_b = 0.0f, dv_a = 0.0f;
    snn_membrane_compute_dv_two_compartment(
        kV_REST, kV_REST,
        kV_REST, kTAU_B, kTAU_A, kDT,
        /* g_ampa_b */ 1.0f, 0.0f, 0.0f, 0.0f,
        kG_COUP,
        kE_AMPA, kE_NMDA, kE_GABA_A, kE_GABA_B,
        kMG,
        false, 0.0f, kPL_GAIN, kPL_TAU,
        &dv_b, &dv_a);
    EXPECT_GT(dv_b, 0.0f);  /* basal moves toward 0 mV — positive dv */
    /* Apical sees only the gap-junction pull from a depolarizing basal,
     * so it should also be ≥ 0 (initially basal == apical so coupling is
     * zero; on this single step apical drive is purely leak which is 0). */
    EXPECT_NEAR(dv_a, 0.0f, 0.1f);
}

//-----------------------------------------------------------------------------
// 3. NMDA-on-apical at rest is silent (Mg block intact).
//-----------------------------------------------------------------------------
TEST(WaveHDendriticUnit, NmdaApicalAtRestSilent) {
    float dv_b = 0.0f, dv_a = 0.0f;
    snn_membrane_compute_dv_two_compartment(
        kV_REST, kV_REST,
        kV_REST, kTAU_B, kTAU_A, kDT,
        0.0f, 0.0f, /* g_nmda_a */ 1.0f, 0.0f,
        kG_COUP,
        kE_AMPA, kE_NMDA, kE_GABA_A, kE_GABA_B,
        kMG,
        false, 0.0f, kPL_GAIN, kPL_TAU,
        &dv_b, &dv_a);
    /* Apical at -65 mV: Mg block ≈ 0.06 → very weak NMDA drive. dv_a should
     * be small (< 1 mV) even with g_nmda = 1. */
    EXPECT_LT(std::fabs(dv_a), 1.0f);
}

//-----------------------------------------------------------------------------
// 4. NMDA-on-apical at depolarized (-50) is unblocked. Verified by isolating
//    the NMDA contribution: dv_with_nmda - dv_without_nmda. This contribution
//    must be larger at -50 mV than at -65 mV (Mg block partially relieved).
//-----------------------------------------------------------------------------
TEST(WaveHDendriticUnit, NmdaApicalDepolarizedUnblocked) {
    auto dv_apical = [](float v_apical, float g_nmda) {
        float dv_b = 0.0f, dv_a = 0.0f;
        snn_membrane_compute_dv_two_compartment(
            kV_REST, v_apical,
            kV_REST, kTAU_B, kTAU_A, kDT,
            0.0f, 0.0f, g_nmda, 0.0f,
            /* g_coup */ 0.0f,  /* decouple — isolate NMDA */
            kE_AMPA, kE_NMDA, kE_GABA_A, kE_GABA_B,
            kMG,
            false, 0.0f, kPL_GAIN, kPL_TAU,
            &dv_b, &dv_a);
        return dv_a;
    };
    /* Isolate NMDA contribution at each voltage by subtracting off the
     * baseline (g_nmda = 0). This removes the leak term so we see only
     * what NMDA contributed. */
    const float nmda_at_rest = dv_apical(kV_REST, 1.0f) - dv_apical(kV_REST, 0.0f);
    const float nmda_at_dep  = dv_apical(-50.0f, 1.0f) - dv_apical(-50.0f, 0.0f);
    /* At rest, Mg block ≈ 0.06 (heavily blocked) → tiny contribution.
     * At -50 mV, block ≈ 0.13 (partially relieved) → larger absolute
     * contribution per unit g_nmda. Even though driving force (E_nmda - V)
     * is smaller at -50 (50 mV) than at -65 (65 mV), the block-relief
     * factor more than makes up for it for this V range. */
    EXPECT_GT(std::fabs(nmda_at_dep), std::fabs(nmda_at_rest));
    /* Both should be depolarizing (positive — driving toward E_NMDA = 0). */
    EXPECT_GT(nmda_at_dep,  0.0f);
    EXPECT_GT(nmda_at_rest, 0.0f);
}

//-----------------------------------------------------------------------------
// 5. Electrotonic coupling pulls compartments toward shared V.
//-----------------------------------------------------------------------------
TEST(WaveHDendriticUnit, ElectrotonicCouplingPullsTogether) {
    float dv_b = 0.0f, dv_a = 0.0f;
    /* Basal at -55, apical at -75 (artificial split). With g_coup > 0,
     * basal is pulled NEGATIVE (toward apical) and apical is pulled
     * POSITIVE (toward basal). */
    snn_membrane_compute_dv_two_compartment(
        /* v_basal  */ -55.0f,
        /* v_apical */ -75.0f,
        kV_REST, kTAU_B, kTAU_A, kDT,
        0.0f, 0.0f, 0.0f, 0.0f,
        kG_COUP,
        kE_AMPA, kE_NMDA, kE_GABA_A, kE_GABA_B,
        kMG,
        false, 0.0f, kPL_GAIN, kPL_TAU,
        &dv_b, &dv_a);
    /* Basal sees: leak (rest - v) = -65 - (-55) = -10 (pulls negative)
     *           + g_coup * (v_apical - v_basal) = 0.05 * (-75 - -55) = -1
     * → dv_b is negative.
     * Apical sees: leak = -65 - (-75) = +10 (pulls positive)
     *            + g_coup * (v_basal - v_apical) = 0.05 * 20 = +1
     * → dv_a is positive. */
    EXPECT_LT(dv_b, 0.0f);
    EXPECT_GT(dv_a, 0.0f);
}

//-----------------------------------------------------------------------------
// 6. Plateau onset detector: apical V crossing -40 mV returns true.
//-----------------------------------------------------------------------------
TEST(WaveHDendriticUnit, PlateauOnsetDetector) {
    EXPECT_FALSE(snn_membrane_check_plateau_onset(-65.0f, kPL_THR));
    EXPECT_FALSE(snn_membrane_check_plateau_onset(-50.0f, kPL_THR));
    EXPECT_TRUE (snn_membrane_check_plateau_onset(-40.0f, kPL_THR));  /* edge */
    EXPECT_TRUE (snn_membrane_check_plateau_onset(-30.0f, kPL_THR));
    EXPECT_TRUE (snn_membrane_check_plateau_onset(  0.0f, kPL_THR));
}

//-----------------------------------------------------------------------------
// 7. Plateau drive decay envelope: at t=0 drive ≈ plateau_gain; at t=tau
//    drive ≈ plateau_gain / e; at t=3τ drive < 5 % of peak.
//
//    We can't isolate the plateau drive from compute_dv_two_compartment in a
//    pure-math sense, so we test by running with everything zero except the
//    plateau, with v_basal = v_apical = v_rest, g_coup = 0 (decoupled) — the
//    apical dv is then drive_a = leak + plateau_drive = 0 + plateau_drive.
//    With tau_a = 1 ms and dt = 1 ms, dv_a ≈ plateau_drive (no leak since
//    v_apical == v_rest).
//-----------------------------------------------------------------------------
TEST(WaveHDendriticUnit, PlateauDecayEnvelope) {
    auto compute_apical_dv = [](float t_since) {
        float dv_b = 0.0f, dv_a = 0.0f;
        snn_membrane_compute_dv_two_compartment(
            kV_REST, kV_REST,
            kV_REST, /* tau_b */ 1.0f, /* tau_a */ 1.0f, kDT,
            0.0f, 0.0f, 0.0f, 0.0f,
            /* g_coup */ 0.0f,
            kE_AMPA, kE_NMDA, kE_GABA_A, kE_GABA_B,
            kMG,
            true, t_since, kPL_GAIN, kPL_TAU,
            &dv_b, &dv_a);
        return dv_a;
    };
    const float dv_at_0    = compute_apical_dv(0.0f);
    const float dv_at_tau  = compute_apical_dv(kPL_TAU);
    const float dv_at_3tau = compute_apical_dv(3.0f * kPL_TAU);

    /* Onset (t=0): drive == plateau_gain × exp(0) == plateau_gain.
     * With tau_a=dt=1, dv_a is approximately plateau_gain (modulo the dt/tau
     * factor of 1; leak is zero since v_apical == v_rest). */
    EXPECT_NEAR(dv_at_0, kPL_GAIN, 1e-4f);
    /* At 1 τ, drive should be plateau_gain / e (≈ 0.18 with default 0.5). */
    EXPECT_NEAR(dv_at_tau, kPL_GAIN / std::exp(1.0f), 1e-3f);
    /* At 3 τ drive should be below 5 % of peak. */
    EXPECT_LT(dv_at_3tau, 0.05f * kPL_GAIN);
    /* Strict ordering. */
    EXPECT_GT(dv_at_0,   dv_at_tau);
    EXPECT_GT(dv_at_tau, dv_at_3tau);
}

//-----------------------------------------------------------------------------
// 8. Plateau active=false suppresses drive entirely (one Heaviside guard).
//-----------------------------------------------------------------------------
TEST(WaveHDendriticUnit, PlateauInactiveZerosDrive) {
    float dv_b_off = 0.0f, dv_a_off = 0.0f;
    float dv_b_on  = 0.0f, dv_a_on  = 0.0f;
    snn_membrane_compute_dv_two_compartment(
        kV_REST, kV_REST, kV_REST, 1.0f, 1.0f, kDT,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        kE_AMPA, kE_NMDA, kE_GABA_A, kE_GABA_B,
        kMG,
        false, 0.0f, kPL_GAIN, kPL_TAU,
        &dv_b_off, &dv_a_off);
    snn_membrane_compute_dv_two_compartment(
        kV_REST, kV_REST, kV_REST, 1.0f, 1.0f, kDT,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        kE_AMPA, kE_NMDA, kE_GABA_A, kE_GABA_B,
        kMG,
        true, 0.0f, kPL_GAIN, kPL_TAU,
        &dv_b_on, &dv_a_on);
    EXPECT_NEAR(dv_a_off, 0.0f, 1e-6f);
    EXPECT_GT(dv_a_on, 0.0f);
}

//-----------------------------------------------------------------------------
// 9. Runtime flag setter/getter idempotency.
//-----------------------------------------------------------------------------
TEST(WaveHDendriticUnit, RuntimeFlagSetGetIdempotent) {
    /* Save default and restore at end (unit tests must not leak state). */
    const float saved = snn_tune_get_dendritic_enabled();

    snn_tune_set_dendritic_enabled(0.0f);
    EXPECT_EQ(snn_tune_get_dendritic_enabled(), 0.0f);

    snn_tune_set_dendritic_enabled(1.0f);
    EXPECT_EQ(snn_tune_get_dendritic_enabled(), 1.0f);

    /* Bool semantics: any nonzero is on; expect canonical 1.0 readback. */
    snn_tune_set_dendritic_enabled(42.0f);
    EXPECT_EQ(snn_tune_get_dendritic_enabled(), 1.0f);

    /* Idempotent re-set. */
    snn_tune_set_dendritic_enabled(1.0f);
    EXPECT_EQ(snn_tune_get_dendritic_enabled(), 1.0f);
    snn_tune_set_dendritic_enabled(1.0f);
    EXPECT_EQ(snn_tune_get_dendritic_enabled(), 1.0f);

    /* Restore. */
    snn_tune_set_dendritic_enabled(saved);
}

//-----------------------------------------------------------------------------
// 10. Compartmental deposit helper: AMPA → basal-AMPA, NMDA → apical-NMDA.
//-----------------------------------------------------------------------------
TEST(WaveHDendriticUnit, CompartmentalDepositRouting) {
    float i_syn = 0.0f;
    float g_ampa = 0.0f, g_nmda = 0.0f, g_gaba_a = 0.0f, g_gaba_b = 0.0f;
    float g_ampa_b = 0.0f, g_gaba_a_b = 0.0f;
    float g_nmda_a = 0.0f, g_gaba_b_a = 0.0f;

    /* Dendritic ON, CB ON → AMPA goes to basal */
    snn_membrane_deposit_synapse_compartmental(
        &i_syn,
        &g_ampa, &g_nmda, &g_gaba_a, &g_gaba_b,
        &g_ampa_b, &g_gaba_a_b,
        &g_nmda_a, &g_gaba_b_a,
        2.5f, SYNAPSE_AMPA, /*cb*/true, /*dend*/true);
    EXPECT_NEAR(g_ampa_b, 2.5f, 1e-6f);
    EXPECT_NEAR(g_ampa,   0.0f, 1e-6f);

    /* NMDA goes to apical */
    snn_membrane_deposit_synapse_compartmental(
        &i_syn,
        &g_ampa, &g_nmda, &g_gaba_a, &g_gaba_b,
        &g_ampa_b, &g_gaba_a_b,
        &g_nmda_a, &g_gaba_b_a,
        1.5f, SYNAPSE_NMDA, true, true);
    EXPECT_NEAR(g_nmda_a, 1.5f, 1e-6f);
    EXPECT_NEAR(g_nmda,   0.0f, 1e-6f);

    /* GABA_A goes to basal (perisomatic) */
    snn_membrane_deposit_synapse_compartmental(
        &i_syn,
        &g_ampa, &g_nmda, &g_gaba_a, &g_gaba_b,
        &g_ampa_b, &g_gaba_a_b,
        &g_nmda_a, &g_gaba_b_a,
        3.0f, SYNAPSE_GABA_A, true, true);
    EXPECT_NEAR(g_gaba_a_b, 3.0f, 1e-6f);

    /* GABA_B goes to apical */
    snn_membrane_deposit_synapse_compartmental(
        &i_syn,
        &g_ampa, &g_nmda, &g_gaba_a, &g_gaba_b,
        &g_ampa_b, &g_gaba_a_b,
        &g_nmda_a, &g_gaba_b_a,
        4.0f, SYNAPSE_GABA_B, true, true);
    EXPECT_NEAR(g_gaba_b_a, 4.0f, 1e-6f);

    /* Dendritic OFF: should fall through to legacy single-compartment. */
    g_ampa = 0.0f;
    snn_membrane_deposit_synapse_compartmental(
        &i_syn,
        &g_ampa, &g_nmda, &g_gaba_a, &g_gaba_b,
        nullptr, nullptr, nullptr, nullptr,
        7.0f, SYNAPSE_AMPA, /*cb*/true, /*dend*/false);
    EXPECT_NEAR(g_ampa, 7.0f, 1e-6f);
}

}  // namespace
