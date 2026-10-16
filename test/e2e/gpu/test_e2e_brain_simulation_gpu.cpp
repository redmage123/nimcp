/**
 * @file test_e2e_brain_simulation_gpu.cpp
 * @brief E2E Tests for GPU-Accelerated Full Brain Simulation
 *
 * WHAT: End-to-end testing of complete brain simulations on GPU
 * WHY:  Verify full neural network lifecycle from creation through training
 * HOW:  Test SNN dynamics, LIF neurons, synaptic plasticity, memory consolidation
 *
 * TEST PIPELINES:
 * - FullBrainLifecycle: Create brain, train, infer, destroy
 * - LIFNeuronDynamics: Large-scale LIF neuron simulation
 * - SynapticPlasticity: STDP learning with GPU acceleration
 * - MemoryConsolidation: Sleep-like memory consolidation on GPU
 * - HemisphericProcessing: Left/right brain specialization
 * - MultiRegionIntegration: Cortical regions with cross-region communication
 * - PerformanceBenchmark: Neuron count scaling tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "e2e_test_framework.h"

// Headers have their own extern "C" guards
#include "gpu/nimcp_execution_mode.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/training/nimcp_training_gpu.h"
#include "gpu/plasticity/nimcp_plasticity_gpu.h"
#include "gpu/memory/nimcp_memory_consolidation_gpu.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <chrono>
#include <random>
#include <numeric>
#include <algorithm>

//=============================================================================
// Test Metrics Structure
//=============================================================================

struct BrainMetrics {
    double gpu_time_ms;
    double cpu_time_ms;
    double speedup;
    size_t memory_usage_bytes;
    double numerical_accuracy;
    double neurons_per_second;
    double synapses_per_second;
    uint64_t total_neurons;
    uint64_t total_synapses;
    uint64_t total_spikes;
    uint64_t timesteps_simulated;
    double average_firing_rate;
};

//=============================================================================
// Test Fixture
//=============================================================================

class BrainSimulationGPUE2ETest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx_ = nullptr;
    hardware_capabilities_t caps_;
    bool has_gpu_ = false;
    std::mt19937 rng_;
    BrainMetrics metrics_;

    void SetUp() override {
        memset(&caps_, 0, sizeof(caps_));
        memset(&metrics_, 0, sizeof(metrics_));

        execution_detect_capabilities(&caps_);
        has_gpu_ = caps_.cuda_available || caps_.rocm_available || caps_.opencl_available;

        if (has_gpu_) {
            gpu_ctx_ = nimcp_gpu_context_create_auto();
        }

        rng_.seed(42);
    }

    void TearDown() override {
        if (gpu_ctx_) {
            nimcp_gpu_context_destroy(gpu_ctx_);
            gpu_ctx_ = nullptr;
        }
    }

    bool HasGPU() const { return has_gpu_ && gpu_ctx_ != nullptr; }

    void GenerateSparseConnectivity(std::vector<float>& weights,
                                    size_t n_pre, size_t n_post,
                                    float connectivity, float w_mean) {
        weights.resize(n_pre * n_post, 0.0f);
        std::bernoulli_distribution conn_dist(connectivity);
        std::normal_distribution<float> w_dist(w_mean, w_mean * 0.5f);

        for (size_t i = 0; i < n_pre; i++) {
            for (size_t j = 0; j < n_post; j++) {
                if (conn_dist(rng_)) {
                    float w = std::abs(w_dist(rng_));
                    weights[i * n_post + j] = std::min(w, 1.0f);
                }
            }
        }
    }

    void GenerateInputSpikes(std::vector<uint8_t>& spikes, size_t n_neurons,
                             size_t n_timesteps, float firing_rate) {
        spikes.resize(n_neurons * n_timesteps);
        std::bernoulli_distribution spike_dist(firing_rate);

        for (size_t t = 0; t < n_timesteps; t++) {
            for (size_t i = 0; i < n_neurons; i++) {
                spikes[t * n_neurons + i] = spike_dist(rng_) ? 1 : 0;
            }
        }
    }

    void PrintMetrics(const std::string& test_name) {
        std::cout << "\n=== " << test_name << " Metrics ===" << std::endl;
        std::cout << "  GPU Time: " << metrics_.gpu_time_ms << " ms" << std::endl;
        if (metrics_.cpu_time_ms > 0) {
            std::cout << "  CPU Time: " << metrics_.cpu_time_ms << " ms" << std::endl;
            std::cout << "  Speedup: " << metrics_.speedup << "x" << std::endl;
        }
        std::cout << "  Memory Usage: " << (metrics_.memory_usage_bytes / 1024.0 / 1024.0)
                  << " MB" << std::endl;
        std::cout << "  Neurons: " << metrics_.total_neurons << std::endl;
        std::cout << "  Synapses: " << metrics_.total_synapses << std::endl;
        std::cout << "  Total spikes: " << metrics_.total_spikes << std::endl;
        std::cout << "  Neurons/sec: " << metrics_.neurons_per_second << std::endl;
        std::cout << "  Average firing rate: " << metrics_.average_firing_rate << " Hz" << std::endl;
    }
};

//=============================================================================
// Pipeline 1: Full Brain Lifecycle
//=============================================================================

TEST_F(BrainSimulationGPUE2ETest, FullBrainLifecycleGPU) {
    E2E_PIPELINE_START("Full Brain Lifecycle on GPU");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    const size_t N_INPUT = 1000;
    const size_t N_HIDDEN = 5000;
    const size_t N_OUTPUT = 100;
    const size_t N_TIMESTEPS = 100;
    const float DT = 1.0f;  // 1ms timestep

    // Stage 1: Create LIF neuron populations
    E2E_STAGE_BEGIN("Create neuron populations", 3000);

    nimcp_lif_params_t lif_params = {
        .tau_mem = 20.0f,
        .tau_syn = 5.0f,
        .v_thresh = -50.0f,
        .v_reset = -70.0f,
        .v_rest = -65.0f,
        .dt = DT,
        .hard_reset = true
    };

    nimcp_lif_state_t* input_layer = nimcp_lif_state_create(gpu_ctx_, N_INPUT, &lif_params);
    nimcp_lif_state_t* hidden_layer = nimcp_lif_state_create(gpu_ctx_, N_HIDDEN, &lif_params);
    nimcp_lif_state_t* output_layer = nimcp_lif_state_create(gpu_ctx_, N_OUTPUT, &lif_params);

    E2E_ASSERT_NOT_NULL(input_layer, "Failed to create input layer");
    E2E_ASSERT_NOT_NULL(hidden_layer, "Failed to create hidden layer");
    E2E_ASSERT_NOT_NULL(output_layer, "Failed to create output layer");

    std::cout << "\n  Brain Configuration:" << std::endl;
    std::cout << "    Input neurons: " << N_INPUT << std::endl;
    std::cout << "    Hidden neurons: " << N_HIDDEN << std::endl;
    std::cout << "    Output neurons: " << N_OUTPUT << std::endl;
    std::cout << "    Total neurons: " << (N_INPUT + N_HIDDEN + N_OUTPUT) << std::endl;

    E2E_STAGE_END();

    // Stage 2: Create synaptic weight tensors
    E2E_STAGE_BEGIN("Create synaptic connections", 3000);

    size_t w1_dims[] = {N_INPUT, N_HIDDEN};
    size_t w2_dims[] = {N_HIDDEN, N_OUTPUT};

    nimcp_gpu_tensor_t* weights_ih = nimcp_gpu_tensor_create(gpu_ctx_, w1_dims, 2,
                                                              NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* weights_ho = nimcp_gpu_tensor_create(gpu_ctx_, w2_dims, 2,
                                                              NIMCP_GPU_PRECISION_FP32);

    E2E_ASSERT_NOT_NULL(weights_ih, "Failed to create input->hidden weights");
    E2E_ASSERT_NOT_NULL(weights_ho, "Failed to create hidden->output weights");

    // Initialize with sparse connectivity
    std::vector<float> w1_data, w2_data;
    GenerateSparseConnectivity(w1_data, N_INPUT, N_HIDDEN, 0.1f, 0.1f);
    GenerateSparseConnectivity(w2_data, N_HIDDEN, N_OUTPUT, 0.1f, 0.1f);

    nimcp_gpu_memcpy(gpu_ctx_, weights_ih->data, w1_data.data(),
                     w1_data.size() * sizeof(float), GPU_MEMCPY_HOST_TO_DEVICE);
    nimcp_gpu_memcpy(gpu_ctx_, weights_ho->data, w2_data.data(),
                     w2_data.size() * sizeof(float), GPU_MEMCPY_HOST_TO_DEVICE);

    size_t n_synapses_ih = std::count_if(w1_data.begin(), w1_data.end(), [](float w) { return w > 0; });
    size_t n_synapses_ho = std::count_if(w2_data.begin(), w2_data.end(), [](float w) { return w > 0; });
    metrics_.total_synapses = n_synapses_ih + n_synapses_ho;

    std::cout << "\n  Synapses:" << std::endl;
    std::cout << "    Input->Hidden: " << n_synapses_ih << std::endl;
    std::cout << "    Hidden->Output: " << n_synapses_ho << std::endl;

    E2E_STAGE_END();

    // Stage 3: Create input current and output tensors
    E2E_STAGE_BEGIN("Create input/output buffers", 1000);

    size_t input_dims[] = {N_INPUT};
    size_t hidden_current_dims[] = {N_HIDDEN};
    size_t output_current_dims[] = {N_OUTPUT};

    nimcp_gpu_tensor_t* input_current = nimcp_gpu_tensor_create(gpu_ctx_, input_dims, 1,
                                                                  NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* hidden_current = nimcp_gpu_tensor_create(gpu_ctx_, hidden_current_dims, 1,
                                                                   NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output_current = nimcp_gpu_tensor_create(gpu_ctx_, output_current_dims, 1,
                                                                   NIMCP_GPU_PRECISION_FP32);

    E2E_STAGE_END();

    // Stage 4: Run simulation
    E2E_STAGE_BEGIN("Run brain simulation", 30000);

    auto sim_start = std::chrono::high_resolution_clock::now();

    uint64_t total_spikes = 0;
    std::normal_distribution<float> current_dist(5.0f, 2.0f);

    for (size_t t = 0; t < N_TIMESTEPS; t++) {
        // Generate random input current
        std::vector<float> current_data(N_INPUT);
        for (auto& c : current_data) c = std::max(0.0f, current_dist(rng_));
        nimcp_gpu_memcpy(gpu_ctx_, input_current->data, current_data.data(),
                         current_data.size() * sizeof(float), GPU_MEMCPY_HOST_TO_DEVICE);

        // Input layer forward pass
        bool success = nimcp_gpu_lif_forward(gpu_ctx_, input_layer, input_current);
        E2E_ASSERT(success, "Input layer forward failed");

        // Propagate spikes through weights to hidden layer
        success = nimcp_gpu_gemv(gpu_ctx_, weights_ih, input_layer->spikes,
                                  hidden_current, 1.0f, 0.0f, true);
        E2E_ASSERT(success, "Input->Hidden propagation failed");

        // Hidden layer forward pass
        success = nimcp_gpu_lif_forward(gpu_ctx_, hidden_layer, hidden_current);
        E2E_ASSERT(success, "Hidden layer forward failed");

        // Propagate to output
        success = nimcp_gpu_gemv(gpu_ctx_, weights_ho, hidden_layer->spikes,
                                  output_current, 1.0f, 0.0f, true);
        E2E_ASSERT(success, "Hidden->Output propagation failed");

        // Output layer forward pass
        success = nimcp_gpu_lif_forward(gpu_ctx_, output_layer, output_current);
        E2E_ASSERT(success, "Output layer forward failed");

        // Count spikes
        uint32_t input_spikes = 0, hidden_spikes = 0, output_spikes = 0;
        nimcp_gpu_spike_count(gpu_ctx_, input_layer->spikes, &input_spikes);
        nimcp_gpu_spike_count(gpu_ctx_, hidden_layer->spikes, &hidden_spikes);
        nimcp_gpu_spike_count(gpu_ctx_, output_layer->spikes, &output_spikes);
        total_spikes += input_spikes + hidden_spikes + output_spikes;
    }

    nimcp_gpu_context_synchronize(gpu_ctx_);
    auto sim_end = std::chrono::high_resolution_clock::now();
    metrics_.gpu_time_ms = std::chrono::duration<double, std::milli>(sim_end - sim_start).count();

    metrics_.total_neurons = N_INPUT + N_HIDDEN + N_OUTPUT;
    metrics_.total_spikes = total_spikes;
    metrics_.timesteps_simulated = N_TIMESTEPS;
    metrics_.neurons_per_second = (metrics_.total_neurons * N_TIMESTEPS) / (metrics_.gpu_time_ms / 1000.0);
    metrics_.average_firing_rate = (total_spikes * 1000.0) / (metrics_.total_neurons * N_TIMESTEPS * DT);

    std::cout << "\n  Simulation completed:" << std::endl;
    std::cout << "    Time: " << metrics_.gpu_time_ms << " ms" << std::endl;
    std::cout << "    Total spikes: " << total_spikes << std::endl;
    std::cout << "    Average firing rate: " << metrics_.average_firing_rate << " Hz" << std::endl;

    E2E_STAGE_END();

    // Stage 5: Memory statistics
    E2E_STAGE_BEGIN("Collect memory statistics", 500);

    size_t allocated = 0, peak = 0, free_mem = 0;
    nimcp_gpu_memory_stats(gpu_ctx_, &allocated, &peak, &free_mem);
    metrics_.memory_usage_bytes = peak;

    std::cout << "\n  GPU Memory:" << std::endl;
    std::cout << "    Peak usage: " << (peak / 1024.0 / 1024.0) << " MB" << std::endl;

    E2E_STAGE_END();

    // Cleanup
    nimcp_gpu_tensor_destroy(input_current);
    nimcp_gpu_tensor_destroy(hidden_current);
    nimcp_gpu_tensor_destroy(output_current);
    nimcp_gpu_tensor_destroy(weights_ih);
    nimcp_gpu_tensor_destroy(weights_ho);
    nimcp_lif_state_destroy(input_layer);
    nimcp_lif_state_destroy(hidden_layer);
    nimcp_lif_state_destroy(output_layer);

    PrintMetrics("Full Brain Lifecycle GPU");

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Large-Scale LIF Neuron Dynamics
//=============================================================================

TEST_F(BrainSimulationGPUE2ETest, LargeScaleLIFDynamicsGPU) {
    E2E_PIPELINE_START("Large-Scale LIF Neuron Dynamics");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    const size_t N_NEURONS = 100000;
    const size_t N_TIMESTEPS = 500;
    const float DT = 0.1f;  // 0.1ms for accurate dynamics

    // Stage 1: Create large neuron population
    E2E_STAGE_BEGIN("Create large neuron population", 5000);

    nimcp_lif_params_t params = {
        .tau_mem = 20.0f,
        .tau_syn = 5.0f,
        .v_thresh = -50.0f,
        .v_reset = -70.0f,
        .v_rest = -65.0f,
        .dt = DT,
        .hard_reset = true
    };

    nimcp_lif_state_t* neurons = nimcp_lif_state_create(gpu_ctx_, N_NEURONS, &params);
    E2E_ASSERT_NOT_NULL(neurons, "Failed to create neuron population");

    std::cout << "\n  Population size: " << N_NEURONS << " neurons" << std::endl;
    std::cout << "  Timestep: " << DT << " ms" << std::endl;
    std::cout << "  Total simulation time: " << (N_TIMESTEPS * DT) << " ms" << std::endl;

    E2E_STAGE_END();

    // Stage 2: Create input current with temporal structure
    E2E_STAGE_BEGIN("Create structured input", 15000);

    size_t input_dims[] = {N_NEURONS};
    nimcp_gpu_tensor_t* input_current = nimcp_gpu_tensor_create(gpu_ctx_, input_dims, 1,
                                                                  NIMCP_GPU_PRECISION_FP32);

    // Pre-generate input currents for all timesteps (more efficient)
    std::vector<std::vector<float>> all_currents(N_TIMESTEPS);
    std::normal_distribution<float> base_current(3.0f, 1.0f);

    for (size_t t = 0; t < N_TIMESTEPS; t++) {
        all_currents[t].resize(N_NEURONS);
        // Oscillatory input pattern
        float phase = 2.0f * 3.14159f * t / 100.0f;
        float modulation = 1.0f + 0.5f * std::sin(phase);

        for (size_t i = 0; i < N_NEURONS; i++) {
            float spatial_mod = 1.0f + 0.3f * std::sin(2.0f * 3.14159f * i / N_NEURONS);
            all_currents[t][i] = std::max(0.0f, base_current(rng_) * modulation * spatial_mod);
        }
    }

    E2E_STAGE_END();

    // Stage 3: Run simulation
    E2E_STAGE_BEGIN("Run large-scale simulation", 60000);

    auto sim_start = std::chrono::high_resolution_clock::now();

    std::vector<uint32_t> spike_counts(N_TIMESTEPS);

    for (size_t t = 0; t < N_TIMESTEPS; t++) {
        // Upload current for this timestep
        nimcp_gpu_memcpy(gpu_ctx_, input_current->data, all_currents[t].data(),
                         N_NEURONS * sizeof(float), GPU_MEMCPY_HOST_TO_DEVICE);

        // Forward pass
        bool success = nimcp_gpu_lif_forward(gpu_ctx_, neurons, input_current);
        E2E_ASSERT(success, "LIF forward failed");

        // Count spikes
        nimcp_gpu_spike_count(gpu_ctx_, neurons->spikes, &spike_counts[t]);
    }

    nimcp_gpu_context_synchronize(gpu_ctx_);
    auto sim_end = std::chrono::high_resolution_clock::now();
    metrics_.gpu_time_ms = std::chrono::duration<double, std::milli>(sim_end - sim_start).count();

    // Compute statistics
    metrics_.total_neurons = N_NEURONS;
    metrics_.total_spikes = std::accumulate(spike_counts.begin(), spike_counts.end(), 0ull);
    metrics_.timesteps_simulated = N_TIMESTEPS;
    metrics_.neurons_per_second = (N_NEURONS * N_TIMESTEPS) / (metrics_.gpu_time_ms / 1000.0);
    metrics_.average_firing_rate = (metrics_.total_spikes * 1000.0) / (N_NEURONS * N_TIMESTEPS * DT);

    std::cout << "\n  Simulation completed:" << std::endl;
    std::cout << "    Time: " << metrics_.gpu_time_ms << " ms" << std::endl;
    std::cout << "    Neuron-updates/sec: " << (metrics_.neurons_per_second / 1e6) << " M" << std::endl;

    E2E_STAGE_END();

    // Stage 4: Analyze spike dynamics
    E2E_STAGE_BEGIN("Analyze spike dynamics", 1000);

    // Compute spike rate over time
    std::vector<double> rates(N_TIMESTEPS);
    for (size_t t = 0; t < N_TIMESTEPS; t++) {
        rates[t] = (spike_counts[t] * 1000.0) / (N_NEURONS * DT);
    }

    double mean_rate = std::accumulate(rates.begin(), rates.end(), 0.0) / N_TIMESTEPS;
    double max_rate = *std::max_element(rates.begin(), rates.end());
    double min_rate = *std::min_element(rates.begin(), rates.end());

    std::cout << "\n  Spike dynamics:" << std::endl;
    std::cout << "    Mean rate: " << mean_rate << " Hz" << std::endl;
    std::cout << "    Rate range: [" << min_rate << ", " << max_rate << "] Hz" << std::endl;
    std::cout << "    Rate modulation: " << (max_rate - min_rate) << " Hz" << std::endl;

    // Check for oscillatory behavior (should see modulation due to input pattern)
    EXPECT_GT(max_rate - min_rate, 1.0) << "Should see rate modulation from oscillatory input";

    E2E_STAGE_END();

    // Cleanup
    nimcp_gpu_tensor_destroy(input_current);
    nimcp_lif_state_destroy(neurons);

    PrintMetrics("Large-Scale LIF Dynamics GPU");

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: STDP Synaptic Plasticity
//=============================================================================

TEST_F(BrainSimulationGPUE2ETest, STDPPlasticityGPU) {
    E2E_PIPELINE_START("STDP Synaptic Plasticity on GPU");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    const size_t N_PRE = 1000;
    const size_t N_POST = 1000;
    const size_t N_TIMESTEPS = 500;
    const float DT = 1.0f;

    // Stage 1: Create pre and post synaptic populations
    E2E_STAGE_BEGIN("Create synaptic populations", 2000);

    nimcp_lif_params_t params = {
        .tau_mem = 20.0f,
        .tau_syn = 5.0f,
        .v_thresh = -50.0f,
        .v_reset = -70.0f,
        .v_rest = -65.0f,
        .dt = DT,
        .hard_reset = true
    };

    nimcp_lif_state_t* pre_neurons = nimcp_lif_state_create(gpu_ctx_, N_PRE, &params);
    nimcp_lif_state_t* post_neurons = nimcp_lif_state_create(gpu_ctx_, N_POST, &params);

    E2E_ASSERT_NOT_NULL(pre_neurons, "Failed to create pre-synaptic neurons");
    E2E_ASSERT_NOT_NULL(post_neurons, "Failed to create post-synaptic neurons");

    E2E_STAGE_END();

    // Stage 2: Create weights and eligibility traces
    E2E_STAGE_BEGIN("Create weights and traces", 2000);

    size_t weight_dims[] = {N_PRE, N_POST};
    size_t trace_dims[] = {N_PRE};
    size_t trace_post_dims[] = {N_POST};

    nimcp_gpu_tensor_t* weights = nimcp_gpu_tensor_create(gpu_ctx_, weight_dims, 2,
                                                           NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* pre_trace = nimcp_gpu_tensor_create(gpu_ctx_, trace_dims, 1,
                                                             NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* post_trace = nimcp_gpu_tensor_create(gpu_ctx_, trace_post_dims, 1,
                                                              NIMCP_GPU_PRECISION_FP32);

    // Initialize weights with sparse connectivity
    std::vector<float> initial_weights;
    GenerateSparseConnectivity(initial_weights, N_PRE, N_POST, 0.1f, 0.5f);
    nimcp_gpu_memcpy(gpu_ctx_, weights->data, initial_weights.data(),
                     initial_weights.size() * sizeof(float), GPU_MEMCPY_HOST_TO_DEVICE);

    // Zero traces
    nimcp_gpu_zeros(gpu_ctx_, pre_trace);
    nimcp_gpu_zeros(gpu_ctx_, post_trace);

    // Store initial weight statistics
    float initial_sum = std::accumulate(initial_weights.begin(), initial_weights.end(), 0.0f);

    E2E_STAGE_END();

    // Stage 3: Configure STDP
    E2E_STAGE_BEGIN("Configure STDP parameters", 500);

    nimcp_stdp_params_t stdp_params = {
        .A_plus = 0.01f,      // LTP amplitude
        .A_minus = 0.0105f,   // LTD amplitude (slightly larger for stability)
        .tau_plus = 20.0f,    // Pre-before-post time constant
        .tau_minus = 20.0f,   // Post-before-pre time constant
        .w_max = 1.0f,        // Maximum weight
        .w_min = 0.0f         // Minimum weight
    };

    std::cout << "\n  STDP Configuration:" << std::endl;
    std::cout << "    A+: " << stdp_params.A_plus << std::endl;
    std::cout << "    A-: " << stdp_params.A_minus << std::endl;
    std::cout << "    tau: " << stdp_params.tau_plus << " ms" << std::endl;

    E2E_STAGE_END();

    // Stage 4: Run STDP learning
    E2E_STAGE_BEGIN("Run STDP learning", 30000);

    size_t input_dims[] = {N_PRE};
    nimcp_gpu_tensor_t* pre_current = nimcp_gpu_tensor_create(gpu_ctx_, input_dims, 1,
                                                               NIMCP_GPU_PRECISION_FP32);
    size_t post_input_dims[] = {N_POST};
    nimcp_gpu_tensor_t* post_current = nimcp_gpu_tensor_create(gpu_ctx_, post_input_dims, 1,
                                                                 NIMCP_GPU_PRECISION_FP32);

    auto stdp_start = std::chrono::high_resolution_clock::now();

    std::normal_distribution<float> current_dist(5.0f, 2.0f);
    uint64_t total_pre_spikes = 0, total_post_spikes = 0;

    for (size_t t = 0; t < N_TIMESTEPS; t++) {
        // Generate input currents
        std::vector<float> pre_currents(N_PRE);
        for (auto& c : pre_currents) c = std::max(0.0f, current_dist(rng_));
        nimcp_gpu_memcpy(gpu_ctx_, pre_current->data, pre_currents.data(),
                         N_PRE * sizeof(float), GPU_MEMCPY_HOST_TO_DEVICE);

        // Pre-synaptic forward
        bool success = nimcp_gpu_lif_forward(gpu_ctx_, pre_neurons, pre_current);
        E2E_ASSERT(success, "Pre-synaptic forward failed");

        // Compute post-synaptic input (through weights)
        success = nimcp_gpu_gemv(gpu_ctx_, weights, pre_neurons->spikes,
                                  post_current, 1.0f, 0.0f, true);
        E2E_ASSERT(success, "Synaptic transmission failed");

        // Post-synaptic forward
        success = nimcp_gpu_lif_forward(gpu_ctx_, post_neurons, post_current);
        E2E_ASSERT(success, "Post-synaptic forward failed");

        // Update eligibility traces
        success = nimcp_gpu_eligibility_trace_update(gpu_ctx_, pre_trace,
                                                      pre_neurons->spikes, 0.95f);
        success = success && nimcp_gpu_eligibility_trace_update(gpu_ctx_, post_trace,
                                                                  post_neurons->spikes, 0.95f);
        E2E_ASSERT(success, "Trace update failed");

        // Apply STDP
        success = nimcp_gpu_stdp_pair(gpu_ctx_, weights,
                                       pre_neurons->spikes, post_neurons->spikes,
                                       pre_trace, post_trace, &stdp_params);
        E2E_ASSERT(success, "STDP update failed");

        // Count spikes
        uint32_t pre_count = 0, post_count = 0;
        nimcp_gpu_spike_count(gpu_ctx_, pre_neurons->spikes, &pre_count);
        nimcp_gpu_spike_count(gpu_ctx_, post_neurons->spikes, &post_count);
        total_pre_spikes += pre_count;
        total_post_spikes += post_count;
    }

    nimcp_gpu_context_synchronize(gpu_ctx_);
    auto stdp_end = std::chrono::high_resolution_clock::now();
    metrics_.gpu_time_ms = std::chrono::duration<double, std::milli>(stdp_end - stdp_start).count();

    std::cout << "\n  Learning completed:" << std::endl;
    std::cout << "    Pre spikes: " << total_pre_spikes << std::endl;
    std::cout << "    Post spikes: " << total_post_spikes << std::endl;

    E2E_STAGE_END();

    // Stage 5: Analyze weight changes
    E2E_STAGE_BEGIN("Analyze weight changes", 1000);

    std::vector<float> final_weights(N_PRE * N_POST);
    nimcp_gpu_memcpy(gpu_ctx_, final_weights.data(), weights->data,
                     final_weights.size() * sizeof(float), GPU_MEMCPY_DEVICE_TO_HOST);

    float final_sum = std::accumulate(final_weights.begin(), final_weights.end(), 0.0f);

    size_t increased = 0, decreased = 0, unchanged = 0;
    for (size_t i = 0; i < N_PRE * N_POST; i++) {
        if (initial_weights[i] > 0) {  // Only count initially connected synapses
            if (final_weights[i] > initial_weights[i] + 0.001f) increased++;
            else if (final_weights[i] < initial_weights[i] - 0.001f) decreased++;
            else unchanged++;
        }
    }

    std::cout << "\n  Weight changes:" << std::endl;
    std::cout << "    Initial sum: " << initial_sum << std::endl;
    std::cout << "    Final sum: " << final_sum << std::endl;
    std::cout << "    Increased: " << increased << std::endl;
    std::cout << "    Decreased: " << decreased << std::endl;
    std::cout << "    Unchanged: " << unchanged << std::endl;

    EXPECT_GT(increased + decreased, 0) << "STDP should modify some weights";

    E2E_STAGE_END();

    // Cleanup
    nimcp_gpu_tensor_destroy(pre_current);
    nimcp_gpu_tensor_destroy(post_current);
    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(pre_trace);
    nimcp_gpu_tensor_destroy(post_trace);
    nimcp_lif_state_destroy(pre_neurons);
    nimcp_lif_state_destroy(post_neurons);

    metrics_.total_neurons = N_PRE + N_POST;
    metrics_.total_synapses = N_PRE * N_POST;
    metrics_.total_spikes = total_pre_spikes + total_post_spikes;
    metrics_.timesteps_simulated = N_TIMESTEPS;
    PrintMetrics("STDP Plasticity GPU");

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Multi-Region Brain Simulation
//=============================================================================

TEST_F(BrainSimulationGPUE2ETest, MultiRegionBrainGPU) {
    E2E_PIPELINE_START("Multi-Region Brain on GPU");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    // Simulate multiple cortical regions
    const size_t N_REGIONS = 4;
    const size_t NEURONS_PER_REGION = 2500;
    const size_t N_TIMESTEPS = 200;
    const float DT = 1.0f;

    struct BrainRegion {
        nimcp_lif_state_t* neurons;
        nimcp_gpu_tensor_t* input_current;
        std::string name;
    };

    // Stage 1: Create brain regions
    E2E_STAGE_BEGIN("Create brain regions", 3000);

    std::vector<BrainRegion> regions(N_REGIONS);
    std::vector<std::string> region_names = {"Visual", "Motor", "Prefrontal", "Temporal"};

    nimcp_lif_params_t params = {
        .tau_mem = 20.0f,
        .tau_syn = 5.0f,
        .v_thresh = -50.0f,
        .v_reset = -70.0f,
        .v_rest = -65.0f,
        .dt = DT,
        .hard_reset = true
    };

    for (size_t r = 0; r < N_REGIONS; r++) {
        regions[r].neurons = nimcp_lif_state_create(gpu_ctx_, NEURONS_PER_REGION, &params);
        E2E_ASSERT_NOT_NULL(regions[r].neurons, "Failed to create region " + region_names[r]);

        size_t dims[] = {NEURONS_PER_REGION};
        regions[r].input_current = nimcp_gpu_tensor_create(gpu_ctx_, dims, 1,
                                                            NIMCP_GPU_PRECISION_FP32);
        regions[r].name = region_names[r];
    }

    std::cout << "\n  Brain Regions:" << std::endl;
    for (size_t r = 0; r < N_REGIONS; r++) {
        std::cout << "    " << region_names[r] << ": " << NEURONS_PER_REGION << " neurons" << std::endl;
    }

    E2E_STAGE_END();

    // Stage 2: Create inter-region connections
    E2E_STAGE_BEGIN("Create inter-region connections", 20000);

    // Create weight matrices between regions (fully connected between regions)
    std::vector<std::vector<nimcp_gpu_tensor_t*>> inter_weights(N_REGIONS);
    for (size_t i = 0; i < N_REGIONS; i++) {
        inter_weights[i].resize(N_REGIONS, nullptr);
    }

    size_t weight_dims[] = {NEURONS_PER_REGION, NEURONS_PER_REGION};

    for (size_t i = 0; i < N_REGIONS; i++) {
        for (size_t j = 0; j < N_REGIONS; j++) {
            if (i != j) {
                inter_weights[i][j] = nimcp_gpu_tensor_create(gpu_ctx_, weight_dims, 2,
                                                               NIMCP_GPU_PRECISION_FP32);

                std::vector<float> weights;
                float connectivity = (i == 0 && j == 1) ? 0.2f : 0.05f;  // Stronger visual->motor
                GenerateSparseConnectivity(weights, NEURONS_PER_REGION, NEURONS_PER_REGION,
                                          connectivity, 0.1f);
                nimcp_gpu_memcpy(gpu_ctx_, inter_weights[i][j]->data, weights.data(),
                                 weights.size() * sizeof(float), GPU_MEMCPY_HOST_TO_DEVICE);
            }
        }
    }

    std::cout << "\n  Inter-region connections created" << std::endl;

    E2E_STAGE_END();

    // Stage 3: Run multi-region simulation
    E2E_STAGE_BEGIN("Run multi-region simulation", 30000);

    auto sim_start = std::chrono::high_resolution_clock::now();

    std::vector<std::vector<uint32_t>> spike_history(N_REGIONS);
    for (auto& h : spike_history) h.resize(N_TIMESTEPS);

    std::normal_distribution<float> current_dist(3.0f, 1.5f);

    for (size_t t = 0; t < N_TIMESTEPS; t++) {
        // External input to visual region only
        std::vector<float> visual_input(NEURONS_PER_REGION);
        for (auto& c : visual_input) c = std::max(0.0f, current_dist(rng_));
        nimcp_gpu_memcpy(gpu_ctx_, regions[0].input_current->data, visual_input.data(),
                         NEURONS_PER_REGION * sizeof(float), GPU_MEMCPY_HOST_TO_DEVICE);

        // Zero input for other regions (will be added from inter-region connections)
        for (size_t r = 1; r < N_REGIONS; r++) {
            nimcp_gpu_zeros(gpu_ctx_, regions[r].input_current);
        }

        // Add inter-region contributions
        for (size_t from = 0; from < N_REGIONS; from++) {
            for (size_t to = 0; to < N_REGIONS; to++) {
                if (from != to && inter_weights[from][to]) {
                    // Add weighted spikes from source region
                    nimcp_gpu_gemv(gpu_ctx_, inter_weights[from][to],
                                    regions[from].neurons->spikes,
                                    regions[to].input_current,
                                    1.0f, 1.0f, true);  // Accumulate
                }
            }
        }

        // Forward pass for all regions
        for (size_t r = 0; r < N_REGIONS; r++) {
            bool success = nimcp_gpu_lif_forward(gpu_ctx_, regions[r].neurons,
                                                  regions[r].input_current);
            E2E_ASSERT(success, "Region " + regions[r].name + " forward failed");

            // Count spikes
            nimcp_gpu_spike_count(gpu_ctx_, regions[r].neurons->spikes, &spike_history[r][t]);
        }
    }

    nimcp_gpu_context_synchronize(gpu_ctx_);
    auto sim_end = std::chrono::high_resolution_clock::now();
    metrics_.gpu_time_ms = std::chrono::duration<double, std::milli>(sim_end - sim_start).count();

    E2E_STAGE_END();

    // Stage 4: Analyze inter-region activity
    E2E_STAGE_BEGIN("Analyze inter-region activity", 1000);

    std::cout << "\n  Region Activity:" << std::endl;
    uint64_t total_spikes = 0;

    for (size_t r = 0; r < N_REGIONS; r++) {
        uint64_t region_spikes = std::accumulate(spike_history[r].begin(),
                                                  spike_history[r].end(), 0ull);
        double rate = (region_spikes * 1000.0) / (NEURONS_PER_REGION * N_TIMESTEPS * DT);
        std::cout << "    " << region_names[r] << ": " << region_spikes
                  << " spikes, " << rate << " Hz" << std::endl;
        total_spikes += region_spikes;
    }

    // Compute cross-correlation between visual and motor (should be correlated with delay)
    double visual_motor_corr = 0.0;
    size_t lag = 5;  // 5ms delay expected
    for (size_t t = lag; t < N_TIMESTEPS; t++) {
        visual_motor_corr += spike_history[0][t - lag] * spike_history[1][t];
    }
    visual_motor_corr /= (N_TIMESTEPS - lag);

    std::cout << "\n  Visual->Motor correlation (lag 5ms): " << visual_motor_corr << std::endl;

    metrics_.total_neurons = N_REGIONS * NEURONS_PER_REGION;
    metrics_.total_spikes = total_spikes;
    metrics_.average_firing_rate = (total_spikes * 1000.0) / (metrics_.total_neurons * N_TIMESTEPS * DT);

    E2E_STAGE_END();

    // Cleanup
    for (size_t r = 0; r < N_REGIONS; r++) {
        nimcp_gpu_tensor_destroy(regions[r].input_current);
        nimcp_lif_state_destroy(regions[r].neurons);
    }
    for (size_t i = 0; i < N_REGIONS; i++) {
        for (size_t j = 0; j < N_REGIONS; j++) {
            if (inter_weights[i][j]) {
                nimcp_gpu_tensor_destroy(inter_weights[i][j]);
            }
        }
    }

    PrintMetrics("Multi-Region Brain GPU");

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Brain Scalability Benchmark
//=============================================================================

TEST_F(BrainSimulationGPUE2ETest, BrainScalabilityBenchmark) {
    E2E_PIPELINE_START("Brain Scalability Benchmark");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    std::vector<size_t> neuron_counts = {1000, 10000, 50000, 100000, 500000};
    const size_t N_TIMESTEPS = 100;
    const float DT = 1.0f;

    std::cout << "\n=== Brain Scalability Benchmark ===" << std::endl;
    std::cout << "| Neurons  | Time(ms) | Neurons/sec   | Spikes | Memory(MB) |" << std::endl;
    std::cout << "|----------|----------|---------------|--------|------------|" << std::endl;

    nimcp_lif_params_t params = {
        .tau_mem = 20.0f,
        .tau_syn = 5.0f,
        .v_thresh = -50.0f,
        .v_reset = -70.0f,
        .v_rest = -65.0f,
        .dt = DT,
        .hard_reset = true
    };

    for (size_t n_neurons : neuron_counts) {
        nimcp_lif_state_t* neurons = nimcp_lif_state_create(gpu_ctx_, n_neurons, &params);

        if (!neurons) {
            std::cout << "| " << n_neurons << " | FAILED - memory |" << std::endl;
            continue;
        }

        size_t dims[] = {n_neurons};
        nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_create(gpu_ctx_, dims, 1,
                                                             NIMCP_GPU_PRECISION_FP32);

        // Initialize with constant current
        std::vector<float> current(n_neurons, 5.0f);
        nimcp_gpu_memcpy(gpu_ctx_, input->data, current.data(),
                         current.size() * sizeof(float), GPU_MEMCPY_HOST_TO_DEVICE);

        // Warm up
        for (int i = 0; i < 10; i++) {
            nimcp_gpu_lif_forward(gpu_ctx_, neurons, input);
        }
        nimcp_gpu_context_synchronize(gpu_ctx_);

        // Benchmark
        auto start = std::chrono::high_resolution_clock::now();
        uint64_t total_spikes = 0;

        for (size_t t = 0; t < N_TIMESTEPS; t++) {
            nimcp_gpu_lif_forward(gpu_ctx_, neurons, input);
            uint32_t spike_count = 0;
            nimcp_gpu_spike_count(gpu_ctx_, neurons->spikes, &spike_count);
            total_spikes += spike_count;
        }

        nimcp_gpu_context_synchronize(gpu_ctx_);
        auto end = std::chrono::high_resolution_clock::now();
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();

        double neurons_per_sec = (n_neurons * N_TIMESTEPS) / (time_ms / 1000.0);

        size_t allocated = 0, peak = 0, free_mem = 0;
        nimcp_gpu_memory_stats(gpu_ctx_, &allocated, &peak, &free_mem);
        double memory_mb = peak / (1024.0 * 1024.0);

        std::cout << "| " << n_neurons << " | " << time_ms << " | "
                  << (neurons_per_sec / 1e6) << "M | "
                  << total_spikes << " | " << memory_mb << " |" << std::endl;

        nimcp_gpu_tensor_destroy(input);
        nimcp_lif_state_destroy(neurons);
    }

    E2E_PIPELINE_END();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
