/**
 * @file test_biology_sim.cpp
 * @brief Tests for the core biology simulation engine (gtest)
 *
 * Validates create/destroy, species management, logistic growth,
 * predator-prey oscillation stability, biodiversity index, ecosystem loading.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>

extern "C" {
#include "cognitive/physics/nimcp_biology_sim.h"
}

class BiologySimTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = biology_sim_default_config();
        sim = biology_sim_create(&cfg);
    }
    void TearDown() override {
        if (sim) biology_sim_destroy(sim);
    }
    bio_config_t cfg{};
    biology_sim_t* sim = nullptr;
};

TEST_F(BiologySimTest, CreateDestroy) {
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
}

TEST(BiologySimCreateTest, CreateNullConfig) {
    biology_sim_t* s = biology_sim_create(NULL);
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->initialized);
    biology_sim_destroy(s);
}

TEST_F(BiologySimTest, AddSpecies) {
    bio_species_t sp = {};
    snprintf(sp.name, BIO_MAX_NAME_LEN, "rabbit");
    sp.kingdom = BIO_KINGDOM_ANIMAL;
    sp.trophic_level = BIO_TROPHIC_PRIMARY;
    sp.population = 100.0f;
    sp.carrying_capacity = 1000.0f;
    sp.growth_rate = 0.5f;
    sp.active = true;

    uint32_t id = biology_sim_add_species(sim, &sp);
    EXPECT_EQ(sim->num_species, 1u);
    EXPECT_NEAR(sim->species[id].population, 100.0f, 0.1f);
}

TEST(BiologySimGrowthTest, LogisticGrowth) {
    bio_config_t cfg = biology_sim_default_config();
    cfg.dt = 0.1f;
    biology_sim_t* sim = biology_sim_create(&cfg);
    ASSERT_NE(sim, nullptr);

    bio_species_t sp = {};
    snprintf(sp.name, BIO_MAX_NAME_LEN, "bacteria");
    sp.kingdom = BIO_KINGDOM_BACTERIA;
    sp.trophic_level = BIO_TROPHIC_DECOMPOSER;
    sp.population = 10.0f;
    sp.carrying_capacity = 1000.0f;
    sp.growth_rate = 1.0f;
    sp.active = true;
    uint32_t id = biology_sim_add_species(sim, &sp);

    for (int i = 0; i < 500; i++) {
        biology_sim_step(sim, cfg.dt);
    }

    float pop = sim->species[id].population;
    EXPECT_GT(pop, 900.0f);
    EXPECT_LT(pop, 1100.0f);

    biology_sim_destroy(sim);
}

TEST(BiologySimGrowthTest, PopulationBounded) {
    bio_config_t cfg = biology_sim_default_config();
    cfg.dt = 0.1f;
    biology_sim_t* sim = biology_sim_create(&cfg);
    ASSERT_NE(sim, nullptr);

    bio_species_t sp = {};
    snprintf(sp.name, BIO_MAX_NAME_LEN, "elk");
    sp.population = 500.0f;
    sp.carrying_capacity = 500.0f;
    sp.growth_rate = 0.3f;
    sp.active = true;
    uint32_t id = biology_sim_add_species(sim, &sp);

    for (int i = 0; i < 100; i++) {
        biology_sim_step(sim, cfg.dt);
    }

    float pop = sim->species[id].population;
    EXPECT_NEAR(pop, 500.0f, 50.0f);

    biology_sim_destroy(sim);
}

TEST(BiologySimPredatorPreyTest, Oscillation) {
    bio_config_t cfg = biology_sim_default_config();
    cfg.dt = 0.01f;
    biology_sim_t* sim = biology_sim_create(&cfg);
    ASSERT_NE(sim, nullptr);

    bio_species_t prey = {};
    snprintf(prey.name, BIO_MAX_NAME_LEN, "hare");
    prey.trophic_level = BIO_TROPHIC_PRIMARY;
    prey.population = 100.0f;
    prey.carrying_capacity = 500.0f;
    prey.growth_rate = 0.5f;
    prey.active = true;
    uint32_t prey_id = biology_sim_add_species(sim, &prey);

    bio_species_t pred = {};
    snprintf(pred.name, BIO_MAX_NAME_LEN, "lynx");
    pred.trophic_level = BIO_TROPHIC_SECONDARY;
    pred.population = 20.0f;
    pred.carrying_capacity = 100.0f;
    pred.growth_rate = 0.1f;
    pred.active = true;
    uint32_t pred_id = biology_sim_add_species(sim, &pred);

    bio_interaction_t inter = {};
    inter.species_a = pred_id;
    inter.species_b = prey_id;
    inter.type = BIO_INTERACT_PREDATION;
    inter.strength = 0.01f;
    inter.efficiency = 0.1f;
    inter.active = true;
    biology_sim_add_interaction(sim, &inter);

    for (int i = 0; i < 1000; i++) {
        biology_sim_step(sim, cfg.dt);
    }

    float prey_pop = sim->species[prey_id].population;
    float pred_pop = sim->species[pred_id].population;
    EXPECT_GT(prey_pop, 0.1f);
    EXPECT_LT(prey_pop, 10000.0f);
    EXPECT_GT(pred_pop, 0.1f);
    EXPECT_LT(pred_pop, 10000.0f);

    biology_sim_destroy(sim);
}

TEST_F(BiologySimTest, BiodiversityIndex) {
    for (int i = 0; i < 3; i++) {
        bio_species_t sp = {};
        snprintf(sp.name, BIO_MAX_NAME_LEN, "sp_%d", i);
        sp.population = 100.0f;
        sp.carrying_capacity = 1000.0f;
        sp.active = true;
        biology_sim_add_species(sim, &sp);
    }

    float H = biology_sim_biodiversity(sim);
    EXPECT_NEAR(H, logf(3.0f), 0.1f);
}

TEST_F(BiologySimTest, TotalBiomass) {
    bio_species_t sp = {};
    sp.population = 50.0f;
    sp.active = true;
    biology_sim_add_species(sim, &sp);

    float biomass = biology_sim_total_biomass(sim);
    EXPECT_GT(biomass, 0.0f);
}

TEST_F(BiologySimTest, LoadGrassland) {
    biology_sim_load_grassland(sim);
    EXPECT_GE(sim->num_species, 3u);
    EXPECT_GE(sim->num_interactions, 1u);
}

TEST_F(BiologySimTest, StepBasic) {
    int rc = biology_sim_step(sim, 0.1f);
    EXPECT_EQ(rc, 0);
}
