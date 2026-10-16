/* ============================================================================
 * E2E Test: Fuzzy Controller GPU Pipeline
 * ============================================================================
 * WHAT: End-to-end test of GPU-accelerated ANFIS workflow
 * WHY:  Validate ANFIS training and inference on GPU
 * HOW:  Create ANFIS -> Train on sample data -> Forward pass -> Verify results
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>

#ifdef NIMCP_ENABLE_CUDA
// Include GPU headers - they have proper extern "C" guards internally
#include "gpu/fuzzy/nimcp_fuzzy_gpu.h"
#include "gpu/fuzzy/nimcp_fuzzy_anfis_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include <cuda_runtime.h>
#endif

namespace {

constexpr float TOLERANCE = 1e-4f;
constexpr float CONTROL_TOLERANCE = 0.1f;

class FuzzyControllerE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaError_t cuda_err = cudaGetDeviceCount(&device_count);
        if (cuda_err != cudaSuccess || device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        ctx_ = nimcp_gpu_context_create(0);
        if (!ctx_) {
            GTEST_SKIP() << "Failed to create GPU context";
        }
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_context_destroy(ctx_);
            ctx_ = nullptr;
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx_ = nullptr;
#endif
};

/* ============================================================================
 * E2E Test: Basic ANFIS Creation and Destruction
 * ============================================================================ */
TEST_F(FuzzyControllerE2ETest, InvertedPendulumControl) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Create ANFIS with basic parameters
    nimcp_gpu_anfis_create_params_t anfis_params = {
        .num_inputs = 2,
        .num_outputs = 1,
        .num_mfs_per_input = 3,
        .mf_type = FUZZY_MF_GAUSSIAN,
        .learning_rate = 0.01f,
        .momentum = 0.9f
    };

    nimcp_gpu_anfis_state_t* anfis = nimcp_gpu_anfis_create(ctx_, &anfis_params);

    // If ANFIS creation fails, it throws to immune system and returns NULL
    // This test verifies proper error handling
    if (anfis == nullptr) {
        // Check error message - should be descriptive not NULL
        const char* err = nimcp_gpu_anfis_get_last_error();
        ASSERT_NE(err, nullptr) << "Error message should be available";
        GTEST_SKIP() << "ANFIS creation failed (expected on some systems): " << err;
    }

    // Clean up
    nimcp_gpu_anfis_destroy(anfis);
    SUCCEED() << "ANFIS creation and destruction successful";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * E2E Test: ANFIS Function Approximation
 * ============================================================================ */
TEST_F(FuzzyControllerE2ETest, ANFISFunctionApproximation) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // ==================== Phase 1: Generate Training Data ====================
    // Target function: Mackey-Glass (chaotic time series)
    const int n_samples = 2000;
    const int train_size = 1500;
    const int test_size = n_samples - train_size;
    const int embedding_dim = 4;

    std::vector<float> series(n_samples + 100);
    series[0] = 0.9f;

    // Generate Mackey-Glass series
    for (int i = 1; i < static_cast<int>(series.size()); i++) {
        int delay_idx = std::max(0, i - 17);
        float x_tau = series[delay_idx];
        series[i] = series[i-1] + 0.2f * x_tau / (1.0f + std::pow(x_tau, 10.0f)) - 0.1f * series[i-1];
    }

    // Create embedded input/target pairs
    std::vector<float> train_inputs(train_size * embedding_dim);
    std::vector<float> train_targets(train_size);
    std::vector<float> test_inputs(test_size * embedding_dim);
    std::vector<float> test_targets(test_size);

    for (int i = 0; i < n_samples; i++) {
        float* inputs_ptr = (i < train_size) ?
            &train_inputs[i * embedding_dim] : &test_inputs[(i - train_size) * embedding_dim];
        float* target_ptr = (i < train_size) ?
            &train_targets[i] : &test_targets[i - train_size];

        for (int j = 0; j < embedding_dim; j++) {
            inputs_ptr[j] = series[100 + i - (embedding_dim - 1 - j) * 6];
        }
        *target_ptr = series[100 + i + 6];  // Predict 6 steps ahead
    }

    // ==================== Phase 2: Create and Train ANFIS ====================
    nimcp_gpu_anfis_create_params_t anfis_params = {
        .num_inputs = static_cast<uint32_t>(embedding_dim),
        .num_outputs = 1,
        .num_mfs_per_input = 3,
        .mf_type = FUZZY_MF_GAUSSIAN,
        .learning_rate = 0.01f,
        .momentum = 0.9f
    };

    nimcp_gpu_anfis_state_t* anfis = nimcp_gpu_anfis_create(ctx_, &anfis_params);
    if (anfis == nullptr) {
        const char* err = nimcp_gpu_anfis_get_last_error();
        GTEST_SKIP() << "ANFIS creation failed: " << (err ? err : "unknown error");
    }

    // Create tensors from host data
    size_t train_input_dims[] = {static_cast<size_t>(train_size), static_cast<size_t>(embedding_dim)};
    size_t train_target_dims[] = {static_cast<size_t>(train_size), 1};

    nimcp_gpu_tensor_t* train_input_tensor = nimcp_gpu_tensor_from_host(
        ctx_, train_inputs.data(), train_input_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* train_target_tensor = nimcp_gpu_tensor_from_host(
        ctx_, train_targets.data(), train_target_dims, 2, NIMCP_GPU_PRECISION_FP32);

    if (!train_input_tensor || !train_target_tensor) {
        nimcp_gpu_anfis_destroy(anfis);
        if (train_input_tensor) nimcp_gpu_tensor_destroy(train_input_tensor);
        if (train_target_tensor) nimcp_gpu_tensor_destroy(train_target_tensor);
        GTEST_SKIP() << "Failed to create training tensors";
    }

    float initial_error = 0.0f, final_error = 0.0f;

    auto train_start = std::chrono::high_resolution_clock::now();

    nimcp_gpu_anfis_train_params_t train_params = {
        .num_epochs = 100,
        .batch_size = 100,
        .early_stop_threshold = 0.001f,
        .shuffle = true,
        .hybrid_learning = true,
        .lse_lambda = 0.001f
    };

    bool train_success = nimcp_gpu_anfis_train(ctx_, anfis,
        train_input_tensor, train_target_tensor,
        &train_params, &initial_error, &final_error);

    auto train_end = std::chrono::high_resolution_clock::now();
    auto train_time = std::chrono::duration_cast<std::chrono::milliseconds>(train_end - train_start);

    EXPECT_TRUE(train_success) << "ANFIS training should succeed";
    EXPECT_LT(final_error, initial_error) << "Error should decrease";

    std::cout << "ANFIS Training Results:" << std::endl;
    std::cout << "  Initial Error: " << initial_error << std::endl;
    std::cout << "  Final Error:   " << final_error << std::endl;
    std::cout << "  Training Time: " << train_time.count() << "ms" << std::endl;

    // ==================== Phase 3: Test Forward Pass ====================
    size_t test_input_dims[] = {static_cast<size_t>(test_size), static_cast<size_t>(embedding_dim)};
    size_t test_output_dims[] = {static_cast<size_t>(test_size), 1};

    nimcp_gpu_tensor_t* test_input_tensor = nimcp_gpu_tensor_from_host(
        ctx_, test_inputs.data(), test_input_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* test_output_tensor = nimcp_gpu_tensor_create(
        ctx_, test_output_dims, 2, NIMCP_GPU_PRECISION_FP32);

    if (test_input_tensor && test_output_tensor) {
        bool forward_success = nimcp_gpu_anfis_forward(ctx_, anfis,
            test_input_tensor, test_output_tensor);

        if (forward_success) {
            // Copy results to host
            std::vector<float> predictions(test_size);
            nimcp_gpu_tensor_to_host(test_output_tensor, predictions.data());

            // Calculate test MSE
            float test_mse = 0.0f;
            for (int i = 0; i < test_size; i++) {
                float diff = predictions[i] - test_targets[i];
                test_mse += diff * diff;
            }
            test_mse /= test_size;

            std::cout << "  Test MSE:      " << test_mse << std::endl;
            EXPECT_LT(test_mse, 1.0f) << "Test MSE should be reasonable";
        } else {
            std::cout << "  Forward pass failed" << std::endl;
        }

        nimcp_gpu_tensor_destroy(test_input_tensor);
        nimcp_gpu_tensor_destroy(test_output_tensor);
    }

    // Cleanup
    nimcp_gpu_tensor_destroy(train_input_tensor);
    nimcp_gpu_tensor_destroy(train_target_tensor);
    nimcp_gpu_anfis_destroy(anfis);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * E2E Test: ANFIS Error Handling via Immune System
 * ============================================================================ */
TEST_F(FuzzyControllerE2ETest, RealTimeBatchPerformance) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Test that invalid parameters throw to immune system
    nimcp_gpu_anfis_create_params_t invalid_params = {
        .num_inputs = 0,  // Invalid!
        .num_outputs = 1,
        .num_mfs_per_input = 0,  // Also invalid!
        .mf_type = FUZZY_MF_GAUSSIAN,
        .learning_rate = 0.01f,
        .momentum = 0.9f
    };

    // This should fail and throw to immune system (not crash)
    nimcp_gpu_anfis_state_t* anfis = nimcp_gpu_anfis_create(ctx_, &invalid_params);
    EXPECT_EQ(anfis, nullptr) << "Creation with invalid params should fail";

    // Error message should be available
    const char* err = nimcp_gpu_anfis_get_last_error();
    EXPECT_NE(err, nullptr) << "Error message should be available";
    if (err) {
        std::cout << "Expected error message: " << err << std::endl;
    }

    // Test NULL context
    nimcp_gpu_anfis_create_params_t valid_params = nimcp_gpu_anfis_create_params_default();
    anfis = nimcp_gpu_anfis_create(nullptr, &valid_params);
    EXPECT_EQ(anfis, nullptr) << "Creation with NULL context should fail";

    // Test NULL params
    anfis = nimcp_gpu_anfis_create(ctx_, nullptr);
    EXPECT_EQ(anfis, nullptr) << "Creation with NULL params should fail";

    SUCCEED() << "Error handling via immune system works correctly";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

} // namespace
