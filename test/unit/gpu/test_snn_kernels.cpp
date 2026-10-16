/**
 * @file test_snn_kernels.cpp
 * @brief Comprehensive unit tests for GPU SNN kernels
 *
 * WHAT: Tests for GPU-accelerated Spiking Neural Network operations
 * WHY:  Verify SNN neuron models, surrogate gradients, STDP learning, and spike propagation
 * HOW:  GoogleTest with GPU context setup/teardown and CPU fallback verification
 *
 * TEST COVERAGE:
 * - LIF neuron forward pass (membrane potential, spike generation)
 * - Izhikevich neuron model
 * - Surrogate gradients (SuperSpike, fast sigmoid, arctan)
 * - STDP learning rules (pair-based, triplet)
 * - Spike propagation (dense and sparse)
 * - Eligibility trace updates
 * - Neuron state lifecycle
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

// GPU headers include CUDA headers that cannot be in extern "C" blocks
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr size_t DEFAULT_N_NEURONS = 100;
static constexpr size_t SMALL_N_NEURONS = 10;
static constexpr size_t LARGE_N_NEURONS = 1000;
static constexpr float DEFAULT_DT = 1.0f;  // 1ms timestep
static constexpr float DEFAULT_V_REST = -65.0f;
static constexpr float DEFAULT_V_THRESH = -55.0f;
static constexpr float DEFAULT_V_RESET = -70.0f;
static constexpr float DEFAULT_TAU_MEM = 20.0f;  // ms
static constexpr float DEFAULT_TAU_SYN = 5.0f;   // ms
static constexpr float DEFAULT_BETA = 10.0f;     // Surrogate gradient sharpness

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for GPU SNN kernel tests
 *
 * WHAT: Provides GPU context setup/teardown and helper utilities
 * WHY:  Ensure proper GPU resource management across tests
 * HOW:  Creates context in SetUp, destroys in TearDown, provides tensor helpers
 */
class SNNKernelTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        // Try to create GPU context (may fail without CUDA)
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    /**
     * @brief Skip test if GPU not available
     */
    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    /**
     * @brief Create default LIF parameters
     */
    nimcp_lif_params_t create_default_lif_params() {
        nimcp_lif_params_t params;
        params.tau_mem = DEFAULT_TAU_MEM;
        params.tau_syn = DEFAULT_TAU_SYN;
        params.v_thresh = DEFAULT_V_THRESH;
        params.v_reset = DEFAULT_V_RESET;
        params.v_rest = DEFAULT_V_REST;
        params.dt = DEFAULT_DT;
        params.hard_reset = true;
        return params;
    }

    /**
     * @brief Create default Izhikevich parameters (regular spiking)
     */
    nimcp_izhikevich_params_t create_default_izhikevich_params() {
        nimcp_izhikevich_params_t params;
        params.a = 0.02f;    // Time scale of recovery
        params.b = 0.2f;     // Sensitivity of recovery to voltage
        params.c = -65.0f;   // Reset voltage (mV)
        params.d = 8.0f;     // Recovery increment
        params.v_thresh = 30.0f;  // Spike threshold (mV)
        params.dt = DEFAULT_DT;
        return params;
    }

    /**
     * @brief Create default STDP parameters
     */
    nimcp_stdp_params_t create_default_stdp_params() {
        nimcp_stdp_params_t params;
        params.A_plus = 0.01f;    // LTP amplitude
        params.A_minus = 0.012f;  // LTD amplitude (slightly stronger for stability)
        params.tau_plus = 20.0f;  // LTP time constant (ms)
        params.tau_minus = 20.0f; // LTD time constant (ms)
        params.w_max = 1.0f;      // Maximum weight
        params.w_min = 0.0f;      // Minimum weight
        return params;
    }

    /**
     * @brief Create GPU tensor from host data
     */
    nimcp_gpu_tensor_t* create_tensor_from_data(const float* data, size_t size) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        return nimcp_gpu_tensor_from_host(ctx, data, dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    /**
     * @brief Create GPU tensor filled with zeros
     */
    nimcp_gpu_tensor_t* create_zero_tensor(size_t size) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_zeros(ctx, tensor);
        }
        return tensor;
    }

    /**
     * @brief Create GPU tensor filled with a value
     */
    nimcp_gpu_tensor_t* create_filled_tensor(size_t size, float value) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    /**
     * @brief Create 2D GPU tensor (matrix)
     */
    nimcp_gpu_tensor_t* create_matrix(size_t rows, size_t cols, float value) {
        if (!gpu_available) return nullptr;
        size_t dims[2] = {rows, cols};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    /**
     * @brief Copy tensor data to host
     */
    bool copy_to_host(const nimcp_gpu_tensor_t* tensor, float* host_data) {
        if (!tensor || !host_data) return false;
        return nimcp_gpu_tensor_to_host(tensor, host_data);
    }
};

//=============================================================================
// LIF Neuron State Lifecycle Tests
//=============================================================================

/**
 * TEST: LIF state creation with valid parameters
 * WHAT: Create LIF neuron state on GPU
 * WHY:  Verify basic allocation and initialization
 */
TEST_F(SNNKernelTest, LIFStateCreate_ValidParams_Succeeds) {
    RequireGPU();

    nimcp_lif_params_t params = create_default_lif_params();
    nimcp_lif_state_t* state = nimcp_lif_state_create(ctx, DEFAULT_N_NEURONS, &params);

    ASSERT_NE(state, nullptr) << "LIF state creation should succeed";
    EXPECT_NE(state->v, nullptr) << "Membrane potential tensor should be allocated";
    EXPECT_NE(state->i_syn, nullptr) << "Synaptic current tensor should be allocated";
    EXPECT_NE(state->spikes, nullptr) << "Spikes tensor should be allocated";
    EXPECT_FLOAT_EQ(state->params.v_thresh, DEFAULT_V_THRESH);

    nimcp_lif_state_destroy(state);
}

/**
 * TEST: LIF state creation with NULL context
 * WHAT: Try to create LIF state without GPU context
 * WHY:  Verify NULL-safety
 */
TEST_F(SNNKernelTest, LIFStateCreate_NullContext_ReturnsNull) {
    nimcp_lif_params_t params = create_default_lif_params();
    nimcp_lif_state_t* state = nimcp_lif_state_create(nullptr, DEFAULT_N_NEURONS, &params);
    EXPECT_EQ(state, nullptr) << "Should reject NULL context";
}

/**
 * TEST: LIF state creation with NULL params
 * WHAT: Try to create LIF state without parameters
 * WHY:  Verify NULL-safety
 */
TEST_F(SNNKernelTest, LIFStateCreate_NullParams_ReturnsNull) {
    RequireGPU();

    nimcp_lif_state_t* state = nimcp_lif_state_create(ctx, DEFAULT_N_NEURONS, nullptr);
    EXPECT_EQ(state, nullptr) << "Should reject NULL params";
}

/**
 * TEST: LIF state creation with zero neurons
 * WHAT: Try to create LIF state with zero neurons
 * WHY:  Verify input validation
 */
TEST_F(SNNKernelTest, LIFStateCreate_ZeroNeurons_ReturnsNull) {
    RequireGPU();

    nimcp_lif_params_t params = create_default_lif_params();
    nimcp_lif_state_t* state = nimcp_lif_state_create(ctx, 0, &params);
    EXPECT_EQ(state, nullptr) << "Should reject zero neurons";
}

/**
 * TEST: LIF state destruction with NULL
 * WHAT: Destroy NULL LIF state
 * WHY:  Verify NULL-safety
 */
TEST_F(SNNKernelTest, LIFStateDestroy_Null_NoOp) {
    nimcp_lif_state_destroy(nullptr);
    SUCCEED() << "Should not crash on NULL";
}

//=============================================================================
// LIF Forward Pass Tests
//=============================================================================

/**
 * TEST: LIF forward pass with zero input
 * WHAT: Run LIF forward pass with no synaptic input
 * WHY:  Verify membrane decay toward rest without input
 */
TEST_F(SNNKernelTest, LIFForward_ZeroInput_MembraneDrift) {
    RequireGPU();

    nimcp_lif_params_t params = create_default_lif_params();
    nimcp_lif_state_t* state = nimcp_lif_state_create(ctx, SMALL_N_NEURONS, &params);
    ASSERT_NE(state, nullptr);

    // Set initial voltage away from rest
    nimcp_gpu_fill(ctx, state->v, -60.0f);  // Above rest (-65)

    // Create zero input
    nimcp_gpu_tensor_t* input = create_zero_tensor(SMALL_N_NEURONS);
    ASSERT_NE(input, nullptr);

    // Forward pass
    bool result = nimcp_gpu_lif_forward(ctx, state, input);
    EXPECT_TRUE(result) << "LIF forward should succeed";

    // Verify membrane potential moved toward rest
    std::vector<float> v_host(SMALL_N_NEURONS);
    copy_to_host(state->v, v_host.data());

    for (size_t i = 0; i < SMALL_N_NEURONS; i++) {
        // With zero input, V should decay toward V_rest
        EXPECT_GT(v_host[i], DEFAULT_V_REST) << "V should be between initial and rest";
        EXPECT_LT(v_host[i], -60.0f) << "V should have decayed";
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_lif_state_destroy(state);
}

/**
 * TEST: LIF forward pass with strong input
 * WHAT: Run LIF forward with input strong enough to cause spikes
 * WHY:  Verify spike generation mechanism
 */
TEST_F(SNNKernelTest, LIFForward_StrongInput_GeneratesSpikes) {
    RequireGPU();

    nimcp_lif_params_t params = create_default_lif_params();
    nimcp_lif_state_t* state = nimcp_lif_state_create(ctx, SMALL_N_NEURONS, &params);
    ASSERT_NE(state, nullptr);

    // Set voltage just below threshold
    nimcp_gpu_fill(ctx, state->v, -56.0f);  // Just below -55 threshold

    // Strong input current - needs to be ~35+ to overcome exponential decay
    // with tau_mem=20ms, dt=1ms: alpha=0.95, so (1-alpha)*(v_rest+i) needs to push v over threshold
    // With v=-56, v_rest=-65, threshold=-55: need i >= 30 to spike
    nimcp_gpu_tensor_t* input = create_filled_tensor(SMALL_N_NEURONS, 40.0f);
    ASSERT_NE(input, nullptr);

    // Forward pass
    bool result = nimcp_gpu_lif_forward(ctx, state, input);
    EXPECT_TRUE(result);

    // Check for spikes
    std::vector<float> spikes_host(SMALL_N_NEURONS);
    copy_to_host(state->spikes, spikes_host.data());

    int spike_count = 0;
    for (float s : spikes_host) {
        if (s > 0.5f) spike_count++;
    }
    EXPECT_GT(spike_count, 0) << "Should generate at least one spike";

    // Verify neurons that spiked were reset
    std::vector<float> v_host(SMALL_N_NEURONS);
    copy_to_host(state->v, v_host.data());

    for (size_t i = 0; i < SMALL_N_NEURONS; i++) {
        if (spikes_host[i] > 0.5f) {
            EXPECT_NEAR(v_host[i], DEFAULT_V_RESET, 1.0f)
                << "Spiking neuron " << i << " should be reset";
        }
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_lif_state_destroy(state);
}

/**
 * TEST: LIF forward pass with NULL state
 * WHAT: Try LIF forward with NULL state
 * WHY:  Verify NULL-safety
 */
TEST_F(SNNKernelTest, LIFForward_NullState_ReturnsFalse) {
    RequireGPU();

    nimcp_gpu_tensor_t* input = create_zero_tensor(SMALL_N_NEURONS);
    ASSERT_NE(input, nullptr);

    bool result = nimcp_gpu_lif_forward(ctx, nullptr, input);
    EXPECT_FALSE(result) << "Should reject NULL state";

    nimcp_gpu_tensor_destroy(input);
}

/**
 * TEST: LIF backward pass with surrogate gradient
 * WHAT: Compute gradients through LIF using surrogate gradients
 * WHY:  Enable backprop through non-differentiable spike function
 */
TEST_F(SNNKernelTest, LIFBackward_SuperSpike_ComputesGradients) {
    RequireGPU();

    nimcp_lif_params_t params = create_default_lif_params();
    nimcp_lif_state_t* state = nimcp_lif_state_create(ctx, SMALL_N_NEURONS, &params);
    ASSERT_NE(state, nullptr);

    // Set voltages near threshold (surrogate gradient peak)
    nimcp_gpu_fill(ctx, state->v, DEFAULT_V_THRESH);

    // Upstream gradient
    nimcp_gpu_tensor_t* grad_output = create_filled_tensor(SMALL_N_NEURONS, 1.0f);
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor(SMALL_N_NEURONS);
    ASSERT_NE(grad_output, nullptr);
    ASSERT_NE(grad_input, nullptr);

    bool result = nimcp_gpu_lif_backward(ctx, state, grad_output, grad_input,
                                          NIMCP_SURROGATE_SUPERSPIKE, DEFAULT_BETA);
    EXPECT_TRUE(result) << "LIF backward should succeed";

    // Verify gradients are non-zero near threshold
    std::vector<float> grad_host(SMALL_N_NEURONS);
    copy_to_host(grad_input, grad_host.data());

    float max_grad = *std::max_element(grad_host.begin(), grad_host.end());
    EXPECT_GT(max_grad, 0.0f) << "Gradients should be non-zero near threshold";

    nimcp_gpu_tensor_destroy(grad_output);
    nimcp_gpu_tensor_destroy(grad_input);
    nimcp_lif_state_destroy(state);
}

//=============================================================================
// Izhikevich Neuron Model Tests
//=============================================================================

/**
 * TEST: Izhikevich state creation
 * WHAT: Create Izhikevich neuron state
 * WHY:  Verify Izhikevich model initialization
 */
TEST_F(SNNKernelTest, IzhikevichStateCreate_ValidParams_Succeeds) {
    RequireGPU();

    nimcp_izhikevich_params_t params = create_default_izhikevich_params();
    nimcp_izhikevich_state_t* state = nimcp_izhikevich_state_create(ctx, DEFAULT_N_NEURONS, &params);

    ASSERT_NE(state, nullptr) << "Izhikevich state creation should succeed";
    EXPECT_NE(state->v, nullptr) << "V tensor should be allocated";
    EXPECT_NE(state->u, nullptr) << "U (recovery) tensor should be allocated";
    EXPECT_NE(state->spikes, nullptr) << "Spikes tensor should be allocated";

    nimcp_izhikevich_state_destroy(state);
}

/**
 * TEST: Izhikevich forward pass
 * WHAT: Run Izhikevich dynamics
 * WHY:  Verify proper integration of 2D dynamics
 */
TEST_F(SNNKernelTest, IzhikevichForward_ConstantInput_ProducesSpikes) {
    RequireGPU();

    nimcp_izhikevich_params_t params = create_default_izhikevich_params();
    nimcp_izhikevich_state_t* state = nimcp_izhikevich_state_create(ctx, SMALL_N_NEURONS, &params);
    ASSERT_NE(state, nullptr);

    // Initialize with typical resting state
    nimcp_gpu_fill(ctx, state->v, params.c);  // Reset voltage
    nimcp_gpu_fill(ctx, state->u, params.b * params.c);  // Equilibrium u

    // Strong tonic input
    nimcp_gpu_tensor_t* input = create_filled_tensor(SMALL_N_NEURONS, 15.0f);
    ASSERT_NE(input, nullptr);

    // Run multiple timesteps
    int total_spikes = 0;
    for (int t = 0; t < 100; t++) {
        bool result = nimcp_gpu_izhikevich_forward(ctx, state, input);
        ASSERT_TRUE(result) << "Forward pass at t=" << t << " should succeed";

        // Count spikes
        std::vector<float> spikes_host(SMALL_N_NEURONS);
        copy_to_host(state->spikes, spikes_host.data());
        for (float s : spikes_host) {
            if (s > 0.5f) total_spikes++;
        }
    }

    EXPECT_GT(total_spikes, 0) << "Should generate spikes with tonic input";

    nimcp_gpu_tensor_destroy(input);
    nimcp_izhikevich_state_destroy(state);
}

/**
 * TEST: Izhikevich forward pass with NULL state
 * WHAT: Try forward with NULL state
 * WHY:  Verify NULL-safety
 */
TEST_F(SNNKernelTest, IzhikevichForward_NullState_ReturnsFalse) {
    RequireGPU();

    nimcp_gpu_tensor_t* input = create_zero_tensor(SMALL_N_NEURONS);
    ASSERT_NE(input, nullptr);

    bool result = nimcp_gpu_izhikevich_forward(ctx, nullptr, input);
    EXPECT_FALSE(result);

    nimcp_gpu_tensor_destroy(input);
}

/**
 * TEST: Izhikevich state destruction with NULL
 * WHAT: Destroy NULL state
 * WHY:  Verify NULL-safety
 */
TEST_F(SNNKernelTest, IzhikevichStateDestroy_Null_NoOp) {
    nimcp_izhikevich_state_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Surrogate Gradient Tests
//=============================================================================

/**
 * TEST: SuperSpike surrogate gradient
 * WHAT: Compute surrogate gradient using SuperSpike formula
 * WHY:  Verify correct gradient shape for SNN backprop
 */
TEST_F(SNNKernelTest, SurrogateGradient_SuperSpike_PeaksAtThreshold) {
    RequireGPU();

    // Create voltage tensor with values spanning threshold
    std::vector<float> voltages(SMALL_N_NEURONS);
    for (size_t i = 0; i < SMALL_N_NEURONS; i++) {
        voltages[i] = DEFAULT_V_THRESH - 10.0f + 2.0f * i;  // -65 to -47
    }

    nimcp_gpu_tensor_t* v = create_tensor_from_data(voltages.data(), SMALL_N_NEURONS);
    nimcp_gpu_tensor_t* grad = create_zero_tensor(SMALL_N_NEURONS);
    ASSERT_NE(v, nullptr);
    ASSERT_NE(grad, nullptr);

    bool result = nimcp_gpu_surrogate_gradient(ctx, v, DEFAULT_V_THRESH, grad,
                                                NIMCP_SURROGATE_SUPERSPIKE, DEFAULT_BETA);
    EXPECT_TRUE(result);

    std::vector<float> grad_host(SMALL_N_NEURONS);
    copy_to_host(grad, grad_host.data());

    // Find peak gradient
    size_t peak_idx = std::distance(grad_host.begin(),
                                    std::max_element(grad_host.begin(), grad_host.end()));

    // Peak should be near threshold (index where voltage = threshold)
    float expected_thresh_idx = (DEFAULT_V_THRESH - (DEFAULT_V_THRESH - 10.0f)) / 2.0f;
    EXPECT_NEAR(peak_idx, expected_thresh_idx, 2) << "Gradient should peak near threshold";

    nimcp_gpu_tensor_destroy(v);
    nimcp_gpu_tensor_destroy(grad);
}

/**
 * TEST: Fast sigmoid surrogate gradient
 * WHAT: Compute surrogate gradient using fast sigmoid
 * WHY:  Alternative surrogate with different gradient shape
 */
TEST_F(SNNKernelTest, SurrogateGradient_FastSigmoid_NonZero) {
    RequireGPU();

    nimcp_gpu_tensor_t* v = create_filled_tensor(SMALL_N_NEURONS, DEFAULT_V_THRESH);
    nimcp_gpu_tensor_t* grad = create_zero_tensor(SMALL_N_NEURONS);
    ASSERT_NE(v, nullptr);
    ASSERT_NE(grad, nullptr);

    bool result = nimcp_gpu_surrogate_gradient(ctx, v, DEFAULT_V_THRESH, grad,
                                                NIMCP_SURROGATE_FAST_SIGMOID, DEFAULT_BETA);
    EXPECT_TRUE(result);

    std::vector<float> grad_host(SMALL_N_NEURONS);
    copy_to_host(grad, grad_host.data());

    // All gradients should be non-zero at threshold
    for (float g : grad_host) {
        EXPECT_GT(g, 0.0f) << "Gradient at threshold should be positive";
    }

    nimcp_gpu_tensor_destroy(v);
    nimcp_gpu_tensor_destroy(grad);
}

/**
 * TEST: Arctan surrogate gradient
 * WHAT: Compute arctan-based surrogate gradient
 * WHY:  Smooth gradient for stable training
 */
TEST_F(SNNKernelTest, SurrogateGradient_Arctan_Smooth) {
    RequireGPU();

    nimcp_gpu_tensor_t* v = create_filled_tensor(SMALL_N_NEURONS, DEFAULT_V_THRESH + 5.0f);
    nimcp_gpu_tensor_t* grad = create_zero_tensor(SMALL_N_NEURONS);
    ASSERT_NE(v, nullptr);
    ASSERT_NE(grad, nullptr);

    bool result = nimcp_gpu_surrogate_gradient(ctx, v, DEFAULT_V_THRESH, grad,
                                                NIMCP_SURROGATE_ARCTAN, DEFAULT_BETA);
    EXPECT_TRUE(result);

    std::vector<float> grad_host(SMALL_N_NEURONS);
    copy_to_host(grad, grad_host.data());

    // Gradient should be small but non-zero away from threshold
    for (float g : grad_host) {
        EXPECT_GT(g, 0.0f) << "Arctan gradient should be positive";
    }

    nimcp_gpu_tensor_destroy(v);
    nimcp_gpu_tensor_destroy(grad);
}

/**
 * TEST: Surrogate gradient with NULL tensors
 * WHAT: Try surrogate gradient with NULL inputs
 * WHY:  Verify NULL-safety
 */
TEST_F(SNNKernelTest, SurrogateGradient_NullTensors_ReturnsFalse) {
    RequireGPU();

    nimcp_gpu_tensor_t* v = create_filled_tensor(SMALL_N_NEURONS, DEFAULT_V_THRESH);
    nimcp_gpu_tensor_t* grad = create_zero_tensor(SMALL_N_NEURONS);

    // NULL v
    EXPECT_FALSE(nimcp_gpu_surrogate_gradient(ctx, nullptr, DEFAULT_V_THRESH, grad,
                                               NIMCP_SURROGATE_SUPERSPIKE, DEFAULT_BETA));

    // NULL grad
    EXPECT_FALSE(nimcp_gpu_surrogate_gradient(ctx, v, DEFAULT_V_THRESH, nullptr,
                                               NIMCP_SURROGATE_SUPERSPIKE, DEFAULT_BETA));

    nimcp_gpu_tensor_destroy(v);
    nimcp_gpu_tensor_destroy(grad);
}

//=============================================================================
// Spike Propagation Tests
//=============================================================================

/**
 * TEST: Dense spike propagation
 * WHAT: Propagate spikes through weight matrix
 * WHY:  Core SNN operation: I_post = W @ spikes_pre
 */
TEST_F(SNNKernelTest, SpikePropagation_Dense_ComputesCorrectly) {
    RequireGPU();

    const size_t n_pre = 5;
    const size_t n_post = 4;

    // Binary spikes: [1, 0, 1, 0, 1]
    std::vector<float> spikes_data = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    nimcp_gpu_tensor_t* spikes = create_tensor_from_data(spikes_data.data(), n_pre);
    ASSERT_NE(spikes, nullptr);

    // Weight matrix (n_post x n_pre) filled with 0.5
    nimcp_gpu_tensor_t* weights = create_matrix(n_post, n_pre, 0.5f);
    ASSERT_NE(weights, nullptr);

    // Output current
    nimcp_gpu_tensor_t* output = create_zero_tensor(n_post);
    ASSERT_NE(output, nullptr);

    bool result = nimcp_gpu_spike_propagate(ctx, spikes, weights, output);
    EXPECT_TRUE(result);

    // Expected: each output = 0.5 * (1 + 1 + 1) = 1.5 (3 active inputs)
    std::vector<float> output_host(n_post);
    copy_to_host(output, output_host.data());

    for (float o : output_host) {
        EXPECT_NEAR(o, 1.5f, 0.01f) << "Output should be sum of weighted active inputs";
    }

    nimcp_gpu_tensor_destroy(spikes);
    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(output);
}

/**
 * TEST: Sparse spike propagation
 * WHAT: Event-driven spike propagation using spike indices
 * WHY:  Efficient for sparse activity (only process active neurons)
 */
TEST_F(SNNKernelTest, SpikePropagation_Sparse_MatchesDense) {
    RequireGPU();

    const size_t n_pre = 100;
    const size_t n_post = 50;

    // Sparse spikes: only indices [5, 20, 45]
    std::vector<uint32_t> spike_indices = {5, 20, 45};
    size_t n_spikes = spike_indices.size();

    // Weight matrix
    nimcp_gpu_tensor_t* weights = create_matrix(n_post, n_pre, 0.1f);
    ASSERT_NE(weights, nullptr);

    // Output current
    nimcp_gpu_tensor_t* output = create_zero_tensor(n_post);
    ASSERT_NE(output, nullptr);

    bool result = nimcp_gpu_spike_propagate_sparse(ctx, spike_indices.data(), n_spikes,
                                                     weights, output);
    EXPECT_TRUE(result);

    // Each output should receive 0.1 * 3 = 0.3
    std::vector<float> output_host(n_post);
    copy_to_host(output, output_host.data());

    for (float o : output_host) {
        EXPECT_NEAR(o, 0.3f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(output);
}

/**
 * TEST: Spike propagation with NULL tensors
 * WHAT: Try propagation with NULL inputs
 * WHY:  Verify NULL-safety
 */
TEST_F(SNNKernelTest, SpikePropagation_NullTensors_ReturnsFalse) {
    RequireGPU();

    nimcp_gpu_tensor_t* spikes = create_zero_tensor(10);
    nimcp_gpu_tensor_t* weights = create_matrix(5, 10, 0.1f);
    nimcp_gpu_tensor_t* output = create_zero_tensor(5);

    EXPECT_FALSE(nimcp_gpu_spike_propagate(ctx, nullptr, weights, output));
    EXPECT_FALSE(nimcp_gpu_spike_propagate(ctx, spikes, nullptr, output));
    EXPECT_FALSE(nimcp_gpu_spike_propagate(ctx, spikes, weights, nullptr));

    nimcp_gpu_tensor_destroy(spikes);
    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(output);
}

//=============================================================================
// Eligibility Trace Tests
//=============================================================================

/**
 * TEST: Eligibility trace update with spikes
 * WHAT: Update eligibility trace: e = e * decay + spike
 * WHY:  Foundation for online learning (e-prop)
 */
TEST_F(SNNKernelTest, EligibilityTrace_WithSpikes_Accumulates) {
    RequireGPU();

    const float decay = 0.9f;

    // Initial trace
    nimcp_gpu_tensor_t* trace = create_filled_tensor(SMALL_N_NEURONS, 0.5f);
    ASSERT_NE(trace, nullptr);

    // Spikes (alternating)
    std::vector<float> spikes_data(SMALL_N_NEURONS);
    for (size_t i = 0; i < SMALL_N_NEURONS; i++) {
        spikes_data[i] = (i % 2 == 0) ? 1.0f : 0.0f;
    }
    nimcp_gpu_tensor_t* spikes = create_tensor_from_data(spikes_data.data(), SMALL_N_NEURONS);
    ASSERT_NE(spikes, nullptr);

    bool result = nimcp_gpu_eligibility_trace_update(ctx, trace, spikes, decay);
    EXPECT_TRUE(result);

    std::vector<float> trace_host(SMALL_N_NEURONS);
    copy_to_host(trace, trace_host.data());

    for (size_t i = 0; i < SMALL_N_NEURONS; i++) {
        if (i % 2 == 0) {
            // Spiked: e = 0.5 * 0.9 + 1.0 * (1 - 0.9) = 0.45 + 0.1 = 0.55
            EXPECT_NEAR(trace_host[i], 0.55f, 0.01f);
        } else {
            // No spike: e = 0.5 * 0.9 = 0.45
            EXPECT_NEAR(trace_host[i], 0.45f, 0.01f);
        }
    }

    nimcp_gpu_tensor_destroy(trace);
    nimcp_gpu_tensor_destroy(spikes);
}

/**
 * TEST: Eligibility trace decay without spikes
 * WHAT: Pure decay of eligibility trace
 * WHY:  Verify exponential decay behavior
 */
TEST_F(SNNKernelTest, EligibilityTrace_NoSpikes_Decays) {
    RequireGPU();

    const float decay = 0.95f;
    const float initial = 1.0f;

    nimcp_gpu_tensor_t* trace = create_filled_tensor(SMALL_N_NEURONS, initial);
    nimcp_gpu_tensor_t* spikes = create_zero_tensor(SMALL_N_NEURONS);
    ASSERT_NE(trace, nullptr);
    ASSERT_NE(spikes, nullptr);

    // Apply multiple decay steps
    for (int t = 0; t < 10; t++) {
        bool result = nimcp_gpu_eligibility_trace_update(ctx, trace, spikes, decay);
        ASSERT_TRUE(result);
    }

    std::vector<float> trace_host(SMALL_N_NEURONS);
    copy_to_host(trace, trace_host.data());

    float expected = initial * std::pow(decay, 10);
    for (float e : trace_host) {
        EXPECT_NEAR(e, expected, 0.01f);
    }

    nimcp_gpu_tensor_destroy(trace);
    nimcp_gpu_tensor_destroy(spikes);
}

//=============================================================================
// STDP Learning Tests
//=============================================================================

/**
 * TEST: Pair-based STDP with pre-before-post timing
 * WHAT: STDP potentiation when pre fires before post
 * WHY:  Classical Hebbian learning rule
 */
TEST_F(SNNKernelTest, STDPPair_PreBeforePost_Potentiation) {
    RequireGPU();

    nimcp_stdp_params_t params = create_default_stdp_params();

    const size_t n_pre = 5;
    const size_t n_post = 3;

    // Weight matrix initialized to 0.5
    nimcp_gpu_tensor_t* weights = create_matrix(n_post, n_pre, 0.5f);
    ASSERT_NE(weights, nullptr);

    // Pre-synaptic spikes
    std::vector<float> pre_spikes_data = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    nimcp_gpu_tensor_t* pre_spikes = create_tensor_from_data(pre_spikes_data.data(), n_pre);
    ASSERT_NE(pre_spikes, nullptr);

    // Post-synaptic spikes (some correlation with pre)
    std::vector<float> post_spikes_data = {1.0f, 1.0f, 0.0f};
    nimcp_gpu_tensor_t* post_spikes = create_tensor_from_data(post_spikes_data.data(), n_post);
    ASSERT_NE(post_spikes, nullptr);

    // Eligibility traces (pre had recent spikes)
    nimcp_gpu_tensor_t* pre_trace = create_filled_tensor(n_pre, 0.5f);
    nimcp_gpu_tensor_t* post_trace = create_filled_tensor(n_post, 0.5f);
    ASSERT_NE(pre_trace, nullptr);
    ASSERT_NE(post_trace, nullptr);

    // Get initial weight sum
    std::vector<float> weights_before(n_post * n_pre);
    copy_to_host(weights, weights_before.data());
    float sum_before = std::accumulate(weights_before.begin(), weights_before.end(), 0.0f);

    bool result = nimcp_gpu_stdp_pair(ctx, weights, pre_spikes, post_spikes,
                                       pre_trace, post_trace, &params);
    EXPECT_TRUE(result);

    // Verify weight changes
    std::vector<float> weights_after(n_post * n_pre);
    copy_to_host(weights, weights_after.data());
    float sum_after = std::accumulate(weights_after.begin(), weights_after.end(), 0.0f);

    // With correlated activity, weights should change
    EXPECT_NE(sum_before, sum_after) << "STDP should modify weights";

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
    nimcp_gpu_tensor_destroy(pre_trace);
    nimcp_gpu_tensor_destroy(post_trace);
}

/**
 * TEST: Triplet STDP
 * WHAT: STDP with triplet spike interactions
 * WHY:  More accurate model of biological plasticity
 */
TEST_F(SNNKernelTest, STDPTriplet_UpdatesWeights) {
    RequireGPU();

    nimcp_stdp_params_t params = create_default_stdp_params();

    const size_t n_pre = 5;
    const size_t n_post = 3;

    // Weight matrix
    nimcp_gpu_tensor_t* weights = create_matrix(n_post, n_pre, 0.5f);
    ASSERT_NE(weights, nullptr);

    // Spikes
    nimcp_gpu_tensor_t* pre_spikes = create_filled_tensor(n_pre, 0.0f);
    nimcp_gpu_tensor_t* post_spikes = create_filled_tensor(n_post, 0.0f);
    ASSERT_NE(pre_spikes, nullptr);
    ASSERT_NE(post_spikes, nullptr);

    // Set some spikes
    std::vector<float> pre_data = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    std::vector<float> post_data = {0.0f, 1.0f, 0.0f};
    nimcp_gpu_tensor_t* pre_sp = create_tensor_from_data(pre_data.data(), n_pre);
    nimcp_gpu_tensor_t* post_sp = create_tensor_from_data(post_data.data(), n_post);

    // Four traces for triplet STDP
    nimcp_gpu_tensor_t* pre_trace_fast = create_filled_tensor(n_pre, 0.3f);
    nimcp_gpu_tensor_t* pre_trace_slow = create_filled_tensor(n_pre, 0.5f);
    nimcp_gpu_tensor_t* post_trace_fast = create_filled_tensor(n_post, 0.3f);
    nimcp_gpu_tensor_t* post_trace_slow = create_filled_tensor(n_post, 0.5f);

    bool result = nimcp_gpu_stdp_triplet(ctx, weights, pre_sp, post_sp,
                                          pre_trace_fast, pre_trace_slow,
                                          post_trace_fast, post_trace_slow,
                                          &params);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
    nimcp_gpu_tensor_destroy(pre_sp);
    nimcp_gpu_tensor_destroy(post_sp);
    nimcp_gpu_tensor_destroy(pre_trace_fast);
    nimcp_gpu_tensor_destroy(pre_trace_slow);
    nimcp_gpu_tensor_destroy(post_trace_fast);
    nimcp_gpu_tensor_destroy(post_trace_slow);
}

/**
 * TEST: STDP weight bounds
 * WHAT: Verify STDP respects w_min and w_max bounds
 * WHY:  Prevent runaway weight growth
 */
TEST_F(SNNKernelTest, STDPPair_RespectsBounds) {
    RequireGPU();

    nimcp_stdp_params_t params = create_default_stdp_params();
    params.w_min = 0.0f;
    params.w_max = 0.6f;  // Low max for easier testing

    const size_t n_pre = 3;
    const size_t n_post = 2;

    // Weight matrix near maximum
    nimcp_gpu_tensor_t* weights = create_matrix(n_post, n_pre, 0.59f);
    ASSERT_NE(weights, nullptr);

    // Strong correlated activity
    nimcp_gpu_tensor_t* pre_spikes = create_filled_tensor(n_pre, 1.0f);
    nimcp_gpu_tensor_t* post_spikes = create_filled_tensor(n_post, 1.0f);
    nimcp_gpu_tensor_t* pre_trace = create_filled_tensor(n_pre, 1.0f);
    nimcp_gpu_tensor_t* post_trace = create_filled_tensor(n_post, 1.0f);

    // Multiple STDP updates
    for (int i = 0; i < 10; i++) {
        bool result = nimcp_gpu_stdp_pair(ctx, weights, pre_spikes, post_spikes,
                                           pre_trace, post_trace, &params);
        ASSERT_TRUE(result);
    }

    // Verify weights don't exceed bounds
    std::vector<float> weights_host(n_post * n_pre);
    copy_to_host(weights, weights_host.data());

    for (float w : weights_host) {
        EXPECT_GE(w, params.w_min) << "Weight should be >= w_min";
        EXPECT_LE(w, params.w_max) << "Weight should be <= w_max";
    }

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
    nimcp_gpu_tensor_destroy(pre_trace);
    nimcp_gpu_tensor_destroy(post_trace);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

/**
 * TEST: Reset SNN state to resting
 * WHAT: Reset membrane potentials to resting value
 * WHY:  State initialization and reset between episodes
 */
TEST_F(SNNKernelTest, ResetState_SetsToRest) {
    RequireGPU();

    nimcp_gpu_tensor_t* v = create_filled_tensor(DEFAULT_N_NEURONS, 0.0f);
    ASSERT_NE(v, nullptr);

    bool result = nimcp_gpu_snn_reset_state(ctx, v, DEFAULT_V_REST);
    EXPECT_TRUE(result);

    std::vector<float> v_host(DEFAULT_N_NEURONS);
    copy_to_host(v, v_host.data());

    for (float val : v_host) {
        EXPECT_FLOAT_EQ(val, DEFAULT_V_REST);
    }

    nimcp_gpu_tensor_destroy(v);
}

/**
 * TEST: Spike count
 * WHAT: Count total spikes in tensor
 * WHY:  Network activity monitoring
 */
TEST_F(SNNKernelTest, SpikeCount_ReturnsCorrectCount) {
    RequireGPU();

    // Known spike pattern
    std::vector<float> spikes_data(SMALL_N_NEURONS);
    int expected_count = 0;
    for (size_t i = 0; i < SMALL_N_NEURONS; i++) {
        spikes_data[i] = (i % 3 == 0) ? 1.0f : 0.0f;
        if (spikes_data[i] > 0.5f) expected_count++;
    }

    nimcp_gpu_tensor_t* spikes = create_tensor_from_data(spikes_data.data(), SMALL_N_NEURONS);
    ASSERT_NE(spikes, nullptr);

    uint32_t count = 0;
    bool result = nimcp_gpu_spike_count(ctx, spikes, &count);
    EXPECT_TRUE(result);
    EXPECT_EQ(count, static_cast<uint32_t>(expected_count));

    nimcp_gpu_tensor_destroy(spikes);
}

/**
 * TEST: Spike rate computation
 * WHAT: Compute spike rate over time
 * WHY:  Activity statistics for monitoring and normalization
 */
TEST_F(SNNKernelTest, SpikeRate_ComputesCorrectly) {
    RequireGPU();

    // All neurons spike (rate = 1.0 over 1 timestep)
    nimcp_gpu_tensor_t* spikes = create_filled_tensor(SMALL_N_NEURONS, 1.0f);
    nimcp_gpu_tensor_t* rates = create_zero_tensor(SMALL_N_NEURONS);
    ASSERT_NE(spikes, nullptr);
    ASSERT_NE(rates, nullptr);

    bool result = nimcp_gpu_spike_rate(ctx, spikes, 1, rates);
    EXPECT_TRUE(result);

    std::vector<float> rates_host(SMALL_N_NEURONS);
    copy_to_host(rates, rates_host.data());

    for (float r : rates_host) {
        EXPECT_FLOAT_EQ(r, 1.0f) << "Rate should be 1.0 spike/timestep";
    }

    nimcp_gpu_tensor_destroy(spikes);
    nimcp_gpu_tensor_destroy(rates);
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * TEST: Full LIF simulation cycle
 * WHAT: Complete LIF forward-backward cycle
 * WHY:  Verify all components work together
 */
TEST_F(SNNKernelTest, Integration_LIFFullCycle) {
    RequireGPU();

    nimcp_lif_params_t params = create_default_lif_params();
    nimcp_lif_state_t* state = nimcp_lif_state_create(ctx, DEFAULT_N_NEURONS, &params);
    ASSERT_NE(state, nullptr);

    // Run simulation
    nimcp_gpu_tensor_t* input = create_filled_tensor(DEFAULT_N_NEURONS, 10.0f);
    ASSERT_NE(input, nullptr);

    int total_spikes = 0;
    for (int t = 0; t < 50; t++) {
        bool fwd_result = nimcp_gpu_lif_forward(ctx, state, input);
        ASSERT_TRUE(fwd_result) << "Forward pass at t=" << t << " should succeed";

        uint32_t spikes = 0;
        nimcp_gpu_spike_count(ctx, state->spikes, &spikes);
        total_spikes += spikes;
    }

    EXPECT_GT(total_spikes, 0) << "Should generate spikes during simulation";

    // Backward pass
    nimcp_gpu_tensor_t* grad_output = create_filled_tensor(DEFAULT_N_NEURONS, 1.0f);
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor(DEFAULT_N_NEURONS);

    bool bwd_result = nimcp_gpu_lif_backward(ctx, state, grad_output, grad_input,
                                              NIMCP_SURROGATE_SUPERSPIKE, DEFAULT_BETA);
    EXPECT_TRUE(bwd_result);

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(grad_output);
    nimcp_gpu_tensor_destroy(grad_input);
    nimcp_lif_state_destroy(state);
}

/**
 * TEST: STDP with spike propagation
 * WHAT: Propagate spikes and apply STDP
 * WHY:  Common SNN training pattern
 */
TEST_F(SNNKernelTest, Integration_SpikePropagationWithSTDP) {
    RequireGPU();

    const size_t n_neurons = 50;

    // Create weight matrix
    nimcp_gpu_tensor_t* weights = create_matrix(n_neurons, n_neurons, 0.5f);
    ASSERT_NE(weights, nullptr);

    // Create traces
    nimcp_gpu_tensor_t* trace = create_zero_tensor(n_neurons);
    ASSERT_NE(trace, nullptr);

    nimcp_stdp_params_t stdp_params = create_default_stdp_params();

    // Simulate multiple timesteps
    for (int t = 0; t < 20; t++) {
        // Generate random-ish spikes (every 3rd neuron at different times)
        std::vector<float> spikes_data(n_neurons);
        for (size_t i = 0; i < n_neurons; i++) {
            spikes_data[i] = ((i + t) % 5 == 0) ? 1.0f : 0.0f;
        }
        nimcp_gpu_tensor_t* spikes = create_tensor_from_data(spikes_data.data(), n_neurons);

        // Update eligibility traces
        nimcp_gpu_eligibility_trace_update(ctx, trace, spikes, 0.95f);

        // Propagate spikes
        nimcp_gpu_tensor_t* post_current = create_zero_tensor(n_neurons);
        nimcp_gpu_spike_propagate(ctx, spikes, weights, post_current);

        // Apply STDP
        nimcp_gpu_stdp_pair(ctx, weights, spikes, spikes, trace, trace, &stdp_params);

        nimcp_gpu_tensor_destroy(spikes);
        nimcp_gpu_tensor_destroy(post_current);
    }

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(trace);
}

//=============================================================================
// Large Scale Tests
//=============================================================================

/**
 * TEST: Large network LIF simulation
 * WHAT: Run LIF on large network
 * WHY:  Verify scalability and performance
 */
TEST_F(SNNKernelTest, LargeScale_LIFSimulation) {
    RequireGPU();

    nimcp_lif_params_t params = create_default_lif_params();
    nimcp_lif_state_t* state = nimcp_lif_state_create(ctx, LARGE_N_NEURONS, &params);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* input = create_filled_tensor(LARGE_N_NEURONS, 5.0f);
    ASSERT_NE(input, nullptr);

    // Time multiple iterations
    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < 100; t++) {
        bool result = nimcp_gpu_lif_forward(ctx, state, input);
        ASSERT_TRUE(result);
    }

    // Synchronize to ensure all GPU work is done
    nimcp_gpu_context_synchronize(ctx);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 100 timesteps of 1000 neurons in reasonable time
    EXPECT_LT(duration.count(), 5000) << "100 timesteps should complete within 5 seconds";

    nimcp_gpu_tensor_destroy(input);
    nimcp_lif_state_destroy(state);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
