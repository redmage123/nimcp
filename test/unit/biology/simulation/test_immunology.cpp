/**
 * @file test_immunology.cpp
 * @brief Tests for the immunology simulation engine (gtest)
 *
 * Validates complement cascade, antibody binding affinity, vaccine secondary
 * response timing, cytokine dynamics, and infection management.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>

extern "C" {
#include "cognitive/physics/nimcp_immunology.h"
}

class ImmunologyTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = immunology_default_config();
        sim = immunology_create(&cfg);
    }
    void TearDown() override {
        if (sim) immunology_destroy(sim);
    }
    immunology_config_t cfg{};
    immunology_sim_t* sim = nullptr;
};

TEST_F(ImmunologyTest, CreateDestroy) {
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
}

/* ---- complement cascade activates ---------------------------------------- */

TEST(ImmunologyComplementTest, Activates) {
    immunology_config_t cfg = immunology_default_config();
    cfg.enable_complement = true;
    cfg.dt = 1.0f;
    immunology_sim_t* sim = immunology_create(&cfg);
    ASSERT_NE(sim, nullptr);

    int rc = immunology_activate_complement(sim, IMMUNO_COMPLEMENT_CLASSICAL);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(sim->complement.activated);

    float mac_before = sim->complement.mac_level;
    for (int i = 0; i < 50; i++) {
        immunology_step_complement(sim, 1.0f);
    }
    float mac_after = sim->complement.mac_level;
    EXPECT_GT(mac_after, mac_before);

    immunology_destroy(sim);
}

/* ---- antibody binding increases with affinity ---------------------------- */

TEST(ImmunologyAntibodyTest, BindingAffinity) {
    float ab_conc = 1.0e-6f;
    float ag_conc = 1.0e-6f;

    float binding_low = immunology_antibody_binding(IMMUNO_KA_LOW, ab_conc, ag_conc);
    float binding_high = immunology_antibody_binding(IMMUNO_KA_HIGH, ab_conc, ag_conc);

    EXPECT_GT(binding_high, binding_low);
}

TEST(ImmunologyAntibodyTest, BindingMonotonic) {
    float ab = 1.0e-6f, ag = 1.0e-6f;
    float prev = 0.0f;
    float Ka_values[] = {1e5f, 1e7f, 1e9f, 1e11f};
    for (int i = 0; i < 4; i++) {
        float b = immunology_antibody_binding(Ka_values[i], ab, ag);
        EXPECT_GE(b, prev);
        prev = b;
    }
}

/* ---- vaccine: secondary response faster than primary --------------------- */

TEST(ImmunologyVaccineTest, SecondaryFaster) {
    immunology_config_t cfg = immunology_default_config();
    cfg.enable_adaptive = true;
    cfg.enable_memory_cells = true;
    cfg.dt = 1.0f;
    immunology_sim_t* sim = immunology_create(&cfg);
    ASSERT_NE(sim, nullptr);

    immuno_pathogen_t pathogen = {};
    snprintf(pathogen.name, IMMUNO_MAX_NAME_LEN, "test_virus");
    pathogen.type = IMMUNO_PATHOGEN_VIRUS;
    pathogen.virulence = 0.5f;
    pathogen.replication_rate = 0.1f;
    pathogen.antigen_strength = 0.8f;
    pathogen.active = true;
    immunology_add_pathogen(sim, &pathogen);

    immunology_vaccinate(sim, 0, 1.0f);

    float primary_time = 0.0f;
    for (int i = 0; i < 500; i++) {
        immunology_step(sim, cfg.dt);
        primary_time += cfg.dt;
    }

    immunology_stats_t stats_primary = immunology_get_stats(sim);

    immunology_infect(sim, 0, 3.0f);

    for (int i = 0; i < 100; i++) {
        immunology_step(sim, cfg.dt);
    }

    immunology_stats_t stats_secondary = immunology_get_stats(sim);
    EXPECT_GE(stats_secondary.total_antibody_conc,
              stats_primary.total_antibody_conc * 0.5f);

    immunology_destroy(sim);
}

/* ---- add immune cell ----------------------------------------------------- */

TEST_F(ImmunologyTest, AddImmuneCell) {
    immuno_cell_t cell = {};
    cell.type = IMMUNO_CELL_NEUTROPHIL;
    cell.count = IMMUNO_NEUTROPHIL_NORMAL;
    cell.activation = 0.0f;
    cell.cytotoxicity = 0.3f;
    cell.phagocytic_rate = 10.0f;
    cell.active = true;

    int rc = immunology_add_immune_cell(sim, &cell);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(sim->num_cells, 1u);
}

/* ---- cytokine dynamics --------------------------------------------------- */

TEST_F(ImmunologyTest, CytokineDynamics) {
    sim->cytokines[IMMUNO_CYTOKINE_IL6].production_rate = 100.0f;
    sim->cytokines[IMMUNO_CYTOKINE_IL6].decay_rate = 0.1f;

    float initial = sim->cytokines[IMMUNO_CYTOKINE_IL6].concentration;
    for (int i = 0; i < 10; i++) {
        immunology_step_cytokines(sim, 1.0f);
    }
    float after = sim->cytokines[IMMUNO_CYTOKINE_IL6].concentration;
    EXPECT_GT(after, initial);
}

/* ---- load bacterial infection -------------------------------------------- */

TEST_F(ImmunologyTest, LoadBacterialInfection) {
    immunology_load_bacterial_infection(sim);
    EXPECT_GE(sim->num_pathogens, 1u);
}

/* ---- class switch -------------------------------------------------------- */

TEST_F(ImmunologyTest, AntibodyClassSwitch) {
    immuno_antibody_t ab = {};
    ab.ab_class = IMMUNO_AB_IGM;
    ab.concentration = 10.0f;
    ab.affinity_ka = IMMUNO_KA_MODERATE;
    ab.active = true;
    sim->antibodies[0] = ab;
    sim->num_antibodies = 1;

    int rc = immunology_class_switch(sim, 0, IMMUNO_AB_IGG);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(sim->antibodies[0].ab_class, IMMUNO_AB_IGG);
}

/* ---- step basic ---------------------------------------------------------- */

TEST_F(ImmunologyTest, StepBasic) {
    int rc = immunology_step(sim, 1.0f);
    EXPECT_EQ(rc, 0);
}
