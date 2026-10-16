/**
 * @file test_neural_plasticity_coordinator.cpp
 * @brief Unit Tests for Neural Plasticity Coordinator
 *
 * Tests all aspects of the neural plasticity coordinator including:
 * - Lifecycle management (create/destroy)
 * - Configuration
 * - Neuron and synapse registration
 * - Simulation step
 * - Reward processing
 * - Weight queries
 * - Integration with immune/UMM/bio-async
 * - Statistics
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <atomic>

// Headers have their own extern "C" guards
#include "plasticity/orchestrator/nimcp_neural_plasticity_coordinator.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "core/axon/nimcp_axon.h"
#include "core/dendrite/nimcp_dendrite.h"
#include "core/neuron_models/nimcp_neuron_model.h"
#include "core/neuron_models/nimcp_izhikevich.h"
#include "utils/memory/nimcp_memory.h"

// ============================================================================
// Test Fixture
// ============================================================================

class NeuralPlasticityCoordinatorTest : public ::testing::Test {
protected:
    neural_plasticity_coordinator_t* coordinator = nullptr;
    axon_network_t* axon_network = nullptr;
    dendrite_network_t* dendrite_network = nullptr;
    neural_plasticity_config_t config;

    void SetUp() override {
        neural_plasticity_default_config(&config);

        // Create axon network with capacity
        axon_network = axon_network_create(100);

        // Create dendrite network with capacity
        dendrite_network = dendrite_network_create(100);
    }

    void TearDown() override {
        if (coordinator) {
            neural_plasticity_coordinator_destroy(coordinator);
            coordinator = nullptr;
        }
        if (axon_network) {
            axon_network_destroy(axon_network);
            axon_network = nullptr;
        }
        if (dendrite_network) {
            dendrite_network_destroy(dendrite_network);
            dendrite_network = nullptr;
        }
    }

    // Helper to create coordinator
    void CreateCoordinator() {
        coordinator = neural_plasticity_coordinator_create(&config, axon_network, dendrite_network);
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(NeuralPlasticityCoordinatorTest, DefaultConfigSetsReasonableDefaults) {
    EXPECT_EQ(neural_plasticity_default_config(&config), 0);

    // Check defaults
    EXPECT_GT(config.default_dt_ms, 0.0f);
    EXPECT_GT(config.orchestrator_interval_ms, 0.0f);
    EXPECT_GT(config.sync_interval_ms, 0.0f);
    /* These features are now enabled by default */
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.enable_immune_integration);
    EXPECT_TRUE(config.enable_umm);
}

TEST_F(NeuralPlasticityCoordinatorTest, DefaultConfigReturnsErrorForNull) {
    EXPECT_EQ(neural_plasticity_default_config(nullptr), -1);
}

TEST_F(NeuralPlasticityCoordinatorTest, CreateWithDefaultConfig) {
    coordinator = neural_plasticity_coordinator_create(nullptr, axon_network, dendrite_network);
    ASSERT_NE(coordinator, nullptr);
}

TEST_F(NeuralPlasticityCoordinatorTest, CreateWithCustomConfig) {
    config.default_dt_ms = 0.5f;
    config.orchestrator_interval_ms = 5.0f;
    config.sync_interval_ms = 50.0f;

    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);
}

TEST_F(NeuralPlasticityCoordinatorTest, CreateWithNullNetworks) {
    // Both networks optional (for minimal setup)
    coordinator = neural_plasticity_coordinator_create(&config, nullptr, nullptr);
    // May or may not succeed depending on implementation
}

TEST_F(NeuralPlasticityCoordinatorTest, CreateWithOnlyAxonNetwork) {
    coordinator = neural_plasticity_coordinator_create(&config, axon_network, nullptr);
    // Should at least not crash
}

TEST_F(NeuralPlasticityCoordinatorTest, CreateWithOnlyDendriteNetwork) {
    coordinator = neural_plasticity_coordinator_create(&config, nullptr, dendrite_network);
    // Should at least not crash
}

TEST_F(NeuralPlasticityCoordinatorTest, DestroyNullIsSafe) {
    neural_plasticity_coordinator_destroy(nullptr);
    // Should not crash
}

// ============================================================================
// Neuron Registration Tests
// ============================================================================

TEST_F(NeuralPlasticityCoordinatorTest, RegisterNeuron) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    ASSERT_NE(vtable, nullptr);

    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(vtable, &params);

    EXPECT_EQ(neural_plasticity_register_neuron(coordinator, 1, state, vtable), 0);
}

TEST_F(NeuralPlasticityCoordinatorTest, RegisterMultipleNeurons) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);

    for (uint32_t i = 0; i < 50; i++) {
        neuron_model_state_t state = NULL;
        state = neuron_model_create(vtable, &params);
        EXPECT_EQ(neural_plasticity_register_neuron(coordinator, i, state, vtable), 0);
    }
}

TEST_F(NeuralPlasticityCoordinatorTest, RegisterNeuronWithNullCoordinatorReturnsError) {
    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    neuron_model_state_t state = NULL;

    EXPECT_EQ(neural_plasticity_register_neuron(nullptr, 1, state, vtable), -1);
}

TEST_F(NeuralPlasticityCoordinatorTest, AddAxonToNeuron) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(vtable, &params);
    neural_plasticity_register_neuron(coordinator, 1, state, vtable);

    EXPECT_EQ(neural_plasticity_add_neuron_axon(coordinator, 1, 10), 0);
}

TEST_F(NeuralPlasticityCoordinatorTest, AddDendriteToNeuron) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(vtable, &params);
    neural_plasticity_register_neuron(coordinator, 1, state, vtable);

    EXPECT_EQ(neural_plasticity_add_neuron_dendrite(coordinator, 1, 20), 0);
}

// ============================================================================
// Synapse Registration Tests
// ============================================================================

TEST_F(NeuralPlasticityCoordinatorTest, RegisterSynapse) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    EXPECT_EQ(neural_plasticity_register_synapse(
        coordinator, 100, 1, 1, 0, 0.5f), 0);
}

TEST_F(NeuralPlasticityCoordinatorTest, RegisterMultipleSynapses) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    for (uint32_t i = 0; i < 100; i++) {
        EXPECT_EQ(neural_plasticity_register_synapse(
            coordinator, i, i % 10, i / 10, i % 5, 0.3f + 0.01f * i), 0);
    }
}

TEST_F(NeuralPlasticityCoordinatorTest, UnregisterSynapse) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    neural_plasticity_register_synapse(coordinator, 100, 1, 1, 0, 0.5f);
    EXPECT_EQ(neural_plasticity_unregister_synapse(coordinator, 100), 0);
}

TEST_F(NeuralPlasticityCoordinatorTest, UnregisterNonExistentSynapseSucceeds) {
    /* Unregistering a non-existent synapse is a no-op - returns success */
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    EXPECT_EQ(neural_plasticity_unregister_synapse(coordinator, 99999), 0);
}

// ============================================================================
// Simulation Step Tests
// ============================================================================

TEST_F(NeuralPlasticityCoordinatorTest, StepWithNoNeurons) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    int result = neural_plasticity_step(coordinator, 0.1f, nullptr, 1000);
    EXPECT_EQ(result, 0);  // 0 spikes
}

TEST_F(NeuralPlasticityCoordinatorTest, StepWithNeurons) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);

    for (uint32_t i = 0; i < 10; i++) {
        neuron_model_state_t state = NULL;
        state = neuron_model_create(vtable, &params);
        neural_plasticity_register_neuron(coordinator, i, state, vtable);
    }

    float inputs[10] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    int result = neural_plasticity_step(coordinator, 0.1f, inputs, 1000);
    EXPECT_GE(result, 0);
}

TEST_F(NeuralPlasticityCoordinatorTest, StepWithDefaultDt) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    // dt = 0 uses default
    int result = neural_plasticity_step(coordinator, 0.0f, nullptr, 1000);
    EXPECT_GE(result, 0);
}

TEST_F(NeuralPlasticityCoordinatorTest, StepWithNullCoordinatorReturnsError) {
    EXPECT_EQ(neural_plasticity_step(nullptr, 0.1f, nullptr, 1000), -1);
}

TEST_F(NeuralPlasticityCoordinatorTest, MultipleSteps) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);

    for (uint32_t i = 0; i < 5; i++) {
        neuron_model_state_t state = NULL;
        state = neuron_model_create(vtable, &params);
        neural_plasticity_register_neuron(coordinator, i, state, vtable);
    }

    float inputs[5] = {10.0f, 10.0f, 10.0f, 10.0f, 10.0f};

    int total_spikes = 0;
    for (int t = 0; t < 1000; t++) {
        int result = neural_plasticity_step(coordinator, 0.1f, inputs, t * 100);
        if (result > 0) total_spikes += result;
    }

    // Should have generated some spikes with constant input
    EXPECT_GT(total_spikes, 0);
}

// ============================================================================
// Reward Processing Tests
// ============================================================================

TEST_F(NeuralPlasticityCoordinatorTest, ProcessReward) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    EXPECT_EQ(neural_plasticity_reward(coordinator, 1.0f, 1000), 0);
}

TEST_F(NeuralPlasticityCoordinatorTest, ProcessNegativeReward) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    EXPECT_EQ(neural_plasticity_reward(coordinator, -0.5f, 1000), 0);
}

TEST_F(NeuralPlasticityCoordinatorTest, RewardWithNullCoordinatorReturnsError) {
    EXPECT_EQ(neural_plasticity_reward(nullptr, 1.0f, 1000), -1);
}

// ============================================================================
// Weight Query Tests
// ============================================================================

TEST_F(NeuralPlasticityCoordinatorTest, GetWeight) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    neural_plasticity_register_synapse(coordinator, 100, 1, 1, 0, 0.75f);

    float weight = neural_plasticity_get_weight(coordinator, 100);
    EXPECT_FLOAT_EQ(weight, 0.75f);
}

TEST_F(NeuralPlasticityCoordinatorTest, SetWeight) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    neural_plasticity_register_synapse(coordinator, 100, 1, 1, 0, 0.5f);
    EXPECT_EQ(neural_plasticity_set_weight(coordinator, 100, 0.9f), 0);

    float weight = neural_plasticity_get_weight(coordinator, 100);
    EXPECT_FLOAT_EQ(weight, 0.9f);
}

TEST_F(NeuralPlasticityCoordinatorTest, GetWeightForUnknownSynapseReturnsNan) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    float weight = neural_plasticity_get_weight(coordinator, 99999);
    EXPECT_TRUE(std::isnan(weight));
}

TEST_F(NeuralPlasticityCoordinatorTest, SetWeightWithNullCoordinatorReturnsError) {
    EXPECT_EQ(neural_plasticity_set_weight(nullptr, 100, 0.5f), -1);
}

// ============================================================================
// Firing Rate Query Tests
// ============================================================================

TEST_F(NeuralPlasticityCoordinatorTest, GetFiringRate) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(vtable, &params);
    neural_plasticity_register_neuron(coordinator, 1, state, vtable);

    float rate = neural_plasticity_get_firing_rate(coordinator, 1);
    // Initially should be 0 or near 0
    EXPECT_GE(rate, 0.0f);
}

TEST_F(NeuralPlasticityCoordinatorTest, GetFiringRateForUnregisteredNeuronReturnsNegative) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    float rate = neural_plasticity_get_firing_rate(coordinator, 99999);
    EXPECT_LT(rate, 0.0f);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(NeuralPlasticityCoordinatorTest, GetStatsInitiallyZero) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    neural_plasticity_stats_t stats;
    EXPECT_EQ(neural_plasticity_get_stats(coordinator, &stats), 0);

    EXPECT_EQ(stats.total_spikes, 0u);
    EXPECT_EQ(stats.total_neuron_steps, 0u);
    EXPECT_EQ(stats.neurons_registered, 0u);
    EXPECT_EQ(stats.axon_spikes_initiated, 0u);
    EXPECT_EQ(stats.axon_spikes_arrived, 0u);
    EXPECT_EQ(stats.pre_spikes_forwarded, 0u);
    EXPECT_EQ(stats.total_steps, 0u);
}

TEST_F(NeuralPlasticityCoordinatorTest, GetStatsWithNullCoordinatorReturnsError) {
    neural_plasticity_stats_t stats;
    EXPECT_EQ(neural_plasticity_get_stats(nullptr, &stats), -1);
}

TEST_F(NeuralPlasticityCoordinatorTest, GetStatsWithNullStatsReturnsError) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    EXPECT_EQ(neural_plasticity_get_stats(coordinator, nullptr), -1);
}

TEST_F(NeuralPlasticityCoordinatorTest, ResetStats) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(vtable, &params);
    neural_plasticity_register_neuron(coordinator, 1, state, vtable);

    // Create activity
    float input = 20.0f;
    for (int i = 0; i < 100; i++) {
        neural_plasticity_step(coordinator, 0.1f, &input, i * 100);
    }

    // Reset and verify
    EXPECT_EQ(neural_plasticity_reset_stats(coordinator), 0);

    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.total_spikes, 0u);
    EXPECT_EQ(stats.total_steps, 0u);
}

TEST_F(NeuralPlasticityCoordinatorTest, StatsTrackNeuronRegistration) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);

    for (uint32_t i = 0; i < 10; i++) {
        neuron_model_state_t state = NULL;
        state = neuron_model_create(vtable, &params);
        neural_plasticity_register_neuron(coordinator, i, state, vtable);
    }

    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.neurons_registered, 10u);
}

TEST_F(NeuralPlasticityCoordinatorTest, StatsTrackSteps) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    for (int i = 0; i < 50; i++) {
        neural_plasticity_step(coordinator, 0.1f, nullptr, i * 100);
    }

    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.total_steps, 50u);
}

// ============================================================================
// Accessor Tests
// ============================================================================

TEST_F(NeuralPlasticityCoordinatorTest, GetOrchestrator) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    plasticity_orchestrator_t* orch = neural_plasticity_get_orchestrator(coordinator);
    ASSERT_NE(orch, nullptr);
}

TEST_F(NeuralPlasticityCoordinatorTest, GetAxonBridge) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    axon_orchestrator_bridge_t* bridge = neural_plasticity_get_axon_bridge(coordinator);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(NeuralPlasticityCoordinatorTest, GetNeuronBridge) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    neuron_orchestrator_bridge_t* bridge = neural_plasticity_get_neuron_bridge(coordinator);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(NeuralPlasticityCoordinatorTest, GetDendriteBridge) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    dendrite_orchestrator_bridge_t* bridge = neural_plasticity_get_dendrite_bridge(coordinator);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(NeuralPlasticityCoordinatorTest, GetAxonNetwork) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    axon_network_t* net = neural_plasticity_get_axon_network(coordinator);
    EXPECT_EQ(net, axon_network);
}

TEST_F(NeuralPlasticityCoordinatorTest, GetDendriteNetwork) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    dendrite_network_t* net = neural_plasticity_get_dendrite_network(coordinator);
    EXPECT_EQ(net, dendrite_network);
}

TEST_F(NeuralPlasticityCoordinatorTest, AccessorsWithNullReturnNull) {
    EXPECT_EQ(neural_plasticity_get_orchestrator(nullptr), nullptr);
    EXPECT_EQ(neural_plasticity_get_axon_bridge(nullptr), nullptr);
    EXPECT_EQ(neural_plasticity_get_neuron_bridge(nullptr), nullptr);
    EXPECT_EQ(neural_plasticity_get_dendrite_bridge(nullptr), nullptr);
    EXPECT_EQ(neural_plasticity_get_axon_network(nullptr), nullptr);
    EXPECT_EQ(neural_plasticity_get_dendrite_network(nullptr), nullptr);
}

// ============================================================================
// Bio-Async Integration Tests
// ============================================================================

TEST_F(NeuralPlasticityCoordinatorTest, ConnectBioAsync) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    int result = neural_plasticity_connect_bio_async(coordinator);
    // May succeed or fail depending on router availability
    (void)result;
}

TEST_F(NeuralPlasticityCoordinatorTest, ConnectBioAsyncWithNullReturnsError) {
    EXPECT_EQ(neural_plasticity_connect_bio_async(nullptr), -1);
}

// ============================================================================
// UMM Integration Tests
// ============================================================================

TEST_F(NeuralPlasticityCoordinatorTest, ConnectUmm) {
    config.enable_umm = true;
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    // UMM connection is optional
    int result = neural_plasticity_connect_umm(coordinator, nullptr);
    (void)result;  // May or may not succeed
}

// ============================================================================
// Immune Integration Tests
// ============================================================================

TEST_F(NeuralPlasticityCoordinatorTest, ConnectImmune) {
    config.enable_immune_integration = true;
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    // Immune connection is optional
    int result = neural_plasticity_connect_immune(coordinator, nullptr);
    (void)result;  // May or may not succeed
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(NeuralPlasticityCoordinatorTest, StepWithVerySmallDt) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    int result = neural_plasticity_step(coordinator, 0.001f, nullptr, 1000);
    EXPECT_GE(result, 0);
}

TEST_F(NeuralPlasticityCoordinatorTest, StepWithLargeDt) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    int result = neural_plasticity_step(coordinator, 10.0f, nullptr, 1000);
    EXPECT_GE(result, 0);
}

TEST_F(NeuralPlasticityCoordinatorTest, LongSimulation) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);

    // Register neurons
    for (uint32_t i = 0; i < 10; i++) {
        neuron_model_state_t state = NULL;
        state = neuron_model_create(vtable, &params);
        neural_plasticity_register_neuron(coordinator, i, state, vtable);
    }

    // Register synapses
    for (uint32_t i = 0; i < 20; i++) {
        neural_plasticity_register_synapse(coordinator, i, i % 5, i / 5, 0, 0.5f);
    }

    // Run long simulation
    float inputs[10] = {5.0f, 10.0f, 15.0f, 10.0f, 5.0f,
                        5.0f, 10.0f, 15.0f, 10.0f, 5.0f};

    int total_spikes = 0;
    for (int t = 0; t < 10000; t++) {
        int result = neural_plasticity_step(coordinator, 0.1f, inputs, t * 100);
        if (result > 0) total_spikes += result;
    }

    // Should have generated spikes
    EXPECT_GT(total_spikes, 0);

    // Check stats
    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.total_steps, 10000u);
    EXPECT_GT(stats.total_spikes, 0u);
}

TEST_F(NeuralPlasticityCoordinatorTest, WeightChangesOverTime) {
    CreateCoordinator();
    ASSERT_NE(coordinator, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);

    // Register pre and post neurons
    for (uint32_t i = 0; i < 2; i++) {
        neuron_model_state_t state = NULL;
        state = neuron_model_create(vtable, &params);
        neural_plasticity_register_neuron(coordinator, i, state, vtable);
    }

    // Register synapse between them
    neural_plasticity_register_synapse(coordinator, 100, 0, 1, 0, 0.5f);

    float initial_weight = neural_plasticity_get_weight(coordinator, 100);
    EXPECT_FLOAT_EQ(initial_weight, 0.5f);

    // Run simulation with stimulation
    float inputs[2] = {20.0f, 20.0f};
    for (int t = 0; t < 1000; t++) {
        neural_plasticity_step(coordinator, 0.1f, inputs, t * 100);
    }

    // Weight may have changed due to plasticity
    float final_weight = neural_plasticity_get_weight(coordinator, 100);
    // Just verify it's still a valid weight
    EXPECT_GE(final_weight, 0.0f);
    EXPECT_LE(final_weight, 1.0f);
}
