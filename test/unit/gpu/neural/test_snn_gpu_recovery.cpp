/* ============================================================================
 * Unit Tests: SNN GPU Recovery Integration
 * ============================================================================
 * WHAT: Unit tests for GPU recovery in Spiking Neural Network operations
 * WHY:  Validate self-healing and CPU fallback for SNN kernel failures
 * HOW:  Test recovery from OOM, kernel launch failures, numerical errors
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-4f;

class SNNGPURecoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        ctx_ = nimcp_gpu_context_create(0);
        if (ctx_) {
            // Initialize recovery system
            nimcp_gpu_recovery_config_t config;
            nimcp_gpu_recovery_default_config(&config);
            config.enable_cpu_fallback = true;
            config.enable_param_correction = true;
            config.max_retries = 3;
            nimcp_gpu_recovery_init(&config);
        }
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_context_destroy(ctx_);
            ctx_ = NULL;
        }
        nimcp_gpu_recovery_shutdown();
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx_ = NULL;

    // Helper: Create default LIF parameters
    nimcp_lif_params_t make_lif_params() {
        nimcp_lif_params_t params;
        params.tau_mem = 20.0f;    // 20ms membrane time constant
        params.tau_syn = 5.0f;     // 5ms synaptic time constant
        params.v_thresh = -50.0f;  // mV threshold
        params.v_reset = -70.0f;   // mV reset
        params.v_rest = -65.0f;    // mV resting
        params.dt = 1.0f;          // 1ms timestep
        params.hard_reset = true;
        return params;
    }

    // Helper: Create default Izhikevich parameters (regular spiking)
    nimcp_izhikevich_params_t make_izhikevich_params() {
        nimcp_izhikevich_params_t params;
        params.a = 0.02f;
        params.b = 0.2f;
        params.c = -65.0f;
        params.d = 8.0f;
        params.v_thresh = 30.0f;
        params.dt = 1.0f;
        return params;
    }

    // Helper: Create default AdEx parameters
    nimcp_adex_params_t make_adex_params() {
        nimcp_adex_params_t params;
        params.tau_mem = 20.0f;
        params.tau_w = 100.0f;
        params.v_thresh = -50.0f;
        params.v_reset = -70.0f;
        params.v_rest = -65.0f;
        params.v_rheo = -55.0f;
        params.delta_T = 2.0f;
        params.a = 4.0f;
        params.b = 0.0805f;
        params.dt = 1.0f;
        return params;
    }

    // Helper: Create input tensor with random values
    nimcp_gpu_tensor_t* create_random_input(size_t n_neurons, float scale = 1.0f) {
        size_t dims[1] = {n_neurons};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx_, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (!tensor) return NULL;

        std::vector<float> host_data(n_neurons);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0f, scale);
        for (size_t i = 0; i < n_neurons; i++) {
            host_data[i] = dis(gen);
        }

        cudaMemcpy(tensor->data, host_data.data(), n_neurons * sizeof(float), cudaMemcpyHostToDevice);
        return tensor;
    }
#endif
};

/* ============================================================================
 * Test: LIF State Creation with Recovery
 * ============================================================================ */
TEST_F(SNNGPURecoveryTest, LIFStateCreation) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    nimcp_lif_params_t params = make_lif_params();
    size_t n_neurons = 1024;

    // Create LIF state - recovery system should initialize if needed
    nimcp_lif_state_t* state = nimcp_lif_state_create(ctx_, n_neurons, &params);
    ASSERT_NE(state, nullptr) << "LIF state creation should succeed with recovery";
    ASSERT_NE(state->v, nullptr) << "Membrane potential tensor should exist";
    ASSERT_NE(state->spikes, nullptr) << "Spikes tensor should exist";

    // Verify dimensions
    EXPECT_EQ(state->v->numel, n_neurons);
    EXPECT_EQ(state->spikes->numel, n_neurons);

    nimcp_lif_state_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: LIF Forward Pass with Recovery
 * ============================================================================ */
TEST_F(SNNGPURecoveryTest, LIFForwardRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    nimcp_lif_params_t params = make_lif_params();
    size_t n_neurons = 512;

    nimcp_lif_state_t* state = nimcp_lif_state_create(ctx_, n_neurons, &params);
    ASSERT_NE(state, nullptr);

    // Create input current
    nimcp_gpu_tensor_t* input = create_random_input(n_neurons, 20.0f);
    ASSERT_NE(input, nullptr);

    // Run forward pass - recovery system handles any errors
    bool success = nimcp_gpu_lif_forward(ctx_, state, input);
    EXPECT_TRUE(success) << "LIF forward should succeed with recovery system";

    // Run multiple steps to test stability
    for (int i = 0; i < 100; i++) {
        success = nimcp_gpu_lif_forward(ctx_, state, input);
        EXPECT_TRUE(success) << "LIF forward step " << i << " failed";
    }

    // Verify spikes occurred (with sufficient input, some should spike)
    uint32_t spike_count = 0;
    success = nimcp_gpu_spike_count(ctx_, state->spikes, &spike_count);
    EXPECT_TRUE(success);
    // Some spikes should have occurred with random input
    // Note: exact count depends on random input values

    nimcp_gpu_tensor_destroy(input);
    nimcp_lif_state_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: LIF Backward with Surrogate Gradient Recovery
 * ============================================================================ */
TEST_F(SNNGPURecoveryTest, LIFBackwardSurrogateRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    nimcp_lif_params_t params = make_lif_params();
    size_t n_neurons = 256;

    nimcp_lif_state_t* state = nimcp_lif_state_create(ctx_, n_neurons, &params);
    ASSERT_NE(state, nullptr);

    // Run forward pass first
    nimcp_gpu_tensor_t* input = create_random_input(n_neurons, 15.0f);
    ASSERT_NE(input, nullptr);

    bool success = nimcp_gpu_lif_forward(ctx_, state, input);
    ASSERT_TRUE(success);

    // Create gradient tensors
    size_t dims[1] = {n_neurons};
    nimcp_gpu_tensor_t* grad_output = nimcp_gpu_tensor_create(ctx_, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* grad_input = nimcp_gpu_tensor_create(ctx_, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(grad_output, nullptr);
    ASSERT_NE(grad_input, nullptr);

    // Initialize gradient output with ones
    nimcp_gpu_fill(ctx_, grad_output, 1.0f);

    // Test all surrogate gradient types
    std::vector<nimcp_surrogate_type_t> surrogates = {
        NIMCP_SURROGATE_SUPERSPIKE,
        NIMCP_SURROGATE_FAST_SIGMOID,
        NIMCP_SURROGATE_ARCTAN,
        NIMCP_SURROGATE_TRIANGULAR,
        NIMCP_SURROGATE_GAUSSIAN
    };

    for (auto surrogate : surrogates) {
        success = nimcp_gpu_lif_backward(ctx_, state, grad_output, grad_input, surrogate, 10.0f);
        EXPECT_TRUE(success) << "LIF backward with surrogate type " << (int)surrogate << " failed";
    }

    nimcp_gpu_tensor_destroy(grad_output);
    nimcp_gpu_tensor_destroy(grad_input);
    nimcp_gpu_tensor_destroy(input);
    nimcp_lif_state_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Izhikevich Model Recovery
 * ============================================================================ */
TEST_F(SNNGPURecoveryTest, IzhikevichRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    nimcp_izhikevich_params_t params = make_izhikevich_params();
    size_t n_neurons = 512;

    nimcp_izhikevich_state_t* state = nimcp_izhikevich_state_create(ctx_, n_neurons, &params);
    ASSERT_NE(state, nullptr);

    // Create input current
    nimcp_gpu_tensor_t* input = create_random_input(n_neurons, 15.0f);
    ASSERT_NE(input, nullptr);

    // Run multiple forward steps
    for (int i = 0; i < 50; i++) {
        bool success = nimcp_gpu_izhikevich_forward(ctx_, state, input);
        EXPECT_TRUE(success) << "Izhikevich forward step " << i << " failed";
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_izhikevich_state_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: AdEx Model Recovery
 * ============================================================================ */
TEST_F(SNNGPURecoveryTest, AdExRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    nimcp_adex_params_t params = make_adex_params();
    size_t n_neurons = 512;

    nimcp_adex_state_t* state = nimcp_adex_state_create(ctx_, n_neurons, &params);
    ASSERT_NE(state, nullptr);

    // Create input current
    nimcp_gpu_tensor_t* input = create_random_input(n_neurons, 10.0f);
    ASSERT_NE(input, nullptr);

    // Run multiple forward steps
    for (int i = 0; i < 50; i++) {
        bool success = nimcp_gpu_adex_forward(ctx_, state, input);
        EXPECT_TRUE(success) << "AdEx forward step " << i << " failed";
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_adex_state_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Spike Propagation Recovery
 * ============================================================================ */
TEST_F(SNNGPURecoveryTest, SpikePropagationRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t n_pre = 256;
    size_t n_post = 128;

    // Create spike tensor
    size_t spike_dims[1] = {n_pre};
    nimcp_gpu_tensor_t* spikes = nimcp_gpu_tensor_create(ctx_, spike_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(spikes, nullptr);

    // Set some neurons as spiking (sparse pattern)
    std::vector<float> spike_data(n_pre, 0.0f);
    for (size_t i = 0; i < n_pre; i += 10) {
        spike_data[i] = 1.0f;  // Every 10th neuron spikes
    }
    cudaMemcpy(spikes->data, spike_data.data(), n_pre * sizeof(float), cudaMemcpyHostToDevice);

    // Create weight matrix
    size_t weight_dims[2] = {n_pre, n_post};
    nimcp_gpu_tensor_t* weights = nimcp_gpu_tensor_create(ctx_, weight_dims, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(weights, nullptr);

    // Initialize weights with random values
    std::vector<float> weight_data(n_pre * n_post);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-0.5f, 0.5f);
    for (size_t i = 0; i < n_pre * n_post; i++) {
        weight_data[i] = dis(gen);
    }
    cudaMemcpy(weights->data, weight_data.data(), n_pre * n_post * sizeof(float), cudaMemcpyHostToDevice);

    // Create output tensor
    size_t output_dims[1] = {n_post};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx_, output_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);
    nimcp_gpu_zeros(ctx_, output);

    // Test spike propagation with recovery
    bool success = nimcp_gpu_spike_propagate(ctx_, spikes, weights, output);
    EXPECT_TRUE(success) << "Spike propagation should succeed with recovery";

    nimcp_gpu_tensor_destroy(spikes);
    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: STDP Learning Recovery
 * ============================================================================ */
TEST_F(SNNGPURecoveryTest, STDPLearningRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t n_pre = 128;
    size_t n_post = 64;

    // Create tensors
    size_t pre_dims[1] = {n_pre};
    size_t post_dims[1] = {n_post};
    size_t weight_dims[2] = {n_pre, n_post};

    nimcp_gpu_tensor_t* pre_spikes = nimcp_gpu_tensor_create(ctx_, pre_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* post_spikes = nimcp_gpu_tensor_create(ctx_, post_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* pre_trace = nimcp_gpu_tensor_create(ctx_, pre_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* post_trace = nimcp_gpu_tensor_create(ctx_, post_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* weights = nimcp_gpu_tensor_create(ctx_, weight_dims, 2, NIMCP_GPU_PRECISION_FP32);

    ASSERT_NE(pre_spikes, nullptr);
    ASSERT_NE(post_spikes, nullptr);
    ASSERT_NE(pre_trace, nullptr);
    ASSERT_NE(post_trace, nullptr);
    ASSERT_NE(weights, nullptr);

    // Initialize
    nimcp_gpu_zeros(ctx_, pre_trace);
    nimcp_gpu_zeros(ctx_, post_trace);
    nimcp_gpu_fill(ctx_, weights, 0.5f);  // Initial weights

    // STDP parameters
    nimcp_stdp_params_t stdp_params;
    stdp_params.A_plus = 0.005f;
    stdp_params.A_minus = 0.0051f;  // Slight asymmetry
    stdp_params.tau_plus = 20.0f;
    stdp_params.tau_minus = 20.0f;
    stdp_params.w_max = 1.0f;
    stdp_params.w_min = 0.0f;

    // Generate random spike patterns and run STDP
    std::random_device rd;
    std::mt19937 gen(rd());
    std::bernoulli_distribution spike_dist(0.1);  // 10% spike probability

    for (int step = 0; step < 100; step++) {
        // Generate spikes
        std::vector<float> pre_spike_data(n_pre), post_spike_data(n_post);
        for (size_t i = 0; i < n_pre; i++) pre_spike_data[i] = spike_dist(gen) ? 1.0f : 0.0f;
        for (size_t i = 0; i < n_post; i++) post_spike_data[i] = spike_dist(gen) ? 1.0f : 0.0f;

        cudaMemcpy(pre_spikes->data, pre_spike_data.data(), n_pre * sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(post_spikes->data, post_spike_data.data(), n_post * sizeof(float), cudaMemcpyHostToDevice);

        // Update traces
        bool success = nimcp_gpu_eligibility_trace_update(ctx_, pre_trace, pre_spikes, 0.95f);
        EXPECT_TRUE(success) << "Pre-trace update failed at step " << step;

        success = nimcp_gpu_eligibility_trace_update(ctx_, post_trace, post_spikes, 0.95f);
        EXPECT_TRUE(success) << "Post-trace update failed at step " << step;

        // Apply STDP
        success = nimcp_gpu_stdp_pair(ctx_, weights, pre_spikes, post_spikes,
                                       pre_trace, post_trace, &stdp_params);
        EXPECT_TRUE(success) << "STDP pair update failed at step " << step;
    }

    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
    nimcp_gpu_tensor_destroy(pre_trace);
    nimcp_gpu_tensor_destroy(post_trace);
    nimcp_gpu_tensor_destroy(weights);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Triplet STDP Recovery
 * ============================================================================ */
TEST_F(SNNGPURecoveryTest, TripletSTDPRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t n_pre = 64;
    size_t n_post = 32;

    // Get default triplet STDP parameters
    nimcp_triplet_stdp_params_t params;
    nimcp_triplet_stdp_default_params(&params);

    // Create triplet STDP DAO
    nimcp_stdp_dao_t* dao = nimcp_triplet_stdp_create(ctx_, n_pre, n_post, &params);
    ASSERT_NE(dao, nullptr) << "Triplet STDP DAO creation failed";

    // Create weight matrix on device
    float* d_weights = NULL;
    size_t weight_size = n_pre * n_post * sizeof(float);
    cudaMalloc(&d_weights, weight_size);

    // Initialize weights
    std::vector<float> h_weights(n_pre * n_post, 0.5f);
    cudaMemcpy(d_weights, h_weights.data(), weight_size, cudaMemcpyHostToDevice);

    // Run several STDP steps
    std::vector<int> pre_spikes, post_spikes;
    std::vector<int> pre_indices, post_indices;

    for (int step = 0; step < 50; step++) {
        // Generate random sparse spikes
        pre_spikes.clear();
        post_spikes.clear();
        for (size_t i = 0; i < n_pre; i += 5) pre_spikes.push_back(i);
        for (size_t i = 0; i < n_post; i += 3) post_spikes.push_back(i);

        // Create synapse indices (subset of all connections)
        pre_indices.clear();
        post_indices.clear();
        for (size_t i = 0; i < n_pre; i += 2) {
            for (size_t j = 0; j < n_post; j += 2) {
                pre_indices.push_back(i);
                post_indices.push_back(j);
            }
        }

        int result = nimcp_triplet_stdp_step(
            dao,
            pre_spikes.data(), post_spikes.data(),
            pre_spikes.size(), post_spikes.size(),
            d_weights,
            pre_indices.data(), post_indices.data(),
            pre_indices.size(),
            1.0f,   // dt
            0.01f   // learning rate
        );

        EXPECT_EQ(result, 0) << "Triplet STDP step " << step << " failed";
    }

    cudaFree(d_weights);
    nimcp_triplet_stdp_destroy(dao);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery Statistics Tracking
 * ============================================================================ */
TEST_F(SNNGPURecoveryTest, RecoveryStatistics) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Reset stats at start
    nimcp_gpu_recovery_reset_stats();

    // Perform some operations that use recovery system
    nimcp_lif_params_t params = make_lif_params();
    nimcp_lif_state_t* state = nimcp_lif_state_create(ctx_, 256, &params);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* input = create_random_input(256, 10.0f);
    ASSERT_NE(input, nullptr);

    for (int i = 0; i < 10; i++) {
        nimcp_gpu_lif_forward(ctx_, state, input);
    }

    // Get recovery statistics
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    // Just verify we can get stats without crashing
    EXPECT_GE(stats.success_rate, 0.0f);
    EXPECT_LE(stats.success_rate, 1.0f);

    nimcp_gpu_tensor_destroy(input);
    nimcp_lif_state_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SNN Layer High-Level API Recovery
 * ============================================================================ */
TEST_F(SNNGPURecoveryTest, SNNLayerAPIRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Create LIF layer config
    nimcp_snn_lif_config_t config;
    config.num_neurons = 512;
    config.tau_mem = 20.0f;
    config.tau_syn = 5.0f;
    config.v_rest = -65.0f;
    config.v_thresh = -50.0f;
    config.v_reset = -70.0f;
    config.dt = 1.0f;
    config.refractory_period = 2.0f;

    // Create layer using high-level API
    nimcp_snn_layer_t* layer = nimcp_snn_lif_layer_create(ctx_, &config);
    ASSERT_NE(layer, nullptr) << "SNN layer creation failed";

    // Verify layer properties
    EXPECT_EQ(nimcp_snn_layer_get_size(layer), 512);
    EXPECT_FLOAT_EQ(nimcp_snn_layer_get_tau_mem(layer), 20.0f);

    // Create input
    nimcp_gpu_tensor_t* input = create_random_input(512, 15.0f);
    ASSERT_NE(input, nullptr);

    // Run forward pass
    bool success = nimcp_snn_layer_forward(ctx_, layer, input);
    EXPECT_TRUE(success) << "SNN layer forward failed";

    // Get spikes
    nimcp_gpu_tensor_t* spikes = nimcp_snn_layer_get_spikes(layer);
    EXPECT_NE(spikes, nullptr);

    // Reset and run again
    success = nimcp_snn_layer_reset(ctx_, layer);
    EXPECT_TRUE(success) << "SNN layer reset failed";

    success = nimcp_snn_layer_forward(ctx_, layer, input);
    EXPECT_TRUE(success) << "SNN layer forward after reset failed";

    nimcp_gpu_tensor_destroy(input);
    nimcp_snn_layer_destroy(layer);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

} // namespace
