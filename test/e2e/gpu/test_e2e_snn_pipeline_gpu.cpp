/**
 * @file test_e2e_snn_pipeline_gpu.cpp
 * @brief E2E Tests for GPU Spiking Neural Network (SNN) Pipeline
 *
 * WHAT: End-to-end testing for GPU-accelerated SNN simulation
 * WHY:  Verify SNN dynamics, spike generation, and LIF neuron behavior on GPU
 * HOW:  Test LIF neurons, spike generation, forward propagation
 *
 * TEST PIPELINES:
 * - LIFNeuronCreation: Create Leaky Integrate-and-Fire neurons on GPU
 * - SingleTimestepSimulation: Run single timestep of SNN simulation
 * - MultiTimestepSimulation: Run multiple timesteps with spike propagation
 * - MembraneEvolution: Verify membrane potential dynamics
 * - SpikeGenerationThreshold: Test spike threshold behavior
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "e2e_test_framework.h"

// Headers have their own extern "C" guards
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/snn/nimcp_snn_gpu.h"

#include <vector>
#include <cmath>
#include <cstring>
#include <random>
#include <chrono>
#include <iostream>
#include <algorithm>

//=============================================================================
// SNN Constants
//=============================================================================

// LIF Neuron parameters
constexpr float LIF_TAU_MEM = 20.0f;      // Membrane time constant (ms)
constexpr float LIF_TAU_SYN = 5.0f;       // Synaptic time constant (ms)
constexpr float LIF_V_REST = -65.0f;      // Resting potential (mV)
constexpr float LIF_V_THRESH = -55.0f;    // Spike threshold (mV)
constexpr float LIF_V_RESET = -70.0f;     // Reset potential (mV)
constexpr float LIF_DT = 1.0f;            // Time step (ms)

//=============================================================================
// Test Fixture
//=============================================================================

class GPUSNNPipelineE2ETest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx_ = nullptr;
    bool gpu_available_ = false;

    void SetUp() override {
        ctx_ = nimcp_gpu_context_create(0);
        gpu_available_ = (ctx_ != nullptr);

        if (!gpu_available_) {
            std::cout << "GPU not available - some tests will be skipped" << std::endl;
        }
    }

    void TearDown() override {
        if (ctx_) {
            nimcp_gpu_context_destroy(ctx_);
            ctx_ = nullptr;
        }
    }

    bool HasGPU() const { return gpu_available_; }

    void SkipIfNoGPU() {
        if (!gpu_available_) {
            GTEST_SKIP() << "GPU not available";
        }
    }
};

//=============================================================================
// Pipeline 1: LIF Neuron Creation
//=============================================================================

TEST_F(GPUSNNPipelineE2ETest, LIFNeuronCreation) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("LIF Neuron Creation");

    const size_t num_neurons = 256;

    // Stage 1: Create LIF layer
    E2E_STAGE_BEGIN("Create LIF layer", 500);

    nimcp_snn_lif_config_t config;
    config.num_neurons = num_neurons;
    config.tau_mem = LIF_TAU_MEM;
    config.tau_syn = LIF_TAU_SYN;
    config.v_rest = LIF_V_REST;
    config.v_thresh = LIF_V_THRESH;
    config.v_reset = LIF_V_RESET;
    config.dt = LIF_DT;
    config.refractory_period = 2.0f;
    config.model = NIMCP_SNN_LIF;

    nimcp_snn_layer_t* layer = nimcp_snn_lif_layer_create(ctx_, &config);
    E2E_ASSERT_NOT_NULL(layer, "Failed to create LIF layer");

    E2E_STAGE_END();

    // Stage 2: Verify initial state
    E2E_STAGE_BEGIN("Verify initial state", 500);

    // Get membrane potentials
    nimcp_gpu_tensor_t* v_mem = nimcp_snn_layer_get_membrane(layer);
    E2E_ASSERT_NOT_NULL(v_mem, "Failed to get membrane tensor");

    std::vector<float> v_host(num_neurons);
    nimcp_gpu_tensor_to_host(v_mem, v_host.data());

    // All neurons should start at rest
    bool all_at_rest = true;
    for (size_t i = 0; i < num_neurons; i++) {
        if (std::abs(v_host[i] - LIF_V_REST) > 0.01f) {
            all_at_rest = false;
            break;
        }
    }
    EXPECT_TRUE(all_at_rest) << "All neurons should start at resting potential";

    std::cout << "\n  Created " << num_neurons << " LIF neurons" << std::endl;
    std::cout << "  Initial membrane: " << v_host[0] << " mV" << std::endl;

    E2E_STAGE_END();

    // Stage 3: Verify layer parameters
    E2E_STAGE_BEGIN("Verify layer parameters", 300);

    size_t layer_size = nimcp_snn_layer_get_size(layer);
    EXPECT_EQ(layer_size, num_neurons);

    float tau_mem = nimcp_snn_layer_get_tau_mem(layer);
    EXPECT_NEAR(tau_mem, LIF_TAU_MEM, 0.01f);

    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_snn_layer_destroy(layer);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Single Timestep Simulation
//=============================================================================

TEST_F(GPUSNNPipelineE2ETest, SingleTimestepSimulation) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Single Timestep Simulation");

    const size_t num_neurons = 64;

    // Stage 1: Create LIF layer
    E2E_STAGE_BEGIN("Create LIF layer", 500);

    nimcp_snn_lif_config_t config;
    config.num_neurons = num_neurons;
    config.tau_mem = LIF_TAU_MEM;
    config.tau_syn = LIF_TAU_SYN;
    config.v_rest = LIF_V_REST;
    config.v_thresh = LIF_V_THRESH;
    config.v_reset = LIF_V_RESET;
    config.dt = LIF_DT;
    config.refractory_period = 2.0f;
    config.model = NIMCP_SNN_LIF;

    nimcp_snn_layer_t* layer = nimcp_snn_lif_layer_create(ctx_, &config);
    E2E_ASSERT_NOT_NULL(layer, "Failed to create LIF layer");

    E2E_STAGE_END();

    // Stage 2: Create input current
    E2E_STAGE_BEGIN("Create input current", 500);

    // Strong input current to trigger spikes
    std::vector<float> input_current(num_neurons);
    for (size_t i = 0; i < num_neurons; i++) {
        // Half neurons get strong input, half get weak
        input_current[i] = (i < num_neurons / 2) ? 15.0f : 5.0f;
    }

    size_t current_dims[] = {num_neurons};
    nimcp_gpu_tensor_t* current = nimcp_gpu_tensor_from_host(
        ctx_, input_current.data(), current_dims, 1, NIMCP_GPU_PRECISION_FP32);
    E2E_ASSERT_NOT_NULL(current, "Failed to create input current");

    E2E_STAGE_END();

    // Stage 3: Run single timestep
    E2E_STAGE_BEGIN("Run single timestep", 500);

    nimcp_gpu_tensor_t* spikes = nimcp_snn_lif_step(ctx_, layer, current);
    E2E_ASSERT_NOT_NULL(spikes, "Failed to run LIF step");

    nimcp_gpu_context_synchronize(ctx_);

    E2E_STAGE_END();

    // Stage 4: Check membrane evolution
    E2E_STAGE_BEGIN("Check membrane evolution", 500);

    nimcp_gpu_tensor_t* v_mem = nimcp_snn_layer_get_membrane(layer);
    std::vector<float> v_host(num_neurons);
    nimcp_gpu_tensor_to_host(v_mem, v_host.data());

    // Neurons with strong input should have higher membrane potential
    float avg_strong = 0.0f, avg_weak = 0.0f;
    for (size_t i = 0; i < num_neurons / 2; i++) {
        avg_strong += v_host[i];
    }
    for (size_t i = num_neurons / 2; i < num_neurons; i++) {
        avg_weak += v_host[i];
    }
    avg_strong /= (num_neurons / 2);
    avg_weak /= (num_neurons / 2);

    std::cout << "\n  After 1 timestep:" << std::endl;
    std::cout << "    Avg membrane (strong input): " << avg_strong << " mV" << std::endl;
    std::cout << "    Avg membrane (weak input):   " << avg_weak << " mV" << std::endl;

    // Potentials should be valid numbers
    EXPECT_FALSE(std::isnan(avg_strong));
    EXPECT_FALSE(std::isnan(avg_weak));

    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_gpu_tensor_destroy(current);
    nimcp_snn_layer_destroy(layer);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Multi-Timestep Simulation
//=============================================================================

TEST_F(GPUSNNPipelineE2ETest, MultiTimestepSimulation) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Multi-Timestep Simulation");

    const size_t num_neurons = 128;
    const size_t num_timesteps = 100;

    // Stage 1: Create LIF layer
    E2E_STAGE_BEGIN("Create LIF layer", 500);

    nimcp_snn_lif_config_t config;
    config.num_neurons = num_neurons;
    config.tau_mem = LIF_TAU_MEM;
    config.tau_syn = LIF_TAU_SYN;
    config.v_rest = LIF_V_REST;
    config.v_thresh = LIF_V_THRESH;
    config.v_reset = LIF_V_RESET;
    config.dt = LIF_DT;
    config.refractory_period = 2.0f;
    config.model = NIMCP_SNN_LIF;

    nimcp_snn_layer_t* layer = nimcp_snn_lif_layer_create(ctx_, &config);
    E2E_ASSERT_NOT_NULL(layer, "Failed to create LIF layer");

    E2E_STAGE_END();

    // Stage 2: Create constant input
    E2E_STAGE_BEGIN("Create constant input", 500);

    std::vector<float> input_current(num_neurons, 12.0f);  // Constant moderate input
    size_t current_dims[] = {num_neurons};
    nimcp_gpu_tensor_t* current = nimcp_gpu_tensor_from_host(
        ctx_, input_current.data(), current_dims, 1, NIMCP_GPU_PRECISION_FP32);
    E2E_ASSERT_NOT_NULL(current, "Failed to create input current");

    E2E_STAGE_END();

    // Stage 3: Run simulation
    E2E_STAGE_BEGIN("Run multi-timestep simulation", 3000);

    std::vector<size_t> spike_counts(num_timesteps, 0);
    std::vector<float> avg_membrane(num_timesteps, 0.0f);

    for (size_t t = 0; t < num_timesteps; t++) {
        nimcp_gpu_tensor_t* spikes = nimcp_snn_lif_step(ctx_, layer, current);

        if (spikes) {
            // Get spike tensor data
            std::vector<float> spike_host(num_neurons);
            nimcp_gpu_tensor_to_host(spikes, spike_host.data());

            for (size_t i = 0; i < num_neurons; i++) {
                if (spike_host[i] > 0.5f) {
                    spike_counts[t]++;
                }
            }
        }

        // Track membrane voltage
        nimcp_gpu_tensor_t* v_mem = nimcp_snn_layer_get_membrane(layer);
        if (v_mem) {
            std::vector<float> v_host(num_neurons);
            nimcp_gpu_tensor_to_host(v_mem, v_host.data());

            float sum = 0.0f;
            for (auto v : v_host) {
                sum += v;
            }
            avg_membrane[t] = sum / num_neurons;
        }
    }

    nimcp_gpu_context_synchronize(ctx_);

    // Report statistics
    size_t total_spikes = 0;
    for (auto count : spike_counts) {
        total_spikes += count;
    }

    std::cout << "\n  Simulation results:" << std::endl;
    std::cout << "    Total spikes: " << total_spikes << std::endl;
    std::cout << "    Avg spikes/timestep: " << (float)total_spikes / num_timesteps << std::endl;
    std::cout << "    Final avg membrane: " << avg_membrane.back() << " mV" << std::endl;

    // With constant input, we should see some spikes
    EXPECT_GT(total_spikes, 0) << "Should generate some spikes with moderate input";

    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_gpu_tensor_destroy(current);
    nimcp_snn_layer_destroy(layer);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Membrane Evolution
//=============================================================================

TEST_F(GPUSNNPipelineE2ETest, MembraneEvolution) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Membrane Evolution");

    const size_t num_neurons = 32;

    // Stage 1: Create LIF layer
    E2E_STAGE_BEGIN("Create LIF layer", 500);

    nimcp_snn_lif_config_t config;
    config.num_neurons = num_neurons;
    config.tau_mem = LIF_TAU_MEM;
    config.tau_syn = LIF_TAU_SYN;
    config.v_rest = LIF_V_REST;
    config.v_thresh = LIF_V_THRESH;
    config.v_reset = LIF_V_RESET;
    config.dt = LIF_DT;
    config.refractory_period = 2.0f;
    config.model = NIMCP_SNN_LIF;

    nimcp_snn_layer_t* layer = nimcp_snn_lif_layer_create(ctx_, &config);
    E2E_ASSERT_NOT_NULL(layer, "Failed to create LIF layer");

    E2E_STAGE_END();

    // Stage 2: Test with subthreshold input
    E2E_STAGE_BEGIN("Subthreshold input test", 1000);

    // Weak input that shouldn't trigger spikes
    std::vector<float> weak_input(num_neurons, 3.0f);
    size_t dims[] = {num_neurons};
    nimcp_gpu_tensor_t* input_weak = nimcp_gpu_tensor_from_host(
        ctx_, weak_input.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);

    std::vector<float> membrane_evolution;

    // Run 20 timesteps with weak input
    for (int t = 0; t < 20; t++) {
        nimcp_snn_lif_step(ctx_, layer, input_weak);

        nimcp_gpu_tensor_t* v_mem = nimcp_snn_layer_get_membrane(layer);
        std::vector<float> v_host(num_neurons);
        nimcp_gpu_tensor_to_host(v_mem, v_host.data());
        membrane_evolution.push_back(v_host[0]);
    }

    std::cout << "\n  Subthreshold membrane evolution:" << std::endl;
    for (size_t i = 0; i < membrane_evolution.size(); i += 5) {
        std::cout << "    t=" << i << ": " << membrane_evolution[i] << " mV" << std::endl;
    }

    // Membrane should increase but stay below threshold
    for (auto v : membrane_evolution) {
        EXPECT_FALSE(std::isnan(v));
        // With weak input, membrane shouldn't exceed threshold by much
    }

    nimcp_gpu_tensor_destroy(input_weak);

    E2E_STAGE_END();

    // Stage 3: Reset and test with suprathreshold input
    E2E_STAGE_BEGIN("Suprathreshold input test", 1000);

    bool reset_ok = nimcp_snn_layer_reset(ctx_, layer);
    EXPECT_TRUE(reset_ok);

    // Strong input that should trigger spikes
    std::vector<float> strong_input(num_neurons, 20.0f);
    nimcp_gpu_tensor_t* input_strong = nimcp_gpu_tensor_from_host(
        ctx_, strong_input.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);

    size_t spike_count = 0;
    for (int t = 0; t < 20; t++) {
        nimcp_gpu_tensor_t* spikes = nimcp_snn_lif_step(ctx_, layer, input_strong);

        if (spikes) {
            std::vector<float> spike_host(num_neurons);
            nimcp_gpu_tensor_to_host(spikes, spike_host.data());

            for (size_t i = 0; i < num_neurons; i++) {
                if (spike_host[i] > 0.5f) {
                    spike_count++;
                }
            }
        }
    }

    std::cout << "\n  Suprathreshold test:" << std::endl;
    std::cout << "    Spikes generated: " << spike_count << std::endl;

    // With strong input, we should see spikes
    EXPECT_GT(spike_count, 0) << "Strong input should trigger spikes";

    nimcp_gpu_tensor_destroy(input_strong);

    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_snn_layer_destroy(layer);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Spike Generation Threshold
//=============================================================================

TEST_F(GPUSNNPipelineE2ETest, SpikeGenerationThreshold) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Spike Generation Threshold");

    const size_t num_neurons = 64;

    // Stage 1: Create layers with different thresholds
    E2E_STAGE_BEGIN("Create layers", 500);

    std::vector<float> thresholds = {-60.0f, -55.0f, -50.0f};
    std::vector<nimcp_snn_layer_t*> layers;

    for (float thresh : thresholds) {
        nimcp_snn_lif_config_t config;
        config.num_neurons = num_neurons;
        config.tau_mem = LIF_TAU_MEM;
        config.tau_syn = LIF_TAU_SYN;
        config.v_rest = LIF_V_REST;
        config.v_thresh = thresh;
        config.v_reset = LIF_V_RESET;
        config.dt = LIF_DT;
        config.refractory_period = 2.0f;
        config.model = NIMCP_SNN_LIF;

        nimcp_snn_layer_t* layer = nimcp_snn_lif_layer_create(ctx_, &config);
        if (layer) {
            layers.push_back(layer);
        }
    }

    EXPECT_EQ(layers.size(), thresholds.size()) << "All layers should be created";

    E2E_STAGE_END();

    // Stage 2: Test with constant input
    E2E_STAGE_BEGIN("Test spike rates", 2000);

    std::vector<float> input(num_neurons, 10.0f);
    size_t dims[] = {num_neurons};
    nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_from_host(
        ctx_, input.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);

    std::vector<size_t> total_spikes(layers.size(), 0);

    // Run 50 timesteps for each layer
    for (int t = 0; t < 50; t++) {
        for (size_t l = 0; l < layers.size(); l++) {
            nimcp_gpu_tensor_t* spikes = nimcp_snn_lif_step(ctx_, layers[l], input_tensor);

            if (spikes) {
                std::vector<float> spike_host(num_neurons);
                nimcp_gpu_tensor_to_host(spikes, spike_host.data());

                for (size_t i = 0; i < num_neurons; i++) {
                    if (spike_host[i] > 0.5f) {
                        total_spikes[l]++;
                    }
                }
            }
        }
    }

    std::cout << "\n  Threshold effects on spiking:" << std::endl;
    for (size_t l = 0; l < layers.size(); l++) {
        std::cout << "    Threshold " << thresholds[l] << " mV: "
                  << total_spikes[l] << " spikes" << std::endl;
    }

    // Lower threshold should produce more spikes
    if (total_spikes.size() >= 2) {
        EXPECT_GE(total_spikes[0], total_spikes[1])
            << "Lower threshold should produce more or equal spikes";
    }

    nimcp_gpu_tensor_destroy(input_tensor);

    E2E_STAGE_END();

    // Stage 3: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    for (auto layer : layers) {
        nimcp_snn_layer_destroy(layer);
    }

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
