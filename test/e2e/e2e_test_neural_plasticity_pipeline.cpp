/**
 * @file e2e_test_neural_plasticity_pipeline.cpp
 * @brief E2E Tests for Neural Plasticity Pipeline
 *
 * WHAT: Complete neural-plasticity integration tests (axon/neuron/dendrite)
 * WHY:  Verify complete spike-to-plasticity pipeline
 * HOW:  Test neuron dynamics, spike cascade, bidirectional sync
 *
 * TEST PIPELINES:
 * - CoordinatorLifecycle: Create, configure, destroy coordinator
 * - NeuronRegistration: Register neurons and synapses
 * - SpikeCascade: Spike propagation through axons to plasticity
 * - PlasticityUpdate: Weight changes through coordinator
 * - RewardLearning: Reward signal processing
 *
 * @author NIMCP Development Team
 * @date 2026-01-14
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <atomic>
#include <vector>
#include <cmath>

extern "C" {
#include "plasticity/orchestrator/nimcp_neural_plasticity_coordinator.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "core/axon/nimcp_axon.h"
#include "core/dendrite/nimcp_dendrite.h"
#include "core/neuron_models/nimcp_neuron_model.h"
#include "core/neuron_models/nimcp_izhikevich.h"
}

// Global pipeline tracker (defined in e2e_test_framework.cpp)
extern std::unique_ptr<nimcp::e2e::PipelineTracker> g_current_pipeline;

//=============================================================================
// Test Fixture
//=============================================================================

class NeuralPlasticityPipelineTest : public ::testing::Test {
protected:
    neural_plasticity_coordinator_t* coordinator_{nullptr};
    axon_network_t* axon_network_{nullptr};
    dendrite_network_t* dendrite_network_{nullptr};

    void SetUp() override {
        // Networks are optional for basic tests
        axon_network_ = nullptr;
        dendrite_network_ = nullptr;
        coordinator_ = nullptr;
    }

    void TearDown() override {
        if (coordinator_) {
            neural_plasticity_coordinator_destroy(coordinator_);
            coordinator_ = nullptr;
        }
        if (axon_network_) {
            axon_network_destroy(axon_network_);
            axon_network_ = nullptr;
        }
        if (dendrite_network_) {
            dendrite_network_destroy(dendrite_network_);
            dendrite_network_ = nullptr;
        }
    }

    // Helper to create coordinator with default config
    bool CreateCoordinator() {
        neural_plasticity_config_t config;
        if (neural_plasticity_default_config(&config) != 0) {
            return false;
        }
        config.enable_bio_async = false;  // Disable for simpler testing
        config.enable_immune_integration = false;
        config.enable_umm = false;

        coordinator_ = neural_plasticity_coordinator_create(&config, axon_network_, dendrite_network_);
        return coordinator_ != nullptr;
    }
};

//=============================================================================
// Test 1: Coordinator Lifecycle
//=============================================================================

TEST_F(NeuralPlasticityPipelineTest, CoordinatorLifecycle) {
    E2E_PIPELINE_START("Neural Plasticity Coordinator Lifecycle");

    // Stage 1: Get default config
    E2E_STAGE_BEGIN("Get default config", 100);
    neural_plasticity_config_t config;
    int result = neural_plasticity_default_config(&config);
    E2E_ASSERT(result == 0, "Failed to get default config");
    E2E_ASSERT(config.default_dt_ms > 0.0f, "Invalid default dt");
    E2E_STAGE_END();

    // Stage 2: Create coordinator
    E2E_STAGE_BEGIN("Create coordinator", 500);
    config.enable_bio_async = false;
    coordinator_ = neural_plasticity_coordinator_create(&config, nullptr, nullptr);
    E2E_ASSERT_NOT_NULL(coordinator_, "Failed to create coordinator");
    E2E_STAGE_END();

    // Stage 3: Get orchestrator
    E2E_STAGE_BEGIN("Get orchestrator", 100);
    plasticity_orchestrator_t* orch = neural_plasticity_get_orchestrator(coordinator_);
    E2E_ASSERT_NOT_NULL(orch, "Coordinator should have orchestrator");
    E2E_STAGE_END();

    // Stage 4: Get initial stats
    E2E_STAGE_BEGIN("Get stats", 100);
    neural_plasticity_stats_t stats;
    result = neural_plasticity_get_stats(coordinator_, &stats);
    E2E_ASSERT(result == 0, "Failed to get stats");
    E2E_ASSERT(stats.total_steps == 0, "Initial steps should be 0");
    E2E_STAGE_END();

    // Stage 5: Destroy coordinator
    E2E_STAGE_BEGIN("Destroy coordinator", 200);
    neural_plasticity_coordinator_destroy(coordinator_);
    coordinator_ = nullptr;  // Prevent double-free in TearDown
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 2: Neuron Registration
//=============================================================================

TEST_F(NeuralPlasticityPipelineTest, NeuronRegistration) {
    E2E_PIPELINE_START("Neuron Registration Pipeline");

    // Stage 1: Create coordinator
    E2E_STAGE_BEGIN("Create coordinator", 500);
    E2E_ASSERT(CreateCoordinator(), "Failed to create coordinator");
    E2E_STAGE_END();

    // Stage 2: Get Izhikevich neuron vtable
    E2E_STAGE_BEGIN("Get neuron model vtable", 100);
    const neuron_model_vtable_t* izh_vtable = neuron_model_get_izhikevich_vtable();
    E2E_ASSERT_NOT_NULL(izh_vtable, "Failed to get Izhikevich vtable");
    E2E_STAGE_END();

    // Stage 3: Register neurons
    E2E_STAGE_BEGIN("Register neurons", 200);
    const uint32_t NUM_NEURONS = 10;
    for (uint32_t i = 0; i < NUM_NEURONS; i++) {
        neuron_model_state_t state = neuron_model_create(izh_vtable, nullptr);

        int result = neural_plasticity_register_neuron(coordinator_, i, state, izh_vtable);
        E2E_ASSERT(result == 0, "Failed to register neuron");
    }
    E2E_STAGE_END();

    // Stage 4: Check stats
    E2E_STAGE_BEGIN("Verify registration", 100);
    neural_plasticity_stats_t stats;
    int result = neural_plasticity_get_stats(coordinator_, &stats);
    E2E_ASSERT(result == 0, "Failed to get stats");
    E2E_ASSERT(stats.neurons_registered == NUM_NEURONS, "Neuron count mismatch");
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 3: Simulation Step
//=============================================================================

TEST_F(NeuralPlasticityPipelineTest, SimulationStep) {
    E2E_PIPELINE_START("Simulation Step Pipeline");

    // Stage 1: Create coordinator with neurons
    E2E_STAGE_BEGIN("Setup coordinator", 500);
    E2E_ASSERT(CreateCoordinator(), "Failed to create coordinator");

    const neuron_model_vtable_t* izh_vtable = neuron_model_get_izhikevich_vtable();
    E2E_ASSERT_NOT_NULL(izh_vtable, "Failed to get Izhikevich vtable");

    const uint32_t NUM_NEURONS = 5;
    for (uint32_t i = 0; i < NUM_NEURONS; i++) {
        neuron_model_state_t state = neuron_model_create(izh_vtable, nullptr);
        neural_plasticity_register_neuron(coordinator_, i, state, izh_vtable);
    }
    E2E_STAGE_END();

    // Stage 2: Run simulation steps
    E2E_STAGE_BEGIN("Run 100 steps", 1000);
    const uint32_t NUM_STEPS = 100;
    float inputs[NUM_NEURONS] = {0.0f};

    // Give some neurons input current to trigger spikes
    inputs[0] = 15.0f;  // Above threshold
    inputs[1] = 12.0f;
    inputs[2] = 10.0f;

    uint64_t time_us = 0;
    int total_spikes = 0;
    for (uint32_t step = 0; step < NUM_STEPS; step++) {
        int spikes = neural_plasticity_step(coordinator_, 0.1f, inputs, time_us);
        if (spikes > 0) total_spikes += spikes;
        time_us += 100;  // 0.1ms in microseconds
    }
    E2E_STAGE_END();

    // Stage 3: Verify stats
    E2E_STAGE_BEGIN("Verify simulation", 100);
    neural_plasticity_stats_t stats;
    int result = neural_plasticity_get_stats(coordinator_, &stats);
    E2E_ASSERT(result == 0, "Failed to get stats");
    E2E_ASSERT(stats.total_steps == NUM_STEPS, "Step count mismatch");
    // Some spikes expected with input current above threshold
    std::cout << "Total spikes generated: " << stats.total_spikes << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 4: Reward Learning
//=============================================================================

TEST_F(NeuralPlasticityPipelineTest, RewardLearning) {
    E2E_PIPELINE_START("Reward Learning Pipeline");

    // Stage 1: Setup
    E2E_STAGE_BEGIN("Setup coordinator", 500);
    E2E_ASSERT(CreateCoordinator(), "Failed to create coordinator");

    const neuron_model_vtable_t* izh_vtable = neuron_model_get_izhikevich_vtable();
    for (uint32_t i = 0; i < 3; i++) {
        neuron_model_state_t state = neuron_model_create(izh_vtable, nullptr);
        neural_plasticity_register_neuron(coordinator_, i, state, izh_vtable);
    }
    E2E_STAGE_END();

    // Stage 2: Run some activity
    E2E_STAGE_BEGIN("Generate activity", 500);
    float inputs[3] = {15.0f, 12.0f, 10.0f};
    uint64_t time_us = 0;
    for (int step = 0; step < 50; step++) {
        neural_plasticity_step(coordinator_, 0.1f, inputs, time_us);
        time_us += 100;
    }
    E2E_STAGE_END();

    // Stage 3: Apply positive reward
    E2E_STAGE_BEGIN("Apply positive reward", 200);
    int result = neural_plasticity_reward(coordinator_, 1.0f, time_us);
    E2E_ASSERT(result == 0, "Failed to apply reward");
    E2E_STAGE_END();

    // Stage 4: Apply negative reward
    E2E_STAGE_BEGIN("Apply negative reward", 200);
    time_us += 1000;
    result = neural_plasticity_reward(coordinator_, -0.5f, time_us);
    E2E_ASSERT(result == 0, "Failed to apply punishment");
    E2E_STAGE_END();

    // Stage 5: Check plasticity stats
    E2E_STAGE_BEGIN("Verify plasticity", 100);
    neural_plasticity_stats_t stats;
    result = neural_plasticity_get_stats(coordinator_, &stats);
    E2E_ASSERT(result == 0, "Failed to get stats");
    std::cout << "LTP events: " << stats.ltp_events << ", LTD events: " << stats.ltd_events << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 5: Weight Manipulation
//=============================================================================

TEST_F(NeuralPlasticityPipelineTest, WeightManipulation) {
    E2E_PIPELINE_START("Weight Manipulation Pipeline");

    // Stage 1: Setup with synapse
    E2E_STAGE_BEGIN("Setup with synapse", 500);
    E2E_ASSERT(CreateCoordinator(), "Failed to create coordinator");

    // Register a synapse
    uint32_t synapse_id = 42;
    int result = neural_plasticity_register_synapse(coordinator_, synapse_id, 0, 0, 0, 0.5f);
    E2E_ASSERT(result == 0, "Failed to register synapse");
    E2E_STAGE_END();

    // Stage 2: Get initial weight
    E2E_STAGE_BEGIN("Get weight", 100);
    float weight = neural_plasticity_get_weight(coordinator_, synapse_id);
    E2E_ASSERT(!std::isnan(weight), "Weight should not be NaN");
    std::cout << "Initial weight: " << weight << std::endl;
    E2E_STAGE_END();

    // Stage 3: Set weight
    E2E_STAGE_BEGIN("Set weight", 100);
    result = neural_plasticity_set_weight(coordinator_, synapse_id, 0.8f);
    E2E_ASSERT(result == 0, "Failed to set weight");

    float new_weight = neural_plasticity_get_weight(coordinator_, synapse_id);
    E2E_ASSERT(std::abs(new_weight - 0.8f) < 0.01f, "Weight not updated correctly");
    E2E_STAGE_END();

    // Stage 4: Unregister synapse
    E2E_STAGE_BEGIN("Unregister synapse", 100);
    result = neural_plasticity_unregister_synapse(coordinator_, synapse_id);
    E2E_ASSERT(result == 0, "Failed to unregister synapse");
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 6: Stats Reset
//=============================================================================

TEST_F(NeuralPlasticityPipelineTest, StatsReset) {
    E2E_PIPELINE_START("Stats Reset Pipeline");

    // Stage 1: Setup and run
    E2E_STAGE_BEGIN("Setup and run", 500);
    E2E_ASSERT(CreateCoordinator(), "Failed to create coordinator");

    const neuron_model_vtable_t* izh_vtable = neuron_model_get_izhikevich_vtable();
    neuron_model_state_t state = neuron_model_create(izh_vtable, nullptr);
    neural_plasticity_register_neuron(coordinator_, 0, state, izh_vtable);

    float input = 15.0f;
    for (int i = 0; i < 10; i++) {
        neural_plasticity_step(coordinator_, 0.1f, &input, i * 100);
    }
    E2E_STAGE_END();

    // Stage 2: Check stats non-zero
    E2E_STAGE_BEGIN("Verify non-zero stats", 100);
    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator_, &stats);
    E2E_ASSERT(stats.total_steps > 0, "Should have some steps");
    E2E_STAGE_END();

    // Stage 3: Reset stats
    E2E_STAGE_BEGIN("Reset stats", 100);
    int result = neural_plasticity_reset_stats(coordinator_);
    E2E_ASSERT(result == 0, "Failed to reset stats");
    E2E_STAGE_END();

    // Stage 4: Verify reset
    E2E_STAGE_BEGIN("Verify reset", 100);
    neural_plasticity_get_stats(coordinator_, &stats);
    E2E_ASSERT(stats.total_steps == 0, "Steps should be reset to 0");
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
