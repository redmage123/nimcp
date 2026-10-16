/* ============================================================================
 * Integration Tests: GPU Recovery with Financial Module
 * ============================================================================
 * WHAT: Integration tests for GPU recovery with financial computations
 * WHY:  Validate recovery works correctly with financial GPU kernels
 * HOW:  Test recovery scenarios specific to Monte Carlo, options, etc.
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
class GPURecoveryFinancialIntegrationTest : public ::testing::Test {
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

    /* CPU fallback for Monte Carlo simulation */
    static bool monte_carlo_cpu_fallback(void* context, void* params, void* result) {
        (void)context;
        if (!params || !result) return false;

        /* Simple Black-Scholes CPU approximation */
        float* spot = static_cast<float*>(params);
        float* price = static_cast<float*>(result);

        /* Simplified option pricing */
        *price = *spot * 0.05f;  /* Placeholder */
        return true;
    }
#endif
};

/* ============================================================================
 * Test: RecoverFromMonteCarloOOM
 * Verify recovery strategy selection for Monte Carlo OOM
 * Note: FREE_CACHE may "fail" if no cached memory is available.
 * ============================================================================ */
TEST_F(GPURecoveryFinancialIntegrationTest, RecoverFromMonteCarloOOM) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, monte_carlo_cpu_fallback, nullptr);

    /* Monte Carlo with large path count may OOM */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);

    /* First OOM action should be FREE_CACHE (may succeed/fail based on cache state) */
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_FREE_CACHE);
    EXPECT_EQ(ctx_->retry_count, 0u) << "retry_count stays 0 for non-RETRY actions";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoverFromRNGInitFailure
 * Verify recovery from RNG initialization failure
 * ============================================================================ */
TEST_F(GPURecoveryFinancialIntegrationTest, RecoverFromRNGInitFailure) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, monte_carlo_cpu_fallback, nullptr);

    /* RNG init failure is typically a library error */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_LIBRARY,
                                           cudaSuccess, &result);

    EXPECT_TRUE(recovered);
    /* Library errors first try immediate retry */
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_RETRY_IMMEDIATE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoverFromPortfolioOptNumerical
 * Verify recovery from numerical errors in portfolio optimization
 * ============================================================================ */
TEST_F(GPURecoveryFinancialIntegrationTest, RecoverFromPortfolioOptNumerical) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, monte_carlo_cpu_fallback, nullptr);

    /* Portfolio optimization may have numerical stability issues */
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
 * Test: RecoverFromBinomialTreeOOM
 * Verify OOM recovery strategy progression
 * Note: Tests use select_recovery_strategy directly to verify progression logic.
 * ============================================================================ */
TEST_F(GPURecoveryFinancialIntegrationTest, RecoverFromBinomialTreeOOM) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, monte_carlo_cpu_fallback, nullptr);

    /* Verify OOM recovery strategy progression */
    nimcp_gpu_recovery_action_t action0 = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 0);
    EXPECT_EQ(action0, GPU_RECOVERY_FREE_CACHE) << "First OOM action should be FREE_CACHE";

    nimcp_gpu_recovery_action_t action1 = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 1);
    EXPECT_EQ(action1, GPU_RECOVERY_REDUCE_BATCH) << "Second OOM action should be REDUCE_BATCH";

    nimcp_gpu_recovery_action_t action2 = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 2);
    EXPECT_EQ(action2, GPU_RECOVERY_REDUCE_DIMENSIONS) << "Third OOM action should be REDUCE_DIMENSIONS";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CPUFallbackOnDeviceFailure
 * Verify CPU fallback executes for financial calculations
 * ============================================================================ */
TEST_F(GPURecoveryFinancialIntegrationTest, CPUFallbackOnDeviceFailure) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, monte_carlo_cpu_fallback, nullptr);

    /* Execute CPU fallback */
    float spot_price = 100.0f;
    float option_price = 0.0f;

    bool success = nimcp_gpu_execute_cpu_fallback(ctx_, &spot_price, &option_price);
    EXPECT_TRUE(success);
    EXPECT_GT(option_price, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RetryWithReducedPaths
 * Verify batch reduction action and tracking for Monte Carlo
 * ============================================================================ */
TEST_F(GPURecoveryFinancialIntegrationTest, RetryWithReducedPaths) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    /* Test batch reduction action directly */
    uint32_t initial_reductions = ctx_->batch_reductions;
    EXPECT_EQ(initial_reductions, 0u);

    /* Execute REDUCE_BATCH action */
    bool success = nimcp_gpu_execute_recovery_action(ctx_, GPU_RECOVERY_REDUCE_BATCH);
    EXPECT_TRUE(success);
    EXPECT_EQ(ctx_->batch_reductions, initial_reductions + 1);

    /* Verify batch factor calculation */
    float expected_factor = powf(ctx_->config.batch_reduction_factor, 1.0f);
    EXPECT_LT(expected_factor, 1.0f) << "Batch reduction factor should reduce batch";

    /* Multiple reductions further reduce batch */
    nimcp_gpu_execute_recovery_action(ctx_, GPU_RECOVERY_REDUCE_BATCH);
    EXPECT_EQ(ctx_->batch_reductions, 2u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
