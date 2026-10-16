/**
 * @file test_exception_api_comprehensive.cpp
 * @brief Comprehensive regression tests for exception handling API stability
 * @date 2026-01-23
 *
 * PURPOSE: This test file provides comprehensive API stability regression tests
 * to ensure the exception handling system maintains consistent behavior across
 * versions. These tests serve as contracts that external code may depend on.
 *
 * TESTS COVER:
 * 1. All public exception macros maintain consistent behavior
 * 2. Error code values remain stable (specific numeric values)
 * 3. Exception structure layout compatibility
 * 4. Function signatures unchanged
 * 5. Callback interface stability
 * 6. Thread-safety guarantees
 * 7. Memory layout of exception types
 * 8. Exception severity mapping from error codes
 * 9. Immune integration contracts
 * 10. Memory management in exception paths
 *
 * IMPORTANT: If any of these tests fail, it indicates a breaking API change
 * that may affect downstream code depending on the exception system.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstddef>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <condition_variable>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExceptionAPIRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize exception system for each test
        int result = nimcp_exception_system_init();
        ASSERT_EQ(result, 0) << "Failed to initialize exception system";
    }

    void TearDown() override {
        // Clean up after each test
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

/* ============================================================================
 * SECTION 1: Error Code Value Stability Tests
 *
 * Error codes are contracts that external code depends on.
 * These numeric values MUST NOT change.
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, ErrorCodeStability_SuccessCodes) {
    // Success codes (0-999) - these are foundational
    EXPECT_EQ(NIMCP_SUCCESS, 0) << "NIMCP_SUCCESS must be 0";
    EXPECT_EQ(NIMCP_SUCCESS_WITH_WARNINGS, 1);
    EXPECT_EQ(NIMCP_SUCCESS_PARTIAL, 2);
}

TEST_F(ExceptionAPIRegressionTest, ErrorCodeStability_GenericErrorCodes) {
    // Generic errors (1000-1999) - most commonly used
    EXPECT_EQ(NIMCP_ERROR_UNKNOWN, 1000);
    EXPECT_EQ(NIMCP_ERROR_NOT_IMPLEMENTED, 1001);
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER, 1002);
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, 1002);  // Alias must match
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, 1003);
    EXPECT_EQ(NIMCP_ERROR_OUT_OF_RANGE, 1004);
    EXPECT_EQ(NIMCP_ERROR_INVALID_STATE, 1005);
    EXPECT_EQ(NIMCP_ERROR_OPERATION_FAILED, 1006);
    EXPECT_EQ(NIMCP_ERROR_NOT_INITIALIZED, 1007);
    EXPECT_EQ(NIMCP_ERROR_ALREADY_EXISTS, 1008);
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND, 1009);
    EXPECT_EQ(NIMCP_ERROR_TIMEOUT, 1010);
    EXPECT_EQ(NIMCP_ERROR_CANCELLED, 1011);
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED, 1012);
}

TEST_F(ExceptionAPIRegressionTest, ErrorCodeStability_GPUErrorCodes) {
    // GPU errors (1100-1199)
    EXPECT_EQ(NIMCP_ERROR_GPU, 1100);
    EXPECT_EQ(NIMCP_ERROR_GPU_NOT_AVAILABLE, 1101);
    EXPECT_EQ(NIMCP_ERROR_GPU_MEMORY, 1102);
    EXPECT_EQ(NIMCP_ERROR_CUDA, 1103);
    EXPECT_EQ(NIMCP_ERROR_KERNEL_LAUNCH, 1104);
    EXPECT_EQ(NIMCP_ERROR_GPU_SYNC, 1105);
}

TEST_F(ExceptionAPIRegressionTest, ErrorCodeStability_MemoryErrorCodes) {
    // Memory errors (2000-2999)
    EXPECT_EQ(NIMCP_ERROR_NO_MEMORY, 2000);
    EXPECT_EQ(NIMCP_ERROR_BUFFER_TOO_SMALL, 2001);
    EXPECT_EQ(NIMCP_ERROR_BUFFER_OVERFLOW, 2002);
    EXPECT_EQ(NIMCP_ERROR_MEMORY_CORRUPTION, 2003);
    EXPECT_EQ(NIMCP_ERROR_INVALID_ADDRESS, 2004);
    EXPECT_EQ(NIMCP_ERROR_MEMORY_LEAK, 2005);
    EXPECT_EQ(NIMCP_ERROR_DOUBLE_FREE, 2006);
}

TEST_F(ExceptionAPIRegressionTest, ErrorCodeStability_BrainErrorCodes) {
    // Brain/Network errors (3000-3999)
    EXPECT_EQ(NIMCP_ERROR_BRAIN_CREATION, 3000);
    EXPECT_EQ(NIMCP_ERROR_BRAIN_INVALID, 3001);
    EXPECT_EQ(NIMCP_ERROR_NETWORK_CREATION, 3002);
    EXPECT_EQ(NIMCP_ERROR_NETWORK_INVALID, 3003);
    EXPECT_EQ(NIMCP_ERROR_DIMENSION_MISMATCH, 3004);
    EXPECT_EQ(NIMCP_ERROR_WEIGHT_INIT, 3005);
    EXPECT_EQ(NIMCP_ERROR_FORWARD_PASS, 3006);
    EXPECT_EQ(NIMCP_ERROR_BACKWARD_PASS, 3007);
    EXPECT_EQ(NIMCP_ERROR_LEARNING_FAILED, 3008);
    EXPECT_EQ(NIMCP_ERROR_INFERENCE_FAILED, 3009);
}

TEST_F(ExceptionAPIRegressionTest, ErrorCodeStability_IOErrorCodes) {
    // I/O errors (4000-4999)
    EXPECT_EQ(NIMCP_ERROR_FILE_NOT_FOUND, 4000);
    EXPECT_EQ(NIMCP_ERROR_FILE_READ, 4001);
    EXPECT_EQ(NIMCP_ERROR_FILE_WRITE, 4002);
    EXPECT_EQ(NIMCP_ERROR_FILE_OPEN, 4003);
    EXPECT_EQ(NIMCP_ERROR_FILE_CLOSE, 4004);
    EXPECT_EQ(NIMCP_ERROR_FILE_CORRUPT, 4005);
    EXPECT_EQ(NIMCP_ERROR_SERIALIZATION, 4006);
    EXPECT_EQ(NIMCP_ERROR_DESERIALIZATION, 4007);
    EXPECT_EQ(NIMCP_ERROR_NETWORK_IO, 4008);
    EXPECT_EQ(NIMCP_ERROR_IO, 4010);
}

TEST_F(ExceptionAPIRegressionTest, ErrorCodeStability_ThreadingErrorCodes) {
    // Threading errors (6000-6999)
    EXPECT_EQ(NIMCP_ERROR_THREAD_CREATE, 6000);
    EXPECT_EQ(NIMCP_ERROR_THREAD_JOIN, 6001);
    EXPECT_EQ(NIMCP_ERROR_MUTEX_LOCK, 6002);
    EXPECT_EQ(NIMCP_ERROR_MUTEX_UNLOCK, 6003);
    EXPECT_EQ(NIMCP_ERROR_MUTEX_INIT, 6004);
    EXPECT_EQ(NIMCP_ERROR_DEADLOCK, 6005);
    EXPECT_EQ(NIMCP_ERROR_RACE_CONDITION, 6006);
    EXPECT_EQ(NIMCP_ERROR_THREAD_SYNC, 6007);
}

TEST_F(ExceptionAPIRegressionTest, ErrorCodeStability_SignalErrorCodes) {
    // Signal/Crash errors (7000-7999)
    EXPECT_EQ(NIMCP_ERROR_SIGNAL_RECEIVED, 7000);
    EXPECT_EQ(NIMCP_ERROR_SIGSEGV, 7001);
    EXPECT_EQ(NIMCP_ERROR_SIGABRT, 7002);
    EXPECT_EQ(NIMCP_ERROR_SIGFPE, 7003);
    EXPECT_EQ(NIMCP_ERROR_SIGBUS, 7004);
    EXPECT_EQ(NIMCP_ERROR_SIGILL, 7005);
    EXPECT_EQ(NIMCP_ERROR_CRASH_RECOVERY, 7006);
    EXPECT_EQ(NIMCP_ERROR_CHECKPOINT_SAVE, 7007);
    EXPECT_EQ(NIMCP_ERROR_CHECKPOINT_LOAD, 7008);
}

TEST_F(ExceptionAPIRegressionTest, ErrorCodeStability_SecurityErrorCodes) {
    // Security errors (9000-9099)
    EXPECT_EQ(NIMCP_ERROR_SECURITY_BASE, 9000);
    EXPECT_EQ(NIMCP_ERROR_BBB_REJECTED, 9001);
    EXPECT_EQ(NIMCP_ERROR_BBB_VALIDATION, 9002);
    EXPECT_EQ(NIMCP_ERROR_SECURITY_THREAT, 9003);
    EXPECT_EQ(NIMCP_ERROR_ACCESS_DENIED, 9004);
    EXPECT_EQ(NIMCP_ERROR_SIGNATURE_INVALID, 9005);
    EXPECT_EQ(NIMCP_ERROR_ENCRYPTION_FAILED, 9006);
    EXPECT_EQ(NIMCP_ERROR_DECRYPTION_FAILED, 9007);
}

TEST_F(ExceptionAPIRegressionTest, ErrorCodeStability_CognitiveErrorCodes) {
    // Cognitive errors (8000-8999)
    EXPECT_EQ(NIMCP_ERROR_WORKING_MEMORY, 8000);
    EXPECT_EQ(NIMCP_ERROR_EMOTIONAL_TAGGING, 8001);
    EXPECT_EQ(NIMCP_ERROR_EXECUTIVE_CONTROL, 8002);
    EXPECT_EQ(NIMCP_ERROR_SLEEP_WAKE, 8003);
    EXPECT_EQ(NIMCP_ERROR_MENTAL_HEALTH, 8004);
    EXPECT_EQ(NIMCP_ERROR_THEORY_OF_MIND, 8005);
    EXPECT_EQ(NIMCP_ERROR_EXPLANATIONS, 8006);
    EXPECT_EQ(NIMCP_ERROR_META_LEARNING, 8007);
    EXPECT_EQ(NIMCP_ERROR_PREDICTIVE, 8008);
}

/* ============================================================================
 * SECTION 2: Exception Severity Enum Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, SeverityEnumStability_AllValues) {
    EXPECT_EQ(static_cast<int>(EXCEPTION_SEVERITY_DEBUG), 1);
    EXPECT_EQ(static_cast<int>(EXCEPTION_SEVERITY_INFO), 2);
    EXPECT_EQ(static_cast<int>(EXCEPTION_SEVERITY_WARNING), 3);
    EXPECT_EQ(static_cast<int>(EXCEPTION_SEVERITY_ERROR), 5);
    EXPECT_EQ(static_cast<int>(EXCEPTION_SEVERITY_SEVERE), 7);
    EXPECT_EQ(static_cast<int>(EXCEPTION_SEVERITY_CRITICAL), 9);
    EXPECT_EQ(static_cast<int>(EXCEPTION_SEVERITY_FATAL), 10);
}

TEST_F(ExceptionAPIRegressionTest, SeverityEnumStability_Ordering) {
    // Verify severity ordering is maintained
    EXPECT_LT(EXCEPTION_SEVERITY_DEBUG, EXCEPTION_SEVERITY_INFO);
    EXPECT_LT(EXCEPTION_SEVERITY_INFO, EXCEPTION_SEVERITY_WARNING);
    EXPECT_LT(EXCEPTION_SEVERITY_WARNING, EXCEPTION_SEVERITY_ERROR);
    EXPECT_LT(EXCEPTION_SEVERITY_ERROR, EXCEPTION_SEVERITY_SEVERE);
    EXPECT_LT(EXCEPTION_SEVERITY_SEVERE, EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_LT(EXCEPTION_SEVERITY_CRITICAL, EXCEPTION_SEVERITY_FATAL);
}

/* ============================================================================
 * SECTION 3: Exception Category Enum Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, CategoryEnumStability_AllValues) {
    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_GENERIC), 1);
    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_MEMORY), 2);
    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_BRAIN), 3);
    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_IO), 4);
    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_CONFIG), 5);
    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_THREADING), 6);
    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_SIGNAL), 7);
    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_COGNITIVE), 8);
    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_GPU), 11);
    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_BRAIN_REGION), 10);
    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_SECURITY), 20);
}

/* ============================================================================
 * SECTION 4: Exception Type Enum Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, TypeEnumStability_AllValues) {
    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_BASE), 0);
    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_MEMORY), 1);
    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_BRAIN), 2);
    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_IO), 3);
    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_THREADING), 4);
    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_SECURITY), 5);
    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_COGNITIVE), 6);
    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_GPU), 7);
    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_AGGREGATE), 8);
    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_SIGNAL), 9);
}

/* ============================================================================
 * SECTION 5: Recovery Action Enum Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, RecoveryActionEnumStability_AllValues) {
    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_NONE), 0);
    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_RETRY), 1);
    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_GC), 2);
    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_COMPACT), 3);
    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_ROLLBACK), 4);
    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_RESTART_THREAD), 5);
    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_RESTART_COMPONENT), 6);
    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_QUARANTINE), 7);
    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_REDUCE_LOAD), 8);
    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_CLEAR_CACHE), 9);
    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_EMERGENCY_SAVE), 10);
    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN), 11);
}

/* ============================================================================
 * SECTION 6: Structure Layout Compatibility Tests
 *
 * These tests verify that structure layouts haven't changed.
 * External code may depend on specific field offsets for serialization.
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, StructureLayout_BaseException) {
    // Verify base exception has expected fields and base layout
    nimcp_exception_t ex = {};

    // type must be the first field for polymorphism to work
    EXPECT_EQ(offsetof(nimcp_exception_t, type), 0UL);

    // Verify all expected fields exist and are accessible
    ex.type = EXCEPTION_TYPE_BASE;
    ex.category = EXCEPTION_CATEGORY_GENERIC;
    ex.code = NIMCP_ERROR_UNKNOWN;
    ex.severity = EXCEPTION_SEVERITY_ERROR;
    ex.file = "test.c";
    ex.line = 42;
    ex.function = "test_func";
    ex.timestamp_us = 12345678ULL;
    ex.presented_to_immune = false;
    ex.antigen_id = 0;
    ex.suggested_action = EXCEPTION_RECOVERY_NONE;
    ex.recovery_attempted = false;
    ex.recovery_succeeded = false;
    ex.ref_count = 1;
    ex.cause = nullptr;
    ex.context_count = 0;

    EXPECT_EQ(ex.type, EXCEPTION_TYPE_BASE);
    EXPECT_EQ(ex.category, EXCEPTION_CATEGORY_GENERIC);
    EXPECT_EQ(ex.code, NIMCP_ERROR_UNKNOWN);
}

TEST_F(ExceptionAPIRegressionTest, StructureLayout_MemoryException_BaseMustBeFirst) {
    // For C polymorphism to work, base must be first field
    EXPECT_EQ(offsetof(nimcp_memory_exception_t, base), 0UL);

    // Verify memory-specific fields exist
    nimcp_memory_exception_t mex = {};
    mex.requested_size = 1024;
    mex.available_size = 512;
    mex.failed_address = nullptr;
    mex.allocator_name = "test_pool";
    mex.is_heap = true;

    EXPECT_EQ(mex.requested_size, 1024UL);
}

TEST_F(ExceptionAPIRegressionTest, StructureLayout_BrainException_BaseMustBeFirst) {
    EXPECT_EQ(offsetof(nimcp_brain_exception_t, base), 0UL);

    nimcp_brain_exception_t bex = {};
    bex.brain_id = 1;
    bex.network_id = 2;
    bex.layer_id = 3;
    bex.region_name = "prefrontal";
    bex.gradient_norm = 1.5f;
    bex.has_nan_weights = false;
    bex.learning_diverged = false;

    EXPECT_EQ(bex.brain_id, 1U);
}

TEST_F(ExceptionAPIRegressionTest, StructureLayout_IOException_BaseMustBeFirst) {
    EXPECT_EQ(offsetof(nimcp_io_exception_t, base), 0UL);

    nimcp_io_exception_t iex = {};
    iex.path = "/tmp/test.dat";
    iex.errno_value = 2;
    iex.bytes_transferred = 100;
    iex.bytes_expected = 200;
    iex.is_network = false;
    iex.socket_fd = -1;

    EXPECT_STREQ(iex.path, "/tmp/test.dat");
}

TEST_F(ExceptionAPIRegressionTest, StructureLayout_ThreadingException_BaseMustBeFirst) {
    EXPECT_EQ(offsetof(nimcp_threading_exception_t, base), 0UL);

    nimcp_threading_exception_t tex = {};
    tex.thread_id = 12345;
    tex.thread_name = "worker-1";
    tex.mutex_address = nullptr;
    tex.lock_wait_time_us = 1000;
    tex.is_deadlock = true;
    tex.deadlock_cycle_len = 2;

    EXPECT_EQ(tex.thread_id, 12345UL);
}

TEST_F(ExceptionAPIRegressionTest, StructureLayout_SignalException_BaseMustBeFirst) {
    EXPECT_EQ(offsetof(nimcp_signal_exception_t, base), 0UL);

    nimcp_signal_exception_t sex = {};
    sex.signal_number = 11;  // SIGSEGV
    sex.fault_address = (void*)0xDEADBEEF;
    sex.instruction_pointer = nullptr;
    sex.stack_pointer = nullptr;
    sex.siglongjmp_executed = false;
    sex.retry_count = 0;

    EXPECT_EQ(sex.signal_number, 11);
}

TEST_F(ExceptionAPIRegressionTest, StructureLayout_AggregateException_BaseMustBeFirst) {
    EXPECT_EQ(offsetof(nimcp_aggregate_exception_t, base), 0UL);

    nimcp_aggregate_exception_t aex = {};
    aex.child_count = 0;

    EXPECT_EQ(aex.child_count, 0UL);
}

/* ============================================================================
 * SECTION 7: Constant Value Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, ConstantStability_ExceptionLimits) {
    EXPECT_EQ(NIMCP_EXCEPTION_MAX_MESSAGE, 256);
    EXPECT_EQ(NIMCP_EXCEPTION_MAX_STACK_DEPTH, 32);
    EXPECT_EQ(NIMCP_EXCEPTION_EPITOPE_SIZE, 64);
    EXPECT_EQ(NIMCP_EXCEPTION_MAX_CONTEXT, 512);
    EXPECT_EQ(NIMCP_EXCEPTION_MAX_CHILDREN, 16);
    EXPECT_EQ(NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES, 16);
    EXPECT_EQ(NIMCP_EXCEPTION_MAX_CONTEXT_KEY, 32);
    EXPECT_EQ(NIMCP_EXCEPTION_MAX_CONTEXT_VALUE, 128);
}

TEST_F(ExceptionAPIRegressionTest, ConstantStability_HandlerLimits) {
    EXPECT_EQ(NIMCP_HANDLER_MAX_REGISTERED, 64);
    EXPECT_EQ(NIMCP_TRY_STACK_DEPTH, 16);
    EXPECT_EQ(NIMCP_HANDLER_PRIORITY_HIGH, 100);
    EXPECT_EQ(NIMCP_HANDLER_PRIORITY_NORMAL, 50);
    EXPECT_EQ(NIMCP_HANDLER_PRIORITY_LOW, 10);
}

TEST_F(ExceptionAPIRegressionTest, ConstantStability_KGWiringMessages) {
    EXPECT_STREQ(KG_MSG_EXCEPTION_RAISED, "EXCEPTION_RAISED");
    EXPECT_STREQ(KG_MSG_ANTIGEN_PRESENTED, "ANTIGEN_PRESENTED");
    EXPECT_STREQ(KG_MSG_RECOVERY_REQUEST, "RECOVERY_REQUEST");
    EXPECT_STREQ(KG_MSG_RECOVERY_RESULT, "RECOVERY_RESULT");
    EXPECT_STREQ(KG_MSG_ERROR_REPORT, "ERROR_REPORT");
    EXPECT_STREQ(KG_MSG_CRASH_SIGNAL, "CRASH_SIGNAL");
    EXPECT_STREQ(KG_EXCEPTION_MODULE_NAME, "exception_handler");
    EXPECT_STREQ(KG_EXCEPTION_MODULE_TYPE, "FAULT_TOLERANCE");
}

/* ============================================================================
 * SECTION 8: Function Signature Stability Tests
 *
 * These tests verify that function signatures compile correctly
 * by calling the functions with proper parameters.
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, FunctionSignature_ExceptionCreate) {
    // Verify function accepts documented parameters
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,   // nimcp_error_t code
        EXCEPTION_SEVERITY_ERROR,       // nimcp_exception_severity_t severity
        __FILE__,                       // const char* file
        __LINE__,                       // int line
        __func__,                       // const char* func
        "Test %s %d",                   // const char* format
        "message", 42                   // ... (variadic)
    );
    ASSERT_NE(ex, nullptr);
    nimcp_exception_unref(ex);
}

TEST_F(ExceptionAPIRegressionTest, FunctionSignature_MemoryExceptionCreate) {
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,          // nimcp_error_t code
        EXCEPTION_SEVERITY_SEVERE,      // nimcp_exception_severity_t severity
        __FILE__,                       // const char* file
        __LINE__,                       // int line
        __func__,                       // const char* func
        1024,                           // size_t requested_size
        "Alloc failed"                  // const char* format
    );
    ASSERT_NE(mex, nullptr);
    nimcp_exception_unref((nimcp_exception_t*)mex);
}

TEST_F(ExceptionAPIRegressionTest, FunctionSignature_BrainExceptionCreate) {
    nimcp_brain_exception_t* bex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,       // nimcp_error_t code
        EXCEPTION_SEVERITY_ERROR,       // nimcp_exception_severity_t severity
        __FILE__,                       // const char* file
        __LINE__,                       // int line
        __func__,                       // const char* func
        123,                            // uint32_t brain_id
        "prefrontal",                   // const char* region_name
        "Forward pass failed"           // const char* format
    );
    ASSERT_NE(bex, nullptr);
    nimcp_exception_unref((nimcp_exception_t*)bex);
}

TEST_F(ExceptionAPIRegressionTest, FunctionSignature_IOExceptionCreate) {
    nimcp_io_exception_t* iex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_NOT_FOUND,     // nimcp_error_t code
        EXCEPTION_SEVERITY_ERROR,       // nimcp_exception_severity_t severity
        __FILE__,                       // const char* file
        __LINE__,                       // int line
        __func__,                       // const char* func
        "/path/to/file",                // const char* path
        "File not found"                // const char* format
    );
    ASSERT_NE(iex, nullptr);
    nimcp_exception_unref((nimcp_exception_t*)iex);
}

TEST_F(ExceptionAPIRegressionTest, FunctionSignature_ThreadingExceptionCreate) {
    nimcp_threading_exception_t* tex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,           // nimcp_error_t code
        EXCEPTION_SEVERITY_CRITICAL,    // nimcp_exception_severity_t severity
        __FILE__,                       // const char* file
        __LINE__,                       // int line
        __func__,                       // const char* func
        12345,                          // uint64_t thread_id
        "Deadlock detected"             // const char* format
    );
    ASSERT_NE(tex, nullptr);
    nimcp_exception_unref((nimcp_exception_t*)tex);
}

TEST_F(ExceptionAPIRegressionTest, FunctionSignature_SecurityExceptionCreate) {
    nimcp_security_exception_t* sex = nimcp_security_exception_create(
        NIMCP_ERROR_SECURITY_THREAT,    // nimcp_error_t code
        EXCEPTION_SEVERITY_CRITICAL,    // nimcp_exception_severity_t severity
        __FILE__,                       // const char* file
        __LINE__,                       // int line
        __func__,                       // const char* func
        1,                              // uint32_t threat_type
        "Security threat"               // const char* format
    );
    ASSERT_NE(sex, nullptr);
    nimcp_exception_unref((nimcp_exception_t*)sex);
}

TEST_F(ExceptionAPIRegressionTest, FunctionSignature_GPUExceptionCreate) {
    nimcp_gpu_exception_t* gex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU,                // nimcp_error_t code
        EXCEPTION_SEVERITY_ERROR,       // nimcp_exception_severity_t severity
        __FILE__,                       // const char* file
        __LINE__,                       // int line
        __func__,                       // const char* func
        0,                              // int device_id
        1,                              // int cuda_error
        "GPU error"                     // const char* format
    );
    ASSERT_NE(gex, nullptr);
    nimcp_exception_unref((nimcp_exception_t*)gex);
}

/* ============================================================================
 * SECTION 9: Callback Interface Stability Tests
 * ============================================================================ */

static std::atomic<int> s_callback_invocations{0};
static std::atomic<bool> s_handler_called{false};

static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    s_callback_invocations.fetch_add(1);
    s_handler_called.store(true);
    EXPECT_NE(ex, nullptr);
    return false;  // Don't consume exception
}

TEST_F(ExceptionAPIRegressionTest, CallbackInterface_HandlerSignature) {
    s_handler_called.store(false);
    s_callback_invocations.store(0);

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "callback_test";
    opts.handler = test_exception_handler;
    opts.user_data = nullptr;
    opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    opts.category_filter = static_cast<nimcp_exception_category_t>(0);  // All
    opts.min_severity = EXCEPTION_SEVERITY_DEBUG;
    opts.type_filter = static_cast<nimcp_exception_type_t>(0);  // All

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Callback test"
    );

    nimcp_exception_dispatch(ex);
    EXPECT_TRUE(s_handler_called.load());
    EXPECT_EQ(s_callback_invocations.load(), 1);

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(reg);
}

static int test_recovery_callback(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action,
    void* user_data
) {
    (void)ex;
    (void)action;
    (void)user_data;
    s_callback_invocations.fetch_add(1);
    return 0;  // Success
}

TEST_F(ExceptionAPIRegressionTest, CallbackInterface_RecoveryCallbackSignature) {
    s_callback_invocations.store(0);

    int result = nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_GC,
        test_recovery_callback,
        nullptr
    );
    EXPECT_EQ(result, 0);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Recovery test"
    );

    result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(s_callback_invocations.load(), 1);

    nimcp_exception_unref(ex);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
}

/* ============================================================================
 * SECTION 10: Thread-Safety Guarantee Tests
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, ThreadSafety_ExceptionCreateFromMultipleThreads) {
    constexpr int NUM_THREADS = 4;
    constexpr int ITERATIONS = 100;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    auto thread_func = [&success_count, &failure_count]() {
        for (int i = 0; i < ITERATIONS; ++i) {
            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_OPERATION_FAILED,
                EXCEPTION_SEVERITY_ERROR,
                __FILE__, __LINE__, __func__,
                "Thread test %d", i
            );

            if (ex != nullptr) {
                success_count.fetch_add(1);
                nimcp_exception_unref(ex);
            } else {
                failure_count.fetch_add(1);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS * ITERATIONS);
    EXPECT_EQ(failure_count.load(), 0);
}

TEST_F(ExceptionAPIRegressionTest, ThreadSafety_ThreadLocalCurrentException) {
    constexpr int NUM_THREADS = 4;
    std::atomic<int> success_count{0};

    auto thread_func = [&success_count](int thread_id) {
        // Each thread sets its own current exception
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED + thread_id,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Thread %d exception", thread_id
        );

        nimcp_exception_set_current(ex);

        // Verify it's our exception
        nimcp_exception_t* current = nimcp_exception_get_current();
        if (current && current->code == NIMCP_ERROR_OPERATION_FAILED + thread_id) {
            success_count.fetch_add(1);
        }

        nimcp_exception_clear_current();
        nimcp_exception_unref(ex);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(thread_func, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS);
}

TEST_F(ExceptionAPIRegressionTest, ThreadSafety_RefCountConcurrentAccess) {
    // Create one exception, pass to multiple threads
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Shared exception"
    );
    ASSERT_NE(ex, nullptr);

    constexpr int NUM_THREADS = 4;
    constexpr int ITERATIONS = 100;

    auto thread_func = [ex]() {
        for (int i = 0; i < ITERATIONS; ++i) {
            nimcp_exception_ref(ex);
            // Brief pause to increase contention
            std::this_thread::yield();
            nimcp_exception_unref(ex);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    // After all threads finish, ref_count should be back to 1
    EXPECT_EQ(ex->ref_count, 1);
    nimcp_exception_unref(ex);
}

TEST_F(ExceptionAPIRegressionTest, ThreadSafety_HandlerRegistrationConcurrent) {
    // Test concurrent handler registration/unregistration
    constexpr int NUM_THREADS = 4;
    constexpr int ITERATIONS = 20;
    std::atomic<int> success_count{0};

    auto thread_func = [&success_count](int thread_id) {
        for (int i = 0; i < ITERATIONS; ++i) {
            char name[64];
            snprintf(name, sizeof(name), "handler_%d_%d", thread_id, i);

            nimcp_handler_options_t opts;
            nimcp_handler_default_options(&opts);
            opts.name = name;
            opts.handler = test_exception_handler;

            nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
            if (reg != nullptr) {
                success_count.fetch_add(1);
                nimcp_handler_unregister(reg);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(thread_func, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    // All registrations should succeed
    EXPECT_EQ(success_count.load(), NUM_THREADS * ITERATIONS);
}

TEST_F(ExceptionAPIRegressionTest, ThreadSafety_ConcurrentDispatch) {
    // Test concurrent exception dispatch
    std::atomic<int> dispatch_count{0};

    auto handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        counter->fetch_add(1);
        return true;  // Mark handled
    };

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "concurrent_dispatch_handler";
    opts.handler = handler;
    opts.user_data = &dispatch_count;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    constexpr int NUM_THREADS = 4;
    constexpr int ITERATIONS = 50;

    auto thread_func = []() {
        for (int i = 0; i < ITERATIONS; ++i) {
            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_OPERATION_FAILED,
                EXCEPTION_SEVERITY_ERROR,
                __FILE__, __LINE__, __func__,
                "Concurrent dispatch %d", i
            );
            nimcp_exception_dispatch(ex);
            nimcp_exception_unref(ex);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(dispatch_count.load(), NUM_THREADS * ITERATIONS);
    nimcp_handler_unregister(reg);
}

/* ============================================================================
 * SECTION 11: Public Macro Behavior Tests
 * ============================================================================ */

// Test helper functions that simulate functions using macros
static nimcp_error_t test_nimcp_check_throw_returns_on_failure(bool should_fail) {
    NIMCP_CHECK_THROW(should_fail == false, NIMCP_ERROR_INVALID_PARAM, "Expected failure");
    return NIMCP_SUCCESS;
}

TEST_F(ExceptionAPIRegressionTest, MacroBehavior_NIMCP_CHECK_THROW) {
    // Install handler to capture exceptions
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "macro_test_handler";
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        return true;  // Mark handled
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Should return success when condition is true
    nimcp_error_t result = test_nimcp_check_throw_returns_on_failure(false);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Should return error when condition is false
    result = test_nimcp_check_throw_returns_on_failure(true);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);

    nimcp_handler_unregister(reg);
}

static nimcp_error_t test_nimcp_check_throw_with_multiple_codes(int code_type) {
    switch (code_type) {
        case 0:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "NULL pointer");
            break;
        case 1:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE, "Out of range");
            break;
        case 2:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_NO_MEMORY, "No memory");
            break;
        default:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "Invalid state");
            break;
    }
    return NIMCP_SUCCESS;
}

TEST_F(ExceptionAPIRegressionTest, MacroBehavior_NIMCP_CHECK_THROW_ReturnsCorrectErrorCode) {
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "error_code_test_handler";
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Test that each error code is returned correctly
    EXPECT_EQ(test_nimcp_check_throw_with_multiple_codes(0), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(test_nimcp_check_throw_with_multiple_codes(1), NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(test_nimcp_check_throw_with_multiple_codes(2), NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(test_nimcp_check_throw_with_multiple_codes(3), NIMCP_ERROR_INVALID_STATE);

    nimcp_handler_unregister(reg);
}

static nimcp_error_t test_nimcp_throw_if(bool condition) {
    NIMCP_THROW_IF(!condition, NIMCP_ERROR_NULL_POINTER, "Condition was false");
    return NIMCP_SUCCESS;
}

TEST_F(ExceptionAPIRegressionTest, MacroBehavior_NIMCP_THROW_IF) {
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "throw_if_handler";
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // NIMCP_THROW_IF throws when condition is FALSE
    // No return, so we can continue - but exception was thrown
    nimcp_error_t result = test_nimcp_throw_if(true);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // When condition is false, throws but continues
    result = test_nimcp_throw_if(false);
    // Function returns normally, but exception was created
    EXPECT_EQ(result, NIMCP_SUCCESS);

    nimcp_handler_unregister(reg);
}

/* ============================================================================
 * SECTION 12: Severity Mapping from Error Codes
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, SeverityMapping_GenericErrors) {
    // Generic errors should map to ERROR severity
    nimcp_exception_severity_t sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_UNKNOWN);
    EXPECT_EQ(sev, EXCEPTION_SEVERITY_ERROR);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(sev, EXCEPTION_SEVERITY_ERROR);
}

TEST_F(ExceptionAPIRegressionTest, SeverityMapping_MemoryErrors) {
    // Memory errors should map to SEVERE
    nimcp_exception_severity_t sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_NO_MEMORY);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_ERROR);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_MEMORY_CORRUPTION);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_SEVERE);
}

TEST_F(ExceptionAPIRegressionTest, SeverityMapping_SignalErrors) {
    // Signal errors should map to CRITICAL or FATAL
    nimcp_exception_severity_t sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_SIGSEGV);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_CRITICAL);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_SIGABRT);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_CRITICAL);
}

TEST_F(ExceptionAPIRegressionTest, SeverityMapping_SecurityErrors) {
    // Security errors should map to CRITICAL
    nimcp_exception_severity_t sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_SEVERE);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_BBB_REJECTED);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_ERROR);
}

TEST_F(ExceptionAPIRegressionTest, CategoryMapping_FromErrorCodes) {
    // Verify category is correctly determined from error codes
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_UNKNOWN), EXCEPTION_CATEGORY_GENERIC);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_NO_MEMORY), EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_BRAIN_CREATION), EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_FILE_NOT_FOUND), EXCEPTION_CATEGORY_IO);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_CONFIG_INVALID), EXCEPTION_CATEGORY_CONFIG);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_THREAD_CREATE), EXCEPTION_CATEGORY_THREADING);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_SIGSEGV), EXCEPTION_CATEGORY_SIGNAL);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_WORKING_MEMORY), EXCEPTION_CATEGORY_COGNITIVE);
}

/* ============================================================================
 * SECTION 13: String Conversion Stability Tests
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, StringConversion_SeverityToString) {
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_DEBUG), "DEBUG");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_INFO), "INFO");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_WARNING), "WARNING");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_ERROR), "ERROR");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_SEVERE), "SEVERE");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_CRITICAL), "CRITICAL");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_FATAL), "FATAL");
}

TEST_F(ExceptionAPIRegressionTest, StringConversion_CategoryToString) {
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_GENERIC), "GENERIC");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_MEMORY), "MEMORY");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_BRAIN), "BRAIN");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_IO), "IO");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_CONFIG), "CONFIG");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_THREADING), "THREADING");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_SIGNAL), "SIGNAL");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_COGNITIVE), "COGNITIVE");
}

TEST_F(ExceptionAPIRegressionTest, StringConversion_TypeToString) {
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_BASE), "BASE");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_MEMORY), "MEMORY");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_BRAIN), "BRAIN");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_IO), "IO");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_THREADING), "THREADING");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_SECURITY), "SECURITY");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_COGNITIVE), "COGNITIVE");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_GPU), "GPU");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_AGGREGATE), "AGGREGATE");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_SIGNAL), "SIGNAL");
}

TEST_F(ExceptionAPIRegressionTest, StringConversion_RecoveryActionToString) {
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_NONE), "NONE");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RETRY), "RETRY");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GC), "GC");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_COMPACT), "COMPACT");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_ROLLBACK), "ROLLBACK");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RESTART_THREAD), "RESTART_THREAD");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RESTART_COMPONENT), "RESTART_COMPONENT");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_QUARANTINE), "QUARANTINE");
}

/* ============================================================================
 * SECTION 14: Error Helper Function Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, ErrorHelpers_IsSuccess) {
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS));
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS_WITH_WARNINGS));
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS_PARTIAL));

    EXPECT_FALSE(nimcp_error_is_success(NIMCP_ERROR_UNKNOWN));
    EXPECT_FALSE(nimcp_error_is_success(NIMCP_ERROR_NO_MEMORY));
    EXPECT_FALSE(nimcp_error_is_success(-1));
}

TEST_F(ExceptionAPIRegressionTest, ErrorHelpers_IsFailure) {
    EXPECT_FALSE(nimcp_error_is_failure(NIMCP_SUCCESS));
    EXPECT_FALSE(nimcp_error_is_failure(NIMCP_SUCCESS_WITH_WARNINGS));

    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_UNKNOWN));
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_NO_MEMORY));
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_SIGSEGV));
}

TEST_F(ExceptionAPIRegressionTest, ErrorHelpers_GetCategory) {
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_UNKNOWN), 1);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_NO_MEMORY), 2);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_BRAIN_CREATION), 3);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_FILE_NOT_FOUND), 4);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_CONFIG_INVALID), 5);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_THREAD_CREATE), 6);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_SIGSEGV), 7);
}

TEST_F(ExceptionAPIRegressionTest, ErrorHelpers_FEPCompatibility) {
    // FEP bridge helpers
    EXPECT_TRUE(nimcp_is_ok(NIMCP_SUCCESS));
    EXPECT_FALSE(nimcp_is_ok(NIMCP_ERROR_UNKNOWN));

    EXPECT_FALSE(nimcp_is_error(NIMCP_SUCCESS));
    EXPECT_TRUE(nimcp_is_error(NIMCP_ERROR_UNKNOWN));

    EXPECT_EQ(nimcp_from_fep_result(0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_from_fep_result(-1), NIMCP_ERROR_OPERATION_FAILED);

    EXPECT_EQ(nimcp_to_fep_result(NIMCP_SUCCESS), 0);
    EXPECT_EQ(nimcp_to_fep_result(NIMCP_ERROR_UNKNOWN), -1);
}

/* ============================================================================
 * SECTION 15: Context API Stability Tests
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, ContextAPI_SetGetRemove) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Context test"
    );
    ASSERT_NE(ex, nullptr);

    // Set context
    EXPECT_EQ(nimcp_exception_set_context(ex, "key1", "value1"), 0);
    EXPECT_EQ(nimcp_exception_set_context(ex, "key2", "value2"), 0);

    // Get context
    EXPECT_STREQ(nimcp_exception_get_context(ex, "key1"), "value1");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "key2"), "value2");
    EXPECT_EQ(nimcp_exception_get_context(ex, "nonexistent"), nullptr);

    // Count
    EXPECT_EQ(nimcp_exception_context_count(ex), 2UL);

    // Remove
    EXPECT_EQ(nimcp_exception_remove_context(ex, "key1"), 0);
    EXPECT_EQ(nimcp_exception_context_count(ex), 1UL);
    EXPECT_EQ(nimcp_exception_get_context(ex, "key1"), nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionAPIRegressionTest, ContextAPI_NullSafety) {
    EXPECT_EQ(nimcp_exception_set_context(nullptr, "key", "value"), -1);
    EXPECT_EQ(nimcp_exception_get_context(nullptr, "key"), nullptr);
    EXPECT_EQ(nimcp_exception_remove_context(nullptr, "key"), -1);
    EXPECT_EQ(nimcp_exception_context_count(nullptr), 0UL);
}

TEST_F(ExceptionAPIRegressionTest, ContextAPI_MaxEntries) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Max entries test"
    );
    ASSERT_NE(ex, nullptr);

    // Fill context to max capacity
    for (int i = 0; i < NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES; ++i) {
        char key[32], value[64];
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(value, sizeof(value), "value%d", i);
        EXPECT_EQ(nimcp_exception_set_context(ex, key, value), 0);
    }

    // Next entry should fail
    EXPECT_EQ(nimcp_exception_set_context(ex, "overflow_key", "overflow_value"), -1);
    EXPECT_EQ(nimcp_exception_context_count(ex), static_cast<size_t>(NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES));

    nimcp_exception_unref(ex);
}

/* ============================================================================
 * SECTION 16: Aggregate Exception API Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, AggregateAPI_CreateAddGet) {
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Aggregate test"
    );
    ASSERT_NE(agg, nullptr);
    EXPECT_EQ(agg->base.type, EXCEPTION_TYPE_AGGREGATE);

    // Add children
    for (int i = 0; i < 5; ++i) {
        nimcp_exception_t* child = nimcp_exception_create(
            NIMCP_ERROR_NULL_POINTER + i,
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "Child %d", i
        );
        EXPECT_EQ(nimcp_aggregate_exception_add(agg, child), 0);
    }

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 5UL);

    // Get children
    for (size_t i = 0; i < 5; ++i) {
        nimcp_exception_t* child = nimcp_aggregate_exception_get(agg, i);
        EXPECT_NE(child, nullptr);
        EXPECT_EQ(child->code, NIMCP_ERROR_NULL_POINTER + static_cast<int>(i));
    }

    // Out of bounds
    EXPECT_EQ(nimcp_aggregate_exception_get(agg, 10), nullptr);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

TEST_F(ExceptionAPIRegressionTest, AggregateAPI_NullSafety) {
    EXPECT_EQ(nimcp_aggregate_exception_count(nullptr), 0UL);
    EXPECT_EQ(nimcp_aggregate_exception_get(nullptr, 0), nullptr);
    EXPECT_EQ(nimcp_aggregate_exception_add(nullptr, nullptr), -1);
}

TEST_F(ExceptionAPIRegressionTest, AggregateAPI_MaxChildren) {
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Max children test"
    );
    ASSERT_NE(agg, nullptr);

    // Fill to max capacity
    for (int i = 0; i < NIMCP_EXCEPTION_MAX_CHILDREN; ++i) {
        nimcp_exception_t* child = nimcp_exception_create(
            NIMCP_ERROR_UNKNOWN + i,
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "Child %d", i
        );
        EXPECT_EQ(nimcp_aggregate_exception_add(agg, child), 0);
    }

    // Next should fail
    nimcp_exception_t* overflow_child = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Overflow child"
    );
    EXPECT_EQ(nimcp_aggregate_exception_add(agg, overflow_child), -1);
    nimcp_exception_unref(overflow_child);

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), static_cast<size_t>(NIMCP_EXCEPTION_MAX_CHILDREN));

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

/* ============================================================================
 * SECTION 17: Exception Chaining Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, Chaining_SetAndGetCause) {
    nimcp_exception_t* cause = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Root cause"
    );

    nimcp_exception_t* wrapper = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Wrapper exception"
    );

    ASSERT_NE(cause, nullptr);
    ASSERT_NE(wrapper, nullptr);

    // Take extra ref since set_cause takes ownership
    nimcp_exception_ref(cause);
    nimcp_exception_set_cause(wrapper, cause);

    EXPECT_EQ(nimcp_exception_get_cause(wrapper), cause);
    EXPECT_EQ(nimcp_exception_get_cause(cause), nullptr);

    nimcp_exception_unref(cause);
    nimcp_exception_unref(wrapper);
}

TEST_F(ExceptionAPIRegressionTest, Chaining_DeepChain) {
    // Create a chain of 5 exceptions
    nimcp_exception_t* current = nullptr;

    for (int i = 0; i < 5; ++i) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED + i,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Chain level %d", i
        );
        if (current) {
            nimcp_exception_set_cause(ex, current);
        }
        current = ex;
    }

    // Traverse the chain
    int depth = 0;
    nimcp_exception_t* iter = current;
    while (iter) {
        depth++;
        iter = nimcp_exception_get_cause(iter);
    }
    EXPECT_EQ(depth, 5);

    // Clean up (unreffing top should cascade)
    nimcp_exception_unref(current);
}

/* ============================================================================
 * SECTION 18: Handler Registration Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, HandlerRegistration_DefaultOptions) {
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);

    // Verify defaults
    EXPECT_EQ(opts.priority, NIMCP_HANDLER_PRIORITY_NORMAL);
    EXPECT_EQ(opts.category_filter, static_cast<nimcp_exception_category_t>(0));
    EXPECT_EQ(opts.min_severity, EXCEPTION_SEVERITY_DEBUG);
    EXPECT_EQ(opts.type_filter, static_cast<nimcp_exception_type_t>(0));
}

TEST_F(ExceptionAPIRegressionTest, HandlerRegistration_EnableDisable) {
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "toggle_test";
    opts.handler = test_exception_handler;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    EXPECT_TRUE(reg->active);

    nimcp_handler_disable(reg);
    EXPECT_FALSE(reg->active);

    nimcp_handler_enable(reg);
    EXPECT_TRUE(reg->active);

    nimcp_handler_unregister(reg);
}

TEST_F(ExceptionAPIRegressionTest, HandlerRegistration_PriorityOrdering) {
    // Register handlers with different priorities
    std::vector<int> call_order;

    auto handler_high = [](nimcp_exception_t* ex, void* user_data) -> bool {
        auto* order = static_cast<std::vector<int>*>(user_data);
        order->push_back(1);  // High priority
        return false;
    };

    auto handler_normal = [](nimcp_exception_t* ex, void* user_data) -> bool {
        auto* order = static_cast<std::vector<int>*>(user_data);
        order->push_back(2);  // Normal priority
        return false;
    };

    auto handler_low = [](nimcp_exception_t* ex, void* user_data) -> bool {
        auto* order = static_cast<std::vector<int>*>(user_data);
        order->push_back(3);  // Low priority
        return false;
    };

    nimcp_handler_options_t opts_low, opts_normal, opts_high;
    nimcp_handler_default_options(&opts_low);
    nimcp_handler_default_options(&opts_normal);
    nimcp_handler_default_options(&opts_high);

    opts_low.name = "low";
    opts_low.priority = NIMCP_HANDLER_PRIORITY_LOW;
    opts_low.handler = handler_low;
    opts_low.user_data = &call_order;

    opts_normal.name = "normal";
    opts_normal.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    opts_normal.handler = handler_normal;
    opts_normal.user_data = &call_order;

    opts_high.name = "high";
    opts_high.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts_high.handler = handler_high;
    opts_high.user_data = &call_order;

    // Register in reverse order to test sorting
    nimcp_handler_registration_t* reg_low = nimcp_handler_register(&opts_low);
    nimcp_handler_registration_t* reg_normal = nimcp_handler_register(&opts_normal);
    nimcp_handler_registration_t* reg_high = nimcp_handler_register(&opts_high);

    ASSERT_NE(reg_low, nullptr);
    ASSERT_NE(reg_normal, nullptr);
    ASSERT_NE(reg_high, nullptr);

    // Dispatch exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Priority test"
    );
    nimcp_exception_dispatch(ex);
    nimcp_exception_unref(ex);

    // Verify call order: high (1) -> normal (2) -> low (3)
    ASSERT_EQ(call_order.size(), 3UL);
    EXPECT_EQ(call_order[0], 1);  // High first
    EXPECT_EQ(call_order[1], 2);  // Normal second
    EXPECT_EQ(call_order[2], 3);  // Low last

    nimcp_handler_unregister(reg_low);
    nimcp_handler_unregister(reg_normal);
    nimcp_handler_unregister(reg_high);
}

/* ============================================================================
 * SECTION 19: System Lifecycle Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, SystemLifecycle_InitShutdownCycle) {
    // Already initialized in SetUp, shutdown
    nimcp_exception_system_shutdown();
    EXPECT_FALSE(nimcp_exception_system_is_initialized());

    // Re-initialize
    EXPECT_EQ(nimcp_exception_system_init(), 0);
    EXPECT_TRUE(nimcp_exception_system_is_initialized());

    // Multiple init calls should be idempotent
    EXPECT_EQ(nimcp_exception_system_init(), 0);
    EXPECT_TRUE(nimcp_exception_system_is_initialized());
}

/* ============================================================================
 * SECTION 20: Exception Formatting Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, Formatting_ExceptionToString) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Format test message"
    );
    ASSERT_NE(ex, nullptr);

    char buffer[1024];
    size_t written = nimcp_exception_to_string(ex, buffer, sizeof(buffer));

    EXPECT_GT(written, 0UL);
    EXPECT_NE(strstr(buffer, "ERROR"), nullptr);  // Severity should be in output
    EXPECT_NE(strstr(buffer, "1006"), nullptr);   // Error code should be in output

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionAPIRegressionTest, Formatting_ToStringNullSafety) {
    char buffer[256];
    EXPECT_EQ(nimcp_exception_to_string(nullptr, buffer, sizeof(buffer)), 0UL);
    EXPECT_EQ(nimcp_exception_to_string(nullptr, nullptr, 0), 0UL);
}

TEST_F(ExceptionAPIRegressionTest, Formatting_ToStringBufferSizes) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Buffer size test message"
    );
    ASSERT_NE(ex, nullptr);

    // Very small buffer
    char small_buffer[32];
    size_t written = nimcp_exception_to_string(ex, small_buffer, sizeof(small_buffer));
    EXPECT_LE(written, sizeof(small_buffer) - 1);

    // Large buffer
    char large_buffer[4096];
    written = nimcp_exception_to_string(ex, large_buffer, sizeof(large_buffer));
    EXPECT_GT(written, 0UL);

    nimcp_exception_unref(ex);
}

/* ============================================================================
 * SECTION 21: Immune Integration Enum Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, ImmuneEnums_AntigenSourceValues) {
    EXPECT_EQ(static_cast<int>(EX_ANTIGEN_SOURCE_BBB), 0);
    EXPECT_EQ(static_cast<int>(EX_ANTIGEN_SOURCE_BFT), 1);
    EXPECT_EQ(static_cast<int>(EX_ANTIGEN_SOURCE_ANOMALY), 2);
    EXPECT_EQ(static_cast<int>(EX_ANTIGEN_SOURCE_SWARM), 3);
    EXPECT_EQ(static_cast<int>(EX_ANTIGEN_SOURCE_MANUAL), 4);
}

TEST_F(ExceptionAPIRegressionTest, ImmuneConversion_CategoryToAntigenSource) {
    // Memory -> ANOMALY
    EXPECT_EQ(
        nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_MEMORY),
        EX_ANTIGEN_SOURCE_ANOMALY
    );

    // Security -> BBB
    EXPECT_EQ(
        nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_SECURITY),
        EX_ANTIGEN_SOURCE_BBB
    );

    // Threading -> BFT
    EXPECT_EQ(
        nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_THREADING),
        EX_ANTIGEN_SOURCE_BFT
    );
}

TEST_F(ExceptionAPIRegressionTest, ImmuneConversion_SeverityMapping) {
    // Verify severity maps to 1-10 scale
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_DEBUG), 1U);
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_INFO), 2U);
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_WARNING), 3U);
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_ERROR), 5U);
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_SEVERE), 7U);
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_CRITICAL), 9U);
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_FATAL), 10U);
}

TEST_F(ExceptionAPIRegressionTest, ImmuneIntegration_PresentToImmuneSetsFlag) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Immune presentation test"
    );
    ASSERT_NE(ex, nullptr);

    // Before presentation
    EXPECT_FALSE(ex->presented_to_immune);

    // Present to immune (may fail if immune not connected, but flag should be set)
    nimcp_exception_present_to_immune(ex, nullptr);

    // After presentation, flag should be set
    EXPECT_TRUE(ex->presented_to_immune);

    nimcp_exception_unref(ex);
}

/* ============================================================================
 * SECTION 22: Epitope Generation Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, Epitope_GenerationReturnsValidLength) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Epitope test"
    );
    ASSERT_NE(ex, nullptr);

    size_t len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(len, 0UL);
    EXPECT_LE(len, static_cast<size_t>(NIMCP_EXCEPTION_EPITOPE_SIZE));

    // Epitope should be populated
    EXPECT_EQ(ex->epitope_len, len);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionAPIRegressionTest, Epitope_SameExceptionProducesSameEpitope) {
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_ERROR,
        "test.c", 100, "test_func",
        "Same message"
    );

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_ERROR,
        "test.c", 100, "test_func",
        "Same message"
    );

    ASSERT_NE(ex1, nullptr);
    ASSERT_NE(ex2, nullptr);

    nimcp_exception_generate_epitope(ex1);
    nimcp_exception_generate_epitope(ex2);

    // Same exception properties should produce same epitope
    EXPECT_EQ(ex1->epitope_len, ex2->epitope_len);
    EXPECT_EQ(memcmp(ex1->epitope, ex2->epitope, ex1->epitope_len), 0);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

TEST_F(ExceptionAPIRegressionTest, Epitope_DifferentExceptionsProduceDifferentEpitopes) {
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_ERROR,
        "test.c", 100, "test_func",
        "Message 1"
    );

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_OUT_OF_RANGE,  // Different error code
        EXCEPTION_SEVERITY_ERROR,
        "test.c", 100, "test_func",
        "Message 2"
    );

    ASSERT_NE(ex1, nullptr);
    ASSERT_NE(ex2, nullptr);

    nimcp_exception_generate_epitope(ex1);
    nimcp_exception_generate_epitope(ex2);

    // Different error codes should produce different epitopes
    EXPECT_NE(memcmp(ex1->epitope, ex2->epitope, ex1->epitope_len), 0);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

/* ============================================================================
 * SECTION 23: Brain Region Error Code Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, BrainRegionErrors_BaseValues) {
    EXPECT_EQ(NIMCP_ERROR_MOTOR_BASE, 10000);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_BASE, 10100);
    EXPECT_EQ(NIMCP_ERROR_ENTORHINAL_BASE, 10200);
    EXPECT_EQ(NIMCP_ERROR_PREFRONTAL_BASE, 10300);
    EXPECT_EQ(NIMCP_ERROR_CEREBELLUM_BASE, 10400);
    EXPECT_EQ(NIMCP_ERROR_THALAMUS_BASE, 10500);
    EXPECT_EQ(NIMCP_ERROR_HYPOTHALAMUS_BASE, 10600);
    EXPECT_EQ(NIMCP_ERROR_AMYGDALA_BASE, 10700);
    EXPECT_EQ(NIMCP_ERROR_BASAL_GANGLIA_BASE, 10800);
    EXPECT_EQ(NIMCP_ERROR_CINGULATE_BASE, 10900);
    EXPECT_EQ(NIMCP_ERROR_INSULA_BASE, 11000);
    EXPECT_EQ(NIMCP_ERROR_OCCIPITAL_BASE, 11100);
    EXPECT_EQ(NIMCP_ERROR_PARIETAL_BASE, 11200);
    EXPECT_EQ(NIMCP_ERROR_TEMPORAL_BASE, 11300);
}

TEST_F(ExceptionAPIRegressionTest, BrainRegionErrors_IsBrainRegion) {
    EXPECT_TRUE(nimcp_error_is_brain_region(10000));  // Motor base
    EXPECT_TRUE(nimcp_error_is_brain_region(15000));  // In range
    EXPECT_TRUE(nimcp_error_is_brain_region(19999));  // Upper bound

    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_SUCCESS));
    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_ERROR_NO_MEMORY));
    EXPECT_FALSE(nimcp_error_is_brain_region(20000));  // Above range
}

/* ============================================================================
 * SECTION 24: Cleanup Stack API Stability
 * ============================================================================ */

static void test_cleanup_func(void* resource) {
    if (resource) {
        int* counter = static_cast<int*>(resource);
        (*counter)++;
    }
}

TEST_F(ExceptionAPIRegressionTest, CleanupStack_PushExecute) {
    nimcp_cleanup_stack_t stack;
    nimcp_cleanup_init(&stack);
    EXPECT_EQ(stack.count, 0UL);

    int cleanup_counter = 0;

    EXPECT_TRUE(nimcp_cleanup_push(&stack, test_cleanup_func, &cleanup_counter, "test1"));
    EXPECT_TRUE(nimcp_cleanup_push(&stack, test_cleanup_func, &cleanup_counter, "test2"));
    EXPECT_EQ(stack.count, 2UL);

    nimcp_cleanup_execute(&stack);

    EXPECT_EQ(cleanup_counter, 2);
    EXPECT_EQ(stack.count, 0UL);
}

TEST_F(ExceptionAPIRegressionTest, CleanupStack_ClearDoesNotExecute) {
    nimcp_cleanup_stack_t stack;
    nimcp_cleanup_init(&stack);

    int cleanup_counter = 0;

    nimcp_cleanup_push(&stack, test_cleanup_func, &cleanup_counter, "test");
    nimcp_cleanup_clear(&stack);

    EXPECT_EQ(cleanup_counter, 0);
    EXPECT_EQ(stack.count, 0UL);
}

TEST_F(ExceptionAPIRegressionTest, CleanupStack_MaxEntries) {
    EXPECT_EQ(NIMCP_CLEANUP_STACK_MAX, 32);
}

TEST_F(ExceptionAPIRegressionTest, CleanupStack_LIFOOrder) {
    nimcp_cleanup_stack_t stack;
    nimcp_cleanup_init(&stack);

    std::vector<int> cleanup_order;

    auto cleanup_with_order = [](void* resource) {
        auto* order = static_cast<std::pair<std::vector<int>*, int>*>(resource);
        order->first->push_back(order->second);
    };

    std::pair<std::vector<int>*, int> data1{&cleanup_order, 1};
    std::pair<std::vector<int>*, int> data2{&cleanup_order, 2};
    std::pair<std::vector<int>*, int> data3{&cleanup_order, 3};

    nimcp_cleanup_push(&stack, cleanup_with_order, &data1, "first");
    nimcp_cleanup_push(&stack, cleanup_with_order, &data2, "second");
    nimcp_cleanup_push(&stack, cleanup_with_order, &data3, "third");

    nimcp_cleanup_execute(&stack);

    // LIFO order: 3, 2, 1
    ASSERT_EQ(cleanup_order.size(), 3UL);
    EXPECT_EQ(cleanup_order[0], 3);  // Last pushed, first executed
    EXPECT_EQ(cleanup_order[1], 2);
    EXPECT_EQ(cleanup_order[2], 1);  // First pushed, last executed
}

/* ============================================================================
 * SECTION 25: Try/Catch Context Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, TryContext_InTryBlockInitiallyFalse) {
    EXPECT_FALSE(nimcp_in_try_block());
}

TEST_F(ExceptionAPIRegressionTest, TryContext_PushPop) {
    nimcp_try_context_t ctx = {};
    ctx.file = __FILE__;
    ctx.line = __LINE__;
    ctx.function = __func__;

    EXPECT_EQ(nimcp_try_push(&ctx), 0);
    EXPECT_TRUE(nimcp_in_try_block());

    nimcp_try_context_t* current = nimcp_try_current();
    EXPECT_EQ(current, &ctx);

    nimcp_try_context_t* popped = nimcp_try_pop();
    EXPECT_EQ(popped, &ctx);
    EXPECT_FALSE(nimcp_in_try_block());
}

TEST_F(ExceptionAPIRegressionTest, TryContext_NestedPushPop) {
    nimcp_try_context_t ctx1 = {};
    nimcp_try_context_t ctx2 = {};
    nimcp_try_context_t ctx3 = {};

    EXPECT_EQ(nimcp_try_push(&ctx1), 0);
    EXPECT_EQ(nimcp_try_push(&ctx2), 0);
    EXPECT_EQ(nimcp_try_push(&ctx3), 0);

    EXPECT_EQ(nimcp_try_current(), &ctx3);

    EXPECT_EQ(nimcp_try_pop(), &ctx3);
    EXPECT_EQ(nimcp_try_current(), &ctx2);

    EXPECT_EQ(nimcp_try_pop(), &ctx2);
    EXPECT_EQ(nimcp_try_current(), &ctx1);

    EXPECT_EQ(nimcp_try_pop(), &ctx1);
    EXPECT_FALSE(nimcp_in_try_block());
}

/* ============================================================================
 * SECTION 26: Error Context API Stability
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, ErrorContext_SetAndGet) {
    nimcp_set_error_ex(
        NIMCP_ERROR_NULL_POINTER,
        __FILE__, __LINE__, __func__,
        "Test error: %s", "details"
    );

    nimcp_error_t code = nimcp_get_last_error();
    EXPECT_EQ(code, NIMCP_ERROR_NULL_POINTER);

    const char* msg = nimcp_get_error_message();
    EXPECT_NE(msg, nullptr);
    EXPECT_NE(strstr(msg, "details"), nullptr);

    const nimcp_error_context_t* ctx = nimcp_get_error_context();
    EXPECT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->code, NIMCP_ERROR_NULL_POINTER);

    nimcp_error_clear();
}

TEST_F(ExceptionAPIRegressionTest, ErrorContext_Clear) {
    nimcp_set_error_ex(
        NIMCP_ERROR_OPERATION_FAILED,
        __FILE__, __LINE__, __func__,
        "Error to clear"
    );

    EXPECT_EQ(nimcp_get_last_error(), NIMCP_ERROR_OPERATION_FAILED);

    nimcp_error_clear();

    EXPECT_EQ(nimcp_get_last_error(), NIMCP_SUCCESS);
}

TEST_F(ExceptionAPIRegressionTest, ErrorContext_ThreadLocal) {
    std::atomic<bool> error_set{false};
    std::atomic<nimcp_error_t> thread_error{NIMCP_SUCCESS};

    auto thread_func = [&error_set, &thread_error]() {
        nimcp_set_error_ex(
            NIMCP_ERROR_THREAD_CREATE,
            __FILE__, __LINE__, __func__,
            "Thread-specific error"
        );
        error_set.store(true);

        // Wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Should still have our error
        thread_error.store(nimcp_get_last_error());

        nimcp_error_clear();
    };

    // Set main thread error
    nimcp_set_error_ex(
        NIMCP_ERROR_NULL_POINTER,
        __FILE__, __LINE__, __func__,
        "Main thread error"
    );

    std::thread t(thread_func);

    // Wait for thread to set its error
    while (!error_set.load()) {
        std::this_thread::yield();
    }

    // Main thread should still have its error
    EXPECT_EQ(nimcp_get_last_error(), NIMCP_ERROR_NULL_POINTER);

    t.join();

    // Thread should have had its own error
    EXPECT_EQ(thread_error.load(), NIMCP_ERROR_THREAD_CREATE);

    nimcp_error_clear();
}

/* ============================================================================
 * SECTION 27: Memory Management Tests
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, MemoryManagement_RefCountBasic) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Ref count test"
    );
    ASSERT_NE(ex, nullptr);

    EXPECT_EQ(ex->ref_count, 1);

    nimcp_exception_ref(ex);
    EXPECT_EQ(ex->ref_count, 2);

    nimcp_exception_ref(ex);
    EXPECT_EQ(ex->ref_count, 3);

    nimcp_exception_unref(ex);
    EXPECT_EQ(ex->ref_count, 2);

    nimcp_exception_unref(ex);
    EXPECT_EQ(ex->ref_count, 1);

    nimcp_exception_unref(ex);
    // After this, ex is freed - no more access
}

TEST_F(ExceptionAPIRegressionTest, MemoryManagement_NoLeaksInRapidCreateDestroy) {
    // Create and destroy many exceptions rapidly
    // If there's a memory leak, this test would show issues under sanitizers
    for (int i = 0; i < 1000; ++i) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Leak test iteration %d", i
        );
        ASSERT_NE(ex, nullptr);
        nimcp_exception_unref(ex);
    }
}

TEST_F(ExceptionAPIRegressionTest, MemoryManagement_TypedExceptionCleanup) {
    // Create each typed exception and verify cleanup
    {
        nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
            NIMCP_ERROR_NO_MEMORY, EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            1024, "Memory test"
        );
        ASSERT_NE(mex, nullptr);
        nimcp_exception_unref((nimcp_exception_t*)mex);
    }

    {
        nimcp_brain_exception_t* bex = nimcp_brain_exception_create(
            NIMCP_ERROR_FORWARD_PASS, EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            123, "prefrontal", "Brain test"
        );
        ASSERT_NE(bex, nullptr);
        nimcp_exception_unref((nimcp_exception_t*)bex);
    }

    {
        nimcp_io_exception_t* iex = nimcp_io_exception_create(
            NIMCP_ERROR_FILE_NOT_FOUND, EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "/path", "IO test"
        );
        ASSERT_NE(iex, nullptr);
        nimcp_exception_unref((nimcp_exception_t*)iex);
    }

    {
        nimcp_threading_exception_t* tex = nimcp_threading_exception_create(
            NIMCP_ERROR_DEADLOCK, EXCEPTION_SEVERITY_CRITICAL,
            __FILE__, __LINE__, __func__,
            12345, "Threading test"
        );
        ASSERT_NE(tex, nullptr);
        nimcp_exception_unref((nimcp_exception_t*)tex);
    }

    {
        nimcp_security_exception_t* sex = nimcp_security_exception_create(
            NIMCP_ERROR_SECURITY_THREAT, EXCEPTION_SEVERITY_CRITICAL,
            __FILE__, __LINE__, __func__,
            1, "Security test"
        );
        ASSERT_NE(sex, nullptr);
        nimcp_exception_unref((nimcp_exception_t*)sex);
    }

    {
        nimcp_gpu_exception_t* gex = nimcp_gpu_exception_create(
            NIMCP_ERROR_GPU, EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            0, 1, "GPU test"
        );
        ASSERT_NE(gex, nullptr);
        nimcp_exception_unref((nimcp_exception_t*)gex);
    }

    {
        nimcp_aggregate_exception_t* aex = nimcp_aggregate_exception_create(
            NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Aggregate test"
        );
        ASSERT_NE(aex, nullptr);
        nimcp_exception_unref((nimcp_exception_t*)aex);
    }
}

TEST_F(ExceptionAPIRegressionTest, MemoryManagement_DispatchWithHandledExceptions) {
    // Handler that marks exceptions as handled
    std::atomic<int> handled_count{0};

    auto handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        counter->fetch_add(1);
        return true;  // Mark handled - should be cleaned up
    };

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "cleanup_test_handler";
    opts.handler = handler;
    opts.user_data = &handled_count;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Create and dispatch many exceptions
    for (int i = 0; i < 500; ++i) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Cleanup dispatch test %d", i
        );
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    EXPECT_EQ(handled_count.load(), 500);

    nimcp_handler_unregister(reg);
}

/* ============================================================================
 * SECTION 28: Recovery Strategy Tests
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, RecoveryStrategy_GetStrategy) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Recovery strategy test"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Memory exceptions should have GC as a recovery action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionAPIRegressionTest, RecoveryStrategy_SuggestedRecovery) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Suggested recovery test"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_action_t action = nimcp_exception_get_suggested_recovery(ex);
    // Should suggest something for memory errors
    EXPECT_NE(action, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN);

    nimcp_exception_unref(ex);
}

/* ============================================================================
 * SECTION 29: API Stability After Exceptions
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, APIStability_ExceptionSystemRemainsUsable) {
    // WHAT: Test that exception system remains usable after exceptions
    // WHY:  API must remain stable and functional after error conditions
    // HOW:  Throw exceptions and verify system still works correctly

    // Throw many exceptions
    for (int i = 0; i < 100; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "API stability test exception %d", i);
        ASSERT_NE(ex, nullptr);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    // System should still be fully functional
    nimcp_exception_t* final_ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_DEBUG,
        __FILE__, __LINE__, __func__,
        "Final test exception");
    ASSERT_NE(final_ex, nullptr);
    EXPECT_EQ(final_ex->code, NIMCP_ERROR_UNKNOWN);
    EXPECT_EQ(final_ex->severity, EXCEPTION_SEVERITY_DEBUG);
    nimcp_exception_unref(final_ex);
}

TEST_F(ExceptionAPIRegressionTest, APIStability_HandlerChainAfterExceptions) {
    // WHAT: Test handler chain remains functional after many exceptions
    // WHY:  Handler chain is critical infrastructure
    // HOW:  Register handlers, throw exceptions, verify handlers still work

    std::atomic<int> handler_count{0};
    auto test_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        counter->fetch_add(1);
        return false;
    };

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "stability_handler";
    opts.handler = test_handler;
    opts.user_data = &handler_count;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Throw many exceptions
    for (int i = 0; i < 50; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_INVALID_PARAM + (i % 10),
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "Handler chain stability test %d", i);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    EXPECT_EQ(handler_count.load(), 50);

    // Handler should still work
    handler_count = 0;
    nimcp_exception_t* final_ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Final handler test");
    nimcp_exception_dispatch(final_ex);
    nimcp_exception_unref(final_ex);

    EXPECT_EQ(handler_count.load(), 1);

    nimcp_handler_unregister(reg);
}

TEST_F(ExceptionAPIRegressionTest, APIStability_ContextAPIAfterExceptions) {
    // WHAT: Test context API remains stable after exceptions
    // WHY:  Context management must work reliably
    // HOW:  Use context API extensively, verify consistent behavior

    for (int round = 0; round < 20; round++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Context API stability round %d", round);
        ASSERT_NE(ex, nullptr);

        // Add many context entries
        for (int i = 0; i < NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES; i++) {
            char key[64], value[128];
            snprintf(key, sizeof(key), "key_%d", i);
            snprintf(value, sizeof(value), "value_%d_round_%d", i, round);
            nimcp_exception_set_context(ex, key, value);
        }

        EXPECT_EQ(nimcp_exception_context_count(ex), NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES);

        // Verify all context entries
        for (int i = 0; i < (int)NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES; i++) {
            char key[64], expected_value[128];
            snprintf(key, sizeof(key), "key_%d", i);
            snprintf(expected_value, sizeof(expected_value), "value_%d_round_%d", i, round);
            EXPECT_STREQ(nimcp_exception_get_context(ex, key), expected_value);
        }

        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }
}

TEST_F(ExceptionAPIRegressionTest, APIStability_TypedExceptionsAfterErrors) {
    // WHAT: Test typed exception APIs work after previous exceptions
    // WHY:  All exception types must remain functional
    // HOW:  Create various typed exceptions after error conditions

    // First, throw some errors to "stress" the system
    for (int i = 0; i < 30; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "Pre-stress test %d", i);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    // Now verify all typed exception APIs work correctly
    // Memory exception
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024 * 1024,
        "Memory exception after stress");
    ASSERT_NE(mex, nullptr);
    EXPECT_EQ(mex->base.type, EXCEPTION_TYPE_MEMORY);
    EXPECT_EQ(mex->requested_size, 1024u * 1024u);
    nimcp_exception_unref((nimcp_exception_t*)mex);

    // Brain exception
    nimcp_brain_exception_t* bex = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        42, "hippocampus",
        "Brain exception after stress");
    ASSERT_NE(bex, nullptr);
    EXPECT_EQ(bex->base.type, EXCEPTION_TYPE_BRAIN);
    EXPECT_EQ(bex->brain_id, 42u);
    nimcp_exception_unref((nimcp_exception_t*)bex);

    // I/O exception
    nimcp_io_exception_t* iex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/test/path",
        "IO exception after stress");
    ASSERT_NE(iex, nullptr);
    EXPECT_EQ(iex->base.type, EXCEPTION_TYPE_IO);
    EXPECT_STREQ(iex->path, "/test/path");
    nimcp_exception_unref((nimcp_exception_t*)iex);

    // Threading exception
    nimcp_threading_exception_t* tex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        12345,
        "Threading exception after stress");
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(tex->base.type, EXCEPTION_TYPE_THREADING);
    EXPECT_EQ(tex->thread_id, 12345u);
    nimcp_exception_unref((nimcp_exception_t*)tex);
}

TEST_F(ExceptionAPIRegressionTest, APIStability_AggregateExceptionsAfterErrors) {
    // WHAT: Test aggregate exception API stability
    // WHY:  Aggregate exceptions are complex and must remain stable
    // HOW:  Create and manipulate aggregates after error conditions

    // Pre-stress
    for (int i = 0; i < 20; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Aggregate pre-stress %d", i);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    // Create aggregate
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Aggregate after stress test");
    ASSERT_NE(agg, nullptr);
    EXPECT_EQ(agg->base.type, EXCEPTION_TYPE_AGGREGATE);

    // Add children
    for (int i = 0; i < NIMCP_EXCEPTION_MAX_CHILDREN; i++) {
        nimcp_exception_t* child = nimcp_exception_create(
            NIMCP_ERROR_INVALID_PARAM,
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "Child %d of aggregate", i);
        ASSERT_NE(child, nullptr);
        nimcp_aggregate_exception_add(agg, child);
    }

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), (size_t)NIMCP_EXCEPTION_MAX_CHILDREN);

    // Iterate children
    for (size_t i = 0; i < nimcp_aggregate_exception_count(agg); i++) {
        nimcp_exception_t* child = nimcp_aggregate_exception_get(agg, i);
        ASSERT_NE(child, nullptr);
        EXPECT_EQ(child->code, NIMCP_ERROR_INVALID_PARAM);
    }

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

/* ============================================================================
 * SECTION 30: Module State Recovery Tests
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, ModuleStateRecovery_ExceptionSystemReinitialization) {
    // WHAT: Test exception system can be properly re-initialized
    // WHY:  Recovery may require system restart
    // HOW:  Shutdown and reinitialize exception system

    // Throw some exceptions first
    for (int i = 0; i < 10; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Pre-reinit test %d", i);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    // Shutdown
    nimcp_exception_clear_current();
    nimcp_exception_system_shutdown();

    // Reinitialize
    int result = nimcp_exception_system_init();
    EXPECT_EQ(result, 0);

    // System should work normally
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_DEBUG,
        __FILE__, __LINE__, __func__,
        "Post-reinit test");
    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_UNKNOWN);
    nimcp_exception_unref(ex);
}

TEST_F(ExceptionAPIRegressionTest, ModuleStateRecovery_HandlerReregistration) {
    // WHAT: Test handlers can be re-registered after recovery
    // WHY:  Recovery may require handler chain rebuild
    // HOW:  Register, unregister, re-register handlers

    std::atomic<int> call_count{0};
    auto handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        counter->fetch_add(1);
        return false;
    };

    // Register first handler
    nimcp_handler_options_t opts1;
    nimcp_handler_default_options(&opts1);
    opts1.name = "recovery_handler_1";
    opts1.handler = handler;
    opts1.user_data = &call_count;
    opts1.priority = 100;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&opts1);
    ASSERT_NE(reg1, nullptr);

    // Throw exception
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "First throw");
    nimcp_exception_dispatch(ex1);
    nimcp_exception_unref(ex1);
    EXPECT_EQ(call_count.load(), 1);

    // Unregister
    nimcp_handler_unregister(reg1);

    // Re-register with different name
    nimcp_handler_options_t opts2;
    nimcp_handler_default_options(&opts2);
    opts2.name = "recovery_handler_2";
    opts2.handler = handler;
    opts2.user_data = &call_count;
    opts2.priority = 100;

    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&opts2);
    ASSERT_NE(reg2, nullptr);

    // Throw again
    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Second throw after re-register");
    nimcp_exception_dispatch(ex2);
    nimcp_exception_unref(ex2);
    EXPECT_EQ(call_count.load(), 2);

    nimcp_handler_unregister(reg2);
}

TEST_F(ExceptionAPIRegressionTest, ModuleStateRecovery_ThreadLocalStateCleared) {
    // WHAT: Test thread-local exception state is properly cleared
    // WHY:  Recovery requires clean state
    // HOW:  Set and clear thread-local exception state

    // Set current exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Thread local test");
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_current(ex);
    EXPECT_NE(nimcp_exception_get_current(), nullptr);

    // Clear
    nimcp_exception_clear_current();
    EXPECT_EQ(nimcp_exception_get_current(), nullptr);

    // Set again to verify state is clean
    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Thread local test 2");
    ASSERT_NE(ex2, nullptr);

    nimcp_exception_set_current(ex2);
    nimcp_exception_t* current = nimcp_exception_get_current();
    ASSERT_NE(current, nullptr);
    EXPECT_EQ(current->code, NIMCP_ERROR_TIMEOUT);

    nimcp_exception_clear_current();
}

TEST_F(ExceptionAPIRegressionTest, ModuleStateRecovery_RecoveryCallbackReregistration) {
    // WHAT: Test recovery callbacks can be re-registered
    // WHY:  Recovery callbacks may need to be updated after recovery
    // HOW:  Register, trigger, re-register recovery callbacks

    std::atomic<int> callback_count{0};
    auto recovery_cb = [](nimcp_exception_t* ex,
                          nimcp_exception_recovery_action_t action,
                          void* user_data) -> int {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        counter->fetch_add(1);
        return 0;  // Success
    };

    // Register recovery callback
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, recovery_cb, &callback_count);

    // Create exception and attempt recovery
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Recovery callback test");
    ASSERT_NE(ex, nullptr);

    int result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_RETRY);
    EXPECT_EQ(result, 0);
    EXPECT_GE(callback_count.load(), 1);

    nimcp_exception_unref(ex);

    // Re-register with different callback
    std::atomic<int> new_callback_count{0};
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, recovery_cb, &new_callback_count);

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Recovery callback test 2");
    ASSERT_NE(ex2, nullptr);

    result = nimcp_execute_recovery(ex2, EXCEPTION_RECOVERY_RETRY);
    EXPECT_EQ(result, 0);
    EXPECT_GE(new_callback_count.load(), 1);

    nimcp_exception_unref(ex2);
}

TEST_F(ExceptionAPIRegressionTest, ModuleStateRecovery_StatsResetAfterRecovery) {
    // WHAT: Test stats tracking works correctly after recovery
    // WHY:  Stats must accurately reflect post-recovery state
    // HOW:  Check stats before and after simulated recovery

    // Get initial stats
    nimcp_exception_immune_stats_t initial_stats;
    nimcp_exception_immune_get_stats(&initial_stats);

    // Throw some exceptions to accumulate stats
    for (int i = 0; i < 10; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "Stats test %d", i);
        nimcp_exception_present_to_immune(ex, NULL);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    // Get updated stats
    nimcp_exception_immune_stats_t updated_stats;
    nimcp_exception_immune_get_stats(&updated_stats);

    // Stats should show activity
    EXPECT_GE(updated_stats.exceptions_presented,
              initial_stats.exceptions_presented);
}

TEST_F(ExceptionAPIRegressionTest, ModuleStateRecovery_MultipleRecoveryAttempts) {
    // WHAT: Test system handles multiple recovery attempts gracefully
    // WHY:  Real recovery may require multiple attempts
    // HOW:  Attempt recovery multiple times for same exception type

    std::atomic<int> attempt_count{0};
    auto recovery_cb = [](nimcp_exception_t* ex,
                          nimcp_exception_recovery_action_t action,
                          void* user_data) -> int {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        int attempts = counter->fetch_add(1) + 1;
        // Succeed on 3rd attempt
        return (attempts >= 3) ? 0 : -1;
    };

    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, recovery_cb, &attempt_count);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Multiple recovery attempt test");
    ASSERT_NE(ex, nullptr);

    // First attempt - should fail
    int result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
    EXPECT_NE(result, 0);

    // Second attempt - should fail
    result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
    EXPECT_NE(result, 0);

    // Third attempt - should succeed
    result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(attempt_count.load(), 3);

    nimcp_exception_unref(ex);
}

/* ============================================================================
 * SECTION 31: Concurrent Access Stability Tests
 * ============================================================================ */

TEST_F(ExceptionAPIRegressionTest, ConcurrentAccess_SimultaneousExceptionCreation) {
    // WHAT: Test concurrent exception creation is thread-safe
    // WHY:  Multiple threads may create exceptions simultaneously
    // HOW:  Launch threads that create exceptions concurrently

    const int NUM_THREADS = 10;
    const int EXCEPTIONS_PER_THREAD = 50;
    std::atomic<int> created_count{0};
    std::atomic<int> failed_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED,
                    EXCEPTION_SEVERITY_WARNING,
                    __FILE__, __LINE__, __func__,
                    "Thread %d exception %d", t, i);
                if (ex) {
                    created_count++;
                    nimcp_exception_unref(ex);
                } else {
                    failed_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(created_count.load(), NUM_THREADS * EXCEPTIONS_PER_THREAD);
    EXPECT_EQ(failed_count.load(), 0);
}

TEST_F(ExceptionAPIRegressionTest, ConcurrentAccess_SimultaneousHandlerCalls) {
    // WHAT: Test concurrent handler calls are thread-safe
    // WHY:  Handler chain must handle concurrent access
    // HOW:  Register handler and dispatch from multiple threads

    std::atomic<int> handler_calls{0};
    auto handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        counter->fetch_add(1);
        return false;
    };

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "concurrent_handler";
    opts.handler = handler;
    opts.user_data = &handler_calls;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    const int NUM_THREADS = 8;
    const int DISPATCHES_PER_THREAD = 25;
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < DISPATCHES_PER_THREAD; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_THREAD_SYNC,
                    EXCEPTION_SEVERITY_WARNING,
                    __FILE__, __LINE__, __func__,
                    "Concurrent dispatch %d-%d", t, i);
                if (ex) {
                    nimcp_exception_dispatch(ex);
                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(handler_calls.load(), NUM_THREADS * DISPATCHES_PER_THREAD);

    nimcp_handler_unregister(reg);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
