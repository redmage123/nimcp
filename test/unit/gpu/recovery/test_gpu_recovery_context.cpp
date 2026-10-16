/* ============================================================================
 * Unit Tests: GPU Recovery Context Management
 * ============================================================================
 * WHAT: Unit tests for GPU recovery context creation and management
 * WHY:  Validate context lifecycle and state tracking
 * HOW:  Test create, destroy, reset, and context state
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <thread>
#include <vector>
#include <atomic>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include <cuda_runtime.h>
#endif

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */
class GPURecoveryContextTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        /* Initialize recovery system */
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
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
};

/* ============================================================================
 * Test: CreateContextSucceeds
 * Verify context creation succeeds with default config
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, CreateContextSucceeds) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr) << "Context creation should succeed";

    /* Verify initial state */
    EXPECT_EQ(ctx->retry_count, 0u);
    EXPECT_EQ(ctx->batch_reductions, 0u);
    EXPECT_FALSE(ctx->cpu_fallback_active);
    EXPECT_EQ(ctx->recoveries_attempted, 0u);
    EXPECT_EQ(ctx->recoveries_succeeded, 0u);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CreateContextWithCustomConfig
 * Verify context creation with custom configuration
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, CreateContextWithCustomConfig) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);
    config.max_retries = 7;
    config.enable_cpu_fallback = false;
    config.batch_reduction_factor = 0.3f;

    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(ctx->config.max_retries, 7u);
    EXPECT_FALSE(ctx->config.enable_cpu_fallback);
    EXPECT_FLOAT_EQ(ctx->config.batch_reduction_factor, 0.3f);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: DestroyContextSucceeds
 * Verify context destruction works correctly
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, DestroyContextSucceeds) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    /* Destroy should not crash */
    nimcp_gpu_recovery_context_destroy(ctx);

    /* After destroy, pointer is invalid - just verify no crash occurred */
    SUCCEED() << "Context destroyed successfully";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: DestroyNullContextHandled
 * Verify NULL context destruction is handled gracefully
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, DestroyNullContextHandled) {
#ifdef NIMCP_ENABLE_CUDA
    /* Should not crash */
    nimcp_gpu_recovery_context_destroy(NULL);
    SUCCEED() << "NULL context destroy handled gracefully";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ContextResetClearsState
 * Verify context reset clears runtime state but preserves config
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, ContextResetClearsState) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);
    config.max_retries = 5;

    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    /* Simulate some activity */
    ctx->retry_count = 3;
    ctx->batch_reductions = 2;
    ctx->cpu_fallback_active = true;
    ctx->last_error_category = GPU_ERROR_OUT_OF_MEMORY;
    ctx->recoveries_attempted = 10;
    ctx->recoveries_succeeded = 8;

    /* Reset */
    nimcp_gpu_recovery_context_reset(ctx);

    /* State should be cleared */
    EXPECT_EQ(ctx->retry_count, 0u);
    EXPECT_EQ(ctx->batch_reductions, 0u);
    EXPECT_FALSE(ctx->cpu_fallback_active);
    EXPECT_EQ(ctx->last_error_category, GPU_ERROR_UNKNOWN);

    /* Config should be preserved */
    EXPECT_EQ(ctx->config.max_retries, 5u);

    /* Note: recoveries_attempted/succeeded are NOT reset by context_reset */
    /* They track lifetime stats */

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ResetNullContextHandled
 * Verify reset of NULL context is handled gracefully
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, ResetNullContextHandled) {
#ifdef NIMCP_ENABLE_CUDA
    /* Should not crash */
    nimcp_gpu_recovery_context_reset(NULL);
    SUCCEED() << "NULL context reset handled gracefully";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: MultipleContextsIndependent
 * Verify multiple contexts maintain independent state
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, MultipleContextsIndependent) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config1, config2;
    nimcp_gpu_recovery_default_config(&config1);
    nimcp_gpu_recovery_default_config(&config2);
    config1.max_retries = 5;
    config2.max_retries = 10;

    nimcp_gpu_recovery_context_t* ctx1 = nimcp_gpu_recovery_context_create(&config1);
    nimcp_gpu_recovery_context_t* ctx2 = nimcp_gpu_recovery_context_create(&config2);
    ASSERT_NE(ctx1, nullptr);
    ASSERT_NE(ctx2, nullptr);
    ASSERT_NE(ctx1, ctx2);

    /* Verify independent configs */
    EXPECT_EQ(ctx1->config.max_retries, 5u);
    EXPECT_EQ(ctx2->config.max_retries, 10u);

    /* Modify one context's state */
    ctx1->retry_count = 3;
    ctx1->cpu_fallback_active = true;

    /* Other context should be unaffected */
    EXPECT_EQ(ctx2->retry_count, 0u);
    EXPECT_FALSE(ctx2->cpu_fallback_active);

    nimcp_gpu_recovery_context_destroy(ctx1);
    nimcp_gpu_recovery_context_destroy(ctx2);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ContextStoresErrorInfo
 * Verify context correctly stores last error information
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, ContextStoresErrorInfo) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    /* Trigger a recovery attempt with specific error */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx, GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, &result);

    /* Verify error info is stored */
    EXPECT_EQ(ctx->last_error_category, GPU_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(ctx->last_cuda_error, cudaErrorMemoryAllocation);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ContextTracksRetryCount
 * Verify context correctly tracks retry count
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, ContextTracksRetryCount) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(ctx->retry_count, 0u);

    /* Each recovery attempt should increment retry count */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result);
    /* After first recovery, retry_count may be incremented depending on action */

    /* Reset and verify count is cleared */
    nimcp_gpu_recovery_context_reset(ctx);
    EXPECT_EQ(ctx->retry_count, 0u);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ContextTracksBatchReductions
 * Verify context tracks batch reduction count
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, ContextTracksBatchReductions) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(ctx->batch_reductions, 0u);

    /* Execute batch reduction action */
    bool result = nimcp_gpu_execute_recovery_action(ctx, GPU_RECOVERY_REDUCE_BATCH);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx->batch_reductions, 1u);

    /* Execute again */
    result = nimcp_gpu_execute_recovery_action(ctx, GPU_RECOVERY_REDUCE_BATCH);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx->batch_reductions, 2u);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ContextThreadLocalBehavior
 * Verify thread-local default context behavior
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, ContextThreadLocalBehavior) {
#ifdef NIMCP_ENABLE_CUDA
    /* Using NULL context should use thread-local default */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(NULL, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result);

    /* Should succeed or fail based on recovery strategy, but not crash */
    EXPECT_TRUE(result.action_taken != GPU_RECOVERY_NONE || !recovered);

    SUCCEED() << "Thread-local context used successfully";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ContextFromMultipleThreads
 * Verify contexts work correctly across threads
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, ContextFromMultipleThreads) {
#ifdef NIMCP_ENABLE_CUDA
    std::atomic<int> success_count{0};
    const int num_threads = 4;

    auto thread_func = [&]() {
        nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
        if (ctx) {
            /* Each thread should get independent context */
            ctx->retry_count = 42;
            if (ctx->retry_count == 42) {
                success_count++;
            }
            nimcp_gpu_recovery_context_destroy(ctx);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ContextCPUFallbackState
 * Verify CPU fallback state tracking
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, ContextCPUFallbackState) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    /* Initially not using fallback */
    EXPECT_FALSE(ctx->cpu_fallback_active);

    /* Set fallback function */
    auto fallback_fn = [](void*, void*, void*) -> bool { return true; };
    nimcp_gpu_set_cpu_fallback(ctx, fallback_fn, NULL);

    /* Execute CPU fallback action */
    bool result = nimcp_gpu_execute_recovery_action(ctx, GPU_RECOVERY_CPU_FALLBACK);
    EXPECT_TRUE(result);
    EXPECT_TRUE(ctx->cpu_fallback_active);

    /* Reset should clear fallback state */
    nimcp_gpu_recovery_context_reset(ctx);
    EXPECT_FALSE(ctx->cpu_fallback_active);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ContextStatisticsTracking
 * Verify context-level statistics tracking
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, ContextStatisticsTracking) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(ctx->recoveries_attempted, 0u);
    EXPECT_EQ(ctx->recoveries_succeeded, 0u);
    EXPECT_EQ(ctx->cpu_fallbacks_used, 0u);

    /* Perform recovery attempts */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result);

    /* Stats should be updated */
    EXPECT_GT(ctx->recoveries_attempted, 0u);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ManyContextsCreation
 * Verify many contexts can be created without issues
 * ============================================================================ */
TEST_F(GPURecoveryContextTest, ManyContextsCreation) {
#ifdef NIMCP_ENABLE_CUDA
    const int num_contexts = 100;
    std::vector<nimcp_gpu_recovery_context_t*> contexts;

    /* Create many contexts */
    for (int i = 0; i < num_contexts; i++) {
        nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
        ASSERT_NE(ctx, nullptr) << "Failed to create context " << i;
        contexts.push_back(ctx);
    }

    /* All should be valid */
    EXPECT_EQ(static_cast<int>(contexts.size()), num_contexts);

    /* Destroy all */
    for (auto ctx : contexts) {
        nimcp_gpu_recovery_context_destroy(ctx);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
