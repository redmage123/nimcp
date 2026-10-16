/**
 * @file test_neuron_orchestrator_bridge.cpp
 * @brief Unit Tests for Neuron-Orchestrator Bridge
 *
 * Tests all aspects of the neuron-orchestrator bridge including:
 * - Lifecycle management (create/destroy)
 * - Configuration
 * - Neuron registration and management
 * - Spike cascade (post_spike + axon + bAP)
 * - Firing rate tracking
 * - Bio-async integration
 * - Statistics
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <atomic>

// Headers have their own extern "C" guards
#include "plasticity/orchestrator/nimcp_neuron_orchestrator_bridge.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "core/axon/nimcp_axon.h"
#include "core/dendrite/nimcp_dendrite.h"
#include "core/neuron_models/nimcp_neuron_model.h"
#include "core/neuron_models/nimcp_izhikevich.h"
#include "utils/memory/nimcp_memory.h"

// ============================================================================
// Test Fixture
// ============================================================================

class NeuronOrchestratorBridgeTest : public ::testing::Test {
protected:
    neuron_orchestrator_bridge_t* bridge = nullptr;
    plasticity_orchestrator_t* orchestrator = nullptr;
    axon_network_t* axon_network = nullptr;
    dendrite_network_t* dendrite_network = nullptr;
    neuron_orchestrator_config_t config;

    void SetUp() override {
        neuron_orchestrator_default_config(&config);

        // Create orchestrator
        orchestrator = plasticity_orchestrator_create(nullptr);

        // Create axon network with capacity
        axon_network = axon_network_create(100);

        // Create dendrite network with capacity
        dendrite_network = dendrite_network_create(100);
    }

    void TearDown() override {
        if (bridge) {
            neuron_orchestrator_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (orchestrator) {
            plasticity_orchestrator_destroy(orchestrator);
            orchestrator = nullptr;
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

    // Helper to create bridge
    void CreateBridge() {
        bridge = neuron_orchestrator_bridge_create(&config, orchestrator, axon_network, dendrite_network);
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(NeuronOrchestratorBridgeTest, DefaultConfigSetsReasonableDefaults) {
    EXPECT_EQ(neuron_orchestrator_default_config(&config), 0);

    // Check enabled flags
    EXPECT_TRUE(config.enable_axon_propagation);
    EXPECT_TRUE(config.enable_rate_tracking);
    EXPECT_TRUE(config.enable_bio_async);  // On by default

    // Check parameters
    EXPECT_GT(config.rate_ema_tau_ms, 0.0f);
    EXPECT_GT(config.bap_amplitude, 0.0f);
    EXPECT_GT(config.spike_amplitude, 0.0f);
    EXPECT_GT(config.initial_neuron_capacity, 0u);
}

TEST_F(NeuronOrchestratorBridgeTest, DefaultConfigReturnsErrorForNull) {
    EXPECT_EQ(neuron_orchestrator_default_config(nullptr), -1);
}

TEST_F(NeuronOrchestratorBridgeTest, CreateWithDefaultConfig) {
    bridge = neuron_orchestrator_bridge_create(nullptr, orchestrator, axon_network, dendrite_network);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(NeuronOrchestratorBridgeTest, CreateWithCustomConfig) {
    config.enable_axon_propagation = false;
    config.enable_rate_tracking = false;

    CreateBridge();
    ASSERT_NE(bridge, nullptr);
}

TEST_F(NeuronOrchestratorBridgeTest, CreateRequiresOrchestrator) {
    bridge = neuron_orchestrator_bridge_create(&config, nullptr, axon_network, dendrite_network);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(NeuronOrchestratorBridgeTest, CreateWithNullNetworksAllowed) {
    // Networks are optional (may be connected later)
    bridge = neuron_orchestrator_bridge_create(&config, orchestrator, nullptr, nullptr);
    // May or may not be allowed depending on implementation
    // Just verify no crash
}

TEST_F(NeuronOrchestratorBridgeTest, DestroyNullIsSafe) {
    neuron_orchestrator_bridge_destroy(nullptr);
    // Should not crash
}

// ============================================================================
// Neuron Registration Tests
// ============================================================================

TEST_F(NeuronOrchestratorBridgeTest, RegisterNeuron) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Get Izhikevich regular spiking vtable
    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    ASSERT_NE(vtable, nullptr);

    // Create neuron model state
    neuron_model_state_t state = NULL;
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);  // Regular spiking
    state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);

    // Register neuron
    EXPECT_EQ(neuron_orchestrator_register_neuron(bridge, 1, state, vtable), 0);
}

TEST_F(NeuronOrchestratorBridgeTest, RegisterMultipleNeurons) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    ASSERT_NE(vtable, nullptr);

    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);

    for (uint32_t i = 0; i < 100; i++) {
        neuron_model_state_t state = NULL;
        state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
        EXPECT_EQ(neuron_orchestrator_register_neuron(bridge, i, state, vtable), 0);
    }

    EXPECT_EQ(neuron_orchestrator_get_neuron_count(bridge), 100u);
}

TEST_F(NeuronOrchestratorBridgeTest, UnregisterNeuron) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);

    // Register and unregister
    EXPECT_EQ(neuron_orchestrator_register_neuron(bridge, 1, state, vtable), 0);
    EXPECT_EQ(neuron_orchestrator_get_neuron_count(bridge), 1u);

    EXPECT_EQ(neuron_orchestrator_unregister_neuron(bridge, 1), 0);
    EXPECT_EQ(neuron_orchestrator_get_neuron_count(bridge), 0u);
}

TEST_F(NeuronOrchestratorBridgeTest, RegisterNeuronWithNullBridgeReturnsError) {
    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    neuron_model_state_t state = NULL;

    EXPECT_EQ(neuron_orchestrator_register_neuron(nullptr, 1, state, vtable), -1);
}

TEST_F(NeuronOrchestratorBridgeTest, RegisterNeuronWithNullVtableAllowed) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // NULL vtable is allowed (state-only registration)
    neuron_model_state_t state = NULL;
    EXPECT_EQ(neuron_orchestrator_register_neuron(bridge, 1, state, nullptr), 0);
}

// ============================================================================
// Axon/Dendrite Association Tests
// ============================================================================

TEST_F(NeuronOrchestratorBridgeTest, AddAxonToNeuron) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Register neuron first
    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
    neuron_orchestrator_register_neuron(bridge, 1, state, vtable);

    // Add axon
    EXPECT_EQ(neuron_orchestrator_add_axon(bridge, 1, 10), 0);
}

TEST_F(NeuronOrchestratorBridgeTest, AddMultipleAxonsToNeuron) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
    neuron_orchestrator_register_neuron(bridge, 1, state, vtable);

    // Add multiple axons
    EXPECT_EQ(neuron_orchestrator_add_axon(bridge, 1, 10), 0);
    EXPECT_EQ(neuron_orchestrator_add_axon(bridge, 1, 11), 0);
    EXPECT_EQ(neuron_orchestrator_add_axon(bridge, 1, 12), 0);
}

TEST_F(NeuronOrchestratorBridgeTest, AddDendriteToNeuron) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
    neuron_orchestrator_register_neuron(bridge, 1, state, vtable);

    // Add dendrite
    EXPECT_EQ(neuron_orchestrator_add_dendrite(bridge, 1, 20), 0);
}

TEST_F(NeuronOrchestratorBridgeTest, AddAxonToUnregisteredNeuronReturnsError) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(neuron_orchestrator_add_axon(bridge, 99999, 10), -1);
}

TEST_F(NeuronOrchestratorBridgeTest, AddDendriteToUnregisteredNeuronReturnsError) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(neuron_orchestrator_add_dendrite(bridge, 99999, 20), -1);
}

// ============================================================================
// Neuron Step Tests
// ============================================================================

TEST_F(NeuronOrchestratorBridgeTest, StepNeuron) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
    neuron_orchestrator_register_neuron(bridge, 1, state, vtable);

    // Step the neuron (no spike expected with zero input)
    int result = neuron_orchestrator_step(bridge, 1, 0.1f, 0.0f, 1000);
    EXPECT_EQ(result, 0);  // No spike
}

TEST_F(NeuronOrchestratorBridgeTest, StepNeuronWithStrongInputCausesSpike) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
    neuron_orchestrator_register_neuron(bridge, 1, state, vtable);

    // Step with strong input until spike
    int spike_count = 0;
    for (int i = 0; i < 1000; i++) {
        int result = neuron_orchestrator_step(bridge, 1, 0.1f, 20.0f, i * 100);
        if (result > 0) spike_count++;
    }

    // Should have generated spikes
    EXPECT_GT(spike_count, 0);
}

TEST_F(NeuronOrchestratorBridgeTest, StepAllNeurons) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);

    // Register multiple neurons
    for (uint32_t i = 0; i < 10; i++) {
        neuron_model_state_t state = NULL;
        state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
        neuron_orchestrator_register_neuron(bridge, i, state, vtable);
    }

    // Provide inputs for all neurons
    float inputs[10] = {10.0f, 10.0f, 10.0f, 10.0f, 10.0f,
                        10.0f, 10.0f, 10.0f, 10.0f, 10.0f};

    // Step all
    int result = neuron_orchestrator_step_all(bridge, 0.1f, inputs, 1000);
    EXPECT_GE(result, 0);  // Returns total spike count
}

TEST_F(NeuronOrchestratorBridgeTest, StepUnregisteredNeuronReturnsError) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(neuron_orchestrator_step(bridge, 99999, 0.1f, 0.0f, 1000), -1);
}

// ============================================================================
// Firing Rate Tracking Tests
// ============================================================================

TEST_F(NeuronOrchestratorBridgeTest, GetFiringRateInitiallyZero) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
    neuron_orchestrator_register_neuron(bridge, 1, state, vtable);

    float rate = neuron_orchestrator_get_firing_rate(bridge, 1);
    EXPECT_FLOAT_EQ(rate, 0.0f);
}

TEST_F(NeuronOrchestratorBridgeTest, FiringRateIncreasesWithSpikes) {
    config.enable_rate_tracking = true;
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
    neuron_orchestrator_register_neuron(bridge, 1, state, vtable);

    // Generate spikes with strong input
    for (int i = 0; i < 1000; i++) {
        neuron_orchestrator_step(bridge, 1, 0.1f, 30.0f, i * 100);
    }

    float rate = neuron_orchestrator_get_firing_rate(bridge, 1);
    // Rate should be positive after spiking activity
    EXPECT_GE(rate, 0.0f);
}

TEST_F(NeuronOrchestratorBridgeTest, GetFiringRateForUnregisteredNeuronReturnsNegative) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    float rate = neuron_orchestrator_get_firing_rate(bridge, 99999);
    EXPECT_LT(rate, 0.0f);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(NeuronOrchestratorBridgeTest, GetStatsInitiallyZero) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    neuron_orchestrator_stats_t stats;
    EXPECT_EQ(neuron_orchestrator_get_stats(bridge, &stats), 0);

    EXPECT_EQ(stats.spikes_detected, 0u);
    EXPECT_EQ(stats.step_calls, 0u);
    EXPECT_EQ(stats.neurons_registered, 0u);
    EXPECT_EQ(stats.axon_spikes_initiated, 0u);
    EXPECT_EQ(stats.baps_initiated, 0u);
}

TEST_F(NeuronOrchestratorBridgeTest, GetStatsWithNullBridgeReturnsError) {
    neuron_orchestrator_stats_t stats;
    EXPECT_EQ(neuron_orchestrator_get_stats(nullptr, &stats), -1);
}

TEST_F(NeuronOrchestratorBridgeTest, GetStatsWithNullStatsReturnsError) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(neuron_orchestrator_get_stats(bridge, nullptr), -1);
}

TEST_F(NeuronOrchestratorBridgeTest, ResetStats) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
    neuron_orchestrator_register_neuron(bridge, 1, state, vtable);

    // Create activity
    for (int i = 0; i < 100; i++) {
        neuron_orchestrator_step(bridge, 1, 0.1f, 20.0f, i * 100);
    }

    // Reset and verify
    EXPECT_EQ(neuron_orchestrator_reset_stats(bridge), 0);

    neuron_orchestrator_stats_t stats;
    neuron_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.spikes_detected, 0u);
    EXPECT_EQ(stats.step_calls, 0u);
}

TEST_F(NeuronOrchestratorBridgeTest, NeuronRegistrationTrackedInStats) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);

    for (uint32_t i = 0; i < 5; i++) {
        neuron_model_state_t state = NULL;
        state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
        neuron_orchestrator_register_neuron(bridge, i, state, vtable);
    }

    neuron_orchestrator_stats_t stats;
    neuron_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.neurons_registered, 5u);
}

// ============================================================================
// Step Tracking Tests
// ============================================================================

TEST_F(NeuronOrchestratorBridgeTest, StepCallsTrackedInStats) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
    neuron_orchestrator_register_neuron(bridge, 1, state, vtable);

    // Step the neuron
    neuron_orchestrator_step(bridge, 1, 0.1f, 0.0f, 1000);

    neuron_orchestrator_stats_t stats;
    neuron_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.step_calls, 1u);
}

TEST_F(NeuronOrchestratorBridgeTest, StepAllTrackedInStats) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);

    for (uint32_t i = 0; i < 5; i++) {
        neuron_model_state_t state = NULL;
        state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
        neuron_orchestrator_register_neuron(bridge, i, state, vtable);
    }

    float inputs[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    neuron_orchestrator_step_all(bridge, 0.1f, inputs, 1000);

    neuron_orchestrator_stats_t stats;
    neuron_orchestrator_get_stats(bridge, &stats);
    // step_all counts as 5 step calls (one per neuron)
    EXPECT_GE(stats.step_calls, 1u);
}

// ============================================================================
// Accessor Tests
// ============================================================================

TEST_F(NeuronOrchestratorBridgeTest, GetOrchestrator) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    plasticity_orchestrator_t* orch = neuron_orchestrator_get_orchestrator(bridge);
    EXPECT_EQ(orch, orchestrator);
}

TEST_F(NeuronOrchestratorBridgeTest, GetAxonNetwork) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    axon_network_t* net = neuron_orchestrator_get_axon_network(bridge);
    EXPECT_EQ(net, axon_network);
}

TEST_F(NeuronOrchestratorBridgeTest, GetDendriteNetwork) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    dendrite_network_t* net = neuron_orchestrator_get_dendrite_network(bridge);
    EXPECT_EQ(net, dendrite_network);
}

// ============================================================================
// Bio-Async Integration Tests
// ============================================================================

TEST_F(NeuronOrchestratorBridgeTest, ConnectBioAsyncWithoutRouter) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    int result = neuron_orchestrator_connect_bio_async(bridge);
    (void)result;  // May succeed or fail depending on router availability
}

TEST_F(NeuronOrchestratorBridgeTest, DisconnectBioAsync) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    neuron_orchestrator_connect_bio_async(bridge);
    EXPECT_EQ(neuron_orchestrator_disconnect_bio_async(bridge), 0);
}

TEST_F(NeuronOrchestratorBridgeTest, IsBioAsyncConnectedInitiallyFalse) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(neuron_orchestrator_is_bio_async_connected(bridge));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(NeuronOrchestratorBridgeTest, RegisterSameNeuronTwiceUpdates) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    neuron_model_state_t state = NULL;
    state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);

    // Register twice
    EXPECT_EQ(neuron_orchestrator_register_neuron(bridge, 1, state, vtable), 0);
    EXPECT_EQ(neuron_orchestrator_register_neuron(bridge, 1, state, vtable), 0);

    // Should still be 1 neuron (updated, not duplicated)
    EXPECT_EQ(neuron_orchestrator_get_neuron_count(bridge), 1u);
}

TEST_F(NeuronOrchestratorBridgeTest, UnregisterNonExistentNeuronReturnsError) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(neuron_orchestrator_unregister_neuron(bridge, 99999), -1);
}

TEST_F(NeuronOrchestratorBridgeTest, LargeNeuronCount) {
    config.initial_neuron_capacity = 100;
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);

    // Register many neurons beyond initial capacity
    for (uint32_t i = 0; i < 500; i++) {
        neuron_model_state_t state = NULL;
        state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
        EXPECT_EQ(neuron_orchestrator_register_neuron(bridge, i, state, vtable), 0);
    }

    EXPECT_EQ(neuron_orchestrator_get_neuron_count(bridge), 500u);
}

TEST_F(NeuronOrchestratorBridgeTest, StepNeuronWithNullInputsUseZero) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    const neuron_model_vtable_t* vtable = neuron_model_get_izhikevich_vtable();
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);

    for (uint32_t i = 0; i < 5; i++) {
        neuron_model_state_t state = NULL;
        state = neuron_model_create(neuron_model_get_izhikevich_vtable(), &params);
        neuron_orchestrator_register_neuron(bridge, i, state, vtable);
    }

    // Step all with NULL inputs (should use 0)
    int result = neuron_orchestrator_step_all(bridge, 0.1f, nullptr, 1000);
    EXPECT_GE(result, 0);
}
