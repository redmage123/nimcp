/**
 * @file test_analytical_chemistry.cpp
 * @brief Tests for analytical chemistry: pH, buffers, titration, Beer-Lambert (gtest)
 *
 * Validates strong acid pH, Henderson-Hasselbalch, titration equivalence,
 * Van Deemter, calibration, and Nernst equation.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>

extern "C" {
#include "cognitive/physics/nimcp_analytical_chemistry.h"
}

class AnalyticalChemistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = analytical_chemistry_default_config();
        sim = analytical_chemistry_create(&cfg);
    }
    void TearDown() override {
        if (sim) analytical_chemistry_destroy(sim);
    }
    analytical_chemistry_config_t cfg{};
    analytical_chemistry_sim_t* sim = nullptr;
};

TEST_F(AnalyticalChemistryTest, CreateDestroy) {
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
}

/* ---- strong acid pH: pH = -log10([H+]) ---------------------------------- */

TEST(AnaChemPhTest, StrongAcid01M) {
    /* 0.1 M HCl -> pH = 1.0 */
    float ph = analytical_chemistry_strong_acid_pH(0.1f);
    EXPECT_NEAR(ph, 1.0f, 0.05f);
}

TEST(AnaChemPhTest, StrongAcid001M) {
    /* 0.01 M HCl -> pH = 2.0 */
    float ph = analytical_chemistry_strong_acid_pH(0.01f);
    EXPECT_NEAR(ph, 2.0f, 0.05f);
}

TEST(AnaChemPhTest, StrongBase) {
    /* 0.1 M NaOH -> pH = 13.0 */
    float ph = analytical_chemistry_strong_base_pH(0.1f);
    EXPECT_NEAR(ph, 13.0f, 0.1f);
}

/* ---- Henderson-Hasselbalch: pH = pKa + log([A-]/[HA]) ------------------- */

TEST(AnaChemHHTest, Equal) {
    /* When [A-] = [HA], pH = pKa */
    float ph = analytical_chemistry_henderson_hasselbalch(4.76f, 0.1f, 0.1f);
    EXPECT_NEAR(ph, 4.76f, 0.01f);
}

TEST(AnaChemHHTest, ExcessBase) {
    /* [A-] = 10*[HA] -> pH = pKa + 1 */
    float ph = analytical_chemistry_henderson_hasselbalch(4.76f, 1.0f, 0.1f);
    EXPECT_NEAR(ph, 5.76f, 0.05f);
}

TEST(AnaChemHHTest, ExcessAcid) {
    /* [A-] = 0.1*[HA] -> pH = pKa - 1 */
    float ph = analytical_chemistry_henderson_hasselbalch(4.76f, 0.1f, 1.0f);
    EXPECT_NEAR(ph, 3.76f, 0.05f);
}

/* ---- titration equivalence point ----------------------------------------- */

TEST_F(AnalyticalChemistryTest, TitrationEquivalencePoint) {
    /* Analyte: 0.1M HCl, 50 mL */
    anachem_solution_t analyte = {};
    snprintf(analyte.name, ANACHEM_MAX_NAME, "HCl");
    analyte.type = ANACHEM_ACID_STRONG;
    analyte.concentration = 0.1f;
    analyte.volume = 0.050f;
    analyte.active = true;
    uint32_t aid = analytical_chemistry_add_solution(sim, &analyte);

    /* Titrant: 0.1M NaOH */
    anachem_solution_t titrant = {};
    snprintf(titrant.name, ANACHEM_MAX_NAME, "NaOH");
    titrant.type = ANACHEM_BASE_STRONG;
    titrant.concentration = 0.1f;
    titrant.volume = 0.100f;
    titrant.active = true;
    uint32_t tid = analytical_chemistry_add_solution(sim, &titrant);

    anachem_titration_t tit = {};
    tit.type = ANACHEM_TITRATION_SA_SB;
    tit.analyte_idx = aid;
    tit.titrant_idx = tid;
    tit.active = true;
    analytical_chemistry_add_titration(sim, &tit);

    float ph_eq = analytical_chemistry_titration_pH(&sim->titrations[0],
        &sim->solutions[aid], &sim->solutions[tid], 50.0f);
    EXPECT_NEAR(ph_eq, 7.0f, 0.5f);
}

/* ---- Beer-Lambert -------------------------------------------------------- */

TEST(AnaChemBeerLambertTest, Absorbance) {
    anachem_beer_lambert_t bl = analytical_chemistry_beer_lambert(50.0f, 2.0f, 0.01f);
    EXPECT_NEAR(bl.absorbance, 1.0f, 0.001f);
    EXPECT_NEAR(bl.transmittance, 0.1f, 0.01f);
}

/* ---- Van Deemter: H = A + B/u + C*u ------------------------------------ */

TEST(AnaChemVanDeemterTest, Value) {
    float H = analytical_chemistry_van_deemter(0.1f, 1.0f, 0.01f, 10.0f);
    EXPECT_NEAR(H, 0.3f, 0.01f);
}

TEST(AnaChemVanDeemterTest, OptimalFlow) {
    float u_opt = analytical_chemistry_optimal_flow(1.0f, 0.01f);
    EXPECT_NEAR(u_opt, 10.0f, 0.5f);
}

/* ---- Nernst equation ----------------------------------------------------- */

TEST(AnaChemNernstTest, StandardConditions) {
    float E = analytical_chemistry_nernst(0.34f, 2, 1.0f, 298.15f);
    EXPECT_NEAR(E, 0.34f, 0.01f);
}

/* ---- buffer capacity ----------------------------------------------------- */

TEST(AnaChemBufferTest, CapacityMaximum) {
    float Ka = 1.738e-5f;
    float H_at_pka = Ka;
    float beta = analytical_chemistry_buffer_capacity(0.1f, Ka, H_at_pka);
    EXPECT_GT(beta, 0.0f);

    float H_off = Ka * 10.0f;
    float beta_off = analytical_chemistry_buffer_capacity(0.1f, Ka, H_off);
    EXPECT_GT(beta, beta_off);
}

/* ---- load common buffers ------------------------------------------------- */

TEST_F(AnalyticalChemistryTest, LoadCommonBuffers) {
    int rc = analytical_chemistry_load_common_buffers(sim);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(sim->num_solutions, 6u);
}
