/**
 * @file test_organic_chemistry.cpp
 * @brief Tests for organic chemistry: unsaturation, aromaticity, mechanisms (gtest)
 *
 * Validates degree of unsaturation, Huckel rule, SN2 stereochemistry,
 * molecule creation, reaction prediction.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>

extern "C" {
#include "cognitive/physics/nimcp_organic_chemistry.h"
}

class OrganicChemistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = organic_chemistry_default_config();
        sim = organic_chemistry_create(&cfg);
    }
    void TearDown() override {
        if (sim) organic_chemistry_destroy(sim);
    }
    ochem_config_t cfg{};
    organic_chemistry_sim_t* sim = nullptr;
};

TEST_F(OrganicChemistryTest, CreateDestroy) {
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
}

/* ---- degree of unsaturation: DoU = (2C+2+N-H-X)/2 ----------------------- */

TEST(OChemDOUTest, Benzene) {
    /* Benzene C6H6: DoU = (2*6+2-6)/2 = 4 */
    uint32_t dou = organic_chemistry_degree_unsaturation(6, 6, 0, 0);
    EXPECT_EQ(dou, 4u);
}

TEST(OChemDOUTest, Ethane) {
    /* Ethane C2H6: DoU = 0 (saturated) */
    uint32_t dou = organic_chemistry_degree_unsaturation(2, 6, 0, 0);
    EXPECT_EQ(dou, 0u);
}

TEST(OChemDOUTest, Ethylene) {
    /* Ethylene C2H4: DoU = 1 (one double bond) */
    uint32_t dou = organic_chemistry_degree_unsaturation(2, 4, 0, 0);
    EXPECT_EQ(dou, 1u);
}

TEST(OChemDOUTest, WithNitrogen) {
    /* Pyridine C5H5N: DoU = 4 */
    uint32_t dou = organic_chemistry_degree_unsaturation(5, 5, 1, 0);
    EXPECT_EQ(dou, 4u);
}

/* ---- Huckel rule: 4n+2 pi electrons = aromatic -------------------------- */

TEST(OChemHuckelTest, SixAromatic) {
    bool aromatic = organic_chemistry_is_huckel_aromatic(6);
    EXPECT_TRUE(aromatic);
}

TEST(OChemHuckelTest, FourAntiaromatic) {
    bool aromatic = organic_chemistry_is_huckel_aromatic(4);
    EXPECT_FALSE(aromatic);
}

TEST(OChemHuckelTest, TenAromatic) {
    bool aromatic = organic_chemistry_is_huckel_aromatic(10);
    EXPECT_TRUE(aromatic);
}

TEST(OChemHuckelTest, TwoAromatic) {
    bool aromatic = organic_chemistry_is_huckel_aromatic(2);
    EXPECT_TRUE(aromatic);
}

/* ---- SN2 stereochemistry: inversion ------------------------------------- */

TEST(OChemSN2Test, PredictsInversion) {
    ochem_stereochemistry_t result = organic_chemistry_predict_stereo(
        OCHEM_RXN_SN2, OCHEM_STEREO_R);
    EXPECT_EQ(result, OCHEM_STEREO_S);
}

TEST(OChemSN2Test, InversionSToR) {
    ochem_stereochemistry_t result = organic_chemistry_predict_stereo(
        OCHEM_RXN_SN2, OCHEM_STEREO_S);
    EXPECT_EQ(result, OCHEM_STEREO_R);
}

/* ---- create molecule ----------------------------------------------------- */

TEST_F(OrganicChemistryTest, CreateMolecule) {
    ochem_molecule_t mol = {};
    snprintf(mol.name, OCHEM_MAX_NAME, "methane");
    snprintf(mol.smiles, OCHEM_MAX_NAME, "C");
    mol.molecular_weight = 16.04f;
    mol.degree_of_unsaturation = 0;
    mol.is_aromatic = false;
    mol.active = true;

    mol.atoms[0].type = OCHEM_ATOM_C;
    mol.atoms[0].hybridization = 3; /* sp3 */
    mol.atoms[0].num_hydrogens = 4;
    mol.num_atoms = 1;

    uint32_t id = organic_chemistry_add_molecule(sim, &mol);
    EXPECT_EQ(sim->num_molecules, 1u);
    (void)id;
}

/* ---- step ---------------------------------------------------------------- */

TEST_F(OrganicChemistryTest, StepBasic) {
    int rc = organic_chemistry_step(sim, 0.01f);
    EXPECT_EQ(rc, 0);
}
