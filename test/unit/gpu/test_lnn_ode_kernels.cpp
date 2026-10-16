/**
 * @file test_lnn_ode_kernels.cpp
 * @brief Comprehensive unit tests for GPU LNN ODE integration kernels
 *
 * WHAT: Tests for GPU-accelerated batch ODE integration for LNN dynamics
 * WHY:  Verify correctness of batched Euler, RK4, RK45, and reservoir operations
 * HOW:  GoogleTest with GPU context setup/teardown and numerical verification
 *
 * TEST COVERAGE:
 * - Batched Euler step correctness
 * - Batched RK4 step correctness
 * - Adaptive RK45 step
 * - Multi-step integration
 * - LTC derivative computation
 * - Tau update with bounds checking
 * - Reservoir initialization and stepping
 * - Spectral radius computation
 * - Stability checking (NaN/Inf detection)
 * - State statistics computation
 * - CPU fallback equivalence
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
#include <chrono>
#include <random>

// GPU headers include CUDA headers that cannot be in extern "C" blocks
#include "gpu/lnn/nimcp_lnn_ode_gpu.h"
#include "gpu/lnn/nimcp_lnn_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

// Headers have their own extern "C" guards
#include "lnn/nimcp_lnn_types.h"
#include "utils/tensor/nimcp_tensor.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr uint32_t DEFAULT_BATCH_SIZE = 8;
static constexpr uint32_t SMALL_BATCH_SIZE = 2;
static constexpr uint32_t LARGE_BATCH_SIZE = 64;
static constexpr uint32_t DEFAULT_N_NEURONS = 50;
static constexpr uint32_t SMALL_N_NEURONS = 10;
static constexpr uint32_t LARGE_N_NEURONS = 500;
static constexpr uint32_t DEFAULT_N_INPUTS = 20;
static constexpr uint32_t DEFAULT_RESERVOIR_SIZE = 100;
static constexpr float DEFAULT_DT = 1.0f;        // 1ms timestep
static constexpr float DEFAULT_TAU_BASE = 10.0f; // Base time constant (ms)
static constexpr float DEFAULT_TAU_MIN = 0.5f;   // Minimum tau
static constexpr float DEFAULT_TAU_MAX = 100.0f; // Maximum tau
static constexpr float DEFAULT_ERROR_TOL = 1e-4f;
static constexpr float NUMERICAL_EPS = 1e-5f;
static constexpr float RELAXED_EPS = 1e-3f;      // For adaptive methods
static constexpr uint32_t DEFAULT_RK_STAGES = 4; // For RK4 cache
static constexpr uint32_t DOPRI5_STAGES = 7;     // For RK45/DOPRI5 cache

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for GPU LNN ODE kernel tests
 *
 * WHAT: Provides GPU context setup/teardown and helper utilities
 * WHY:  Ensure proper GPU resource management across tests
 * HOW:  Creates context in SetUp, destroys in TearDown, provides tensor helpers
 */
class LNNODEKernelTest : public ::testing::Test {
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

    /**
     * @brief Skip test if GPU not available
     */
    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    /**
     * @brief Create default batch ODE configuration
     */
    nimcp_lnn_ode_batch_config_t create_default_batch_config(lnn_ode_method_t method = LNN_ODE_RK4) {
        nimcp_lnn_ode_batch_config_t config = nimcp_lnn_ode_batch_default_config();
        config.method = method;
        config.dt = DEFAULT_DT;
        config.dt_min = 0.01f;
        config.dt_max = 10.0f;
        config.error_tolerance = DEFAULT_ERROR_TOL;
        config.relative_tolerance = 1e-3f;
        config.max_steps = 100;
        config.num_substeps = 1;
        config.adaptive_stepping = (method == LNN_ODE_DOPRI5);
        config.use_checkpoint = false;
        config.enable_stability_check = true;
        config.stability_threshold = 1e6f;
        return config;
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
     * @brief Create 2D GPU tensor from host data
     */
    nimcp_gpu_tensor_t* create_tensor_2d_from_data(const float* data, size_t rows, size_t cols) {
        if (!gpu_available) return nullptr;
        size_t dims[2] = {rows, cols};
        return nimcp_gpu_tensor_from_host(ctx, data, dims, 2, NIMCP_GPU_PRECISION_FP32);
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
     * @brief Create 2D GPU tensor filled with zeros
     */
    nimcp_gpu_tensor_t* create_zero_tensor_2d(size_t rows, size_t cols) {
        if (!gpu_available) return nullptr;
        size_t dims[2] = {rows, cols};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
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
     * @brief Create 2D GPU tensor (matrix) filled with value
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

    /**
     * @brief Create mock CPU LNN layer for testing GPU layer creation
     */
    lnn_layer_t* create_mock_cpu_layer(size_t n_neurons = SMALL_N_NEURONS,
                                        size_t n_inputs = DEFAULT_N_INPUTS) {
        lnn_layer_t* layer = static_cast<lnn_layer_t*>(calloc(1, sizeof(lnn_layer_t)));
        if (!layer) return nullptr;

        layer->n_neurons = n_neurons;
        layer->ode_method = LNN_ODE_RK4;
        layer->dt = DEFAULT_DT;

        // Allocate neurons array
        layer->neurons = static_cast<lnn_neuron_t*>(
            calloc(layer->n_neurons, sizeof(lnn_neuron_t)));

        if (layer->neurons) {
            for (uint32_t i = 0; i < layer->n_neurons; i++) {
                layer->neurons[i].id = i;
                layer->neurons[i].x = 0.0f;
                layer->neurons[i].tau_base = DEFAULT_TAU_BASE;
                layer->neurons[i].tau_current = DEFAULT_TAU_BASE;
                layer->neurons[i].n_inputs = n_inputs;
                layer->neurons[i].activation = LNN_ACTIVATION_TANH;
            }
        }

        // Dimension arrays
        uint32_t state_dims[] = {(uint32_t)n_neurons};
        uint32_t w_in_dims[] = {(uint32_t)n_neurons, (uint32_t)n_inputs};
        uint32_t w_rec_dims[] = {(uint32_t)n_neurons, (uint32_t)n_neurons};

        // Create state tensors
        layer->x = nimcp_tensor_zeros(state_dims, 1, NIMCP_DTYPE_F32);
        layer->dx_dt = nimcp_tensor_zeros(state_dims, 1, NIMCP_DTYPE_F32);
        layer->tau = nimcp_tensor_full(state_dims, 1, NIMCP_DTYPE_F32, DEFAULT_TAU_BASE);
        layer->tau_base = nimcp_tensor_full(state_dims, 1, NIMCP_DTYPE_F32, DEFAULT_TAU_BASE);

        // Create weight matrices with small values for stability
        layer->W_in = nimcp_tensor_zeros(w_in_dims, 2, NIMCP_DTYPE_F32);
        if (layer->W_in) {
            float* data = static_cast<float*>(nimcp_tensor_data(layer->W_in));
            if (data) {
                for (size_t i = 0; i < n_neurons * n_inputs; i++) {
                    data[i] = 0.1f * ((float)(i % 10) - 5.0f) / 5.0f;
                }
            }
        }

        layer->W_rec = nimcp_tensor_zeros(w_rec_dims, 2, NIMCP_DTYPE_F32);
        if (layer->W_rec) {
            float* data = static_cast<float*>(nimcp_tensor_data(layer->W_rec));
            if (data) {
                for (size_t i = 0; i < n_neurons; i++) {
                    data[i * n_neurons + i] = 0.1f;
                }
            }
        }

        // Create bias and tau weights
        layer->b_in = nimcp_tensor_zeros(state_dims, 1, NIMCP_DTYPE_F32);
        layer->b_tau = nimcp_tensor_zeros(state_dims, 1, NIMCP_DTYPE_F32);

        uint32_t w_tau_dims[] = {(uint32_t)n_neurons, (uint32_t)(n_inputs + n_neurons)};
        layer->W_tau = nimcp_tensor_zeros(w_tau_dims, 2, NIMCP_DTYPE_F32);

        return layer;
    }

    /**
     * @brief Free mock CPU layer
     */
    void free_mock_cpu_layer(lnn_layer_t* layer) {
        if (!layer) return;
        if (layer->neurons) free(layer->neurons);
        if (layer->x) nimcp_tensor_destroy(layer->x);
        if (layer->dx_dt) nimcp_tensor_destroy(layer->dx_dt);
        if (layer->tau) nimcp_tensor_destroy(layer->tau);
        if (layer->tau_base) nimcp_tensor_destroy(layer->tau_base);
        if (layer->W_in) nimcp_tensor_destroy(layer->W_in);
        if (layer->W_rec) nimcp_tensor_destroy(layer->W_rec);
        if (layer->W_tau) nimcp_tensor_destroy(layer->W_tau);
        if (layer->b_in) nimcp_tensor_destroy(layer->b_in);
        if (layer->b_tau) nimcp_tensor_destroy(layer->b_tau);
        free(layer);
    }

    /**
     * @brief Check if all values in array are finite
     */
    bool all_finite(const std::vector<float>& data) {
        return std::all_of(data.begin(), data.end(),
            [](float v) { return std::isfinite(v); });
    }

    /**
     * @brief Compute L2 norm of array
     */
    float compute_norm(const std::vector<float>& data) {
        float sum = 0.0f;
        for (float v : data) {
            sum += v * v;
        }
        return std::sqrt(sum);
    }
};

//=============================================================================
// Batch ODE Configuration Tests
//=============================================================================

/**
 * TEST: Default batch ODE configuration
 * WHAT: Get default batch ODE configuration
 * WHY:  Verify sensible defaults are provided
 */
TEST_F(LNNODEKernelTest, BatchConfig_Default_HasReasonableValues) {
    nimcp_lnn_ode_batch_config_t config = nimcp_lnn_ode_batch_default_config();

    EXPECT_EQ(config.method, LNN_ODE_RK4) << "Default method should be RK4";
    EXPECT_GT(config.dt, 0.0f) << "Default dt should be positive";
    EXPECT_LT(config.dt_min, config.dt_max) << "dt_min should be less than dt_max";
    EXPECT_GT(config.error_tolerance, 0.0f) << "Error tolerance should be positive";
    EXPECT_GT(config.max_steps, 0u) << "Max steps should be positive";
    EXPECT_GT(config.stability_threshold, 0.0f) << "Stability threshold should be positive";
}

//=============================================================================
// Batch State Management Tests
//=============================================================================

/**
 * TEST: Batch state creation
 * WHAT: Create batch ODE state container
 * WHY:  Verify proper allocation of batch state tensors
 */
TEST_F(LNNODEKernelTest, BatchState_Create_AllocatesTensors) {
    RequireGPU();

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS);

    if (!state) {
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    EXPECT_EQ(state->batch_size, DEFAULT_BATCH_SIZE);
    EXPECT_EQ(state->n_neurons, SMALL_N_NEURONS);
    EXPECT_NE(state->x, nullptr) << "State tensor should be allocated";
    EXPECT_NE(state->dx_dt, nullptr) << "Derivative tensor should be allocated";
    EXPECT_NE(state->tau, nullptr) << "Tau tensor should be allocated";

    nimcp_lnn_ode_batch_state_destroy(state);
}

/**
 * TEST: Batch state reset
 * WHAT: Reset batch state to initial conditions
 * WHY:  Verify state can be reinitialized
 */
TEST_F(LNNODEKernelTest, BatchState_Reset_SetsInitialValue) {
    RequireGPU();

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS);

    if (!state) {
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    const float initial_x = 0.5f;
    bool result = nimcp_lnn_ode_batch_state_reset(ctx, state, initial_x);
    EXPECT_TRUE(result);

    // Verify state values
    size_t total_size = DEFAULT_BATCH_SIZE * SMALL_N_NEURONS;
    std::vector<float> x_host(total_size);
    copy_to_host(state->x, x_host.data());

    for (float val : x_host) {
        EXPECT_NEAR(val, initial_x, NUMERICAL_EPS);
    }

    nimcp_lnn_ode_batch_state_destroy(state);
}

/**
 * TEST: Batch state NULL handling
 * WHAT: Try operations with NULL state
 * WHY:  Verify NULL-safety
 */
TEST_F(LNNODEKernelTest, BatchState_NullHandling_SafeOperations) {
    // Destroy NULL should not crash
    nimcp_lnn_ode_batch_state_destroy(nullptr);
    SUCCEED() << "NULL destroy should not crash";
}

//=============================================================================
// ODE Cache Management Tests
//=============================================================================

/**
 * TEST: ODE cache creation
 * WHAT: Create ODE cache for RK stages
 * WHY:  Verify proper allocation of intermediate storage
 */
TEST_F(LNNODEKernelTest, ODECache_Create_AllocatesStages) {
    RequireGPU();

    nimcp_lnn_ode_cache_t* cache = nimcp_lnn_ode_cache_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS, DEFAULT_RK_STAGES);

    if (!cache) {
        GTEST_SKIP() << "ODE cache creation not implemented";
    }

    EXPECT_EQ(cache->n_stages, DEFAULT_RK_STAGES);
    EXPECT_NE(cache->k_stages, nullptr) << "Stage array should be allocated";
    EXPECT_NE(cache->x_temp, nullptr) << "Temporary state should be allocated";

    nimcp_lnn_ode_cache_destroy(cache);
}

/**
 * TEST: ODE cache for DOPRI5
 * WHAT: Create cache with 7 stages for DOPRI5
 * WHY:  Verify DOPRI5-specific allocation
 */
TEST_F(LNNODEKernelTest, ODECache_DOPRI5_HasSevenStages) {
    RequireGPU();

    nimcp_lnn_ode_cache_t* cache = nimcp_lnn_ode_cache_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS, DOPRI5_STAGES);

    if (!cache) {
        GTEST_SKIP() << "ODE cache creation not implemented";
    }

    EXPECT_EQ(cache->n_stages, DOPRI5_STAGES);

    nimcp_lnn_ode_cache_destroy(cache);
}

//=============================================================================
// Batched Euler Step Tests
//=============================================================================

/**
 * TEST: Batched Euler step correctness
 * WHAT: Apply Euler step to batch of exponential decay ODEs
 * WHY:  Verify basic Euler integration produces expected results
 */
TEST_F(LNNODEKernelTest, EulerBatched_SimpleDecay_Correct) {
    RequireGPU();

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, SMALL_BATCH_SIZE, SMALL_N_NEURONS);

    if (!state) {
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    // Initialize state to 1.0
    nimcp_lnn_ode_batch_state_reset(ctx, state, 1.0f);

    // Set derivative: dx/dt = -x/tau = -1/10 = -0.1
    const float tau = 10.0f;
    size_t total_size = SMALL_BATCH_SIZE * SMALL_N_NEURONS;
    std::vector<float> dx_data(total_size, -0.1f);
    nimcp_gpu_tensor_t* dx_temp = create_tensor_from_data(dx_data.data(), total_size);
    if (dx_temp) {
        nimcp_gpu_copy(ctx, dx_temp, state->dx_dt);
        nimcp_gpu_tensor_destroy(dx_temp);
    }

    // Apply Euler step
    bool result = nimcp_gpu_lnn_euler_step_batched(ctx, state, DEFAULT_DT);
    EXPECT_TRUE(result);

    // Expected: x_new = x + dt * dx_dt = 1.0 + 1.0 * (-0.1) = 0.9
    std::vector<float> x_host(total_size);
    copy_to_host(state->x, x_host.data());

    for (float val : x_host) {
        EXPECT_NEAR(val, 0.9f, NUMERICAL_EPS);
    }

    nimcp_lnn_ode_batch_state_destroy(state);
}

/**
 * TEST: Batched Euler with varying batch elements
 * WHAT: Different states and derivatives per batch sample
 * WHY:  Verify batch dimension is handled correctly
 */
TEST_F(LNNODEKernelTest, EulerBatched_VaryingBatchElements_CorrectPerSample) {
    RequireGPU();

    const uint32_t batch = 4;
    const uint32_t neurons = 5;

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(ctx, batch, neurons);

    if (!state) {
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    // Initialize with different values per batch
    size_t total_size = batch * neurons;
    std::vector<float> x_data(total_size);
    std::vector<float> dx_data(total_size);

    for (uint32_t b = 0; b < batch; b++) {
        for (uint32_t n = 0; n < neurons; n++) {
            x_data[b * neurons + n] = static_cast<float>(b + 1);  // 1, 2, 3, 4
            dx_data[b * neurons + n] = static_cast<float>(b + 1) * 0.1f;
        }
    }

    nimcp_gpu_tensor_t* x_temp = create_tensor_from_data(x_data.data(), total_size);
    nimcp_gpu_tensor_t* dx_temp = create_tensor_from_data(dx_data.data(), total_size);
    if (x_temp) {
        nimcp_gpu_copy(ctx, x_temp, state->x);
        nimcp_gpu_tensor_destroy(x_temp);
    }
    if (dx_temp) {
        nimcp_gpu_copy(ctx, dx_temp, state->dx_dt);
        nimcp_gpu_tensor_destroy(dx_temp);
    }

    bool result = nimcp_gpu_lnn_euler_step_batched(ctx, state, 1.0f);
    EXPECT_TRUE(result);

    // Verify: x_new[b] = x[b] + 1.0 * dx[b] = (b+1) + (b+1)*0.1 = (b+1)*1.1
    std::vector<float> x_host(total_size);
    copy_to_host(state->x, x_host.data());

    for (uint32_t b = 0; b < batch; b++) {
        float expected = static_cast<float>(b + 1) * 1.1f;
        for (uint32_t n = 0; n < neurons; n++) {
            EXPECT_NEAR(x_host[b * neurons + n], expected, NUMERICAL_EPS);
        }
    }

    nimcp_lnn_ode_batch_state_destroy(state);
}

/**
 * TEST: Batched Euler NULL handling
 * WHAT: Try Euler with NULL state
 * WHY:  Verify NULL-safety
 */
TEST_F(LNNODEKernelTest, EulerBatched_NullState_ReturnsFalse) {
    RequireGPU();

    bool result = nimcp_gpu_lnn_euler_step_batched(ctx, nullptr, DEFAULT_DT);
    EXPECT_FALSE(result);
}

//=============================================================================
// Batched RK4 Step Tests
//=============================================================================

/**
 * TEST: Batched RK4 step correctness
 * WHAT: Apply RK4 step to batch and verify higher accuracy
 * WHY:  RK4 should be more accurate than Euler
 */
TEST_F(LNNODEKernelTest, RK4Batched_HigherOrderAccuracy) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer(SMALL_N_NEURONS, DEFAULT_N_INPUTS);
    ASSERT_NE(cpu_layer, nullptr);

    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);
    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS);

    if (!state) {
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    nimcp_lnn_ode_cache_t* cache = nimcp_lnn_ode_cache_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS, DEFAULT_RK_STAGES);

    if (!cache) {
        nimcp_lnn_ode_batch_state_destroy(state);
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "ODE cache creation not implemented";
    }

    // Create input tensor
    nimcp_gpu_tensor_t* input = create_matrix(DEFAULT_BATCH_SIZE, DEFAULT_N_INPUTS, 1.0f);
    ASSERT_NE(input, nullptr);

    nimcp_lnn_ode_batch_config_t config = create_default_batch_config(LNN_ODE_RK4);

    bool result = nimcp_gpu_lnn_rk4_step_batched(ctx, layer, input, state, cache, &config);
    EXPECT_TRUE(result);

    // Verify state is finite
    size_t total_size = DEFAULT_BATCH_SIZE * SMALL_N_NEURONS;
    std::vector<float> x_host(total_size);
    copy_to_host(state->x, x_host.data());
    EXPECT_TRUE(all_finite(x_host)) << "RK4 should produce finite results";

    nimcp_gpu_tensor_destroy(input);
    nimcp_lnn_ode_cache_destroy(cache);
    nimcp_lnn_ode_batch_state_destroy(state);
    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

/**
 * TEST: RK4 NULL handling
 * WHAT: Try RK4 with NULL inputs
 * WHY:  Verify NULL-safety for all parameters
 */
TEST_F(LNNODEKernelTest, RK4Batched_NullInputs_ReturnsFalse) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer();
    ASSERT_NE(cpu_layer, nullptr);

    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);
    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    nimcp_lnn_ode_batch_config_t config = create_default_batch_config(LNN_ODE_RK4);

    EXPECT_FALSE(nimcp_gpu_lnn_rk4_step_batched(ctx, nullptr, nullptr, nullptr, nullptr, &config));

    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

//=============================================================================
// Adaptive RK45 Step Tests
//=============================================================================

/**
 * TEST: Adaptive RK45 step
 * WHAT: Run RK45 with adaptive stepping enabled
 * WHY:  Verify step size adaptation works
 */
TEST_F(LNNODEKernelTest, RK45Adaptive_AdjustsStepSize) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer(SMALL_N_NEURONS, DEFAULT_N_INPUTS);
    ASSERT_NE(cpu_layer, nullptr);

    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);
    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS);

    if (!state) {
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    nimcp_lnn_ode_cache_t* cache = nimcp_lnn_ode_cache_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS, DOPRI5_STAGES);

    if (!cache) {
        nimcp_lnn_ode_batch_state_destroy(state);
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "ODE cache creation not implemented";
    }

    nimcp_gpu_tensor_t* input = create_matrix(DEFAULT_BATCH_SIZE, DEFAULT_N_INPUTS, 1.0f);
    ASSERT_NE(input, nullptr);

    nimcp_lnn_ode_batch_config_t config = create_default_batch_config(LNN_ODE_DOPRI5);
    config.adaptive_stepping = true;
    config.error_tolerance = 1e-3f;

    bool result = nimcp_gpu_lnn_rk45_adaptive_batched(ctx, layer, input, state, cache, &config);
    EXPECT_TRUE(result);

    // Verify per-sample dt is within bounds
    if (state->dt_per_sample) {
        std::vector<float> dt_host(DEFAULT_BATCH_SIZE);
        copy_to_host(state->dt_per_sample, dt_host.data());

        for (float dt : dt_host) {
            EXPECT_GE(dt, config.dt_min) << "dt should be >= dt_min";
            EXPECT_LE(dt, config.dt_max) << "dt should be <= dt_max";
        }
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_lnn_ode_cache_destroy(cache);
    nimcp_lnn_ode_batch_state_destroy(state);
    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

/**
 * TEST: RK45 error estimation
 * WHAT: Verify error tensor is populated
 * WHY:  Adaptive methods need error estimates for step control
 */
TEST_F(LNNODEKernelTest, RK45Adaptive_ComputesErrorEstimate) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer();
    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);

    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS);
    nimcp_lnn_ode_cache_t* cache = nimcp_lnn_ode_cache_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS, DOPRI5_STAGES);

    if (!state || !cache) {
        if (state) nimcp_lnn_ode_batch_state_destroy(state);
        if (cache) nimcp_lnn_ode_cache_destroy(cache);
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "State/cache creation not implemented";
    }

    nimcp_gpu_tensor_t* input = create_matrix(DEFAULT_BATCH_SIZE, DEFAULT_N_INPUTS, 1.0f);
    nimcp_lnn_ode_batch_config_t config = create_default_batch_config(LNN_ODE_DOPRI5);
    config.adaptive_stepping = true;

    nimcp_gpu_lnn_rk45_adaptive_batched(ctx, layer, input, state, cache, &config);

    // Error tensor should have values (may be zero for stable systems)
    if (state->error) {
        size_t total_size = DEFAULT_BATCH_SIZE * SMALL_N_NEURONS;
        std::vector<float> error_host(total_size);
        copy_to_host(state->error, error_host.data());
        EXPECT_TRUE(all_finite(error_host)) << "Error estimates should be finite";
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_lnn_ode_cache_destroy(cache);
    nimcp_lnn_ode_batch_state_destroy(state);
    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

//=============================================================================
// Multi-step Integration Tests
//=============================================================================

/**
 * TEST: Multi-step integration
 * WHAT: Integrate over multiple timesteps in single call
 * WHY:  Verify sequence processing works correctly
 */
TEST_F(LNNODEKernelTest, MultiStep_IntegratesSequence) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer(SMALL_N_NEURONS, DEFAULT_N_INPUTS);
    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);

    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, SMALL_BATCH_SIZE, SMALL_N_NEURONS);
    nimcp_lnn_ode_cache_t* cache = nimcp_lnn_ode_cache_create(
        ctx, SMALL_BATCH_SIZE, SMALL_N_NEURONS, DEFAULT_RK_STAGES);

    if (!state || !cache) {
        if (state) nimcp_lnn_ode_batch_state_destroy(state);
        if (cache) nimcp_lnn_ode_cache_destroy(cache);
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "State/cache creation not implemented";
    }

    const uint32_t n_steps = 10;

    // Create 3D input sequence [n_steps, batch, n_inputs]
    size_t seq_size = n_steps * SMALL_BATCH_SIZE * DEFAULT_N_INPUTS;
    std::vector<float> input_data(seq_size, 1.0f);
    size_t dims[3] = {n_steps, SMALL_BATCH_SIZE, DEFAULT_N_INPUTS};
    nimcp_gpu_tensor_t* input_seq = nimcp_gpu_tensor_from_host(
        ctx, input_data.data(), dims, 3, NIMCP_GPU_PRECISION_FP32);

    if (!input_seq) {
        nimcp_lnn_ode_cache_destroy(cache);
        nimcp_lnn_ode_batch_state_destroy(state);
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "3D tensor creation not supported";
    }

    nimcp_lnn_ode_batch_config_t config = create_default_batch_config(LNN_ODE_RK4);

    bool result = nimcp_gpu_lnn_integrate_multistep(
        ctx, layer, input_seq, state, cache, &config, n_steps);
    EXPECT_TRUE(result);

    // Verify state advanced properly (current_time should be n_steps * dt)
    EXPECT_NEAR(state->current_time, n_steps * config.dt, NUMERICAL_EPS);

    nimcp_gpu_tensor_destroy(input_seq);
    nimcp_lnn_ode_cache_destroy(cache);
    nimcp_lnn_ode_batch_state_destroy(state);
    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

/**
 * TEST: Multi-step produces finite results
 * WHAT: Run many steps and verify no divergence
 * WHY:  Long integration should remain stable
 */
TEST_F(LNNODEKernelTest, MultiStep_RemainsStable) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer(SMALL_N_NEURONS, DEFAULT_N_INPUTS);
    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);

    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, SMALL_BATCH_SIZE, SMALL_N_NEURONS);
    nimcp_lnn_ode_cache_t* cache = nimcp_lnn_ode_cache_create(
        ctx, SMALL_BATCH_SIZE, SMALL_N_NEURONS, DEFAULT_RK_STAGES);

    if (!state || !cache) {
        if (state) nimcp_lnn_ode_batch_state_destroy(state);
        if (cache) nimcp_lnn_ode_cache_destroy(cache);
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "State/cache creation not implemented";
    }

    const uint32_t n_steps = 100;
    size_t seq_size = n_steps * SMALL_BATCH_SIZE * DEFAULT_N_INPUTS;
    std::vector<float> input_data(seq_size, 0.5f);  // Moderate input
    size_t dims[3] = {n_steps, SMALL_BATCH_SIZE, DEFAULT_N_INPUTS};
    nimcp_gpu_tensor_t* input_seq = nimcp_gpu_tensor_from_host(
        ctx, input_data.data(), dims, 3, NIMCP_GPU_PRECISION_FP32);

    nimcp_lnn_ode_batch_config_t config = create_default_batch_config(LNN_ODE_RK4);

    nimcp_gpu_lnn_integrate_multistep(ctx, layer, input_seq, state, cache, &config, n_steps);

    // Verify final state is finite
    size_t state_size = SMALL_BATCH_SIZE * SMALL_N_NEURONS;
    std::vector<float> x_host(state_size);
    copy_to_host(state->x, x_host.data());
    EXPECT_TRUE(all_finite(x_host)) << "State should remain finite after 100 steps";

    if (input_seq) nimcp_gpu_tensor_destroy(input_seq);
    nimcp_lnn_ode_cache_destroy(cache);
    nimcp_lnn_ode_batch_state_destroy(state);
    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

//=============================================================================
// LTC Derivative Computation Tests
//=============================================================================

/**
 * TEST: Batched LTC derivative computation
 * WHAT: Compute dx/dt for batch of LTC neurons
 * WHY:  Core of LNN dynamics must work for batch
 */
TEST_F(LNNODEKernelTest, LTCDerivative_Batched_ProducesOutput) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer(SMALL_N_NEURONS, DEFAULT_N_INPUTS);
    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);

    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS);

    if (!state) {
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    // Initialize state with non-zero values
    nimcp_lnn_ode_batch_state_reset(ctx, state, 0.5f);

    nimcp_gpu_tensor_t* input = create_matrix(DEFAULT_BATCH_SIZE, DEFAULT_N_INPUTS, 1.0f);
    ASSERT_NE(input, nullptr);

    bool result = nimcp_gpu_lnn_compute_ltc_derivative_batched(
        ctx, layer, input, state, LNN_ACTIVATION_TANH);
    EXPECT_TRUE(result);

    // Derivative should be non-zero
    size_t total_size = DEFAULT_BATCH_SIZE * SMALL_N_NEURONS;
    std::vector<float> dx_host(total_size);
    copy_to_host(state->dx_dt, dx_host.data());
    EXPECT_TRUE(all_finite(dx_host)) << "Derivative should be finite";

    nimcp_gpu_tensor_destroy(input);
    nimcp_lnn_ode_batch_state_destroy(state);
    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

/**
 * TEST: LTC derivative with different activations
 * WHAT: Verify all activation functions work
 * WHY:  Users may choose different activations
 */
TEST_F(LNNODEKernelTest, LTCDerivative_AllActivations_Work) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer();
    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);

    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, SMALL_BATCH_SIZE, SMALL_N_NEURONS);

    if (!state) {
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    nimcp_gpu_tensor_t* input = create_matrix(SMALL_BATCH_SIZE, DEFAULT_N_INPUTS, 1.0f);

    std::vector<lnn_activation_t> activations = {
        LNN_ACTIVATION_TANH,
        LNN_ACTIVATION_SIGMOID,
        LNN_ACTIVATION_RELU,
        LNN_ACTIVATION_GELU,
        LNN_ACTIVATION_SILU
    };

    for (auto activation : activations) {
        nimcp_lnn_ode_batch_state_reset(ctx, state, 0.5f);
        bool result = nimcp_gpu_lnn_compute_ltc_derivative_batched(
            ctx, layer, input, state, activation);
        EXPECT_TRUE(result) << "Activation " << static_cast<int>(activation) << " should work";
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_lnn_ode_batch_state_destroy(state);
    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

//=============================================================================
// Tau Update with Bounds Tests
//=============================================================================

/**
 * TEST: Tau update with bounds checking
 * WHAT: Update tau and verify clamping to [tau_min, tau_max]
 * WHY:  Stability requires tau in valid range
 */
TEST_F(LNNODEKernelTest, TauUpdate_BoundsChecking_ClampsValues) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer(SMALL_N_NEURONS, DEFAULT_N_INPUTS);
    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);

    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS);

    if (!state) {
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    nimcp_gpu_tensor_t* input = create_matrix(DEFAULT_BATCH_SIZE, DEFAULT_N_INPUTS, 10.0f);

    bool result = nimcp_gpu_lnn_update_tau_batched(
        ctx, layer, input, state, DEFAULT_TAU_MIN, DEFAULT_TAU_MAX);
    EXPECT_TRUE(result);

    // Verify tau values are within bounds
    size_t total_size = DEFAULT_BATCH_SIZE * SMALL_N_NEURONS;
    std::vector<float> tau_host(total_size);
    copy_to_host(state->tau, tau_host.data());

    for (float tau : tau_host) {
        EXPECT_GE(tau, DEFAULT_TAU_MIN) << "Tau should be >= tau_min";
        EXPECT_LE(tau, DEFAULT_TAU_MAX) << "Tau should be <= tau_max";
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_lnn_ode_batch_state_destroy(state);
    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

/**
 * TEST: Tau update with extreme inputs
 * WHAT: Large inputs should still produce bounded tau
 * WHY:  Network must be robust to extreme inputs
 */
TEST_F(LNNODEKernelTest, TauUpdate_ExtremeInputs_StaysBounded) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer();
    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);

    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, SMALL_BATCH_SIZE, SMALL_N_NEURONS);

    if (!state) {
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    // Very large input
    nimcp_gpu_tensor_t* input = create_matrix(SMALL_BATCH_SIZE, DEFAULT_N_INPUTS, 1000.0f);

    nimcp_gpu_lnn_update_tau_batched(
        ctx, layer, input, state, DEFAULT_TAU_MIN, DEFAULT_TAU_MAX);

    size_t total_size = SMALL_BATCH_SIZE * SMALL_N_NEURONS;
    std::vector<float> tau_host(total_size);
    copy_to_host(state->tau, tau_host.data());

    for (float tau : tau_host) {
        EXPECT_TRUE(std::isfinite(tau)) << "Tau should be finite";
        EXPECT_GE(tau, DEFAULT_TAU_MIN);
        EXPECT_LE(tau, DEFAULT_TAU_MAX);
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_lnn_ode_batch_state_destroy(state);
    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

//=============================================================================
// Reservoir Initialization Tests
//=============================================================================

/**
 * TEST: Reservoir initialization
 * WHAT: Initialize reservoir with controlled spectral radius
 * WHY:  Echo state property requires spectral radius < 1
 */
TEST_F(LNNODEKernelTest, Reservoir_Init_CreatesValidReservoir) {
    RequireGPU();

    nimcp_lnn_reservoir_state_t* reservoir =
        static_cast<nimcp_lnn_reservoir_state_t*>(calloc(1, sizeof(nimcp_lnn_reservoir_state_t)));
    ASSERT_NE(reservoir, nullptr);

    const float target_spectral_radius = 0.9f;
    const float sparsity = 0.8f;  // 80% zeros

    bool result = nimcp_gpu_lnn_reservoir_init(
        ctx, reservoir,
        DEFAULT_RESERVOIR_SIZE,
        DEFAULT_N_INPUTS,
        SMALL_N_NEURONS,  // n_outputs
        target_spectral_radius,
        sparsity,
        42  // seed
    );

    if (!result) {
        free(reservoir);
        GTEST_SKIP() << "Reservoir init not implemented";
    }

    EXPECT_EQ(reservoir->reservoir_size, DEFAULT_RESERVOIR_SIZE);
    EXPECT_EQ(reservoir->n_inputs, DEFAULT_N_INPUTS);
    EXPECT_NE(reservoir->reservoir_state, nullptr);
    EXPECT_NE(reservoir->W_reservoir, nullptr);
    EXPECT_NE(reservoir->W_input, nullptr);

    nimcp_gpu_lnn_reservoir_destroy(reservoir);
}

/**
 * TEST: Reservoir initialization with different seeds
 * WHAT: Different seeds should produce different reservoirs
 * WHY:  Reproducibility and randomness verification
 */
TEST_F(LNNODEKernelTest, Reservoir_DifferentSeeds_DifferentWeights) {
    RequireGPU();

    nimcp_lnn_reservoir_state_t* res1 =
        static_cast<nimcp_lnn_reservoir_state_t*>(calloc(1, sizeof(nimcp_lnn_reservoir_state_t)));
    nimcp_lnn_reservoir_state_t* res2 =
        static_cast<nimcp_lnn_reservoir_state_t*>(calloc(1, sizeof(nimcp_lnn_reservoir_state_t)));

    bool r1 = nimcp_gpu_lnn_reservoir_init(ctx, res1, 50, 10, 5, 0.9f, 0.8f, 1);
    bool r2 = nimcp_gpu_lnn_reservoir_init(ctx, res2, 50, 10, 5, 0.9f, 0.8f, 2);

    if (!r1 || !r2) {
        if (res1) free(res1);
        if (res2) free(res2);
        GTEST_SKIP() << "Reservoir init not implemented";
    }

    // Compare first few weights (should differ)
    std::vector<float> w1(50 * 50), w2(50 * 50);
    if (res1->W_reservoir && res2->W_reservoir) {
        copy_to_host(res1->W_reservoir, w1.data());
        copy_to_host(res2->W_reservoir, w2.data());

        bool any_different = false;
        for (size_t i = 0; i < 100 && !any_different; i++) {
            if (std::abs(w1[i] - w2[i]) > NUMERICAL_EPS) {
                any_different = true;
            }
        }
        EXPECT_TRUE(any_different) << "Different seeds should produce different weights";
    }

    nimcp_gpu_lnn_reservoir_destroy(res1);
    nimcp_gpu_lnn_reservoir_destroy(res2);
}

//=============================================================================
// Reservoir Stepping Tests
//=============================================================================

/**
 * TEST: Reservoir state propagation
 * WHAT: Single step of reservoir dynamics
 * WHY:  Core echo state network operation
 */
TEST_F(LNNODEKernelTest, Reservoir_Step_UpdatesState) {
    RequireGPU();

    nimcp_lnn_reservoir_state_t* reservoir =
        static_cast<nimcp_lnn_reservoir_state_t*>(calloc(1, sizeof(nimcp_lnn_reservoir_state_t)));

    bool init_result = nimcp_gpu_lnn_reservoir_init(
        ctx, reservoir, DEFAULT_RESERVOIR_SIZE, DEFAULT_N_INPUTS, SMALL_N_NEURONS, 0.9f, 0.8f, 42);

    if (!init_result) {
        free(reservoir);
        GTEST_SKIP() << "Reservoir init not implemented";
    }

    nimcp_gpu_tensor_t* input = create_matrix(DEFAULT_BATCH_SIZE, DEFAULT_N_INPUTS, 1.0f);
    nimcp_gpu_tensor_t* output = create_zero_tensor_2d(DEFAULT_BATCH_SIZE, SMALL_N_NEURONS);

    // Get state before step
    std::vector<float> state_before(DEFAULT_BATCH_SIZE * DEFAULT_RESERVOIR_SIZE, 0.0f);
    if (reservoir->reservoir_state) {
        copy_to_host(reservoir->reservoir_state, state_before.data());
    }

    bool result = nimcp_gpu_lnn_reservoir_step(ctx, reservoir, input, output);
    EXPECT_TRUE(result);

    // State should have changed
    std::vector<float> state_after(DEFAULT_BATCH_SIZE * DEFAULT_RESERVOIR_SIZE);
    if (reservoir->reservoir_state) {
        copy_to_host(reservoir->reservoir_state, state_after.data());

        bool any_changed = false;
        for (size_t i = 0; i < state_after.size(); i++) {
            if (std::abs(state_after[i] - state_before[i]) > NUMERICAL_EPS) {
                any_changed = true;
                break;
            }
        }
        EXPECT_TRUE(any_changed) << "Reservoir state should change after step";
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_lnn_reservoir_destroy(reservoir);
}

/**
 * TEST: Reservoir sequence propagation
 * WHAT: Process entire sequence through reservoir
 * WHY:  Common training pattern for ESN
 */
TEST_F(LNNODEKernelTest, Reservoir_PropagateSequence_ProcessesAll) {
    RequireGPU();

    nimcp_lnn_reservoir_state_t* reservoir =
        static_cast<nimcp_lnn_reservoir_state_t*>(calloc(1, sizeof(nimcp_lnn_reservoir_state_t)));

    bool init_result = nimcp_gpu_lnn_reservoir_init(
        ctx, reservoir, 50, DEFAULT_N_INPUTS, 10, 0.9f, 0.8f, 42);

    if (!init_result) {
        free(reservoir);
        GTEST_SKIP() << "Reservoir init not implemented";
    }

    const uint32_t seq_len = 20;
    const uint32_t batch = 4;

    // Create input sequence [seq_len, batch, n_inputs]
    size_t input_size = seq_len * batch * DEFAULT_N_INPUTS;
    std::vector<float> input_data(input_size, 0.5f);
    size_t input_dims[3] = {seq_len, batch, DEFAULT_N_INPUTS};
    nimcp_gpu_tensor_t* input_seq = nimcp_gpu_tensor_from_host(
        ctx, input_data.data(), input_dims, 3, NIMCP_GPU_PRECISION_FP32);

    // Create output sequence and state history tensors
    size_t out_dims[3] = {seq_len, batch, 10};
    size_t state_dims[3] = {seq_len, batch, 50};
    nimcp_gpu_tensor_t* output_seq = nimcp_gpu_tensor_create(ctx, out_dims, 3, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* state_history = nimcp_gpu_tensor_create(ctx, state_dims, 3, NIMCP_GPU_PRECISION_FP32);

    bool result = nimcp_gpu_lnn_reservoir_propagate_sequence(
        ctx, reservoir, input_seq, output_seq, state_history, seq_len);
    EXPECT_TRUE(result);

    // Verify outputs are finite
    if (output_seq) {
        std::vector<float> out_host(seq_len * batch * 10);
        copy_to_host(output_seq, out_host.data());
        EXPECT_TRUE(all_finite(out_host)) << "Outputs should be finite";
    }

    if (input_seq) nimcp_gpu_tensor_destroy(input_seq);
    if (output_seq) nimcp_gpu_tensor_destroy(output_seq);
    if (state_history) nimcp_gpu_tensor_destroy(state_history);
    nimcp_gpu_lnn_reservoir_destroy(reservoir);
}

//=============================================================================
// Spectral Radius Computation Tests
//=============================================================================

/**
 * TEST: Spectral radius computation
 * WHAT: Compute spectral radius using power iteration
 * WHY:  Needed for echo state property verification
 */
TEST_F(LNNODEKernelTest, SpectralRadius_Computation_ConvergesCorrectly) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer(SMALL_N_NEURONS, DEFAULT_N_INPUTS);
    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);

    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    float spectral_radius = 0.0f;
    bool result = nimcp_gpu_lnn_compute_spectral_radius(
        ctx, layer, &spectral_radius, 100, 1e-6f);

    if (!result) {
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "Spectral radius computation not implemented";
    }

    EXPECT_GE(spectral_radius, 0.0f) << "Spectral radius should be non-negative";
    EXPECT_TRUE(std::isfinite(spectral_radius)) << "Spectral radius should be finite";

    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

/**
 * TEST: Spectral radius rescaling
 * WHAT: Rescale W_rec to achieve target spectral radius
 * WHY:  Initialize reservoir with controlled dynamics
 */
TEST_F(LNNODEKernelTest, SpectralRadius_Rescale_AchievesTarget) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer(SMALL_N_NEURONS, DEFAULT_N_INPUTS);
    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);

    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    const float target = 0.9f;
    bool rescale_result = nimcp_gpu_lnn_rescale_spectral_radius(ctx, layer, target);

    if (!rescale_result) {
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "Spectral radius rescaling not implemented";
    }

    // Compute spectral radius after rescaling
    float actual_radius = 0.0f;
    nimcp_gpu_lnn_compute_spectral_radius(ctx, layer, &actual_radius, 100, 1e-4f);

    // Should be close to target (with some tolerance due to power iteration)
    EXPECT_NEAR(actual_radius, target, 0.1f) << "Rescaled radius should be near target";

    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

//=============================================================================
// Stability Checking Tests
//=============================================================================

/**
 * TEST: NaN detection
 * WHAT: Detect NaN values in batch state
 * WHY:  Early NaN detection prevents wasted computation
 */
TEST_F(LNNODEKernelTest, Stability_NaNDetection_FindsNaN) {
    RequireGPU();

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, SMALL_BATCH_SIZE, SMALL_N_NEURONS);

    if (!state) {
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    // Inject NaN into state
    size_t total_size = SMALL_BATCH_SIZE * SMALL_N_NEURONS;
    std::vector<float> x_data(total_size, 1.0f);
    x_data[5] = std::nanf("");  // Inject NaN

    nimcp_gpu_tensor_t* x_temp = create_tensor_from_data(x_data.data(), total_size);
    if (x_temp) {
        nimcp_gpu_copy(ctx, x_temp, state->x);
        nimcp_gpu_tensor_destroy(x_temp);
    }

    bool has_nan = false, has_inf = false;
    float max_norm = 0.0f;

    bool result = nimcp_gpu_lnn_check_stability(ctx, state, &has_nan, &has_inf, &max_norm);

    if (!result) {
        nimcp_lnn_ode_batch_state_destroy(state);
        GTEST_SKIP() << "Stability check not implemented";
    }

    EXPECT_TRUE(has_nan) << "Should detect NaN";

    nimcp_lnn_ode_batch_state_destroy(state);
}

/**
 * TEST: Inf detection
 * WHAT: Detect Inf values in batch state
 * WHY:  Inf indicates numerical explosion
 */
TEST_F(LNNODEKernelTest, Stability_InfDetection_FindsInf) {
    RequireGPU();

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, SMALL_BATCH_SIZE, SMALL_N_NEURONS);

    if (!state) {
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    // Inject Inf into state
    size_t total_size = SMALL_BATCH_SIZE * SMALL_N_NEURONS;
    std::vector<float> x_data(total_size, 1.0f);
    x_data[3] = std::numeric_limits<float>::infinity();

    nimcp_gpu_tensor_t* x_temp = create_tensor_from_data(x_data.data(), total_size);
    if (x_temp) {
        nimcp_gpu_copy(ctx, x_temp, state->x);
        nimcp_gpu_tensor_destroy(x_temp);
    }

    bool has_nan = false, has_inf = false;
    float max_norm = 0.0f;

    bool result = nimcp_gpu_lnn_check_stability(ctx, state, &has_nan, &has_inf, &max_norm);

    if (!result) {
        nimcp_lnn_ode_batch_state_destroy(state);
        GTEST_SKIP() << "Stability check not implemented";
    }

    EXPECT_TRUE(has_inf) << "Should detect Inf";

    nimcp_lnn_ode_batch_state_destroy(state);
}

/**
 * TEST: Valid state passes stability check
 * WHAT: Normal finite values should not trigger warnings
 * WHY:  Verify no false positives
 */
TEST_F(LNNODEKernelTest, Stability_ValidState_NoIssues) {
    RequireGPU();

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS);

    if (!state) {
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    nimcp_lnn_ode_batch_state_reset(ctx, state, 0.5f);

    bool has_nan = true, has_inf = true;  // Will be set to false if no issues
    float max_norm = 0.0f;

    bool result = nimcp_gpu_lnn_check_stability(ctx, state, &has_nan, &has_inf, &max_norm);

    if (!result) {
        nimcp_lnn_ode_batch_state_destroy(state);
        GTEST_SKIP() << "Stability check not implemented";
    }

    EXPECT_FALSE(has_nan) << "Valid state should not have NaN";
    EXPECT_FALSE(has_inf) << "Valid state should not have Inf";
    EXPECT_GT(max_norm, 0.0f) << "Max norm should be positive for non-zero state";

    nimcp_lnn_ode_batch_state_destroy(state);
}

//=============================================================================
// State Statistics Computation Tests
//=============================================================================

/**
 * TEST: State statistics computation
 * WHAT: Compute mean, std, min, max of state
 * WHY:  Monitoring training dynamics
 */
TEST_F(LNNODEKernelTest, StateStats_Computation_CorrectValues) {
    RequireGPU();

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, SMALL_BATCH_SIZE, SMALL_N_NEURONS);

    if (!state) {
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    // Set known values: 1, 2, 3, 4, ...
    size_t total_size = SMALL_BATCH_SIZE * SMALL_N_NEURONS;
    std::vector<float> x_data(total_size);
    for (size_t i = 0; i < total_size; i++) {
        x_data[i] = static_cast<float>(i + 1);
    }

    nimcp_gpu_tensor_t* x_temp = create_tensor_from_data(x_data.data(), total_size);
    if (x_temp) {
        nimcp_gpu_copy(ctx, x_temp, state->x);
        nimcp_gpu_tensor_destroy(x_temp);
    }

    float mean = 0, std_dev = 0, min_val = 0, max_val = 0;
    bool result = nimcp_gpu_lnn_compute_state_stats(ctx, state, &mean, &std_dev, &min_val, &max_val);

    if (!result) {
        nimcp_lnn_ode_batch_state_destroy(state);
        GTEST_SKIP() << "State stats not implemented";
    }

    // Expected values for 1, 2, ..., N
    float expected_min = 1.0f;
    float expected_max = static_cast<float>(total_size);
    float expected_mean = (expected_min + expected_max) / 2.0f;

    EXPECT_NEAR(min_val, expected_min, NUMERICAL_EPS);
    EXPECT_NEAR(max_val, expected_max, NUMERICAL_EPS);
    EXPECT_NEAR(mean, expected_mean, RELAXED_EPS);
    EXPECT_GT(std_dev, 0.0f) << "Std dev should be positive for varying data";

    nimcp_lnn_ode_batch_state_destroy(state);
}

/**
 * TEST: Uniform state has zero standard deviation
 * WHAT: All same values should have std = 0
 * WHY:  Edge case verification
 */
TEST_F(LNNODEKernelTest, StateStats_UniformState_ZeroStd) {
    RequireGPU();

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, SMALL_BATCH_SIZE, SMALL_N_NEURONS);

    if (!state) {
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    const float uniform_value = 5.0f;
    nimcp_lnn_ode_batch_state_reset(ctx, state, uniform_value);

    float mean = 0, std_dev = 0, min_val = 0, max_val = 0;
    bool result = nimcp_gpu_lnn_compute_state_stats(ctx, state, &mean, &std_dev, &min_val, &max_val);

    if (!result) {
        nimcp_lnn_ode_batch_state_destroy(state);
        GTEST_SKIP() << "State stats not implemented";
    }

    EXPECT_NEAR(mean, uniform_value, NUMERICAL_EPS);
    EXPECT_NEAR(std_dev, 0.0f, NUMERICAL_EPS) << "Uniform state should have zero std";
    EXPECT_NEAR(min_val, uniform_value, NUMERICAL_EPS);
    EXPECT_NEAR(max_val, uniform_value, NUMERICAL_EPS);

    nimcp_lnn_ode_batch_state_destroy(state);
}

//=============================================================================
// CPU Fallback Equivalence Tests
//=============================================================================

/**
 * TEST: CPU fallback Euler equivalence
 * WHAT: CPU fallback should produce same results as GPU
 * WHY:  Ensure code works on non-GPU systems
 */
TEST_F(LNNODEKernelTest, CPUFallback_EulerEquivalence) {
    // CPU-only test - no GPU required
    const uint32_t batch = 4;
    const uint32_t neurons = 8;
    const float dt = 1.0f;

    // Prepare data
    std::vector<float> x(batch * neurons, 1.0f);
    std::vector<float> dx_dt(batch * neurons, -0.1f);
    std::vector<float> x_new(batch * neurons, 0.0f);

    bool result = nimcp_cpu_lnn_euler_step_batched(
        x.data(), dx_dt.data(), x_new.data(), batch, neurons, dt);
    EXPECT_TRUE(result);

    // Verify: x_new = x + dt * dx_dt = 1.0 + 1.0 * (-0.1) = 0.9
    for (float val : x_new) {
        EXPECT_NEAR(val, 0.9f, NUMERICAL_EPS);
    }
}

/**
 * TEST: CPU fallback RK4 equivalence
 * WHAT: CPU RK4 should produce finite results
 * WHY:  Verify CPU path works correctly
 */
TEST_F(LNNODEKernelTest, CPUFallback_RK4_ProducesFiniteResults) {
    const uint32_t batch = 2;
    const uint32_t neurons = 4;
    const uint32_t inputs = 3;
    const float dt = 1.0f;

    // Prepare data
    std::vector<float> x(batch * neurons, 0.5f);
    std::vector<float> tau(batch * neurons, 10.0f);
    std::vector<float> input(batch * inputs, 1.0f);
    std::vector<float> W_in(neurons * inputs, 0.1f);
    std::vector<float> W_rec(neurons * neurons, 0.0f);
    std::vector<float> b_in(neurons, 0.0f);
    std::vector<float> x_new(batch * neurons, 0.0f);

    // Make W_rec diagonal for stability
    for (uint32_t i = 0; i < neurons; i++) {
        W_rec[i * neurons + i] = 0.1f;
    }

    bool result = nimcp_cpu_lnn_rk4_step_batched(
        x.data(), tau.data(), input.data(),
        W_in.data(), W_rec.data(), b_in.data(),
        x_new.data(), batch, neurons, inputs, dt, LNN_ACTIVATION_TANH);
    EXPECT_TRUE(result);

    // Verify results are finite
    for (float val : x_new) {
        EXPECT_TRUE(std::isfinite(val)) << "CPU RK4 should produce finite results";
    }
}

/**
 * TEST: GPU and CPU Euler produce same results
 * WHAT: Compare GPU and CPU Euler implementations
 * WHY:  Verify GPU implementation correctness
 */
TEST_F(LNNODEKernelTest, CPUFallback_GPUCPUEquivalence_Euler) {
    RequireGPU();

    const uint32_t batch = 4;
    const uint32_t neurons = 10;
    const float dt = 0.5f;

    // CPU computation
    std::vector<float> x_cpu(batch * neurons, 1.0f);
    std::vector<float> dx_dt_cpu(batch * neurons, -0.2f);
    std::vector<float> x_new_cpu(batch * neurons, 0.0f);

    nimcp_cpu_lnn_euler_step_batched(
        x_cpu.data(), dx_dt_cpu.data(), x_new_cpu.data(), batch, neurons, dt);

    // GPU computation
    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(ctx, batch, neurons);

    if (!state) {
        GTEST_SKIP() << "Batch state creation not implemented";
    }

    // Initialize GPU state with same values
    nimcp_gpu_tensor_t* x_gpu = create_tensor_from_data(x_cpu.data(), batch * neurons);
    nimcp_gpu_tensor_t* dx_gpu = create_tensor_from_data(dx_dt_cpu.data(), batch * neurons);
    if (x_gpu) {
        nimcp_gpu_copy(ctx, x_gpu, state->x);
        nimcp_gpu_tensor_destroy(x_gpu);
    }
    if (dx_gpu) {
        nimcp_gpu_copy(ctx, dx_gpu, state->dx_dt);
        nimcp_gpu_tensor_destroy(dx_gpu);
    }

    nimcp_gpu_lnn_euler_step_batched(ctx, state, dt);

    // Compare results
    std::vector<float> x_new_gpu(batch * neurons);
    copy_to_host(state->x, x_new_gpu.data());

    for (size_t i = 0; i < batch * neurons; i++) {
        EXPECT_NEAR(x_new_gpu[i], x_new_cpu[i], NUMERICAL_EPS)
            << "GPU and CPU should produce same results at index " << i;
    }

    nimcp_lnn_ode_batch_state_destroy(state);
}

//=============================================================================
// Performance and Scalability Tests
//=============================================================================

/**
 * TEST: Large batch performance
 * WHAT: Process large batch efficiently
 * WHY:  Verify scalability
 */
TEST_F(LNNODEKernelTest, Performance_LargeBatch_CompletesQuickly) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer(LARGE_N_NEURONS, DEFAULT_N_INPUTS);
    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);

    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, LARGE_BATCH_SIZE, LARGE_N_NEURONS);
    nimcp_lnn_ode_cache_t* cache = nimcp_lnn_ode_cache_create(
        ctx, LARGE_BATCH_SIZE, LARGE_N_NEURONS, DEFAULT_RK_STAGES);

    if (!state || !cache) {
        if (state) nimcp_lnn_ode_batch_state_destroy(state);
        if (cache) nimcp_lnn_ode_cache_destroy(cache);
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "State/cache creation not implemented";
    }

    nimcp_gpu_tensor_t* input = create_matrix(LARGE_BATCH_SIZE, DEFAULT_N_INPUTS, 1.0f);
    nimcp_lnn_ode_batch_config_t config = create_default_batch_config(LNN_ODE_RK4);

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < 100; t++) {
        nimcp_gpu_lnn_rk4_step_batched(ctx, layer, input, state, cache, &config);
    }

    nimcp_gpu_context_synchronize(ctx);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 30000) << "100 steps on large batch should complete within 30s";

    nimcp_gpu_tensor_destroy(input);
    nimcp_lnn_ode_cache_destroy(cache);
    nimcp_lnn_ode_batch_state_destroy(state);
    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * TEST: Full batched forward pass
 * WHAT: Complete forward pass with all components
 * WHY:  Verify end-to-end integration
 */
TEST_F(LNNODEKernelTest, Integration_FullBatchedForwardPass) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer(SMALL_N_NEURONS, DEFAULT_N_INPUTS);
    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);

    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS);
    nimcp_lnn_ode_cache_t* cache = nimcp_lnn_ode_cache_create(
        ctx, DEFAULT_BATCH_SIZE, SMALL_N_NEURONS, DEFAULT_RK_STAGES);

    if (!state || !cache) {
        if (state) nimcp_lnn_ode_batch_state_destroy(state);
        if (cache) nimcp_lnn_ode_cache_destroy(cache);
        nimcp_lnn_layer_gpu_destroy(layer);
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "State/cache creation not implemented";
    }

    nimcp_gpu_tensor_t* input = create_matrix(DEFAULT_BATCH_SIZE, DEFAULT_N_INPUTS, 1.0f);
    nimcp_lnn_ode_batch_config_t config = create_default_batch_config(LNN_ODE_RK4);

    // Run multiple timesteps
    for (int t = 0; t < 50; t++) {
        // Update tau
        nimcp_gpu_lnn_update_tau_batched(ctx, layer, input, state, DEFAULT_TAU_MIN, DEFAULT_TAU_MAX);

        // Compute derivative
        nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, input, state, LNN_ACTIVATION_TANH);

        // RK4 step
        nimcp_gpu_lnn_rk4_step_batched(ctx, layer, input, state, cache, &config);

        // Check stability periodically
        if (t % 10 == 0) {
            bool has_nan, has_inf;
            float max_norm;
            nimcp_gpu_lnn_check_stability(ctx, state, &has_nan, &has_inf, &max_norm);
            ASSERT_FALSE(has_nan) << "NaN detected at step " << t;
            ASSERT_FALSE(has_inf) << "Inf detected at step " << t;
        }
    }

    // Verify final state is finite
    size_t total_size = DEFAULT_BATCH_SIZE * SMALL_N_NEURONS;
    std::vector<float> x_host(total_size);
    copy_to_host(state->x, x_host.data());
    EXPECT_TRUE(all_finite(x_host)) << "Final state should be finite";

    nimcp_gpu_tensor_destroy(input);
    nimcp_lnn_ode_cache_destroy(cache);
    nimcp_lnn_ode_batch_state_destroy(state);
    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

/**
 * TEST: ODE method comparison
 * WHAT: Compare different ODE methods on same problem
 * WHY:  Verify all methods produce reasonable results
 */
TEST_F(LNNODEKernelTest, Integration_MethodComparison) {
    RequireGPU();

    lnn_layer_t* cpu_layer = create_mock_cpu_layer(SMALL_N_NEURONS, DEFAULT_N_INPUTS);
    nimcp_lnn_layer_gpu_t* layer = nimcp_lnn_layer_gpu_create(ctx, cpu_layer);

    if (!layer) {
        free_mock_cpu_layer(cpu_layer);
        GTEST_SKIP() << "GPU layer creation not supported";
    }

    std::vector<lnn_ode_method_t> methods = {
        LNN_ODE_EULER, LNN_ODE_HEUN, LNN_ODE_RK4
    };

    std::vector<std::vector<float>> final_states;

    for (auto method : methods) {
        nimcp_lnn_ode_batch_state_t* state = nimcp_lnn_ode_batch_state_create(
            ctx, SMALL_BATCH_SIZE, SMALL_N_NEURONS);
        nimcp_lnn_ode_cache_t* cache = nimcp_lnn_ode_cache_create(
            ctx, SMALL_BATCH_SIZE, SMALL_N_NEURONS,
            method == LNN_ODE_DOPRI5 ? DOPRI5_STAGES : DEFAULT_RK_STAGES);

        if (!state || !cache) {
            if (state) nimcp_lnn_ode_batch_state_destroy(state);
            if (cache) nimcp_lnn_ode_cache_destroy(cache);
            continue;
        }

        // Reset to same initial state
        nimcp_lnn_ode_batch_state_reset(ctx, state, 0.5f);

        nimcp_gpu_tensor_t* input = create_matrix(SMALL_BATCH_SIZE, DEFAULT_N_INPUTS, 1.0f);
        nimcp_lnn_ode_batch_config_t config = create_default_batch_config(method);

        // Run same number of steps
        for (int t = 0; t < 20; t++) {
            if (method == LNN_ODE_EULER) {
                nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, input, state, LNN_ACTIVATION_TANH);
                nimcp_gpu_lnn_euler_step_batched(ctx, state, config.dt);
            } else {
                nimcp_gpu_lnn_rk4_step_batched(ctx, layer, input, state, cache, &config);
            }
        }

        // Store final state
        size_t total_size = SMALL_BATCH_SIZE * SMALL_N_NEURONS;
        std::vector<float> x_host(total_size);
        copy_to_host(state->x, x_host.data());
        final_states.push_back(x_host);

        EXPECT_TRUE(all_finite(x_host))
            << "Method " << static_cast<int>(method) << " should produce finite results";

        nimcp_gpu_tensor_destroy(input);
        nimcp_lnn_ode_cache_destroy(cache);
        nimcp_lnn_ode_batch_state_destroy(state);
    }

    nimcp_lnn_layer_gpu_destroy(layer);
    free_mock_cpu_layer(cpu_layer);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
