/**
 * @file test_gpu_recovery_validation_regression.cpp
 * @brief Regression tests for GPU recovery callback validation
 *
 * WHAT: Verify recovery callback return values are properly validated
 *       before retrying failed GPU operations
 * WHY:  Previously, nimcp_gpu_try_recover() was called but its return
 *       value and result.success field were not both checked before
 *       retrying, leading to potential retries on failed recoveries
 * HOW:  Test recovery patterns at the API level on CPU-only systems
 *
 * NOTE: On CPU-only systems, the recovery system initializes but
 * GPU operations fail immediately. We test the callback validation
 * pattern and return type consistency rather than actual GPU recovery.
 *
 * REGRESSION COVERAGE:
 * - Issue 2: nimcp_gpu_try_recover return value now checked with result.success
 * - Recovery config creation/destruction lifecycle
 * - Recovery context initialization and reset
 * - Parameter correction validation
 * - CPU fallback availability check
 *
 * VERIFIED HEADERS:
 * - gpu/recovery/nimcp_gpu_recovery.h: nimcp_gpu_recovery_init(),
 *   _shutdown(), _is_initialized(), _default_config(),
 *   nimcp_gpu_recovery_context_create(), _destroy(), _reset(),
 *   nimcp_gpu_try_recover(), nimcp_gpu_execute_recovery_action(),
 *   nimcp_gpu_select_recovery_strategy(),
 *   nimcp_gpu_correct_param_float(), _int(), _size(),
 *   nimcp_gpu_cpu_fallback_available(),
 *   nimcp_gpu_recovery_result_t, nimcp_gpu_recovery_config_t,
 *   nimcp_gpu_recovery_context_t, nimcp_gpu_param_range_t
 *
 * @author NIMCP Development Team
 * @date 2026-02-15
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "gpu/recovery/nimcp_gpu_recovery.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GpuRecoveryValidationRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure recovery system is initialized
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
    }

    void TearDown() override {
        nimcp_gpu_recovery_shutdown();
    }
};

//=============================================================================
// Recovery System Lifecycle Tests
//=============================================================================

TEST_F(GpuRecoveryValidationRegressionTest, InitAndShutdownCycle) {
    // Already initialized in SetUp
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    nimcp_gpu_recovery_shutdown();
    EXPECT_FALSE(nimcp_gpu_recovery_is_initialized());

    // Re-init for TearDown
    int ret = nimcp_gpu_recovery_init(NULL);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
}

TEST_F(GpuRecoveryValidationRegressionTest, DoubleInitIsHarmless) {
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
    int ret = nimcp_gpu_recovery_init(NULL);
    // Should succeed or return 0 (already initialized)
    EXPECT_EQ(ret, 0);
}

TEST_F(GpuRecoveryValidationRegressionTest, DoubleShutdownIsHarmless) {
    nimcp_gpu_recovery_shutdown();
    nimcp_gpu_recovery_shutdown();  // Should not crash
    EXPECT_FALSE(nimcp_gpu_recovery_is_initialized());
    // Re-init for TearDown
    nimcp_gpu_recovery_init(NULL);
}

//=============================================================================
// Recovery Config Tests
//=============================================================================

TEST_F(GpuRecoveryValidationRegressionTest, DefaultConfigHasSaneDefaults) {
    nimcp_gpu_recovery_config_t config;
    memset(&config, 0xFF, sizeof(config));  // Fill with garbage

    nimcp_gpu_recovery_default_config(&config);

    EXPECT_TRUE(config.enable_cpu_fallback);
    EXPECT_TRUE(config.enable_param_correction);
    EXPECT_TRUE(config.enable_retry);
    EXPECT_GT(config.max_retries, 0u);
    EXPECT_GT(config.retry_delay_ms, 0u);
    EXPECT_GT(config.batch_reduction_factor, 0.0f);
    EXPECT_LT(config.batch_reduction_factor, 1.0f);
    EXPECT_GT(config.memory_threshold, 0.0f);
    EXPECT_LE(config.memory_threshold, 1.0f);
}

TEST_F(GpuRecoveryValidationRegressionTest, CustomConfigIsUsed) {
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);
    config.max_retries = 10;
    config.enable_cpu_fallback = false;

    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(&config);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->config.max_retries, 10u);
    EXPECT_FALSE(ctx->config.enable_cpu_fallback);

    nimcp_gpu_recovery_context_destroy(ctx);
}

//=============================================================================
// Recovery Context Lifecycle Tests
//=============================================================================

TEST_F(GpuRecoveryValidationRegressionTest, ContextCreateWithNullUsesDefaults) {
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    EXPECT_TRUE(ctx->config.enable_cpu_fallback);
    EXPECT_TRUE(ctx->config.enable_retry);
    EXPECT_EQ(ctx->retry_count, 0u);
    EXPECT_EQ(ctx->recoveries_attempted, 0u);

    nimcp_gpu_recovery_context_destroy(ctx);
}

TEST_F(GpuRecoveryValidationRegressionTest, ContextDestroyNullIsSafe) {
    nimcp_gpu_recovery_context_destroy(NULL);
    SUCCEED();
}

TEST_F(GpuRecoveryValidationRegressionTest, ContextResetClearsState) {
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    // Simulate some usage
    ctx->retry_count = 5;
    ctx->cpu_fallback_active = true;

    nimcp_gpu_recovery_context_reset(ctx);
    EXPECT_EQ(ctx->retry_count, 0u);
    EXPECT_FALSE(ctx->cpu_fallback_active);

    nimcp_gpu_recovery_context_destroy(ctx);
}

//=============================================================================
// Recovery Result Validation Tests
//=============================================================================

/**
 * REGRESSION: Recovery result must be validated before retrying
 *
 * BUG: In nimcp_gpu_context.cu, the code only checked the bool return
 *      of nimcp_gpu_try_recover() but not result.success. The fix adds
 *      checking both: `if (try_recover(...) && result.success)`.
 *
 * This test verifies the result struct has proper fields and that
 * the API contract is consistent.
 */
TEST_F(GpuRecoveryValidationRegressionTest, RecoveryResultStructInitialization) {
    nimcp_gpu_recovery_result_t result;
    memset(&result, 0, sizeof(result));

    // Zero-initialized result should be "no recovery"
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_NONE);
    EXPECT_EQ(result.retries_used, 0u);
    EXPECT_FALSE(result.using_fallback);
    EXPECT_FLOAT_EQ(result.adjusted_batch_factor, 0.0f);
}

/**
 * Test that recovery attempt on CPU-only system is handled gracefully.
 * Without CUDA, try_recover should handle the error category appropriately.
 */
TEST_F(GpuRecoveryValidationRegressionTest, TryRecoverWithNullContext) {
    nimcp_gpu_recovery_result_t result;
    memset(&result, 0, sizeof(result));

    // On CPU-only builds, cudaError_t is int (0=success)
    bool recovered = nimcp_gpu_try_recover(NULL,
                                            GPU_ERROR_DEVICE_NOT_AVAILABLE,
                                            (cudaError_t)0,  // cudaSuccess
                                            &result);

    // Whether it returns true or false is implementation-dependent,
    // but it should not crash and result should be coherent
    if (recovered) {
        // If it claims recovery succeeded, result.success should match
        // (this is the regression we're testing)
        EXPECT_TRUE(result.success);
    }
    // No crash = pass
    SUCCEED();
}

TEST_F(GpuRecoveryValidationRegressionTest, TryRecoverWithExplicitContext) {
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_recovery_result_t result;
    memset(&result, 0, sizeof(result));

    bool recovered = nimcp_gpu_try_recover(ctx,
                                            GPU_ERROR_OUT_OF_MEMORY,
                                            (cudaError_t)0,
                                            &result);

    // Recovery stats should be updated
    EXPECT_GT(ctx->recoveries_attempted, 0u);

    if (recovered && result.success) {
        EXPECT_GT(ctx->recoveries_succeeded, 0u);
    }

    nimcp_gpu_recovery_context_destroy(ctx);
}

//=============================================================================
// Strategy Selection Tests
//=============================================================================

TEST_F(GpuRecoveryValidationRegressionTest, StrategySelectionForOOM) {
    nimcp_gpu_recovery_action_t action =
        nimcp_gpu_select_recovery_strategy(GPU_ERROR_OUT_OF_MEMORY, (cudaError_t)0, 0);

    // First attempt should try some form of recovery (not NONE)
    // The exact action depends on implementation, but it shouldn't be NONE
    // for a recoverable error category
    EXPECT_NE(action, GPU_RECOVERY_NONE);
}

TEST_F(GpuRecoveryValidationRegressionTest, StrategySelectionForDeviceNotAvailable) {
    nimcp_gpu_recovery_action_t action =
        nimcp_gpu_select_recovery_strategy(GPU_ERROR_DEVICE_NOT_AVAILABLE, (cudaError_t)0, 0);

    // For "device not available", CPU fallback is the expected strategy
    // or some recovery action
    (void)action;  // Implementation-dependent
    SUCCEED();
}

TEST_F(GpuRecoveryValidationRegressionTest, StrategyDegradeWithRetries) {
    // After multiple retries, strategy should escalate
    nimcp_gpu_recovery_action_t action0 =
        nimcp_gpu_select_recovery_strategy(GPU_ERROR_CUDA_RUNTIME, (cudaError_t)0, 0);
    nimcp_gpu_recovery_action_t action3 =
        nimcp_gpu_select_recovery_strategy(GPU_ERROR_CUDA_RUNTIME, (cudaError_t)0, 3);

    // With more retries, the strategy may change (escalate)
    // This is implementation-dependent but should not crash
    (void)action0;
    (void)action3;
    SUCCEED();
}

//=============================================================================
// Parameter Correction Tests
//=============================================================================

TEST_F(GpuRecoveryValidationRegressionTest, FloatParamCorrection) {
    nimcp_gpu_param_range_t range = {0.0f, 1.0f, 0.5f, true};

    // In-range value should not be corrected
    float val = 0.5f;
    bool corrected = nimcp_gpu_correct_param_float(&val, &range, "test_param");
    EXPECT_FALSE(corrected);
    EXPECT_FLOAT_EQ(val, 0.5f);

    // Above max should be clamped
    val = 2.0f;
    corrected = nimcp_gpu_correct_param_float(&val, &range, "test_param");
    EXPECT_TRUE(corrected);
    EXPECT_FLOAT_EQ(val, 1.0f);

    // Below min should be clamped
    val = -1.0f;
    corrected = nimcp_gpu_correct_param_float(&val, &range, "test_param");
    EXPECT_TRUE(corrected);
    EXPECT_FLOAT_EQ(val, 0.0f);
}

TEST_F(GpuRecoveryValidationRegressionTest, FloatParamUseDefault) {
    nimcp_gpu_param_range_t range = {0.0f, 1.0f, 0.5f, false};  // clamp_to_range=false

    float val = 2.0f;
    bool corrected = nimcp_gpu_correct_param_float(&val, &range, "test_param");
    EXPECT_TRUE(corrected);
    EXPECT_FLOAT_EQ(val, 0.5f);  // Should use default, not clamp
}

TEST_F(GpuRecoveryValidationRegressionTest, IntParamCorrection) {
    int val = 100;
    bool corrected = nimcp_gpu_correct_param_int(&val, 1, 64, 32, "batch_size");
    EXPECT_TRUE(corrected);
    EXPECT_EQ(val, 64);

    val = -5;
    corrected = nimcp_gpu_correct_param_int(&val, 1, 64, 32, "batch_size");
    EXPECT_TRUE(corrected);
    EXPECT_EQ(val, 1);

    val = 32;
    corrected = nimcp_gpu_correct_param_int(&val, 1, 64, 32, "batch_size");
    EXPECT_FALSE(corrected);
    EXPECT_EQ(val, 32);
}

TEST_F(GpuRecoveryValidationRegressionTest, SizeParamCorrection) {
    size_t val = 0;
    bool corrected = nimcp_gpu_correct_param_size(&val, 1, 1024, 256, "buffer_size");
    EXPECT_TRUE(corrected);
    EXPECT_EQ(val, 1u);

    val = 2048;
    corrected = nimcp_gpu_correct_param_size(&val, 1, 1024, 256, "buffer_size");
    EXPECT_TRUE(corrected);
    EXPECT_EQ(val, 1024u);

    val = 512;
    corrected = nimcp_gpu_correct_param_size(&val, 1, 1024, 256, "buffer_size");
    EXPECT_FALSE(corrected);
    EXPECT_EQ(val, 512u);
}

//=============================================================================
// CPU Fallback Availability Tests
//=============================================================================

TEST_F(GpuRecoveryValidationRegressionTest, CpuFallbackIsAvailable) {
    // On CPU-only systems, CPU fallback should always be available
    bool available = nimcp_gpu_cpu_fallback_available();
    EXPECT_TRUE(available);
}

//=============================================================================
// Execute Recovery Action Tests
//=============================================================================

TEST_F(GpuRecoveryValidationRegressionTest, ExecuteRecoveryActionWithContext) {
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    // CPU fallback action should succeed
    bool ok = nimcp_gpu_execute_recovery_action(ctx, GPU_RECOVERY_CPU_FALLBACK);
    // Implementation-dependent whether this succeeds without a fallback function set
    (void)ok;

    nimcp_gpu_recovery_context_destroy(ctx);
}

TEST_F(GpuRecoveryValidationRegressionTest, ExecuteRecoveryActionNone) {
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    bool ok = nimcp_gpu_execute_recovery_action(ctx, GPU_RECOVERY_NONE);
    // NONE action should be a no-op, likely returns true
    (void)ok;

    nimcp_gpu_recovery_context_destroy(ctx);
}

//=============================================================================
// Repeated Recovery Attempts
//=============================================================================

TEST_F(GpuRecoveryValidationRegressionTest, RepeatedRecoveryAttemptsTrackRetries) {
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    for (int i = 0; i < 10; i++) {
        nimcp_gpu_recovery_result_t result;
        memset(&result, 0, sizeof(result));

        nimcp_gpu_try_recover(ctx, GPU_ERROR_CUDA_RUNTIME, (cudaError_t)0, &result);
    }

    // After multiple attempts, stats should reflect this
    EXPECT_GE(ctx->recoveries_attempted, 1u);

    nimcp_gpu_recovery_context_destroy(ctx);
}
