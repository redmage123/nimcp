/**
 * @file test_chemistry.cpp
 * @brief Unit tests for chemistry reasoning module
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_chemistry.h"

class ChemistryTest : public ::testing::Test {
protected:
    chemistry_t* chem = nullptr;

    void SetUp() override {
        chem = chemistry_create();
        ASSERT_NE(chem, nullptr);
    }

    void TearDown() override {
        chemistry_destroy(chem);
        chem = nullptr;
    }

    static constexpr float FLOAT_TOLERANCE = 0.01f;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ChemistryTest, CreateDefault)
{
    EXPECT_NE(chem, nullptr);
}

TEST_F(ChemistryTest, CreateCustom)
{
    chemistry_config_t config = chemistry_default_config();
    config.temperature_k = 373.15f;  // 100°C
    config.pressure_atm = 2.0f;

    chemistry_t* custom = chemistry_create_custom(&config);
    ASSERT_NE(custom, nullptr);

    chemistry_destroy(custom);
}

TEST_F(ChemistryTest, CreateWithNullConfig)
{
    chemistry_t* c = chemistry_create_custom(nullptr);
    EXPECT_NE(c, nullptr);
    chemistry_destroy(c);
}

TEST_F(ChemistryTest, DestroyNullSafe)
{
    chemistry_destroy(nullptr);  // Should not crash
}

TEST_F(ChemistryTest, DefaultConfig)
{
    chemistry_config_t config = chemistry_default_config();
    EXPECT_NEAR(config.temperature_k, 298.15f, FLOAT_TOLERANCE);
    EXPECT_NEAR(config.pressure_atm, 1.0f, FLOAT_TOLERANCE);
    EXPECT_TRUE(config.enable_thermodynamics);
}

TEST_F(ChemistryTest, ValidateConfig)
{
    chemistry_config_t config = chemistry_default_config();
    EXPECT_TRUE(chemistry_validate_config(&config));

    config.temperature_k = -100.0f;
    EXPECT_FALSE(chemistry_validate_config(&config));

    config.temperature_k = 298.15f;
    config.pressure_atm = 0.0f;
    EXPECT_FALSE(chemistry_validate_config(&config));
}

TEST_F(ChemistryTest, ValidateNullConfig)
{
    EXPECT_FALSE(chemistry_validate_config(nullptr));
}

//=============================================================================
// Periodic Table Tests
//=============================================================================

TEST_F(ChemistryTest, GetElementHydrogen)
{
    element_properties_t props;
    int result = chemistry_get_element(chem, 1, &props);

    EXPECT_EQ(result, 0);
    EXPECT_STREQ(props.symbol, "H");
    EXPECT_STREQ(props.name, "Hydrogen");
    EXPECT_NEAR(props.atomic_mass, 1.008f, FLOAT_TOLERANCE);
    EXPECT_EQ(props.period, 1);
    EXPECT_EQ(props.group, 1);
}

TEST_F(ChemistryTest, GetElementOxygen)
{
    element_properties_t props;
    int result = chemistry_get_element(chem, 8, &props);

    EXPECT_EQ(result, 0);
    EXPECT_STREQ(props.symbol, "O");
    EXPECT_NEAR(props.atomic_mass, 15.999f, FLOAT_TOLERANCE);
    EXPECT_NEAR(props.electronegativity, 3.44f, FLOAT_TOLERANCE);
}

TEST_F(ChemistryTest, GetElementBySymbol)
{
    element_properties_t props;
    int result = chemistry_get_element_by_symbol(chem, "Na", &props);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(props.atomic_number, 11);
    EXPECT_STREQ(props.name, "Sodium");
    EXPECT_EQ(props.category, ELEMENT_CATEGORY_ALKALI_METAL);
}

TEST_F(ChemistryTest, GetElementInvalidNumber)
{
    element_properties_t props;
    EXPECT_EQ(chemistry_get_element(chem, 0, &props), -1);
    EXPECT_EQ(chemistry_get_element(chem, 200, &props), -1);
}

TEST_F(ChemistryTest, GetElementNullHandling)
{
    element_properties_t props;
    EXPECT_EQ(chemistry_get_element(nullptr, 1, &props), -1);
    EXPECT_EQ(chemistry_get_element(chem, 1, nullptr), -1);
}

TEST_F(ChemistryTest, CanFormIonicBond)
{
    // NaCl - classic ionic bond
    EXPECT_TRUE(chemistry_can_form_ionic_bond(chem, 11, 17));  // Na + Cl

    // H2O - covalent, not ionic
    EXPECT_FALSE(chemistry_can_form_ionic_bond(chem, 1, 8));  // H + O
}

TEST_F(ChemistryTest, PredictBondType)
{
    // Na + Cl -> ionic
    EXPECT_EQ(chemistry_predict_bond_type(chem, 11, 17), BOND_IONIC);

    // Fe + Fe -> metallic
    EXPECT_EQ(chemistry_predict_bond_type(chem, 26, 26), BOND_METALLIC);

    // C + H -> covalent
    EXPECT_EQ(chemistry_predict_bond_type(chem, 6, 1), BOND_SINGLE);
}

//=============================================================================
// Molecule Tests
//=============================================================================

TEST_F(ChemistryTest, ParseWater)
{
    molecule_t mol;
    int result = chemistry_parse_molecule(chem, "H2O", &mol);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(mol.num_atoms, 3);  // 2 H + 1 O
    EXPECT_STREQ(mol.formula, "H2O");
    EXPECT_NEAR(mol.molar_mass, 18.015f, 0.1f);  // 2*1.008 + 15.999
}

TEST_F(ChemistryTest, ParseCO2)
{
    molecule_t mol;
    int result = chemistry_parse_molecule(chem, "CO2", &mol);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(mol.num_atoms, 3);  // 1 C + 2 O
    EXPECT_NEAR(mol.molar_mass, 44.009f, 0.1f);  // 12.011 + 2*15.999
}

TEST_F(ChemistryTest, ParseGlucose)
{
    molecule_t mol;
    int result = chemistry_parse_molecule(chem, "C6H12O6", &mol);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(mol.num_atoms, 24);  // 6 C + 12 H + 6 O
    EXPECT_NEAR(mol.molar_mass, 180.156f, 0.5f);
}

TEST_F(ChemistryTest, ParseNaCl)
{
    molecule_t mol;
    int result = chemistry_parse_molecule(chem, "NaCl", &mol);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(mol.num_atoms, 2);
    EXPECT_NEAR(mol.molar_mass, 58.44f, 0.1f);  // 22.990 + 35.45
}

TEST_F(ChemistryTest, MolarMass)
{
    molecule_t mol;
    chemistry_parse_molecule(chem, "H2O", &mol);

    float mass = chemistry_molar_mass(chem, &mol);
    EXPECT_NEAR(mass, 18.015f, 0.1f);
}

TEST_F(ChemistryTest, CountElement)
{
    molecule_t mol;
    chemistry_parse_molecule(chem, "C6H12O6", &mol);

    EXPECT_EQ(chemistry_count_element(&mol, 6), 6);   // Carbon
    EXPECT_EQ(chemistry_count_element(&mol, 1), 12);  // Hydrogen
    EXPECT_EQ(chemistry_count_element(&mol, 8), 6);   // Oxygen
}

TEST_F(ChemistryTest, MoleculeToString)
{
    molecule_t mol;
    chemistry_parse_molecule(chem, "H2SO4", &mol);

    char buffer[64];
    int result = chemistry_molecule_to_string(&mol, buffer, sizeof(buffer));

    EXPECT_EQ(result, 0);
    EXPECT_STREQ(buffer, "H2SO4");
}

TEST_F(ChemistryTest, ParseMoleculeNullHandling)
{
    molecule_t mol;
    EXPECT_EQ(chemistry_parse_molecule(nullptr, "H2O", &mol), -1);
    EXPECT_EQ(chemistry_parse_molecule(chem, nullptr, &mol), -1);
    EXPECT_EQ(chemistry_parse_molecule(chem, "H2O", nullptr), -1);
}

//=============================================================================
// Reaction Tests
//=============================================================================

TEST_F(ChemistryTest, ParseReaction)
{
    reaction_t rxn;
    int result = chemistry_parse_reaction(chem, "H2 + O2 -> H2O", &rxn);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(rxn.num_reactants, 2);
    EXPECT_EQ(rxn.num_products, 1);
}

TEST_F(ChemistryTest, ParseReactionWithCoefficients)
{
    reaction_t rxn;
    int result = chemistry_parse_reaction(chem, "2H2 + O2 -> 2H2O", &rxn);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(rxn.reactants[0].coefficient, 2);
    EXPECT_EQ(rxn.reactants[1].coefficient, 1);
    EXPECT_EQ(rxn.products[0].coefficient, 2);
}

TEST_F(ChemistryTest, IsBalanced)
{
    reaction_t rxn;

    // Unbalanced: H2 + O2 -> H2O
    chemistry_parse_reaction(chem, "H2 + O2 -> H2O", &rxn);
    EXPECT_FALSE(chemistry_is_balanced(&rxn));

    // Balanced: 2H2 + O2 -> 2H2O
    chemistry_parse_reaction(chem, "2H2 + O2 -> 2H2O", &rxn);
    EXPECT_TRUE(chemistry_is_balanced(&rxn));
}

TEST_F(ChemistryTest, BalanceEquation)
{
    reaction_t rxn;
    chemistry_parse_reaction(chem, "H2 + O2 -> H2O", &rxn);

    bool balanced = chemistry_balance_equation(chem, &rxn);
    EXPECT_TRUE(balanced);
    EXPECT_TRUE(rxn.is_balanced);

    // Check coefficients
    EXPECT_EQ(rxn.reactants[0].coefficient, 2);  // 2H2
    EXPECT_EQ(rxn.reactants[1].coefficient, 1);  // O2
    EXPECT_EQ(rxn.products[0].coefficient, 2);   // 2H2O
}

TEST_F(ChemistryTest, ClassifyReactionSynthesis)
{
    reaction_t rxn;
    chemistry_parse_reaction(chem, "2H2 + O2 -> 2H2O", &rxn);

    reaction_type_t type = chemistry_classify_reaction(chem, &rxn);
    // Note: This returns SYNTHESIS because 2 reactants -> 1 product
    // Even though coefficients make it look like more
    EXPECT_EQ(type, REACTION_SYNTHESIS);
}

TEST_F(ChemistryTest, ClassifyReactionDecomposition)
{
    reaction_t rxn;
    chemistry_parse_reaction(chem, "2H2O -> 2H2 + O2", &rxn);

    reaction_type_t type = chemistry_classify_reaction(chem, &rxn);
    EXPECT_EQ(type, REACTION_DECOMPOSITION);
}

TEST_F(ChemistryTest, IsSpontaneous)
{
    reaction_t rxn = {};
    rxn.enthalpy_kj = -100.0f;  // Exothermic
    rxn.entropy_j_k = 50.0f;    // Positive entropy change

    // G = H - TS = -100000 - 298*50 < 0
    EXPECT_TRUE(chemistry_is_spontaneous(&rxn, 298.15f));

    rxn.enthalpy_kj = 100.0f;   // Endothermic
    rxn.entropy_j_k = -50.0f;   // Negative entropy change

    // G = 100000 - 298*(-50) > 0
    EXPECT_FALSE(chemistry_is_spontaneous(&rxn, 298.15f));
}

TEST_F(ChemistryTest, ParseReactionNoArrow)
{
    reaction_t rxn;
    int result = chemistry_parse_reaction(chem, "H2 + O2 = H2O", &rxn);
    EXPECT_EQ(result, -1);  // No arrow
}

//=============================================================================
// Stoichiometry Tests
//=============================================================================

TEST_F(ChemistryTest, CalculateStoichiometry)
{
    reaction_t rxn;
    chemistry_parse_reaction(chem, "2H2 + O2 -> 2H2O", &rxn);

    stoichiometry_result_t result;
    int ret = chemistry_calculate_stoichiometry(chem, &rxn, 2.0f, 0, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_NEAR(result.moles_needed[0], 2.0f, FLOAT_TOLERANCE);  // H2
    EXPECT_NEAR(result.moles_needed[1], 1.0f, FLOAT_TOLERANCE);  // O2
    EXPECT_NEAR(result.moles_produced[0], 2.0f, FLOAT_TOLERANCE);  // H2O
}

TEST_F(ChemistryTest, FindLimitingReagent)
{
    reaction_t rxn;
    chemistry_parse_reaction(chem, "2H2 + O2 -> 2H2O", &rxn);

    // 3 moles H2, 1 mole O2
    // H2 needs 2:1 ratio, so 3/2 = 1.5 moles O2 needed
    // But we only have 1 mole O2, so O2 is limiting
    float moles[] = {3.0f, 1.0f};
    uint32_t limiting = chemistry_find_limiting_reagent(chem, &rxn, moles);

    EXPECT_EQ(limiting, 1);  // O2 is limiting

    // 2 moles H2, 2 moles O2
    // H2 needs 2:1 ratio, so 2/2 = 1 mole O2 needed
    // We have 2 moles O2, so H2 is limiting
    float moles2[] = {2.0f, 2.0f};
    limiting = chemistry_find_limiting_reagent(chem, &rxn, moles2);

    EXPECT_EQ(limiting, 0);  // H2 is limiting
}

TEST_F(ChemistryTest, PercentYield)
{
    float percent = chemistry_percent_yield(36.0f, 30.0f);
    EXPECT_NEAR(percent, 83.33f, 0.1f);

    percent = chemistry_percent_yield(100.0f, 100.0f);
    EXPECT_NEAR(percent, 100.0f, FLOAT_TOLERANCE);
}

TEST_F(ChemistryTest, MolesToGrams)
{
    // 2 moles of water (18.015 g/mol)
    float grams = chemistry_moles_to_grams(2.0f, 18.015f);
    EXPECT_NEAR(grams, 36.03f, 0.1f);
}

TEST_F(ChemistryTest, GramsToMoles)
{
    // 36 grams of water (18.015 g/mol)
    float moles = chemistry_grams_to_moles(36.0f, 18.015f);
    EXPECT_NEAR(moles, 2.0f, 0.01f);
}

TEST_F(ChemistryTest, StoichiometryUnbalanced)
{
    reaction_t rxn;
    chemistry_parse_reaction(chem, "H2 + O2 -> H2O", &rxn);  // Unbalanced

    stoichiometry_result_t result;
    int ret = chemistry_calculate_stoichiometry(chem, &rxn, 2.0f, 0, &result);
    EXPECT_EQ(ret, -1);  // Should fail for unbalanced
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(ChemistryTest, SetInflammation)
{
    EXPECT_EQ(chemistry_set_inflammation(chem, 0.5f), 0);
    EXPECT_EQ(chemistry_set_inflammation(chem, 1.5f), 0);  // Should clamp
    EXPECT_EQ(chemistry_set_inflammation(chem, -0.5f), 0);  // Should clamp
}

TEST_F(ChemistryTest, SetSleepDeprivation)
{
    EXPECT_EQ(chemistry_set_sleep_deprivation(chem, 0.5f), 0);
}

TEST_F(ChemistryTest, ModulationNullHandling)
{
    EXPECT_EQ(chemistry_set_inflammation(nullptr, 0.5f), -1);
    EXPECT_EQ(chemistry_set_sleep_deprivation(nullptr, 0.5f), -1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ChemistryTest, GetStats)
{
    // Perform some operations
    molecule_t mol;
    chemistry_parse_molecule(chem, "H2O", &mol);
    chemistry_parse_molecule(chem, "CO2", &mol);

    element_properties_t props;
    chemistry_get_element(chem, 1, &props);

    chemistry_stats_t stats;
    int result = chemistry_get_stats(chem, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.molecules_parsed, 2);
    EXPECT_GE(stats.property_lookups, 1);
}

TEST_F(ChemistryTest, ResetStats)
{
    molecule_t mol;
    chemistry_parse_molecule(chem, "H2O", &mol);

    chemistry_reset_stats(chem);

    chemistry_stats_t stats;
    chemistry_get_stats(chem, &stats);

    EXPECT_EQ(stats.molecules_parsed, 0);
    EXPECT_EQ(stats.reactions_balanced, 0);
}

TEST_F(ChemistryTest, StatsNullHandling)
{
    chemistry_stats_t stats;
    EXPECT_EQ(chemistry_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(chemistry_get_stats(chem, nullptr), -1);
}

TEST_F(ChemistryTest, ResetStatsNullSafe)
{
    chemistry_reset_stats(nullptr);  // Should not crash
}

TEST_F(ChemistryTest, GetLastError)
{
    const char* err = chemistry_get_last_error();
    EXPECT_NE(err, nullptr);
}
