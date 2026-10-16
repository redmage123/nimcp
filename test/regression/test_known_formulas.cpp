/**
 * @file test_known_formulas.cpp
 * @brief Regression tests — known formula results that must never change
 *
 * These are the "gold standard" values. If any of these fail, something
 * fundamental has broken in the simulation engines.
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "cognitive/physics/nimcp_surface_physics.h"
#include "cognitive/physics/nimcp_surface_chemistry.h"
#include "cognitive/physics/nimcp_relativistic_physics.h"
#include "cognitive/physics/nimcp_qed.h"
#include "cognitive/physics/nimcp_astrophysics.h"
#include "cognitive/physics/nimcp_nuclear_physics.h"
}

/* ============================================================
 * Physics Formulas
 * ============================================================ */

TEST(PhysicsFormulas, LorentzGamma) {
    /* γ(0) = 1, γ(0.5c) = 1.1547, γ(0.99c) = 7.0888 */
    wm_parietal_vec3_t v0 = {0, 0, 0};
    EXPECT_NEAR(relativistic_gamma(v0), 1.0f, 1e-6f);

    float c = 299792458.0f;
    wm_parietal_vec3_t v5 = {0, c * 0.5f, 0};
    EXPECT_NEAR(relativistic_gamma(v5), 1.1547f, 0.001f);

    wm_parietal_vec3_t v99 = {c * 0.99f, 0, 0};
    EXPECT_NEAR(relativistic_gamma(v99), 7.0888f, 0.01f);
}

TEST(PhysicsFormulas, CapillaryPressure) {
    /* ΔP = 2γ/R for a sphere. Water (γ=0.0728), R=1mm → 145.6 Pa */
    float dp = surface_physics_capillary_pressure(0.0728f, 0.001f, 0.001f);
    EXPECT_NEAR(dp, 145.6f, 0.5f);
}

TEST(PhysicsFormulas, FresnelNormalIncidence) {
    /* R = ((n1-n2)/(n1+n2))² = ((1-1.5)/(1+1.5))² = 0.04 */
    float R = surface_physics_fresnel_normal(1.0f, 1.5f);
    EXPECT_NEAR(R, 0.04f, 0.001f);
}

TEST(PhysicsFormulas, CriticalAngle) {
    /* θ_c = arcsin(n2/n1) = arcsin(1/1.5) ≈ 41.8° */
    float tc = surface_physics_critical_angle(1.5f, 1.0f);
    EXPECT_NEAR(tc * 180.0f / M_PI, 41.81f, 0.1f);
}

TEST(PhysicsFormulas, SchwarzschildRadius) {
    /* r_s(Sun) = 2GM/c² ≈ 2954 m */
    float rs = astro_schwarzschild_radius(1.989e30f);
    EXPECT_NEAR(rs, 2954.0f, 10.0f);
}

TEST(PhysicsFormulas, EscapeVelocity) {
    /* v_esc(Earth) = sqrt(2GM/R) ≈ 11186 m/s */
    float ve = astro_escape_velocity(5.972e24f, 6.371e6f);
    EXPECT_NEAR(ve, 11186.0f, 50.0f);
}

TEST(PhysicsFormulas, KeplerThirdLaw) {
    /* T(Earth) = 2π√(a³/GM_sun) ≈ 3.156e7 s (1 year) */
    float T = astro_orbital_period(1.496e11f, 1.989e30f);
    EXPECT_NEAR(T, 3.156e7f, 1e5f);  /* within ~1 day */
}

TEST(PhysicsFormulas, BindingEnergyFe56) {
    /* Fe-56: most tightly bound nucleus, ~8.79 MeV/nucleon */
    float bpa = np_binding_energy_per_nucleon(26, 56);
    EXPECT_NEAR(bpa, 8.79f, 0.15f);
}

/* ============================================================
 * Chemistry Formulas
 * ============================================================ */

TEST(ChemistryFormulas, Langmuir) {
    /* θ = KP/(1+KP). At K=0.01, P=100: θ = 1/(1+1) = 0.5 */
    float theta = surface_chemistry_langmuir(0.01f, 100.0f);
    EXPECT_NEAR(theta, 0.5f, 0.001f);
}

TEST(ChemistryFormulas, Nernst) {
    /* E = E° - (RT/nF)ln(Q). At Q=1: E = E° */
    float E = surface_chemistry_nernst(0.34f, 298.15f, 2, 1.0f);
    EXPECT_NEAR(E, 0.34f, 0.001f);
}

TEST(ChemistryFormulas, Arrhenius) {
    /* k = A·exp(-Ea/RT). Check rate increases with temperature */
    float k300 = surface_chemistry_arrhenius(1e13f, 60.0f, 300.0f);
    float k500 = surface_chemistry_arrhenius(1e13f, 60.0f, 500.0f);
    EXPECT_GT(k500, k300);
    EXPECT_GT(k500, 0.0f);
}

/* ============================================================
 * QED Formulas
 * ============================================================ */

TEST(QEDFormulas, ThomsonExact) {
    /* σ_T = 8π/3 · r_e² = 6.6524587321e-29 m² */
    float sigma = qed_thomson_cross_section();
    EXPECT_NEAR(sigma, 6.6524e-29f, 1e-32f);
}

TEST(QEDFormulas, SchwingerTerm) {
    /* a_e = α/(2π) ≈ 0.0011614 (first order) */
    float ae1 = qed_anomalous_moment_first_order();
    EXPECT_NEAR(ae1, (1.0f/137.036f) / (2.0f * M_PI), 1e-7f);
}

TEST(QEDFormulas, PairProductionThreshold) {
    /* E_γ > 2m_e = 1.022 MeV = 0.001022 GeV */
    float thresh = qed_pair_production_threshold();
    EXPECT_NEAR(thresh, 0.001022f, 0.0001f);
}

/* ============================================================
 * Mathematical Constants
 * ============================================================ */

TEST(MathConstants, EulerIdentity) {
    /* e^(iπ) + 1 = 0 → |e^(iπ) + 1| < ε */
    double re = cos(M_PI);  /* e^(iπ) real part = -1 */
    double im = sin(M_PI);  /* e^(iπ) imag part ≈ 0 */
    EXPECT_NEAR(re + 1.0, 0.0, 1e-15);
    EXPECT_NEAR(im, 0.0, 1e-15);
}

TEST(MathConstants, GoldenRatio) {
    /* φ = (1+√5)/2 ≈ 1.6180339887 */
    double phi = (1.0 + sqrt(5.0)) / 2.0;
    EXPECT_NEAR(phi, 1.6180339887, 1e-10);
    /* φ² = φ + 1 */
    EXPECT_NEAR(phi * phi, phi + 1.0, 1e-10);
}
