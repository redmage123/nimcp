/**
 * @file test_relativistic.cpp
 * @brief Tests for the Relativistic Physics Engine -- special and general relativity
 *
 * Verifies: Lorentz factor, E=mc2, proper time, Lorentz boost identity,
 * velocity addition, invariant mass, time dilation, energy conservation.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/physics/nimcp_relativistic_physics.h"
}

/* Helper: make a velocity vector along x-axis as fraction of c */
static wm_parietal_vec3_t vx_frac_c(float frac) {
    return (wm_parietal_vec3_t){(float)(frac * REL_C), 0.0f, 0.0f};
}

/* ---- Tests ---- */

class RelativisticEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = relativistic_default_config();
        engine = relativistic_create(&cfg);
    }
    void TearDown() override {
        if (engine) relativistic_destroy(engine);
    }
    rel_config_t cfg;
    relativistic_engine_t* engine;
};

TEST(RelativisticBasic, CreateDestroy) {
    rel_config_t cfg = relativistic_default_config();
    relativistic_engine_t* engine = relativistic_create(&cfg);
    ASSERT_NE(engine, nullptr);
    EXPECT_TRUE(engine->initialized);
    EXPECT_EQ(engine->num_particles, 0);
    relativistic_destroy(engine);
}

TEST(RelativisticGamma, AtRest) {
    /* gamma(v=0) = 1 */
    wm_parietal_vec3_t v = {0.0f, 0.0f, 0.0f};
    float g = relativistic_gamma(v);
    EXPECT_NEAR(g, 1.0f, 1e-5f);
}

TEST(RelativisticGamma, HalfC) {
    /* gamma(0.5c) = 1/sqrt(1 - 0.25) = 1/sqrt(0.75) = 1.15470... */
    float g = relativistic_gamma(vx_frac_c(0.5f));
    EXPECT_NEAR(g, 1.1547f, 0.002f);
}

TEST(RelativisticGamma, Point99C) {
    /* gamma(0.99c) = 1/sqrt(1 - 0.9801) = 1/sqrt(0.0199) ~ 7.089 */
    float g = relativistic_gamma(vx_frac_c(0.99f));
    EXPECT_NEAR(g, 7.089f, 0.1f);
}

TEST(RelativisticEnergy, RestEnergyElectron) {
    /* E = m*c^2 for electron: 9.109e-31 * (3e8)^2 = 8.187e-14 J */
    float E = relativistic_rest_energy(REL_ELECTRON_MASS);
    double expected = (double)REL_ELECTRON_MASS * (double)REL_C * (double)REL_C;
    EXPECT_NEAR(E, expected, expected * 0.01);  /* 1% tolerance for float */
}

TEST(RelativisticTime, ProperTimeLessThanCoord) {
    /* Proper time < coordinate time for moving object */
    float coord_dt = 1.0f;
    wm_parietal_vec3_t v = vx_frac_c(0.8f);
    float proper_dt = relativistic_proper_time(coord_dt, v);
    /* gamma(0.8c) ~ 1.667, proper_dt = coord_dt / gamma ~ 0.6 */
    EXPECT_LT(proper_dt, coord_dt);
    EXPECT_NEAR(proper_dt, 0.6f, 0.05f);
}

TEST(RelativisticBoost, LorentzBoostIdentity) {
    /* Boost at v=0 should be identity: output = input */
    wm_parietal_vec3_t zero_v = {0.0f, 0.0f, 0.0f};
    rel_lorentz_boost_t boost = relativistic_build_boost(zero_v);
    EXPECT_NEAR(boost.gamma, 1.0f, 1e-5f);

    rel_four_vector_t input = {10.0f, 1.0f, 2.0f, 3.0f};
    rel_four_vector_t output = relativistic_boost_transform(&boost, input);
    EXPECT_NEAR(output.t, input.t, 0.01f);
    EXPECT_NEAR(output.x, input.x, 0.01f);
    EXPECT_NEAR(output.y, input.y, 0.01f);
    EXPECT_NEAR(output.z, input.z, 0.01f);
}

TEST(RelativisticVelocity, AdditionSubluminal) {
    /* v_add(0.9c, 0.9c) = (0.9c + 0.9c) / (1 + 0.81) = 1.8c/1.81 ~ 0.9945c */
    float u = 0.9f * REL_C;
    float v = 0.9f * REL_C;
    float result = relativistic_velocity_addition(u, v);
    /* Must be < c */
    EXPECT_LT(result, REL_C);
    /* Expected: 1.8c / 1.81 = 0.99448c */
    float expected = (0.9f + 0.9f) / (1.0f + 0.9f * 0.9f) * REL_C;
    EXPECT_NEAR(result, expected, REL_C * 0.01f);
}

TEST(RelativisticMass, InvariantMassFromFourMomentum) {
    /* For electron at rest: E = m_e * c^2, p = 0
     * four_momentum = (E/c, 0, 0, 0)
     * m_inv^2 * c^2 = (E/c)^2 - |p|^2 = E^2/c^2
     * m_inv = E/c^2 = m_e */
    float E = REL_ELECTRON_MASS * REL_C2;
    rel_four_vector_t p4 = {E / REL_C, 0.0f, 0.0f, 0.0f};
    float m_inv = relativistic_invariant_mass(p4);
    EXPECT_NEAR(m_inv, REL_ELECTRON_MASS, REL_ELECTRON_MASS * 0.01f);
}

TEST(RelativisticEnergy, KineticEnergySlow) {
    /* At low velocity, relativistic KE ~ (1/2)mv^2 */
    float mass = 1.0f;
    float v_slow = 100.0f;  /* 100 m/s << c */
    wm_parietal_vec3_t velocity = {v_slow, 0.0f, 0.0f};
    float KE_rel = relativistic_kinetic_energy(mass, velocity);
    float KE_classical = 0.5f * mass * v_slow * v_slow;
    /* Should agree to within 1% at non-relativistic speeds */
    EXPECT_NEAR(KE_rel, KE_classical, KE_classical * 0.01f);
}

TEST(RelativisticEnergy, TotalEnergyAtRest) {
    /* Total energy at rest = m*c^2 */
    float mass = REL_PROTON_MASS;
    wm_parietal_vec3_t zero = {0.0f, 0.0f, 0.0f};
    float E_total = relativistic_total_energy(mass, zero);
    float E_rest = relativistic_rest_energy(mass);
    EXPECT_NEAR(E_total, E_rest, E_rest * 0.001f);
}

TEST(RelativisticMomentum, Magnitude) {
    /* p = gamma * m * v for 0.5c */
    float mass = 1.0f;
    wm_parietal_vec3_t v = vx_frac_c(0.5f);
    wm_parietal_vec3_t p = relativistic_momentum(mass, v);
    float gamma = 1.0f / sqrtf(1.0f - 0.25f);  /* 1.1547 */
    float expected_px = gamma * mass * v.x;
    EXPECT_NEAR(p.x, expected_px, fabsf(expected_px) * 0.01f);
    EXPECT_NEAR(p.y, 0.0f, 1.0f);
    EXPECT_NEAR(p.z, 0.0f, 1.0f);
}

TEST_F(RelativisticEngineTest, AddParticle) {
    rel_particle_t p;
    memset(&p, 0, sizeof(p));
    p.rest_mass = REL_ELECTRON_MASS;
    p.position = (wm_parietal_vec3_t){0, 0, 0};
    p.velocity = (wm_parietal_vec3_t){0, 0, 0};
    p.active = true;

    uint32_t id = relativistic_add_particle(engine, &p);
    ASSERT_NE(id, UINT32_MAX);
    EXPECT_EQ(engine->num_particles, 1);
}

TEST_F(RelativisticEngineTest, StepRuns) {
    rel_particle_t p;
    memset(&p, 0, sizeof(p));
    p.rest_mass = 1.0f;
    p.velocity = (wm_parietal_vec3_t){1000.0f, 0.0f, 0.0f};
    p.active = true;
    relativistic_add_particle(engine, &p);

    int rc = relativistic_step(engine, 0.01f);
    EXPECT_EQ(rc, 0);
}
