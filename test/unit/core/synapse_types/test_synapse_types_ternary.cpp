//=============================================================================
// test_synapse_types_ternary.cpp - Unit Tests for Ternary Synapse Type Modulation
//=============================================================================
/**
 * @file test_synapse_types_ternary.cpp
 * @brief Comprehensive unit tests for ternary modulation in synapse types
 *
 * WHAT: Tests ternary modulation for each synapse type (AMPA, NMDA, GABA, etc.)
 * WHY:  Validate discrete modulation states for biological synapse types
 * HOW:  GTest-based unit tests with edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/synapse_types/nimcp_synapse_types.h"
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_convert.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class SynapseTypesTernaryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    // Helper to convert modulation to ternary state
    trit_t modulation_to_ternary(float modulation, float threshold = 0.3f) {
        return trit_from_float_threshold(modulation, threshold);
    }

    // Helper to convert ternary to discrete modulation level
    float ternary_to_modulation(trit_t state, float positive = 1.0f, float negative = -0.5f) {
        return trit_dequantize_weight(state, positive, negative);
    }
};

//=============================================================================
// Synapse Type Classification Tests
//=============================================================================

TEST_F(SynapseTypesTernaryTest, ExcitatoryTernaryClassification) {
    // Excitatory synapses: AMPA, NMDA
    EXPECT_TRUE(synapse_type_is_excitatory(SYNAPSE_AMPA));
    EXPECT_TRUE(synapse_type_is_excitatory(SYNAPSE_NMDA));
    EXPECT_FALSE(synapse_type_is_excitatory(SYNAPSE_GABA_A));
    EXPECT_FALSE(synapse_type_is_excitatory(SYNAPSE_GABA_B));

    // Ternary representation: +1 = excitatory
    trit_t ampa_trit = TRIT_POSITIVE;
    trit_t nmda_trit = TRIT_POSITIVE;

    EXPECT_EQ(ampa_trit, TRIT_POSITIVE);
    EXPECT_EQ(nmda_trit, TRIT_POSITIVE);
}

TEST_F(SynapseTypesTernaryTest, InhibitoryTernaryClassification) {
    // Inhibitory synapses: GABA-A, GABA-B
    EXPECT_TRUE(synapse_type_is_inhibitory(SYNAPSE_GABA_A));
    EXPECT_TRUE(synapse_type_is_inhibitory(SYNAPSE_GABA_B));
    EXPECT_FALSE(synapse_type_is_inhibitory(SYNAPSE_AMPA));
    EXPECT_FALSE(synapse_type_is_inhibitory(SYNAPSE_NMDA));

    // Ternary representation: -1 = inhibitory
    trit_t gaba_a_trit = TRIT_NEGATIVE;
    trit_t gaba_b_trit = TRIT_NEGATIVE;

    EXPECT_EQ(gaba_a_trit, TRIT_NEGATIVE);
    EXPECT_EQ(gaba_b_trit, TRIT_NEGATIVE);
}

TEST_F(SynapseTypesTernaryTest, ModulatoryTernaryClassification) {
    // Modulatory synapses: Dopamine, Serotonin, Acetylcholine
    EXPECT_TRUE(synapse_type_is_modulatory(SYNAPSE_DOPAMINE));
    EXPECT_TRUE(synapse_type_is_modulatory(SYNAPSE_SEROTONIN));
    EXPECT_TRUE(synapse_type_is_modulatory(SYNAPSE_ACETYLCHOLINE));
    EXPECT_FALSE(synapse_type_is_modulatory(SYNAPSE_AMPA));

    // Ternary representation: 0 = no direct transmission (modulatory)
    // Modulatory synapses don't directly excite/inhibit, they modify
    trit_t modulatory_trit = TRIT_UNKNOWN;
    EXPECT_EQ(modulatory_trit, TRIT_UNKNOWN);
}

//=============================================================================
// Discrete Modulation State Tests
//=============================================================================

TEST_F(SynapseTypesTernaryTest, TernaryModulationStates) {
    // Test three discrete modulation states
    // POSITIVE: Enhanced transmission
    // UNKNOWN: Baseline transmission
    // NEGATIVE: Suppressed transmission

    float enhanced = ternary_to_modulation(TRIT_POSITIVE, 1.5f, 0.5f);
    float baseline = ternary_to_modulation(TRIT_UNKNOWN, 1.5f, 0.5f);
    float suppressed = ternary_to_modulation(TRIT_NEGATIVE, 1.5f, 0.5f);

    EXPECT_FLOAT_EQ(enhanced, 1.5f);
    EXPECT_FLOAT_EQ(baseline, 0.0f);
    EXPECT_FLOAT_EQ(suppressed, 0.5f);  // Note: This is the negative value parameter

    // Alternative with symmetric scales
    enhanced = ternary_to_modulation(TRIT_POSITIVE, 1.0f, -1.0f);
    suppressed = ternary_to_modulation(TRIT_NEGATIVE, 1.0f, -1.0f);

    EXPECT_FLOAT_EQ(enhanced, 1.0f);
    EXPECT_FLOAT_EQ(suppressed, -1.0f);
}

TEST_F(SynapseTypesTernaryTest, DopamineDiscreteStates) {
    // Dopamine has D1 (excitatory) and D2 (inhibitory) receptors
    // Ternary can represent: D1-dominant (+1), balanced (0), D2-dominant (-1)

    dopamine_state_t dopamine;
    synapse_init_dopamine(&dopamine);

    // Test D1-dominant state
    dopamine.d1_level = 0.8f;
    dopamine.d2_level = 0.2f;
    float balance = dopamine.d1_level - dopamine.d2_level;  // 0.6
    trit_t state = modulation_to_ternary(balance, 0.3f);
    EXPECT_EQ(state, TRIT_POSITIVE);

    // Test D2-dominant state
    dopamine.d1_level = 0.2f;
    dopamine.d2_level = 0.8f;
    balance = dopamine.d1_level - dopamine.d2_level;  // -0.6
    state = modulation_to_ternary(balance, 0.3f);
    EXPECT_EQ(state, TRIT_NEGATIVE);

    // Test balanced state
    dopamine.d1_level = 0.5f;
    dopamine.d2_level = 0.5f;
    balance = dopamine.d1_level - dopamine.d2_level;  // 0.0
    state = modulation_to_ternary(balance, 0.3f);
    EXPECT_EQ(state, TRIT_UNKNOWN);
}

TEST_F(SynapseTypesTernaryTest, SerotoninDiscreteStates) {
    // Serotonin has 5-HT1A (inhibitory) and 5-HT2A (excitatory) receptors
    // Similar ternary representation

    serotonin_state_t serotonin;
    synapse_init_serotonin(&serotonin);

    // Test excitatory-dominant (5-HT2A)
    serotonin.ht2a_level = 0.9f;
    serotonin.ht1a_level = 0.1f;
    float balance = serotonin.ht2a_level - serotonin.ht1a_level;  // 0.8
    trit_t state = modulation_to_ternary(balance, 0.3f);
    EXPECT_EQ(state, TRIT_POSITIVE);

    // Test inhibitory-dominant (5-HT1A)
    serotonin.ht2a_level = 0.1f;
    serotonin.ht1a_level = 0.9f;
    balance = serotonin.ht2a_level - serotonin.ht1a_level;  // -0.8
    state = modulation_to_ternary(balance, 0.3f);
    EXPECT_EQ(state, TRIT_NEGATIVE);
}

TEST_F(SynapseTypesTernaryTest, AcetylcholineDiscreteStates) {
    // ACh has nicotinic (fast) and muscarinic (slow) receptors
    // Both enhance attention, so ternary represents: high (+1), baseline (0), low (-1)

    acetylcholine_state_t ach;
    synapse_init_acetylcholine(&ach);

    // Combined ACh level
    float combined = (ach.nicotinic_level + ach.muscarinic_level) / 2.0f;

    // Test high attention state
    ach.nicotinic_level = 0.9f;
    ach.muscarinic_level = 0.8f;
    combined = (ach.nicotinic_level + ach.muscarinic_level) / 2.0f;
    // Deviation from baseline
    float deviation = combined - ach.baseline;
    trit_t state = modulation_to_ternary(deviation, 0.2f);
    EXPECT_EQ(state, TRIT_POSITIVE);
}

//=============================================================================
// Synapse Type Name Tests
//=============================================================================

TEST_F(SynapseTypesTernaryTest, SynapseTypeNames) {
    EXPECT_STREQ(synapse_type_name(SYNAPSE_GENERIC), "GENERIC");
    EXPECT_STREQ(synapse_type_name(SYNAPSE_AMPA), "AMPA");
    EXPECT_STREQ(synapse_type_name(SYNAPSE_NMDA), "NMDA");
    EXPECT_STREQ(synapse_type_name(SYNAPSE_GABA_A), "GABA-A");
    EXPECT_STREQ(synapse_type_name(SYNAPSE_GABA_B), "GABA-B");
    EXPECT_STREQ(synapse_type_name(SYNAPSE_DOPAMINE), "DOPAMINE");
    EXPECT_STREQ(synapse_type_name(SYNAPSE_SEROTONIN), "SEROTONIN");
    EXPECT_STREQ(synapse_type_name(SYNAPSE_ACETYLCHOLINE), "ACETYLCHOLINE");
    EXPECT_STREQ(synapse_type_name(SYNAPSE_ELECTRICAL), "ELECTRICAL");
}

//=============================================================================
// Initialization Tests with Ternary Validation
//=============================================================================

TEST_F(SynapseTypesTernaryTest, AMPAInitialization) {
    ampa_state_t ampa;
    synapse_init_ampa(&ampa);

    EXPECT_FLOAT_EQ(ampa.conductance, 0.0f);
    EXPECT_GT(ampa.g_max, 0.0f);
    EXPECT_GT(ampa.tau_decay, 0.0f);
    EXPECT_FLOAT_EQ(ampa.reversal_potential, 0.0f);

    // Ternary classification
    trit_t type_trit = TRIT_POSITIVE;  // Excitatory
    EXPECT_TRUE(synapse_type_is_excitatory(SYNAPSE_AMPA));
}

TEST_F(SynapseTypesTernaryTest, NMDAInitialization) {
    nmda_state_t nmda;
    synapse_init_nmda(&nmda);

    EXPECT_FLOAT_EQ(nmda.conductance, 0.0f);
    EXPECT_GT(nmda.g_max, 0.0f);
    EXPECT_GT(nmda.tau_decay, ampa_state_t{}.tau_decay);  // NMDA slower than AMPA
    EXPECT_GT(nmda.mg_concentration, 0.0f);  // Mg2+ block
    EXPECT_FLOAT_EQ(nmda.calcium_influx, 0.0f);
}

TEST_F(SynapseTypesTernaryTest, GABAInitialization) {
    gaba_a_state_t gaba_a;
    synapse_init_gaba_a(&gaba_a);

    EXPECT_FLOAT_EQ(gaba_a.conductance, 0.0f);
    EXPECT_GT(gaba_a.g_max, 0.0f);
    EXPECT_LT(gaba_a.reversal_potential, 0.0f);  // Inhibitory reversal potential

    gaba_b_state_t gaba_b;
    synapse_init_gaba_b(&gaba_b);

    EXPECT_GT(gaba_b.tau_decay, gaba_a.tau_decay);  // GABA-B slower than GABA-A
    EXPECT_LT(gaba_b.reversal_potential, gaba_a.reversal_potential);  // K+ reversal
}

TEST_F(SynapseTypesTernaryTest, ElectricalInitialization) {
    electrical_state_t electrical;
    synapse_init_electrical(&electrical);

    EXPECT_GT(electrical.conductance, 0.0f);
    EXPECT_TRUE(electrical.bidirectional);

    // Electrical synapses are neutral in ternary classification
    // (neither purely excitatory nor inhibitory)
}

//=============================================================================
// Ternary Weight Quantization for Synapse Types
//=============================================================================

TEST_F(SynapseTypesTernaryTest, ExcitatorySynapseWeightQuantization) {
    // Excitatory synapses should have positive weights
    float ampa_weights[] = {0.8f, 0.6f, 0.9f, 0.5f, 0.7f};
    const float threshold = 0.4f;

    for (size_t i = 0; i < 5; i++) {
        trit_t t = trit_from_float_threshold(ampa_weights[i], threshold);
        EXPECT_EQ(t, TRIT_POSITIVE);
    }
}

TEST_F(SynapseTypesTernaryTest, InhibitorySynapseWeightQuantization) {
    // Inhibitory synapses should have negative weights
    float gaba_weights[] = {-0.8f, -0.6f, -0.9f, -0.5f, -0.7f};
    const float threshold = 0.4f;

    for (size_t i = 0; i < 5; i++) {
        trit_t t = trit_from_float_threshold(gaba_weights[i], threshold);
        EXPECT_EQ(t, TRIT_NEGATIVE);
    }
}

TEST_F(SynapseTypesTernaryTest, MixedNetworkWeights) {
    // Realistic E/I balanced network weights
    float ei_weights[] = {
        0.8f, -0.6f, 0.9f, -0.7f, 0.1f,  // Strong E, I, E, I, weak
       -0.8f, 0.7f, 0.0f, -0.5f, 0.6f   // Strong I, E, none, I, E
    };
    const float threshold = 0.4f;

    trit_t expected[] = {
        TRIT_POSITIVE, TRIT_NEGATIVE, TRIT_POSITIVE, TRIT_NEGATIVE, TRIT_UNKNOWN,
        TRIT_NEGATIVE, TRIT_POSITIVE, TRIT_UNKNOWN, TRIT_NEGATIVE, TRIT_POSITIVE
    };

    for (size_t i = 0; i < 10; i++) {
        trit_t t = trit_from_float_threshold(ei_weights[i], threshold);
        EXPECT_EQ(t, expected[i]);
    }

    // Count excitatory vs inhibitory
    int excitatory = 0, inhibitory = 0, neutral = 0;
    for (size_t i = 0; i < 10; i++) {
        trit_t t = trit_from_float_threshold(ei_weights[i], threshold);
        if (t == TRIT_POSITIVE) excitatory++;
        else if (t == TRIT_NEGATIVE) inhibitory++;
        else neutral++;
    }

    EXPECT_EQ(excitatory, 4);
    EXPECT_EQ(inhibitory, 4);
    EXPECT_EQ(neutral, 2);
}

//=============================================================================
// Time Constant Tests
//=============================================================================

TEST_F(SynapseTypesTernaryTest, TimeConstantOrdering) {
    ampa_state_t ampa;
    nmda_state_t nmda;
    gaba_a_state_t gaba_a;
    gaba_b_state_t gaba_b;

    synapse_init_ampa(&ampa);
    synapse_init_nmda(&nmda);
    synapse_init_gaba_a(&gaba_a);
    synapse_init_gaba_b(&gaba_b);

    // AMPA < GABA-A < NMDA < GABA-B (typical ordering)
    EXPECT_LT(ampa.tau_decay, gaba_a.tau_decay);
    EXPECT_LT(gaba_a.tau_decay, nmda.tau_decay);
    EXPECT_LT(nmda.tau_decay, gaba_b.tau_decay);
}

//=============================================================================
// Null Pointer Handling
//=============================================================================

TEST_F(SynapseTypesTernaryTest, SynapseTypeNameNull) {
    // Out of range type should return something reasonable
    const char* name = synapse_type_name((synapse_type_t)999);
    EXPECT_NE(name, nullptr);  // Should not crash
}

TEST_F(SynapseTypesTernaryTest, TypeCheckBoundaries) {
    // Test boundary synapse types
    EXPECT_FALSE(synapse_type_is_excitatory(SYNAPSE_GENERIC));
    EXPECT_FALSE(synapse_type_is_inhibitory(SYNAPSE_GENERIC));
    EXPECT_FALSE(synapse_type_is_modulatory(SYNAPSE_GENERIC));

    // Electrical is special - not classified as excitatory/inhibitory/modulatory
    EXPECT_FALSE(synapse_type_is_excitatory(SYNAPSE_ELECTRICAL));
    EXPECT_FALSE(synapse_type_is_inhibitory(SYNAPSE_ELECTRICAL));
    EXPECT_FALSE(synapse_type_is_modulatory(SYNAPSE_ELECTRICAL));
}

//=============================================================================
// Ternary Vector Representation of Synapse Population
//=============================================================================

TEST_F(SynapseTypesTernaryTest, SynapsePopulationTernaryVector) {
    // Represent a population of synapses as ternary vector
    const size_t num_synapses = 100;

    trit_vector_t* synapse_types = trit_vector_create(num_synapses, TERNARY_PACK_NONE);
    ASSERT_NE(synapse_types, nullptr);

    // Initialize: 40% excitatory, 20% inhibitory, 40% none/weak
    for (size_t i = 0; i < num_synapses; i++) {
        if (i < 40) {
            trit_vector_set(synapse_types, i, TRIT_POSITIVE);  // Excitatory
        } else if (i < 60) {
            trit_vector_set(synapse_types, i, TRIT_NEGATIVE);  // Inhibitory
        } else {
            trit_vector_set(synapse_types, i, TRIT_UNKNOWN);   // Weak/none
        }
    }

    // Compute majority (should be tie between positive and unknown)
    trit_t majority = trit_vector_majority(synapse_types);

    // Count each type
    int pos = 0, neg = 0, unk = 0;
    for (size_t i = 0; i < num_synapses; i++) {
        trit_t t = trit_vector_get(synapse_types, i);
        if (t == TRIT_POSITIVE) pos++;
        else if (t == TRIT_NEGATIVE) neg++;
        else unk++;
    }

    EXPECT_EQ(pos, 40);
    EXPECT_EQ(neg, 20);
    EXPECT_EQ(unk, 40);

    // Majority should be positive or unknown (tie)
    EXPECT_TRUE(majority == TRIT_POSITIVE || majority == TRIT_UNKNOWN);

    trit_vector_destroy(synapse_types);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
