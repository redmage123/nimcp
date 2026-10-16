/* ============================================================================
 * Unit Tests: GPU Recovery Initialization
 * ============================================================================
 * WHAT: Unit tests for GPU recovery initialization/shutdown API
 * WHY:  Validate lifecycle management of recovery system
 * HOW:  Test init, shutdown, reinit, and state queries
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include <cuda_runtime.h>
#endif

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */
class GPURecoveryInitTest : public ::testing::Test {
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
 * Test: InitializeSucceeds
 * Verify basic initialization succeeds
 * ============================================================================ */
TEST_F(GPURecoveryInitTest, InitializeSucceeds) {
#ifdef NIMCP_ENABLE_CUDA
    EXPECT_FALSE(nimcp_gpu_recovery_is_initialized()) << "Should not be initialized before init";

    int result = nimcp_gpu_recovery_init(NULL);
    EXPECT_EQ(result, 0) << "Init should return 0 on success";

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized()) << "Should be initialized after init";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: DoubleInitializeHandled
 * Verify double initialization is handled gracefully
 * ============================================================================ */
TEST_F(GPURecoveryInitTest, DoubleInitializeHandled) {
#ifdef NIMCP_ENABLE_CUDA
    /* First init */
    ASSERT_EQ(nimcp_gpu_recovery_init(NULL), 0);
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    /* Second init should succeed (idempotent) */
    int result = nimcp_gpu_recovery_init(NULL);
    EXPECT_EQ(result, 0) << "Double init should succeed (idempotent)";

    /* Should still be initialized */
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ShutdownSucceeds
 * Verify shutdown after init succeeds
 * ============================================================================ */
TEST_F(GPURecoveryInitTest, ShutdownSucceeds) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_EQ(nimcp_gpu_recovery_init(NULL), 0);
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    /* Shutdown should work */
    nimcp_gpu_recovery_shutdown();

    EXPECT_FALSE(nimcp_gpu_recovery_is_initialized()) << "Should not be initialized after shutdown";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ShutdownWithoutInitHandled
 * Verify shutdown without init is handled gracefully
 * ============================================================================ */
TEST_F(GPURecoveryInitTest, ShutdownWithoutInitHandled) {
#ifdef NIMCP_ENABLE_CUDA
    EXPECT_FALSE(nimcp_gpu_recovery_is_initialized());

    /* Shutdown without init should not crash */
    nimcp_gpu_recovery_shutdown();

    EXPECT_FALSE(nimcp_gpu_recovery_is_initialized());
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: DoubleShutdownHandled
 * Verify double shutdown is handled gracefully
 * ============================================================================ */
TEST_F(GPURecoveryInitTest, DoubleShutdownHandled) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_EQ(nimcp_gpu_recovery_init(NULL), 0);

    /* First shutdown */
    nimcp_gpu_recovery_shutdown();
    EXPECT_FALSE(nimcp_gpu_recovery_is_initialized());

    /* Second shutdown should not crash */
    nimcp_gpu_recovery_shutdown();
    EXPECT_FALSE(nimcp_gpu_recovery_is_initialized());
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ReinitializeAfterShutdown
 * Verify system can be reinitialized after shutdown
 * ============================================================================ */
TEST_F(GPURecoveryInitTest, ReinitializeAfterShutdown) {
#ifdef NIMCP_ENABLE_CUDA
    /* First init */
    ASSERT_EQ(nimcp_gpu_recovery_init(NULL), 0);
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    /* Shutdown */
    nimcp_gpu_recovery_shutdown();
    EXPECT_FALSE(nimcp_gpu_recovery_is_initialized());

    /* Reinit should succeed */
    ASSERT_EQ(nimcp_gpu_recovery_init(NULL), 0);
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    /* Verify it works */
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr) << "Context creation should work after reinit";
    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: IsInitializedReturnsCorrectState
 * Verify is_initialized returns correct state at all points
 * ============================================================================ */
TEST_F(GPURecoveryInitTest, IsInitializedReturnsCorrectState) {
#ifdef NIMCP_ENABLE_CUDA
    /* Initially not initialized */
    EXPECT_FALSE(nimcp_gpu_recovery_is_initialized());

    /* After init */
    ASSERT_EQ(nimcp_gpu_recovery_init(NULL), 0);
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    /* After shutdown */
    nimcp_gpu_recovery_shutdown();
    EXPECT_FALSE(nimcp_gpu_recovery_is_initialized());

    /* After reinit */
    ASSERT_EQ(nimcp_gpu_recovery_init(NULL), 0);
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: InitWithCustomConfig
 * Verify initialization with custom config
 * ============================================================================ */
TEST_F(GPURecoveryInitTest, InitWithCustomConfig) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);

    /* Set custom values */
    config.max_retries = 10;
    config.retry_delay_ms = 100;
    config.enable_cpu_fallback = false;
    config.batch_reduction_factor = 0.25f;

    ASSERT_EQ(nimcp_gpu_recovery_init(&config), 0);
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    /* Verify config is applied */
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(ctx->config.max_retries, 10u);
    EXPECT_EQ(ctx->config.retry_delay_ms, 100u);
    EXPECT_FALSE(ctx->config.enable_cpu_fallback);
    EXPECT_FLOAT_EQ(ctx->config.batch_reduction_factor, 0.25f);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: InitResetsStats
 * Verify that init resets statistics
 * ============================================================================ */
TEST_F(GPURecoveryInitTest, InitResetsStats) {
#ifdef NIMCP_ENABLE_CUDA
    /* First init */
    ASSERT_EQ(nimcp_gpu_recovery_init(NULL), 0);

    /* Generate some activity to populate stats */
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    /* Simulate a recovery attempt */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result);

    nimcp_gpu_recovery_context_destroy(ctx);

    /* Check stats have some data */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GT(stats.recoveries_attempted, 0u);

    /* Shutdown and reinit */
    nimcp_gpu_recovery_shutdown();
    ASSERT_EQ(nimcp_gpu_recovery_init(NULL), 0);

    /* Stats should be reset */
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_EQ(stats.recoveries_attempted, 0u);
    EXPECT_EQ(stats.recoveries_succeeded, 0u);
    EXPECT_EQ(stats.total_errors, 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ContextCreationBeforeInit
 * Verify context can be created even before explicit init (auto-init)
 * ============================================================================ */
TEST_F(GPURecoveryInitTest, ContextCreationBeforeInit) {
#ifdef NIMCP_ENABLE_CUDA
    EXPECT_FALSE(nimcp_gpu_recovery_is_initialized());

    /* Create context before init - should use defaults */
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    /* Should have default config */
    EXPECT_EQ(ctx->config.max_retries, 3u);
    EXPECT_TRUE(ctx->config.enable_cpu_fallback);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: MultipleInitShutdownCycles
 * Verify multiple init/shutdown cycles work correctly
 * ============================================================================ */
TEST_F(GPURecoveryInitTest, MultipleInitShutdownCycles) {
#ifdef NIMCP_ENABLE_CUDA
    for (int cycle = 0; cycle < 5; cycle++) {
        nimcp_gpu_recovery_config_t config;
        nimcp_gpu_recovery_default_config(&config);
        config.max_retries = cycle + 1;

        ASSERT_EQ(nimcp_gpu_recovery_init(&config), 0) << "Cycle " << cycle;
        EXPECT_TRUE(nimcp_gpu_recovery_is_initialized()) << "Cycle " << cycle;

        nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
        ASSERT_NE(ctx, nullptr) << "Cycle " << cycle;
        EXPECT_EQ(ctx->config.max_retries, static_cast<uint32_t>(cycle + 1)) << "Cycle " << cycle;
        nimcp_gpu_recovery_context_destroy(ctx);

        nimcp_gpu_recovery_shutdown();
        EXPECT_FALSE(nimcp_gpu_recovery_is_initialized()) << "Cycle " << cycle;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
