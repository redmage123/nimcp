/**
 * @file test_surface_chemistry.cpp
 * @brief Tests for surface chemistry: isotherms, Arrhenius, Nernst, Butler-Volmer (gtest)
 *
 * Validates Langmuir isotherm, Arrhenius rate, Nernst electrode potential,
 * Butler-Volmer kinetics, Pt catalyst loading, and corrosion computations.
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "cognitive/physics/nimcp_surface_chemistry.h"
}

class SurfaceChemistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = surface_chemistry_default_config();
        sim = surface_chemistry_create(&cfg);
    }
    void TearDown() override {
        if (sim) surface_chemistry_destroy(sim);
    }
    schem_config_t cfg{};
    surface_chemistry_sim_t* sim = nullptr;
};

TEST_F(SurfaceChemistryTest, CreateDestroy) {
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
}

/* ---- Langmuir isotherm: theta = KP/(1+KP) ------------------------------- */

TEST(SurfaceChemLangmuirTest, KnownValue) {
    /* K=0.01, P=100 -> theta = 0.01*100 / (1 + 0.01*100) = 1/2 = 0.5 */
    float theta = surface_chemistry_langmuir(0.01f, 100.0f);
    EXPECT_NEAR(theta, 0.5f, 0.001f);
}

TEST(SurfaceChemLangmuirTest, LowPressure) {
    /* K=0.01, P=1 -> theta = 0.01/(1.01) ~ 0.0099 (linear regime) */
    float theta = surface_chemistry_langmuir(0.01f, 1.0f);
    EXPECT_NEAR(theta, 0.0099f, 0.001f);
}

TEST(SurfaceChemLangmuirTest, HighPressure) {
    /* K=0.01, P=100000 -> theta ~ 1.0 (saturation) */
    float theta = surface_chemistry_langmuir(0.01f, 100000.0f);
    EXPECT_GT(theta, 0.99f);
    EXPECT_LE(theta, 1.0f);
}

/* ---- Arrhenius: k = A * exp(-Ea/RT) ------------------------------------- */

TEST(SurfaceChemArrheniusTest, At500K) {
    float A = 1.0e10f;
    float Ea = 50.0f;  /* kJ/mol */
    float T = 500.0f;
    float k = surface_chemistry_arrhenius(A, Ea, T);
    double expected = 1.0e10 * exp(-50000.0 / (8.314 * 500.0));
    EXPECT_NEAR(k, (float)expected, (float)(expected * 0.05));
}

TEST(SurfaceChemArrheniusTest, TemperatureDependence) {
    float A = 1.0e10f;
    float Ea = 50.0f;
    float k_low = surface_chemistry_arrhenius(A, Ea, 300.0f);
    float k_high = surface_chemistry_arrhenius(A, Ea, 600.0f);
    EXPECT_GT(k_high, k_low);
}

/* ---- Nernst: E = E0 - (RT/nF) * ln(Q) ---------------------------------- */

TEST(SurfaceChemNernstTest, CuStandard) {
    /* Cu2+/Cu: E0=0.34V, T=298.15K, n=2, Q=1 -> E = E0 */
    float E = surface_chemistry_nernst(0.34f, 298.15f, 2, 1.0f);
    EXPECT_NEAR(E, 0.34f, 0.01f);
}

TEST(SurfaceChemNernstTest, NonunitQ) {
    /* E0=0.34V, T=298.15, n=2, Q=0.01 -> E ~ 0.399 */
    float E = surface_chemistry_nernst(0.34f, 298.15f, 2, 0.01f);
    EXPECT_NEAR(E, 0.399f, 0.02f);
}

/* ---- Butler-Volmer: sign test ------------------------------------------- */

TEST(SurfaceChemButlerVolmerTest, PositiveEta) {
    float j = surface_chemistry_butler_volmer(1.0f, 0.5f, 1, 0.1f, 298.15f);
    EXPECT_GT(j, 0.0f);
}

TEST(SurfaceChemButlerVolmerTest, NegativeEta) {
    float j = surface_chemistry_butler_volmer(1.0f, 0.5f, 1, -0.1f, 298.15f);
    EXPECT_LT(j, 0.0f);
}

TEST(SurfaceChemButlerVolmerTest, ZeroEta) {
    float j = surface_chemistry_butler_volmer(1.0f, 0.5f, 1, 0.0f, 298.15f);
    EXPECT_NEAR(j, 0.0f, 0.001f);
}

/* ---- Pt catalyst loads and steps ----------------------------------------- */

TEST(SurfaceChemCatalystTest, PtCatalystLoadAndStep) {
    schem_config_t cfg = surface_chemistry_default_config();
    cfg.enable_catalysis = true;
    surface_chemistry_sim_t* s = surface_chemistry_create(&cfg);
    ASSERT_NE(s, nullptr);

    surface_chemistry_load_pt_catalyst(s);
    EXPECT_GE(s->num_surfaces, 1u);
    EXPECT_GE(s->num_reactions, 1u);

    int rc = surface_chemistry_step(s, 0.01f);
    EXPECT_EQ(rc, 0);

    surface_chemistry_destroy(s);
}

/* ---- Tafel slope --------------------------------------------------------- */

TEST(SurfaceChemTafelTest, SlopeValue) {
    /* b = 2.303 * RT / (alpha * n * F) */
    /* alpha=0.5, n=1, T=298.15: b = 0.1183 V */
    float b = surface_chemistry_tafel_slope(0.5f, 1, 298.15f);
    EXPECT_NEAR(b, 0.1183f, 0.005f);
}
