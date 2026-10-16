/**
 * @file test_surface_physics.cpp
 * @brief Tests for the Surface Physics Engine -- interfaces, wetting, optics, capillarity
 *
 * Verifies: Young-Laplace capillary pressure, Fresnel normal reflectance,
 * critical angle, common materials, contact angle, droplets, heat transfer.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/physics/nimcp_surface_physics.h"
}

/* ---- Fixture ---- */

class SurfacePhysicsTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = surface_physics_default_config();
        sim = surface_physics_create(&cfg);
    }
    void TearDown() override {
        if (sim) surface_physics_destroy(sim);
    }
    surf_phys_config_t cfg;
    surface_physics_sim_t* sim;
};

/* ---- Tests ---- */

TEST(SurfacePhysicsBasic, CreateDestroy) {
    surf_phys_config_t cfg = surface_physics_default_config();
    surface_physics_sim_t* sim = surface_physics_create(&cfg);
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
    EXPECT_EQ(sim->num_materials, 0);
    surface_physics_destroy(sim);
}

TEST(SurfacePhysicsCapillary, PressureSphere) {
    /* Young-Laplace: Delta_P = gamma * (1/R1 + 1/R2)
     * For a sphere (R1=R2=R): Delta_P = 2*gamma/R
     * Water: gamma = 0.0728 N/m, R = 1 mm = 0.001 m
     * Delta_P = 2 * 0.0728 / 0.001 = 145.6 Pa */
    float dP = surface_physics_capillary_pressure(SURF_WATER_TENSION, 0.001f, 0.001f);
    EXPECT_NEAR(dP, 145.6f, 5.0f);
}

TEST(SurfacePhysicsCapillary, PressureCylinder) {
    /* For a cylinder (R2 -> inf): Delta_P = gamma/R
     * gamma = 0.0728, R = 1mm: Delta_P = 72.8 Pa */
    float dP = surface_physics_capillary_pressure(SURF_WATER_TENSION, 0.001f, 1e10f);
    /* 1/R2 ~ 0, so Delta_P ~ gamma/R1 */
    EXPECT_NEAR(dP, 72.8f, 3.0f);
}

TEST(SurfacePhysicsFresnel, NormalAirGlass) {
    /* R = ((n1-n2)/(n1+n2))^2
     * Air (n=1.0) to glass (n=1.5):
     * R = ((1.0-1.5)/(1.0+1.5))^2 = (0.5/2.5)^2 = 0.04 = 4% */
    float R = surface_physics_fresnel_normal(1.0f, 1.5f);
    EXPECT_NEAR(R, 0.04f, 0.005f);
}

TEST(SurfacePhysicsFresnel, NormalSymmetric) {
    /* R is symmetric in n1, n2: ((n1-n2)/(n1+n2))^2 = ((n2-n1)/(n1+n2))^2 */
    float R1 = surface_physics_fresnel_normal(1.0f, 2.0f);
    float R2 = surface_physics_fresnel_normal(2.0f, 1.0f);
    EXPECT_NEAR(R1, R2, 1e-6f);
}

TEST(SurfacePhysicsFresnel, NormalSameMedium) {
    /* Same medium: R = 0 */
    float R = surface_physics_fresnel_normal(1.5f, 1.5f);
    EXPECT_NEAR(R, 0.0f, 1e-8f);
}

TEST(SurfacePhysicsOptics, CriticalAngleGlassAir) {
    /* sin(theta_c) = n2/n1 = 1.0/1.5 = 0.667
     * theta_c = arcsin(0.667) = 41.81 degrees = 0.7297 rad */
    float theta_c = surface_physics_critical_angle(1.5f, 1.0f);
    float expected_rad = asinf(1.0f / 1.5f);
    EXPECT_NEAR(theta_c, expected_rad, 0.02f);
    /* In degrees: ~41.8 */
    float degrees = theta_c * 180.0f / (float)M_PI;
    EXPECT_NEAR(degrees, 41.81f, 0.5f);
}

TEST_F(SurfacePhysicsTest, LoadCommonMaterials) {
    surface_physics_load_common_materials(sim);
    /* Should have loaded several materials (water, glass, steel, teflon, etc.) */
    ASSERT_GE(sim->num_materials, 4u);

    /* Find water */
    bool found_water = false;
    for (uint32_t i = 0; i < sim->num_materials; i++) {
        if (strstr(sim->materials[i].name, "water") ||
            strstr(sim->materials[i].name, "Water")) {
            found_water = true;
            EXPECT_EQ(sim->materials[i].phase, SURF_TYPE_LIQUID);
            EXPECT_NEAR(sim->materials[i].surface_tension, SURF_WATER_TENSION, 0.005f);
            break;
        }
    }
    EXPECT_TRUE(found_water);
}

TEST_F(SurfacePhysicsTest, TeflonHydrophobic) {
    surface_physics_load_common_materials(sim);

    /* Find teflon */
    int32_t teflon_idx = -1;
    for (uint32_t i = 0; i < sim->num_materials; i++) {
        if (strstr(sim->materials[i].name, "teflon") ||
            strstr(sim->materials[i].name, "Teflon") ||
            strstr(sim->materials[i].name, "PTFE")) {
            teflon_idx = (int32_t)i;
            break;
        }
    }

    if (teflon_idx >= 0) {
        EXPECT_TRUE(sim->materials[teflon_idx].hydrophobic);
    }
}

TEST(SurfacePhysicsWetting, ContactAngleComputation) {
    /* Young's equation: cos(theta) = (gamma_SG - gamma_SL) / gamma_LG
     * Example: gamma_SG = 0.050, gamma_SL = 0.025, gamma_LG = 0.0728
     * cos(theta) = 0.025/0.0728 = 0.3434 => theta = 69.9 deg */
    float theta = surface_physics_contact_angle(0.050f, 0.025f, 0.0728f);
    float expected = acosf(0.025f / 0.0728f);
    EXPECT_NEAR(theta, expected, 0.02f);
}

TEST(SurfacePhysicsHeat, RadiativeFlux) {
    /* q = epsilon * sigma * (T1^4 - T2^4)
     * epsilon = 1.0, T1 = 500K, T2 = 300K
     * q = 5.67e-8 * (500^4 - 300^4) = 5.67e-8 * (6.25e10 - 8.1e9)
     * = 5.67e-8 * 5.44e10 = 3084 W/m^2 */
    float q = surface_physics_radiative_flux(1.0f, 500.0f, 300.0f);
    double expected = SURF_STEFAN_BOLTZMANN * (pow(500.0, 4) - pow(300.0, 4));
    EXPECT_NEAR(q, expected, fabs(expected) * 0.02);
}

TEST_F(SurfacePhysicsTest, AddMaterial) {
    surf_material_t mat;
    memset(&mat, 0, sizeof(mat));
    strncpy(mat.name, "test_material", sizeof(mat.name) - 1);
    mat.phase = SURF_TYPE_SOLID;
    mat.surface_energy = 0.5f;
    mat.refractive_index = 1.5f;
    mat.active = true;

    uint32_t id = surface_physics_add_material(sim, &mat);
    ASSERT_NE(id, UINT32_MAX);
    EXPECT_EQ(sim->num_materials, 1);
}

TEST_F(SurfacePhysicsTest, StepNoCrash) {
    int rc = surface_physics_step(sim, 0.01f);
    EXPECT_EQ(rc, 0);
}

TEST(SurfacePhysicsCapillary, CapillaryRise) {
    /* h = 2*gamma*cos(theta) / (rho*g*r)
     * Water in glass tube: gamma=0.0728, theta~0, rho=1000, g=9.81, r=0.5mm
     * h = 2*0.0728 / (1000*9.81*0.0005) = 0.1456 / 4.905 = 0.0297 m ~ 3 cm */
    float h = surface_physics_capillary_rise(SURF_WATER_TENSION, 0.0f, 1000.0f, 9.81f, 0.0005f);
    EXPECT_NEAR(h, 0.0297f, 0.005f);
}
