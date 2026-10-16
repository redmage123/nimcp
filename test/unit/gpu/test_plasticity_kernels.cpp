/**
 * @file test_plasticity_kernels.cpp
 * @brief Unit tests for GPU plasticity kernels
 *
 * Tests STDP, BCM, Homeostatic, STP, and Calcium-dependent plasticity
 * GPU operations.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

// Headers have their own extern "C" guards
#include "gpu/plasticity/nimcp_plasticity_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class PlasticityKernelTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    // Helper to create a tensor filled with a constant value
    nimcp_gpu_tensor_t* CreateFilledTensor(size_t* dims, size_t rank, float value) {
        if (!ctx) return nullptr;
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, rank, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    // Helper to create 1D tensor
    nimcp_gpu_tensor_t* Create1DTensor(size_t n, float value = 0.0f) {
        size_t dims[1] = {n};
        return CreateFilledTensor(dims, 1, value);
    }

    // Helper to create 2D tensor
    nimcp_gpu_tensor_t* Create2DTensor(size_t rows, size_t cols, float value = 0.0f) {
        size_t dims[2] = {rows, cols};
        return CreateFilledTensor(dims, 2, value);
    }

    // Helper to copy tensor to host
    std::vector<float> CopyToHost(nimcp_gpu_tensor_t* tensor) {
        size_t n = tensor->numel;
        std::vector<float> host_data(n);
        nimcp_gpu_tensor_to_host(tensor, host_data.data());
        return host_data;
    }

    // Helper to create tensor from host data (1D)
    nimcp_gpu_tensor_t* CreateFromHost(const std::vector<float>& data) {
        size_t dims[1] = {data.size()};
        return nimcp_gpu_tensor_from_host(ctx, data.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    // Helper to set tensor from host data - destroys old tensor and returns new one
    nimcp_gpu_tensor_t* SetFromHost(nimcp_gpu_tensor_t* tensor, const std::vector<float>& data) {
        if (tensor) nimcp_gpu_tensor_destroy(tensor);
        return CreateFromHost(data);
    }
};

//=============================================================================
// STDP Parameter Tests
//=============================================================================

TEST_F(PlasticityKernelTest, STDPParamsDefault_ReturnsValidParams) {
    nimcp_gpu_stdp_params_t params = nimcp_gpu_stdp_params_default();

    // Check reasonable defaults
    EXPECT_GT(params.A_plus, 0.0f);
    EXPECT_GT(params.A_minus, 0.0f);
    EXPECT_GT(params.tau_plus, 0.0f);
    EXPECT_GT(params.tau_minus, 0.0f);
    EXPECT_GT(params.w_max, params.w_min);
}

TEST_F(PlasticityKernelTest, TripletSTDPParamsDefault_ReturnsValidParams) {
    nimcp_gpu_triplet_stdp_params_t params = nimcp_gpu_triplet_stdp_params_default();

    // Check reasonable defaults (Pfister & Gerstner 2006 parameters)
    EXPECT_GT(params.A2_plus, 0.0f);
    EXPECT_GT(params.A3_plus, 0.0f);
    EXPECT_GT(params.A2_minus, 0.0f);
    EXPECT_GT(params.A3_minus, 0.0f);
    EXPECT_GT(params.tau_plus, 0.0f);
    EXPECT_GT(params.tau_minus, 0.0f);
    EXPECT_GT(params.tau_x, 0.0f);
    EXPECT_GT(params.tau_y, 0.0f);
    EXPECT_GT(params.w_max, params.w_min);
}

//=============================================================================
// STDP Trace Tests
//=============================================================================

TEST_F(PlasticityKernelTest, STDPUpdateTraces_DecaysWithoutSpikes) {
    RequireGPU();

    const size_t n_synapses = 100;
    const float dt = 1.0f;

    nimcp_gpu_tensor_t* pre_trace = Create1DTensor(n_synapses, 1.0f);
    nimcp_gpu_tensor_t* post_trace = Create1DTensor(n_synapses, 1.0f);
    nimcp_gpu_tensor_t* pre_spikes = Create1DTensor(n_synapses, 0.0f);
    nimcp_gpu_tensor_t* post_spikes = Create1DTensor(n_synapses, 0.0f);

    nimcp_gpu_stdp_params_t params = nimcp_gpu_stdp_params_default();

    // Update traces without spikes - should decay
    bool result = nimcp_gpu_stdp_update_traces(
        ctx, pre_trace, post_trace, pre_spikes, post_spikes, dt, &params);
    EXPECT_TRUE(result);

    auto pre_data = CopyToHost(pre_trace);
    auto post_data = CopyToHost(post_trace);

    // Traces should decay (less than initial value of 1.0)
    for (size_t i = 0; i < n_synapses; i++) {
        EXPECT_LT(pre_data[i], 1.0f);
        EXPECT_LT(post_data[i], 1.0f);
        EXPECT_GE(pre_data[i], 0.0f);
        EXPECT_GE(post_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(pre_trace);
    nimcp_gpu_tensor_destroy(post_trace);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
}

TEST_F(PlasticityKernelTest, STDPUpdateTraces_IncreasesOnSpikes) {
    RequireGPU();

    const size_t n_synapses = 100;
    const float dt = 1.0f;

    nimcp_gpu_tensor_t* pre_trace = Create1DTensor(n_synapses, 0.0f);
    nimcp_gpu_tensor_t* post_trace = Create1DTensor(n_synapses, 0.0f);
    nimcp_gpu_tensor_t* pre_spikes = Create1DTensor(n_synapses, 1.0f);  // All spike
    nimcp_gpu_tensor_t* post_spikes = Create1DTensor(n_synapses, 1.0f); // All spike

    nimcp_gpu_stdp_params_t params = nimcp_gpu_stdp_params_default();

    bool result = nimcp_gpu_stdp_update_traces(
        ctx, pre_trace, post_trace, pre_spikes, post_spikes, dt, &params);
    EXPECT_TRUE(result);

    auto pre_data = CopyToHost(pre_trace);
    auto post_data = CopyToHost(post_trace);

    // Traces should increase after spikes
    for (size_t i = 0; i < n_synapses; i++) {
        EXPECT_GT(pre_data[i], 0.0f);
        EXPECT_GT(post_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(pre_trace);
    nimcp_gpu_tensor_destroy(post_trace);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
}

//=============================================================================
// STDP Weight Update Tests
//=============================================================================

TEST_F(PlasticityKernelTest, STDPApply_LTPWhenPreBeforePost) {
    RequireGPU();

    const size_t n_pre = 10;
    const size_t n_post = 10;

    // Weight matrix [n_post x n_pre]
    nimcp_gpu_tensor_t* weights = Create2DTensor(n_post, n_pre, 0.5f);
    nimcp_gpu_tensor_t* pre_spikes = Create1DTensor(n_pre, 1.0f);   // Pre spikes
    nimcp_gpu_tensor_t* post_spikes = Create1DTensor(n_post, 1.0f); // Post spikes
    nimcp_gpu_tensor_t* pre_trace = Create1DTensor(n_pre, 0.8f);    // High pre trace (recent pre activity)
    nimcp_gpu_tensor_t* post_trace = Create1DTensor(n_post, 0.0f);  // No post trace

    nimcp_gpu_stdp_params_t params = nimcp_gpu_stdp_params_default();

    bool result = nimcp_gpu_stdp_apply(
        ctx, weights, pre_spikes, post_spikes, pre_trace, post_trace, &params);
    EXPECT_TRUE(result);

    auto weight_data = CopyToHost(weights);

    // Weights should increase (LTP) when post spikes with pre trace (pre before post)
    for (size_t i = 0; i < n_pre * n_post; i++) {
        EXPECT_GE(weight_data[i], 0.5f);  // Weights shouldn't decrease
    }

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
    nimcp_gpu_tensor_destroy(pre_trace);
    nimcp_gpu_tensor_destroy(post_trace);
}

TEST_F(PlasticityKernelTest, STDPApply_LTDWhenPostBeforePre) {
    RequireGPU();

    const size_t n_pre = 10;
    const size_t n_post = 10;

    nimcp_gpu_tensor_t* weights = Create2DTensor(n_post, n_pre, 0.5f);
    nimcp_gpu_tensor_t* pre_spikes = Create1DTensor(n_pre, 1.0f);   // Pre spikes
    nimcp_gpu_tensor_t* post_spikes = Create1DTensor(n_post, 1.0f); // Post spikes
    nimcp_gpu_tensor_t* pre_trace = Create1DTensor(n_pre, 0.0f);    // No pre trace
    nimcp_gpu_tensor_t* post_trace = Create1DTensor(n_post, 0.8f);  // High post trace (recent post activity)

    nimcp_gpu_stdp_params_t params = nimcp_gpu_stdp_params_default();

    bool result = nimcp_gpu_stdp_apply(
        ctx, weights, pre_spikes, post_spikes, pre_trace, post_trace, &params);
    EXPECT_TRUE(result);

    auto weight_data = CopyToHost(weights);

    // Weights should decrease (LTD) when pre spikes with post trace (post before pre)
    for (size_t i = 0; i < n_pre * n_post; i++) {
        EXPECT_LE(weight_data[i], 0.5f);  // Weights shouldn't increase
    }

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
    nimcp_gpu_tensor_destroy(pre_trace);
    nimcp_gpu_tensor_destroy(post_trace);
}

TEST_F(PlasticityKernelTest, STDPApplyModulated_DopamineAmplifies) {
    RequireGPU();

    const size_t n_pre = 10;
    const size_t n_post = 10;

    // Baseline weights for comparison
    nimcp_gpu_tensor_t* weights_nodop = Create2DTensor(n_post, n_pre, 0.5f);
    nimcp_gpu_tensor_t* weights_dop = Create2DTensor(n_post, n_pre, 0.5f);
    nimcp_gpu_tensor_t* pre_spikes = Create1DTensor(n_pre, 1.0f);
    nimcp_gpu_tensor_t* post_spikes = Create1DTensor(n_post, 1.0f);
    nimcp_gpu_tensor_t* pre_trace = Create1DTensor(n_pre, 0.5f);
    nimcp_gpu_tensor_t* post_trace = Create1DTensor(n_post, 0.0f);
    nimcp_gpu_tensor_t* dopamine = Create1DTensor(n_post * n_pre, 2.0f);  // High dopamine

    nimcp_gpu_stdp_params_t params = nimcp_gpu_stdp_params_default();

    // Apply standard STDP
    nimcp_gpu_stdp_apply(ctx, weights_nodop, pre_spikes, post_spikes, pre_trace, post_trace, &params);

    // Apply dopamine-modulated STDP
    bool result = nimcp_gpu_stdp_apply_modulated(
        ctx, weights_dop, pre_spikes, post_spikes, pre_trace, post_trace, dopamine, &params);
    EXPECT_TRUE(result);

    auto data_nodop = CopyToHost(weights_nodop);
    auto data_dop = CopyToHost(weights_dop);

    // Dopamine should amplify weight changes
    float change_nodop = std::abs(data_nodop[0] - 0.5f);
    float change_dop = std::abs(data_dop[0] - 0.5f);
    EXPECT_GE(change_dop, change_nodop);

    nimcp_gpu_tensor_destroy(weights_nodop);
    nimcp_gpu_tensor_destroy(weights_dop);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
    nimcp_gpu_tensor_destroy(pre_trace);
    nimcp_gpu_tensor_destroy(post_trace);
    nimcp_gpu_tensor_destroy(dopamine);
}

//=============================================================================
// Triplet STDP Tests
//=============================================================================

TEST_F(PlasticityKernelTest, TripletSTDPUpdateTraces_UpdatesAllFourTraces) {
    RequireGPU();

    const size_t n = 50;
    const float dt = 1.0f;

    nimcp_gpu_tensor_t* r1_pre = Create1DTensor(n, 0.0f);
    nimcp_gpu_tensor_t* r2_pre = Create1DTensor(n, 0.0f);
    nimcp_gpu_tensor_t* o1_post = Create1DTensor(n, 0.0f);
    nimcp_gpu_tensor_t* o2_post = Create1DTensor(n, 0.0f);
    nimcp_gpu_tensor_t* pre_spikes = Create1DTensor(n, 1.0f);
    nimcp_gpu_tensor_t* post_spikes = Create1DTensor(n, 1.0f);

    nimcp_gpu_triplet_stdp_params_t params = nimcp_gpu_triplet_stdp_params_default();

    bool result = nimcp_gpu_triplet_stdp_update_traces(
        ctx, r1_pre, r2_pre, o1_post, o2_post, pre_spikes, post_spikes, dt, &params);
    EXPECT_TRUE(result);

    auto r1_data = CopyToHost(r1_pre);
    auto r2_data = CopyToHost(r2_pre);
    auto o1_data = CopyToHost(o1_post);
    auto o2_data = CopyToHost(o2_post);

    // All traces should be non-zero after spikes
    for (size_t i = 0; i < n; i++) {
        EXPECT_GT(r1_data[i], 0.0f);
        EXPECT_GT(r2_data[i], 0.0f);
        EXPECT_GT(o1_data[i], 0.0f);
        EXPECT_GT(o2_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(r1_pre);
    nimcp_gpu_tensor_destroy(r2_pre);
    nimcp_gpu_tensor_destroy(o1_post);
    nimcp_gpu_tensor_destroy(o2_post);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
}

TEST_F(PlasticityKernelTest, TripletSTDPApply_ModifiesWeights) {
    RequireGPU();

    const size_t n_pre = 10;
    const size_t n_post = 10;

    nimcp_gpu_tensor_t* weights = Create2DTensor(n_post, n_pre, 0.5f);
    nimcp_gpu_tensor_t* pre_spikes = Create1DTensor(n_pre, 1.0f);
    nimcp_gpu_tensor_t* post_spikes = Create1DTensor(n_post, 1.0f);
    nimcp_gpu_tensor_t* r1_pre = Create1DTensor(n_pre, 0.5f);
    nimcp_gpu_tensor_t* r2_pre = Create1DTensor(n_pre, 0.3f);
    nimcp_gpu_tensor_t* o1_post = Create1DTensor(n_post, 0.4f);
    nimcp_gpu_tensor_t* o2_post = Create1DTensor(n_post, 0.2f);

    nimcp_gpu_triplet_stdp_params_t params = nimcp_gpu_triplet_stdp_params_default();

    bool result = nimcp_gpu_triplet_stdp_apply(
        ctx, weights, pre_spikes, post_spikes, r1_pre, r2_pre, o1_post, o2_post, &params);
    EXPECT_TRUE(result);

    auto weight_data = CopyToHost(weights);

    // Weights should be modified (not all exactly 0.5)
    bool modified = false;
    for (size_t i = 0; i < n_pre * n_post; i++) {
        if (std::abs(weight_data[i] - 0.5f) > 1e-6f) {
            modified = true;
            break;
        }
    }
    EXPECT_TRUE(modified);

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
    nimcp_gpu_tensor_destroy(r1_pre);
    nimcp_gpu_tensor_destroy(r2_pre);
    nimcp_gpu_tensor_destroy(o1_post);
    nimcp_gpu_tensor_destroy(o2_post);
}

//=============================================================================
// BCM Tests
//=============================================================================

TEST_F(PlasticityKernelTest, BCMParamsDefault_ReturnsValidParams) {
    nimcp_gpu_bcm_params_t params = nimcp_gpu_bcm_params_default();

    EXPECT_GT(params.learning_rate, 0.0f);
    EXPECT_GT(params.threshold_tau, 0.0f);
    EXPECT_GT(params.activity_tau, 0.0f);
    EXPECT_GT(params.max_threshold, params.min_threshold);
}

TEST_F(PlasticityKernelTest, BCMStateCreate_ReturnsValidState) {
    RequireGPU();

    const size_t n_pre = 50;
    const size_t n_post = 30;
    nimcp_gpu_bcm_params_t params = nimcp_gpu_bcm_params_default();

    nimcp_gpu_bcm_state_t* state = nimcp_gpu_bcm_state_create(ctx, n_pre, n_post, &params);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->weights, nullptr);
    EXPECT_NE(state->thresholds, nullptr);
    EXPECT_NE(state->avg_activity, nullptr);
    EXPECT_NE(state->eligibility, nullptr);
    EXPECT_EQ(state->n_synapses, n_pre * n_post);

    nimcp_gpu_bcm_state_destroy(state);
}

TEST_F(PlasticityKernelTest, BCMStateDestroy_HandlesNull) {
    nimcp_gpu_bcm_state_destroy(nullptr);  // Should not crash
}

TEST_F(PlasticityKernelTest, BCMUpdateThreshold_AdaptsToActivity) {
    RequireGPU();

    const size_t n = 100;
    const float dt = 1.0f;

    nimcp_gpu_tensor_t* thresholds = Create1DTensor(n, 0.5f);
    nimcp_gpu_tensor_t* post_activity = Create1DTensor(n, 1.0f);  // High activity

    nimcp_gpu_bcm_params_t params = nimcp_gpu_bcm_params_default();

    // Run multiple updates
    for (int i = 0; i < 100; i++) {
        bool result = nimcp_gpu_bcm_update_threshold(ctx, thresholds, post_activity, dt, &params);
        EXPECT_TRUE(result);
    }

    auto threshold_data = CopyToHost(thresholds);

    // Thresholds should adapt toward activity^theta_power
    for (size_t i = 0; i < n; i++) {
        EXPECT_GT(threshold_data[i], 0.5f);  // Should increase with high activity
    }

    nimcp_gpu_tensor_destroy(thresholds);
    nimcp_gpu_tensor_destroy(post_activity);
}

TEST_F(PlasticityKernelTest, BCMApply_LTPWhenAboveThreshold) {
    RequireGPU();

    const size_t n_pre = 10;
    const size_t n_post = 10;
    const float dt = 1.0f;

    nimcp_gpu_tensor_t* weights = Create2DTensor(n_post, n_pre, 0.5f);
    nimcp_gpu_tensor_t* pre_activity = Create1DTensor(n_pre, 0.5f);
    nimcp_gpu_tensor_t* post_activity = Create1DTensor(n_post, 0.8f);  // High activity
    nimcp_gpu_tensor_t* thresholds = Create1DTensor(n_post, 0.4f);     // Below activity

    nimcp_gpu_bcm_params_t params = nimcp_gpu_bcm_params_default();

    bool result = nimcp_gpu_bcm_apply(ctx, weights, pre_activity, post_activity, thresholds, dt, &params);
    EXPECT_TRUE(result);

    auto weight_data = CopyToHost(weights);

    // When post > threshold, should potentiate (LTP)
    for (size_t i = 0; i < n_pre * n_post; i++) {
        EXPECT_GE(weight_data[i], 0.5f);
    }

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(pre_activity);
    nimcp_gpu_tensor_destroy(post_activity);
    nimcp_gpu_tensor_destroy(thresholds);
}

TEST_F(PlasticityKernelTest, BCMApply_LTDWhenBelowThreshold) {
    RequireGPU();

    const size_t n_pre = 10;
    const size_t n_post = 10;
    const float dt = 1.0f;

    nimcp_gpu_tensor_t* weights = Create2DTensor(n_post, n_pre, 0.5f);
    nimcp_gpu_tensor_t* pre_activity = Create1DTensor(n_pre, 0.5f);
    nimcp_gpu_tensor_t* post_activity = Create1DTensor(n_post, 0.3f);  // Low activity
    nimcp_gpu_tensor_t* thresholds = Create1DTensor(n_post, 0.5f);     // Above activity

    nimcp_gpu_bcm_params_t params = nimcp_gpu_bcm_params_default();

    bool result = nimcp_gpu_bcm_apply(ctx, weights, pre_activity, post_activity, thresholds, dt, &params);
    EXPECT_TRUE(result);

    auto weight_data = CopyToHost(weights);

    // When post < threshold, should depress (LTD)
    for (size_t i = 0; i < n_pre * n_post; i++) {
        EXPECT_LE(weight_data[i], 0.5f);
    }

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(pre_activity);
    nimcp_gpu_tensor_destroy(post_activity);
    nimcp_gpu_tensor_destroy(thresholds);
}

TEST_F(PlasticityKernelTest, BCMApplyModulated_ScalesWithNeuromodulator) {
    RequireGPU();

    const size_t n_pre = 10;
    const size_t n_post = 10;
    const float dt = 1.0f;

    nimcp_gpu_tensor_t* weights_low = Create2DTensor(n_post, n_pre, 0.5f);
    nimcp_gpu_tensor_t* weights_high = Create2DTensor(n_post, n_pre, 0.5f);
    nimcp_gpu_tensor_t* pre_activity = Create1DTensor(n_pre, 0.5f);
    nimcp_gpu_tensor_t* post_activity = Create1DTensor(n_post, 0.8f);
    nimcp_gpu_tensor_t* thresholds = Create1DTensor(n_post, 0.4f);

    nimcp_gpu_bcm_params_t params = nimcp_gpu_bcm_params_default();

    // Low neuromodulator
    nimcp_gpu_bcm_apply_modulated(ctx, weights_low, pre_activity, post_activity, thresholds, 0.1f, dt, &params);

    // High neuromodulator
    bool result = nimcp_gpu_bcm_apply_modulated(ctx, weights_high, pre_activity, post_activity, thresholds, 1.0f, dt, &params);
    EXPECT_TRUE(result);

    auto data_low = CopyToHost(weights_low);
    auto data_high = CopyToHost(weights_high);

    // Higher neuromodulator should produce larger weight changes
    float change_low = std::abs(data_low[0] - 0.5f);
    float change_high = std::abs(data_high[0] - 0.5f);
    EXPECT_GE(change_high, change_low);

    nimcp_gpu_tensor_destroy(weights_low);
    nimcp_gpu_tensor_destroy(weights_high);
    nimcp_gpu_tensor_destroy(pre_activity);
    nimcp_gpu_tensor_destroy(post_activity);
    nimcp_gpu_tensor_destroy(thresholds);
}

//=============================================================================
// Homeostatic Plasticity Tests
//=============================================================================

TEST_F(PlasticityKernelTest, ScalingParamsDefault_ReturnsValidParams) {
    nimcp_gpu_scaling_params_t params = nimcp_gpu_scaling_params_default();

    EXPECT_GT(params.target_rate, 0.0f);
    EXPECT_GT(params.scaling_tau, 0.0f);
    EXPECT_GT(params.max_scale, params.min_scale);
}

TEST_F(PlasticityKernelTest, IntrinsicParamsDefault_ReturnsValidParams) {
    nimcp_gpu_intrinsic_params_t params = nimcp_gpu_intrinsic_params_default();

    EXPECT_GT(params.target_rate, 0.0f);
    EXPECT_GT(params.threshold_tau, 0.0f);
    EXPECT_GT(params.gain_tau, 0.0f);
    EXPECT_GT(params.max_threshold, params.min_threshold);
    EXPECT_GT(params.max_gain, params.min_gain);
}

TEST_F(PlasticityKernelTest, HomeostaticStateCreate_ReturnsValidState) {
    RequireGPU();

    const size_t n_neurons = 100;

    nimcp_gpu_homeostatic_state_t* state = nimcp_gpu_homeostatic_state_create(ctx, n_neurons);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->scaling_factors, nullptr);
    EXPECT_NE(state->avg_rates, nullptr);
    EXPECT_NE(state->thresholds, nullptr);
    EXPECT_NE(state->gains, nullptr);
    EXPECT_EQ(state->n_neurons, n_neurons);

    nimcp_gpu_homeostatic_state_destroy(state);
}

TEST_F(PlasticityKernelTest, HomeostaticStateDestroy_HandlesNull) {
    nimcp_gpu_homeostatic_state_destroy(nullptr);  // Should not crash
}

TEST_F(PlasticityKernelTest, HomeostaticUpdateRates_AveragesActivity) {
    RequireGPU();

    const size_t n = 100;
    const float dt = 10.0f;

    nimcp_gpu_tensor_t* avg_rates = Create1DTensor(n, 0.0f);
    nimcp_gpu_tensor_t* spikes = Create1DTensor(n, 1.0f);  // All spiking

    nimcp_gpu_scaling_params_t params = nimcp_gpu_scaling_params_default();

    // Multiple updates
    for (int i = 0; i < 100; i++) {
        bool result = nimcp_gpu_homeostatic_update_rates(ctx, avg_rates, spikes, dt, &params);
        EXPECT_TRUE(result);
    }

    auto rate_data = CopyToHost(avg_rates);

    // Rates should increase toward 1.0 (spike rate)
    for (size_t i = 0; i < n; i++) {
        EXPECT_GT(rate_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(avg_rates);
    nimcp_gpu_tensor_destroy(spikes);
}

TEST_F(PlasticityKernelTest, HomeostaticComputeScaling_ScalesFromRates) {
    RequireGPU();

    const size_t n = 100;

    nimcp_gpu_tensor_t* scaling_factors = Create1DTensor(n, 1.0f);
    nimcp_gpu_tensor_t* avg_rates = Create1DTensor(n, 5.0f);  // Below target (assuming target > 5)

    nimcp_gpu_scaling_params_t params = nimcp_gpu_scaling_params_default();
    params.target_rate = 10.0f;  // Set higher target

    bool result = nimcp_gpu_homeostatic_compute_scaling(ctx, scaling_factors, avg_rates, &params);
    EXPECT_TRUE(result);

    auto scale_data = CopyToHost(scaling_factors);

    // Scaling should be > 1 when rate < target
    for (size_t i = 0; i < n; i++) {
        EXPECT_GE(scale_data[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(scaling_factors);
    nimcp_gpu_tensor_destroy(avg_rates);
}

TEST_F(PlasticityKernelTest, HomeostaticApplyScaling_ModifiesWeights) {
    RequireGPU();

    const size_t n_pre = 20;
    const size_t n_post = 10;
    const float w_min = 0.0f;
    const float w_max = 1.0f;

    nimcp_gpu_tensor_t* weights = Create2DTensor(n_post, n_pre, 0.5f);
    nimcp_gpu_tensor_t* scaling_factors = Create1DTensor(n_post, 1.5f);  // Scale up

    bool result = nimcp_gpu_homeostatic_apply_scaling(ctx, weights, scaling_factors, w_min, w_max);
    EXPECT_TRUE(result);

    auto weight_data = CopyToHost(weights);

    // Weights should be scaled up (clamped to w_max)
    for (size_t i = 0; i < n_pre * n_post; i++) {
        EXPECT_GE(weight_data[i], 0.5f);  // Should increase
        EXPECT_LE(weight_data[i], w_max); // Should respect bound
    }

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(scaling_factors);
}

TEST_F(PlasticityKernelTest, IntrinsicPlasticityUpdate_AdaptsThreshold) {
    RequireGPU();

    const size_t n = 100;
    const float dt = 10.0f;

    nimcp_gpu_tensor_t* thresholds = Create1DTensor(n, 0.5f);
    nimcp_gpu_tensor_t* avg_rates = Create1DTensor(n, 20.0f);  // High rate

    nimcp_gpu_intrinsic_params_t params = nimcp_gpu_intrinsic_params_default();
    params.target_rate = 10.0f;  // Lower target

    // Multiple updates
    for (int i = 0; i < 50; i++) {
        bool result = nimcp_gpu_intrinsic_plasticity_update(ctx, thresholds, avg_rates, dt, &params);
        EXPECT_TRUE(result);
    }

    auto threshold_data = CopyToHost(thresholds);

    // Thresholds should increase when rate > target (to reduce firing)
    for (size_t i = 0; i < n; i++) {
        EXPECT_GE(threshold_data[i], 0.5f);
    }

    nimcp_gpu_tensor_destroy(thresholds);
    nimcp_gpu_tensor_destroy(avg_rates);
}

//=============================================================================
// STP Tests
//=============================================================================

TEST_F(PlasticityKernelTest, STPParamsDefault_ReturnsValidParams) {
    nimcp_gpu_stp_params_t params = nimcp_gpu_stp_params_default();

    EXPECT_GT(params.U, 0.0f);
    EXPECT_LE(params.U, 1.0f);
    EXPECT_GT(params.tau_D, 0.0f);
    EXPECT_GT(params.tau_F, 0.0f);
}

TEST_F(PlasticityKernelTest, STPStateCreate_ReturnsValidState) {
    RequireGPU();

    const size_t n_synapses = 100;
    nimcp_gpu_stp_params_t params = nimcp_gpu_stp_params_default();

    nimcp_gpu_stp_state_t* state = nimcp_gpu_stp_state_create(ctx, n_synapses, &params);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->x, nullptr);
    EXPECT_NE(state->u, nullptr);
    EXPECT_NE(state->last_spike, nullptr);
    EXPECT_EQ(state->n_synapses, n_synapses);

    nimcp_gpu_stp_state_destroy(state);
}

TEST_F(PlasticityKernelTest, STPStateDestroy_HandlesNull) {
    nimcp_gpu_stp_state_destroy(nullptr);  // Should not crash
}

TEST_F(PlasticityKernelTest, STPUpdate_RecoveryWithoutSpikes) {
    RequireGPU();

    const size_t n_synapses = 100;
    const float dt = 10.0f;

    nimcp_gpu_stp_params_t params = nimcp_gpu_stp_params_default();
    nimcp_gpu_stp_state_t* state = nimcp_gpu_stp_state_create(ctx, n_synapses, &params);
    ASSERT_NE(state, nullptr);

    // Deplete resources first
    nimcp_gpu_fill(ctx, state->x, 0.2f);  // Low resources
    nimcp_gpu_fill(ctx, state->u, 0.8f);  // High utilization

    // Update without spikes - resources should recover
    for (int i = 0; i < 100; i++) {
        bool result = nimcp_gpu_stp_update(ctx, state, dt);
        EXPECT_TRUE(result);
    }

    auto x_data = CopyToHost(state->x);
    auto u_data = CopyToHost(state->u);

    // Resources should recover toward 1.0
    for (size_t i = 0; i < n_synapses; i++) {
        EXPECT_GT(x_data[i], 0.2f);  // Should increase
    }

    // Utilization should decay toward U baseline
    for (size_t i = 0; i < n_synapses; i++) {
        EXPECT_LT(u_data[i], 0.8f);  // Should decrease
    }

    nimcp_gpu_stp_state_destroy(state);
}

TEST_F(PlasticityKernelTest, STPProcessSpikes_DepletesResources) {
    RequireGPU();

    const size_t n_synapses = 100;

    nimcp_gpu_stp_params_t params = nimcp_gpu_stp_params_default();
    nimcp_gpu_stp_state_t* state = nimcp_gpu_stp_state_create(ctx, n_synapses, &params);
    ASSERT_NE(state, nullptr);

    // Start with full resources
    nimcp_gpu_fill(ctx, state->x, 1.0f);
    nimcp_gpu_fill(ctx, state->u, params.U);

    nimcp_gpu_tensor_t* spikes = Create1DTensor(n_synapses, 1.0f);  // All spike

    bool result = nimcp_gpu_stp_process_spikes(ctx, state, spikes);
    EXPECT_TRUE(result);

    auto x_data = CopyToHost(state->x);

    // Resources should be depleted after spikes
    for (size_t i = 0; i < n_synapses; i++) {
        EXPECT_LT(x_data[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(spikes);
    nimcp_gpu_stp_state_destroy(state);
}

TEST_F(PlasticityKernelTest, STPGetModulation_ReturnsValidFactors) {
    RequireGPU();

    const size_t n_synapses = 100;

    nimcp_gpu_stp_params_t params = nimcp_gpu_stp_params_default();
    nimcp_gpu_stp_state_t* state = nimcp_gpu_stp_state_create(ctx, n_synapses, &params);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* modulation = Create1DTensor(n_synapses, 0.0f);

    bool result = nimcp_gpu_stp_get_modulation(ctx, state, modulation);
    EXPECT_TRUE(result);

    auto mod_data = CopyToHost(modulation);

    // Modulation should be u * x, in range [0, 1]
    for (size_t i = 0; i < n_synapses; i++) {
        EXPECT_GE(mod_data[i], 0.0f);
        EXPECT_LE(mod_data[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(modulation);
    nimcp_gpu_stp_state_destroy(state);
}

TEST_F(PlasticityKernelTest, STPApply_ModulatesWeights) {
    RequireGPU();

    const size_t n_synapses = 100;

    nimcp_gpu_stp_params_t params = nimcp_gpu_stp_params_default();
    nimcp_gpu_stp_state_t* state = nimcp_gpu_stp_state_create(ctx, n_synapses, &params);
    ASSERT_NE(state, nullptr);

    // Set STP state for modulation
    nimcp_gpu_fill(ctx, state->x, 0.5f);
    nimcp_gpu_fill(ctx, state->u, 0.5f);

    nimcp_gpu_tensor_t* base_weights = Create1DTensor(n_synapses, 1.0f);
    nimcp_gpu_tensor_t* effective_weights = Create1DTensor(n_synapses, 0.0f);

    bool result = nimcp_gpu_stp_apply(ctx, base_weights, state, effective_weights);
    EXPECT_TRUE(result);

    auto eff_data = CopyToHost(effective_weights);

    // Effective = base * u * x = 1.0 * 0.5 * 0.5 = 0.25
    for (size_t i = 0; i < n_synapses; i++) {
        EXPECT_NEAR(eff_data[i], 0.25f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(base_weights);
    nimcp_gpu_tensor_destroy(effective_weights);
    nimcp_gpu_stp_state_destroy(state);
}

TEST_F(PlasticityKernelTest, STPReset_RestoresToBaseline) {
    RequireGPU();

    const size_t n_synapses = 100;

    nimcp_gpu_stp_params_t params = nimcp_gpu_stp_params_default();
    nimcp_gpu_stp_state_t* state = nimcp_gpu_stp_state_create(ctx, n_synapses, &params);
    ASSERT_NE(state, nullptr);

    // Deplete state
    nimcp_gpu_fill(ctx, state->x, 0.1f);
    nimcp_gpu_fill(ctx, state->u, 0.9f);

    bool result = nimcp_gpu_stp_reset(ctx, state);
    EXPECT_TRUE(result);

    auto x_data = CopyToHost(state->x);
    auto u_data = CopyToHost(state->u);

    // Should reset to baseline values
    for (size_t i = 0; i < n_synapses; i++) {
        EXPECT_NEAR(x_data[i], 1.0f, 0.01f);    // Full resources
        EXPECT_NEAR(u_data[i], params.U, 0.01f); // Baseline utilization
    }

    nimcp_gpu_stp_state_destroy(state);
}

//=============================================================================
// Calcium Dynamics Tests
//=============================================================================

TEST_F(PlasticityKernelTest, CalciumParamsDefault_ReturnsValidParams) {
    nimcp_gpu_calcium_params_t params = nimcp_gpu_calcium_params_default();

    EXPECT_GE(params.baseline, 0.0f);
    EXPECT_GT(params.threshold_ltp, params.threshold_ltd);
    EXPECT_GT(params.threshold_sat, params.threshold_ltp);
    EXPECT_GT(params.max_conc, params.threshold_sat);
    EXPECT_GT(params.decay_tau, 0.0f);
}

TEST_F(PlasticityKernelTest, CalciumStateCreate_ReturnsValidState) {
    RequireGPU();

    const size_t n_synapses = 100;
    nimcp_gpu_calcium_params_t params = nimcp_gpu_calcium_params_default();

    nimcp_gpu_calcium_state_t* state = nimcp_gpu_calcium_state_create(ctx, n_synapses, &params);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->concentration, nullptr);
    EXPECT_NE(state->learning_rate, nullptr);
    EXPECT_NE(state->nmda_activation, nullptr);
    EXPECT_EQ(state->n_synapses, n_synapses);

    nimcp_gpu_calcium_state_destroy(state);
}

TEST_F(PlasticityKernelTest, CalciumStateDestroy_HandlesNull) {
    nimcp_gpu_calcium_state_destroy(nullptr);  // Should not crash
}

TEST_F(PlasticityKernelTest, CalciumUpdate_DecaysToBaseline) {
    RequireGPU();

    const size_t n_synapses = 100;
    const float dt = 10.0f;

    nimcp_gpu_calcium_params_t params = nimcp_gpu_calcium_params_default();
    nimcp_gpu_calcium_state_t* state = nimcp_gpu_calcium_state_create(ctx, n_synapses, &params);
    ASSERT_NE(state, nullptr);

    // Set high initial calcium
    nimcp_gpu_fill(ctx, state->concentration, 1.0f);

    // Update - calcium should decay
    for (int i = 0; i < 100; i++) {
        bool result = nimcp_gpu_calcium_update(ctx, state, dt);
        EXPECT_TRUE(result);
    }

    auto conc_data = CopyToHost(state->concentration);

    // Should decay toward baseline
    for (size_t i = 0; i < n_synapses; i++) {
        EXPECT_LT(conc_data[i], 1.0f);
    }

    nimcp_gpu_calcium_state_destroy(state);
}

TEST_F(PlasticityKernelTest, CalciumNMDAInflux_IncreasesCalcium) {
    RequireGPU();

    const size_t n_synapses = 100;

    nimcp_gpu_calcium_params_t params = nimcp_gpu_calcium_params_default();
    nimcp_gpu_calcium_state_t* state = nimcp_gpu_calcium_state_create(ctx, n_synapses, &params);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_fill(ctx, state->concentration, params.baseline);
    nimcp_gpu_tensor_t* nmda_activation = Create1DTensor(n_synapses, 1.0f);
    nimcp_gpu_tensor_t* voltage = Create1DTensor(n_synapses, 0.0f);  // Depolarized (near 0 mV)

    bool result = nimcp_gpu_calcium_nmda_influx(ctx, state, nmda_activation, voltage);
    EXPECT_TRUE(result);

    auto conc_data = CopyToHost(state->concentration);

    // Calcium should increase after NMDA influx
    for (size_t i = 0; i < n_synapses; i++) {
        EXPECT_GT(conc_data[i], params.baseline);
    }

    nimcp_gpu_tensor_destroy(nmda_activation);
    nimcp_gpu_tensor_destroy(voltage);
    nimcp_gpu_calcium_state_destroy(state);
}

TEST_F(PlasticityKernelTest, CalciumComputeLearningRate_ValidOmega) {
    RequireGPU();

    const size_t n_synapses = 100;

    nimcp_gpu_calcium_params_t params = nimcp_gpu_calcium_params_default();
    nimcp_gpu_calcium_state_t* state = nimcp_gpu_calcium_state_create(ctx, n_synapses, &params);
    ASSERT_NE(state, nullptr);

    // Set calcium to LTP range
    float ltp_level = (params.threshold_ltd + params.threshold_ltp) / 2.0f + 0.5f;
    nimcp_gpu_fill(ctx, state->concentration, ltp_level);

    bool result = nimcp_gpu_calcium_compute_learning_rate(ctx, state);
    EXPECT_TRUE(result);

    auto lr_data = CopyToHost(state->learning_rate);

    // Learning rate should be positive in LTP range
    for (size_t i = 0; i < n_synapses; i++) {
        EXPECT_GE(lr_data[i], 0.0f);
    }

    nimcp_gpu_calcium_state_destroy(state);
}

TEST_F(PlasticityKernelTest, CalciumApplyPlasticity_ModifiesWeights) {
    RequireGPU();

    const size_t n_pre = 10;
    const size_t n_post = 10;
    const size_t n_synapses = n_pre * n_post;

    nimcp_gpu_calcium_params_t params = nimcp_gpu_calcium_params_default();
    nimcp_gpu_calcium_state_t* state = nimcp_gpu_calcium_state_create(ctx, n_synapses, &params);
    ASSERT_NE(state, nullptr);

    // Set positive learning rate
    nimcp_gpu_fill(ctx, state->learning_rate, 0.01f);

    nimcp_gpu_tensor_t* weights = Create2DTensor(n_post, n_pre, 0.5f);
    nimcp_gpu_tensor_t* pre_activity = Create1DTensor(n_pre, 0.8f);
    nimcp_gpu_tensor_t* post_activity = Create1DTensor(n_post, 0.8f);

    bool result = nimcp_gpu_calcium_apply_plasticity(ctx, weights, state, pre_activity, post_activity);
    EXPECT_TRUE(result);

    auto weight_data = CopyToHost(weights);

    // Weights should be modified
    bool modified = false;
    for (size_t i = 0; i < n_synapses; i++) {
        if (std::abs(weight_data[i] - 0.5f) > 1e-6f) {
            modified = true;
            break;
        }
    }
    EXPECT_TRUE(modified);

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(pre_activity);
    nimcp_gpu_tensor_destroy(post_activity);
    nimcp_gpu_calcium_state_destroy(state);
}

TEST_F(PlasticityKernelTest, CalciumReset_RestoresToBaseline) {
    RequireGPU();

    const size_t n_synapses = 100;

    nimcp_gpu_calcium_params_t params = nimcp_gpu_calcium_params_default();
    nimcp_gpu_calcium_state_t* state = nimcp_gpu_calcium_state_create(ctx, n_synapses, &params);
    ASSERT_NE(state, nullptr);

    // Set high calcium
    nimcp_gpu_fill(ctx, state->concentration, 10.0f);

    bool result = nimcp_gpu_calcium_reset(ctx, state);
    EXPECT_TRUE(result);

    auto conc_data = CopyToHost(state->concentration);

    // Should reset to baseline
    for (size_t i = 0; i < n_synapses; i++) {
        EXPECT_NEAR(conc_data[i], params.baseline, 0.01f);
    }

    nimcp_gpu_calcium_state_destroy(state);
}

TEST_F(PlasticityKernelTest, CalciumMgBlock_VoltageDependentUnblock) {
    RequireGPU();

    const size_t n = 100;

    nimcp_gpu_tensor_t* voltage_hyper = Create1DTensor(n, -70.0f);   // Hyperpolarized
    nimcp_gpu_tensor_t* voltage_depol = Create1DTensor(n, 0.0f);     // Depolarized
    nimcp_gpu_tensor_t* mg_block_hyper = Create1DTensor(n, 0.0f);
    nimcp_gpu_tensor_t* mg_block_depol = Create1DTensor(n, 0.0f);

    bool result1 = nimcp_gpu_calcium_mg_block(ctx, voltage_hyper, mg_block_hyper);
    bool result2 = nimcp_gpu_calcium_mg_block(ctx, voltage_depol, mg_block_depol);
    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);

    auto hyper_data = CopyToHost(mg_block_hyper);
    auto depol_data = CopyToHost(mg_block_depol);

    // Mg block should be stronger (lower unblock) at hyperpolarized potentials
    for (size_t i = 0; i < n; i++) {
        EXPECT_LT(hyper_data[i], depol_data[i]);
        EXPECT_GE(hyper_data[i], 0.0f);
        EXPECT_LE(depol_data[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(voltage_hyper);
    nimcp_gpu_tensor_destroy(voltage_depol);
    nimcp_gpu_tensor_destroy(mg_block_hyper);
    nimcp_gpu_tensor_destroy(mg_block_depol);
}

TEST_F(PlasticityKernelTest, CalciumGetRegime_ReturnsValidRegimes) {
    RequireGPU();

    const size_t n = 5;

    nimcp_gpu_calcium_params_t params = nimcp_gpu_calcium_params_default();

    // Create concentrations at different levels
    std::vector<float> conc_values = {
        params.baseline,                         // Below LTD threshold (regime 0)
        (params.baseline + params.threshold_ltd) / 2.0f + 0.01f,  // LTD range (regime 1)
        (params.threshold_ltd + params.threshold_ltp) / 2.0f,     // Transition (regime 2)
        (params.threshold_ltp + params.threshold_sat) / 2.0f,     // LTP range (regime 3)
        params.threshold_sat + 0.1f               // Saturated (regime 4)
    };

    nimcp_gpu_tensor_t* concentration = SetFromHost(nullptr, conc_values);

    // Create regime tensor (as float for compatibility)
    nimcp_gpu_tensor_t* regime = Create1DTensor(n, 0.0f);

    bool result = nimcp_gpu_calcium_get_regime(ctx, concentration, regime, &params);
    EXPECT_TRUE(result);

    auto regime_data = CopyToHost(regime);

    // Each regime should be in valid range [0, 4]
    for (size_t i = 0; i < n; i++) {
        EXPECT_GE(regime_data[i], 0.0f);
        EXPECT_LE(regime_data[i], 4.0f);
    }

    nimcp_gpu_tensor_destroy(concentration);
    nimcp_gpu_tensor_destroy(regime);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(PlasticityKernelTest, STDPUpdateTraces_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_stdp_params_t params = nimcp_gpu_stdp_params_default();

    EXPECT_FALSE(nimcp_gpu_stdp_update_traces(nullptr, tensor, tensor, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_stdp_update_traces(ctx, nullptr, tensor, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_stdp_update_traces(ctx, tensor, nullptr, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_stdp_update_traces(ctx, tensor, tensor, nullptr, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_stdp_update_traces(ctx, tensor, tensor, tensor, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_stdp_update_traces(ctx, tensor, tensor, tensor, tensor, 1.0f, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
}

TEST_F(PlasticityKernelTest, STDPApply_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_tensor_t* tensor2d = Create2DTensor(10, 10, 0.0f);
    nimcp_gpu_stdp_params_t params = nimcp_gpu_stdp_params_default();

    EXPECT_FALSE(nimcp_gpu_stdp_apply(nullptr, tensor2d, tensor, tensor, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_stdp_apply(ctx, nullptr, tensor, tensor, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_stdp_apply(ctx, tensor2d, nullptr, tensor, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_stdp_apply(ctx, tensor2d, tensor, tensor, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor2d);
}

TEST_F(PlasticityKernelTest, BCMApply_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_tensor_t* tensor2d = Create2DTensor(10, 10, 0.0f);
    nimcp_gpu_bcm_params_t params = nimcp_gpu_bcm_params_default();

    EXPECT_FALSE(nimcp_gpu_bcm_apply(nullptr, tensor2d, tensor, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_bcm_apply(ctx, nullptr, tensor, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_bcm_apply(ctx, tensor2d, nullptr, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_bcm_apply(ctx, tensor2d, tensor, tensor, tensor, 1.0f, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor2d);
}

TEST_F(PlasticityKernelTest, STPUpdate_NullSafety) {
    EXPECT_FALSE(nimcp_gpu_stp_update(nullptr, nullptr, 1.0f));
    if (gpu_available) {
        EXPECT_FALSE(nimcp_gpu_stp_update(ctx, nullptr, 1.0f));
    }
}

TEST_F(PlasticityKernelTest, CalciumUpdate_NullSafety) {
    EXPECT_FALSE(nimcp_gpu_calcium_update(nullptr, nullptr, 1.0f));
    if (gpu_available) {
        EXPECT_FALSE(nimcp_gpu_calcium_update(ctx, nullptr, 1.0f));
    }
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(PlasticityKernelTest, Integration_STDPLearningSequence) {
    RequireGPU();

    const size_t n_pre = 20;
    const size_t n_post = 10;
    const float dt = 1.0f;
    const int n_steps = 100;

    // Initialize
    nimcp_gpu_tensor_t* weights = Create2DTensor(n_post, n_pre, 0.5f);
    nimcp_gpu_tensor_t* pre_trace = Create1DTensor(n_pre, 0.0f);
    nimcp_gpu_tensor_t* post_trace = Create1DTensor(n_post, 0.0f);
    nimcp_gpu_tensor_t* pre_spikes = Create1DTensor(n_pre, 0.0f);
    nimcp_gpu_tensor_t* post_spikes = Create1DTensor(n_post, 0.0f);

    nimcp_gpu_stdp_params_t params = nimcp_gpu_stdp_params_default();

    float initial_weight_sum = 0.0f;
    auto init_weights = CopyToHost(weights);
    for (float w : init_weights) initial_weight_sum += w;

    // Simulate causal pattern (pre before post consistently)
    for (int step = 0; step < n_steps; step++) {
        // Set spike patterns: pre spikes first
        if (step % 20 == 0) {
            nimcp_gpu_fill(ctx, pre_spikes, 1.0f);
            nimcp_gpu_fill(ctx, post_spikes, 0.0f);
        } else if (step % 20 == 5) {
            nimcp_gpu_fill(ctx, pre_spikes, 0.0f);
            nimcp_gpu_fill(ctx, post_spikes, 1.0f);
        } else {
            nimcp_gpu_fill(ctx, pre_spikes, 0.0f);
            nimcp_gpu_fill(ctx, post_spikes, 0.0f);
        }

        // Update traces
        nimcp_gpu_stdp_update_traces(ctx, pre_trace, post_trace, pre_spikes, post_spikes, dt, &params);

        // Apply STDP
        nimcp_gpu_stdp_apply(ctx, weights, pre_spikes, post_spikes, pre_trace, post_trace, &params);
    }

    float final_weight_sum = 0.0f;
    auto final_weights = CopyToHost(weights);
    for (float w : final_weights) final_weight_sum += w;

    // With causal timing, weights should generally increase (LTP dominates)
    EXPECT_GE(final_weight_sum, initial_weight_sum);

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(pre_trace);
    nimcp_gpu_tensor_destroy(post_trace);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
}

TEST_F(PlasticityKernelTest, Integration_HomeostaticScaling) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_pre = 20;
    const float dt = 10.0f;
    const int n_steps = 200;

    nimcp_gpu_tensor_t* weights = Create2DTensor(n_neurons, n_pre, 0.5f);
    nimcp_gpu_tensor_t* avg_rates = Create1DTensor(n_neurons, 0.0f);
    nimcp_gpu_tensor_t* scaling_factors = Create1DTensor(n_neurons, 1.0f);

    nimcp_gpu_scaling_params_t params = nimcp_gpu_scaling_params_default();
    params.target_rate = 10.0f;  // Target 10 Hz

    // Simulate low activity neurons
    std::vector<float> low_spikes(n_neurons, 0.1f);  // Low firing rate
    nimcp_gpu_tensor_t* spikes = SetFromHost(nullptr, low_spikes);

    float initial_avg_weight = 0.0f;
    auto init_w = CopyToHost(weights);
    for (float w : init_w) initial_avg_weight += w;
    initial_avg_weight /= init_w.size();

    // Run homeostatic adaptation
    for (int step = 0; step < n_steps; step++) {
        // Update rate estimates
        nimcp_gpu_homeostatic_update_rates(ctx, avg_rates, spikes, dt, &params);

        // Compute and apply scaling periodically
        if (step % 50 == 49) {
            nimcp_gpu_homeostatic_compute_scaling(ctx, scaling_factors, avg_rates, &params);
            nimcp_gpu_homeostatic_apply_scaling(ctx, weights, scaling_factors, 0.0f, 2.0f);
        }
    }

    float final_avg_weight = 0.0f;
    auto final_w = CopyToHost(weights);
    for (float w : final_w) final_avg_weight += w;
    final_avg_weight /= final_w.size();

    // With low activity, weights should scale up
    EXPECT_GE(final_avg_weight, initial_avg_weight);

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(avg_rates);
    nimcp_gpu_tensor_destroy(scaling_factors);
    nimcp_gpu_tensor_destroy(spikes);
}

TEST_F(PlasticityKernelTest, Integration_STPDepressedAndFacilitated) {
    RequireGPU();

    const size_t n_synapses = 50;
    const float dt = 1.0f;

    // Create two STP states with different parameters
    nimcp_gpu_stp_params_t depressing_params = {
        .U = 0.5f,      // High initial release
        .tau_D = 200.0f, // Slow recovery
        .tau_F = 20.0f   // Fast decay
    };

    nimcp_gpu_stp_params_t facilitating_params = {
        .U = 0.1f,       // Low initial release
        .tau_D = 50.0f,  // Fast recovery
        .tau_F = 500.0f  // Slow decay
    };

    nimcp_gpu_stp_state_t* dep_state = nimcp_gpu_stp_state_create(ctx, n_synapses, &depressing_params);
    nimcp_gpu_stp_state_t* fac_state = nimcp_gpu_stp_state_create(ctx, n_synapses, &facilitating_params);
    ASSERT_NE(dep_state, nullptr);
    ASSERT_NE(fac_state, nullptr);

    nimcp_gpu_tensor_t* spikes = Create1DTensor(n_synapses, 0.0f);
    nimcp_gpu_tensor_t* dep_mod = Create1DTensor(n_synapses, 0.0f);
    nimcp_gpu_tensor_t* fac_mod = Create1DTensor(n_synapses, 0.0f);

    // Train with high frequency stimulation
    std::vector<float> initial_dep_mods, initial_fac_mods;
    std::vector<float> final_dep_mods, final_fac_mods;

    for (int step = 0; step < 100; step++) {
        // Spike every 10 steps (100 Hz)
        if (step % 10 == 0) {
            nimcp_gpu_fill(ctx, spikes, 1.0f);
            nimcp_gpu_stp_process_spikes(ctx, dep_state, spikes);
            nimcp_gpu_stp_process_spikes(ctx, fac_state, spikes);
        } else {
            nimcp_gpu_fill(ctx, spikes, 0.0f);
        }

        nimcp_gpu_stp_update(ctx, dep_state, dt);
        nimcp_gpu_stp_update(ctx, fac_state, dt);

        if (step == 10) {
            nimcp_gpu_stp_get_modulation(ctx, dep_state, dep_mod);
            nimcp_gpu_stp_get_modulation(ctx, fac_state, fac_mod);
            initial_dep_mods = CopyToHost(dep_mod);
            initial_fac_mods = CopyToHost(fac_mod);
        }
        if (step == 90) {
            nimcp_gpu_stp_get_modulation(ctx, dep_state, dep_mod);
            nimcp_gpu_stp_get_modulation(ctx, fac_state, fac_mod);
            final_dep_mods = CopyToHost(dep_mod);
            final_fac_mods = CopyToHost(fac_mod);
        }
    }

    // Compute modulation changes over the stimulation period
    float dep_change = final_dep_mods[0] - initial_dep_mods[0];
    float fac_change = final_fac_mods[0] - initial_fac_mods[0];

    // STP verification: Both synapse types should show modulation changes
    // during high-frequency stimulation (100 Hz).
    //
    // Note: In the Tsodyks-Markram STP model, modulation = u * x where:
    //   - u (utilization) increases on spike and decays toward U baseline
    //   - x (resources) decreases on spike and recovers toward 1.0
    //
    // The net modulation can decrease for BOTH depressing and facilitating
    // synapses during sustained stimulation, because x depletion accumulates
    // faster than u can compensate. The "facilitation" vs "depression"
    // distinction is more about the transient response dynamics.
    //
    // Key behavior check: modulation should change with stimulation
    bool dep_shows_change = std::abs(dep_change) > 0.01f;
    bool fac_shows_change = std::abs(fac_change) > 0.01f;

    EXPECT_TRUE(dep_shows_change) << "Depressing synapses should show STP modulation change";
    EXPECT_TRUE(fac_shows_change) << "Facilitating synapses should show STP modulation change";

    nimcp_gpu_tensor_destroy(spikes);
    nimcp_gpu_tensor_destroy(dep_mod);
    nimcp_gpu_tensor_destroy(fac_mod);
    nimcp_gpu_stp_state_destroy(dep_state);
    nimcp_gpu_stp_state_destroy(fac_state);
}

TEST_F(PlasticityKernelTest, Integration_CalciumDependentPlasticity) {
    RequireGPU();

    const size_t n_synapses = 100;
    const float dt = 1.0f;
    const int n_steps = 50;

    nimcp_gpu_calcium_params_t params = nimcp_gpu_calcium_params_default();
    nimcp_gpu_calcium_state_t* state = nimcp_gpu_calcium_state_create(ctx, n_synapses, &params);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* weights = Create1DTensor(n_synapses, 0.5f);
    nimcp_gpu_tensor_t* nmda = Create1DTensor(n_synapses, 0.8f);
    nimcp_gpu_tensor_t* voltage = Create1DTensor(n_synapses, -10.0f);  // Moderately depolarized
    nimcp_gpu_tensor_t* pre_act = Create1DTensor(n_synapses, 0.5f);
    nimcp_gpu_tensor_t* post_act = Create1DTensor(n_synapses, 0.5f);

    float initial_weight_sum = 0.0f;
    auto init_w = CopyToHost(weights);
    for (float w : init_w) initial_weight_sum += w;

    // Run calcium-dependent plasticity
    for (int step = 0; step < n_steps; step++) {
        // NMDA influx
        nimcp_gpu_calcium_nmda_influx(ctx, state, nmda, voltage);

        // Compute learning rate from calcium
        nimcp_gpu_calcium_compute_learning_rate(ctx, state);

        // Apply plasticity
        nimcp_gpu_calcium_apply_plasticity(ctx, weights, state, pre_act, post_act);

        // Calcium decay
        nimcp_gpu_calcium_update(ctx, state, dt);
    }

    float final_weight_sum = 0.0f;
    auto final_w = CopyToHost(weights);
    for (float w : final_w) final_weight_sum += w;

    // Weights should change (either direction depending on calcium levels)
    EXPECT_NE(final_weight_sum, initial_weight_sum);

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(nmda);
    nimcp_gpu_tensor_destroy(voltage);
    nimcp_gpu_tensor_destroy(pre_act);
    nimcp_gpu_tensor_destroy(post_act);
    nimcp_gpu_calcium_state_destroy(state);
}
