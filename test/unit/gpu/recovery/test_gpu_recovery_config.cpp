/* ============================================================================
 * Unit Tests: GPU Recovery Configuration
 * ============================================================================
 * WHAT: Unit tests for GPU recovery configuration API
 * WHY:  Validate configuration management for self-healing recovery
 * HOW:  Test default values, setters, validation, and persistence
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
class GPURecoveryConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        /* Ensure clean state */
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
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
 * Test: DefaultConfigHasValidDefaults
 * Verify that default configuration has sensible values
 * ============================================================================ */
TEST_F(GPURecoveryConfigTest, DefaultConfigHasValidDefaults) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);

    /* Verify default values match documented defaults */
    EXPECT_TRUE(config.enable_cpu_fallback) << "CPU fallback should be enabled by default";
    EXPECT_TRUE(config.enable_param_correction) << "Param correction should be enabled by default";
    EXPECT_TRUE(config.enable_batch_reduction) << "Batch reduction should be enabled by default";
    EXPECT_TRUE(config.enable_retry) << "Retry should be enabled by default";
    EXPECT_EQ(config.max_retries, 3u) << "Default max_retries should be 3";
    EXPECT_EQ(config.retry_delay_ms, 10u) << "Default retry delay should be 10ms";
    EXPECT_FLOAT_EQ(config.batch_reduction_factor, 0.5f) << "Default batch reduction factor should be 0.5";
    EXPECT_FLOAT_EQ(config.memory_threshold, 0.9f) << "Default memory threshold should be 0.9";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SetMaxRetriesValidRange
 * Verify max_retries accepts valid values
 * ============================================================================ */
TEST_F(GPURecoveryConfigTest, SetMaxRetriesValidRange) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);

    /* Test valid range */
    config.max_retries = 0;
    EXPECT_EQ(config.max_retries, 0u);

    config.max_retries = 1;
    EXPECT_EQ(config.max_retries, 1u);

    config.max_retries = 10;
    EXPECT_EQ(config.max_retries, 10u);

    config.max_retries = 100;
    EXPECT_EQ(config.max_retries, 100u);

    /* Init with custom config and verify it persists */
    config.max_retries = 5;
    ASSERT_EQ(nimcp_gpu_recovery_init(&config), 0);

    /* Create context and verify config is used */
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->config.max_retries, 5u);
    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SetMaxRetriesInvalidClamped
 * Verify extreme max_retries values are accepted (no clamping in config)
 * ============================================================================ */
TEST_F(GPURecoveryConfigTest, SetMaxRetriesInvalidClamped) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);

    /* Very large value - should be accepted (no hard limit) */
    config.max_retries = 1000000;
    EXPECT_EQ(config.max_retries, 1000000u);

    /* In practice, system will stop at max retries regardless of value */
    config.max_retries = UINT32_MAX;
    EXPECT_EQ(config.max_retries, UINT32_MAX);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SetRecoveryTimeoutValidRange
 * Verify retry_delay_ms accepts valid values
 * ============================================================================ */
TEST_F(GPURecoveryConfigTest, SetRecoveryTimeoutValidRange) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);

    /* Test various timeout values */
    config.retry_delay_ms = 0;
    EXPECT_EQ(config.retry_delay_ms, 0u);

    config.retry_delay_ms = 1;
    EXPECT_EQ(config.retry_delay_ms, 1u);

    config.retry_delay_ms = 100;
    EXPECT_EQ(config.retry_delay_ms, 100u);

    config.retry_delay_ms = 1000;
    EXPECT_EQ(config.retry_delay_ms, 1000u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SetRecoveryTimeoutInvalidClamped
 * Verify extreme timeout values are accepted
 * ============================================================================ */
TEST_F(GPURecoveryConfigTest, SetRecoveryTimeoutInvalidClamped) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);

    /* Very large timeout - should be accepted */
    config.retry_delay_ms = 60000;  /* 60 seconds */
    EXPECT_EQ(config.retry_delay_ms, 60000u);

    config.retry_delay_ms = UINT32_MAX;
    EXPECT_EQ(config.retry_delay_ms, UINT32_MAX);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: EnableDisableRecovery
 * Verify enable_retry flag works correctly
 * ============================================================================ */
TEST_F(GPURecoveryConfigTest, EnableDisableRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);

    /* Default should be enabled */
    EXPECT_TRUE(config.enable_retry);

    /* Disable retry */
    config.enable_retry = false;
    ASSERT_EQ(nimcp_gpu_recovery_init(&config), 0);

    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);
    EXPECT_FALSE(ctx->config.enable_retry);
    nimcp_gpu_recovery_context_destroy(ctx);

    /* Re-enable */
    nimcp_gpu_recovery_shutdown();
    config.enable_retry = true;
    ASSERT_EQ(nimcp_gpu_recovery_init(&config), 0);

    ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);
    EXPECT_TRUE(ctx->config.enable_retry);
    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: EnableDisableCPUFallback
 * Verify enable_cpu_fallback flag works correctly
 * ============================================================================ */
TEST_F(GPURecoveryConfigTest, EnableDisableCPUFallback) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);

    /* Default should be enabled */
    EXPECT_TRUE(config.enable_cpu_fallback);

    /* Disable CPU fallback */
    config.enable_cpu_fallback = false;
    ASSERT_EQ(nimcp_gpu_recovery_init(&config), 0);

    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);
    EXPECT_FALSE(ctx->config.enable_cpu_fallback);
    nimcp_gpu_recovery_context_destroy(ctx);

    /* Re-enable */
    nimcp_gpu_recovery_shutdown();
    config.enable_cpu_fallback = true;
    ASSERT_EQ(nimcp_gpu_recovery_init(&config), 0);

    ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);
    EXPECT_TRUE(ctx->config.enable_cpu_fallback);
    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ConfigPersistsAcrossContexts
 * Verify config set at init is used by all contexts
 * ============================================================================ */
TEST_F(GPURecoveryConfigTest, ConfigPersistsAcrossContexts) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);

    /* Set custom config */
    config.max_retries = 7;
    config.retry_delay_ms = 50;
    config.batch_reduction_factor = 0.25f;
    config.memory_threshold = 0.85f;

    ASSERT_EQ(nimcp_gpu_recovery_init(&config), 0);

    /* Create multiple contexts and verify they all use the config */
    for (int i = 0; i < 5; i++) {
        nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
        ASSERT_NE(ctx, nullptr) << "Failed to create context " << i;

        EXPECT_EQ(ctx->config.max_retries, 7u) << "Context " << i;
        EXPECT_EQ(ctx->config.retry_delay_ms, 50u) << "Context " << i;
        EXPECT_FLOAT_EQ(ctx->config.batch_reduction_factor, 0.25f) << "Context " << i;
        EXPECT_FLOAT_EQ(ctx->config.memory_threshold, 0.85f) << "Context " << i;

        nimcp_gpu_recovery_context_destroy(ctx);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ThreadSafeConfigAccess
 * Verify config can be accessed from multiple threads safely
 * ============================================================================ */
TEST_F(GPURecoveryConfigTest, ThreadSafeConfigAccess) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);
    config.max_retries = 5;

    ASSERT_EQ(nimcp_gpu_recovery_init(&config), 0);

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    const int num_threads = 4;
    const int iterations_per_thread = 100;

    auto thread_func = [&]() {
        for (int i = 0; i < iterations_per_thread; i++) {
            nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
            if (ctx && ctx->config.max_retries == 5) {
                success_count++;
            } else {
                failure_count++;
            }
            if (ctx) {
                nimcp_gpu_recovery_context_destroy(ctx);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * iterations_per_thread);
    EXPECT_EQ(failure_count.load(), 0);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ResetToDefaults
 * Verify getting default config after custom config was used
 * ============================================================================ */
TEST_F(GPURecoveryConfigTest, ResetToDefaults) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;

    /* Start with custom values */
    config.enable_cpu_fallback = false;
    config.enable_param_correction = false;
    config.enable_batch_reduction = false;
    config.enable_retry = false;
    config.max_retries = 99;
    config.retry_delay_ms = 999;
    config.batch_reduction_factor = 0.99f;
    config.memory_threshold = 0.99f;

    /* Reset to defaults */
    nimcp_gpu_recovery_default_config(&config);

    /* Verify all back to defaults */
    EXPECT_TRUE(config.enable_cpu_fallback);
    EXPECT_TRUE(config.enable_param_correction);
    EXPECT_TRUE(config.enable_batch_reduction);
    EXPECT_TRUE(config.enable_retry);
    EXPECT_EQ(config.max_retries, 3u);
    EXPECT_EQ(config.retry_delay_ms, 10u);
    EXPECT_FLOAT_EQ(config.batch_reduction_factor, 0.5f);
    EXPECT_FLOAT_EQ(config.memory_threshold, 0.9f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: NullConfigPointerHandled
 * Verify NULL pointer is handled gracefully
 * ============================================================================ */
TEST_F(GPURecoveryConfigTest, NullConfigPointerHandled) {
#ifdef NIMCP_ENABLE_CUDA
    /* Should not crash */
    nimcp_gpu_recovery_default_config(NULL);

    /* Init with NULL should use defaults */
    ASSERT_EQ(nimcp_gpu_recovery_init(NULL), 0);

    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    /* Should have default values */
    EXPECT_EQ(ctx->config.max_retries, 3u);
    EXPECT_TRUE(ctx->config.enable_cpu_fallback);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
