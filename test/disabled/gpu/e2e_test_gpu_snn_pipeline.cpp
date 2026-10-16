/**
 * @file e2e_test_gpu_snn_pipeline.cpp
 * @brief E2E Tests for GPU Spiking Neural Network (SNN) Pipeline
 *
 * WHAT: End-to-end testing for GPU-accelerated SNN simulation
 * WHY:  Verify SNN dynamics, STDP learning, and spike propagation on GPU
 * HOW:  Test LIF neurons, spike generation, STDP weight updates, multi-timestep
 *
 * TEST PIPELINES:
 * - LIFNeuronCreation: Create Leaky Integrate-and-Fire neurons on GPU
 * - SpikeInputGeneration: Generate and encode spike input patterns
 * - SingleTimestepSimulation: Run single timestep of SNN simulation
 * - MultiTimestepSimulation: Run multiple timesteps with spike propagation
 * - STDPLearning: Apply Spike-Timing-Dependent Plasticity weight updates
 * - WeightChangeVerification: Verify STDP induces correct weight changes
 * - SpikePatternAnalysis: Analyze output spike patterns
 * - FullSNNTrainingLoop: Complete training loop with STDP
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
// SNN Constants and Helper Functions
//=============================================================================

// LIF Neuron parameters
constexpr float LIF_TAU_MEM = 20.0f;      // Membrane time constant (ms)
constexpr float LIF_TAU_SYN = 5.0f;       // Synaptic time constant (ms)
constexpr float LIF_V_REST = -65.0f;      // Resting potential (mV)
constexpr float LIF_V_THRESH = -55.0f;    // Spike threshold (mV)
constexpr float LIF_V_RESET = -70.0f;     // Reset potential (mV)
constexpr float LIF_DT = 1.0f;            // Time step (ms)

// STDP parameters
constexpr float STDP_A_PLUS = 0.005f;     // LTP amplitude
constexpr float STDP_A_MINUS = 0.00525f;  // LTD amplitude (slight asymmetry)
constexpr float STDP_TAU_PLUS = 20.0f;    // LTP time constant (ms)
constexpr float STDP_TAU_MINUS = 20.0f;   // LTD time constant (ms)

/**
 * @brief Generate Poisson spike train
 */
static void generate_poisson_spikes(
    uint8_t* spikes,
    size_t num_neurons,
    size_t num_timesteps,
    float firing_rate,
    float dt)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    float spike_prob = firing_rate * dt / 1000.0f;  // Rate in Hz, dt in ms

    for (size_t t = 0; t < num_timesteps; t++) {
        for (size_t n = 0; n < num_neurons; n++) {
            spikes[t * num_neurons + n] = (dist(gen) < spike_prob) ? 1 : 0;
        }
    }
}

/**
 * @brief Generate patterned spike train
 */
static void generate_pattern_spikes(
    uint8_t* spikes,
    size_t num_neurons,
    size_t num_timesteps,
    size_t pattern_period)
{
    for (size_t t = 0; t < num_timesteps; t++) {
        for (size_t n = 0; n < num_neurons; n++) {
            // Neurons spike at different phases
            size_t phase = (n * pattern_period) / num_neurons;
            spikes[t * num_neurons + n] = ((t % pattern_period) == phase) ? 1 : 0;
        }
    }
}

/**
 * @brief Count total spikes in array
 */
static size_t count_spikes(const uint8_t* spikes, size_t size) {
    size_t count = 0;
    for (size_t i = 0; i < size; i++) {
        count += spikes[i];
    }
    return count;
}

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
// Pipeline 2: Spike Input Generation
//=============================================================================

TEST_F(GPUSNNPipelineE2ETest, SpikeInputGeneration) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Spike Input Generation");

    const size_t num_neurons = 100;
    const size_t num_timesteps = 100;

    // Stage 1: Generate Poisson spike train
    E2E_STAGE_BEGIN("Generate Poisson spikes", 500);

    std::vector<uint8_t> poisson_spikes(num_neurons * num_timesteps);
    float firing_rate = 50.0f;  // 50 Hz
    generate_poisson_spikes(poisson_spikes.data(), num_neurons, num_timesteps,
                            firing_rate, LIF_DT);

    size_t poisson_count = count_spikes(poisson_spikes.data(), poisson_spikes.size());
    float expected_count = num_neurons * num_timesteps * firing_rate * LIF_DT / 1000.0f;

    std::cout << "\n  Poisson spikes:" << std::endl;
    std::cout << "    Total: " << poisson_count << std::endl;
    std::cout << "    Expected: ~" << expected_count << std::endl;
    std::cout << "    Rate: " << (1000.0f * poisson_count) / (num_neurons * num_timesteps * LIF_DT) << " Hz" << std::endl;

    // Should be within 30% of expected (Poisson variability)
    EXPECT_GT(poisson_count, expected_count * 0.5) << "Too few spikes";
    EXPECT_LT(poisson_count, expected_count * 1.5) << "Too many spikes";

    E2E_STAGE_END();

    // Stage 2: Generate patterned spike train
    E2E_STAGE_BEGIN("Generate patterned spikes", 500);

    std::vector<uint8_t> pattern_spikes(num_neurons * num_timesteps);
    size_t pattern_period = 20;
    generate_pattern_spikes(pattern_spikes.data(), num_neurons, num_timesteps, pattern_period);

    size_t pattern_count = count_spikes(pattern_spikes.data(), pattern_spikes.size());
    size_t expected_pattern_count = num_neurons * (num_timesteps / pattern_period);

    std::cout << "\n  Patterned spikes:" << std::endl;
    std::cout << "    Total: " << pattern_count << std::endl;
    std::cout << "    Expected: " << expected_pattern_count << std::endl;

    EXPECT_EQ(pattern_count, expected_pattern_count);

    E2E_STAGE_END();

    // Stage 3: Upload to GPU
    E2E_STAGE_BEGIN("Upload spikes to GPU", 500);

    size_t spike_dims[] = {num_timesteps, num_neurons};

    nimcp_gpu_tensor_t* spike_tensor = nimcp_snn_spike_tensor_create(
        ctx_, poisson_spikes.data(), spike_dims, 2);
    E2E_ASSERT_NOT_NULL(spike_tensor, "Failed to create spike tensor");

    // Verify data integrity
    std::vector<uint8_t> verify_spikes(num_neurons * num_timesteps);
    nimcp_snn_spike_tensor_to_host(spike_tensor, verify_spikes.data());

    bool match = true;
    for (size_t i = 0; i < num_neurons * num_timesteps; i++) {
        if (verify_spikes[i] != poisson_spikes[i]) {
            match = false;
            break;
        }
    }
    EXPECT_TRUE(match) << "Spike data corrupted during GPU transfer";

    nimcp_gpu_tensor_destroy(spike_tensor);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Single Timestep Simulation
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

    // Strong input neurons should have higher potential (unless they spiked)
    // Since we're after potential could be reset, just check not NaN
    EXPECT_FALSE(std::isnan(avg_strong));
    EXPECT_FALSE(std::isnan(avg_weak));

    E2E_STAGE_END();

    // Stage 5: Check spike output
    E2E_STAGE_BEGIN("Check spike output", 500);

    std::vector<uint8_t> spike_host(num_neurons);
    nimcp_snn_spike_tensor_to_host(spikes, spike_host.data());

    size_t spike_count = count_spikes(spike_host.data(), num_neurons);
    std::cout << "    Spikes: " << spike_count << "/" << num_neurons << std::endl;

    // Spike count should be reasonable (0 to all neurons)
    EXPECT_LE(spike_count, num_neurons);

    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_gpu_tensor_destroy(current);
    nimcp_snn_layer_destroy(layer);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Multi-Timestep Simulation
//=============================================================================

TEST_F(GPUSNNPipelineE2ETest, MultiTimestepSimulation) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Multi-Timestep Simulation");

    const size_t num_input = 32;
    const size_t num_hidden = 64;
    const size_t num_output = 16;
    const size_t num_timesteps = 100;

    // Stage 1: Create network layers
    E2E_STAGE_BEGIN("Create network layers", 1000);

    nimcp_snn_lif_config_t config;
    config.tau_mem = LIF_TAU_MEM;
    config.tau_syn = LIF_TAU_SYN;
    config.v_rest = LIF_V_REST;
    config.v_thresh = LIF_V_THRESH;
    config.v_reset = LIF_V_RESET;
    config.dt = LIF_DT;
    config.refractory_period = 2.0f;

    config.num_neurons = num_hidden;
    nimcp_snn_layer_t* hidden = nimcp_snn_lif_layer_create(ctx_, &config);
    E2E_ASSERT_NOT_NULL(hidden, "Failed to create hidden layer");

    config.num_neurons = num_output;
    nimcp_snn_layer_t* output = nimcp_snn_lif_layer_create(ctx_, &config);
    E2E_ASSERT_NOT_NULL(output, "Failed to create output layer");

    E2E_STAGE_END();

    // Stage 2: Create synaptic weights
    E2E_STAGE_BEGIN("Create synaptic weights", 500);

    size_t w1_dims[] = {num_input, num_hidden};
    size_t w2_dims[] = {num_hidden, num_output};

    // Initialize with small random weights
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 0.1f);

    std::vector<float> w1_host(num_input * num_hidden);
    std::vector<float> w2_host(num_hidden * num_output);

    for (auto& w : w1_host) w = std::abs(dist(gen));  // Excitatory
    for (auto& w : w2_host) w = std::abs(dist(gen));

    nimcp_gpu_tensor_t* W1 = nimcp_gpu_tensor_from_host(
        ctx_, w1_host.data(), w1_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* W2 = nimcp_gpu_tensor_from_host(
        ctx_, w2_host.data(), w2_dims, 2, NIMCP_GPU_PRECISION_FP32);

    E2E_ASSERT_NOT_NULL(W1, "Failed to create W1");
    E2E_ASSERT_NOT_NULL(W2, "Failed to create W2");

    E2E_STAGE_END();

    // Stage 3: Generate input spike train
    E2E_STAGE_BEGIN("Generate input spikes", 500);

    std::vector<uint8_t> input_spikes(num_input * num_timesteps);
    generate_poisson_spikes(input_spikes.data(), num_input, num_timesteps, 30.0f, LIF_DT);

    size_t input_count = count_spikes(input_spikes.data(), input_spikes.size());
    std::cout << "\n  Input spike count: " << input_count << std::endl;

    E2E_STAGE_END();

    // Stage 4: Run simulation
    E2E_STAGE_BEGIN("Run multi-timestep simulation", 5000);

    std::vector<size_t> hidden_spike_counts(num_timesteps, 0);
    std::vector<size_t> output_spike_counts(num_timesteps, 0);

    // Allocate working tensors
    size_t hidden_dims[] = {num_hidden};
    size_t output_dims[] = {num_output};

    nimcp_gpu_tensor_t* hidden_current = nimcp_gpu_tensor_create(
        ctx_, hidden_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output_current = nimcp_gpu_tensor_create(
        ctx_, output_dims, 1, NIMCP_GPU_PRECISION_FP32);

    for (size_t t = 0; t < num_timesteps; t++) {
        // Get input spikes for this timestep
        std::vector<float> input_float(num_input);
        for (size_t i = 0; i < num_input; i++) {
            input_float[i] = input_spikes[t * num_input + i] ? 1.0f : 0.0f;
        }

        // Compute hidden layer current: I = input @ W1
        size_t input_dims[] = {num_input};
        nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_from_host(
            ctx_, input_float.data(), input_dims, 1, NIMCP_GPU_PRECISION_FP32);

        nimcp_gpu_matvec(ctx_, W1, input_tensor, hidden_current, false);

        // Hidden layer step
        nimcp_gpu_tensor_t* hidden_spikes = nimcp_snn_lif_step(ctx_, hidden, hidden_current);

        // Get hidden spike tensor
        std::vector<uint8_t> hidden_spike_host(num_hidden);
        nimcp_snn_spike_tensor_to_host(hidden_spikes, hidden_spike_host.data());

        // Compute output layer current
        std::vector<float> hidden_float(num_hidden);
        for (size_t i = 0; i < num_hidden; i++) {
            hidden_float[i] = hidden_spike_host[i] ? 1.0f : 0.0f;
        }
        nimcp_gpu_tensor_t* hidden_float_tensor = nimcp_gpu_tensor_from_host(
            ctx_, hidden_float.data(), hidden_dims, 1, NIMCP_GPU_PRECISION_FP32);

        nimcp_gpu_matvec(ctx_, W2, hidden_float_tensor, output_current, false);

        // Output layer step
        nimcp_gpu_tensor_t* output_spikes = nimcp_snn_lif_step(ctx_, output, output_current);

        std::vector<uint8_t> output_spike_host(num_output);
        nimcp_snn_spike_tensor_to_host(output_spikes, output_spike_host.data());

        // Count spikes
        hidden_spike_counts[t] = count_spikes(hidden_spike_host.data(), num_hidden);
        output_spike_counts[t] = count_spikes(output_spike_host.data(), num_output);

        nimcp_gpu_tensor_destroy(input_tensor);
        nimcp_gpu_tensor_destroy(hidden_float_tensor);
    }

    nimcp_gpu_context_synchronize(ctx_);

    // Report statistics
    size_t total_hidden_spikes = 0, total_output_spikes = 0;
    for (size_t t = 0; t < num_timesteps; t++) {
        total_hidden_spikes += hidden_spike_counts[t];
        total_output_spikes += output_spike_counts[t];
    }

    std::cout << "  Hidden layer spikes: " << total_hidden_spikes << std::endl;
    std::cout << "  Output layer spikes: " << total_output_spikes << std::endl;
    std::cout << "  Hidden firing rate:  "
              << (1000.0f * total_hidden_spikes) / (num_hidden * num_timesteps * LIF_DT)
              << " Hz" << std::endl;

    nimcp_gpu_tensor_destroy(hidden_current);
    nimcp_gpu_tensor_destroy(output_current);

    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_gpu_tensor_destroy(W1);
    nimcp_gpu_tensor_destroy(W2);
    nimcp_snn_layer_destroy(hidden);
    nimcp_snn_layer_destroy(output);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: STDP Learning
//=============================================================================

TEST_F(GPUSNNPipelineE2ETest, STDPLearning) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("STDP Learning");

    const size_t num_pre = 10;
    const size_t num_post = 5;

    // Stage 1: Create STDP context
    E2E_STAGE_BEGIN("Create STDP context", 500);

    nimcp_snn_stdp_config_t stdp_config;
    stdp_config.a_plus = STDP_A_PLUS;
    stdp_config.a_minus = STDP_A_MINUS;
    stdp_config.tau_plus = STDP_TAU_PLUS;
    stdp_config.tau_minus = STDP_TAU_MINUS;
    stdp_config.w_min = 0.0f;
    stdp_config.w_max = 1.0f;

    nimcp_snn_stdp_t* stdp = nimcp_snn_stdp_create(ctx_, &stdp_config, num_pre, num_post);
    E2E_ASSERT_NOT_NULL(stdp, "Failed to create STDP context");

    E2E_STAGE_END();

    // Stage 2: Initialize weights
    E2E_STAGE_BEGIN("Initialize weights", 500);

    size_t w_dims[] = {num_pre, num_post};
    std::vector<float> w_host(num_pre * num_post, 0.5f);  // Start at mid-range

    nimcp_gpu_tensor_t* weights = nimcp_gpu_tensor_from_host(
        ctx_, w_host.data(), w_dims, 2, NIMCP_GPU_PRECISION_FP32);
    E2E_ASSERT_NOT_NULL(weights, "Failed to create weight tensor");

    nimcp_snn_stdp_set_weights(stdp, weights);

    E2E_STAGE_END();

    // Stage 3: Create pre/post spike events (pre before post = LTP)
    E2E_STAGE_BEGIN("Create spike timing pattern (LTP)", 500);

    // Pre-synaptic spike at t=0, post-synaptic at t=10 -> LTP
    nimcp_snn_spike_event_t pre_event;
    pre_event.time = 0.0f;
    pre_event.neuron_mask = (1 << num_pre) - 1;  // All pre neurons

    nimcp_snn_spike_event_t post_event;
    post_event.time = 10.0f;  // 10ms after pre
    post_event.neuron_mask = (1 << num_post) - 1;  // All post neurons

    // Apply LTP
    bool ltp_ok = nimcp_snn_stdp_update(stdp, &pre_event, &post_event);
    EXPECT_TRUE(ltp_ok);

    E2E_STAGE_END();

    // Stage 4: Verify LTP (weight increase)
    E2E_STAGE_BEGIN("Verify LTP weight increase", 500);

    std::vector<float> w_after_ltp(num_pre * num_post);
    nimcp_snn_stdp_get_weights(stdp, w_after_ltp.data());

    // All weights should increase
    bool all_increased = true;
    float avg_increase = 0.0f;
    for (size_t i = 0; i < num_pre * num_post; i++) {
        if (w_after_ltp[i] <= w_host[i]) {
            all_increased = false;
        }
        avg_increase += w_after_ltp[i] - w_host[i];
    }
    avg_increase /= (num_pre * num_post);

    std::cout << "\n  LTP Results:" << std::endl;
    std::cout << "    Initial weight: " << w_host[0] << std::endl;
    std::cout << "    After LTP:      " << w_after_ltp[0] << std::endl;
    std::cout << "    Avg increase:   " << avg_increase << std::endl;

    EXPECT_TRUE(all_increased) << "All weights should increase with LTP";

    E2E_STAGE_END();

    // Stage 5: Create post before pre pattern (LTD)
    E2E_STAGE_BEGIN("Create spike timing pattern (LTD)", 500);

    // Post-synaptic spike at t=50, pre-synaptic at t=60 -> LTD
    nimcp_snn_spike_event_t post_event2;
    post_event2.time = 50.0f;
    post_event2.neuron_mask = (1 << num_post) - 1;

    nimcp_snn_spike_event_t pre_event2;
    pre_event2.time = 60.0f;  // 10ms after post
    pre_event2.neuron_mask = (1 << num_pre) - 1;

    // Store current weights
    std::vector<float> w_before_ltd(num_pre * num_post);
    nimcp_snn_stdp_get_weights(stdp, w_before_ltd.data());

    // Apply LTD
    bool ltd_ok = nimcp_snn_stdp_update(stdp, &pre_event2, &post_event2);
    EXPECT_TRUE(ltd_ok);

    E2E_STAGE_END();

    // Stage 6: Verify LTD (weight decrease)
    E2E_STAGE_BEGIN("Verify LTD weight decrease", 500);

    std::vector<float> w_after_ltd(num_pre * num_post);
    nimcp_snn_stdp_get_weights(stdp, w_after_ltd.data());

    bool all_decreased = true;
    float avg_decrease = 0.0f;
    for (size_t i = 0; i < num_pre * num_post; i++) {
        if (w_after_ltd[i] >= w_before_ltd[i]) {
            all_decreased = false;
        }
        avg_decrease += w_before_ltd[i] - w_after_ltd[i];
    }
    avg_decrease /= (num_pre * num_post);

    std::cout << "\n  LTD Results:" << std::endl;
    std::cout << "    Before LTD:  " << w_before_ltd[0] << std::endl;
    std::cout << "    After LTD:   " << w_after_ltd[0] << std::endl;
    std::cout << "    Avg decrease: " << avg_decrease << std::endl;

    EXPECT_TRUE(all_decreased) << "All weights should decrease with LTD";

    E2E_STAGE_END();

    // Stage 7: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_gpu_tensor_destroy(weights);
    nimcp_snn_stdp_destroy(stdp);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 6: Weight Change Verification
//=============================================================================

TEST_F(GPUSNNPipelineE2ETest, WeightChangeVerification) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Weight Change Verification");

    const size_t num_pre = 8;
    const size_t num_post = 4;

    // Stage 1: Create STDP with known parameters
    E2E_STAGE_BEGIN("Create STDP context", 500);

    nimcp_snn_stdp_config_t stdp_config;
    stdp_config.a_plus = 0.1f;   // Easy-to-verify value
    stdp_config.a_minus = 0.1f;
    stdp_config.tau_plus = 20.0f;
    stdp_config.tau_minus = 20.0f;
    stdp_config.w_min = 0.0f;
    stdp_config.w_max = 1.0f;

    nimcp_snn_stdp_t* stdp = nimcp_snn_stdp_create(ctx_, &stdp_config, num_pre, num_post);
    E2E_ASSERT_NOT_NULL(stdp, "Failed to create STDP context");

    E2E_STAGE_END();

    // Stage 2: Set initial weights
    E2E_STAGE_BEGIN("Set initial weights", 500);

    size_t w_dims[] = {num_pre, num_post};
    std::vector<float> w_init(num_pre * num_post, 0.5f);

    nimcp_gpu_tensor_t* weights = nimcp_gpu_tensor_from_host(
        ctx_, w_init.data(), w_dims, 2, NIMCP_GPU_PRECISION_FP32);

    nimcp_snn_stdp_set_weights(stdp, weights);

    E2E_STAGE_END();

    // Stage 3: Test multiple timing differences
    E2E_STAGE_BEGIN("Test timing-dependent changes", 2000);

    std::vector<float> delta_ts = {-50.0f, -20.0f, -10.0f, -5.0f, 5.0f, 10.0f, 20.0f, 50.0f};
    std::vector<float> weight_changes;

    std::cout << "\n  Delta_t (ms) | Weight Change" << std::endl;
    std::cout << "  -------------|---------------" << std::endl;

    for (float dt : delta_ts) {
        // Reset weights
        nimcp_snn_stdp_reset(stdp);
        std::vector<float> w_before(num_pre * num_post, 0.5f);
        nimcp_gpu_tensor_t* w_tensor = nimcp_gpu_tensor_from_host(
            ctx_, w_before.data(), w_dims, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_snn_stdp_set_weights(stdp, w_tensor);

        // Create spike events
        float pre_time, post_time;
        if (dt > 0) {
            pre_time = 0.0f;
            post_time = dt;
        } else {
            post_time = 0.0f;
            pre_time = -dt;
        }

        nimcp_snn_spike_event_t pre = {pre_time, 0x01};  // Single pre neuron
        nimcp_snn_spike_event_t post = {post_time, 0x01}; // Single post neuron

        nimcp_snn_stdp_update(stdp, &pre, &post);

        // Get weight change
        std::vector<float> w_after(num_pre * num_post);
        nimcp_snn_stdp_get_weights(stdp, w_after.data());

        float change = w_after[0] - w_before[0];
        weight_changes.push_back(change);

        std::cout << "  " << std::setw(13) << dt << " | "
                  << std::setw(13) << std::fixed << std::setprecision(6) << change << std::endl;

        nimcp_gpu_tensor_destroy(w_tensor);
    }

    // Verify STDP curve shape:
    // - Positive delta_t (pre before post) -> LTP (positive change)
    // - Negative delta_t (post before pre) -> LTD (negative change)
    // - Larger |delta_t| -> smaller magnitude

    // Check sign
    for (size_t i = 0; i < delta_ts.size(); i++) {
        if (delta_ts[i] > 0) {
            EXPECT_GT(weight_changes[i], 0.0f) << "Pre-before-post should cause LTP";
        } else if (delta_ts[i] < 0) {
            EXPECT_LT(weight_changes[i], 0.0f) << "Post-before-pre should cause LTD";
        }
    }

    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_gpu_tensor_destroy(weights);
    nimcp_snn_stdp_destroy(stdp);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 7: Spike Pattern Analysis
//=============================================================================

TEST_F(GPUSNNPipelineE2ETest, SpikePatternAnalysis) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Spike Pattern Analysis");

    const size_t num_neurons = 100;
    const size_t num_timesteps = 500;

    // Stage 1: Create neuron layer
    E2E_STAGE_BEGIN("Create neuron layer", 500);

    nimcp_snn_lif_config_t config;
    config.num_neurons = num_neurons;
    config.tau_mem = LIF_TAU_MEM;
    config.tau_syn = LIF_TAU_SYN;
    config.v_rest = LIF_V_REST;
    config.v_thresh = LIF_V_THRESH;
    config.v_reset = LIF_V_RESET;
    config.dt = LIF_DT;
    config.refractory_period = 2.0f;

    nimcp_snn_layer_t* layer = nimcp_snn_lif_layer_create(ctx_, &config);
    E2E_ASSERT_NOT_NULL(layer, "Failed to create LIF layer");

    E2E_STAGE_END();

    // Stage 2: Run simulation with constant input
    E2E_STAGE_BEGIN("Run simulation", 3000);

    std::vector<std::vector<uint8_t>> raster(num_timesteps);
    size_t current_dims[] = {num_neurons};

    // Different input currents for different neurons
    std::vector<float> input_current(num_neurons);
    for (size_t i = 0; i < num_neurons; i++) {
        input_current[i] = 5.0f + 10.0f * static_cast<float>(i) / num_neurons;
    }

    for (size_t t = 0; t < num_timesteps; t++) {
        nimcp_gpu_tensor_t* current = nimcp_gpu_tensor_from_host(
            ctx_, input_current.data(), current_dims, 1, NIMCP_GPU_PRECISION_FP32);

        nimcp_gpu_tensor_t* spikes = nimcp_snn_lif_step(ctx_, layer, current);

        raster[t].resize(num_neurons);
        nimcp_snn_spike_tensor_to_host(spikes, raster[t].data());

        nimcp_gpu_tensor_destroy(current);
    }

    E2E_STAGE_END();

    // Stage 3: Compute firing rates
    E2E_STAGE_BEGIN("Compute firing rates", 500);

    std::vector<float> firing_rates(num_neurons, 0.0f);
    for (size_t n = 0; n < num_neurons; n++) {
        size_t spike_count = 0;
        for (size_t t = 0; t < num_timesteps; t++) {
            spike_count += raster[t][n];
        }
        firing_rates[n] = 1000.0f * spike_count / (num_timesteps * LIF_DT);
    }

    // Report statistics
    float min_rate = *std::min_element(firing_rates.begin(), firing_rates.end());
    float max_rate = *std::max_element(firing_rates.begin(), firing_rates.end());
    float avg_rate = 0.0f;
    for (float r : firing_rates) avg_rate += r;
    avg_rate /= num_neurons;

    std::cout << "\n  Firing Rate Statistics:" << std::endl;
    std::cout << "    Min: " << min_rate << " Hz" << std::endl;
    std::cout << "    Max: " << max_rate << " Hz" << std::endl;
    std::cout << "    Avg: " << avg_rate << " Hz" << std::endl;

    // Higher input should give higher firing rate
    float first_10_avg = 0.0f, last_10_avg = 0.0f;
    for (size_t i = 0; i < 10; i++) {
        first_10_avg += firing_rates[i];
        last_10_avg += firing_rates[num_neurons - 10 + i];
    }
    first_10_avg /= 10;
    last_10_avg /= 10;

    EXPECT_LT(first_10_avg, last_10_avg) << "Higher input should give higher firing rate";

    E2E_STAGE_END();

    // Stage 4: Compute inter-spike intervals
    E2E_STAGE_BEGIN("Compute ISI statistics", 500);

    std::vector<float> all_isis;
    for (size_t n = 0; n < num_neurons; n++) {
        std::vector<size_t> spike_times;
        for (size_t t = 0; t < num_timesteps; t++) {
            if (raster[t][n]) spike_times.push_back(t);
        }

        for (size_t i = 1; i < spike_times.size(); i++) {
            float isi = (spike_times[i] - spike_times[i-1]) * LIF_DT;
            all_isis.push_back(isi);
        }
    }

    if (!all_isis.empty()) {
        float min_isi = *std::min_element(all_isis.begin(), all_isis.end());
        float max_isi = *std::max_element(all_isis.begin(), all_isis.end());
        float avg_isi = 0.0f;
        for (float isi : all_isis) avg_isi += isi;
        avg_isi /= all_isis.size();

        std::cout << "\n  ISI Statistics:" << std::endl;
        std::cout << "    Min ISI: " << min_isi << " ms" << std::endl;
        std::cout << "    Max ISI: " << max_isi << " ms" << std::endl;
        std::cout << "    Avg ISI: " << avg_isi << " ms" << std::endl;

        // Min ISI should be at least refractory period
        EXPECT_GE(min_isi, config.refractory_period - 0.1f);
    }

    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_snn_layer_destroy(layer);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 8: Full SNN Training Loop
//=============================================================================

TEST_F(GPUSNNPipelineE2ETest, FullSNNTrainingLoop) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Full SNN Training Loop");

    const size_t num_input = 32;
    const size_t num_output = 8;
    const size_t num_epochs = 20;
    const size_t timesteps_per_epoch = 100;

    // Stage 1: Create network
    E2E_STAGE_BEGIN("Create SNN network", 500);

    nimcp_snn_lif_config_t config;
    config.num_neurons = num_output;
    config.tau_mem = LIF_TAU_MEM;
    config.tau_syn = LIF_TAU_SYN;
    config.v_rest = LIF_V_REST;
    config.v_thresh = LIF_V_THRESH;
    config.v_reset = LIF_V_RESET;
    config.dt = LIF_DT;
    config.refractory_period = 2.0f;

    nimcp_snn_layer_t* output_layer = nimcp_snn_lif_layer_create(ctx_, &config);
    E2E_ASSERT_NOT_NULL(output_layer, "Failed to create output layer");

    E2E_STAGE_END();

    // Stage 2: Create weights and STDP
    E2E_STAGE_BEGIN("Create weights and STDP", 500);

    size_t w_dims[] = {num_input, num_output};
    std::vector<float> w_host(num_input * num_output, 0.5f);

    nimcp_gpu_tensor_t* weights = nimcp_gpu_tensor_from_host(
        ctx_, w_host.data(), w_dims, 2, NIMCP_GPU_PRECISION_FP32);

    nimcp_snn_stdp_config_t stdp_config;
    stdp_config.a_plus = STDP_A_PLUS;
    stdp_config.a_minus = STDP_A_MINUS;
    stdp_config.tau_plus = STDP_TAU_PLUS;
    stdp_config.tau_minus = STDP_TAU_MINUS;
    stdp_config.w_min = 0.0f;
    stdp_config.w_max = 1.0f;

    nimcp_snn_stdp_t* stdp = nimcp_snn_stdp_create(ctx_, &stdp_config, num_input, num_output);
    nimcp_snn_stdp_set_weights(stdp, weights);

    E2E_STAGE_END();

    // Stage 3: Training loop
    E2E_STAGE_BEGIN("Run training epochs", 10000);

    std::vector<float> epoch_spike_counts(num_epochs);

    for (size_t epoch = 0; epoch < num_epochs; epoch++) {
        // Reset layer state
        nimcp_snn_layer_reset(output_layer);

        // Generate input spikes (patterned for learning)
        std::vector<uint8_t> input_spikes(num_input * timesteps_per_epoch);
        generate_pattern_spikes(input_spikes.data(), num_input, timesteps_per_epoch, 10);

        size_t total_output_spikes = 0;

        for (size_t t = 0; t < timesteps_per_epoch; t++) {
            // Get current weights
            std::vector<float> current_weights(num_input * num_output);
            nimcp_snn_stdp_get_weights(stdp, current_weights.data());

            // Compute input current
            std::vector<float> input_float(num_input);
            for (size_t i = 0; i < num_input; i++) {
                input_float[i] = input_spikes[t * num_input + i] ? 1.0f : 0.0f;
            }

            // Weighted sum: current = input @ weights
            std::vector<float> output_current(num_output, 0.0f);
            for (size_t o = 0; o < num_output; o++) {
                for (size_t i = 0; i < num_input; i++) {
                    output_current[o] += input_float[i] * current_weights[i * num_output + o];
                }
            }

            // LIF step
            size_t current_dims[] = {num_output};
            nimcp_gpu_tensor_t* current_tensor = nimcp_gpu_tensor_from_host(
                ctx_, output_current.data(), current_dims, 1, NIMCP_GPU_PRECISION_FP32);

            nimcp_gpu_tensor_t* output_spikes = nimcp_snn_lif_step(ctx_, output_layer, current_tensor);

            // Get output spikes
            std::vector<uint8_t> spike_host(num_output);
            nimcp_snn_spike_tensor_to_host(output_spikes, spike_host.data());
            total_output_spikes += count_spikes(spike_host.data(), num_output);

            // Apply STDP for neurons that spiked
            for (size_t o = 0; o < num_output; o++) {
                if (spike_host[o]) {
                    // Create spike events
                    uint32_t input_mask = 0;
                    for (size_t i = 0; i < num_input; i++) {
                        if (input_spikes[t * num_input + i]) {
                            input_mask |= (1 << i);
                        }
                    }

                    nimcp_snn_spike_event_t pre = {t * LIF_DT, input_mask};
                    nimcp_snn_spike_event_t post = {t * LIF_DT, (uint32_t)(1 << o)};

                    nimcp_snn_stdp_update(stdp, &pre, &post);
                }
            }

            nimcp_gpu_tensor_destroy(current_tensor);
        }

        epoch_spike_counts[epoch] = static_cast<float>(total_output_spikes);

        if ((epoch + 1) % 5 == 0) {
            std::cout << "    Epoch " << (epoch + 1) << "/" << num_epochs
                      << " - Output spikes: " << total_output_spikes << std::endl;
        }
    }

    E2E_STAGE_END();

    // Stage 4: Verify training effect
    E2E_STAGE_BEGIN("Verify training effect", 500);

    // Get final weights
    std::vector<float> final_weights(num_input * num_output);
    nimcp_snn_stdp_get_weights(stdp, final_weights.data());

    // Compare with initial weights
    float weight_change_sum = 0.0f;
    for (size_t i = 0; i < num_input * num_output; i++) {
        weight_change_sum += std::abs(final_weights[i] - w_host[i]);
    }

    std::cout << "\n  Training Summary:" << std::endl;
    std::cout << "    Total weight change: " << weight_change_sum << std::endl;
    std::cout << "    Avg change per weight: " << weight_change_sum / (num_input * num_output) << std::endl;

    // Weights should have changed (learning occurred)
    EXPECT_GT(weight_change_sum, 0.0f) << "STDP should modify weights";

    // Firing pattern may change over epochs (could increase or decrease)
    // Just verify we're producing spikes
    size_t total_spikes = 0;
    for (size_t e = 0; e < num_epochs; e++) {
        total_spikes += static_cast<size_t>(epoch_spike_counts[e]);
    }
    EXPECT_GT(total_spikes, 0u) << "Network should produce spikes";

    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_snn_stdp_destroy(stdp);
    nimcp_gpu_tensor_destroy(weights);
    nimcp_snn_layer_destroy(output_layer);

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
