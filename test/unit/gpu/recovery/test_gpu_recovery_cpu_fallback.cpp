/* ============================================================================
 * Unit Tests: GPU Recovery CPU Fallback
 * ============================================================================
 * WHAT: Unit tests for CPU fallback functionality
 * WHY:  Validate CPU fallback registration and execution
 * HOW:  Test fallback registration, execution, and result matching
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include <cuda_runtime.h>
#endif

namespace {

/* Test data structures for fallback testing */
struct FallbackParams {
    float input;
    int operation;
};

struct FallbackResult {
    float output;
    bool success;
};

/* Global state for tracking fallback execution */
static int g_fallback_call_count = 0;
static void* g_last_fallback_context = nullptr;

/* Test fallback function */
static bool test_fallback_fn(void* context, void* params, void* result) {
    g_fallback_call_count++;
    g_last_fallback_context = context;

    if (!params || !result) return false;

    FallbackParams* p = static_cast<FallbackParams*>(params);
    FallbackResult* r = static_cast<FallbackResult*>(result);

    /* Simple computation */
    r->output = p->input * 2.0f;
    r->success = true;

    return true;
}

/* Failing fallback function */
static bool failing_fallback_fn(void* context, void* params, void* result) {
    (void)context;
    (void)params;
    (void)result;
    return false;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */
class GPURecoveryCPUFallbackTest : public ::testing::Test {
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
        /* Reset global state */
        g_fallback_call_count = 0;
        g_last_fallback_context = nullptr;
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
 * Test: FallbackAvailableByDefault
 * Verify CPU fallback is always available
 * ============================================================================ */
TEST_F(GPURecoveryCPUFallbackTest, FallbackAvailableByDefault) {
#ifdef NIMCP_ENABLE_CUDA
    bool available = nimcp_gpu_cpu_fallback_available();
    EXPECT_TRUE(available) << "CPU fallback should always be available";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: FallbackEnabledByDefault
 * Verify CPU fallback is enabled in default config
 * ============================================================================ */
TEST_F(GPURecoveryCPUFallbackTest, FallbackEnabledByDefault) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);

    EXPECT_TRUE(config.enable_cpu_fallback) << "CPU fallback should be enabled by default";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: FallbackCanBeDisabled
 * Verify CPU fallback can be disabled in config
 * ============================================================================ */
TEST_F(GPURecoveryCPUFallbackTest, FallbackCanBeDisabled) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);
    config.enable_cpu_fallback = false;

    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    EXPECT_FALSE(ctx->config.enable_cpu_fallback);

    /* Set fallback function */
    nimcp_gpu_set_cpu_fallback(ctx, test_fallback_fn, NULL);

    /* CPU fallback action should fail when disabled */
    bool result = nimcp_gpu_execute_recovery_action(ctx, GPU_RECOVERY_CPU_FALLBACK);
    EXPECT_FALSE(result) << "CPU fallback should fail when disabled";

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: FallbackRegistration
 * Verify fallback function registration
 * ============================================================================ */
TEST_F(GPURecoveryCPUFallbackTest, FallbackRegistration) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    /* Initially no fallback */
    EXPECT_EQ(ctx->cpu_fallback_fn, nullptr);
    EXPECT_EQ(ctx->cpu_fallback_context, nullptr);

    /* Register fallback */
    int user_context = 42;
    nimcp_gpu_set_cpu_fallback(ctx, test_fallback_fn, &user_context);

    /* Verify registration */
    EXPECT_NE(ctx->cpu_fallback_fn, nullptr);
    EXPECT_EQ(ctx->cpu_fallback_context, &user_context);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: FallbackExecutesCorrectFunction
 * Verify registered fallback is called correctly
 * ============================================================================ */
TEST_F(GPURecoveryCPUFallbackTest, FallbackExecutesCorrectFunction) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    int user_context = 123;
    nimcp_gpu_set_cpu_fallback(ctx, test_fallback_fn, &user_context);

    FallbackParams params = {5.0f, 1};
    FallbackResult result = {0.0f, false};

    bool success = nimcp_gpu_execute_cpu_fallback(ctx, &params, &result);

    EXPECT_TRUE(success) << "Fallback execution should succeed";
    EXPECT_EQ(g_fallback_call_count, 1) << "Fallback should be called once";
    EXPECT_EQ(g_last_fallback_context, &user_context) << "Context should be passed";
    EXPECT_FLOAT_EQ(result.output, 10.0f) << "Result should be input * 2";
    EXPECT_TRUE(result.success);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: FallbackTriggeredOnDeviceUnavailable
 * Verify CPU fallback is triggered for device unavailable error
 * ============================================================================ */
TEST_F(GPURecoveryCPUFallbackTest, FallbackTriggeredOnDeviceUnavailable) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, test_fallback_fn, NULL);

    /* Device unavailable should trigger immediate CPU fallback */
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_DEVICE_NOT_AVAILABLE, cudaErrorNoDevice, 0);
    EXPECT_EQ(action, GPU_RECOVERY_CPU_FALLBACK);

    /* Execute the recovery */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx, GPU_ERROR_DEVICE_NOT_AVAILABLE, cudaErrorNoDevice, &result);

    /* Should succeed with CPU fallback */
    EXPECT_TRUE(recovered);
    EXPECT_TRUE(ctx->cpu_fallback_active);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: FallbackWithoutRegistration
 * Verify fallback execution without registration fails gracefully
 * ============================================================================ */
TEST_F(GPURecoveryCPUFallbackTest, FallbackWithoutRegistration) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    /* Don't register fallback */
    FallbackParams params = {5.0f, 1};
    FallbackResult result = {0.0f, false};

    bool success = nimcp_gpu_execute_cpu_fallback(ctx, &params, &result);
    EXPECT_FALSE(success) << "Should fail without registered fallback";

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: FallbackActionWithoutFunction
 * Verify recovery action fails when no fallback function is set
 * ============================================================================ */
TEST_F(GPURecoveryCPUFallbackTest, FallbackActionWithoutFunction) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    /* CPU fallback action should fail without registered function */
    bool result = nimcp_gpu_execute_recovery_action(ctx, GPU_RECOVERY_CPU_FALLBACK);
    EXPECT_FALSE(result);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: FailingFallbackHandled
 * Verify failing fallback function is handled correctly
 * ============================================================================ */
TEST_F(GPURecoveryCPUFallbackTest, FailingFallbackHandled) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, failing_fallback_fn, NULL);

    FallbackParams params = {5.0f, 1};
    FallbackResult result = {0.0f, false};

    bool success = nimcp_gpu_execute_cpu_fallback(ctx, &params, &result);
    EXPECT_FALSE(success) << "Failing fallback should return false";

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: FallbackWithNullParams
 * Verify fallback handles NULL params gracefully
 * ============================================================================ */
TEST_F(GPURecoveryCPUFallbackTest, FallbackWithNullParams) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, test_fallback_fn, NULL);

    FallbackResult result = {0.0f, false};

    bool success = nimcp_gpu_execute_cpu_fallback(ctx, NULL, &result);
    EXPECT_FALSE(success) << "Fallback should fail with NULL params";

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: FallbackStatsTracking
 * Verify fallback usage is tracked in statistics
 * ============================================================================ */
TEST_F(GPURecoveryCPUFallbackTest, FallbackStatsTracking) {
#ifdef NIMCP_ENABLE_CUDA
    /* Reset stats */
    nimcp_gpu_recovery_reset_stats();

    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, test_fallback_fn, NULL);

    /* Execute CPU fallback action */
    nimcp_gpu_execute_recovery_action(ctx, GPU_RECOVERY_CPU_FALLBACK);

    /* Check stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    EXPECT_GT(stats.cpu_fallbacks_used, 0u) << "CPU fallback usage should be tracked";

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SetFallbackOnNullContext
 * Verify setting fallback on NULL context is handled
 * ============================================================================ */
TEST_F(GPURecoveryCPUFallbackTest, SetFallbackOnNullContext) {
#ifdef NIMCP_ENABLE_CUDA
    /* Should not crash */
    nimcp_gpu_set_cpu_fallback(NULL, test_fallback_fn, NULL);
    SUCCEED() << "Setting fallback on NULL context handled gracefully";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ExecuteFallbackOnNullContext
 * Verify executing fallback on NULL context fails gracefully
 * ============================================================================ */
TEST_F(GPURecoveryCPUFallbackTest, ExecuteFallbackOnNullContext) {
#ifdef NIMCP_ENABLE_CUDA
    FallbackParams params = {5.0f, 1};
    FallbackResult result = {0.0f, false};

    bool success = nimcp_gpu_execute_cpu_fallback(NULL, &params, &result);
    EXPECT_FALSE(success) << "Fallback execution should fail on NULL context";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
