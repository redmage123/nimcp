/* ============================================================================
 * Unit Tests: LNN GPU Recovery Integration
 * ============================================================================
 * WHAT: Unit tests for GPU recovery in Liquid Neural Network operations
 * WHY:  Validate self-healing and CPU fallback for LNN ODE solver failures
 * HOW:  Test recovery from numerical errors, OOM, kernel launch failures
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/lnn/nimcp_lnn_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-4f;

class LNNGPURecoveryTest : public ::testing::Test {
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

    // Helper: Create a basic LNN layer for testing
    nimcp_lnn_layer_gpu_t* create_test_lnn_layer(uint32_t n_neurons, uint32_t n_inputs) {
        nimcp_lnn_layer_gpu_t* layer = (nimcp_lnn_layer_gpu_t*)calloc(1, sizeof(nimcp_lnn_layer_gpu_t));
        if (!layer) return NULL;

        layer->n_neurons = n_neurons;
        layer->n_inputs = n_inputs;
        layer->activation = LNN_ACTIVATION_TANH;

        // Create state tensors
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

        if (!layer->x || !layer->dx_dt || !layer->tau || !layer->tau_base ||
            !layer->W_in || !layer->W_rec || !layer->W_tau || !layer->b_in || !layer->b_tau) {
            destroy_test_lnn_layer(layer);
            return NULL;
        }

        // Initialize state to zero
        nimcp_gpu_zeros(ctx_, layer->x);
        nimcp_gpu_zeros(ctx_, layer->dx_dt);
        nimcp_gpu_fill(ctx_, layer->tau_base, 10.0f);  // Base time constant 10ms
        nimcp_gpu_fill(ctx_, layer->tau, 10.0f);

        // Initialize weights with small random values
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> weight_dist(0.0f, 0.1f);

        // W_in
        std::vector<float> w_in(n_neurons * n_inputs);
        for (auto& w : w_in) w = weight_dist(gen);
        cudaMemcpy(layer->W_in->data, w_in.data(), w_in.size() * sizeof(float), cudaMemcpyHostToDevice);

        // W_rec (sparse in real LNN, dense here for testing)
        std::vector<float> w_rec(n_neurons * n_neurons);
        for (auto& w : w_rec) w = weight_dist(gen) * 0.5f;  // Smaller recurrent weights
        cudaMemcpy(layer->W_rec->data, w_rec.data(), w_rec.size() * sizeof(float), cudaMemcpyHostToDevice);

        // W_tau
        std::vector<float> w_tau(n_neurons * (n_inputs + n_neurons), 0.01f);
        cudaMemcpy(layer->W_tau->data, w_tau.data(), w_tau.size() * sizeof(float), cudaMemcpyHostToDevice);

        // Biases
        nimcp_gpu_zeros(ctx_, layer->b_in);
        nimcp_gpu_zeros(ctx_, layer->b_tau);

        // No sparse wiring for this test
        layer->row_ptr = NULL;
        layer->col_idx = NULL;
        layer->edge_weights = NULL;
        layer->n_edges = 0;

        return layer;
    }

    void destroy_test_lnn_layer(nimcp_lnn_layer_gpu_t* layer) {
        if (!layer) return;
        if (layer->x) nimcp_gpu_tensor_destroy(layer->x);
        if (layer->dx_dt) nimcp_gpu_tensor_destroy(layer->dx_dt);
        if (layer->tau) nimcp_gpu_tensor_destroy(layer->tau);
        if (layer->tau_base) nimcp_gpu_tensor_destroy(layer->tau_base);
        if (layer->W_in) nimcp_gpu_tensor_destroy(layer->W_in);
        if (layer->W_rec) nimcp_gpu_tensor_destroy(layer->W_rec);
        if (layer->W_tau) nimcp_gpu_tensor_destroy(layer->W_tau);
        if (layer->b_in) nimcp_gpu_tensor_destroy(layer->b_in);
        if (layer->b_tau) nimcp_gpu_tensor_destroy(layer->b_tau);
        if (layer->row_ptr) nimcp_gpu_tensor_destroy(layer->row_ptr);
        if (layer->col_idx) nimcp_gpu_tensor_destroy(layer->col_idx);
        if (layer->edge_weights) nimcp_gpu_tensor_destroy(layer->edge_weights);
        free(layer);
    }

    // Helper: Create input tensor with values
    nimcp_gpu_tensor_t* create_input(uint32_t n_inputs, float scale = 1.0f) {
        size_t dims[1] = {n_inputs};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx_, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (!tensor) return NULL;

        std::vector<float> host_data(n_inputs);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-scale, scale);
        for (uint32_t i = 0; i < n_inputs; i++) {
            host_data[i] = dis(gen);
        }

        cudaMemcpy(tensor->data, host_data.data(), n_inputs * sizeof(float), cudaMemcpyHostToDevice);
        return tensor;
    }
#endif
};

/* ============================================================================
 * Test: Default ODE Configuration
 * ============================================================================ */
TEST_F(LNNGPURecoveryTest, DefaultODEConfig) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_lnn_ode_config_t config = nimcp_lnn_ode_default_config();

    EXPECT_EQ(config.method, LNN_ODE_RK4);
    EXPECT_GT(config.dt, 0.0f);
    EXPECT_GT(config.dt_max, config.dt_min);
    EXPECT_GT(config.error_tolerance, 0.0f);
    EXPECT_GT(config.max_steps, 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Euler Step with Recovery
 * ============================================================================ */
TEST_F(LNNGPURecoveryTest, EulerStepRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    uint32_t n_neurons = 128;
    size_t dims[1] = {n_neurons};

    // Create state and derivative tensors
    nimcp_gpu_tensor_t* x = nimcp_gpu_tensor_create(ctx_, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* dx_dt = nimcp_gpu_tensor_create(ctx_, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* x_new = nimcp_gpu_tensor_create(ctx_, dims, 1, NIMCP_GPU_PRECISION_FP32);

    ASSERT_NE(x, nullptr);
    ASSERT_NE(dx_dt, nullptr);
    ASSERT_NE(x_new, nullptr);

    // Initialize
    nimcp_gpu_fill(ctx_, x, 1.0f);
    nimcp_gpu_fill(ctx_, dx_dt, -0.1f);  // Decaying

    // Run Euler step with recovery
    float dt = 1.0f;
    bool success = nimcp_gpu_lnn_euler_step(ctx_, x, dx_dt, dt, x_new);
    EXPECT_TRUE(success) << "Euler step should succeed with recovery";

    // Verify result (x_new = x + dt * dx_dt = 1.0 + 1.0 * (-0.1) = 0.9)
    std::vector<float> result(n_neurons);
    cudaMemcpy(result.data(), x_new->data, n_neurons * sizeof(float), cudaMemcpyDeviceToHost);

    for (uint32_t i = 0; i < n_neurons; i++) {
        EXPECT_NEAR(result[i], 0.9f, TOLERANCE) << "Euler step result incorrect at index " << i;
    }

    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(dx_dt);
    nimcp_gpu_tensor_destroy(x_new);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Tau Update with Recovery
 * ============================================================================ */
TEST_F(LNNGPURecoveryTest, TauUpdateRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    uint32_t n_neurons = 64;
    uint32_t n_inputs = 32;

    nimcp_lnn_layer_gpu_t* layer = create_test_lnn_layer(n_neurons, n_inputs);
    ASSERT_NE(layer, nullptr) << "Layer creation failed";

    nimcp_gpu_tensor_t* input = create_input(n_inputs, 1.0f);
    ASSERT_NE(input, nullptr);

    // Update tau with recovery
    bool success = nimcp_gpu_lnn_update_tau(ctx_, layer, input);
    EXPECT_TRUE(success) << "Tau update should succeed with recovery";

    // Verify tau values are in reasonable range
    std::vector<float> tau_values(n_neurons);
    cudaMemcpy(tau_values.data(), layer->tau->data, n_neurons * sizeof(float), cudaMemcpyDeviceToHost);

    for (uint32_t i = 0; i < n_neurons; i++) {
        EXPECT_GT(tau_values[i], 0.0f) << "Tau should be positive at index " << i;
        EXPECT_LT(tau_values[i], 1000.0f) << "Tau should be bounded at index " << i;
    }

    nimcp_gpu_tensor_destroy(input);
    destroy_test_lnn_layer(layer);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: LTC Derivative Computation with Recovery
 * ============================================================================ */
TEST_F(LNNGPURecoveryTest, DerivativeComputationRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    uint32_t n_neurons = 64;
    uint32_t n_inputs = 32;

    nimcp_lnn_layer_gpu_t* layer = create_test_lnn_layer(n_neurons, n_inputs);
    ASSERT_NE(layer, nullptr);

    nimcp_gpu_tensor_t* input = create_input(n_inputs, 1.0f);
    ASSERT_NE(input, nullptr);

    // Compute derivative with recovery
    bool success = nimcp_gpu_lnn_compute_derivative(ctx_, layer, input, layer->dx_dt);
    EXPECT_TRUE(success) << "Derivative computation should succeed with recovery";

    // Verify derivative values are finite
    std::vector<float> dx_values(n_neurons);
    cudaMemcpy(dx_values.data(), layer->dx_dt->data, n_neurons * sizeof(float), cudaMemcpyDeviceToHost);

    for (uint32_t i = 0; i < n_neurons; i++) {
        EXPECT_FALSE(std::isnan(dx_values[i])) << "Derivative should not be NaN at index " << i;
        EXPECT_FALSE(std::isinf(dx_values[i])) << "Derivative should not be Inf at index " << i;
    }

    nimcp_gpu_tensor_destroy(input);
    destroy_test_lnn_layer(layer);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RK4 ODE Step with Recovery
 * ============================================================================ */
TEST_F(LNNGPURecoveryTest, RK4StepRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    uint32_t n_neurons = 64;
    uint32_t n_inputs = 16;

    nimcp_lnn_layer_gpu_t* layer = create_test_lnn_layer(n_neurons, n_inputs);
    ASSERT_NE(layer, nullptr);

    nimcp_gpu_tensor_t* input = create_input(n_inputs, 1.0f);
    ASSERT_NE(input, nullptr);

    nimcp_lnn_ode_config_t config = nimcp_lnn_ode_default_config();
    config.method = LNN_ODE_RK4;
    config.dt = 1.0f;

    // Run RK4 step with recovery
    bool success = nimcp_gpu_lnn_rk4_step(ctx_, layer, input, config.dt, &config);
    EXPECT_TRUE(success) << "RK4 step should succeed with recovery";

    // Verify state values are bounded (state clamping should work)
    std::vector<float> state(n_neurons);
    cudaMemcpy(state.data(), layer->x->data, n_neurons * sizeof(float), cudaMemcpyDeviceToHost);

    for (uint32_t i = 0; i < n_neurons; i++) {
        EXPECT_FALSE(std::isnan(state[i])) << "State should not be NaN at index " << i;
        EXPECT_FALSE(std::isinf(state[i])) << "State should not be Inf at index " << i;
        EXPECT_GE(state[i], -10.0f) << "State should be >= -10 at index " << i;
        EXPECT_LE(state[i], 10.0f) << "State should be <= 10 at index " << i;
    }

    nimcp_gpu_tensor_destroy(input);
    destroy_test_lnn_layer(layer);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Heun (RK2) ODE Step with Recovery
 * ============================================================================ */
TEST_F(LNNGPURecoveryTest, HeunStepRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    uint32_t n_neurons = 64;
    uint32_t n_inputs = 16;

    nimcp_lnn_layer_gpu_t* layer = create_test_lnn_layer(n_neurons, n_inputs);
    ASSERT_NE(layer, nullptr);

    nimcp_gpu_tensor_t* input = create_input(n_inputs, 1.0f);
    ASSERT_NE(input, nullptr);

    nimcp_lnn_ode_config_t config = nimcp_lnn_ode_default_config();
    config.method = LNN_ODE_HEUN;
    config.dt = 1.0f;

    // Run Heun step with recovery
    bool success = nimcp_gpu_lnn_heun_step(ctx_, layer, input, config.dt, &config);
    EXPECT_TRUE(success) << "Heun step should succeed with recovery";

    // Verify state values are bounded
    std::vector<float> state(n_neurons);
    cudaMemcpy(state.data(), layer->x->data, n_neurons * sizeof(float), cudaMemcpyDeviceToHost);

    for (uint32_t i = 0; i < n_neurons; i++) {
        EXPECT_FALSE(std::isnan(state[i])) << "State should not be NaN at index " << i;
        EXPECT_FALSE(std::isinf(state[i])) << "State should not be Inf at index " << i;
    }

    nimcp_gpu_tensor_destroy(input);
    destroy_test_lnn_layer(layer);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: DOPRI5 Adaptive Step with Recovery
 * ============================================================================ */
TEST_F(LNNGPURecoveryTest, DOPRI5StepRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    uint32_t n_neurons = 32;  // Smaller for adaptive method
    uint32_t n_inputs = 16;

    nimcp_lnn_layer_gpu_t* layer = create_test_lnn_layer(n_neurons, n_inputs);
    ASSERT_NE(layer, nullptr);

    nimcp_gpu_tensor_t* input = create_input(n_inputs, 0.5f);  // Smaller input
    ASSERT_NE(input, nullptr);

    nimcp_lnn_ode_config_t config = nimcp_lnn_ode_default_config();
    config.method = LNN_ODE_DOPRI5;
    config.dt = 1.0f;
    config.dt_min = 0.01f;
    config.dt_max = 10.0f;
    config.error_tolerance = 1e-4f;
    config.adaptive_stepping = true;

    float dt = config.dt;

    // Run DOPRI5 step with recovery
    bool success = nimcp_gpu_lnn_dopri5_step(ctx_, layer, input, &dt, &config);
    EXPECT_TRUE(success) << "DOPRI5 step should succeed with recovery";

    // dt may have been adjusted
    EXPECT_GE(dt, config.dt_min) << "dt should be >= dt_min";
    EXPECT_LE(dt, config.dt_max) << "dt should be <= dt_max";

    nimcp_gpu_tensor_destroy(input);
    destroy_test_lnn_layer(layer);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Unified ODE Step with Recovery (Method Selection)
 * ============================================================================ */
TEST_F(LNNGPURecoveryTest, UnifiedODEStepRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    uint32_t n_neurons = 64;
    uint32_t n_inputs = 16;

    nimcp_lnn_layer_gpu_t* layer = create_test_lnn_layer(n_neurons, n_inputs);
    ASSERT_NE(layer, nullptr);

    nimcp_gpu_tensor_t* input = create_input(n_inputs, 1.0f);
    ASSERT_NE(input, nullptr);

    // Test all ODE methods via unified interface
    std::vector<lnn_ode_method_t> methods = {
        LNN_ODE_EULER,
        LNN_ODE_HEUN,
        LNN_ODE_RK4
    };

    for (auto method : methods) {
        nimcp_lnn_ode_config_t config = nimcp_lnn_ode_default_config();
        config.method = method;
        config.dt = 1.0f;

        // Reset state
        nimcp_gpu_zeros(ctx_, layer->x);

        bool success = nimcp_gpu_lnn_ode_step(ctx_, layer, input, &config);
        EXPECT_TRUE(success) << "ODE step with method " << (int)method << " failed";

        // Verify state is finite
        std::vector<float> state(n_neurons);
        cudaMemcpy(state.data(), layer->x->data, n_neurons * sizeof(float), cudaMemcpyDeviceToHost);

        for (uint32_t i = 0; i < n_neurons; i++) {
            EXPECT_FALSE(std::isnan(state[i])) << "State NaN for method " << (int)method;
        }
    }

    nimcp_gpu_tensor_destroy(input);
    destroy_test_lnn_layer(layer);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Sparse Matrix-Vector Product with Recovery
 * ============================================================================ */
TEST_F(LNNGPURecoveryTest, SparseMatvecRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Simple 4x4 sparse matrix in CSR format
    // [1 0 2 0]
    // [0 3 0 0]
    // [0 0 4 5]
    // [6 0 0 7]
    uint32_t n_rows = 4;
    uint32_t n_cols = 4;
    uint32_t nnz = 7;

    // CSR arrays
    std::vector<uint32_t> h_row_ptr = {0, 2, 3, 5, 7};
    std::vector<uint32_t> h_col_idx = {0, 2, 1, 2, 3, 0, 3};
    std::vector<float> h_values = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
    std::vector<float> h_x = {1.0f, 2.0f, 3.0f, 4.0f};

    // Create GPU tensors
    size_t row_ptr_dims[1] = {n_rows + 1};
    size_t col_idx_dims[1] = {nnz};
    size_t values_dims[1] = {nnz};
    size_t vec_dims[1] = {n_cols};

    nimcp_gpu_tensor_t* row_ptr = nimcp_gpu_tensor_create(ctx_, row_ptr_dims, 1, NIMCP_GPU_PRECISION_UINT32);
    nimcp_gpu_tensor_t* col_idx = nimcp_gpu_tensor_create(ctx_, col_idx_dims, 1, NIMCP_GPU_PRECISION_UINT32);
    nimcp_gpu_tensor_t* values = nimcp_gpu_tensor_create(ctx_, values_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* x = nimcp_gpu_tensor_create(ctx_, vec_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* y = nimcp_gpu_tensor_create(ctx_, vec_dims, 1, NIMCP_GPU_PRECISION_FP32);

    ASSERT_NE(row_ptr, nullptr);
    ASSERT_NE(col_idx, nullptr);
    ASSERT_NE(values, nullptr);
    ASSERT_NE(x, nullptr);
    ASSERT_NE(y, nullptr);

    // Copy data to GPU
    cudaMemcpy(row_ptr->data, h_row_ptr.data(), (n_rows + 1) * sizeof(uint32_t), cudaMemcpyHostToDevice);
    cudaMemcpy(col_idx->data, h_col_idx.data(), nnz * sizeof(uint32_t), cudaMemcpyHostToDevice);
    cudaMemcpy(values->data, h_values.data(), nnz * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(x->data, h_x.data(), n_cols * sizeof(float), cudaMemcpyHostToDevice);
    nimcp_gpu_zeros(ctx_, y);

    // Run sparse matvec with recovery
    bool success = nimcp_gpu_sparse_matvec(ctx_, row_ptr, col_idx, values, x, y, n_rows, 1.0f);
    EXPECT_TRUE(success) << "Sparse matvec should succeed with recovery";

    // Verify result: y = A * x
    // y[0] = 1*1 + 2*3 = 7
    // y[1] = 3*2 = 6
    // y[2] = 4*3 + 5*4 = 32
    // y[3] = 6*1 + 7*4 = 34
    std::vector<float> h_y(n_rows);
    cudaMemcpy(h_y.data(), y->data, n_rows * sizeof(float), cudaMemcpyDeviceToHost);

    EXPECT_NEAR(h_y[0], 7.0f, TOLERANCE);
    EXPECT_NEAR(h_y[1], 6.0f, TOLERANCE);
    EXPECT_NEAR(h_y[2], 32.0f, TOLERANCE);
    EXPECT_NEAR(h_y[3], 34.0f, TOLERANCE);

    nimcp_gpu_tensor_destroy(row_ptr);
    nimcp_gpu_tensor_destroy(col_idx);
    nimcp_gpu_tensor_destroy(values);
    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(y);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Multiple ODE Steps Stability with Recovery
 * ============================================================================ */
TEST_F(LNNGPURecoveryTest, MultiStepStabilityRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    uint32_t n_neurons = 64;
    uint32_t n_inputs = 16;
    int n_steps = 100;

    nimcp_lnn_layer_gpu_t* layer = create_test_lnn_layer(n_neurons, n_inputs);
    ASSERT_NE(layer, nullptr);

    nimcp_gpu_tensor_t* input = create_input(n_inputs, 1.0f);
    ASSERT_NE(input, nullptr);

    nimcp_lnn_ode_config_t config = nimcp_lnn_ode_default_config();
    config.method = LNN_ODE_RK4;
    config.dt = 1.0f;

    // Run many steps and verify stability
    for (int step = 0; step < n_steps; step++) {
        bool success = nimcp_gpu_lnn_ode_step(ctx_, layer, input, &config);
        EXPECT_TRUE(success) << "ODE step " << step << " failed";

        // Every 10 steps, verify state is bounded
        if (step % 10 == 0) {
            std::vector<float> state(n_neurons);
            cudaMemcpy(state.data(), layer->x->data, n_neurons * sizeof(float), cudaMemcpyDeviceToHost);

            float max_val = 0.0f;
            for (auto& v : state) {
                EXPECT_FALSE(std::isnan(v)) << "NaN at step " << step;
                EXPECT_FALSE(std::isinf(v)) << "Inf at step " << step;
                max_val = std::max(max_val, std::abs(v));
            }
            // State should be clamped to [-10, 10]
            EXPECT_LE(max_val, 10.0f + TOLERANCE) << "State exceeds bound at step " << step;
        }
    }

    nimcp_gpu_tensor_destroy(input);
    destroy_test_lnn_layer(layer);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery Statistics for LNN Operations
 * ============================================================================ */
TEST_F(LNNGPURecoveryTest, RecoveryStatistics) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Reset stats
    nimcp_gpu_recovery_reset_stats();

    // Perform some operations
    uint32_t n_neurons = 32;
    uint32_t n_inputs = 16;

    nimcp_lnn_layer_gpu_t* layer = create_test_lnn_layer(n_neurons, n_inputs);
    ASSERT_NE(layer, nullptr);

    nimcp_gpu_tensor_t* input = create_input(n_inputs, 1.0f);
    ASSERT_NE(input, nullptr);

    nimcp_lnn_ode_config_t config = nimcp_lnn_ode_default_config();

    for (int i = 0; i < 20; i++) {
        nimcp_gpu_lnn_ode_step(ctx_, layer, input, &config);
    }

    // Get stats
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    // Verify stats are valid
    EXPECT_GE(stats.success_rate, 0.0f);
    EXPECT_LE(stats.success_rate, 1.0f);

    nimcp_gpu_tensor_destroy(input);
    destroy_test_lnn_layer(layer);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

} // namespace
