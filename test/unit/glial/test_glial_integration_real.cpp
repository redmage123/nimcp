/**
 * @file test_glial_integration_real.cpp
 * @brief Real tests for glial integration layer
 *
 * COVERAGE TARGET: glial_integration module (currently 0%)
 * APPROACH: Test all real functions with actual instances
 * FOCUS: Integration system creation, cell assignment, modulation queries, event notifications
 */

#include <gtest/gtest.h>

#include "glial/integration/nimcp_glial_integration.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
#include "glial/microglia/nimcp_microglia.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GlialIntegrationRealTest : public ::testing::Test {
protected:
    neural_network_t network = nullptr;
    glial_integration_t* gi = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        // NOTE: neural_network_create doesn't exist as a standalone function
        // neural_network_t only exists within brain context (brain_get_network)
        // but that returns adaptive_network_t, not neural_network_t
        // Tests focus on NULL handling and config validation
        network = nullptr;
    }

    void TearDown() override {
        if (gi) glial_integration_destroy(gi);
        // Network not created in SetUp

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0) << "Memory leak detected";
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST_F(GlialIntegrationRealTest, CreateDestroy) {
    gi = glial_integration_create(network, 100);

    ASSERT_NE(gi, nullptr);
    EXPECT_EQ(gi->network, network);
    EXPECT_EQ(gi->astrocyte_network, nullptr);
    EXPECT_EQ(gi->oligodendrocyte_network, nullptr);
    EXPECT_EQ(gi->microglia_network, nullptr);
}

TEST_F(GlialIntegrationRealTest, CreateWithLargeCapacity) {
    gi = glial_integration_create(network, 10000);

    ASSERT_NE(gi, nullptr);
    EXPECT_EQ(gi->network, network);
}

TEST_F(GlialIntegrationRealTest, DestroyNull) {
    // Should handle null gracefully
    glial_integration_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Network Assignment Tests
//=============================================================================

TEST_F(GlialIntegrationRealTest, SetAstrocyteNetwork) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    astrocyte_network_t* astro_net = astrocyte_network_create(10);
    ASSERT_NE(astro_net, nullptr);

    nimcp_result_t result = glial_integration_set_astrocyte_network(gi, astro_net);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(gi->astrocyte_network, astro_net);

    // Clean up
    astrocyte_network_destroy(astro_net);
}

TEST_F(GlialIntegrationRealTest, SetOligodendrocyteNetwork) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    oligodendrocyte_network_t* oligo_net = oligodendrocyte_network_create(10);
    ASSERT_NE(oligo_net, nullptr);

    nimcp_result_t result = glial_integration_set_oligodendrocyte_network(gi, oligo_net);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(gi->oligodendrocyte_network, oligo_net);

    // Clean up
    oligodendrocyte_network_destroy(oligo_net);
}

TEST_F(GlialIntegrationRealTest, SetMicrogliaNetwork) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    microglia_network_t* micro_net = microglia_network_create(10);
    ASSERT_NE(micro_net, nullptr);

    nimcp_result_t result = glial_integration_set_microglia_network(gi, micro_net);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(gi->microglia_network, micro_net);

    // Clean up
    microglia_network_destroy(micro_net);
}

//=============================================================================
// Cell Assignment Tests
//=============================================================================

TEST_F(GlialIntegrationRealTest, AssignAstrocyteToSynapse) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    astrocyte_network_t* astro_net = astrocyte_network_create(10);
    ASSERT_NE(astro_net, nullptr);
    glial_integration_set_astrocyte_network(gi, astro_net);

    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);
    ASSERT_NE(astro, nullptr);
    astrocyte_network_add(astro_net, astro);

    // Assign astrocyte to synapse (pre=1, post=2, synapse_id = 1*10000 + 2 = 10002)
    nimcp_result_t result = glial_integration_assign_astrocyte_to_synapse(gi, 0, 10002);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Clean up
    astrocyte_network_destroy(astro_net);
}

TEST_F(GlialIntegrationRealTest, AssignOligodendrocyteToNeuron) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    oligodendrocyte_network_t* oligo_net = oligodendrocyte_network_create(10);
    ASSERT_NE(oligo_net, nullptr);
    glial_integration_set_oligodendrocyte_network(gi, oligo_net);

    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);
    oligodendrocyte_network_add(oligo_net, oligo);

    nimcp_result_t result = glial_integration_assign_oligodendrocyte_to_neuron(gi, 0, 5);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Clean up
    oligodendrocyte_network_destroy(oligo_net);
}

TEST_F(GlialIntegrationRealTest, AssignMicrogliaToSynapse) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    microglia_network_t* micro_net = microglia_network_create(10);
    ASSERT_NE(micro_net, nullptr);
    glial_integration_set_microglia_network(gi, micro_net);

    microglia_t* micro = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(micro, nullptr);
    microglia_network_add(micro_net, micro);

    nimcp_result_t result = glial_integration_assign_microglia_to_synapse(gi, 0, 10003);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Clean up
    microglia_network_destroy(micro_net);
}

//=============================================================================
// Event Notification Tests
//=============================================================================

TEST_F(GlialIntegrationRealTest, OnSynapseFired) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    // Call with no glial cells assigned - should not crash
    glial_integration_on_synapse_fired(gi, 1, 2, 0.5f, 1000);
    SUCCEED();
}

TEST_F(GlialIntegrationRealTest, OnNeuronFired) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    // Call with no glial cells assigned - should not crash
    glial_integration_on_neuron_fired(gi, 5, 1000);
    SUCCEED();
}

TEST_F(GlialIntegrationRealTest, OnSynapseFired_WithAstrocyte) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    astrocyte_network_t* astro_net = astrocyte_network_create(10);
    ASSERT_NE(astro_net, nullptr);
    glial_integration_set_astrocyte_network(gi, astro_net);

    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);
    ASSERT_NE(astro, nullptr);
    astrocyte_network_add(astro_net, astro);

    glial_integration_assign_astrocyte_to_synapse(gi, 0, 10002);

    // Fire synapse - astrocyte should be notified
    glial_integration_on_synapse_fired(gi, 1, 2, 0.5f, 1000);
    SUCCEED();

    // Clean up
    astrocyte_network_destroy(astro_net);
}

//=============================================================================
// Modulation Query Tests
//=============================================================================

TEST_F(GlialIntegrationRealTest, GetSynapticModulation_NoAstrocyte) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    float modulation = glial_integration_get_synaptic_modulation(gi, 1, 2);

    // Should return 1.0 (no modulation) when no astrocyte assigned
    EXPECT_FLOAT_EQ(modulation, 1.0f);
}

TEST_F(GlialIntegrationRealTest, GetMyelinationFactor_NoOligodendrocyte) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    float myelination = glial_integration_get_myelination_factor(gi, 5);

    // Should return 0.0 when no oligodendrocyte assigned
    EXPECT_FLOAT_EQ(myelination, 0.0f);
}

TEST_F(GlialIntegrationRealTest, ShouldPruneSynapse_NoMicroglia) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    bool should_prune = glial_integration_should_prune_synapse(gi, 1, 2);

    // Should return false when no microglia assigned
    EXPECT_FALSE(should_prune);
}

//=============================================================================
// Simulation Step Tests
//=============================================================================

TEST_F(GlialIntegrationRealTest, Step_EmptySystem) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    // Should not crash with no glial cells
    glial_integration_step(gi, 1000);
    SUCCEED();
}

TEST_F(GlialIntegrationRealTest, Step_WithGlialNetworks) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    astrocyte_network_t* astro_net = astrocyte_network_create(10);
    oligodendrocyte_network_t* oligo_net = oligodendrocyte_network_create(10);
    microglia_network_t* micro_net = microglia_network_create(10);

    glial_integration_set_astrocyte_network(gi, astro_net);
    glial_integration_set_oligodendrocyte_network(gi, oligo_net);
    glial_integration_set_microglia_network(gi, micro_net);

    glial_integration_step(gi, 1000);
    glial_integration_step(gi, 2000);
    glial_integration_step(gi, 3000);

    SUCCEED();

    // Clean up
    astrocyte_network_destroy(astro_net);
    oligodendrocyte_network_destroy(oligo_net);
    microglia_network_destroy(micro_net);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(GlialIntegrationRealTest, GetStats_EmptySystem) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    glial_integration_stats_t stats;
    nimcp_result_t result = glial_integration_get_stats(gi, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.num_astrocytes, 0);
    EXPECT_EQ(stats.num_oligodendrocytes, 0);
    EXPECT_EQ(stats.num_microglia, 0);
}

TEST_F(GlialIntegrationRealTest, GetAstrocyteSynapseCount) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    uint32_t count = glial_integration_get_astrocyte_synapse_count(gi, 0);

    // Should return 0 when no astrocytes
    EXPECT_EQ(count, 0);
}

TEST_F(GlialIntegrationRealTest, GetOligodendrocyteNeuronCount) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    uint32_t count = glial_integration_get_oligodendrocyte_neuron_count(gi, 0);

    // Should return 0 when no oligodendrocytes
    EXPECT_EQ(count, 0);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(GlialIntegrationRealTest, SetAstrocyteModulationEnabled) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    // Test enabling and disabling
    glial_integration_set_astrocyte_modulation_enabled(gi, true);
    EXPECT_TRUE(gi->enable_astrocyte_modulation);

    glial_integration_set_astrocyte_modulation_enabled(gi, false);
    EXPECT_FALSE(gi->enable_astrocyte_modulation);
}

TEST_F(GlialIntegrationRealTest, SetOligodendrocyteMyelinationEnabled) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    glial_integration_set_oligodendrocyte_myelination_enabled(gi, true);
    EXPECT_TRUE(gi->enable_oligodendrocyte_myelination);

    glial_integration_set_oligodendrocyte_myelination_enabled(gi, false);
    EXPECT_FALSE(gi->enable_oligodendrocyte_myelination);
}

TEST_F(GlialIntegrationRealTest, SetMicrogliaPruningEnabled) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    glial_integration_set_microglia_pruning_enabled(gi, true);
    EXPECT_TRUE(gi->enable_microglia_pruning);

    glial_integration_set_microglia_pruning_enabled(gi, false);
    EXPECT_FALSE(gi->enable_microglia_pruning);
}

//=============================================================================
// Integration Tests (Multiple Glial Types)
//=============================================================================

TEST_F(GlialIntegrationRealTest, FullIntegration_AllGlialTypes) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    // Create all glial networks
    astrocyte_network_t* astro_net = astrocyte_network_create(5);
    oligodendrocyte_network_t* oligo_net = oligodendrocyte_network_create(5);
    microglia_network_t* micro_net = microglia_network_create(5);

    // Assign networks
    glial_integration_set_astrocyte_network(gi, astro_net);
    glial_integration_set_oligodendrocyte_network(gi, oligo_net);
    glial_integration_set_microglia_network(gi, micro_net);

    // Create glial cells
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    microglia_t* micro = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);

    astrocyte_network_add(astro_net, astro);
    oligodendrocyte_network_add(oligo_net, oligo);
    microglia_network_add(micro_net, micro);

    // Assign cells
    glial_integration_assign_astrocyte_to_synapse(gi, 0, 10002);
    glial_integration_assign_oligodendrocyte_to_neuron(gi, 0, 5);
    glial_integration_assign_microglia_to_synapse(gi, 0, 10003);

    // Simulate events
    glial_integration_on_synapse_fired(gi, 1, 2, 0.5f, 1000);
    glial_integration_on_neuron_fired(gi, 5, 1000);

    // Step simulation
    glial_integration_step(gi, 1000);

    // Query modulation
    float modulation = glial_integration_get_synaptic_modulation(gi, 1, 2);
    float myelination = glial_integration_get_myelination_factor(gi, 5);

    EXPECT_GT(modulation, 0.0f);
    EXPECT_GE(myelination, 0.0f);

    // Clean up
    astrocyte_network_destroy(astro_net);
    oligodendrocyte_network_destroy(oligo_net);
    microglia_network_destroy(micro_net);
}

TEST_F(GlialIntegrationRealTest, MultipleAssignments_SameAstrocyte) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    astrocyte_network_t* astro_net = astrocyte_network_create(10);
    glial_integration_set_astrocyte_network(gi, astro_net);

    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);
    astrocyte_network_add(astro_net, astro);

    // Assign same astrocyte to multiple synapses
    glial_integration_assign_astrocyte_to_synapse(gi, 0, 10002);
    glial_integration_assign_astrocyte_to_synapse(gi, 0, 10003);
    glial_integration_assign_astrocyte_to_synapse(gi, 0, 10004);

    SUCCEED();

    // Clean up
    astrocyte_network_destroy(astro_net);
}
