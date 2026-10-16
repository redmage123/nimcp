/**
 * @file test_ecology.cpp
 * @brief Tests for the ecology simulation engine (gtest)
 *
 * Validates Shannon index, species-area power law, nutrient cycling
 * conservation, diversity computations, food web management.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>

extern "C" {
#include "cognitive/physics/nimcp_ecology.h"
}

class EcologyTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = ecology_default_config();
        sim = ecology_create(&cfg);
    }
    void TearDown() override {
        if (sim) ecology_destroy(sim);
    }
    ecology_config_t cfg{};
    ecology_sim_t* sim = nullptr;
};

TEST_F(EcologyTest, CreateDestroy) {
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
}

/* ---- Shannon index of uniform distribution = ln(N) ----------------------- */

TEST(EcologyShannonTest, UniformDistribution) {
    float abundances[] = {100.0f, 100.0f, 100.0f, 100.0f};
    float H = ecology_shannon_index(abundances, 4);
    EXPECT_NEAR(H, logf(4.0f), 0.05f);
}

TEST(EcologyShannonTest, SingleSpecies) {
    float abundances[] = {100.0f};
    float H = ecology_shannon_index(abundances, 1);
    EXPECT_NEAR(H, 0.0f, 0.01f);
}

TEST(EcologyShannonTest, UnequalDistribution) {
    float equal[] = {100.0f, 100.0f, 100.0f};
    float unequal[] = {280.0f, 10.0f, 10.0f};
    float H_eq = ecology_shannon_index(equal, 3);
    float H_uneq = ecology_shannon_index(unequal, 3);
    EXPECT_GT(H_eq, H_uneq);
}

/* ---- Simpson index ------------------------------------------------------- */

TEST(EcologySimpsonTest, Uniform) {
    float abundances[] = {100.0f, 100.0f, 100.0f, 100.0f};
    float D = ecology_simpson_index(abundances, 4);
    EXPECT_NEAR(D, 0.75f, 0.05f);
}

/* ---- species-area power law: S = c * A^z -------------------------------- */

TEST(EcologySpeciesAreaTest, PowerLaw) {
    float S = ecology_species_area(10.0f, 100.0f, 0.25f);
    float expected = 10.0f * powf(100.0f, 0.25f);
    EXPECT_NEAR(S, expected, 0.5f);
}

TEST(EcologySpeciesAreaTest, LargerArea) {
    float S1 = ecology_species_area(10.0f, 10.0f, 0.25f);
    float S2 = ecology_species_area(10.0f, 1000.0f, 0.25f);
    EXPECT_GT(S2, S1);
}

/* ---- nutrient cycling conserves total N ---------------------------------- */

TEST(EcologyNutrientTest, CyclingConservesNitrogen) {
    ecology_config_t cfg = ecology_default_config();
    cfg.enable_nutrient_cycling = true;
    cfg.dt = 1.0f;
    ecology_sim_t* sim = ecology_create(&cfg);
    ASSERT_NE(sim, nullptr);

    sim->nutrients[ECOL_NUTRIENT_N_ATMOSPHERIC].amount = 1000.0f;
    sim->nutrients[ECOL_NUTRIENT_N_AMMONIUM].amount = 50.0f;
    sim->nutrients[ECOL_NUTRIENT_N_NITRATE].amount = 30.0f;
    sim->nutrients[ECOL_NUTRIENT_N_ORGANIC].amount = 20.0f;

    float total_N_before = 0.0f;
    for (int i = ECOL_NUTRIENT_N_ATMOSPHERIC; i <= ECOL_NUTRIENT_N_ORGANIC; i++) {
        total_N_before += sim->nutrients[i].amount;
    }

    for (int i = 0; i < 100; i++) {
        ecology_step_nitrogen_cycle(sim, 1.0f);
    }

    float total_N_after = 0.0f;
    for (int i = ECOL_NUTRIENT_N_ATMOSPHERIC; i <= ECOL_NUTRIENT_N_ORGANIC; i++) {
        total_N_after += sim->nutrients[i].amount;
    }

    EXPECT_NEAR(total_N_after, total_N_before, total_N_before * 0.01f);

    ecology_destroy(sim);
}

/* ---- add species and link ------------------------------------------------ */

TEST_F(EcologyTest, AddSpeciesAndLink) {
    ecol_species_t sp1 = {};
    snprintf(sp1.name, ECOL_MAX_NAME_LEN, "grass");
    sp1.trophic_level = ECOL_TROPHIC_PRODUCER;
    sp1.abundance = 1000.0f;
    sp1.carrying_capacity = 5000.0f;
    sp1.growth_rate = 0.5f;
    sp1.active = true;
    ecology_add_species(sim, &sp1);

    ecol_species_t sp2 = {};
    snprintf(sp2.name, ECOL_MAX_NAME_LEN, "deer");
    sp2.trophic_level = ECOL_TROPHIC_PRIMARY_CONSUMER;
    sp2.abundance = 50.0f;
    sp2.carrying_capacity = 200.0f;
    sp2.growth_rate = 0.1f;
    sp2.active = true;
    ecology_add_species(sim, &sp2);

    ecol_food_web_link_t link = {};
    link.species_a = 1;
    link.species_b = 0;
    link.type = ECOL_INTERACTION_PREDATION;
    link.strength = 0.01f;
    ecology_add_link(sim, &link);

    EXPECT_EQ(sim->num_species, 2u);
    EXPECT_EQ(sim->num_links, 1u);
}

/* ---- May's stability criterion ------------------------------------------- */

TEST(EcologyMayTest, Stability) {
    /* sqrt(S*C) * sigma < 1 for stability */
    float m = ecology_may_stability(10, 0.1f, 0.1f);
    EXPECT_LT(m, 1.0f);

    float m2 = ecology_may_stability(100, 0.5f, 0.5f);
    EXPECT_GT(m2, 1.0f);
}

/* ---- load temperate forest ----------------------------------------------- */

TEST_F(EcologyTest, LoadTemperateForest) {
    ecology_load_temperate_forest(sim);
    EXPECT_GE(sim->num_species, 3u);
}

/* ---- step basic ---------------------------------------------------------- */

TEST_F(EcologyTest, StepBasic) {
    int rc = ecology_step(sim, 1.0f);
    EXPECT_EQ(rc, 0);
}
