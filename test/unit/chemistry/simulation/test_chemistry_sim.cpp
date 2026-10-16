/**
 * @file test_chemistry_sim.cpp
 * @brief Tests for the core chemistry simulation engine (gtest)
 *
 * Validates create/destroy lifecycle, element/substance/reaction management,
 * simulation stepping, mass conservation, pH computation, and preset loading.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>

extern "C" {
#include "cognitive/physics/nimcp_chemistry_sim.h"
}

class ChemistrySimTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = chemistry_sim_default_config();
        sim = chemistry_sim_create(&cfg);
    }
    void TearDown() override {
        if (sim) chemistry_sim_destroy(sim);
    }
    chem_config_t cfg{};
    chemistry_sim_t* sim = nullptr;
};

TEST_F(ChemistrySimTest, CreateDestroy) {
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
    EXPECT_EQ(sim->num_elements, 0u);
    EXPECT_EQ(sim->num_substances, 0u);
    EXPECT_EQ(sim->num_reactions, 0u);
}

TEST(ChemistrySimCreateTest, CreateNullConfig) {
    chemistry_sim_t* s = chemistry_sim_create(NULL);
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->initialized);
    chemistry_sim_destroy(s);
}

TEST(ChemistrySimConfigTest, DefaultConfigValues) {
    chem_config_t cfg = chemistry_sim_default_config();
    EXPECT_NEAR(cfg.default_temperature, 298.15f, 1.0f);
    EXPECT_NEAR(cfg.default_pressure, 1.0f, 0.1f);
    EXPECT_GT(cfg.default_volume, 0.0f);
    EXPECT_GT(cfg.dt, 0.0f);
}

TEST_F(ChemistrySimTest, AddElement) {
    uint32_t h = chemistry_sim_add_element(sim, "H", 1.008f, 1);
    uint32_t o = chemistry_sim_add_element(sim, "O", 15.999f, 8);
    EXPECT_EQ(sim->num_elements, 2u);
    EXPECT_NE(h, o);
}

TEST_F(ChemistrySimTest, AddSubstance) {
    chem_substance_t water = {};
    snprintf(water.name, CHEM_MAX_NAME_LEN, "water");
    snprintf(water.formula, CHEM_MAX_NAME_LEN, "H2O");
    water.molar_mass = 18.015f;
    water.default_phase = CHEM_PHASE_LIQUID;
    water.active = true;

    uint32_t id = chemistry_sim_add_substance(sim, &water);
    EXPECT_EQ(sim->num_substances, 1u);
    (void)id;
}

TEST_F(ChemistrySimTest, AddReaction) {
    chem_substance_t sa = {};
    sa.active = true;
    snprintf(sa.name, CHEM_MAX_NAME_LEN, "A");
    chem_substance_t sb = {};
    sb.active = true;
    snprintf(sb.name, CHEM_MAX_NAME_LEN, "B");
    uint32_t aid = chemistry_sim_add_substance(sim, &sa);
    uint32_t bid = chemistry_sim_add_substance(sim, &sb);

    chem_reaction_t rxn = {};
    rxn.reactant_ids[0] = aid;
    rxn.reactant_coeffs[0] = 1;
    rxn.num_reactants = 1;
    rxn.product_ids[0] = bid;
    rxn.product_coeffs[0] = 1;
    rxn.num_products = 1;
    rxn.rate_constant = 0.1f;
    rxn.active = true;

    uint32_t rid = chemistry_sim_add_reaction(sim, &rxn);
    EXPECT_EQ(sim->num_reactions, 1u);
    (void)rid;
}

TEST_F(ChemistrySimTest, StepBasic) {
    int rc = chemistry_sim_step(sim, 0.01f);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(sim->step_count, 1u);
}

TEST_F(ChemistrySimTest, MassConservation100Steps) {
    chemistry_sim_load_common_elements(sim);
    chemistry_sim_load_common_substances(sim);
    chemistry_sim_load_common_reactions(sim);

    for (uint32_t i = 0; i < sim->num_substances; i++) {
        chemistry_sim_set_concentration(sim, i, 1.0f);
    }

    float mass_before = chemistry_sim_total_mass(sim);
    ASSERT_GT(mass_before, 0.0f);

    for (int i = 0; i < 100; i++) {
        chemistry_sim_step(sim, cfg.dt);
    }

    float mass_after = chemistry_sim_total_mass(sim);
    EXPECT_NEAR(mass_after, mass_before, mass_before * 0.01f);
}

TEST_F(ChemistrySimTest, PhComputation) {
    chemistry_sim_load_common_elements(sim);
    chemistry_sim_load_common_substances(sim);

    float ph = chemistry_sim_get_ph(sim);
    EXPECT_GE(ph, 0.0f);
    EXPECT_LE(ph, 14.0f);
}

TEST_F(ChemistrySimTest, CommonElementsLoaded) {
    chemistry_sim_load_common_elements(sim);
    EXPECT_GE(sim->num_elements, 11u);
}

TEST_F(ChemistrySimTest, SetTemperature) {
    chemistry_sim_set_temperature(sim, 373.15f);
    EXPECT_NEAR(sim->state.temperature, 373.15f, 0.1f);
}

TEST_F(ChemistrySimTest, SetConcentration) {
    chem_substance_t s = {};
    s.active = true;
    snprintf(s.name, CHEM_MAX_NAME_LEN, "test");
    uint32_t id = chemistry_sim_add_substance(sim, &s);

    chemistry_sim_set_concentration(sim, id, 2.5f);
    EXPECT_NEAR(sim->state.concentrations[id], 2.5f, 0.001f);
}

TEST_F(ChemistrySimTest, AtomCountConservation) {
    chemistry_sim_load_common_elements(sim);
    chemistry_sim_load_common_substances(sim);
    chemistry_sim_load_common_reactions(sim);

    for (uint32_t i = 0; i < sim->num_substances; i++) {
        chemistry_sim_set_concentration(sim, i, 1.0f);
    }

    float counts_before[CHEM_MAX_ELEMENTS] = {};
    chemistry_sim_atom_counts(sim, counts_before, CHEM_MAX_ELEMENTS);

    for (int i = 0; i < 50; i++) {
        chemistry_sim_step(sim, cfg.dt);
    }

    float counts_after[CHEM_MAX_ELEMENTS] = {};
    chemistry_sim_atom_counts(sim, counts_after, CHEM_MAX_ELEMENTS);

    for (uint32_t e = 0; e < sim->num_elements; e++) {
        if (counts_before[e] > 0.0f) {
            EXPECT_NEAR(counts_after[e], counts_before[e],
                        counts_before[e] * 0.01f);
        }
    }
}
