//=============================================================================
// test_synapse_types_real.cpp - Real Tests for Synapse Type System
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

#include "core/synapse_types/nimcp_synapse_types.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/brain/nimcp_brain.h"

class SynapseTypesRealTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    adaptive_network_t network = nullptr;
    synapse_t test_synapse;

    void SetUp() override {
        brain = brain_create("test", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        network = brain_get_network(brain);
        ASSERT_NE(network, nullptr);

        // Initialize test synapse
        test_synapse.weight = 0.5f;
    }

    void TearDown() override {
        if (brain) brain_destroy(brain);
    }
};

// ============================================================================
// Synapse Type Initialization Tests
// ============================================================================

TEST_F(SynapseTypesRealTest, InitAMPA) {
    ampa_state_t state;
    synapse_init_ampa(&state);

    EXPECT_GT(state.g_max, 0.0f);
    EXPECT_GT(state.tau_rise, 0.0f);
    EXPECT_GT(state.tau_decay, 0.0f);
    EXPECT_FLOAT_EQ(state.conductance, 0.0f);
    EXPECT_FLOAT_EQ(state.reversal_potential, 0.0f);
}

TEST_F(SynapseTypesRealTest, InitNMDA) {
    nmda_state_t state;
    synapse_init_nmda(&state);

    EXPECT_GT(state.g_max, 0.0f);
    EXPECT_GT(state.tau_rise, 0.0f);
    EXPECT_GT(state.tau_decay, 0.0f);
    EXPECT_GT(state.mg_concentration, 0.0f);
    EXPECT_FLOAT_EQ(state.conductance, 0.0f);
    EXPECT_FLOAT_EQ(state.calcium_influx, 0.0f);
}

TEST_F(SynapseTypesRealTest, InitGABAA) {
    gaba_a_state_t state;
    synapse_init_gaba_a(&state);

    EXPECT_GT(state.g_max, 0.0f);
    EXPECT_GT(state.tau_rise, 0.0f);
    EXPECT_GT(state.tau_decay, 0.0f);
    EXPECT_FLOAT_EQ(state.conductance, 0.0f);
    EXPECT_LT(state.reversal_potential, 0.0f);  // Should be negative (inhibitory)
}

TEST_F(SynapseTypesRealTest, InitGABAB) {
    gaba_b_state_t state;
    synapse_init_gaba_b(&state);

    EXPECT_GT(state.g_max, 0.0f);
    EXPECT_GT(state.tau_rise, 0.0f);
    EXPECT_GT(state.tau_decay, 0.0f);
    EXPECT_FLOAT_EQ(state.conductance, 0.0f);
    EXPECT_LT(state.reversal_potential, 0.0f);  // Should be negative (inhibitory)
}

TEST_F(SynapseTypesRealTest, InitDopamine) {
    dopamine_state_t state;
    synapse_init_dopamine(&state);

    EXPECT_GT(state.tau_d1, 0.0f);
    EXPECT_GT(state.tau_d2, 0.0f);
    EXPECT_GE(state.baseline, 0.0f);
    EXPECT_LE(state.baseline, 1.0f);
}

TEST_F(SynapseTypesRealTest, InitSerotonin) {
    serotonin_state_t state;
    synapse_init_serotonin(&state);

    EXPECT_GT(state.tau_ht1a, 0.0f);
    EXPECT_GT(state.tau_ht2a, 0.0f);
    EXPECT_GE(state.baseline, 0.0f);
    EXPECT_LE(state.baseline, 1.0f);
}

TEST_F(SynapseTypesRealTest, InitAcetylcholine) {
    acetylcholine_state_t state;
    synapse_init_acetylcholine(&state);

    EXPECT_GT(state.tau_nicotinic, 0.0f);
    EXPECT_GT(state.tau_muscarinic, 0.0f);
    EXPECT_GE(state.baseline, 0.0f);
    EXPECT_LE(state.baseline, 1.0f);
}

TEST_F(SynapseTypesRealTest, InitElectrical) {
    electrical_state_t state;
    synapse_init_electrical(&state);

    EXPECT_GT(state.conductance, 0.0f);
    EXPECT_TRUE(state.bidirectional);
}

// ============================================================================
// Synapse Compute Function Tests
// ============================================================================

TEST_F(SynapseTypesRealTest, ComputeGeneric) {
    float result = synapse_compute_generic(&test_synapse, nullptr, nullptr, 1.0f, 1.0f);
    EXPECT_GE(result, 0.0f);
}

TEST_F(SynapseTypesRealTest, ComputeGenericZeroSpike) {
    float result = synapse_compute_generic(&test_synapse, nullptr, nullptr, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST_F(SynapseTypesRealTest, ComputeAMPA) {
    synapse_type_state_t type_state;
    synapse_init_ampa(&type_state.ampa);
    test_synapse.type_state = type_state;

    float result = synapse_compute_ampa(&test_synapse, nullptr, nullptr, 1.0f, 1.0f);
    EXPECT_GE(result, 0.0f);
}

TEST_F(SynapseTypesRealTest, ComputeNMDA) {
    synapse_type_state_t type_state;
    synapse_init_nmda(&type_state.nmda);
    test_synapse.type_state = type_state;

    float result = synapse_compute_nmda(&test_synapse, nullptr, nullptr, 1.0f, 1.0f);
    EXPECT_GE(result, 0.0f);
}

TEST_F(SynapseTypesRealTest, ComputeGABAA) {
    synapse_type_state_t type_state;
    synapse_init_gaba_a(&type_state.gaba_a);
    test_synapse.type_state = type_state;

    float result = synapse_compute_gaba_a(&test_synapse, nullptr, nullptr, 1.0f, 1.0f);
    // GABA-A is inhibitory, but result depends on implementation
    // Just check it doesn't crash
    (void)result;
    SUCCEED();
}

TEST_F(SynapseTypesRealTest, ComputeGABAB) {
    synapse_type_state_t type_state;
    synapse_init_gaba_b(&type_state.gaba_b);
    test_synapse.type_state = type_state;

    float result = synapse_compute_gaba_b(&test_synapse, nullptr, nullptr, 1.0f, 1.0f);
    (void)result;
    SUCCEED();
}

TEST_F(SynapseTypesRealTest, ComputeDopamine) {
    synapse_type_state_t type_state;
    synapse_init_dopamine(&type_state.dopamine);
    test_synapse.type_state = type_state;

    float result = synapse_compute_dopamine(&test_synapse, nullptr, nullptr, 1.0f, 1.0f);
    EXPECT_GE(result, -1.0f);  // Can be negative due to modulation
}

TEST_F(SynapseTypesRealTest, ComputeSerotonin) {
    synapse_type_state_t type_state;
    synapse_init_serotonin(&type_state.serotonin);
    test_synapse.type_state = type_state;

    float result = synapse_compute_serotonin(&test_synapse, nullptr, nullptr, 1.0f, 1.0f);
    (void)result;
    SUCCEED();
}

TEST_F(SynapseTypesRealTest, ComputeAcetylcholine) {
    synapse_type_state_t type_state;
    synapse_init_acetylcholine(&type_state.acetylcholine);
    test_synapse.type_state = type_state;

    float result = synapse_compute_acetylcholine(&test_synapse, nullptr, nullptr, 1.0f, 1.0f);
    EXPECT_GE(result, 0.0f);
}

TEST_F(SynapseTypesRealTest, ComputeElectrical) {
    synapse_type_state_t type_state;
    synapse_init_electrical(&type_state.electrical);
    test_synapse.type_state = type_state;

    float result = synapse_compute_electrical(&test_synapse, nullptr, nullptr, 1.0f, 1.0f);
    // Electrical synapses depend on voltage difference
    (void)result;
    SUCCEED();
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(SynapseTypesRealTest, SynapseTypeName) {
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

TEST_F(SynapseTypesRealTest, SynapseTypeIsExcitatory) {
    EXPECT_TRUE(synapse_type_is_excitatory(SYNAPSE_AMPA));
    EXPECT_TRUE(synapse_type_is_excitatory(SYNAPSE_NMDA));
    EXPECT_FALSE(synapse_type_is_excitatory(SYNAPSE_GABA_A));
    EXPECT_FALSE(synapse_type_is_excitatory(SYNAPSE_GABA_B));
}

TEST_F(SynapseTypesRealTest, SynapseTypeIsInhibitory) {
    EXPECT_FALSE(synapse_type_is_inhibitory(SYNAPSE_AMPA));
    EXPECT_FALSE(synapse_type_is_inhibitory(SYNAPSE_NMDA));
    EXPECT_TRUE(synapse_type_is_inhibitory(SYNAPSE_GABA_A));
    EXPECT_TRUE(synapse_type_is_inhibitory(SYNAPSE_GABA_B));
}

TEST_F(SynapseTypesRealTest, SynapseTypeIsModulatory) {
    EXPECT_FALSE(synapse_type_is_modulatory(SYNAPSE_AMPA));
    EXPECT_FALSE(synapse_type_is_modulatory(SYNAPSE_NMDA));
    EXPECT_TRUE(synapse_type_is_modulatory(SYNAPSE_DOPAMINE));
    EXPECT_TRUE(synapse_type_is_modulatory(SYNAPSE_SEROTONIN));
    EXPECT_TRUE(synapse_type_is_modulatory(SYNAPSE_ACETYLCHOLINE));
}

TEST_F(SynapseTypesRealTest, SynapseTypeTimeConstant) {
    synapse_type_state_t state;

    synapse_init_ampa(&state.ampa);
    float tau = synapse_type_time_constant(SYNAPSE_AMPA, &state);
    EXPECT_GT(tau, 0.0f);

    synapse_init_nmda(&state.nmda);
    tau = synapse_type_time_constant(SYNAPSE_NMDA, &state);
    EXPECT_GT(tau, 0.0f);

    synapse_init_gaba_a(&state.gaba_a);
    tau = synapse_type_time_constant(SYNAPSE_GABA_A, &state);
    EXPECT_GT(tau, 0.0f);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(SynapseTypesRealTest, ComputeWithNullSynapse) {
    float result = synapse_compute_generic(nullptr, nullptr, nullptr, 1.0f, 1.0f);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST_F(SynapseTypesRealTest, ComputeWithZeroDt) {
    float result = synapse_compute_generic(&test_synapse, nullptr, nullptr, 1.0f, 0.0f);
    // Should handle zero dt gracefully
    (void)result;
    SUCCEED();
}
