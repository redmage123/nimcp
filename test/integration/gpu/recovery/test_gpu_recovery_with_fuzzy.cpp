/* ============================================================================
 * Integration Tests: GPU Recovery with Fuzzy Module
 * ============================================================================
 * WHAT: Integration tests for GPU recovery with fuzzy logic operations
 * WHY:  Validate recovery works correctly with fuzzy GPU kernels
 * HOW:  Test recovery scenarios specific to fuzzy computations
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
class GPURecoveryFuzzyIntegrationTest : public ::testing::Test {
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

    /* CPU fallback for fuzzy membership function evaluation */
    static bool fuzzy_mf_cpu_fallback(void* context, void* params, void* result) {
        (void)context;
        if (!params || !result) return false;

        /* Simple triangular MF CPU implementation */
        float* input = static_cast<float*>(params);
        float* output = static_cast<float*>(result);

        /* Triangular MF: a=0, b=0.5, c=1 */
        float x = *input;
        if (x <= 0.0f || x >= 1.0f) {
            *output = 0.0f;
        } else if (x <= 0.5f) {
            *output = x / 0.5f;
        } else {
            *output = (1.0f - x) / 0.5f;
        }
        return true;
    }
#endif
};

/* ============================================================================
 * Test: RecoverFromMFBatchOOM
 * Verify recovery strategy selection for OOM errors
 * Note: FREE_CACHE action may "fail" if no cached memory is available to free,
 *       but the correct strategy is still selected.
 * ============================================================================ */
TEST_F(GPURecoveryFuzzyIntegrationTest, RecoverFromMFBatchOOM) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    /* Register CPU fallback */
    nimcp_gpu_set_cpu_fallback(ctx_, fuzzy_mf_cpu_fallback, nullptr);

    /* Simulate OOM recovery scenario */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);

    /* First action for OOM should be FREE_CACHE (may succeed or fail based on cache state) */
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_FREE_CACHE);
    /* Action was attempted regardless of success */
    EXPECT_NE(result.action_taken, GPU_RECOVERY_NONE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoverFromInferenceNumerical
 * Verify recovery from numerical errors in fuzzy inference
 * ============================================================================ */
TEST_F(GPURecoveryFuzzyIntegrationTest, RecoverFromInferenceNumerical) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, fuzzy_mf_cpu_fallback, nullptr);

    /* Simulate numerical error recovery */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_NUMERICAL,
                                           cudaSuccess, &result);

    EXPECT_TRUE(recovered);
    /* First action for numerical should be REDUCE_PRECISION */
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_REDUCE_PRECISION);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoverFromANFISKernelLaunch
 * Verify recovery from ANFIS kernel launch failures
 * ============================================================================ */
TEST_F(GPURecoveryFuzzyIntegrationTest, RecoverFromANFISKernelLaunch) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, fuzzy_mf_cpu_fallback, nullptr);

    /* Simulate kernel launch failure */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_KERNEL_LAUNCH,
                                           cudaErrorLaunchFailure, &result);

    EXPECT_TRUE(recovered);
    /* First action for kernel launch should be STREAM_SYNC */
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_STREAM_SYNC);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoverFromDefuzzTimeout
 * Verify recovery from defuzzification timeout
 * ============================================================================ */
TEST_F(GPURecoveryFuzzyIntegrationTest, RecoverFromDefuzzTimeout) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, fuzzy_mf_cpu_fallback, nullptr);

    /* Simulate timeout */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_TIMEOUT,
                                           cudaSuccess, &result);

    EXPECT_TRUE(recovered);
    /* First action for timeout should be ASYNC_SPLIT */
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_ASYNC_SPLIT);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CPUFallbackOnDeviceFailure
 * Verify CPU fallback is used when device unavailable
 * ============================================================================ */
TEST_F(GPURecoveryFuzzyIntegrationTest, CPUFallbackOnDeviceFailure) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, fuzzy_mf_cpu_fallback, nullptr);

    /* Device not available should immediately trigger CPU fallback */
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
 * Test: RetryWithReducedBatch
 * Verify batch reduction action and tracking
 * Note: Strategy selection is based on retry_count which only increments for
 * RETRY_* actions. To test REDUCE_BATCH, we use nimcp_gpu_select_recovery_strategy
 * directly with the appropriate retry_count.
 * ============================================================================ */
TEST_F(GPURecoveryFuzzyIntegrationTest, RetryWithReducedBatch) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    /* Test strategy selection for OOM at different retry counts */
    nimcp_gpu_recovery_action_t action0 = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 0);
    EXPECT_EQ(action0, GPU_RECOVERY_FREE_CACHE) << "First OOM action should be FREE_CACHE";

    nimcp_gpu_recovery_action_t action1 = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 1);
    EXPECT_EQ(action1, GPU_RECOVERY_REDUCE_BATCH) << "Second OOM action should be REDUCE_BATCH";

    /* Test that executing REDUCE_BATCH actually tracks batch reductions */
    uint32_t initial_reductions = ctx_->batch_reductions;
    bool success = nimcp_gpu_execute_recovery_action(ctx_, GPU_RECOVERY_REDUCE_BATCH);
    EXPECT_TRUE(success);
    EXPECT_EQ(ctx_->batch_reductions, initial_reductions + 1);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
