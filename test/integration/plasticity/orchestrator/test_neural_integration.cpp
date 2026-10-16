/**
 * @file test_neural_integration.cpp
 * @brief Integration tests for neural-plasticity bridges and coordinator
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Tests integration between axon/neuron/dendrite bridges and plasticity
 * WHY:  Ensure complete spike-to-plasticity pipeline works correctly
 * HOW:  Test multi-bridge coordination, spike cascade, bidirectional sync
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "plasticity/orchestrator/nimcp_neural_plasticity_coordinator.h"
#include "plasticity/orchestrator/nimcp_axon_orchestrator_bridge.h"
#include "plasticity/orchestrator/nimcp_neuron_orchestrator_bridge.h"
#include "plasticity/orchestrator/nimcp_dendrite_orchestrator_bridge.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"

// ============================================================================
// Test Fixtures
// ============================================================================

/**
 * @brief Fixture for testing axon-orchestrator integration
 */
class AxonOrchestratorIntegrationTest : public ::testing::Test {
protected:
    axon_orchestrator_bridge_t* bridge = nullptr;
    plasticity_orchestrator_t* orchestrator = nullptr;

    void SetUp() override {
        plasticity_orchestrator_config_t orch_config;
        plasticity_orchestrator_default_config(&orch_config);
        orchestrator = plasticity_orchestrator_create(&orch_config);
        ASSERT_NE(orchestrator, nullptr);

        axon_orchestrator_config_t config;
        axon_orchestrator_default_config(&config);
        bridge = axon_orchestrator_bridge_create(&config, orchestrator, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) axon_orchestrator_bridge_destroy(bridge);
        if (orchestrator) plasticity_orchestrator_destroy(orchestrator);
    }
};

/**
 * @brief Fixture for testing neuron-orchestrator integration
 */
class NeuronOrchestratorIntegrationTest : public ::testing::Test {
protected:
    neuron_orchestrator_bridge_t* bridge = nullptr;
    plasticity_orchestrator_t* orchestrator = nullptr;

    void SetUp() override {
        plasticity_orchestrator_config_t orch_config;
        plasticity_orchestrator_default_config(&orch_config);
        orchestrator = plasticity_orchestrator_create(&orch_config);
        ASSERT_NE(orchestrator, nullptr);

        neuron_orchestrator_config_t config;
        neuron_orchestrator_default_config(&config);
        bridge = neuron_orchestrator_bridge_create(&config, orchestrator, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) neuron_orchestrator_bridge_destroy(bridge);
        if (orchestrator) plasticity_orchestrator_destroy(orchestrator);
    }
};

/**
 * @brief Fixture for testing dendrite-orchestrator integration
 */
class DendriteOrchestratorIntegrationTest : public ::testing::Test {
protected:
    dendrite_orchestrator_bridge_t* bridge = nullptr;
    plasticity_orchestrator_t* orchestrator = nullptr;

    void SetUp() override {
        plasticity_orchestrator_config_t orch_config;
        plasticity_orchestrator_default_config(&orch_config);
        orchestrator = plasticity_orchestrator_create(&orch_config);
        ASSERT_NE(orchestrator, nullptr);

        dendrite_orchestrator_config_t config;
        dendrite_orchestrator_default_config(&config);
        bridge = dendrite_orchestrator_bridge_create(&config, orchestrator, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) dendrite_orchestrator_bridge_destroy(bridge);
        if (orchestrator) plasticity_orchestrator_destroy(orchestrator);
    }
};

/**
 * @brief Fixture for testing full neural-plasticity coordinator integration
 */
class NeuralPlasticityIntegrationTest : public ::testing::Test {
protected:
    neural_plasticity_coordinator_t* coordinator = nullptr;

    void SetUp() override {
        neural_plasticity_config_t config;
        neural_plasticity_default_config(&config);
        coordinator = neural_plasticity_coordinator_create(&config, nullptr, nullptr);
        ASSERT_NE(coordinator, nullptr);
    }

    void TearDown() override {
        if (coordinator) neural_plasticity_coordinator_destroy(coordinator);
    }
};

// ============================================================================
// Axon-Orchestrator Integration Tests
// ============================================================================

TEST_F(AxonOrchestratorIntegrationTest, SynapseMappingWorks) {
    // Map synapses
    for (uint32_t i = 0; i < 10; i++) {
        EXPECT_EQ(axon_orchestrator_map_synapse(bridge, i, i % 5), 0);
    }

    EXPECT_EQ(axon_orchestrator_get_mapping_count(bridge), 10u);
}

TEST_F(AxonOrchestratorIntegrationTest, MultipleAxonMappings) {
    // Create multiple synapse->axon mappings
    for (uint32_t i = 0; i < 100; i++) {
        axon_orchestrator_map_synapse(bridge, i, i % 10);
    }

    EXPECT_EQ(axon_orchestrator_get_mapping_count(bridge), 100u);
}

TEST_F(AxonOrchestratorIntegrationTest, BridgeUpdateTracked) {
    axon_orchestrator_map_synapse(bridge, 100, 1);

    // Run updates
    for (int t = 0; t < 100; t++) {
        axon_orchestrator_bridge_update(bridge, t * 100);
    }

    axon_orchestrator_stats_t stats;
    axon_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.update_calls, 100u);
}

TEST_F(AxonOrchestratorIntegrationTest, SynapseUnmapping) {
    // Map then unmap
    axon_orchestrator_map_synapse(bridge, 100, 1);
    EXPECT_EQ(axon_orchestrator_get_mapping_count(bridge), 1u);

    axon_orchestrator_unmap_synapse(bridge, 100);
    EXPECT_EQ(axon_orchestrator_get_mapping_count(bridge), 0u);
}

// ============================================================================
// Neuron-Orchestrator Integration Tests
// ============================================================================

TEST_F(NeuronOrchestratorIntegrationTest, NeuronRegistration) {
    for (uint32_t i = 0; i < 10; i++) {
        int result = neuron_orchestrator_register_neuron(bridge, i, nullptr, nullptr);
        EXPECT_EQ(result, 0);
    }

    EXPECT_EQ(neuron_orchestrator_get_neuron_count(bridge), 10u);
}

TEST_F(NeuronOrchestratorIntegrationTest, AxonDendriteAssociation) {
    neuron_orchestrator_register_neuron(bridge, 1, nullptr, nullptr);

    EXPECT_EQ(neuron_orchestrator_add_axon(bridge, 1, 10), 0);
    EXPECT_EQ(neuron_orchestrator_add_dendrite(bridge, 1, 20), 0);

    neuron_orchestrator_stats_t stats;
    neuron_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.neurons_registered, 1u);
}

TEST_F(NeuronOrchestratorIntegrationTest, FiringRateTracking) {
    neuron_orchestrator_register_neuron(bridge, 1, nullptr, nullptr);

    float rate = neuron_orchestrator_get_firing_rate(bridge, 1);
    EXPECT_GE(rate, 0.0f);
}

TEST_F(NeuronOrchestratorIntegrationTest, UnregisteredNeuron) {
    neuron_orchestrator_register_neuron(bridge, 1, nullptr, nullptr);
    EXPECT_EQ(neuron_orchestrator_get_neuron_count(bridge), 1u);

    neuron_orchestrator_unregister_neuron(bridge, 1);
    EXPECT_EQ(neuron_orchestrator_get_neuron_count(bridge), 0u);
}

TEST_F(NeuronOrchestratorIntegrationTest, MultipleAxonsPerNeuron) {
    neuron_orchestrator_register_neuron(bridge, 1, nullptr, nullptr);

    // Add multiple axons
    for (uint32_t i = 0; i < 5; i++) {
        EXPECT_EQ(neuron_orchestrator_add_axon(bridge, 1, i + 100), 0);
    }
}

// ============================================================================
// Dendrite-Orchestrator Integration Tests
// ============================================================================

TEST_F(DendriteOrchestratorIntegrationTest, SpineMappingWorks) {
    for (uint32_t i = 0; i < 10; i++) {
        EXPECT_EQ(dendrite_orchestrator_map_spine(bridge, i, i / 5, i % 5), 0);
    }

    EXPECT_EQ(dendrite_orchestrator_get_mapping_count(bridge), 10u);
}

TEST_F(DendriteOrchestratorIntegrationTest, SpineWeightSync) {
    dendrite_orchestrator_map_spine(bridge, 100, 1, 0);
    plasticity_orchestrator_set_weight(orchestrator, 100, 0.75f);

    EXPECT_EQ(dendrite_orchestrator_sync_weight_to_spine(bridge, 100), 0);

    dendrite_orchestrator_stats_t stats;
    dendrite_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.weight_to_spine_syncs, 1u);
}

TEST_F(DendriteOrchestratorIntegrationTest, SpineFormationElimination) {
    for (uint32_t i = 0; i < 10; i++) {
        dendrite_orchestrator_spine_formed(bridge, i / 5, i % 5, i, 0.3f);
    }

    EXPECT_EQ(dendrite_orchestrator_get_mapping_count(bridge), 10u);

    dendrite_orchestrator_spine_eliminated(bridge, 0);
    dendrite_orchestrator_spine_eliminated(bridge, 5);

    EXPECT_EQ(dendrite_orchestrator_get_mapping_count(bridge), 8u);

    dendrite_orchestrator_stats_t stats;
    dendrite_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.spines_registered, 10u);
    EXPECT_EQ(stats.spines_eliminated, 2u);
}

TEST_F(DendriteOrchestratorIntegrationTest, BidirectionalSync) {
    dendrite_orchestrator_map_spine(bridge, 100, 1, 0);
    plasticity_orchestrator_set_weight(orchestrator, 100, 0.5f);

    dendrite_orchestrator_sync_weight_to_spine(bridge, 100);
    dendrite_orchestrator_sync_spine_to_orchestrator(bridge, 100);

    dendrite_orchestrator_stats_t stats;
    dendrite_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.weight_to_spine_syncs, 1u);
    EXPECT_EQ(stats.spine_to_orchestrator_syncs, 1u);
}

TEST_F(DendriteOrchestratorIntegrationTest, PreSpikeForwarding) {
    for (uint32_t i = 0; i < 10; i++) {
        plasticity_orchestrator_set_weight(orchestrator, i, 0.5f);
    }

    for (uint32_t i = 0; i < 10; i++) {
        dendrite_orchestrator_pre_spike(bridge, i, 1000 + i * 10);
    }

    dendrite_orchestrator_stats_t stats;
    dendrite_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.pre_spikes_forwarded, 10u);
}

// ============================================================================
// Neural Plasticity Coordinator Integration Tests
// ============================================================================

TEST_F(NeuralPlasticityIntegrationTest, AllBridgesAccessible) {
    plasticity_orchestrator_t* orch = neural_plasticity_get_orchestrator(coordinator);
    EXPECT_NE(orch, nullptr);

    neuron_orchestrator_bridge_t* neuron_bridge = neural_plasticity_get_neuron_bridge(coordinator);
    EXPECT_NE(neuron_bridge, nullptr);
}

TEST_F(NeuralPlasticityIntegrationTest, SynapseRegistration) {
    EXPECT_EQ(neural_plasticity_register_synapse(coordinator, 100, 0, 1, 0, 0.5f), 0);

    float weight = neural_plasticity_get_weight(coordinator, 100);
    EXPECT_FLOAT_EQ(weight, 0.5f);
}

TEST_F(NeuralPlasticityIntegrationTest, WeightManipulation) {
    neural_plasticity_register_synapse(coordinator, 100, 0, 1, 0, 0.5f);

    EXPECT_EQ(neural_plasticity_set_weight(coordinator, 100, 0.75f), 0);

    float weight = neural_plasticity_get_weight(coordinator, 100);
    EXPECT_FLOAT_EQ(weight, 0.75f);
}

TEST_F(NeuralPlasticityIntegrationTest, RewardDelivery) {
    EXPECT_EQ(neural_plasticity_reward(coordinator, 1.0f, 1000000), 0);
}

TEST_F(NeuralPlasticityIntegrationTest, SimulationStep) {
    float inputs[10] = {0};
    int result = neural_plasticity_step(coordinator, 0.1f, inputs, 1000000);
    EXPECT_GE(result, 0);

    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.total_steps, 1u);
}

TEST_F(NeuralPlasticityIntegrationTest, StatisticsAggregated) {
    float inputs[5] = {0};
    for (int t = 0; t < 100; t++) {
        neural_plasticity_step(coordinator, 0.1f, inputs, t * 1000);
    }

    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.total_steps, 100u);
}

TEST_F(NeuralPlasticityIntegrationTest, StatsReset) {
    float inputs[5] = {0};
    for (int t = 0; t < 50; t++) {
        neural_plasticity_step(coordinator, 0.1f, inputs, t * 1000);
    }

    neural_plasticity_reset_stats(coordinator);

    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.total_steps, 0u);
}

// ============================================================================
// Cross-Bridge Integration Tests
// ============================================================================

class MultiBridgeIntegrationTest : public ::testing::Test {
protected:
    neural_plasticity_coordinator_t* coordinator = nullptr;

    void SetUp() override {
        neural_plasticity_config_t config;
        neural_plasticity_default_config(&config);
        coordinator = neural_plasticity_coordinator_create(&config, nullptr, nullptr);
        ASSERT_NE(coordinator, nullptr);
    }

    void TearDown() override {
        if (coordinator) neural_plasticity_coordinator_destroy(coordinator);
    }
};

TEST_F(MultiBridgeIntegrationTest, BridgesShareOrchestrator) {
    plasticity_orchestrator_t* orch = neural_plasticity_get_orchestrator(coordinator);
    neuron_orchestrator_bridge_t* neuron_bridge = neural_plasticity_get_neuron_bridge(coordinator);

    EXPECT_EQ(neuron_orchestrator_get_orchestrator(neuron_bridge), orch);
}

TEST_F(MultiBridgeIntegrationTest, CoordinatedWeightChanges) {
    neural_plasticity_register_synapse(coordinator, 100, 0, 1, 0, 0.5f);

    plasticity_orchestrator_t* orch = neural_plasticity_get_orchestrator(coordinator);
    plasticity_orchestrator_set_weight(orch, 100, 0.75f);

    float weight = neural_plasticity_get_weight(coordinator, 100);
    EXPECT_FLOAT_EQ(weight, 0.75f);
}

TEST_F(MultiBridgeIntegrationTest, LongRunningIntegration) {
    for (uint32_t i = 0; i < 20; i++) {
        neural_plasticity_register_synapse(coordinator, i, i / 4, (i / 4 + 1) % 5, 0, 0.5f);
    }

    float inputs[10] = {0};
    for (int t = 0; t < 1000; t++) {
        neural_plasticity_step(coordinator, 0.1f, inputs, t * 1000);
    }

    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.total_steps, 1000u);
}
