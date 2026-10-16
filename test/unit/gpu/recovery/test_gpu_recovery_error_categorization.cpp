/* ============================================================================
 * Unit Tests: GPU Recovery Error Categorization
 * ============================================================================
 * WHAT: Unit tests for GPU error categorization and naming
 * WHY:  Validate correct mapping of CUDA errors to recovery categories
 * HOW:  Test all error categories and CUDA error mappings
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cstring>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include <cuda_runtime.h>
#endif

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */
class GPURecoveryErrorCategorizationTest : public ::testing::Test {
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
 * Test: CategorizeOutOfMemory
 * Verify OOM errors are categorized correctly
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, CategorizeOutOfMemory) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_error_category_t cat;

    cat = nimcp_gpu_categorize_cuda_error(cudaErrorMemoryAllocation);
    EXPECT_EQ(cat, GPU_ERROR_OUT_OF_MEMORY);

    cat = nimcp_gpu_categorize_cuda_error(cudaErrorLaunchOutOfResources);
    EXPECT_EQ(cat, GPU_ERROR_OUT_OF_MEMORY);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CategorizeInvalidDevice
 * Verify device-related errors are categorized correctly
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, CategorizeInvalidDevice) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_error_category_t cat;

    cat = nimcp_gpu_categorize_cuda_error(cudaErrorNoDevice);
    EXPECT_EQ(cat, GPU_ERROR_DEVICE_NOT_AVAILABLE);

    cat = nimcp_gpu_categorize_cuda_error(cudaErrorInvalidDevice);
    EXPECT_EQ(cat, GPU_ERROR_DEVICE_NOT_AVAILABLE);

    cat = nimcp_gpu_categorize_cuda_error(cudaErrorDeviceUninitialized);
    EXPECT_EQ(cat, GPU_ERROR_DEVICE_NOT_AVAILABLE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CategorizeInvalidValue
 * Verify invalid parameter errors are categorized correctly
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, CategorizeInvalidValue) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_error_category_t cat;

    cat = nimcp_gpu_categorize_cuda_error(cudaErrorInvalidValue);
    EXPECT_EQ(cat, GPU_ERROR_INVALID_PARAMS);

    cat = nimcp_gpu_categorize_cuda_error(cudaErrorInvalidDevicePointer);
    EXPECT_EQ(cat, GPU_ERROR_INVALID_PARAMS);

    cat = nimcp_gpu_categorize_cuda_error(cudaErrorInvalidMemcpyDirection);
    EXPECT_EQ(cat, GPU_ERROR_INVALID_PARAMS);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CategorizeKernelLaunch
 * Verify kernel launch errors are categorized correctly
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, CategorizeKernelLaunch) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_error_category_t cat;

    cat = nimcp_gpu_categorize_cuda_error(cudaErrorLaunchFailure);
    EXPECT_EQ(cat, GPU_ERROR_KERNEL_LAUNCH);

    cat = nimcp_gpu_categorize_cuda_error(cudaErrorLaunchTimeout);
    EXPECT_EQ(cat, GPU_ERROR_KERNEL_LAUNCH);

    cat = nimcp_gpu_categorize_cuda_error(cudaErrorInvalidConfiguration);
    EXPECT_EQ(cat, GPU_ERROR_KERNEL_LAUNCH);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CategorizeLaunchFailure
 * Verify launch failure is categorized as kernel launch error
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, CategorizeLaunchFailure) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_error_category_t cat = nimcp_gpu_categorize_cuda_error(cudaErrorLaunchFailure);
    EXPECT_EQ(cat, GPU_ERROR_KERNEL_LAUNCH);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CategorizeCudaRuntime
 * Verify runtime errors are categorized correctly
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, CategorizeCudaRuntime) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_error_category_t cat;

    cat = nimcp_gpu_categorize_cuda_error(cudaErrorCudartUnloading);
    EXPECT_EQ(cat, GPU_ERROR_CUDA_RUNTIME);

    cat = nimcp_gpu_categorize_cuda_error(cudaErrorInitializationError);
    EXPECT_EQ(cat, GPU_ERROR_CUDA_RUNTIME);

    cat = nimcp_gpu_categorize_cuda_error(cudaErrorInsufficientDriver);
    EXPECT_EQ(cat, GPU_ERROR_CUDA_RUNTIME);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CategorizeUnknownError
 * Verify success returns unknown category
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, CategorizeUnknownError) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_error_category_t cat = nimcp_gpu_categorize_cuda_error(cudaSuccess);
    EXPECT_EQ(cat, GPU_ERROR_UNKNOWN);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ErrorCategoryToString
 * Verify all error categories have valid string names
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, ErrorCategoryToString) {
#ifdef NIMCP_ENABLE_CUDA
    /* Test all defined categories */
    EXPECT_STREQ(nimcp_gpu_error_category_name(GPU_ERROR_UNKNOWN), "UNKNOWN");
    EXPECT_STREQ(nimcp_gpu_error_category_name(GPU_ERROR_INVALID_PARAMS), "INVALID_PARAMS");
    EXPECT_STREQ(nimcp_gpu_error_category_name(GPU_ERROR_OUT_OF_MEMORY), "OUT_OF_MEMORY");
    EXPECT_STREQ(nimcp_gpu_error_category_name(GPU_ERROR_CUDA_RUNTIME), "CUDA_RUNTIME");
    EXPECT_STREQ(nimcp_gpu_error_category_name(GPU_ERROR_KERNEL_LAUNCH), "KERNEL_LAUNCH");
    EXPECT_STREQ(nimcp_gpu_error_category_name(GPU_ERROR_NUMERICAL), "NUMERICAL");
    EXPECT_STREQ(nimcp_gpu_error_category_name(GPU_ERROR_TIMEOUT), "TIMEOUT");
    EXPECT_STREQ(nimcp_gpu_error_category_name(GPU_ERROR_CONTEXT_INVALID), "CONTEXT_INVALID");
    EXPECT_STREQ(nimcp_gpu_error_category_name(GPU_ERROR_LIBRARY), "LIBRARY");
    EXPECT_STREQ(nimcp_gpu_error_category_name(GPU_ERROR_DEVICE_NOT_AVAILABLE), "DEVICE_NOT_AVAILABLE");

    /* Invalid category should return "UNKNOWN" */
    EXPECT_STREQ(nimcp_gpu_error_category_name(static_cast<nimcp_gpu_error_category_t>(999)), "UNKNOWN");
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoveryActionToString
 * Verify all recovery actions have valid string names
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, RecoveryActionToString) {
#ifdef NIMCP_ENABLE_CUDA
    /* Test all defined actions */
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_NONE), "NONE");
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_CLAMP_PARAMS), "CLAMP_PARAMS");
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_SET_DEFAULTS), "SET_DEFAULTS");
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_VALIDATE_FIX), "VALIDATE_FIX");
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_REDUCE_BATCH), "REDUCE_BATCH");
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_REDUCE_DIMENSIONS), "REDUCE_DIMENSIONS");
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_REDUCE_PRECISION), "REDUCE_PRECISION");
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_FREE_CACHE), "FREE_CACHE");
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_RESET_DEVICE), "RESET_DEVICE");
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_CPU_FALLBACK), "CPU_FALLBACK");
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_ASYNC_SPLIT), "ASYNC_SPLIT");
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_STREAM_SYNC), "STREAM_SYNC");
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_RETRY_IMMEDIATE), "RETRY_IMMEDIATE");
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_RETRY_BACKOFF), "RETRY_BACKOFF");
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(GPU_RECOVERY_RETRY_REDUCED), "RETRY_REDUCED");

    /* Invalid action should return "UNKNOWN" */
    EXPECT_STREQ(nimcp_gpu_recovery_action_name(static_cast<nimcp_gpu_recovery_action_t>(999)), "UNKNOWN");
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: StrategySelectionForInvalidParams
 * Verify correct strategy is selected for invalid params error
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, StrategySelectionForInvalidParams) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_action_t action;

    /* First retry: clamp params */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_INVALID_PARAMS, cudaSuccess, 0);
    EXPECT_EQ(action, GPU_RECOVERY_CLAMP_PARAMS);

    /* Second retry: set defaults */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_INVALID_PARAMS, cudaSuccess, 1);
    EXPECT_EQ(action, GPU_RECOVERY_SET_DEFAULTS);

    /* Third+ retry: CPU fallback */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_INVALID_PARAMS, cudaSuccess, 2);
    EXPECT_EQ(action, GPU_RECOVERY_CPU_FALLBACK);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: StrategySelectionForOOM
 * Verify correct strategy is selected for OOM error
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, StrategySelectionForOOM) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_action_t action;

    /* First retry: free cache */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 0);
    EXPECT_EQ(action, GPU_RECOVERY_FREE_CACHE);

    /* Second retry: reduce batch */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 1);
    EXPECT_EQ(action, GPU_RECOVERY_REDUCE_BATCH);

    /* Third retry: reduce dimensions */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 2);
    EXPECT_EQ(action, GPU_RECOVERY_REDUCE_DIMENSIONS);

    /* Fourth+ retry: CPU fallback */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 3);
    EXPECT_EQ(action, GPU_RECOVERY_CPU_FALLBACK);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: StrategySelectionForKernelLaunch
 * Verify correct strategy is selected for kernel launch error
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, StrategySelectionForKernelLaunch) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_action_t action;

    /* First retry: stream sync */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_KERNEL_LAUNCH, cudaErrorLaunchFailure, 0);
    EXPECT_EQ(action, GPU_RECOVERY_STREAM_SYNC);

    /* Second retry: reduce batch */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_KERNEL_LAUNCH, cudaErrorLaunchFailure, 1);
    EXPECT_EQ(action, GPU_RECOVERY_REDUCE_BATCH);

    /* Third+ retry: CPU fallback */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_KERNEL_LAUNCH, cudaErrorLaunchFailure, 2);
    EXPECT_EQ(action, GPU_RECOVERY_CPU_FALLBACK);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: StrategySelectionForDeviceNotAvailable
 * Verify immediate CPU fallback for device not available
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, StrategySelectionForDeviceNotAvailable) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_action_t action;

    /* All retries should immediately suggest CPU fallback */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_DEVICE_NOT_AVAILABLE, cudaErrorNoDevice, 0);
    EXPECT_EQ(action, GPU_RECOVERY_CPU_FALLBACK);

    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_DEVICE_NOT_AVAILABLE, cudaErrorNoDevice, 1);
    EXPECT_EQ(action, GPU_RECOVERY_CPU_FALLBACK);

    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_DEVICE_NOT_AVAILABLE, cudaErrorNoDevice, 5);
    EXPECT_EQ(action, GPU_RECOVERY_CPU_FALLBACK);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: StrategySelectionForNumerical
 * Verify correct strategy for numerical errors
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, StrategySelectionForNumerical) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_action_t action;

    /* First retry: reduce precision */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_NUMERICAL, cudaSuccess, 0);
    EXPECT_EQ(action, GPU_RECOVERY_REDUCE_PRECISION);

    /* Second retry: clamp params */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_NUMERICAL, cudaSuccess, 1);
    EXPECT_EQ(action, GPU_RECOVERY_CLAMP_PARAMS);

    /* Third+ retry: CPU fallback */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_NUMERICAL, cudaSuccess, 2);
    EXPECT_EQ(action, GPU_RECOVERY_CPU_FALLBACK);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: StrategySelectionForTimeout
 * Verify correct strategy for timeout errors
 * ============================================================================ */
TEST_F(GPURecoveryErrorCategorizationTest, StrategySelectionForTimeout) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_action_t action;

    /* First retry: async split */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_TIMEOUT, cudaSuccess, 0);
    EXPECT_EQ(action, GPU_RECOVERY_ASYNC_SPLIT);

    /* Second retry: reduce batch */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_TIMEOUT, cudaSuccess, 1);
    EXPECT_EQ(action, GPU_RECOVERY_REDUCE_BATCH);

    /* Third+ retry: CPU fallback */
    action = nimcp_gpu_select_recovery_strategy(GPU_ERROR_TIMEOUT, cudaSuccess, 2);
    EXPECT_EQ(action, GPU_RECOVERY_CPU_FALLBACK);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
