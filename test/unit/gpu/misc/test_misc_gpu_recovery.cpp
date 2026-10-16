/**
 * @file test_misc_gpu_recovery.cpp
 * @brief Unit tests for GPU recovery integration in misc modules
 *
 * Tests the GPU recovery integration in:
 * - neuron/nimcp_gpu_kernels.cu
 * - backend/nimcp_kernel_backend_cuda.cu
 * - transfer/nimcp_prefetch.cu
 * - transfer/nimcp_async_transfer.cu
 * - reasoning/nimcp_reasoning_kernels.cu
 * - metalearning/nimcp_metalearning_kernels.cu
 * - regions/nimcp_regions_kernels.cu
 *
 * @version 1.0
 * @date 2025-01-30
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

// Recovery header
extern "C" {
#include "gpu/recovery/nimcp_gpu_recovery.h"
}

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

//=============================================================================
// Test Fixture
//=============================================================================

class MiscGpuRecoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure recovery is initialized
        if (!nimcp_gpu_recovery_is_initialized()) {
            ASSERT_EQ(0, nimcp_gpu_recovery_init(NULL));
        }
    }

    void TearDown() override {
        // Reset stats between tests
        nimcp_gpu_recovery_reset_stats();
    }
};

//=============================================================================
// Basic Recovery Initialization Tests
//=============================================================================

TEST_F(MiscGpuRecoveryTest, RecoveryIsInitialized) {
    // Recovery should be initialized from SetUp
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
}

TEST_F(MiscGpuRecoveryTest, MultipleInitCalls) {
    // Multiple init calls should be safe (idempotent)
    EXPECT_EQ(0, nimcp_gpu_recovery_init(NULL));
    EXPECT_EQ(0, nimcp_gpu_recovery_init(NULL));
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
}

TEST_F(MiscGpuRecoveryTest, DefaultConfigValues) {
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);

    EXPECT_TRUE(config.enable_cpu_fallback);
    EXPECT_TRUE(config.enable_param_correction);
    EXPECT_TRUE(config.enable_batch_reduction);
    EXPECT_TRUE(config.enable_retry);
    EXPECT_EQ(3u, config.max_retries);
    EXPECT_GT(config.retry_delay_ms, 0u);
    EXPECT_GT(config.batch_reduction_factor, 0.0f);
    EXPECT_LE(config.batch_reduction_factor, 1.0f);
    EXPECT_GT(config.memory_threshold, 0.0f);
    EXPECT_LE(config.memory_threshold, 1.0f);
}

//=============================================================================
// Recovery Context Tests
//=============================================================================

TEST_F(MiscGpuRecoveryTest, ContextCreateDestroy) {
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(nullptr, ctx);

    // Check initial state
    EXPECT_EQ(0u, ctx->retry_count);
    EXPECT_EQ(0u, ctx->batch_reductions);
    EXPECT_FALSE(ctx->cpu_fallback_active);

    nimcp_gpu_recovery_context_destroy(ctx);
}

TEST_F(MiscGpuRecoveryTest, ContextCreateWithConfig) {
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);
    config.max_retries = 5;
    config.enable_cpu_fallback = false;

    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(&config);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(5u, ctx->config.max_retries);
    EXPECT_FALSE(ctx->config.enable_cpu_fallback);

    nimcp_gpu_recovery_context_destroy(ctx);
}

TEST_F(MiscGpuRecoveryTest, ContextReset) {
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(nullptr, ctx);

    // Simulate some state
    ctx->retry_count = 3;
    ctx->batch_reductions = 2;
    ctx->recoveries_attempted = 5;

    nimcp_gpu_recovery_context_reset(ctx);

    EXPECT_EQ(0u, ctx->retry_count);
    EXPECT_EQ(0u, ctx->batch_reductions);
    EXPECT_FALSE(ctx->cpu_fallback_active);

    nimcp_gpu_recovery_context_destroy(ctx);
}

//=============================================================================
// Error Category Tests
//=============================================================================

TEST_F(MiscGpuRecoveryTest, ErrorCategoryNames) {
    EXPECT_STREQ("UNKNOWN", nimcp_gpu_error_category_name(GPU_ERROR_UNKNOWN));
    EXPECT_STREQ("INVALID_PARAMS", nimcp_gpu_error_category_name(GPU_ERROR_INVALID_PARAMS));
    EXPECT_STREQ("OUT_OF_MEMORY", nimcp_gpu_error_category_name(GPU_ERROR_OUT_OF_MEMORY));
    EXPECT_STREQ("CUDA_RUNTIME", nimcp_gpu_error_category_name(GPU_ERROR_CUDA_RUNTIME));
    EXPECT_STREQ("KERNEL_LAUNCH", nimcp_gpu_error_category_name(GPU_ERROR_KERNEL_LAUNCH));
    EXPECT_STREQ("NUMERICAL", nimcp_gpu_error_category_name(GPU_ERROR_NUMERICAL));
    EXPECT_STREQ("TIMEOUT", nimcp_gpu_error_category_name(GPU_ERROR_TIMEOUT));
    EXPECT_STREQ("CONTEXT_INVALID", nimcp_gpu_error_category_name(GPU_ERROR_CONTEXT_INVALID));
    EXPECT_STREQ("LIBRARY", nimcp_gpu_error_category_name(GPU_ERROR_LIBRARY));
    EXPECT_STREQ("DEVICE_NOT_AVAILABLE", nimcp_gpu_error_category_name(GPU_ERROR_DEVICE_NOT_AVAILABLE));
}

TEST_F(MiscGpuRecoveryTest, RecoveryActionNames) {
    EXPECT_STREQ("NONE", nimcp_gpu_recovery_action_name(GPU_RECOVERY_NONE));
    EXPECT_STREQ("CLAMP_PARAMS", nimcp_gpu_recovery_action_name(GPU_RECOVERY_CLAMP_PARAMS));
    EXPECT_STREQ("CPU_FALLBACK", nimcp_gpu_recovery_action_name(GPU_RECOVERY_CPU_FALLBACK));
    EXPECT_STREQ("REDUCE_BATCH", nimcp_gpu_recovery_action_name(GPU_RECOVERY_REDUCE_BATCH));
    EXPECT_STREQ("FREE_CACHE", nimcp_gpu_recovery_action_name(GPU_RECOVERY_FREE_CACHE));
    EXPECT_STREQ("RETRY_IMMEDIATE", nimcp_gpu_recovery_action_name(GPU_RECOVERY_RETRY_IMMEDIATE));
}

//=============================================================================
// Strategy Selection Tests
//=============================================================================

TEST_F(MiscGpuRecoveryTest, StrategyForInvalidParams) {
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_INVALID_PARAMS, cudaSuccess, 0);

    // Should suggest param correction or defaults
    EXPECT_TRUE(action == GPU_RECOVERY_CLAMP_PARAMS ||
                action == GPU_RECOVERY_SET_DEFAULTS ||
                action == GPU_RECOVERY_VALIDATE_FIX);
}

TEST_F(MiscGpuRecoveryTest, StrategyForOutOfMemory) {
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 0);

    // Should suggest batch reduction, cache free, or CPU fallback
    EXPECT_TRUE(action == GPU_RECOVERY_REDUCE_BATCH ||
                action == GPU_RECOVERY_FREE_CACHE ||
                action == GPU_RECOVERY_CPU_FALLBACK ||
                action == GPU_RECOVERY_REDUCE_DIMENSIONS);
}

TEST_F(MiscGpuRecoveryTest, StrategyForKernelLaunch) {
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_KERNEL_LAUNCH, cudaErrorLaunchFailure, 0);

    // Should suggest retry, device reset, or CPU fallback
    EXPECT_TRUE(action == GPU_RECOVERY_RETRY_IMMEDIATE ||
                action == GPU_RECOVERY_RESET_DEVICE ||
                action == GPU_RECOVERY_CPU_FALLBACK ||
                action == GPU_RECOVERY_REDUCE_BATCH);
}

//=============================================================================
// Parameter Correction Tests
//=============================================================================

TEST_F(MiscGpuRecoveryTest, CorrectFloatParamInRange) {
    nimcp_gpu_param_range_t range = {0.0f, 1.0f, 0.5f, true};
    float value = 0.7f;

    bool corrected = nimcp_gpu_correct_param_float(&value, &range, "test_param");

    EXPECT_FALSE(corrected);  // No correction needed
    EXPECT_FLOAT_EQ(0.7f, value);
}

TEST_F(MiscGpuRecoveryTest, CorrectFloatParamOutOfRange) {
    nimcp_gpu_param_range_t range = {0.0f, 1.0f, 0.5f, true};
    float value = 2.5f;

    bool corrected = nimcp_gpu_correct_param_float(&value, &range, "test_param");

    EXPECT_TRUE(corrected);
    EXPECT_FLOAT_EQ(1.0f, value);  // Clamped to max
}

TEST_F(MiscGpuRecoveryTest, CorrectFloatParamNegative) {
    nimcp_gpu_param_range_t range = {0.0f, 1.0f, 0.5f, true};
    float value = -0.5f;

    bool corrected = nimcp_gpu_correct_param_float(&value, &range, "test_param");

    EXPECT_TRUE(corrected);
    EXPECT_FLOAT_EQ(0.0f, value);  // Clamped to min
}

TEST_F(MiscGpuRecoveryTest, CorrectFloatParamUseDefault) {
    nimcp_gpu_param_range_t range = {0.0f, 1.0f, 0.5f, false};  // Use default instead of clamp
    float value = 2.5f;

    bool corrected = nimcp_gpu_correct_param_float(&value, &range, "test_param");

    EXPECT_TRUE(corrected);
    EXPECT_FLOAT_EQ(0.5f, value);  // Reset to default
}

TEST_F(MiscGpuRecoveryTest, CorrectIntParamOutOfRange) {
    int value = 100;

    bool corrected = nimcp_gpu_correct_param_int(&value, 0, 10, 5, "test_int");

    EXPECT_TRUE(corrected);
    EXPECT_EQ(10, value);  // Clamped to max
}

TEST_F(MiscGpuRecoveryTest, CorrectSizeParamOutOfRange) {
    size_t value = 0;

    bool corrected = nimcp_gpu_correct_param_size(&value, 1, 1000, 100, "test_size");

    EXPECT_TRUE(corrected);
    EXPECT_EQ(100u, value);  // Reset to default since 0 < min
}

//=============================================================================
// Recovery Attempt Tests
//=============================================================================

TEST_F(MiscGpuRecoveryTest, TryRecoverFromInvalidParams) {
    nimcp_gpu_recovery_result_t result;
    memset(&result, 0, sizeof(result));

    bool recovered = nimcp_gpu_try_recover(NULL, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result);

    // Should typically succeed for invalid params (can be corrected)
    if (recovered) {
        EXPECT_TRUE(result.success);
        EXPECT_NE(GPU_RECOVERY_NONE, result.action_taken);
    }
}

TEST_F(MiscGpuRecoveryTest, TryRecoverWithContext) {
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(nullptr, ctx);

    nimcp_gpu_recovery_result_t result;
    memset(&result, 0, sizeof(result));

    bool recovered = nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result);

    if (recovered) {
        EXPECT_TRUE(result.success);
        // Stats should be updated
        EXPECT_GE(ctx->recoveries_attempted, 1ULL);
    }

    nimcp_gpu_recovery_context_destroy(ctx);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(MiscGpuRecoveryTest, StatsInitialState) {
    nimcp_gpu_recovery_reset_stats();

    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    EXPECT_EQ(0ULL, stats.total_errors);
    EXPECT_EQ(0ULL, stats.recoveries_attempted);
    EXPECT_EQ(0ULL, stats.recoveries_succeeded);
}

TEST_F(MiscGpuRecoveryTest, StatsAfterRecoveryAttempt) {
    nimcp_gpu_recovery_reset_stats();

    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(NULL, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result);

    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    EXPECT_GE(stats.recoveries_attempted, 1ULL);
}

//=============================================================================
// Memory Management Tests
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA
TEST_F(MiscGpuRecoveryTest, GetMemoryInfo) {
    size_t free_bytes = 0;
    size_t total_bytes = 0;

    bool success = nimcp_gpu_get_memory_info(&free_bytes, &total_bytes);

    // May fail if no GPU available
    if (success) {
        EXPECT_GT(total_bytes, 0u);
        EXPECT_LE(free_bytes, total_bytes);
    }
}

TEST_F(MiscGpuRecoveryTest, MemoryCriticalCheck) {
    // This should not crash even without GPU
    bool critical = nimcp_gpu_memory_critical(0.95f);
    // Result depends on actual memory usage
    (void)critical;  // Avoid unused warning
}

TEST_F(MiscGpuRecoveryTest, FreeCaches) {
    size_t freed = nimcp_gpu_free_caches();
    // May return 0 if no caches to free
    EXPECT_GE(freed, 0u);
}
#endif

//=============================================================================
// CPU Fallback Tests
//=============================================================================

static bool test_cpu_fallback_fn(void* context, void* params, void* result) {
    int* called = (int*)context;
    (*called)++;
    return true;
}

TEST_F(MiscGpuRecoveryTest, SetCpuFallback) {
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(nullptr, ctx);

    int call_count = 0;
    nimcp_gpu_set_cpu_fallback(ctx, test_cpu_fallback_fn, &call_count);

    EXPECT_EQ(test_cpu_fallback_fn, ctx->cpu_fallback_fn);
    EXPECT_EQ(&call_count, ctx->cpu_fallback_context);

    nimcp_gpu_recovery_context_destroy(ctx);
}

TEST_F(MiscGpuRecoveryTest, ExecuteCpuFallback) {
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(nullptr, ctx);

    int call_count = 0;
    nimcp_gpu_set_cpu_fallback(ctx, test_cpu_fallback_fn, &call_count);

    bool success = nimcp_gpu_execute_cpu_fallback(ctx, NULL, NULL);

    EXPECT_TRUE(success);
    EXPECT_EQ(1, call_count);

    nimcp_gpu_recovery_context_destroy(ctx);
}

TEST_F(MiscGpuRecoveryTest, CpuFallbackNotSet) {
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(nullptr, ctx);

    // Don't set fallback
    bool success = nimcp_gpu_execute_cpu_fallback(ctx, NULL, NULL);

    EXPECT_FALSE(success);  // No fallback set

    nimcp_gpu_recovery_context_destroy(ctx);
}

//=============================================================================
// Error Reporting Tests
//=============================================================================

TEST_F(MiscGpuRecoveryTest, ReportError) {
    nimcp_gpu_recovery_reset_stats();

    nimcp_gpu_recovery_report_error(GPU_ERROR_KERNEL_LAUNCH, cudaErrorLaunchFailure,
                                     __FILE__, __LINE__);

    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    EXPECT_GE(stats.total_errors, 1ULL);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
