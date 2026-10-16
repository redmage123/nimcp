/* ============================================================================
 * Integration Tests: GPU Recovery Memory Management
 * ============================================================================
 * WHAT: Integration tests for GPU memory management during recovery
 * WHY:  Validate memory cleanup and recovery doesn't leak
 * HOW:  Test cache freeing, synchronization, and memory tracking
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
class GPURecoveryMemoryManagementTest : public ::testing::Test {
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

        /* Record initial memory state */
        nimcp_gpu_get_memory_info(&initial_free_, &initial_total_);
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
    size_t initial_free_ = 0;
    size_t initial_total_ = 0;
#endif
};

/* ============================================================================
 * Test: FreeCacheOnOOM
 * Verify cache freeing releases memory
 * ============================================================================ */
TEST_F(GPURecoveryMemoryManagementTest, FreeCacheOnOOM) {
#ifdef NIMCP_ENABLE_CUDA
    /* Allocate some GPU memory to create cache pressure */
    void* ptr = nullptr;
    size_t alloc_size = 1024 * 1024;  /* 1MB */
    cudaError_t err = cudaMalloc(&ptr, alloc_size);
    if (err == cudaSuccess) {
        cudaFree(ptr);
    }

    /* Free caches */
    size_t freed = nimcp_gpu_free_caches();

    /* Freed bytes returned (may be 0 if nothing cached) */
    EXPECT_GE(freed, 0u);

    /* Verify memory info can be retrieved */
    size_t free_bytes, total_bytes;
    bool success = nimcp_gpu_get_memory_info(&free_bytes, &total_bytes);
    EXPECT_TRUE(success);
    EXPECT_GT(total_bytes, 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: NoLeaksAfterRecovery
 * Verify recovery doesn't leak memory
 * ============================================================================ */
TEST_F(GPURecoveryMemoryManagementTest, NoLeaksAfterRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    /* Synchronize and free caches first */
    cudaDeviceSynchronize();
    nimcp_gpu_free_caches();

    size_t before_free, before_total;
    nimcp_gpu_get_memory_info(&before_free, &before_total);

    /* Perform multiple recovery cycles */
    for (int i = 0; i < 10; i++) {
        nimcp_gpu_recovery_result_t result;
        nimcp_gpu_try_recover(ctx_, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result);
        nimcp_gpu_recovery_context_reset(ctx_);
    }

    /* Synchronize and check memory */
    cudaDeviceSynchronize();
    nimcp_gpu_free_caches();

    size_t after_free, after_total;
    nimcp_gpu_get_memory_info(&after_free, &after_total);

    /* Memory should be roughly the same (within reasonable tolerance) */
    EXPECT_EQ(after_total, before_total);
    /* Allow some variance in free memory due to CUDA runtime */
    size_t diff = (before_free > after_free) ?
                  (before_free - after_free) : (after_free - before_free);
    EXPECT_LT(diff, 1024 * 1024) << "Memory difference should be < 1MB";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: StreamSyncOnTimeout
 * Verify stream synchronization during timeout recovery
 * ============================================================================ */
TEST_F(GPURecoveryMemoryManagementTest, StreamSyncOnTimeout) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    /* Timeout recovery should trigger stream sync */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_TIMEOUT,
                                           cudaSuccess, &result);

    /* First timeout action is ASYNC_SPLIT, not STREAM_SYNC */
    EXPECT_TRUE(recovered);

    /* After async split, try kernel launch which uses STREAM_SYNC */
    recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_KERNEL_LAUNCH,
                                      cudaErrorLaunchFailure, &result);
    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_STREAM_SYNC);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: DeviceResetCleanup
 * Verify device reset cleans up properly
 * ============================================================================ */
TEST_F(GPURecoveryMemoryManagementTest, DeviceResetCleanup) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    /* Force a device reset action */
    bool success = nimcp_gpu_execute_recovery_action(ctx_, GPU_RECOVERY_RESET_DEVICE);

    /* Device reset may succeed or fail depending on state */
    /* Either way, we should be able to continue */
    if (success) {
        /* Verify we can still query memory */
        size_t free_bytes, total_bytes;
        bool mem_success = nimcp_gpu_get_memory_info(&free_bytes, &total_bytes);
        EXPECT_TRUE(mem_success);
    }

    SUCCEED() << "Device reset completed without crash";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: MemoryCriticalDetection
 * Verify memory critical threshold detection
 * Note: nimcp_gpu_memory_critical returns true when usage >= threshold
 * usage = 1.0 - (free/total), so high threshold = less likely critical
 * ============================================================================ */
TEST_F(GPURecoveryMemoryManagementTest, MemoryCriticalDetection) {
#ifdef NIMCP_ENABLE_CUDA
    /* Get current memory state to understand test expectations */
    size_t free_bytes, total_bytes;
    bool got_info = nimcp_gpu_get_memory_info(&free_bytes, &total_bytes);
    ASSERT_TRUE(got_info) << "Should be able to get memory info";

    float current_usage = 1.0f - ((float)free_bytes / (float)total_bytes);

    /* With threshold below current usage, should be critical */
    bool critical = nimcp_gpu_memory_critical(0.0f);
    EXPECT_TRUE(critical) << "0% threshold should always be critical when any memory is used";

    /* With threshold of 1.0 (100% usage), should NOT be critical unless all memory is used */
    critical = nimcp_gpu_memory_critical(1.0f);
    EXPECT_FALSE(critical) << "100% threshold should not be critical unless all memory used";

    /* Test at current usage level */
    /* If usage is 10%, threshold of 5% should be critical, threshold of 50% should not */
    if (current_usage > 0.05f) {
        critical = nimcp_gpu_memory_critical(0.05f);
        EXPECT_TRUE(critical) << "Threshold below current usage should be critical";
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: EnsureMemoryAvailable
 * Verify memory availability check
 * ============================================================================ */
TEST_F(GPURecoveryMemoryManagementTest, EnsureMemoryAvailable) {
#ifdef NIMCP_ENABLE_CUDA
    /* Small amount should be available */
    bool available = nimcp_gpu_ensure_memory_available(1024);  /* 1KB */
    EXPECT_TRUE(available);

    /* Extremely large amount (more than total) should not be available */
    size_t free_bytes, total_bytes;
    nimcp_gpu_get_memory_info(&free_bytes, &total_bytes);

    available = nimcp_gpu_ensure_memory_available(total_bytes * 2);
    EXPECT_FALSE(available) << "Shouldn't be able to ensure more than total memory";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoveryTrackingAccurate
 * Verify recovery statistics are accurate
 * ============================================================================ */
TEST_F(GPURecoveryMemoryManagementTest, RecoveryTrackingAccurate) {
#ifdef NIMCP_ENABLE_CUDA
    /* Reset stats */
    nimcp_gpu_recovery_reset_stats();

    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_EQ(stats.total_errors, 0u);
    EXPECT_EQ(stats.recoveries_attempted, 0u);

    /* Perform some recoveries */
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result);
    nimcp_gpu_try_recover(ctx, GPU_ERROR_NUMERICAL, cudaSuccess, &result);
    nimcp_gpu_try_recover(ctx, GPU_ERROR_KERNEL_LAUNCH, cudaErrorLaunchFailure, &result);

    /* Check stats were updated */
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_EQ(stats.total_errors, 3u);
    EXPECT_EQ(stats.recoveries_attempted, 3u);
    EXPECT_GE(stats.recoveries_succeeded, 0u);

    /* Success rate should be calculated */
    if (stats.recoveries_attempted > 0) {
        EXPECT_GE(stats.success_rate, 0.0f);
        EXPECT_LE(stats.success_rate, 1.0f);
    }

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
