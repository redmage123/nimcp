/* ============================================================================
 * Regression Tests: GPU Recovery System
 * ============================================================================
 * WHAT: Regression tests for GPU self-healing recovery
 * WHY:  Prevent regressions in recovery stability and correctness
 * HOW:  Test edge cases, stress scenarios, and previously fixed bugs
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include <cuda_runtime.h>
#endif

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */
class GPURecoveryRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
        nimcp_gpu_recovery_init(NULL);
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    static bool dummy_fallback(void*, void*, void*) { return true; }
#endif
};

/* ============================================================================
 * Test: RapidSuccessiveFailures
 * Verify system handles rapid failures without corruption
 * ============================================================================ */
TEST_F(GPURecoveryRegressionTest, RapidSuccessiveFailures) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, dummy_fallback, nullptr);

    /* Rapid successive recovery attempts */
    for (int i = 0; i < 100; i++) {
        nimcp_gpu_recovery_result_t result;
        nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result);
        nimcp_gpu_recovery_context_reset(ctx);
    }

    /* Should still be functional */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS,
                                           cudaSuccess, &result);
    EXPECT_TRUE(recovered);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoveryUnderHighLoad
 * Verify recovery works under simulated high load
 * ============================================================================ */
TEST_F(GPURecoveryRegressionTest, RecoveryUnderHighLoad) {
#ifdef NIMCP_ENABLE_CUDA
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    const int num_threads = 4;
    const int iterations = 50;

    auto thread_func = [&]() {
        nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
        if (!ctx) {
            failure_count++;
            return;
        }

        nimcp_gpu_set_cpu_fallback(ctx, dummy_fallback, nullptr);

        for (int i = 0; i < iterations; i++) {
            nimcp_gpu_recovery_result_t result;
            bool recovered = nimcp_gpu_try_recover(ctx, GPU_ERROR_NUMERICAL,
                                                   cudaSuccess, &result);
            if (recovered) {
                success_count++;
            } else {
                failure_count++;
            }
            nimcp_gpu_recovery_context_reset(ctx);
        }

        nimcp_gpu_recovery_context_destroy(ctx);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    /* Most should succeed */
    EXPECT_GT(success_count.load(), 0);
    EXPECT_EQ(failure_count.load() + success_count.load(), num_threads * iterations);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ConcurrentRecoveryAttempts
 * Verify concurrent recovery from multiple threads
 * ============================================================================ */
TEST_F(GPURecoveryRegressionTest, ConcurrentRecoveryAttempts) {
#ifdef NIMCP_ENABLE_CUDA
    std::atomic<int> completed{0};
    const int num_threads = 8;

    auto thread_func = [&]() {
        nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
        if (ctx) {
            nimcp_gpu_set_cpu_fallback(ctx, dummy_fallback, nullptr);

            nimcp_gpu_recovery_result_t result;
            nimcp_gpu_try_recover(ctx, GPU_ERROR_OUT_OF_MEMORY,
                                  cudaErrorMemoryAllocation, &result);

            nimcp_gpu_recovery_context_destroy(ctx);
            completed++;
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed.load(), num_threads);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoveryPreservesDataIntegrity
 * Verify recovery doesn't corrupt context state
 * ============================================================================ */
TEST_F(GPURecoveryRegressionTest, RecoveryPreservesDataIntegrity) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);
    config.max_retries = 5;
    config.retry_delay_ms = 20;

    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    /* Store initial config values */
    uint32_t initial_max_retries = ctx->config.max_retries;
    uint32_t initial_delay = ctx->config.retry_delay_ms;

    /* Perform many recovery operations */
    nimcp_gpu_set_cpu_fallback(ctx, dummy_fallback, nullptr);
    for (int i = 0; i < 50; i++) {
        nimcp_gpu_recovery_result_t result;
        nimcp_gpu_try_recover(ctx, GPU_ERROR_KERNEL_LAUNCH,
                              cudaErrorLaunchFailure, &result);
    }

    /* Config should be unchanged */
    EXPECT_EQ(ctx->config.max_retries, initial_max_retries);
    EXPECT_EQ(ctx->config.retry_delay_ms, initial_delay);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: MaxRetriesRespected
 * Verify max retries limit is enforced via retry_count
 * Note: retry_count only increments for RETRY_* actions. This test verifies
 * that when retry_count reaches max_retries, recovery fails.
 * ============================================================================ */
TEST_F(GPURecoveryRegressionTest, MaxRetriesRespected) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);
    config.max_retries = 2;
    config.enable_cpu_fallback = false;  /* Disable fallback */

    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_recovery_result_t result;

    /* Manually increment retry_count to simulate retries */
    ctx->retry_count = 0;
    bool recovered = nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS,
                                           cudaSuccess, &result);
    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_CLAMP_PARAMS);

    /* Increment retry_count to max */
    ctx->retry_count = config.max_retries;

    /* Now recovery should fail (max retries exceeded, no fallback) */
    recovered = nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS,
                                      cudaSuccess, &result);
    EXPECT_FALSE(recovered) << "Should fail when retry_count >= max_retries without fallback";

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoveryStatisticsAccurate
 * Verify recovery statistics are accurate
 * ============================================================================ */
TEST_F(GPURecoveryRegressionTest, RecoveryStatisticsAccurate) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, dummy_fallback, nullptr);

    const int num_recoveries = 10;
    for (int i = 0; i < num_recoveries; i++) {
        nimcp_gpu_recovery_result_t result;
        nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result);
        nimcp_gpu_recovery_context_reset(ctx);
    }

    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    EXPECT_EQ(stats.total_errors, static_cast<uint64_t>(num_recoveries));
    EXPECT_EQ(stats.recoveries_attempted, static_cast<uint64_t>(num_recoveries));
    EXPECT_GE(stats.recoveries_succeeded, 0u);
    EXPECT_LE(stats.recoveries_succeeded, static_cast<uint64_t>(num_recoveries));

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoveryCallbacksInvoked
 * Verify CPU fallback callbacks are properly invoked
 * ============================================================================ */
TEST_F(GPURecoveryRegressionTest, RecoveryCallbacksInvoked) {
#ifdef NIMCP_ENABLE_CUDA
    static int callback_count = 0;
    callback_count = 0;

    auto counting_fallback = [](void*, void*, void*) -> bool {
        callback_count++;
        return true;
    };

    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, counting_fallback, nullptr);

    /* Execute fallback multiple times */
    for (int i = 0; i < 5; i++) {
        nimcp_gpu_execute_cpu_fallback(ctx, nullptr, nullptr);
    }

    EXPECT_EQ(callback_count, 5);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: GracefulDegradationChain
 * Verify progressive degradation through recovery strategies based on retry_count
 * Note: This tests strategy selection logic directly, not automatic retry progression.
 * ============================================================================ */
TEST_F(GPURecoveryRegressionTest, GracefulDegradationChain) {
#ifdef NIMCP_ENABLE_CUDA
    /* Verify the strategy progression for OOM errors at increasing retry counts */
    nimcp_gpu_recovery_action_t action0 = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 0);
    EXPECT_EQ(action0, GPU_RECOVERY_FREE_CACHE);

    nimcp_gpu_recovery_action_t action1 = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 1);
    EXPECT_EQ(action1, GPU_RECOVERY_REDUCE_BATCH);

    nimcp_gpu_recovery_action_t action2 = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 2);
    EXPECT_EQ(action2, GPU_RECOVERY_REDUCE_DIMENSIONS);

    nimcp_gpu_recovery_action_t action3 = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 3);
    EXPECT_EQ(action3, GPU_RECOVERY_CPU_FALLBACK);

    /* Verify all subsequent retry counts also result in CPU_FALLBACK */
    for (int i = 4; i < 10; i++) {
        nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
            GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, i);
        EXPECT_EQ(action, GPU_RECOVERY_CPU_FALLBACK);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoveryFromPartialFailure
 * Verify recovery after partial operation failure
 * ============================================================================ */
TEST_F(GPURecoveryRegressionTest, RecoveryFromPartialFailure) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, dummy_fallback, nullptr);

    /* Simulate various error types in sequence */
    nimcp_gpu_recovery_result_t result;

    nimcp_gpu_try_recover(ctx, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);
    nimcp_gpu_recovery_context_reset(ctx);

    nimcp_gpu_try_recover(ctx, GPU_ERROR_NUMERICAL, cudaSuccess, &result);
    nimcp_gpu_recovery_context_reset(ctx);

    nimcp_gpu_try_recover(ctx, GPU_ERROR_KERNEL_LAUNCH,
                          cudaErrorLaunchFailure, &result);
    nimcp_gpu_recovery_context_reset(ctx);

    /* Final recovery should still work */
    bool recovered = nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS,
                                           cudaSuccess, &result);
    EXPECT_TRUE(recovered);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: StabilityAfterMultipleRecoveries
 * Verify system remains stable after many recoveries
 * ============================================================================ */
TEST_F(GPURecoveryRegressionTest, StabilityAfterMultipleRecoveries) {
#ifdef NIMCP_ENABLE_CUDA
    const int num_iterations = 100;

    for (int i = 0; i < num_iterations; i++) {
        nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
        ASSERT_NE(ctx, nullptr) << "Failed at iteration " << i;

        nimcp_gpu_set_cpu_fallback(ctx, dummy_fallback, nullptr);

        nimcp_gpu_recovery_result_t result;
        nimcp_gpu_try_recover(ctx, GPU_ERROR_OUT_OF_MEMORY,
                              cudaErrorMemoryAllocation, &result);

        nimcp_gpu_recovery_context_destroy(ctx);
    }

    /* System should still be functional */
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
    EXPECT_TRUE(nimcp_gpu_cpu_fallback_available());
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: NoRecoveryLoops
 * Verify recovery terminates when retry_count reaches max_retries
 * Note: This test verifies that when retry_count >= max_retries, recovery fails
 * unless CPU fallback is available.
 * ============================================================================ */
TEST_F(GPURecoveryRegressionTest, NoRecoveryLoops) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);
    config.max_retries = 5;
    config.enable_cpu_fallback = false;  /* No fallback to terminate */

    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    /* Test 1: Recovery should work when retry_count is 0 */
    ctx->retry_count = 0;
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS,
                                           cudaSuccess, &result);
    EXPECT_TRUE(recovered) << "Should succeed at retry_count=0";
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_CLAMP_PARAMS);

    /* Test 2: Recovery should fail when retry_count >= max_retries (without fallback) */
    ctx->retry_count = config.max_retries;
    recovered = nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS,
                                      cudaSuccess, &result);
    EXPECT_FALSE(recovered) << "Should fail when retry_count >= max_retries";

    /* Test 3: Recovery at retry_count=1 should also work (SET_DEFAULTS action) */
    ctx->retry_count = 1;
    recovered = nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS,
                                      cudaSuccess, &result);
    EXPECT_TRUE(recovered) << "Should succeed at retry_count=1";
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_SET_DEFAULTS);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: MemoryPressureRecovery
 * Verify recovery under memory pressure
 * ============================================================================ */
TEST_F(GPURecoveryRegressionTest, MemoryPressureRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Check initial memory state */
    size_t initial_free, initial_total;
    nimcp_gpu_get_memory_info(&initial_free, &initial_total);

    /* Perform recoveries */
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, dummy_fallback, nullptr);

    for (int i = 0; i < 20; i++) {
        nimcp_gpu_recovery_result_t result;
        nimcp_gpu_try_recover(ctx, GPU_ERROR_OUT_OF_MEMORY,
                              cudaErrorMemoryAllocation, &result);
        nimcp_gpu_recovery_context_reset(ctx);
    }

    nimcp_gpu_recovery_context_destroy(ctx);

    /* Free caches */
    nimcp_gpu_free_caches();
    cudaDeviceSynchronize();

    /* Memory should be roughly recovered */
    size_t final_free, final_total;
    nimcp_gpu_get_memory_info(&final_free, &final_total);

    EXPECT_EQ(final_total, initial_total);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
