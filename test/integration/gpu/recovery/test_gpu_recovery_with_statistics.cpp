/* ============================================================================
 * Integration Tests: GPU Recovery with Statistics Module
 * ============================================================================
 * WHAT: Integration tests for GPU recovery with statistical computations
 * WHY:  Validate recovery works correctly with statistics GPU kernels
 * HOW:  Test recovery scenarios specific to covariance, PCA, distributions
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
class GPURecoveryStatisticsIntegrationTest : public ::testing::Test {
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

    /* CPU fallback for covariance computation */
    static bool covariance_cpu_fallback(void* context, void* params, void* result) {
        (void)context;
        if (!params || !result) return false;

        /* Simple CPU covariance placeholder */
        float* data = static_cast<float*>(params);
        float* cov = static_cast<float*>(result);
        *cov = data[0] * data[1];  /* Simplified */
        return true;
    }
#endif
};

/* ============================================================================
 * Test: RecoverFromCovarianceOOM
 * Verify recovery strategy for large covariance matrix OOM
 * Note: FREE_CACHE may "fail" if no cached memory is available.
 * ============================================================================ */
TEST_F(GPURecoveryStatisticsIntegrationTest, RecoverFromCovarianceOOM) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, covariance_cpu_fallback, nullptr);

    /* Large covariance matrices can cause OOM */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);

    /* First OOM action should be FREE_CACHE */
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_FREE_CACHE);
    /* result.success depends on whether memory was actually freed */
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoverFromPCANumerical
 * Verify recovery from numerical errors in PCA computation
 * ============================================================================ */
TEST_F(GPURecoveryStatisticsIntegrationTest, RecoverFromPCANumerical) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, covariance_cpu_fallback, nullptr);

    /* PCA eigenvalue decomposition can have numerical issues */
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
 * Test: RecoverFromDistributionKernelLaunch
 * Verify recovery from distribution sampling kernel launch failure
 * ============================================================================ */
TEST_F(GPURecoveryStatisticsIntegrationTest, RecoverFromDistributionKernelLaunch) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, covariance_cpu_fallback, nullptr);

    /* Kernel launch failure in distribution sampling */
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
 * Test: RecoverFromTimeSeriesOOM
 * Verify OOM recovery strategy progression for time series
 * ============================================================================ */
TEST_F(GPURecoveryStatisticsIntegrationTest, RecoverFromTimeSeriesOOM) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    /* Verify OOM recovery strategy progression */
    nimcp_gpu_recovery_action_t action0 = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 0);
    EXPECT_EQ(action0, GPU_RECOVERY_FREE_CACHE) << "First OOM action";

    nimcp_gpu_recovery_action_t action1 = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 1);
    EXPECT_EQ(action1, GPU_RECOVERY_REDUCE_BATCH) << "Second OOM action";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CPUFallbackOnDeviceFailure
 * Verify CPU fallback for statistics computations
 * ============================================================================ */
TEST_F(GPURecoveryStatisticsIntegrationTest, CPUFallbackOnDeviceFailure) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, covariance_cpu_fallback, nullptr);

    /* Device unavailable */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_DEVICE_NOT_AVAILABLE,
                                           cudaErrorNoDevice, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_CPU_FALLBACK);
    EXPECT_TRUE(ctx_->cpu_fallback_active);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RetryWithReducedDimensions
 * Verify dimension reduction strategy in OOM progression
 * ============================================================================ */
TEST_F(GPURecoveryStatisticsIntegrationTest, RetryWithReducedDimensions) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    /* Verify the third OOM strategy is REDUCE_DIMENSIONS */
    nimcp_gpu_recovery_action_t action2 = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 2);
    EXPECT_EQ(action2, GPU_RECOVERY_REDUCE_DIMENSIONS);

    /* Verify executing REDUCE_DIMENSIONS action succeeds */
    bool success = nimcp_gpu_execute_recovery_action(ctx_, GPU_RECOVERY_REDUCE_DIMENSIONS);
    EXPECT_TRUE(success);

    /* Fourth OOM (retry_count=3+) should fall back to CPU */
    nimcp_gpu_recovery_action_t action3 = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 3);
    EXPECT_EQ(action3, GPU_RECOVERY_CPU_FALLBACK);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
