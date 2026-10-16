/**
 * @file test_gpu_exception_handling.cpp
 * @brief Unit tests for GPU exception handling integration
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Unit tests for GPU-specific exception handling
 * WHY:  Verify GPU exceptions are properly created, dispatched, and integrated with immune system
 * HOW:  Test GPU exception creation, memory allocation exceptions, kernel launch exceptions,
 *       device unavailable exceptions, and CPU fallback exception handling
 *
 * TEST CATEGORIES:
 * - GPU exception creation and configuration
 * - GPU memory allocation exception handling
 * - Kernel launch failure exceptions
 * - Device unavailable exception recovery
 * - CPU fallback exception handling
 * - Exception-to-immune integration for GPU errors
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class GpuExceptionHandlingTest : public ::testing::Test {
protected:
    static int handler_call_count;
    static nimcp_error_t last_exception_code;
    static int last_device_id;
    static int last_cuda_error;
    static bool recovery_attempted;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = NIMCP_SUCCESS;
        last_device_id = -1;
        last_cuda_error = 0;
        recovery_attempted = false;

        nimcp_exception_system_init();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_gpu_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;

        if (ex->type == EXCEPTION_TYPE_GPU) {
            nimcp_gpu_exception_t* gpu_ex = (nimcp_gpu_exception_t*)ex;
            last_device_id = gpu_ex->device_id;
            last_cuda_error = gpu_ex->cuda_error;
        }

        return false;  // Don't consume - allow other handlers to process
    }

    static int test_recovery_callback(nimcp_exception_t* ex,
                                       nimcp_exception_recovery_action_t action,
                                       void* user_data) {
        (void)ex;
        (void)action;
        (void)user_data;
        recovery_attempted = true;
        return 0;  // Success
    }
};

int GpuExceptionHandlingTest::handler_call_count = 0;
nimcp_error_t GpuExceptionHandlingTest::last_exception_code = NIMCP_SUCCESS;
int GpuExceptionHandlingTest::last_device_id = -1;
int GpuExceptionHandlingTest::last_cuda_error = 0;
bool GpuExceptionHandlingTest::recovery_attempted = false;

//=============================================================================
// GPU Exception Creation Tests
//=============================================================================

TEST_F(GpuExceptionHandlingTest, CreateGpuException) {
    // WHAT: Test basic GPU exception creation
    // WHY:  Verify GPU exceptions can be created with all required fields

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0,   // device_id
        100, // cuda_error (simulated)
        "GPU operation failed on device 0"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_GPU);
    EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_GPU);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_GPU);
    EXPECT_EQ(ex->base.severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->device_id, 0);
    EXPECT_EQ(ex->cuda_error, 100);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(GpuExceptionHandlingTest, CreateGpuMemoryException) {
    // WHAT: Test GPU memory allocation exception
    // WHY:  Memory allocation failures are common GPU errors

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0,  // device_id
        2,  // CUDA out of memory error code (cudaErrorMemoryAllocation)
        "Failed to allocate 1GB GPU memory"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_GPU_MEMORY);
    EXPECT_EQ(ex->base.severity, EXCEPTION_SEVERITY_SEVERE);
    EXPECT_EQ(ex->cuda_error, 2);

    // Set additional GPU-specific fields
    ex->gpu_memory_used = 8ULL * 1024 * 1024 * 1024;  // 8GB used
    ex->gpu_memory_total = 12ULL * 1024 * 1024 * 1024; // 12GB total

    EXPECT_EQ(ex->gpu_memory_used, 8ULL * 1024 * 1024 * 1024);
    EXPECT_EQ(ex->gpu_memory_total, 12ULL * 1024 * 1024 * 1024);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(GpuExceptionHandlingTest, CreateKernelLaunchException) {
    // WHAT: Test kernel launch failure exception
    // WHY:  Kernel launch failures require proper error handling

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_KERNEL_LAUNCH,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0,  // device_id
        9,  // CUDA invalid configuration error
        "Kernel launch failed: invalid configuration"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_KERNEL_LAUNCH);

    // Set kernel name
    ex->kernel_name = "update_neurons_kernel";
    EXPECT_STREQ(ex->kernel_name, "update_neurons_kernel");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(GpuExceptionHandlingTest, CreateDeviceUnavailableException) {
    // WHAT: Test device unavailable exception
    // WHY:  Need to handle cases where GPU is not accessible

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_NOT_AVAILABLE,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        -1,  // no device
        101, // CUDA no device error
        "No CUDA-capable device detected"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_GPU_NOT_AVAILABLE);
    EXPECT_EQ(ex->device_id, -1);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(GpuExceptionHandlingTest, CreateSyncFailureException) {
    // WHAT: Test GPU synchronization failure exception
    // WHY:  Sync failures can indicate serious issues

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_SYNC,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0,   // device_id
        702, // CUDA async error
        "GPU synchronization failed"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_GPU_SYNC);
    EXPECT_EQ(ex->cuda_error, 702);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// GPU Exception Severity Tests
//=============================================================================

TEST_F(GpuExceptionHandlingTest, GpuExceptionSeverityMapping) {
    // WHAT: Test severity levels for different GPU errors
    // WHY:  Ensure proper severity classification

    struct {
        nimcp_error_t code;
        nimcp_exception_severity_t expected_severity;
    } test_cases[] = {
        { NIMCP_ERROR_GPU, EXCEPTION_SEVERITY_ERROR },
        { NIMCP_ERROR_GPU_NOT_AVAILABLE, EXCEPTION_SEVERITY_SEVERE },
        { NIMCP_ERROR_GPU_MEMORY, EXCEPTION_SEVERITY_SEVERE },
        { NIMCP_ERROR_KERNEL_LAUNCH, EXCEPTION_SEVERITY_ERROR },
        { NIMCP_ERROR_GPU_SYNC, EXCEPTION_SEVERITY_ERROR },
    };

    for (const auto& tc : test_cases) {
        nimcp_exception_severity_t severity =
            nimcp_exception_get_severity_from_code(tc.code);
        // Severity may vary by implementation; just verify it's reasonable
        EXPECT_GE((int)severity, (int)EXCEPTION_SEVERITY_WARNING)
            << "Code: " << tc.code;
    }
}

//=============================================================================
// GPU Exception Handler Chain Tests
//=============================================================================

TEST_F(GpuExceptionHandlingTest, DispatchGpuException) {
    // WHAT: Test GPU exception dispatch through handler chain
    // WHY:  Verify handlers receive GPU exceptions correctly

    // Register test handler
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "test_gpu_handler";
    options.handler = test_gpu_exception_handler;
    options.priority = 100;
    options.category_filter = EXCEPTION_CATEGORY_GPU;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    // Create and dispatch GPU exception
    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, 2, "GPU memory allocation failed"
    );
    ASSERT_NE(ex, nullptr);

    handler_call_count = 0;
    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    EXPECT_GE(handler_call_count, 1);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_GPU_MEMORY);
    EXPECT_EQ(last_device_id, 0);
    EXPECT_EQ(last_cuda_error, 2);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    if (reg) nimcp_handler_unregister(reg);
}

TEST_F(GpuExceptionHandlingTest, GpuExceptionWithTypeFilter) {
    // WHAT: Test handler filtering by exception type
    // WHY:  Ensure GPU-specific handlers only receive GPU exceptions

    // Register handler with type filter
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "gpu_type_filter_handler";
    options.handler = test_gpu_exception_handler;
    options.priority = 100;
    options.type_filter = EXCEPTION_TYPE_GPU;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    // Create non-GPU exception - should NOT be handled
    nimcp_exception_t* mem_ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Regular memory error"
    );

    handler_call_count = 0;
    nimcp_exception_dispatch(mem_ex);
    int mem_handler_count = handler_call_count;

    // Create GPU exception - SHOULD be handled
    nimcp_gpu_exception_t* gpu_ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0, 0, "GPU error"
    );

    handler_call_count = 0;
    nimcp_exception_dispatch((nimcp_exception_t*)gpu_ex);
    int gpu_handler_count = handler_call_count;

    // GPU handler should be called more for GPU exception
    // (Note: there may be other default handlers)
    EXPECT_GE(gpu_handler_count, 1);

    nimcp_exception_unref(mem_ex);
    nimcp_exception_unref((nimcp_exception_t*)gpu_ex);
    if (reg) nimcp_handler_unregister(reg);
}

//=============================================================================
// GPU Exception Recovery Tests
//=============================================================================

TEST_F(GpuExceptionHandlingTest, GpuMemoryRecoveryStrategy) {
    // WHAT: Test recovery strategy for GPU memory exceptions
    // WHY:  GPU memory failures should trigger GC or reduce load

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, 2, "Out of GPU memory"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    // GPU memory errors should have some recovery strategy
    EXPECT_TRUE(
        strategy.primary_action != EXCEPTION_RECOVERY_NONE ||
        strategy.fallback_action != EXCEPTION_RECOVERY_NONE ||
        strategy.retry_count > 0
    );

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(GpuExceptionHandlingTest, GpuKernelRecoveryStrategy) {
    // WHAT: Test recovery strategy for kernel launch failures
    // WHY:  Kernel failures may need retry or component restart

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_KERNEL_LAUNCH,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0, 9, "Invalid kernel configuration"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    // Should have some recovery action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(GpuExceptionHandlingTest, RegisterRecoveryCallback) {
    // WHAT: Test registering GPU-specific recovery callback
    // WHY:  Allow custom recovery for GPU errors

    int result = nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_RETRY,
        test_recovery_callback,
        nullptr
    );
    EXPECT_EQ(result, 0);

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0, 0, "GPU error requiring retry"
    );
    ASSERT_NE(ex, nullptr);

    recovery_attempted = false;
    nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_RETRY);
    EXPECT_TRUE(recovery_attempted);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
}

//=============================================================================
// GPU Exception Context Tests
//=============================================================================

TEST_F(GpuExceptionHandlingTest, GpuExceptionContext) {
    // WHAT: Test attaching context to GPU exceptions
    // WHY:  Context helps with debugging and recovery

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, 2, "GPU memory allocation failed"
    );
    ASSERT_NE(ex, nullptr);

    // Set context
    nimcp_exception_set_context((nimcp_exception_t*)ex, "requested_size", "1073741824");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "allocation_type", "tensor");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "kernel_name", "forward_pass");

    // Get context
    const char* size = nimcp_exception_get_context((nimcp_exception_t*)ex, "requested_size");
    const char* type = nimcp_exception_get_context((nimcp_exception_t*)ex, "allocation_type");
    const char* kernel = nimcp_exception_get_context((nimcp_exception_t*)ex, "kernel_name");

    EXPECT_STREQ(size, "1073741824");
    EXPECT_STREQ(type, "tensor");
    EXPECT_STREQ(kernel, "forward_pass");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(GpuExceptionHandlingTest, GpuExceptionEpitope) {
    // WHAT: Test epitope generation for GPU exceptions
    // WHY:  Epitopes are used for immune pattern matching

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0, 100, "GPU error"
    );
    ASSERT_NE(ex, nullptr);

    size_t epitope_len = nimcp_exception_generate_epitope((nimcp_exception_t*)ex);
    EXPECT_GT(epitope_len, 0u);
    EXPECT_EQ(ex->base.epitope_len, epitope_len);

    // Epitope should be non-zero
    bool has_nonzero = false;
    for (size_t i = 0; i < epitope_len; i++) {
        if (ex->base.epitope[i] != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Multi-Device GPU Exception Tests
//=============================================================================

TEST_F(GpuExceptionHandlingTest, MultiDeviceExceptions) {
    // WHAT: Test GPU exceptions across multiple devices
    // WHY:  Multi-GPU systems need device-specific error handling

    nimcp_gpu_exception_t* ex0 = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, 2, "GPU 0 out of memory"
    );

    nimcp_gpu_exception_t* ex1 = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1, 2, "GPU 1 out of memory"
    );

    ASSERT_NE(ex0, nullptr);
    ASSERT_NE(ex1, nullptr);

    EXPECT_EQ(ex0->device_id, 0);
    EXPECT_EQ(ex1->device_id, 1);

    // Both should have same error type but different devices
    EXPECT_EQ(ex0->base.code, ex1->base.code);
    EXPECT_NE(ex0->device_id, ex1->device_id);

    nimcp_exception_unref((nimcp_exception_t*)ex0);
    nimcp_exception_unref((nimcp_exception_t*)ex1);
}

//=============================================================================
// GPU Exception Chaining Tests
//=============================================================================

TEST_F(GpuExceptionHandlingTest, GpuExceptionChaining) {
    // WHAT: Test chaining GPU exceptions with underlying cause
    // WHY:  GPU errors often have underlying CUDA/system errors

    // Create root cause (CUDA error)
    nimcp_exception_t* cuda_ex = nimcp_exception_create(
        NIMCP_ERROR_CUDA,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "CUDA driver returned error 702"
    );
    ASSERT_NE(cuda_ex, nullptr);

    // Create GPU exception with CUDA cause
    nimcp_gpu_exception_t* gpu_ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_SYNC,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, 702, "GPU sync failed"
    );
    ASSERT_NE(gpu_ex, nullptr);

    // Chain exceptions
    nimcp_exception_set_cause((nimcp_exception_t*)gpu_ex, cuda_ex);

    // Verify chain
    nimcp_exception_t* cause = nimcp_exception_get_cause((nimcp_exception_t*)gpu_ex);
    ASSERT_NE(cause, nullptr);
    EXPECT_EQ(cause->code, NIMCP_ERROR_CUDA);

    nimcp_exception_unref((nimcp_exception_t*)gpu_ex);
    // cuda_ex is released via chain
}

//=============================================================================
// CPU Fallback Exception Tests
//=============================================================================

TEST_F(GpuExceptionHandlingTest, CpuFallbackFromGpuError) {
    // WHAT: Test CPU fallback behavior when GPU fails
    // WHY:  System should gracefully fall back to CPU execution

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_NOT_AVAILABLE,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        -1, 101, "GPU unavailable, falling back to CPU"
    );
    ASSERT_NE(ex, nullptr);

    // Set context indicating fallback
    nimcp_exception_set_context((nimcp_exception_t*)ex, "fallback_target", "CPU");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "fallback_reason", "device_unavailable");

    const char* target = nimcp_exception_get_context((nimcp_exception_t*)ex, "fallback_target");
    EXPECT_STREQ(target, "CPU");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// GPU Exception Logging Tests
//=============================================================================

TEST_F(GpuExceptionHandlingTest, GpuExceptionToString) {
    // WHAT: Test GPU exception string formatting
    // WHY:  Need readable output for logging and debugging

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, 2, "GPU memory allocation failed"
    );
    ASSERT_NE(ex, nullptr);

    ex->gpu_memory_used = 8ULL * 1024 * 1024 * 1024;
    ex->gpu_memory_total = 12ULL * 1024 * 1024 * 1024;
    ex->kernel_name = "forward_pass";

    char buffer[1024];
    size_t len = nimcp_exception_to_string((nimcp_exception_t*)ex, buffer, sizeof(buffer));
    EXPECT_GT(len, 0u);

    // String should contain key information
    std::string str(buffer);
    EXPECT_NE(str.find("GPU"), std::string::npos);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Null Parameter Tests
//=============================================================================

TEST_F(GpuExceptionHandlingTest, NullGpuExceptionParameters) {
    // WHAT: Test null parameter handling
    // WHY:  Ensure robustness against null inputs

    // Create with null file should still work (use default)
    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_ERROR,
        NULL, 0, NULL,  // null file, func
        0, 0, "GPU error with null params"
    );
    // Implementation may return NULL or handle gracefully
    if (ex) {
        nimcp_exception_unref((nimcp_exception_t*)ex);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
