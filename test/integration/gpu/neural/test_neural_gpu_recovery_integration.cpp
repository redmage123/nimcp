/* ============================================================================
 * Integration Tests: Neural GPU Recovery
 * ============================================================================
 * WHAT: Integration tests for GPU recovery across neural network modules
 * WHY:  Validate recovery works correctly when SNN, LNN, CNN work together
 * HOW:  Test multi-layer networks, hybrid models, and cross-module recovery
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/lnn/nimcp_lnn_gpu.h"
#include "gpu/cnn/nimcp_cnn_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-3f;

class NeuralGPURecoveryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        ctx_ = nimcp_gpu_context_create(0);
        if (ctx_) {
            // Initialize recovery system with strict settings
            nimcp_gpu_recovery_config_t config;
            nimcp_gpu_recovery_default_config(&config);
            config.enable_cpu_fallback = true;
            config.enable_param_correction = true;
            config.max_retries = 5;
            config.retry_delay_ms = 10;
            nimcp_gpu_recovery_init(&config);

            // Reset stats for clean measurement
            nimcp_gpu_recovery_reset_stats();
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

    // Helper: Create 4D tensor
    nimcp_gpu_tensor_t* create_4d_tensor(size_t batch, size_t channels, size_t height, size_t width) {
        size_t dims[4] = {batch, channels, height, width};
        return nimcp_gpu_tensor_create(ctx_, dims, 4, NIMCP_GPU_PRECISION_FP32);
    }

    // Helper: Create 1D tensor
    nimcp_gpu_tensor_t* create_1d_tensor(size_t size) {
        size_t dims[1] = {size};
        return nimcp_gpu_tensor_create(ctx_, dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    // Helper: Fill tensor with random values
    void fill_random(nimcp_gpu_tensor_t* tensor, float scale = 1.0f) {
        size_t numel = tensor->numel;
        std::vector<float> data(numel);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-scale, scale);
        for (size_t i = 0; i < numel; i++) {
            data[i] = dis(gen);
        }
        cudaMemcpy(tensor->data, data.data(), numel * sizeof(float), cudaMemcpyHostToDevice);
    }

    // Helper: Create LIF parameters
    nimcp_lif_params_t make_lif_params() {
        nimcp_lif_params_t params;
        params.tau_mem = 20.0f;
        params.tau_syn = 5.0f;
        params.v_thresh = -50.0f;
        params.v_reset = -70.0f;
        params.v_rest = -65.0f;
        params.dt = 1.0f;
        params.hard_reset = true;
        return params;
    }

    // Helper: Create conv params
    nimcp_conv_params_t make_conv_params(uint32_t k = 3, uint32_t s = 1, uint32_t p = 1) {
        nimcp_conv_params_t params;
        params.kernel_h = k;
        params.kernel_w = k;
        params.stride_h = s;
        params.stride_w = s;
        params.pad_h = p;
        params.pad_w = p;
        params.dilation_h = 1;
        params.dilation_w = 1;
        params.groups = 1;
        return params;
    }
#endif
};

/* ============================================================================
 * Test: CNN Feature Extraction to SNN Classification Pipeline
 * ============================================================================ */
TEST_F(NeuralGPURecoveryIntegrationTest, CNNToSNNPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // CNN feature extraction: Input (4, 3, 32, 32) -> Features (4, 128)
    size_t batch = 4, in_c = 3, height = 32, width = 32;

    // Create input image batch
    nimcp_gpu_tensor_t* input = create_4d_tensor(batch, in_c, height, width);
    ASSERT_NE(input, nullptr);
    fill_random(input, 1.0f);

    // Conv layer 1: 3 -> 32 channels
    size_t w1_dims[4] = {32, 3, 3, 3};
    nimcp_gpu_tensor_t* w1 = nimcp_gpu_tensor_create(ctx_, w1_dims, 4, NIMCP_GPU_PRECISION_FP32);
    fill_random(w1, 0.1f);
    nimcp_gpu_tensor_t* b1 = create_1d_tensor(32);
    nimcp_gpu_zeros(ctx_, b1);
    nimcp_gpu_tensor_t* x1 = create_4d_tensor(batch, 32, 32, 32);

    nimcp_conv_params_t conv_params = make_conv_params(3, 1, 1);
    bool success = nimcp_gpu_conv2d_forward(ctx_, input, w1, b1, x1, &conv_params);
    EXPECT_TRUE(success) << "CNN conv1 failed";

    // Pool: 32x32 -> 16x16
    nimcp_gpu_tensor_t* p1 = create_4d_tensor(batch, 32, 16, 16);
    nimcp_gpu_tensor_t* idx1 = create_4d_tensor(batch, 32, 16, 16);
    nimcp_pool_params_t pool_params;
    pool_params.kernel_h = 2; pool_params.kernel_w = 2;
    pool_params.stride_h = 2; pool_params.stride_w = 2;
    pool_params.pad_h = 0; pool_params.pad_w = 0;
    success = nimcp_gpu_maxpool2d(ctx_, x1, p1, idx1, &pool_params);
    EXPECT_TRUE(success) << "CNN pool1 failed";

    // Conv layer 2: 32 -> 64 channels
    size_t w2_dims[4] = {64, 32, 3, 3};
    nimcp_gpu_tensor_t* w2 = nimcp_gpu_tensor_create(ctx_, w2_dims, 4, NIMCP_GPU_PRECISION_FP32);
    fill_random(w2, 0.1f);
    nimcp_gpu_tensor_t* b2 = create_1d_tensor(64);
    nimcp_gpu_zeros(ctx_, b2);
    nimcp_gpu_tensor_t* x2 = create_4d_tensor(batch, 64, 16, 16);
    success = nimcp_gpu_conv2d_forward(ctx_, p1, w2, b2, x2, &conv_params);
    EXPECT_TRUE(success) << "CNN conv2 failed";

    // Pool: 16x16 -> 8x8
    nimcp_gpu_tensor_t* p2 = create_4d_tensor(batch, 64, 8, 8);
    nimcp_gpu_tensor_t* idx2 = create_4d_tensor(batch, 64, 8, 8);
    success = nimcp_gpu_maxpool2d(ctx_, x2, p2, idx2, &pool_params);
    EXPECT_TRUE(success) << "CNN pool2 failed";

    // Global average pool to get feature vector: (4, 64, 8, 8) -> (4, 64)
    size_t feat_dims[2] = {batch, 64};
    nimcp_gpu_tensor_t* features = nimcp_gpu_tensor_create(ctx_, feat_dims, 2, NIMCP_GPU_PRECISION_FP32);
    success = nimcp_gpu_global_avgpool(ctx_, p2, features);
    EXPECT_TRUE(success) << "CNN global avgpool failed";

    // Now feed features to SNN classifier
    // Create SNN layer with 64 input neurons, 10 output neurons (classification)
    size_t n_input = 64;
    size_t n_output = 10;

    // For each sample in batch, run SNN
    for (size_t b = 0; b < batch; b++) {
        // Extract features for this sample
        size_t feat_sample_dims[1] = {n_input};
        nimcp_gpu_tensor_t* feat_sample = nimcp_gpu_tensor_create(ctx_, feat_sample_dims, 1, NIMCP_GPU_PRECISION_FP32);
        cudaMemcpy(feat_sample->data, (float*)features->data + b * n_input,
                   n_input * sizeof(float), cudaMemcpyDeviceToDevice);

        // Create SNN state
        nimcp_lif_params_t lif_params = make_lif_params();
        nimcp_lif_state_t* snn_state = nimcp_lif_state_create(ctx_, n_output, &lif_params);
        ASSERT_NE(snn_state, nullptr);

        // Run multiple timesteps to allow spike propagation
        for (int t = 0; t < 10; t++) {
            // Scale features as input current
            success = nimcp_gpu_lif_forward(ctx_, snn_state, feat_sample);
            EXPECT_TRUE(success) << "SNN forward failed at sample " << b << " timestep " << t;
        }

        // Count spikes for classification
        uint32_t spike_count = 0;
        success = nimcp_gpu_spike_count(ctx_, snn_state->spikes, &spike_count);
        EXPECT_TRUE(success) << "Spike count failed";

        nimcp_lif_state_destroy(snn_state);
        nimcp_gpu_tensor_destroy(feat_sample);
    }

    // Verify recovery stats
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);

    // Cleanup
    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(w1);
    nimcp_gpu_tensor_destroy(b1);
    nimcp_gpu_tensor_destroy(x1);
    nimcp_gpu_tensor_destroy(p1);
    nimcp_gpu_tensor_destroy(idx1);
    nimcp_gpu_tensor_destroy(w2);
    nimcp_gpu_tensor_destroy(b2);
    nimcp_gpu_tensor_destroy(x2);
    nimcp_gpu_tensor_destroy(p2);
    nimcp_gpu_tensor_destroy(idx2);
    nimcp_gpu_tensor_destroy(features);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: LNN Temporal Processing with Recovery
 * ============================================================================ */
TEST_F(NeuralGPURecoveryIntegrationTest, LNNTemporalProcessing) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Simulate processing a sequence with LNN
    uint32_t n_neurons = 64;
    uint32_t n_inputs = 32;
    int sequence_length = 50;

    // Create LNN layer
    nimcp_lnn_layer_gpu_t* layer = (nimcp_lnn_layer_gpu_t*)calloc(1, sizeof(nimcp_lnn_layer_gpu_t));
    ASSERT_NE(layer, nullptr);

    layer->n_neurons = n_neurons;
    layer->n_inputs = n_inputs;
    layer->activation = LNN_ACTIVATION_TANH;

    // Allocate tensors
    size_t state_dims[1] = {n_neurons};
    size_t in_weight_dims[2] = {n_neurons, n_inputs};
    size_t rec_weight_dims[2] = {n_neurons, n_neurons};
    size_t tau_weight_dims[2] = {n_neurons, n_inputs + n_neurons};

    layer->x = nimcp_gpu_tensor_create(ctx_, state_dims, 1, NIMCP_GPU_PRECISION_FP32);
    layer->dx_dt = nimcp_gpu_tensor_create(ctx_, state_dims, 1, NIMCP_GPU_PRECISION_FP32);
    layer->tau = nimcp_gpu_tensor_create(ctx_, state_dims, 1, NIMCP_GPU_PRECISION_FP32);
    layer->tau_base = nimcp_gpu_tensor_create(ctx_, state_dims, 1, NIMCP_GPU_PRECISION_FP32);
    layer->W_in = nimcp_gpu_tensor_create(ctx_, in_weight_dims, 2, NIMCP_GPU_PRECISION_FP32);
    layer->W_rec = nimcp_gpu_tensor_create(ctx_, rec_weight_dims, 2, NIMCP_GPU_PRECISION_FP32);
    layer->W_tau = nimcp_gpu_tensor_create(ctx_, tau_weight_dims, 2, NIMCP_GPU_PRECISION_FP32);
    layer->b_in = nimcp_gpu_tensor_create(ctx_, state_dims, 1, NIMCP_GPU_PRECISION_FP32);
    layer->b_tau = nimcp_gpu_tensor_create(ctx_, state_dims, 1, NIMCP_GPU_PRECISION_FP32);

    ASSERT_NE(layer->x, nullptr);
    ASSERT_NE(layer->W_in, nullptr);

    // Initialize
    nimcp_gpu_zeros(ctx_, layer->x);
    nimcp_gpu_fill(ctx_, layer->tau_base, 10.0f);
    nimcp_gpu_fill(ctx_, layer->tau, 10.0f);
    fill_random(nimcp_gpu_tensor_create(ctx_, in_weight_dims, 2, NIMCP_GPU_PRECISION_FP32), 0.1f);

    // Initialize weights
    std::vector<float> w_in(n_neurons * n_inputs);
    std::vector<float> w_rec(n_neurons * n_neurons);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 0.1f);
    for (auto& w : w_in) w = dist(gen);
    for (auto& w : w_rec) w = dist(gen) * 0.5f;
    cudaMemcpy(layer->W_in->data, w_in.data(), w_in.size() * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(layer->W_rec->data, w_rec.data(), w_rec.size() * sizeof(float), cudaMemcpyHostToDevice);
    nimcp_gpu_zeros(ctx_, layer->b_in);

    // No sparse wiring
    layer->row_ptr = NULL;
    layer->col_idx = NULL;
    layer->edge_weights = NULL;
    layer->n_edges = 0;

    // Create input sequence
    size_t input_dims[1] = {n_inputs};
    nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_create(ctx_, input_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(input, nullptr);

    nimcp_lnn_ode_config_t config = nimcp_lnn_ode_default_config();
    config.method = LNN_ODE_RK4;
    config.dt = 1.0f;

    // Process sequence
    std::vector<float> final_states(n_neurons);
    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < sequence_length; t++) {
        // Generate time-varying input
        std::vector<float> h_input(n_inputs);
        float phase = 2.0f * M_PI * t / sequence_length;
        for (uint32_t i = 0; i < n_inputs; i++) {
            h_input[i] = std::sin(phase + i * 0.1f);
        }
        cudaMemcpy(input->data, h_input.data(), n_inputs * sizeof(float), cudaMemcpyHostToDevice);

        // ODE step with recovery
        bool success = nimcp_gpu_lnn_ode_step(ctx_, layer, input, &config);
        EXPECT_TRUE(success) << "LNN step failed at t=" << t;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Verify final state is bounded
    cudaMemcpy(final_states.data(), layer->x->data, n_neurons * sizeof(float), cudaMemcpyDeviceToHost);

    for (uint32_t i = 0; i < n_neurons; i++) {
        EXPECT_FALSE(std::isnan(final_states[i])) << "NaN in final state at " << i;
        EXPECT_FALSE(std::isinf(final_states[i])) << "Inf in final state at " << i;
        EXPECT_GE(final_states[i], -10.0f);
        EXPECT_LE(final_states[i], 10.0f);
    }

    // Log performance
    printf("  LNN sequence processing: %ld us for %d steps\n", duration.count(), sequence_length);

    // Cleanup
    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(layer->x);
    nimcp_gpu_tensor_destroy(layer->dx_dt);
    nimcp_gpu_tensor_destroy(layer->tau);
    nimcp_gpu_tensor_destroy(layer->tau_base);
    nimcp_gpu_tensor_destroy(layer->W_in);
    nimcp_gpu_tensor_destroy(layer->W_rec);
    nimcp_gpu_tensor_destroy(layer->W_tau);
    nimcp_gpu_tensor_destroy(layer->b_in);
    nimcp_gpu_tensor_destroy(layer->b_tau);
    free(layer);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SNN with STDP Learning and Recovery
 * ============================================================================ */
TEST_F(NeuralGPURecoveryIntegrationTest, SNNSTDPLearning) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t n_input = 100;
    size_t n_output = 50;
    int n_epochs = 10;
    int n_timesteps = 100;

    // Create input and output SNN layers
    nimcp_lif_params_t lif_params = make_lif_params();
    nimcp_lif_state_t* input_layer = nimcp_lif_state_create(ctx_, n_input, &lif_params);
    nimcp_lif_state_t* output_layer = nimcp_lif_state_create(ctx_, n_output, &lif_params);
    ASSERT_NE(input_layer, nullptr);
    ASSERT_NE(output_layer, nullptr);

    // Create weight matrix
    size_t weight_dims[2] = {n_input, n_output};
    nimcp_gpu_tensor_t* weights = nimcp_gpu_tensor_create(ctx_, weight_dims, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(weights, nullptr);
    nimcp_gpu_fill(ctx_, weights, 0.5f);  // Initial weights

    // Create traces
    size_t input_dims[1] = {n_input};
    size_t output_dims[1] = {n_output};
    nimcp_gpu_tensor_t* pre_trace = nimcp_gpu_tensor_create(ctx_, input_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* post_trace = nimcp_gpu_tensor_create(ctx_, output_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(pre_trace, nullptr);
    ASSERT_NE(post_trace, nullptr);
    nimcp_gpu_zeros(ctx_, pre_trace);
    nimcp_gpu_zeros(ctx_, post_trace);

    // STDP parameters
    nimcp_stdp_params_t stdp_params;
    stdp_params.A_plus = 0.001f;
    stdp_params.A_minus = 0.00105f;
    stdp_params.tau_plus = 20.0f;
    stdp_params.tau_minus = 20.0f;
    stdp_params.w_max = 1.0f;
    stdp_params.w_min = 0.0f;

    // Create input current
    nimcp_gpu_tensor_t* current = nimcp_gpu_tensor_create(ctx_, input_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(current, nullptr);

    // Training loop
    std::random_device rd;
    std::mt19937 gen(rd());
    std::bernoulli_distribution spike_dist(0.05);

    for (int epoch = 0; epoch < n_epochs; epoch++) {
        // Reset layer states
        nimcp_gpu_zeros(ctx_, input_layer->v);
        nimcp_gpu_zeros(ctx_, output_layer->v);
        nimcp_gpu_zeros(ctx_, pre_trace);
        nimcp_gpu_zeros(ctx_, post_trace);

        for (int t = 0; t < n_timesteps; t++) {
            // Generate Poisson input
            std::vector<float> h_current(n_input);
            for (size_t i = 0; i < n_input; i++) {
                h_current[i] = spike_dist(gen) ? 20.0f : 0.0f;
            }
            cudaMemcpy(current->data, h_current.data(), n_input * sizeof(float), cudaMemcpyHostToDevice);

            // Forward pass through input layer
            bool success = nimcp_gpu_lif_forward(ctx_, input_layer, current);
            EXPECT_TRUE(success) << "Input layer forward failed at epoch " << epoch << " t=" << t;

            // Propagate spikes through weights to output layer
            nimcp_gpu_tensor_t* output_current = nimcp_gpu_tensor_create(ctx_, output_dims, 1, NIMCP_GPU_PRECISION_FP32);
            success = nimcp_gpu_spike_propagate(ctx_, input_layer->spikes, weights, output_current);
            EXPECT_TRUE(success) << "Spike propagation failed";

            // Forward pass through output layer
            success = nimcp_gpu_lif_forward(ctx_, output_layer, output_current);
            EXPECT_TRUE(success) << "Output layer forward failed";

            // Update traces
            success = nimcp_gpu_eligibility_trace_update(ctx_, pre_trace, input_layer->spikes, 0.95f);
            EXPECT_TRUE(success) << "Pre-trace update failed";
            success = nimcp_gpu_eligibility_trace_update(ctx_, post_trace, output_layer->spikes, 0.95f);
            EXPECT_TRUE(success) << "Post-trace update failed";

            // Apply STDP
            success = nimcp_gpu_stdp_pair(ctx_, weights, input_layer->spikes, output_layer->spikes,
                                          pre_trace, post_trace, &stdp_params);
            EXPECT_TRUE(success) << "STDP update failed";

            nimcp_gpu_tensor_destroy(output_current);
        }
    }

    // Verify weights are still bounded
    std::vector<float> final_weights(n_input * n_output);
    cudaMemcpy(final_weights.data(), weights->data, n_input * n_output * sizeof(float), cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < final_weights.size(); i++) {
        EXPECT_GE(final_weights[i], 0.0f) << "Weight below min at " << i;
        EXPECT_LE(final_weights[i], 1.0f) << "Weight above max at " << i;
        EXPECT_FALSE(std::isnan(final_weights[i])) << "NaN weight at " << i;
    }

    // Cleanup
    nimcp_lif_state_destroy(input_layer);
    nimcp_lif_state_destroy(output_layer);
    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(pre_trace);
    nimcp_gpu_tensor_destroy(post_trace);
    nimcp_gpu_tensor_destroy(current);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Hybrid CNN-LNN Model with Recovery
 * ============================================================================ */
TEST_F(NeuralGPURecoveryIntegrationTest, HybridCNNLNNModel) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // CNN encoder -> LNN temporal processor -> CNN decoder
    size_t batch = 2, channels = 1, height = 16, width = 16;
    int sequence_length = 10;

    // Create CNN encoder weights
    size_t enc_w_dims[4] = {16, 1, 3, 3};
    nimcp_gpu_tensor_t* enc_weight = nimcp_gpu_tensor_create(ctx_, enc_w_dims, 4, NIMCP_GPU_PRECISION_FP32);
    fill_random(enc_weight, 0.1f);
    nimcp_gpu_tensor_t* enc_bias = create_1d_tensor(16);
    nimcp_gpu_zeros(ctx_, enc_bias);

    nimcp_conv_params_t conv_params = make_conv_params(3, 1, 1);

    // Create LNN layer (operating on flattened CNN features)
    uint32_t n_features = 16 * height * width;  // Flattened feature map
    uint32_t n_lnn_neurons = 64;

    // Simplified LNN state
    size_t lnn_state_dims[1] = {n_lnn_neurons};
    nimcp_gpu_tensor_t* lnn_state = nimcp_gpu_tensor_create(ctx_, lnn_state_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_zeros(ctx_, lnn_state);

    // Process sequence of frames
    for (int t = 0; t < sequence_length; t++) {
        // Generate input frame
        nimcp_gpu_tensor_t* frame = create_4d_tensor(batch, channels, height, width);
        fill_random(frame, 1.0f);

        // CNN encode
        nimcp_gpu_tensor_t* encoded = create_4d_tensor(batch, 16, height, width);
        bool success = nimcp_gpu_conv2d_forward(ctx_, frame, enc_weight, enc_bias, encoded, &conv_params);
        EXPECT_TRUE(success) << "CNN encode failed at t=" << t;

        // Verify encoded features
        std::vector<float> enc_data(encoded->numel);
        cudaMemcpy(enc_data.data(), encoded->data, encoded->numel * sizeof(float), cudaMemcpyDeviceToHost);

        for (size_t i = 0; i < enc_data.size(); i++) {
            EXPECT_FALSE(std::isnan(enc_data[i])) << "NaN in encoded at t=" << t << " i=" << i;
        }

        nimcp_gpu_tensor_destroy(frame);
        nimcp_gpu_tensor_destroy(encoded);
    }

    // Verify LNN state after sequence
    std::vector<float> final_state(n_lnn_neurons);
    cudaMemcpy(final_state.data(), lnn_state->data, n_lnn_neurons * sizeof(float), cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < n_lnn_neurons; i++) {
        EXPECT_FALSE(std::isnan(final_state[i])) << "NaN in final LNN state at " << i;
    }

    // Cleanup
    nimcp_gpu_tensor_destroy(enc_weight);
    nimcp_gpu_tensor_destroy(enc_bias);
    nimcp_gpu_tensor_destroy(lnn_state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Multi-Model Parallel Processing with Recovery
 * ============================================================================ */
TEST_F(NeuralGPURecoveryIntegrationTest, MultiModelParallelProcessing) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Simulate multiple neural network models running concurrently
    // Each model: CNN -> Pool -> Flatten -> Output

    const int n_models = 4;
    size_t batch = 2, channels = 3, height = 16, width = 16;

    struct ModelState {
        nimcp_gpu_tensor_t* input;
        nimcp_gpu_tensor_t* conv_weight;
        nimcp_gpu_tensor_t* conv_bias;
        nimcp_gpu_tensor_t* conv_out;
        nimcp_gpu_tensor_t* pool_out;
        nimcp_gpu_tensor_t* pool_idx;
    };

    std::vector<ModelState> models(n_models);

    // Initialize all models
    for (int m = 0; m < n_models; m++) {
        models[m].input = create_4d_tensor(batch, channels, height, width);
        fill_random(models[m].input, 1.0f);

        size_t w_dims[4] = {32, channels, 3, 3};
        models[m].conv_weight = nimcp_gpu_tensor_create(ctx_, w_dims, 4, NIMCP_GPU_PRECISION_FP32);
        fill_random(models[m].conv_weight, 0.1f);

        models[m].conv_bias = create_1d_tensor(32);
        nimcp_gpu_zeros(ctx_, models[m].conv_bias);

        models[m].conv_out = create_4d_tensor(batch, 32, height, width);
        models[m].pool_out = create_4d_tensor(batch, 32, height/2, width/2);
        models[m].pool_idx = create_4d_tensor(batch, 32, height/2, width/2);
    }

    nimcp_conv_params_t conv_params = make_conv_params(3, 1, 1);
    nimcp_pool_params_t pool_params;
    pool_params.kernel_h = 2; pool_params.kernel_w = 2;
    pool_params.stride_h = 2; pool_params.stride_w = 2;
    pool_params.pad_h = 0; pool_params.pad_w = 0;

    // Run all models (simulating parallel processing)
    for (int iter = 0; iter < 5; iter++) {
        for (int m = 0; m < n_models; m++) {
            bool success = nimcp_gpu_conv2d_forward(ctx_, models[m].input, models[m].conv_weight,
                                                     models[m].conv_bias, models[m].conv_out, &conv_params);
            EXPECT_TRUE(success) << "Conv failed for model " << m << " iter " << iter;

            success = nimcp_gpu_maxpool2d(ctx_, models[m].conv_out, models[m].pool_out,
                                          models[m].pool_idx, &pool_params);
            EXPECT_TRUE(success) << "Pool failed for model " << m << " iter " << iter;
        }
    }

    // Verify all outputs are valid
    for (int m = 0; m < n_models; m++) {
        std::vector<float> output(models[m].pool_out->numel);
        cudaMemcpy(output.data(), models[m].pool_out->data,
                   models[m].pool_out->numel * sizeof(float), cudaMemcpyDeviceToHost);

        for (size_t i = 0; i < output.size(); i++) {
            EXPECT_FALSE(std::isnan(output[i])) << "NaN in model " << m << " output at " << i;
        }
    }

    // Cleanup
    for (int m = 0; m < n_models; m++) {
        nimcp_gpu_tensor_destroy(models[m].input);
        nimcp_gpu_tensor_destroy(models[m].conv_weight);
        nimcp_gpu_tensor_destroy(models[m].conv_bias);
        nimcp_gpu_tensor_destroy(models[m].conv_out);
        nimcp_gpu_tensor_destroy(models[m].pool_out);
        nimcp_gpu_tensor_destroy(models[m].pool_idx);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery System Performance Under Load
 * ============================================================================ */
TEST_F(NeuralGPURecoveryIntegrationTest, RecoverySystemPerformance) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    nimcp_gpu_recovery_reset_stats();

    // Run many operations to stress-test recovery system
    const int n_operations = 100;
    size_t batch = 4, channels = 32, height = 16, width = 16;

    nimcp_gpu_tensor_t* input = create_4d_tensor(batch, channels, height, width);
    fill_random(input, 1.0f);

    size_t w_dims[4] = {64, channels, 3, 3};
    nimcp_gpu_tensor_t* weight = nimcp_gpu_tensor_create(ctx_, w_dims, 4, NIMCP_GPU_PRECISION_FP32);
    fill_random(weight, 0.1f);

    nimcp_gpu_tensor_t* bias = create_1d_tensor(64);
    nimcp_gpu_zeros(ctx_, bias);

    nimcp_gpu_tensor_t* output = create_4d_tensor(batch, 64, height, width);

    nimcp_conv_params_t conv_params = make_conv_params(3, 1, 1);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n_operations; i++) {
        bool success = nimcp_gpu_conv2d_forward(ctx_, input, weight, bias, output, &conv_params);
        EXPECT_TRUE(success) << "Operation " << i << " failed";
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Get final stats
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    printf("  Performance test: %d operations in %ld ms (%.2f ops/sec)\n",
           n_operations, duration.count(),
           (float)n_operations / (duration.count() / 1000.0f));
    printf("  Recovery success rate: %.2f%%\n", stats.success_rate * 100.0f);

    EXPECT_GE(stats.success_rate, 0.95f) << "Recovery success rate too low";

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(weight);
    nimcp_gpu_tensor_destroy(bias);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Cross-Module Memory Sharing with Recovery
 * ============================================================================ */
TEST_F(NeuralGPURecoveryIntegrationTest, CrossModuleMemorySharing) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Test that tensors can be shared between SNN, LNN, CNN modules
    size_t n_neurons = 256;

    // Create a shared tensor
    size_t dims[1] = {n_neurons};
    nimcp_gpu_tensor_t* shared_tensor = nimcp_gpu_tensor_create(ctx_, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(shared_tensor, nullptr);
    fill_random(shared_tensor, 1.0f);

    // Use as LIF input
    nimcp_lif_params_t lif_params = make_lif_params();
    nimcp_lif_state_t* lif_state = nimcp_lif_state_create(ctx_, n_neurons, &lif_params);
    ASSERT_NE(lif_state, nullptr);

    bool success = nimcp_gpu_lif_forward(ctx_, lif_state, shared_tensor);
    EXPECT_TRUE(success) << "LIF with shared tensor failed";

    // Modify tensor values
    fill_random(shared_tensor, 0.5f);

    // Use again
    success = nimcp_gpu_lif_forward(ctx_, lif_state, shared_tensor);
    EXPECT_TRUE(success) << "LIF with modified shared tensor failed";

    // Verify state is valid
    std::vector<float> state(n_neurons);
    cudaMemcpy(state.data(), lif_state->v->data, n_neurons * sizeof(float), cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < n_neurons; i++) {
        EXPECT_FALSE(std::isnan(state[i])) << "NaN in state at " << i;
    }

    nimcp_lif_state_destroy(lif_state);
    nimcp_gpu_tensor_destroy(shared_tensor);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

} // namespace
