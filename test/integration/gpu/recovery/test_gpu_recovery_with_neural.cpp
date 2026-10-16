/* ============================================================================
 * Integration Tests: GPU Recovery with Neural Module
 * ============================================================================
 * WHAT: Integration tests for GPU recovery with neural network operations
 * WHY:  Validate recovery works correctly with neural GPU kernels
 * HOW:  Test recovery scenarios specific to training, inference, backprop
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include <cuda_runtime.h>
#endif

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */
class GPURecoveryNeuralIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
        ctx_ = nimcp_gpu_recovery_context_create(NULL);
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_recovery_context_destroy(ctx_);
            ctx_ = nullptr;
        }
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx_ = nullptr;

    /* CPU fallback for forward pass */
    static bool forward_pass_cpu_fallback(void* context, void* params, void* result) {
        (void)context;
        if (!params || !result) return false;

        /* Simple linear layer CPU fallback */
        float* input = static_cast<float*>(params);
        float* output = static_cast<float*>(result);
        *output = *input * 0.5f + 0.1f;  /* y = wx + b simplified */
        return true;
    }
#endif
};

/* ============================================================================
 * Test: RecoverFromForwardPassOOM
 * Verify recovery strategy for forward pass OOM
 * Note: FREE_CACHE may "fail" if no cached memory is available.
 * ============================================================================ */
TEST_F(GPURecoveryNeuralIntegrationTest, RecoverFromForwardPassOOM) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, forward_pass_cpu_fallback, nullptr);

    /* Large batch forward pass can OOM */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);

    /* First OOM action should be FREE_CACHE */
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_FREE_CACHE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoverFromBackpropNumerical
 * Verify recovery from numerical errors in backpropagation
 * ============================================================================ */
TEST_F(GPURecoveryNeuralIntegrationTest, RecoverFromBackpropNumerical) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, forward_pass_cpu_fallback, nullptr);

    /* Gradient explosion/vanishing are numerical errors */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_NUMERICAL,
                                           cudaSuccess, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_REDUCE_PRECISION);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoverFromWeightUpdateKernelLaunch
 * Verify recovery from weight update kernel launch failure
 * ============================================================================ */
TEST_F(GPURecoveryNeuralIntegrationTest, RecoverFromWeightUpdateKernelLaunch) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, forward_pass_cpu_fallback, nullptr);

    /* Kernel launch failure during weight update */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_KERNEL_LAUNCH,
                                           cudaErrorLaunchFailure, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_STREAM_SYNC);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoverFromActivationOverflow
 * Verify recovery from activation function overflow
 * Note: Strategy selection is based on retry_count in context. Retry_count
 * only increments for RETRY_* actions, not for REDUCE_PRECISION.
 * ============================================================================ */
TEST_F(GPURecoveryNeuralIntegrationTest, RecoverFromActivationOverflow) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, forward_pass_cpu_fallback, nullptr);

    /* Activation overflow is a numerical error */
    nimcp_gpu_recovery_result_t result;

    /* First numerical error - at retry_count=0, returns REDUCE_PRECISION */
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_NUMERICAL, cudaSuccess, &result);
    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_REDUCE_PRECISION);

    /* Verify the action is consistent for same error type at same retry level */
    recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_NUMERICAL,
                                       cudaSuccess, &result);
    EXPECT_TRUE(recovered);
    /* Same action returned since retry_count doesn't increment for non-RETRY actions */
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_REDUCE_PRECISION);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CPUFallbackOnDeviceFailure
 * Verify CPU fallback for neural operations
 * ============================================================================ */
TEST_F(GPURecoveryNeuralIntegrationTest, CPUFallbackOnDeviceFailure) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, forward_pass_cpu_fallback, nullptr);

    /* Execute CPU fallback for forward pass */
    float input = 2.0f;
    float output = 0.0f;

    bool success = nimcp_gpu_execute_cpu_fallback(ctx_, &input, &output);
    EXPECT_TRUE(success);
    EXPECT_FLOAT_EQ(output, 1.1f);  /* 2.0 * 0.5 + 0.1 */
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RetryWithReducedPrecision
 * Verify precision reduction for numerical stability
 * ============================================================================ */
TEST_F(GPURecoveryNeuralIntegrationTest, RetryWithReducedPrecision) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    /* Numerical errors trigger precision reduction */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_NUMERICAL,
                                           cudaSuccess, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_REDUCE_PRECISION);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
