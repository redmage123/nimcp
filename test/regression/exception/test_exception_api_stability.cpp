/**
 * @file test_exception_api_stability.cpp
 * @brief Regression tests for exception handling API stability
 * @date 2026-01-21
 *
 * Tests to verify:
 * - API contracts remain stable across versions
 * - Exception macro conversions didn't change function behavior
 * - Backwards compatibility of error handling
 * - Error codes returned match documented behavior
 *
 * These tests serve as a contract for the exception API - if any of
 * these tests fail, it indicates a breaking change that may affect
 * existing code depending on the exception handling system.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <thread>
#include <atomic>

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

class ExceptionAPIStabilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize exception system
        int result = nimcp_exception_system_init();
        ASSERT_EQ(result, 0) << "Failed to initialize exception system";
    }

    void TearDown() override {
        // Clear any pending exceptions
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

/* ============================================================================
 * Error Code Stability Tests
 *
 * Verify that error codes have not changed their numeric values.
 * These are contracts that external code may depend on.
 * ============================================================================ */

TEST_F(ExceptionAPIStabilityTest, ErrorCodeValues_GenericErrors_Stable) {
    // Generic error codes (1000-1999)
    EXPECT_EQ(NIMCP_SUCCESS, 0);
    EXPECT_EQ(NIMCP_ERROR_UNKNOWN, 1000);
    EXPECT_EQ(NIMCP_ERROR_NOT_IMPLEMENTED, 1001);
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER, 1002);
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, 1003);
    EXPECT_EQ(NIMCP_ERROR_OUT_OF_RANGE, 1004);
    EXPECT_EQ(NIMCP_ERROR_INVALID_STATE, 1005);
    EXPECT_EQ(NIMCP_ERROR_OPERATION_FAILED, 1006);
    EXPECT_EQ(NIMCP_ERROR_NOT_INITIALIZED, 1007);
    EXPECT_EQ(NIMCP_ERROR_ALREADY_EXISTS, 1008);
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND, 1009);
    EXPECT_EQ(NIMCP_ERROR_TIMEOUT, 1010);
}

TEST_F(ExceptionAPIStabilityTest, ErrorCodeValues_MemoryErrors_Stable) {
    // Memory error codes (2000-2999)
    EXPECT_EQ(NIMCP_ERROR_NO_MEMORY, 2000);
    EXPECT_EQ(NIMCP_ERROR_BUFFER_TOO_SMALL, 2001);
    EXPECT_EQ(NIMCP_ERROR_BUFFER_OVERFLOW, 2002);
    EXPECT_EQ(NIMCP_ERROR_MEMORY_CORRUPTION, 2003);
    EXPECT_EQ(NIMCP_ERROR_INVALID_ADDRESS, 2004);
}

TEST_F(ExceptionAPIStabilityTest, ErrorCodeValues_BrainErrors_Stable) {
    // Brain/Network error codes (3000-3999)
    EXPECT_EQ(NIMCP_ERROR_BRAIN_CREATION, 3000);
    EXPECT_EQ(NIMCP_ERROR_BRAIN_INVALID, 3001);
    EXPECT_EQ(NIMCP_ERROR_NETWORK_CREATION, 3002);
    EXPECT_EQ(NIMCP_ERROR_FORWARD_PASS, 3006);
    EXPECT_EQ(NIMCP_ERROR_BACKWARD_PASS, 3007);
}

TEST_F(ExceptionAPIStabilityTest, ErrorCodeValues_IOErrors_Stable) {
    // I/O error codes (4000-4999)
    EXPECT_EQ(NIMCP_ERROR_FILE_NOT_FOUND, 4000);
    EXPECT_EQ(NIMCP_ERROR_FILE_READ, 4001);
    EXPECT_EQ(NIMCP_ERROR_FILE_WRITE, 4002);
    EXPECT_EQ(NIMCP_ERROR_FILE_OPEN, 4003);
}

TEST_F(ExceptionAPIStabilityTest, ErrorCodeValues_ThreadingErrors_Stable) {
    // Threading error codes (6000-6999)
    EXPECT_EQ(NIMCP_ERROR_THREAD_CREATE, 6000);
    EXPECT_EQ(NIMCP_ERROR_MUTEX_LOCK, 6002);
    EXPECT_EQ(NIMCP_ERROR_DEADLOCK, 6005);
}

TEST_F(ExceptionAPIStabilityTest, ErrorCodeValues_SignalErrors_Stable) {
    // Signal error codes (7000-7999)
    EXPECT_EQ(NIMCP_ERROR_SIGNAL_RECEIVED, 7000);
    EXPECT_EQ(NIMCP_ERROR_SIGSEGV, 7001);
    EXPECT_EQ(NIMCP_ERROR_SIGABRT, 7002);
    EXPECT_EQ(NIMCP_ERROR_SIGFPE, 7003);
    EXPECT_EQ(NIMCP_ERROR_SIGBUS, 7004);
    EXPECT_EQ(NIMCP_ERROR_SIGILL, 7005);
}

/* ============================================================================
 * Exception Severity Enum Stability
 * ============================================================================ */

TEST_F(ExceptionAPIStabilityTest, SeverityEnumValues_Stable) {
    EXPECT_EQ(EXCEPTION_SEVERITY_DEBUG, 1);
    EXPECT_EQ(EXCEPTION_SEVERITY_INFO, 2);
    EXPECT_EQ(EXCEPTION_SEVERITY_WARNING, 3);
    EXPECT_EQ(EXCEPTION_SEVERITY_ERROR, 5);
    EXPECT_EQ(EXCEPTION_SEVERITY_SEVERE, 7);
    EXPECT_EQ(EXCEPTION_SEVERITY_CRITICAL, 9);
    EXPECT_EQ(EXCEPTION_SEVERITY_FATAL, 10);
}

TEST_F(ExceptionAPIStabilityTest, CategoryEnumValues_Stable) {
    EXPECT_EQ(EXCEPTION_CATEGORY_GENERIC, 1);
    EXPECT_EQ(EXCEPTION_CATEGORY_MEMORY, 2);
    EXPECT_EQ(EXCEPTION_CATEGORY_BRAIN, 3);
    EXPECT_EQ(EXCEPTION_CATEGORY_IO, 4);
    EXPECT_EQ(EXCEPTION_CATEGORY_CONFIG, 5);
    EXPECT_EQ(EXCEPTION_CATEGORY_THREADING, 6);
    EXPECT_EQ(EXCEPTION_CATEGORY_SIGNAL, 7);
    EXPECT_EQ(EXCEPTION_CATEGORY_COGNITIVE, 8);
}

TEST_F(ExceptionAPIStabilityTest, TypeEnumValues_Stable) {
    EXPECT_EQ(EXCEPTION_TYPE_BASE, 0);
    EXPECT_EQ(EXCEPTION_TYPE_MEMORY, 1);
    EXPECT_EQ(EXCEPTION_TYPE_BRAIN, 2);
    EXPECT_EQ(EXCEPTION_TYPE_IO, 3);
    EXPECT_EQ(EXCEPTION_TYPE_THREADING, 4);
    EXPECT_EQ(EXCEPTION_TYPE_SECURITY, 5);
    EXPECT_EQ(EXCEPTION_TYPE_COGNITIVE, 6);
    EXPECT_EQ(EXCEPTION_TYPE_GPU, 7);
    EXPECT_EQ(EXCEPTION_TYPE_AGGREGATE, 8);
    EXPECT_EQ(EXCEPTION_TYPE_SIGNAL, 9);
}

TEST_F(ExceptionAPIStabilityTest, RecoveryActionEnumValues_Stable) {
    EXPECT_EQ(EXCEPTION_RECOVERY_NONE, 0);
    EXPECT_EQ(EXCEPTION_RECOVERY_RETRY, 1);
    EXPECT_EQ(EXCEPTION_RECOVERY_GC, 2);
    EXPECT_EQ(EXCEPTION_RECOVERY_COMPACT, 3);
    EXPECT_EQ(EXCEPTION_RECOVERY_ROLLBACK, 4);
    EXPECT_EQ(EXCEPTION_RECOVERY_RESTART_THREAD, 5);
    EXPECT_EQ(EXCEPTION_RECOVERY_RESTART_COMPONENT, 6);
    EXPECT_EQ(EXCEPTION_RECOVERY_QUARANTINE, 7);
}

/* ============================================================================
 * Exception Creation API Stability
 * ============================================================================ */

TEST_F(ExceptionAPIStabilityTest, ExceptionCreate_ReturnsValidObject) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Test message: %d",
        42
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_STRNE(ex->message, "");
    EXPECT_TRUE(strstr(ex->message, "42") != nullptr);
    EXPECT_NE(ex->file, nullptr);
    EXPECT_GT(ex->line, 0);
    EXPECT_NE(ex->function, nullptr);
    EXPECT_GT(ex->timestamp_us, 0UL);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionAPIStabilityTest, ExceptionCreate_NullMessageHandled) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__,
        __LINE__,
        __func__,
        nullptr  // NULL message should not crash
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionAPIStabilityTest, MemoryExceptionCreate_HasCorrectType) {
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        1024,
        "Allocation failed for %zu bytes",
        (size_t)1024
    );

    ASSERT_NE(mex, nullptr);
    EXPECT_EQ(mex->base.type, EXCEPTION_TYPE_MEMORY);
    EXPECT_EQ(mex->base.category, EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(mex->requested_size, 1024UL);

    nimcp_exception_unref((nimcp_exception_t*)mex);
}

TEST_F(ExceptionAPIStabilityTest, BrainExceptionCreate_HasCorrectType) {
    nimcp_brain_exception_t* bex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        123,
        "prefrontal",
        "Forward pass failed in layer %d",
        5
    );

    ASSERT_NE(bex, nullptr);
    EXPECT_EQ(bex->base.type, EXCEPTION_TYPE_BRAIN);
    EXPECT_EQ(bex->base.category, EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(bex->brain_id, 123U);
    EXPECT_STREQ(bex->region_name, "prefrontal");

    nimcp_exception_unref((nimcp_exception_t*)bex);
}

TEST_F(ExceptionAPIStabilityTest, IOExceptionCreate_HasCorrectType) {
    nimcp_io_exception_t* iex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_NOT_FOUND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "/path/to/file.dat",
        "File not found: %s",
        "/path/to/file.dat"
    );

    ASSERT_NE(iex, nullptr);
    EXPECT_EQ(iex->base.type, EXCEPTION_TYPE_IO);
    EXPECT_EQ(iex->base.category, EXCEPTION_CATEGORY_IO);
    EXPECT_STREQ(iex->path, "/path/to/file.dat");

    nimcp_exception_unref((nimcp_exception_t*)iex);
}

TEST_F(ExceptionAPIStabilityTest, ThreadingExceptionCreate_HasCorrectType) {
    nimcp_threading_exception_t* tex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        12345,
        "Deadlock detected on thread %lu",
        (unsigned long)12345
    );

    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(tex->base.type, EXCEPTION_TYPE_THREADING);
    EXPECT_EQ(tex->base.category, EXCEPTION_CATEGORY_THREADING);
    EXPECT_EQ(tex->thread_id, 12345UL);

    nimcp_exception_unref((nimcp_exception_t*)tex);
}

/* ============================================================================
 * Reference Counting API Stability
 * ============================================================================ */

TEST_F(ExceptionAPIStabilityTest, RefCount_InitializedToOne) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->ref_count, 1);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionAPIStabilityTest, RefCount_IncreasesOnRef) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_t* ex2 = nimcp_exception_ref(ex);
    EXPECT_EQ(ex, ex2);
    EXPECT_EQ(ex->ref_count, 2);

    nimcp_exception_unref(ex);
    EXPECT_EQ(ex->ref_count, 1);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionAPIStabilityTest, RefCount_NullSafe) {
    // ref(NULL) should return NULL
    EXPECT_EQ(nimcp_exception_ref(nullptr), nullptr);

    // unref(NULL) should not crash
    nimcp_exception_unref(nullptr);
}

/* ============================================================================
 * Category/Severity Derivation API Stability
 * ============================================================================ */

TEST_F(ExceptionAPIStabilityTest, GetCategoryFromCode_Memory) {
    nimcp_exception_category_t cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_MEMORY);
}

TEST_F(ExceptionAPIStabilityTest, GetCategoryFromCode_Brain) {
    nimcp_exception_category_t cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_FORWARD_PASS);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN);
}

TEST_F(ExceptionAPIStabilityTest, GetCategoryFromCode_IO) {
    nimcp_exception_category_t cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_IO);
}

TEST_F(ExceptionAPIStabilityTest, GetCategoryFromCode_Threading) {
    nimcp_exception_category_t cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_DEADLOCK);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_THREADING);
}

TEST_F(ExceptionAPIStabilityTest, GetCategoryFromCode_Signal) {
    nimcp_exception_category_t cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_SIGSEGV);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_SIGNAL);
}

TEST_F(ExceptionAPIStabilityTest, GetSeverityFromCode_CriticalErrors) {
    // Memory errors should be at least SEVERE
    nimcp_exception_severity_t sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_NO_MEMORY);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_SEVERE);

    // Signal errors should be CRITICAL
    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_SIGSEGV);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_CRITICAL);
}

/* ============================================================================
 * String Conversion API Stability
 * ============================================================================ */

TEST_F(ExceptionAPIStabilityTest, SeverityToString_AllValues) {
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_DEBUG), "DEBUG");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_INFO), "INFO");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_WARNING), "WARNING");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_ERROR), "ERROR");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_SEVERE), "SEVERE");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_CRITICAL), "CRITICAL");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_FATAL), "FATAL");
}

TEST_F(ExceptionAPIStabilityTest, CategoryToString_AllValues) {
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_GENERIC), "GENERIC");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_MEMORY), "MEMORY");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_BRAIN), "BRAIN");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_IO), "IO");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_THREADING), "THREADING");
}

TEST_F(ExceptionAPIStabilityTest, TypeToString_AllValues) {
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_BASE), "BASE");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_MEMORY), "MEMORY");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_BRAIN), "BRAIN");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_IO), "IO");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_THREADING), "THREADING");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_GPU), "GPU");
}

TEST_F(ExceptionAPIStabilityTest, RecoveryActionToString_AllValues) {
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_NONE), "NONE");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RETRY), "RETRY");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GC), "GC");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_COMPACT), "COMPACT");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_ROLLBACK), "ROLLBACK");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_QUARANTINE), "QUARANTINE");
}

/* ============================================================================
 * Handler Registration API Stability
 * ============================================================================ */

static bool test_handler_invoked = false;

static bool test_stability_handler(nimcp_exception_t* ex, void* user_data) {
    (void)ex;
    (void)user_data;
    test_handler_invoked = true;
    return false;  // Don't consume
}

TEST_F(ExceptionAPIStabilityTest, HandlerRegister_ReturnsHandle) {
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "stability_test_handler";
    opts.handler = test_stability_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);
    EXPECT_STREQ(reg->options.name, "stability_test_handler");
    EXPECT_TRUE(reg->active);

    int result = nimcp_handler_unregister(reg);
    EXPECT_EQ(result, 0);
}

TEST_F(ExceptionAPIStabilityTest, HandlerRegister_NullOptionsReturnsNull) {
    nimcp_handler_registration_t* reg = nimcp_handler_register(nullptr);
    EXPECT_EQ(reg, nullptr);
}

TEST_F(ExceptionAPIStabilityTest, HandlerRegister_NullHandlerReturnsNull) {
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.handler = nullptr;  // Invalid

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    EXPECT_EQ(reg, nullptr);
}

TEST_F(ExceptionAPIStabilityTest, HandlerEnableDisable_Works) {
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "toggle_handler";
    opts.handler = test_stability_handler;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);
    EXPECT_TRUE(reg->active);

    nimcp_handler_disable(reg);
    EXPECT_FALSE(reg->active);

    nimcp_handler_enable(reg);
    EXPECT_TRUE(reg->active);

    nimcp_handler_unregister(reg);
}

/* ============================================================================
 * Exception Dispatch API Stability
 * ============================================================================ */

TEST_F(ExceptionAPIStabilityTest, ExceptionDispatch_InvokesHandlers) {
    test_handler_invoked = false;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "dispatch_test";
    opts.handler = test_stability_handler;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Dispatch test"
    );

    bool handled = nimcp_exception_dispatch(ex);
    EXPECT_TRUE(test_handler_invoked);
    EXPECT_FALSE(handled);  // Our handler returns false

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(reg);
}

/* ============================================================================
 * Thread-Local Context API Stability
 * ============================================================================ */

TEST_F(ExceptionAPIStabilityTest, CurrentException_InitiallyNull) {
    nimcp_exception_clear_current();
    EXPECT_EQ(nimcp_exception_get_current(), nullptr);
}

TEST_F(ExceptionAPIStabilityTest, CurrentException_SetAndGet) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Current test"
    );

    nimcp_exception_set_current(ex);
    EXPECT_EQ(nimcp_exception_get_current(), ex);

    nimcp_exception_clear_current();
    EXPECT_EQ(nimcp_exception_get_current(), nullptr);

    nimcp_exception_unref(ex);
}

/* ============================================================================
 * Exception Chaining API Stability
 * ============================================================================ */

TEST_F(ExceptionAPIStabilityTest, ExceptionChaining_Works) {
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
        "Wrapper"
    );

    nimcp_exception_ref(cause);  // Keep our reference
    nimcp_exception_set_cause(wrapper, cause);

    EXPECT_EQ(nimcp_exception_get_cause(wrapper), cause);

    nimcp_exception_unref(cause);
    nimcp_exception_unref(wrapper);
}

/* ============================================================================
 * Exception Context API Stability
 * ============================================================================ */

TEST_F(ExceptionAPIStabilityTest, ExceptionContext_SetAndGet) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Context test"
    );

    int result = nimcp_exception_set_context(ex, "request_id", "abc123");
    EXPECT_EQ(result, 0);

    result = nimcp_exception_set_context(ex, "user_id", "user456");
    EXPECT_EQ(result, 0);

    EXPECT_EQ(nimcp_exception_context_count(ex), 2UL);

    const char* val = nimcp_exception_get_context(ex, "request_id");
    EXPECT_STREQ(val, "abc123");

    val = nimcp_exception_get_context(ex, "user_id");
    EXPECT_STREQ(val, "user456");

    val = nimcp_exception_get_context(ex, "nonexistent");
    EXPECT_EQ(val, nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionAPIStabilityTest, ExceptionContext_Remove) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Remove test"
    );

    nimcp_exception_set_context(ex, "key1", "value1");
    nimcp_exception_set_context(ex, "key2", "value2");
    EXPECT_EQ(nimcp_exception_context_count(ex), 2UL);

    int result = nimcp_exception_remove_context(ex, "key1");
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_exception_context_count(ex), 1UL);
    EXPECT_EQ(nimcp_exception_get_context(ex, "key1"), nullptr);

    nimcp_exception_unref(ex);
}

/* ============================================================================
 * Aggregate Exception API Stability
 * ============================================================================ */

TEST_F(ExceptionAPIStabilityTest, AggregateException_Create) {
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Aggregate test"
    );

    ASSERT_NE(agg, nullptr);
    EXPECT_EQ(agg->base.type, EXCEPTION_TYPE_AGGREGATE);
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 0UL);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

TEST_F(ExceptionAPIStabilityTest, AggregateException_AddAndGet) {
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Aggregate with children"
    );

    nimcp_exception_t* child1 = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Child 1"
    );

    nimcp_exception_t* child2 = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Child 2"
    );

    int result = nimcp_aggregate_exception_add(agg, child1);
    EXPECT_EQ(result, 0);

    result = nimcp_aggregate_exception_add(agg, child2);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 2UL);
    EXPECT_EQ(nimcp_aggregate_exception_get(agg, 0), child1);
    EXPECT_EQ(nimcp_aggregate_exception_get(agg, 1), child2);
    EXPECT_EQ(nimcp_aggregate_exception_get(agg, 2), nullptr);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

/* ============================================================================
 * System Initialization API Stability
 * ============================================================================ */

TEST_F(ExceptionAPIStabilityTest, SystemInit_ReturnsZeroOnSuccess) {
    // System already initialized in SetUp
    EXPECT_TRUE(nimcp_exception_system_is_initialized());
}

TEST_F(ExceptionAPIStabilityTest, SystemInit_MultipleCallsSafe) {
    // Already initialized in SetUp
    int result = nimcp_exception_system_init();
    EXPECT_EQ(result, 0);  // Should succeed (idempotent)

    EXPECT_TRUE(nimcp_exception_system_is_initialized());
}

/* ============================================================================
 * Backwards Compatibility Tests
 * ============================================================================ */

TEST_F(ExceptionAPIStabilityTest, BackwardsCompat_ErrorHelpers) {
    // These helper functions should work as documented
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS));
    EXPECT_FALSE(nimcp_error_is_success(NIMCP_ERROR_OPERATION_FAILED));

    EXPECT_FALSE(nimcp_error_is_failure(NIMCP_SUCCESS));
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_OPERATION_FAILED));

    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_NO_MEMORY), 2);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_FORWARD_PASS), 3);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_FILE_NOT_FOUND), 4);
}

TEST_F(ExceptionAPIStabilityTest, BackwardsCompat_ErrorToString) {
    const char* msg = nimcp_error_to_string(NIMCP_ERROR_NO_MEMORY);
    EXPECT_NE(msg, nullptr);
    EXPECT_GT(strlen(msg), 0UL);
}

TEST_F(ExceptionAPIStabilityTest, BackwardsCompat_LegacyFEPHelpers) {
    // FEP bridge compatibility helpers
    EXPECT_TRUE(nimcp_is_ok(NIMCP_SUCCESS));
    EXPECT_FALSE(nimcp_is_ok(NIMCP_ERROR_OPERATION_FAILED));

    EXPECT_FALSE(nimcp_is_error(NIMCP_SUCCESS));
    EXPECT_TRUE(nimcp_is_error(NIMCP_ERROR_OPERATION_FAILED));

    EXPECT_EQ(nimcp_from_fep_result(0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_from_fep_result(-1), NIMCP_ERROR_OPERATION_FAILED);

    EXPECT_EQ(nimcp_to_fep_result(NIMCP_SUCCESS), 0);
    EXPECT_EQ(nimcp_to_fep_result(NIMCP_ERROR_OPERATION_FAILED), -1);
}

/* ============================================================================
 * Constants Stability Tests
 * ============================================================================ */

TEST_F(ExceptionAPIStabilityTest, Constants_MaxValues) {
    EXPECT_EQ(NIMCP_EXCEPTION_MAX_MESSAGE, 256);
    EXPECT_EQ(NIMCP_EXCEPTION_MAX_STACK_DEPTH, 32);
    EXPECT_EQ(NIMCP_EXCEPTION_EPITOPE_SIZE, 64);
    EXPECT_EQ(NIMCP_EXCEPTION_MAX_CONTEXT, 512);
    EXPECT_EQ(NIMCP_EXCEPTION_MAX_CHILDREN, 16);
    EXPECT_EQ(NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES, 16);
}

TEST_F(ExceptionAPIStabilityTest, Constants_HandlerPriorities) {
    EXPECT_EQ(NIMCP_HANDLER_PRIORITY_HIGH, 100);
    EXPECT_EQ(NIMCP_HANDLER_PRIORITY_NORMAL, 50);
    EXPECT_EQ(NIMCP_HANDLER_PRIORITY_LOW, 10);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
