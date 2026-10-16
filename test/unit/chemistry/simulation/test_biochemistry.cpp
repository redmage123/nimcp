/**
 * @file test_biochemistry.cpp
 * @brief Tests for the biochemistry simulation engine (gtest)
 *
 * Validates Michaelis-Menten kinetics, Hill equation cooperativity,
 * competitive inhibition, glycolysis loading, enzyme/metabolite management.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>

extern "C" {
#include "cognitive/physics/nimcp_biochemistry.h"
}

class BiochemistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = biochemistry_default_config();
        sim = biochemistry_create(&cfg);
    }
    void TearDown() override {
        if (sim) biochemistry_destroy(sim);
    }
    biochem_config_t cfg{};
    biochemistry_sim_t* sim = nullptr;
};

TEST_F(BiochemistryTest, CreateDestroy) {
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
}

/* ---- Michaelis-Menten: v = Vmax*[S]/(Km+[S]) ---------------------------- */

TEST(BiochemMichaelisTest, HalfVmax) {
    /* When [S] = Km, v = Vmax/2 */
    float v = biochemistry_michaelis_menten(1.0f, 1.0f, 1.0f);
    EXPECT_NEAR(v, 0.5f, 0.001f);
}

TEST(BiochemMichaelisTest, Saturation) {
    /* Very high [S] -> v ~ Vmax */
    float v = biochemistry_michaelis_menten(1.0f, 1.0f, 10000.0f);
    EXPECT_NEAR(v, 1.0f, 0.01f);
}

TEST(BiochemMichaelisTest, LowSubstrate) {
    /* Very low [S] -> v ~ Vmax*[S]/Km (linear) */
    float v = biochemistry_michaelis_menten(1.0f, 1.0f, 0.01f);
    EXPECT_NEAR(v, 0.01f / 1.01f, 0.001f);
}

/* ---- Hill equation: v = Vmax*[S]^n/(K^n+[S]^n) -------------------------- */

TEST(BiochemHillTest, CooperativityN2) {
    /* At [S]=K: v = Vmax/2 */
    float v_at_k = biochemistry_hill_equation(1.0f, 1.0f, 1.0f, 2.0f);
    EXPECT_NEAR(v_at_k, 0.5f, 0.001f);
}

TEST(BiochemHillTest, SteeperThanMM) {
    float mm_low = biochemistry_michaelis_menten(1.0f, 1.0f, 0.5f);
    float hill_low = biochemistry_hill_equation(1.0f, 1.0f, 0.5f, 2.0f);
    EXPECT_LT(hill_low, mm_low);

    float mm_high = biochemistry_michaelis_menten(1.0f, 1.0f, 2.0f);
    float hill_high = biochemistry_hill_equation(1.0f, 1.0f, 2.0f, 2.0f);
    EXPECT_GT(hill_high, mm_high);
}

/* ---- competitive inhibition reduces rate --------------------------------- */

TEST(BiochemInhibitionTest, ReducesRate) {
    float Vmax = 1.0f, Km = 1.0f, S = 1.0f;
    float v_no_inhib = biochemistry_michaelis_menten(Vmax, Km, S);
    float v_with_inhib = biochemistry_competitive_inhibition(Vmax, Km, S, 1.0f, 1.0f);
    EXPECT_LT(v_with_inhib, v_no_inhib);
}

TEST(BiochemInhibitionTest, KnownValue) {
    /* v = Vmax*S / (Km*(1+I/Ki) + S) = 1/(1*(1+1)+1) = 1/3 */
    float v = biochemistry_competitive_inhibition(1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    EXPECT_NEAR(v, 1.0f / 3.0f, 0.01f);
}

/* ---- glycolysis loads with enzymes --------------------------------------- */

TEST_F(BiochemistryTest, GlycolysisLoadsEnzymes) {
    biochemistry_load_glycolysis(sim);
    EXPECT_GE(sim->num_enzymes, 10u);
    EXPECT_GE(sim->num_pathways, 1u);
}

TEST_F(BiochemistryTest, StepBasic) {
    biochemistry_load_glycolysis(sim);
    int rc = biochemistry_step(sim, 0.01f);
    EXPECT_EQ(rc, 0);
}

/* ---- pH activity --------------------------------------------------------- */

TEST(BiochemPhActivityTest, Optimum) {
    float act_opt = biochemistry_ph_activity(7.0f, 5.0f, 9.0f);
    float act_extreme = biochemistry_ph_activity(2.0f, 5.0f, 9.0f);
    EXPECT_GT(act_opt, act_extreme);
}

/* ---- add metabolite / enzyme --------------------------------------------- */

TEST_F(BiochemistryTest, AddMetaboliteAndEnzyme) {
    biochem_metabolite_t met = {};
    snprintf(met.name, BIOCHEM_MAX_NAME, "glucose");
    met.concentration = 5.0f;
    met.molecular_weight = 180.16f;
    met.active = true;
    uint32_t mid = biochemistry_add_metabolite(sim, &met);

    biochem_enzyme_t enz = {};
    snprintf(enz.name, BIOCHEM_MAX_NAME, "hexokinase");
    enz.Vmax = 100.0f;
    enz.Km = 0.1f;
    enz.substrate_id = mid;
    enz.hill_coefficient = 1.0f;
    enz.active = true;
    biochemistry_add_enzyme(sim, &enz);

    EXPECT_EQ(sim->num_metabolites, 1u);
    EXPECT_EQ(sim->num_enzymes, 1u);
}
