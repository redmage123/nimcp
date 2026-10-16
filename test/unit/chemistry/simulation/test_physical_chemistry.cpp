/**
 * @file test_physical_chemistry.cpp
 * @brief Tests for physical chemistry: thermodynamics, stat mech, spectroscopy (gtest)
 *
 * Validates Gibbs free energy, equilibrium constant, ideal gas law,
 * Beer-Lambert law, Wien displacement, Boltzmann distribution, and more.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>

extern "C" {
#include "cognitive/physics/nimcp_physical_chemistry.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class PhysicalChemistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = physical_chemistry_default_config();
        sim = physical_chemistry_create(&cfg);
    }
    void TearDown() override {
        if (sim) physical_chemistry_destroy(sim);
    }
    pchem_config_t cfg{};
    physical_chemistry_sim_t* sim = nullptr;
};

TEST_F(PhysicalChemistryTest, CreateDestroy) {
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
}

/* ---- Gibbs: dG = dH - T*dS ---------------------------------------------- */

TEST(PChemGibbsTest, FreeEnergy) {
    /* dH = -100 kJ/mol, T = 298 K, dS = 0.05 kJ/(mol*K) */
    /* dG = -100 - 298*0.05 = -114.9 kJ/mol */
    float dG = pchem_gibbs_free_energy(-100.0f, 298.0f, 0.05f);
    EXPECT_NEAR(dG, -114.9f, 0.5f);
}

TEST(PChemGibbsTest, Spontaneity) {
    float dG = pchem_gibbs_free_energy(-50.0f, 298.0f, 0.1f);
    EXPECT_LT(dG, 0.0f);
}

/* ---- Equilibrium K = exp(-dG/(RT)) -------------------------------------- */

TEST(PChemEquilibriumTest, KnownValue) {
    /* dG = -5.7058 kJ/mol at 298K -> K ~ 10 */
    float K = pchem_equilibrium_constant(-5.7058f, 298.0f);
    EXPECT_NEAR(K, 10.0f, 1.0f);
}

TEST(PChemEquilibriumTest, PositiveDG) {
    float K = pchem_equilibrium_constant(10.0f, 298.0f);
    EXPECT_LT(K, 1.0f);
    EXPECT_GT(K, 0.0f);
}

/* ---- Ideal gas: PV = nRT ------------------------------------------------ */

TEST(PChemIdealGasTest, Law) {
    /* n=1, T=273.15K, V=0.022414 m^3 -> P = 101325 Pa */
    float P = pchem_ideal_gas_pressure(1.0f, 273.15f, 0.022414f);
    EXPECT_NEAR(P, 101325.0f, 500.0f);
}

TEST(PChemIdealGasTest, Proportionality) {
    float P1 = pchem_ideal_gas_pressure(1.0f, 300.0f, 1.0f);
    float P2 = pchem_ideal_gas_pressure(1.0f, 600.0f, 1.0f);
    EXPECT_NEAR(P2, 2.0f * P1, P1 * 0.01f);
}

/* ---- Beer-Lambert: A = epsilon * l * c ---------------------------------- */

TEST(PChemBeerLambertTest, Known) {
    float A = pchem_beer_lambert(100.0f, 1.0f, 0.01f);
    EXPECT_NEAR(A, 1.0f, 0.001f);
}

TEST(PChemBeerLambertTest, Proportional) {
    float A1 = pchem_beer_lambert(50.0f, 1.0f, 0.01f);
    float A2 = pchem_beer_lambert(50.0f, 1.0f, 0.02f);
    EXPECT_NEAR(A2, 2.0f * A1, 0.001f);
}

/* ---- Wien displacement: lambda_max = b/T -------------------------------- */

TEST(PChemWienTest, SunTemperature) {
    float lambda = pchem_wien_displacement(5778.0f);
    EXPECT_NEAR(lambda, 5.015e-7f, 5.0e-9f);
}

TEST(PChemWienTest, RoomTemperature) {
    float lambda = pchem_wien_displacement(300.0f);
    EXPECT_NEAR(lambda, 9.66e-6f, 1.0e-7f);
}

/* ---- Boltzmann probability ----------------------------------------------- */

TEST(PChemBoltzmannTest, GroundState) {
    float Z = 5.0f;
    float P = pchem_boltzmann_probability(0.0f, 300.0f, Z);
    EXPECT_NEAR(P, 1.0f / Z, 0.01f);
}

TEST(PChemBoltzmannTest, Decreasing) {
    float P_low = pchem_boltzmann_probability(1.0e-21f, 300.0f, 1.0f);
    float P_high = pchem_boltzmann_probability(1.0e-20f, 300.0f, 1.0f);
    EXPECT_GT(P_low, P_high);
}

/* ---- add species and step ------------------------------------------------ */

TEST_F(PhysicalChemistryTest, AddSpeciesAndStep) {
    pchem_species_t sp = {};
    snprintf(sp.name, PCHEM_MAX_NAME, "water");
    sp.enthalpy = -285.8f;
    sp.entropy = 69.91f;
    sp.molar_mass = 18.015f;
    sp.active = true;
    physical_chemistry_add_species(sim, &sp);
    EXPECT_EQ(sim->num_species, 1u);

    int rc = physical_chemistry_step(sim, 0.01f);
    EXPECT_EQ(rc, 0);
}
