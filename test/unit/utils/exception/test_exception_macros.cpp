/**
 * @file test_exception_macros.cpp
 * @brief Unit tests for exception macro integration in NIMCP
 *
 * WHAT: Tests for all exception convenience macros defined in nimcp_exception_macros.h
 * WHY:  Verify macros correctly create, throw, and dispatch exceptions
 * HOW:  GoogleTest framework with fixture setup/teardown for exception system
 *
 * TEST CATEGORIES:
 * 1. NIMCP_CHECK_THROW macro - verify it throws and returns on false condition
 * 2. NIMCP_THROW_IF macro - verify conditional throw
 * 3. NIMCP_THROW_TO_IMMUNE - verify immune system integration
 * 4. Typed exception macros (NIMCP_THROW_MEMORY, NIMCP_THROW_BRAIN, etc.)
 * 5. Severity override macros (NIMCP_THROW_CRITICAL, NIMCP_THROW_FATAL)
 * 6. Recovery macros (NIMCP_RECOVER, NIMCP_THROW_AND_RECOVER)
 *
 * @author NIMCP Development Team
 * @date 2025-01-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>

extern "C" {
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Globals for Handler Tracking
//=============================================================================

static std::atomic<int> g_handler_call_count{0};
static std::atomic<nimcp_error_t> g_last_error_code{NIMCP_SUCCESS};
static std::atomic<nimcp_exception_severity_t> g_last_severity{EXCEPTION_SEVERITY_DEBUG};
static std::atomic<bool> g_exception_presented_to_immune{false};

/**
 * @brief Test handler callback to track exception dispatch
 */
static bool test_tracking_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex) {
        g_handler_call_count++;
        g_last_error_code = ex->code;
        g_last_severity = ex->severity;
        g_exception_presented_to_immune = ex->presented_to_immune;
    }
    return false;  // Don't consume, let chain continue
}

//=============================================================================
// Test Fixture Base
//=============================================================================

/**
 * WHAT: Base fixture for exception macro tests
 * WHY:  Setup/teardown exception system for each test
 */
class ExceptionMacrosTest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* handler_reg_ = nullptr;

    void SetUp() override {
        // Reset tracking
        g_handler_call_count = 0;
        g_last_error_code = NIMCP_SUCCESS;
        g_last_severity = EXCEPTION_SEVERITY_DEBUG;
        g_exception_presented_to_immune = false;

        // Initialize exception system
        nimcp_exception_system_init();

        // Register test handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "test_tracking_handler";
        opts.handler = test_tracking_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        handler_reg_ = nimcp_handler_register(&opts);
    }

    void TearDown() override {
        if (handler_reg_) {
            nimcp_handler_unregister(handler_reg_);
            handler_reg_ = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

//=============================================================================
// Basic NIMCP_THROW Macro Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_THROW creates and dispatches exception
 * WHY:  Verify basic throw functionality
 */
TEST_F(ExceptionMacrosTest, ThrowCreatesAndDispatchesException) {
    NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Test error message");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INVALID_PARAM);
}

/**
 * WHAT: Test NIMCP_THROW with format arguments
 * WHY:  Verify printf-style formatting works
 */
TEST_F(ExceptionMacrosTest, ThrowWithFormatArguments) {
    int value = 42;
    NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "Value %d out of range [0, 10]", value);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_OUT_OF_RANGE);
}

//=============================================================================
// NIMCP_THROW_IF Macro Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_THROW_IF throws when condition is false
 * WHY:  Verify conditional throw on false
 */
TEST_F(ExceptionMacrosTest, ThrowIfThrowsOnFalseCondition) {
    int* ptr = nullptr;

    NIMCP_THROW_IF(ptr != nullptr, NIMCP_ERROR_NULL_POINTER, "ptr is NULL");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test NIMCP_THROW_IF does not throw when condition is true
 * WHY:  Verify no exception when condition passes
 */
TEST_F(ExceptionMacrosTest, ThrowIfDoesNotThrowOnTrueCondition) {
    int value = 42;
    int* ptr = &value;

    NIMCP_THROW_IF(ptr != nullptr, NIMCP_ERROR_NULL_POINTER, "ptr is NULL");

    // Condition was true, so no exception should be thrown
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// NIMCP_CHECK_THROW Macro Tests
//=============================================================================

/**
 * Helper function that uses NIMCP_CHECK_THROW
 */
static nimcp_error_t function_with_check_throw(void* ptr) {
    NIMCP_CHECK_THROW(ptr != nullptr, NIMCP_ERROR_NULL_POINTER, "NULL pointer passed");
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test NIMCP_CHECK_THROW returns error code on false condition
 * WHY:  Verify check-throw-return pattern
 */
TEST_F(ExceptionMacrosTest, CheckThrowReturnsOnFalseCondition) {
    nimcp_error_t result = function_with_check_throw(nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW continues on true condition
 * WHY:  Verify function continues when check passes
 */
TEST_F(ExceptionMacrosTest, CheckThrowContinuesOnTrueCondition) {
    int value = 42;
    nimcp_error_t result = function_with_check_throw(&value);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * Helper function with multiple checks
 */
static nimcp_error_t function_with_multiple_checks(void* ptr, int value) {
    NIMCP_CHECK_THROW(ptr != nullptr, NIMCP_ERROR_NULL_POINTER, "NULL pointer");
    NIMCP_CHECK_THROW(value >= 0, NIMCP_ERROR_INVALID_PARAM, "value must be >= 0");
    NIMCP_CHECK_THROW(value <= 100, NIMCP_ERROR_OUT_OF_RANGE, "value must be <= 100");
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test multiple NIMCP_CHECK_THROW in sequence
 * WHY:  Verify only first failing check throws
 */
TEST_F(ExceptionMacrosTest, MultipleCheckThrowsFirstFailure) {
    int value = 42;

    // First check fails
    nimcp_error_t result = function_with_multiple_checks(nullptr, value);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);

    // Reset
    g_handler_call_count = 0;

    // Second check fails
    result = function_with_multiple_checks(&value, -1);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);

    // Reset
    g_handler_call_count = 0;

    // Third check fails
    result = function_with_multiple_checks(&value, 150);
    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);

    // Reset
    g_handler_call_count = 0;

    // All checks pass
    result = function_with_multiple_checks(&value, 50);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// NIMCP_THROW_TO_IMMUNE Macro Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE marks exception as presented
 * WHY:  Verify immune integration flag is set
 */
TEST_F(ExceptionMacrosTest, ThrowToImmuneMarksAsPresentedToImmune) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Memory allocation failed");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE_IF conditional
 * WHY:  Verify conditional immune presentation
 */
TEST_F(ExceptionMacrosTest, ThrowToImmuneIfCondition) {
    bool memory_low = true;

    NIMCP_THROW_TO_IMMUNE_IF(!memory_low, NIMCP_ERROR_NO_MEMORY, "Low memory");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * Helper function using NIMCP_CHECK_THROW_IMMUNE
 */
static nimcp_error_t function_with_immune_check(void* ptr) {
    NIMCP_CHECK_THROW_IMMUNE(ptr != nullptr, NIMCP_ERROR_NULL_POINTER, "NULL pointer");
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test NIMCP_CHECK_THROW_IMMUNE returns and presents to immune
 * WHY:  Verify combined check-throw-return with immune presentation
 */
TEST_F(ExceptionMacrosTest, CheckThrowImmuneReturnsAndPresents) {
    nimcp_error_t result = function_with_immune_check(nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

//=============================================================================
// Typed Exception Macro Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_THROW_MEMORY creates memory exception
 * WHY:  Verify memory-specific exception creation
 */
TEST_F(ExceptionMacrosTest, ThrowMemoryCreatesMemoryException) {
    size_t requested = 1024 * 1024;  // 1 MB
    NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, requested, "Failed to allocate %zu bytes", requested);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_BRAIN creates brain exception
 * WHY:  Verify brain-specific exception creation
 */
TEST_F(ExceptionMacrosTest, ThrowBrainCreatesBrainException) {
    uint32_t brain_id = 1;
    const char* region = "hippocampus";

    NIMCP_THROW_BRAIN(NIMCP_ERROR_BRAIN_INVALID, brain_id, region,
                      "Brain %u region %s failed", brain_id, region);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_BRAIN_INVALID);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_IO creates I/O exception
 * WHY:  Verify I/O-specific exception creation
 */
TEST_F(ExceptionMacrosTest, ThrowIOCreatesIOException) {
    const char* path = "/tmp/test.dat";

    NIMCP_THROW_IO(NIMCP_ERROR_FILE_NOT_FOUND, path, "File not found: %s", path);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_FILE_NOT_FOUND);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_THREADING creates threading exception
 * WHY:  Verify threading-specific exception creation
 */
TEST_F(ExceptionMacrosTest, ThrowThreadingCreatesThreadingException) {
    uint64_t thread_id = 12345;

    NIMCP_THROW_THREADING(NIMCP_ERROR_DEADLOCK, thread_id,
                          "Deadlock detected in thread %lu", (unsigned long)thread_id);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_DEADLOCK);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_SECURITY creates security exception
 * WHY:  Verify security-specific exception creation with critical severity
 */
TEST_F(ExceptionMacrosTest, ThrowSecurityCreatesSecurityException) {
    uint32_t threat_type = 1;

    NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, threat_type,
                         "Security threat detected: type %u", threat_type);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_SECURITY_THREAT);
    // Security exceptions should always be critical severity
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_GPU creates GPU exception
 * WHY:  Verify GPU-specific exception creation
 */
TEST_F(ExceptionMacrosTest, ThrowGPUCreatesGPUException) {
    int device_id = 0;
    int cuda_err = 2;  // cudaErrorMemoryAllocation

    NIMCP_THROW_GPU(NIMCP_ERROR_GPU_MEMORY, device_id, cuda_err,
                    "GPU %d memory allocation failed (CUDA error %d)", device_id, cuda_err);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_GPU_MEMORY);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

//=============================================================================
// Severity Override Macro Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_THROW_SEVERITY with explicit severity
 * WHY:  Verify severity override functionality
 */
TEST_F(ExceptionMacrosTest, ThrowSeverityOverridesSeverity) {
    // NIMCP_ERROR_INVALID_PARAM normally has lower severity
    NIMCP_THROW_SEVERITY(NIMCP_ERROR_INVALID_PARAM, EXCEPTION_SEVERITY_CRITICAL,
                         "Critical invalid parameter");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    // Critical and above should present to immune
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_CRITICAL sets critical severity
 * WHY:  Verify critical shortcut macro
 */
TEST_F(ExceptionMacrosTest, ThrowCriticalSetsCriticalSeverity) {
    NIMCP_THROW_CRITICAL(NIMCP_ERROR_INVALID_STATE, "Critical state error");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_FATAL sets fatal severity
 * WHY:  Verify fatal shortcut macro for emergency response
 */
TEST_F(ExceptionMacrosTest, ThrowFatalSetsFatalSeverity) {
    NIMCP_THROW_FATAL(NIMCP_ERROR_MEMORY_CORRUPTION, "Fatal memory corruption");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_MEMORY_CORRUPTION);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_FATAL);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test severity levels for immune presentation threshold
 * WHY:  Verify SEVERE and above present to immune automatically
 */
TEST_F(ExceptionMacrosTest, SeverityThresholdForImmunePresentation) {
    // WARNING - below threshold
    NIMCP_THROW_SEVERITY(NIMCP_ERROR_UNKNOWN, EXCEPTION_SEVERITY_WARNING, "Warning");
    EXPECT_FALSE(g_exception_presented_to_immune);

    g_exception_presented_to_immune = false;
    g_handler_call_count = 0;

    // ERROR - below threshold
    NIMCP_THROW_SEVERITY(NIMCP_ERROR_UNKNOWN, EXCEPTION_SEVERITY_ERROR, "Error");
    EXPECT_FALSE(g_exception_presented_to_immune);

    g_exception_presented_to_immune = false;
    g_handler_call_count = 0;

    // SEVERE - at threshold, should present
    NIMCP_THROW_SEVERITY(NIMCP_ERROR_UNKNOWN, EXCEPTION_SEVERITY_SEVERE, "Severe");
    EXPECT_TRUE(g_exception_presented_to_immune);
}

//=============================================================================
// Recovery Macro Tests
//=============================================================================

static std::atomic<int> g_recovery_callback_count{0};
static std::atomic<nimcp_exception_recovery_action_t> g_last_recovery_action{EXCEPTION_RECOVERY_NONE};

/**
 * @brief Test recovery callback
 */
static int test_recovery_callback(nimcp_exception_t* ex,
                                   nimcp_exception_recovery_action_t action,
                                   void* user_data) {
    (void)ex;
    (void)user_data;
    g_recovery_callback_count++;
    g_last_recovery_action = action;
    return 0;  // Success
}

/**
 * WHAT: Fixture for recovery macro tests
 * WHY:  Setup recovery callbacks for testing
 */
class RecoveryMacrosTest : public ExceptionMacrosTest {
protected:
    void SetUp() override {
        ExceptionMacrosTest::SetUp();
        g_recovery_callback_count = 0;
        g_last_recovery_action = EXCEPTION_RECOVERY_NONE;

        // Register test recovery callbacks
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, test_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, test_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, test_recovery_callback, nullptr);
    }

    void TearDown() override {
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK);
        ExceptionMacrosTest::TearDown();
    }
};

/**
 * WHAT: Test NIMCP_THROW_AND_RECOVER triggers recovery
 * WHY:  Verify combined throw and recovery execution
 */
TEST_F(RecoveryMacrosTest, ThrowAndRecoverExecutesRecovery) {
    NIMCP_THROW_AND_RECOVER(NIMCP_ERROR_NO_MEMORY, EXCEPTION_RECOVERY_GC,
                            "Memory low, triggering GC");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(g_recovery_callback_count, 1);
    EXPECT_EQ(g_last_recovery_action, EXCEPTION_RECOVERY_GC);
}

/**
 * WHAT: Test NIMCP_RECOVER with different actions
 * WHY:  Verify various recovery action types
 */
TEST_F(RecoveryMacrosTest, RecoverWithDifferentActions) {
    // First throw an exception to set current exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Test exception"
    );
    ASSERT_NE(ex, nullptr);
    nimcp_exception_set_current(ex);

    // Test GC recovery
    NIMCP_RECOVER(EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(g_recovery_callback_count, 1);
    EXPECT_EQ(g_last_recovery_action, EXCEPTION_RECOVERY_GC);

    g_recovery_callback_count = 0;

    // Test RETRY recovery
    NIMCP_RECOVER(EXCEPTION_RECOVERY_RETRY);
    EXPECT_EQ(g_recovery_callback_count, 1);
    EXPECT_EQ(g_last_recovery_action, EXCEPTION_RECOVERY_RETRY);

    g_recovery_callback_count = 0;

    // Test ROLLBACK recovery
    NIMCP_RECOVER(EXCEPTION_RECOVERY_ROLLBACK);
    EXPECT_EQ(g_recovery_callback_count, 1);
    EXPECT_EQ(g_last_recovery_action, EXCEPTION_RECOVERY_ROLLBACK);

    nimcp_exception_clear_current();
    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test NIMCP_RECOVER when no current exception
 * WHY:  Verify safe behavior when no exception set
 */
TEST_F(RecoveryMacrosTest, RecoverWithNoCurrentException) {
    // Clear any current exception
    nimcp_exception_clear_current();

    // Should be safe to call, just does nothing
    NIMCP_RECOVER(EXCEPTION_RECOVERY_GC);

    EXPECT_EQ(g_recovery_callback_count, 0);
}

//=============================================================================
// Async Throw Macro Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_THROW_ASYNC queues for async processing
 * WHY:  Verify non-blocking async throw behavior
 */
TEST_F(ExceptionMacrosTest, ThrowAsyncQueuesException) {
    // Initialize immune system for async processing
    nimcp_exception_immune_config_t config;
    nimcp_exception_immune_default_config(&config);
    config.async_presentation = true;
    nimcp_exception_immune_init(&config);

    NIMCP_THROW_ASYNC(NIMCP_ERROR_NO_MEMORY, "Async memory error");

    // Handler should still be called for dispatch
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NO_MEMORY);

    nimcp_exception_immune_shutdown();
}

//=============================================================================
// Signal Exception Macro Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_THROW_SIGNAL creates signal exception
 * WHY:  Verify signal-specific exception creation
 */
TEST_F(ExceptionMacrosTest, ThrowSignalCreatesSignalException) {
    int signal_num = 11;  // SIGSEGV
    void* fault_addr = (void*)0xDEADBEEF;

    NIMCP_THROW_SIGNAL(signal_num, fault_addr, "Segmentation fault at %p", fault_addr);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_SIGSEGV);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

//=============================================================================
// Debug Assert Macro Tests
//=============================================================================

#ifndef NDEBUG
/**
 * WHAT: Test NIMCP_ASSERT_THROW in debug mode
 * WHY:  Verify debug-only assert behavior
 */
TEST_F(ExceptionMacrosTest, AssertThrowInDebugMode) {
    bool condition = false;

    NIMCP_ASSERT_THROW(condition, NIMCP_ERROR_INVALID_STATE, "Assert failed");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INVALID_STATE);
    // Assert should use critical severity
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
}

/**
 * WHAT: Test NIMCP_ASSERT_THROW does nothing when condition is true
 * WHY:  Verify no exception on passing assert
 */
TEST_F(ExceptionMacrosTest, AssertThrowNoOpOnTrueCondition) {
    bool condition = true;

    NIMCP_ASSERT_THROW(condition, NIMCP_ERROR_INVALID_STATE, "Should not throw");

    EXPECT_EQ(g_handler_call_count, 0);
}
#endif  // !NDEBUG

//=============================================================================
// Error Code to Severity Mapping Tests
//=============================================================================

/**
 * WHAT: Test severity mapping for different error codes
 * WHY:  Verify nimcp_exception_get_severity_from_code() works correctly
 */
TEST_F(ExceptionMacrosTest, SeverityMappingFromErrorCode) {
    // Memory errors should be severe
    nimcp_exception_severity_t severity = nimcp_exception_get_severity_from_code(NIMCP_ERROR_NO_MEMORY);
    EXPECT_GE(severity, EXCEPTION_SEVERITY_SEVERE);

    // NULL pointer should be error level
    severity = nimcp_exception_get_severity_from_code(NIMCP_ERROR_NULL_POINTER);
    EXPECT_GE(severity, EXCEPTION_SEVERITY_ERROR);

    // Signal errors should be critical
    severity = nimcp_exception_get_severity_from_code(NIMCP_ERROR_SIGSEGV);
    EXPECT_GE(severity, EXCEPTION_SEVERITY_SEVERE);
}

//=============================================================================
// Epitope Generation Tests
//=============================================================================

/**
 * WHAT: Test exception epitope generation for immune matching
 * WHY:  Verify epitope fingerprint is generated correctly
 */
TEST_F(ExceptionMacrosTest, EpitopeGenerationForException) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Test exception for epitope"
    );
    ASSERT_NE(ex, nullptr);

    size_t epitope_len = nimcp_exception_generate_epitope(ex);

    EXPECT_GT(epitope_len, 0u);
    EXPECT_LE(epitope_len, NIMCP_EXCEPTION_EPITOPE_SIZE);

    // Epitope should have non-zero bytes
    bool has_nonzero = false;
    for (size_t i = 0; i < epitope_len; i++) {
        if (ex->epitope[i] != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test same error produces similar epitope
 * WHY:  Verify epitope consistency for pattern matching
 */
TEST_F(ExceptionMacrosTest, SameErrorProducesSimilarEpitope) {
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory error 1"
    );
    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory error 2"
    );

    ASSERT_NE(ex1, nullptr);
    ASSERT_NE(ex2, nullptr);

    size_t len1 = nimcp_exception_generate_epitope(ex1);
    size_t len2 = nimcp_exception_generate_epitope(ex2);

    // Same error code should produce same length epitope
    EXPECT_EQ(len1, len2);

    // Compare first few bytes (error code should be encoded)
    int match_count = 0;
    size_t min_len = (len1 < len2) ? len1 : len2;
    for (size_t i = 0; i < min_len && i < 8; i++) {
        if (ex1->epitope[i] == ex2->epitope[i]) {
            match_count++;
        }
    }
    // At least error code portion should match
    EXPECT_GE(match_count, 4);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

//=============================================================================
// Legacy Compatibility Macro Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_SET_ERROR_EX creates exception and sets error
 * WHY:  Verify gradual migration support
 */
TEST_F(ExceptionMacrosTest, SetErrorExCreatesExceptionAndSetsError) {
    NIMCP_SET_ERROR_EX(NIMCP_ERROR_INVALID_PARAM, "Legacy error: %s", "test");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INVALID_PARAM);

    // Also check thread-local error was set
    nimcp_error_t last_error = nimcp_get_last_error();
    EXPECT_EQ(last_error, NIMCP_ERROR_INVALID_PARAM);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

/**
 * WHAT: Test macro with empty message
 * WHY:  Verify handling of empty format string
 */
TEST_F(ExceptionMacrosTest, ThrowWithEmptyMessage) {
    NIMCP_THROW(NIMCP_ERROR_UNKNOWN, "");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_UNKNOWN);
}

/**
 * WHAT: Test macro with very long message
 * WHY:  Verify message truncation handling
 */
TEST_F(ExceptionMacrosTest, ThrowWithLongMessage) {
    char long_message[1024];
    memset(long_message, 'A', sizeof(long_message) - 1);
    long_message[sizeof(long_message) - 1] = '\0';

    NIMCP_THROW(NIMCP_ERROR_UNKNOWN, "%s", long_message);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_UNKNOWN);
}

/**
 * WHAT: Test rapid exception creation and dispatch
 * WHY:  Verify stability under rapid exception generation
 */
TEST_F(ExceptionMacrosTest, RapidExceptionDispatch) {
    const int iterations = 100;

    for (int i = 0; i < iterations; i++) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Error %d", i);
    }

    EXPECT_EQ(g_handler_call_count, iterations);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
