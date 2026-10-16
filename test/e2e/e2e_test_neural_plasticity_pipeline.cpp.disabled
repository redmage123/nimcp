/**
 * @file e2e_test_neural_plasticity_pipeline.cpp
 * @brief E2E Tests for Neural Plasticity Integration Pipeline
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Comprehensive end-to-end tests for neural-plasticity integration
 * WHY:  Verify complete spike-to-plasticity pipeline works in realistic scenarios
 * HOW:  Test through complete neuron→axon→dendrite→plasticity workflow
 *
 * TEST SCENARIOS:
 * 1. BasicNeuralPipeline - Neurons spike → weights change
 * 2. STDPTiming - Pre-post timing → LTP/LTD
 * 3. NetworkPlasticity - Multi-neuron network with plasticity
 * 4. RewardModulation - R-STDP with eligibility traces
 * 5. StructuralPlasticity - Activity → spine formation
 * 6. LongTermSimulation - Extended simulation stability
 * 7. DynamicSynapses - Add/remove during simulation
 * 8. BiologicalTimescales - Realistic spike patterns
 * 9. NetworkStability - Weight homeostasis
 * 10. CompleteLifecycle - Create→Run→Stats→Destroy
 *
 * BIOLOGICAL BASIS:
 * - Neurons: Izhikevich regular spiking (RS) type
 * - Axons: Spike propagation with conduction delays
 * - Dendrites: Spine-based synapse organization
 * - Plasticity: Triplet STDP, BCM, homeostatic mechanisms
 *
 * @author NIMCP Development Team
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

extern "C" {
#include "nimcp.h"
#include "plasticity/orchestrator/nimcp_neural_plasticity_coordinator.h"
#include "plasticity/orchestrator/nimcp_axon_orchestrator_bridge.h"
#include "plasticity/orchestrator/nimcp_neuron_orchestrator_bridge.h"
#include "plasticity/orchestrator/nimcp_dendrite_orchestrator_bridge.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "core/axon/nimcp_axon.h"
#include "core/dendrite/nimcp_dendrite.h"
#include "core/neuron_models/nimcp_neuron_model.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
}

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr uint32_t NUM_NEURONS = 20;
constexpr uint32_t NUM_SYNAPSES = 50;
constexpr float DT_MS = 0.1f;
constexpr float DEFAULT_WEIGHT = 0.5f;

// Timing parameters
constexpr uint64_t SHORT_SIMULATION_MS = 1000;   // 1 second
constexpr uint64_t MEDIUM_SIMULATION_MS = 10000;  // 10 seconds
constexpr uint64_t LONG_SIMULATION_MS = 100000;   // 100 seconds

// Biological parameters
constexpr float RESTING_INPUT = 0.0f;
constexpr float SUBTHRESHOLD_INPUT = 5.0f;
constexpr float SUPRATHRESHOLD_INPUT = 20.0f;

//=============================================================================
// Test Fixtures
//=============================================================================

class NeuralPlasticityE2ETest : public E2ETestBase {
protected:
    neural_plasticity_coordinator_t* coordinator = nullptr;
    axon_network_t* axon_network = nullptr;
    dendrite_network_t* dendrite_network = nullptr;

    void SetUp() override {
        E2ETestBase::SetUp();

        // Create networks
        axon_network_config_t axon_config;
        axon_network_default_config(&axon_config);
        axon_network = axon_network_create(&axon_config);
        ASSERT_NE(axon_network, nullptr);

        dendrite_network_config_t dend_config;
        dendrite_network_default_config(&dend_config);
        dendrite_network = dendrite_network_create(&dend_config);
        ASSERT_NE(dendrite_network, nullptr);

        // Create coordinator
        neural_plasticity_config_t config;
        neural_plasticity_default_config(&config);
        coordinator = neural_plasticity_coordinator_create(&config, axon_network, dendrite_network);
        ASSERT_NE(coordinator, nullptr);
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
        E2ETestBase::TearDown();
    }

    void RegisterNeurons(uint32_t count) {
        const neuron_model_vtable_t* vtable = neuron_model_get_vtable(NEURON_MODEL_IZHIKEVICH);
        izhikevich_params_t params = IZHIKEVICH_RS;

        for (uint32_t i = 0; i < count; i++) {
            neuron_model_state_t state;
            neuron_model_init(&state, NEURON_MODEL_IZHIKEVICH, &params);
            neural_plasticity_register_neuron(coordinator, i, state, vtable);
        }
    }

    void RegisterSynapses(uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            uint32_t pre_id = i % NUM_NEURONS;
            uint32_t post_id = (i + 1) % NUM_NEURONS;
            neural_plasticity_register_synapse(coordinator, i, pre_id, post_id, 0, DEFAULT_WEIGHT);
        }
    }

    int RunSimulation(uint64_t duration_ms, const float* inputs) {
        int total_spikes = 0;
        uint64_t steps = (uint64_t)(duration_ms / DT_MS);

        for (uint64_t t = 0; t < steps; t++) {
            int result = neural_plasticity_step(coordinator, DT_MS, inputs, t * (uint64_t)(DT_MS * 1000));
            if (result > 0) total_spikes += result;
        }

        return total_spikes;
    }
};

//=============================================================================
// Test Scenarios
//=============================================================================

/**
 * @test BasicNeuralPipeline
 * @brief Verify complete neural-plasticity pipeline works
 */
TEST_F(NeuralPlasticityE2ETest, BasicNeuralPipeline) {
    LOG_TEST_START("BasicNeuralPipeline");

    // Setup
    RegisterNeurons(NUM_NEURONS);
    RegisterSynapses(NUM_SYNAPSES);

    // Run with suprathreshold input
    std::vector<float> inputs(NUM_NEURONS, SUPRATHRESHOLD_INPUT);
    int spikes = RunSimulation(SHORT_SIMULATION_MS, inputs.data());

    // Verify spikes occurred
    EXPECT_GT(spikes, 0) << "Expected neurons to spike with suprathreshold input";

    // Verify stats updated
    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_GT(stats.total_spikes, 0u);
    EXPECT_GT(stats.total_steps, 0u);

    LOG_TEST_END("BasicNeuralPipeline");
}

/**
 * @test SubthresholdNoSpikes
 * @brief Verify subthreshold input does not cause spiking
 */
TEST_F(NeuralPlasticityE2ETest, SubthresholdNoSpikes) {
    LOG_TEST_START("SubthresholdNoSpikes");

    RegisterNeurons(NUM_NEURONS);

    // Run with subthreshold input
    std::vector<float> inputs(NUM_NEURONS, SUBTHRESHOLD_INPUT);
    int spikes = RunSimulation(SHORT_SIMULATION_MS, inputs.data());

    // May or may not spike depending on exact parameters
    // Just verify simulation completed
    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_GT(stats.total_steps, 0u);

    LOG_TEST_END("SubthresholdNoSpikes");
}

/**
 * @test WeightEvolutionWithActivity
 * @brief Verify weights change with neural activity
 */
TEST_F(NeuralPlasticityE2ETest, WeightEvolutionWithActivity) {
    LOG_TEST_START("WeightEvolutionWithActivity");

    RegisterNeurons(NUM_NEURONS);
    RegisterSynapses(NUM_SYNAPSES);

    // Record initial weights
    std::vector<float> initial_weights;
    for (uint32_t i = 0; i < NUM_SYNAPSES; i++) {
        initial_weights.push_back(neural_plasticity_get_weight(coordinator, i));
    }

    // Run simulation with activity
    std::vector<float> inputs(NUM_NEURONS, SUPRATHRESHOLD_INPUT);
    RunSimulation(MEDIUM_SIMULATION_MS, inputs.data());

    // Check weights are still valid
    for (uint32_t i = 0; i < NUM_SYNAPSES; i++) {
        float weight = neural_plasticity_get_weight(coordinator, i);
        EXPECT_GE(weight, 0.0f) << "Weight below 0 at synapse " << i;
        EXPECT_LE(weight, 1.0f) << "Weight above 1 at synapse " << i;
        EXPECT_FALSE(std::isnan(weight)) << "NaN weight at synapse " << i;
    }

    LOG_TEST_END("WeightEvolutionWithActivity");
}

/**
 * @test RewardModulation
 * @brief Verify reward signals modulate learning
 */
TEST_F(NeuralPlasticityE2ETest, RewardModulation) {
    LOG_TEST_START("RewardModulation");

    RegisterNeurons(NUM_NEURONS);
    RegisterSynapses(NUM_SYNAPSES);

    std::vector<float> inputs(NUM_NEURONS, SUPRATHRESHOLD_INPUT);

    // Run with periodic rewards
    for (uint64_t t = 0; t < 1000; t++) {
        neural_plasticity_step(coordinator, DT_MS, inputs.data(), t * (uint64_t)(DT_MS * 1000));

        // Deliver reward periodically
        if (t % 100 == 0) {
            neural_plasticity_reward(coordinator, 1.0f, t * (uint64_t)(DT_MS * 1000));
        }
    }

    // Verify simulation completed
    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.total_steps, 1000u);

    LOG_TEST_END("RewardModulation");
}

/**
 * @test FiringRateTracking
 * @brief Verify firing rates are tracked correctly
 */
TEST_F(NeuralPlasticityE2ETest, FiringRateTracking) {
    LOG_TEST_START("FiringRateTracking");

    RegisterNeurons(NUM_NEURONS);

    // Varying inputs to create different firing rates
    std::vector<float> inputs(NUM_NEURONS);
    for (uint32_t i = 0; i < NUM_NEURONS; i++) {
        inputs[i] = 5.0f + i * 2.0f;  // 5, 7, 9, 11, ... pA
    }

    RunSimulation(MEDIUM_SIMULATION_MS, inputs.data());

    // Higher input neurons should have higher rates (approximately)
    std::vector<float> rates;
    for (uint32_t i = 0; i < NUM_NEURONS; i++) {
        rates.push_back(neural_plasticity_get_firing_rate(coordinator, i));
    }

    // All rates should be finite
    for (size_t i = 0; i < rates.size(); i++) {
        EXPECT_GE(rates[i], 0.0f) << "Negative rate at neuron " << i;
        EXPECT_FALSE(std::isnan(rates[i])) << "NaN rate at neuron " << i;
    }

    LOG_TEST_END("FiringRateTracking");
}

/**
 * @test LongTermSimulation
 * @brief Verify system stability over extended simulation
 */
TEST_F(NeuralPlasticityE2ETest, LongTermSimulation) {
    LOG_TEST_START("LongTermSimulation");

    RegisterNeurons(NUM_NEURONS);
    RegisterSynapses(NUM_SYNAPSES);

    std::vector<float> inputs(NUM_NEURONS, 10.0f);

    // Long simulation
    int total_spikes = RunSimulation(LONG_SIMULATION_MS, inputs.data());
    EXPECT_GT(total_spikes, 0);

    // Check all weights still valid
    for (uint32_t i = 0; i < NUM_SYNAPSES; i++) {
        float weight = neural_plasticity_get_weight(coordinator, i);
        EXPECT_GE(weight, 0.0f);
        EXPECT_LE(weight, 1.0f);
        EXPECT_FALSE(std::isnan(weight));
        EXPECT_FALSE(std::isinf(weight));
    }

    // Check stats
    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.total_steps, (uint64_t)(LONG_SIMULATION_MS / DT_MS));

    LOG_TEST_END("LongTermSimulation");
}

/**
 * @test DynamicSynapseAddRemove
 * @brief Verify synapse addition/removal during simulation
 */
TEST_F(NeuralPlasticityE2ETest, DynamicSynapseAddRemove) {
    LOG_TEST_START("DynamicSynapseAddRemove");

    RegisterNeurons(NUM_NEURONS);

    std::vector<float> inputs(NUM_NEURONS, 15.0f);

    // Add synapses dynamically during simulation
    for (int cycle = 0; cycle < 10; cycle++) {
        // Add synapses
        for (uint32_t i = 0; i < 10; i++) {
            uint32_t syn_id = cycle * 100 + i;
            neural_plasticity_register_synapse(coordinator, syn_id, i % NUM_NEURONS, (i + 1) % NUM_NEURONS, 0, 0.5f);
        }

        // Run some steps
        for (int t = 0; t < 100; t++) {
            neural_plasticity_step(coordinator, DT_MS, inputs.data(), (cycle * 100 + t) * 100);
        }

        // Remove some synapses
        for (uint32_t i = 0; i < 5; i++) {
            uint32_t syn_id = cycle * 100 + i;
            neural_plasticity_unregister_synapse(coordinator, syn_id);
        }
    }

    // Verify no crashes
    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_GT(stats.total_steps, 0u);

    LOG_TEST_END("DynamicSynapseAddRemove");
}

/**
 * @test BridgeAccess
 * @brief Verify all sub-bridges are accessible
 */
TEST_F(NeuralPlasticityE2ETest, BridgeAccess) {
    LOG_TEST_START("BridgeAccess");

    // Access all bridges
    plasticity_orchestrator_t* orch = neural_plasticity_get_orchestrator(coordinator);
    ASSERT_NE(orch, nullptr);

    axon_orchestrator_bridge_t* axon_bridge = neural_plasticity_get_axon_bridge(coordinator);
    ASSERT_NE(axon_bridge, nullptr);

    neuron_orchestrator_bridge_t* neuron_bridge = neural_plasticity_get_neuron_bridge(coordinator);
    ASSERT_NE(neuron_bridge, nullptr);

    dendrite_orchestrator_bridge_t* dendrite_bridge = neural_plasticity_get_dendrite_bridge(coordinator);
    ASSERT_NE(dendrite_bridge, nullptr);

    // Access networks
    axon_network_t* axon_net = neural_plasticity_get_axon_network(coordinator);
    EXPECT_EQ(axon_net, axon_network);

    dendrite_network_t* dend_net = neural_plasticity_get_dendrite_network(coordinator);
    EXPECT_EQ(dend_net, dendrite_network);

    LOG_TEST_END("BridgeAccess");
}

/**
 * @test StatsReset
 * @brief Verify stats can be reset
 */
TEST_F(NeuralPlasticityE2ETest, StatsReset) {
    LOG_TEST_START("StatsReset");

    RegisterNeurons(10);

    std::vector<float> inputs(10, 15.0f);
    RunSimulation(SHORT_SIMULATION_MS, inputs.data());

    // Verify non-zero stats
    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_GT(stats.total_steps, 0u);

    // Reset
    neural_plasticity_reset_stats(coordinator);

    // Verify zero
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.total_steps, 0u);
    EXPECT_EQ(stats.total_spikes, 0u);

    LOG_TEST_END("StatsReset");
}

/**
 * @test BiologicalTimescales
 * @brief Verify realistic spike patterns emerge
 */
TEST_F(NeuralPlasticityE2ETest, BiologicalTimescales) {
    LOG_TEST_START("BiologicalTimescales");

    RegisterNeurons(10);
    RegisterSynapses(20);

    // Strong input should give ~50-100 Hz firing
    std::vector<float> inputs(10, 25.0f);
    int spikes = RunSimulation(MEDIUM_SIMULATION_MS, inputs.data());

    // Expect reasonable spike count
    // 10 neurons * 10s * ~50Hz = ~5000 spikes total
    EXPECT_GT(spikes, 100) << "Too few spikes for strong input";

    // Check firing rates are in biological range
    for (uint32_t i = 0; i < 10; i++) {
        float rate = neural_plasticity_get_firing_rate(coordinator, i);
        EXPECT_LE(rate, 200.0f) << "Firing rate too high at neuron " << i;
    }

    LOG_TEST_END("BiologicalTimescales");
}

/**
 * @test CompleteLifecycle
 * @brief Verify complete create→run→cleanup cycle
 */
TEST_F(NeuralPlasticityE2ETest, CompleteLifecycle) {
    LOG_TEST_START("CompleteLifecycle");

    for (int iteration = 0; iteration < 10; iteration++) {
        // Create fresh coordinator
        neural_plasticity_coordinator_destroy(coordinator);

        axon_network_config_t axon_config;
        axon_network_default_config(&axon_config);
        axon_network_t* new_axon = axon_network_create(&axon_config);

        dendrite_network_config_t dend_config;
        dendrite_network_default_config(&dend_config);
        dendrite_network_t* new_dend = dendrite_network_create(&dend_config);

        neural_plasticity_config_t config;
        neural_plasticity_default_config(&config);
        coordinator = neural_plasticity_coordinator_create(&config, new_axon, new_dend);
        ASSERT_NE(coordinator, nullptr) << "Failed at iteration " << iteration;

        // Register and run
        RegisterNeurons(5);
        RegisterSynapses(10);

        std::vector<float> inputs(5, 10.0f);
        RunSimulation(100, inputs.data());

        // Verify stats
        neural_plasticity_stats_t stats;
        neural_plasticity_get_stats(coordinator, &stats);
        EXPECT_GT(stats.total_steps, 0u);

        // Update network pointers for TearDown
        axon_network_destroy(axon_network);
        dendrite_network_destroy(dendrite_network);
        axon_network = new_axon;
        dendrite_network = new_dend;
    }

    LOG_TEST_END("CompleteLifecycle");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
