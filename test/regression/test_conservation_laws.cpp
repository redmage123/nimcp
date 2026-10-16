/**
 * @file test_conservation_laws.cpp
 * @brief Regression tests — conservation laws must NEVER be violated
 *
 * These tests verify that fundamental conservation laws hold across
 * all simulation engines over extended runs. Any failure here indicates
 * a physics-breaking bug that must be fixed before deployment.
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "cognitive/physics/nimcp_intuitive_physics.h"
#include "cognitive/physics/nimcp_electromagnetic.h"
#include "cognitive/physics/nimcp_chemistry_sim.h"
#include "cognitive/physics/nimcp_relativistic_physics.h"
#include "cognitive/physics/nimcp_world_simulator.h"
#include "cognitive/physics/nimcp_qed.h"
#include "cognitive/physics/nimcp_qft.h"
#include "cognitive/math/nimcp_zeta_functions.h"
#include "cognitive/math/nimcp_combinatorics.h"
#include "cognitive/math/nimcp_number_theory.h"
}

/* ============================================================
 * Energy Conservation
 * ============================================================ */

TEST(EnergyConservation, ElasticCollision) {
    /* Two equal spheres collide head-on in free space (no ground, no gravity).
     * With restitution=1 and friction=0, total KE must be conserved. */
    ip_config_t cfg = intuitive_physics_default_config();
    cfg.gravity_magnitude = 0;  /* disable gravity for pure elastic test */
    auto* phys = intuitive_physics_create(&cfg);
    ASSERT_NE(phys, nullptr);

    ip_object_t ball_a = {};
    ball_a.position = {-2.0f, 0.0f, 0.0f};
    ball_a.velocity = {1.0f, 0.0f, 0.0f};
    ball_a.mass = 1.0f;
    ball_a.restitution = 1.0f;
    ball_a.friction = 0.0f;
    ball_a.shape.type = IP_SHAPE_SPHERE;
    ball_a.shape.sphere.radius = 0.5f;
    ball_a.visible = true;
    ball_a.active = true;

    ip_object_t ball_b = ball_a;
    ball_b.position = {2.0f, 0.0f, 0.0f};
    ball_b.velocity = {-1.0f, 0.0f, 0.0f};

    intuitive_physics_add_object(phys, &ball_a);
    intuitive_physics_add_object(phys, &ball_b);

    /* Step once to compute initial KE, then run */
    intuitive_physics_step(phys, 0.001f);
    ip_stats_t s0 = intuitive_physics_get_stats(phys);
    float ke0 = s0.total_kinetic_energy;
    ASSERT_GT(ke0, 0.5f);  /* both balls moving at 1 m/s, mass 1 → KE = 1.0 */

    for (int i = 0; i < 1000; i++)
        intuitive_physics_step(phys, 0.01f);
    ip_stats_t s1 = intuitive_physics_get_stats(phys);

    /* KE should be conserved within 5% (no ground friction, no gravity) */
    EXPECT_NEAR(s1.total_kinetic_energy, ke0, ke0 * 0.05f);

    intuitive_physics_destroy(phys);
}

TEST(EnergyConservation, RelativisticRestEnergy) {
    /* E = mc² must hold exactly for stationary particles */
    float m_e = 9.109e-31f;  /* electron mass */
    float E = relativistic_rest_energy(m_e);
    float expected = m_e * 299792458.0f * 299792458.0f;
    EXPECT_NEAR(E, expected, expected * 1e-5f);
}

TEST(EnergyConservation, WorldSimulatorDrift) {
    /* World simulator energy drift must stay below tolerance over 100 steps */
    auto* ws = wsim_create(NULL);
    ASSERT_NE(ws, nullptr);

    for (int i = 0; i < 100; i++)
        wsim_step(ws, 0.01f);

    float e_drift, m_drift, c_drift;
    wsim_conservation_report(ws, &e_drift, &m_drift, &c_drift);

    EXPECT_LT(std::fabs(e_drift), 0.01f);  /* <1% energy drift */
    EXPECT_LT(std::fabs(m_drift), 0.01f);  /* <1% mass drift */

    wsim_destroy(ws);
}

/* ============================================================
 * Charge Conservation
 * ============================================================ */

TEST(ChargeConservation, ElectromagneticSim) {
    /* Total charge must be conserved during EM simulation */
    auto* em = em_create(NULL);
    ASSERT_NE(em, nullptr);

    em_charge_t q1 = {};
    q1.position = {0.0f, 0.0f, 0.0f};
    q1.charge = 1.602e-19f;
    q1.mass = 9.109e-31f;
    q1.active = true;
    em_add_charge(em, &q1);

    em_charge_t q2 = q1;
    q2.position = {0.01f, 0.0f, 0.0f};
    q2.charge = -1.602e-19f;
    em_add_charge(em, &q2);

    float Q_before = em_total_charge(em);
    for (int i = 0; i < 100; i++)
        em_step(em, 0);
    float Q_after = em_total_charge(em);

    EXPECT_NEAR(Q_after, Q_before, 1e-25f);  /* exact conservation */

    em_destroy(em);
}

/* ============================================================
 * Mass Conservation
 * ============================================================ */

TEST(MassConservation, ChemicalReaction) {
    /* Total mass must be conserved through chemical reactions */
    auto* chem = chemistry_sim_create(NULL);
    ASSERT_NE(chem, nullptr);

    chemistry_sim_load_common_elements(chem);
    chemistry_sim_load_common_substances(chem);
    chemistry_sim_load_common_reactions(chem);

    float mass_before = chemistry_sim_total_mass(chem);
    for (int i = 0; i < 200; i++)
        chemistry_sim_step(chem, 0.01f);
    float mass_after = chemistry_sim_total_mass(chem);

    if (mass_before > 1e-10f) {
        float drift = std::fabs(mass_after - mass_before) / mass_before;
        EXPECT_LT(drift, 0.01f);  /* <1% mass drift */
    }

    chemistry_sim_destroy(chem);
}

/* ============================================================
 * Exact Known Values (regression baselines)
 * ============================================================ */

TEST(ExactValues, PiSquaredOver6) {
    /* ζ(2) = π²/6 — Basel problem, must match to 10 digits */
    double z2 = zeta_basel();
    EXPECT_NEAR(z2, M_PI * M_PI / 6.0, 1e-12);
}

TEST(ExactValues, ThomsonCrossSection) {
    /* σ_T = 6.6524587321e-29 m² — exact QED result */
    float sigma = qed_thomson_cross_section();
    EXPECT_NEAR(sigma, 6.6524e-29f, 1e-32f);
}

TEST(ExactValues, AnomalousMoment) {
    /* a_e = 0.00115965218... — most precisely tested prediction in physics */
    float ae = qed_anomalous_moment_third_order();
    EXPECT_NEAR(ae, 0.001159652f, 1e-7f);
}

TEST(ExactValues, AlphaStrong) {
    /* α_s(M_Z) = 0.1180 ± 0.0009 — PDG world average */
    float as = qft_alpha_s(91.2f);
    EXPECT_NEAR(as, 0.1180f, 0.002f);
}

TEST(ExactValues, AvogadroConsistency) {
    /* Boltzmann × Avogadro = R (gas constant) */
    float kb = 1.381e-23f;
    float Na = 6.022e23f;
    float R = kb * Na;
    EXPECT_NEAR(R, 8.314f, 0.01f);
}

TEST(ExactValues, Factorial10) {
    /* 10! = 3628800 — must be exact */
    auto* ctx = combinatorics_create();
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(comb_factorial(ctx, 10), 3628800ULL);
    combinatorics_destroy(ctx);
}

TEST(ExactValues, Binomial10_3) {
    auto* ctx = combinatorics_create();
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(comb_binomial(ctx, 10, 3), 120ULL);
    combinatorics_destroy(ctx);
}

TEST(ExactValues, IsPrime997) {
    EXPECT_TRUE(nt_is_prime_miller_rabin(997, 12));
    EXPECT_FALSE(nt_is_prime_miller_rabin(998, 12));
}
